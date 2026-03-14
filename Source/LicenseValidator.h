#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include "Config.h"
#include <atomic>

// Offline license validation for ProEQ8.
// License keys are HMAC-SHA256-signed JSON payloads (Base64-encoded).
// Format: "<base64_payload>.<base64_signature>"
//
// Payload JSON: { "product": "ProEQ8", "email": "...", "expires": "2099-12-31" }
//
// The signing secret is shared between the server (Stripe webhook) and the client.
// In demo mode: audio mutes for 30 seconds every 5 minutes.

class LicenseValidator
{
public:
    LicenseValidator()
    {
        loadStoredLicense();
    }

    // Attempt to activate with a license key string
    bool activate(const juce::String& licenseKey)
    {
        if (validateKey(licenseKey))
        {
            storeLicense(licenseKey);
            activated.store(true);
            licensedEmail = parsedEmail;
            return true;
        }
        return false;
    }

    bool isActivated() const { return activated.load(); }
    juce::String getEmail() const { return licensedEmail; }

    // Demo mode: returns true when audio should be muted
    // Mutes for 30 seconds every 5 minutes (300 seconds)
    bool shouldMuteDemo(double sampleRate, int numSamples)
    {
        if (activated.load()) return false;
        if (!kIsProVersion) return false;  // FreeEQ8 never mutes

        demoSampleCounter += numSamples;
        const double cycleSamples = 300.0 * sampleRate;  // 5-minute cycle
        const double muteSamples  = 30.0 * sampleRate;   // 30-second mute window
        const double pos = std::fmod((double)demoSampleCounter, cycleSamples);

        return pos >= (cycleSamples - muteSamples);
    }

    // Reset demo counter (call from prepareToPlay)
    void resetDemoCounter() { demoSampleCounter = 0; }

    void deactivate()
    {
        activated.store(false);
        licensedEmail.clear();
        clearStoredLicense();
    }

private:
    std::atomic<bool> activated { false };
    juce::String licensedEmail;
    juce::String parsedEmail;
    int64_t demoSampleCounter = 0;

    // HMAC signing secret — must match LICENSE_SIGNING_SECRET on the server.
    // IMPORTANT: Replace this with your actual secret before shipping.
    // In a real production build, obfuscate or derive this at compile time.
    static constexpr const char* licenseSigningSecret =
        "REPLACE_WITH_YOUR_LICENSE_SIGNING_SECRET";

    // Compute HMAC-SHA256 using a simple implementation (no external crypto dependency).
    // This matches the server's crypto.subtle.sign("HMAC", key, payload) output.
    static juce::MemoryBlock hmacSha256(const juce::String& key, const juce::String& message)
    {
        // SHA-256 block size = 64 bytes
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

        // If key > block size, hash it
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

        // Ensure key is padded to blockSize
        if ((int)keyBlock.getSize() < blockSize)
        {
            auto oldSize = keyBlock.getSize();
            keyBlock.setSize(blockSize);
            memset(static_cast<uint8_t*>(keyBlock.getData()) + oldSize, 0, blockSize - oldSize);
        }

        auto* kp = static_cast<const uint8_t*>(keyBlock.getData());

        // ipad = key XOR 0x36, opad = key XOR 0x5c
        uint8_t ipad[blockSize], opad[blockSize];
        for (int i = 0; i < blockSize; ++i)
        {
            ipad[i] = kp[i] ^ 0x36;
            opad[i] = kp[i] ^ 0x5c;
        }

        // inner = SHA256(ipad || message)
        juce::MemoryBlock innerInput;
        innerInput.append(ipad, blockSize);
        innerInput.append(msgBytes, strlen(msgBytes));
        auto innerHash = sha256(innerInput.getData(), innerInput.getSize());

        // outer = SHA256(opad || inner)
        juce::MemoryBlock outerInput;
        outerInput.append(opad, blockSize);
        outerInput.append(innerHash.getData(), innerHash.getSize());
        return sha256(outerInput.getData(), outerInput.getSize());
    }

    bool validateKey(const juce::String& licenseKey)
    {
        // Format: "<base64_payload>.<base64_signature>"
        const int dotIdx = licenseKey.indexOf(".");
        if (dotIdx < 0) return false;

        auto payloadB64   = licenseKey.substring(0, dotIdx);
        auto signatureB64 = licenseKey.substring(dotIdx + 1);

        // Verify HMAC-SHA256 signature
        auto expectedHmac = hmacSha256(juce::String(licenseSigningSecret), payloadB64);
        auto expectedB64  = expectedHmac.toBase64Encoding();

        // Trim trailing whitespace/newlines from base64
        expectedB64 = expectedB64.trim();
        auto receivedSig = signatureB64.trim();

        if (expectedB64 != receivedSig)
            return false;

        // Decode payload
        juce::MemoryBlock payloadData;
        if (!payloadData.fromBase64Encoding(payloadB64))
            return false;

        // Parse JSON payload
        auto payloadStr = payloadData.toString();
        auto json = juce::JSON::parse(payloadStr);
        if (!json.isObject()) return false;

        auto product = json.getProperty("product", "").toString();
        auto email   = json.getProperty("email", "").toString();
        auto expires = json.getProperty("expires", "").toString();

        // Verify product matches
        if (product != "ProEQ8") return false;

        // Check expiration
        auto expiryTime = juce::Time::fromISO8601(expires + "T23:59:59Z");
        if (expiryTime < juce::Time::getCurrentTime()) return false;

        parsedEmail = email;
        return true;
    }

    void storeLicense(const juce::String& key)
    {
        auto props = getAppProperties();
        if (props)
        {
            props->getUserSettings()->setValue("license_key", key);
            props->getUserSettings()->saveIfNeeded();
        }
    }

    void clearStoredLicense()
    {
        auto props = getAppProperties();
        if (props)
        {
            props->getUserSettings()->removeValue("license_key");
            props->getUserSettings()->saveIfNeeded();
        }
    }

    void loadStoredLicense()
    {
        auto props = getAppProperties();
        if (props)
        {
            auto key = props->getUserSettings()->getValue("license_key", "");
            if (key.isNotEmpty() && validateKey(key))
            {
                activated.store(true);
                licensedEmail = parsedEmail;
            }
        }
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
