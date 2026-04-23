#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include "Config.h"
#include <atomic>

// License validation for ProEQ8 with online activation.
//
// License keys are HMAC-SHA256-signed JSON payloads (Base64-encoded).
// Format: "<base64_payload>.<base64_signature>"
// Payload: { "product": "ProEQ8", "email": "...", "license_id": "...", "expires": "..." }
//
// Activation flow:
//   1. Client validates HMAC signature offline (quick reject of bad keys).
//   2. Client POSTs { license_key, device_id } to the activation server.
//   3. Server checks activation count (max 2 devices per license).
//   4. On success, client stores the key locally.
//
// In demo mode (ProEQ8 only, unactivated): audio mutes 30s every 5min.

enum class ActivationResult
{
    Success,
    InvalidKey,
    Expired,
    LimitReached,
    NetworkError,
    ServerError
};

class LicenseValidator
{
public:
    LicenseValidator()
    {
        deviceId = computeDeviceId();
        loadStoredLicense();
    }

    // Attempt to activate with a license key string.
    // This performs an online activation request.
    // Must be called from a background thread (blocks on HTTP).
    ActivationResult activate(const juce::String& licenseKey)
    {
        // 1. Offline HMAC check (quick rejection)
        if (!validateKeyOffline(licenseKey))
            return ActivationResult::InvalidKey;

        // 2. Online activation
        auto result = activateOnline(licenseKey);
        if (result == ActivationResult::Success)
        {
            storeLicense(licenseKey);
            activated.store(true);
            licensedEmail = parsedEmail;
        }
        return result;
    }

    bool isActivated() const { return activated.load(); }
    bool needsPeriodicReverify() const { return needsReverify; }
    juce::String getEmail() const { return licensedEmail; }
    juce::String getDeviceId() const { return deviceId; }

    // Demo mode: returns true when audio should be muted.
    //
    // Cycle: 2 minutes of clean playback, then a 30-second mute window,
    // for a 2:30 period. Honor-system demo: determined users can trivially
    // slice output into 2-minute segments, so this is a nag, not a DRM.
    // Fully disabled in FreeEQ8 (free, GPL-3).
    bool shouldMuteDemo(double sampleRate, int numSamples)
    {
        if (activated.load()) return false;
        if (!kIsProVersion) return false;  // FreeEQ8 never mutes

        demoSampleCounter += numSamples;
        const double cleanSeconds = 120.0;                            // 2 min clean
        const double muteSeconds  = 30.0;                             // 30 s mute
        const double cycleSamples = (cleanSeconds + muteSeconds) * sampleRate;
        const double muteSamples  = muteSeconds * sampleRate;
        const double pos = std::fmod((double)demoSampleCounter, cycleSamples);

        // Muted during the final muteSamples of each cycle, i.e. from
        // pos = cleanSeconds*sr onward.
        return pos >= (cycleSamples - muteSamples);
    }

    // Reset demo counter (call from prepareToPlay)
    void resetDemoCounter() { demoSampleCounter = 0; }

    // Deactivate and release the device slot on the server.
    // Must be called from a background thread.
    void deactivate()
    {
        auto key = getStoredKey();
        if (key.isNotEmpty())
            deactivateOnline(key);

        activated.store(false);
        licensedEmail.clear();
        clearStoredLicense();
    }

    // Get the activation result message for UI display.
    static juce::String getResultMessage(ActivationResult result)
    {
        switch (result)
        {
            case ActivationResult::Success:      return "Activation successful!";
            case ActivationResult::InvalidKey:    return "Invalid or expired license key.";
            case ActivationResult::Expired:       return "This license has expired.";
            case ActivationResult::LimitReached:  return "Activation limit reached (2 devices). Deactivate another device first.";
            case ActivationResult::NetworkError:  return "Could not reach activation server. Check your internet connection.";
            case ActivationResult::ServerError:   return "Server error. Please try again later.";
        }
        return "Unknown error.";
    }

private:
    std::atomic<bool> activated { false };
    bool needsReverify = false;
    juce::String licensedEmail;
    juce::String parsedEmail;
    juce::String deviceId;
    int64_t demoSampleCounter = 0;

    // HMAC signing secret — must match LICENSE_SIGNING_SECRET on the server.
    // XOR-obfuscated at compile time so it's not a plain string in the binary.
    // To generate a new secret: pick a random 48+ char string, XOR each byte
    // with 0x5A, store the result below, and set the same plaintext string
    // as LICENSE_SIGNING_SECRET in your Cloudflare Worker secrets.
    static juce::String getSigningSecret()
    {
        // Obfuscated bytes (XOR 0x5A) of the actual secret.
        // Replace these with your own secret XOR'd with 0x5A.
        static constexpr uint8_t enc[] = {
            0x2a, 0x3b, 0x29, 0x2c, 0x3b, 0x36, 0x28, 0x3b,
            0x31, 0x6e, 0x0b, 0x39, 0x3c, 0x22, 0x3b, 0x31,
            0x6e, 0x09, 0x31, 0x2c, 0x29, 0x32, 0x2c, 0x2b,
            0x3a, 0x31, 0x2c, 0x31, 0x2b, 0x6e, 0x15, 0x3b,
            0x25, 0x38, 0x3e, 0x2c, 0x6e, 0x3c, 0x29, 0x6e,
            0x2c, 0x3a, 0x32, 0x2c, 0x2b, 0x2c, 0x3c, 0x36
        };
        char buf[sizeof(enc) + 1];
        for (size_t i = 0; i < sizeof(enc); ++i)
            buf[i] = (char)(enc[i] ^ 0x5A);
        buf[sizeof(enc)] = 0;
        return juce::String::fromUTF8(buf, (int)sizeof(enc));
    }

    // ── Device Fingerprint ───────────────────────────────────────

    static juce::String computeDeviceId()
    {
        // Build a stable, non-PII device identifier by hashing system info.
        juce::String raw;
        raw << juce::SystemStats::getComputerName();

        // Add platform-specific machine IDs for stability
#if JUCE_MAC
        // macOS: IOPlatformSerialNumber via system_profiler is stable
        // Fall back to computer name + OS name if unavailable
        raw << juce::SystemStats::getOperatingSystemName();
        raw << juce::File("/etc/machine-id").loadFileAsString().trim();

        // Try the macOS hardware UUID
        juce::ChildProcess proc;
        if (proc.start("ioreg -rd1 -c IOPlatformExpertDevice"))
        {
            auto output = proc.readAllProcessOutput();
            // Extract IOPlatformUUID from output
            auto uuidIdx = output.indexOf("IOPlatformUUID");
            if (uuidIdx >= 0)
            {
                auto lineEnd = output.indexOf(uuidIdx, "\n");
                if (lineEnd > uuidIdx)
                    raw << output.substring(uuidIdx, lineEnd);
            }
        }
#elif JUCE_WINDOWS
        raw << juce::SystemStats::getOperatingSystemName();
        // Windows: MachineGuid from registry
        raw << juce::WindowsRegistry::getValue(
            "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Cryptography\\MachineGuid", "");
#elif JUCE_LINUX
        raw << juce::File("/etc/machine-id").loadFileAsString().trim();
        if (raw.isEmpty())
            raw << juce::File("/var/lib/dbus/machine-id").loadFileAsString().trim();
#endif

        // Namespace by product name so FreeEQ8 and ProEQ8 get different IDs
        raw << kProductName;

        // SHA-256 hash → hex string
        juce::SHA256 hash(raw.toUTF8(), (size_t)raw.length());
        return hash.toHexString().replace(" ", "");
    }

    // ── Offline HMAC Validation ──────────────────────────────────

    // Compute HMAC-SHA256.
    static juce::MemoryBlock hmacSha256(const juce::String& key, const juce::String& message)
    {
        constexpr int blockSize = 64;
        constexpr int hashSize  = 32;

        auto sha256 = [](const void* data, size_t len) -> juce::MemoryBlock
        {
            juce::SHA256 hasher(data, len);
            auto result = hasher.toHexString();
            juce::MemoryBlock mb;
            mb.setSize(hashSize);
            auto* dst = static_cast<uint8_t*>(mb.getData());
            for (int i = 0; i < hashSize; ++i)
                dst[i] = (uint8_t)result.substring(i * 2, i * 2 + 2).getHexValue32();
            return mb;
        };

        auto keyBytes = key.toUTF8();
        auto msgBytes = message.toUTF8();

        juce::MemoryBlock keyBlock;
        if ((int)strlen(keyBytes) > blockSize)
        {
            keyBlock = sha256(keyBytes, strlen(keyBytes));
        }
        else
        {
            keyBlock.setSize(blockSize);
            keyBlock.fillWith(0);
            memcpy(keyBlock.getData(), keyBytes, strlen(keyBytes));
        }

        if ((int)keyBlock.getSize() < blockSize)
        {
            auto oldSize = keyBlock.getSize();
            keyBlock.setSize(blockSize);
            memset(static_cast<uint8_t*>(keyBlock.getData()) + oldSize, 0, blockSize - oldSize);
        }

        auto* kp = static_cast<const uint8_t*>(keyBlock.getData());

        uint8_t ipad[blockSize], opad[blockSize];
        for (int i = 0; i < blockSize; ++i)
        {
            ipad[i] = kp[i] ^ 0x36;
            opad[i] = kp[i] ^ 0x5c;
        }

        juce::MemoryBlock innerInput;
        innerInput.append(ipad, blockSize);
        innerInput.append(msgBytes, strlen(msgBytes));
        auto innerHash = sha256(innerInput.getData(), innerInput.getSize());

        juce::MemoryBlock outerInput;
        outerInput.append(opad, blockSize);
        outerInput.append(innerHash.getData(), innerHash.getSize());
        return sha256(outerInput.getData(), outerInput.getSize());
    }

    bool validateKeyOffline(const juce::String& licenseKey)
    {
        const int dotIdx = licenseKey.indexOf(".");
        if (dotIdx < 0) return false;

        auto payloadB64   = licenseKey.substring(0, dotIdx);
        auto signatureB64 = licenseKey.substring(dotIdx + 1);

        // Verify HMAC-SHA256 signature
        auto expectedHmac = hmacSha256(getSigningSecret(), payloadB64);
        auto expectedB64  = expectedHmac.toBase64Encoding().trim();
        auto receivedSig  = signatureB64.trim();

        // Constant-time comparison
        if (expectedB64.length() != receivedSig.length())
            return false;

        int mismatch = 0;
        for (int i = 0; i < expectedB64.length(); ++i)
            mismatch |= expectedB64[i] ^ receivedSig[i];

        if (mismatch != 0)
            return false;

        // Decode payload
        juce::MemoryBlock payloadData;
        if (!payloadData.fromBase64Encoding(payloadB64))
            return false;

        auto payloadStr = payloadData.toString();
        auto json = juce::JSON::parse(payloadStr);
        if (!json.isObject()) return false;

        auto product = json.getProperty("product", "").toString();
        auto email   = json.getProperty("email", "").toString();
        auto expires = json.getProperty("expires", "").toString();

        if (product != "ProEQ8") return false;

        // Check expiration
        auto expiryTime = juce::Time::fromISO8601(expires + "T23:59:59Z");
        if (expiryTime < juce::Time::getCurrentTime()) return false;

        parsedEmail = email;
        return true;
    }

    // ── Online Activation ────────────────────────────────────────

    ActivationResult activateOnline(const juce::String& licenseKey)
    {
        auto serverUrl = juce::String(kActivationServerURL) + "/activate";

        // Build JSON body
        auto* jsonBody = new juce::DynamicObject();
        jsonBody->setProperty("license_key", licenseKey);
        jsonBody->setProperty("device_id", deviceId);
        auto bodyStr = juce::JSON::toString(juce::var(jsonBody));

        // POST request
        juce::URL url(serverUrl);
        url = url.withPOSTData(bodyStr);

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                           .withConnectionTimeoutMs(10000)
                           .withExtraHeaders("Content-Type: application/json");

        std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));

        if (!stream)
            return ActivationResult::NetworkError;

        auto responseText = stream->readEntireStreamAsString();
        if (responseText.isEmpty())
            return ActivationResult::NetworkError;

        auto response = juce::JSON::parse(responseText);
        if (!response.isObject())
            return ActivationResult::ServerError;

        // Check response
        if (response.hasProperty("ok") && (bool)response.getProperty("ok", false))
            return ActivationResult::Success;

        auto error = response.getProperty("error", "").toString();
        if (error.contains("limit") || error.contains("Limit"))
            return ActivationResult::LimitReached;
        if (error.contains("expired") || error.contains("Expired"))
            return ActivationResult::Expired;
        if (error.contains("Invalid") || error.contains("invalid"))
            return ActivationResult::InvalidKey;

        return ActivationResult::ServerError;
    }

    void deactivateOnline(const juce::String& licenseKey)
    {
        auto serverUrl = juce::String(kActivationServerURL) + "/deactivate";

        auto* jsonBody = new juce::DynamicObject();
        jsonBody->setProperty("license_key", licenseKey);
        jsonBody->setProperty("device_id", deviceId);
        auto bodyStr = juce::JSON::toString(juce::var(jsonBody));

        juce::URL url(serverUrl);
        url = url.withPOSTData(bodyStr);

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                           .withConnectionTimeoutMs(10000)
                           .withExtraHeaders("Content-Type: application/json");

        // Fire and forget — deactivation is best-effort
        std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));
        if (stream)
        {
            auto responseText = stream->readEntireStreamAsString();
            DBG("Deactivation response: " + responseText);
        }
    }

    // ── License Storage ──────────────────────────────────────────

    juce::String getStoredKey() const
    {
        auto props = getAppProperties();
        if (props)
            return props->getUserSettings()->getValue("license_key", "");
        return {};
    }

    void storeLicense(const juce::String& key)
    {
        auto props = getAppProperties();
        if (props)
        {
            props->getUserSettings()->setValue("license_key", key);
            props->getUserSettings()->setValue("license_device_id", deviceId);
            props->getUserSettings()->setValue("license_last_verified",
                juce::String(juce::Time::currentTimeMillis()));
            props->getUserSettings()->saveIfNeeded();
        }
    }

    void clearStoredLicense()
    {
        auto props = getAppProperties();
        if (props)
        {
            props->getUserSettings()->removeValue("license_key");
            props->getUserSettings()->removeValue("license_device_id");
            props->getUserSettings()->removeValue("license_last_verified");
            props->getUserSettings()->saveIfNeeded();
        }
    }

    // Re-verification interval: 7 days.  Grace period: 30 days offline max.
    static constexpr int64_t kRecheckIntervalMs = 7LL * 24 * 60 * 60 * 1000;   // 7 days
    static constexpr int64_t kGracePeriodMs     = 30LL * 24 * 60 * 60 * 1000;  // 30 days

    void loadStoredLicense()
    {
        auto props = getAppProperties();
        if (!props) return;

        auto key      = props->getUserSettings()->getValue("license_key", "");
        auto storedId = props->getUserSettings()->getValue("license_device_id", "");
        auto lastCheck = props->getUserSettings()->getValue("license_last_verified", "0").getLargeIntValue();

        if (key.isEmpty()) return;

        // ── Device binding: reject if stored device doesn't match this machine ──
        if (storedId.isNotEmpty() && storedId != deviceId)
        {
            // Settings file was copied from another machine → invalid
            clearStoredLicense();
            return;
        }

        // ── Offline HMAC check ──
        if (!validateKeyOffline(key))
        {
            clearStoredLicense();
            return;
        }

        // ── Check if periodic re-verification is due ──
        auto now = juce::Time::currentTimeMillis();
        auto elapsed = now - lastCheck;

        if (elapsed > kGracePeriodMs)
        {
            // Been offline too long → force re-activation
            clearStoredLicense();
            return;
        }

        // Mark as activated (will re-verify in background if overdue)
        activated.store(true);
        licensedEmail = parsedEmail;
        needsReverify = (elapsed > kRecheckIntervalMs);
    }

public:
    /** Call from a background thread to periodically re-verify with the server. */
    bool reverifyWithServer()
    {
        auto key = getStoredKey();
        if (key.isEmpty()) return false;

        auto serverUrl = juce::String(kActivationServerURL) + "/verify";

        auto* jsonBody = new juce::DynamicObject();
        jsonBody->setProperty("license_key", key);
        jsonBody->setProperty("device_id", deviceId);
        auto bodyStr = juce::JSON::toString(juce::var(jsonBody));

        juce::URL url(serverUrl);
        url = url.withPOSTData(bodyStr);

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                           .withConnectionTimeoutMs(10000)
                           .withExtraHeaders("Content-Type: application/json");

        std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));
        if (!stream)
            return false;  // Network failure → keep grace period

        auto responseText = stream->readEntireStreamAsString();
        auto response = juce::JSON::parse(responseText);

        if (response.isObject() && (bool)response.getProperty("ok", false))
        {
            // Verified — update timestamp
            auto props = getAppProperties();
            if (props)
            {
                props->getUserSettings()->setValue("license_last_verified",
                    juce::String(juce::Time::currentTimeMillis()));
                props->getUserSettings()->saveIfNeeded();
            }
            needsReverify = false;
            return true;
        }

        // Server explicitly rejected → deactivate
        activated.store(false);
        clearStoredLicense();
        needsReverify = false;
        return false;
    }

    static juce::ApplicationProperties* getAppProperties()
    {
        static juce::ApplicationProperties props;
        static bool inited = false;
        if (!inited)
        {
            juce::PropertiesFile::Options opts;
            opts.applicationName = kProductName;
            opts.folderName = "TizWildinEntertainment";
            opts.filenameSuffix = ".settings";
            opts.osxLibrarySubFolder = "Application Support";
            props.setStorageParameters(opts);
            inited = true;
        }
        return &props;
    }
};
