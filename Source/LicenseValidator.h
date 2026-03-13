#pragma once
#include <juce_core/juce_core.h>
// TODO: #include <juce_cryptography/juce_cryptography.h> — add when RSA verification is implemented
#include "Config.h"
#include <atomic>

// Offline license validation for ProEQ8.
// License keys are RSA-signed JSON payloads (Base64-encoded).
// Format: "<base64_payload>.<base64_signature>"
//
// Payload JSON: { "product": "ProEQ8", "email": "...", "expires": "2099-12-31" }
//
// The public key is embedded; the private key lives on the server (Stripe webhook).
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

    // RSA public key (PEM, Base64-encoded modulus).
    // Replace with your actual public key for production.
    // This is a placeholder 2048-bit key for development.
    static constexpr const char* publicKeyPem =
        "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA0"
        "PLACEHOLDER_REPLACE_WITH_REAL_PUBLIC_KEY"
        "AQAB";

    bool validateKey(const juce::String& licenseKey)
    {
        // Format: "<base64_payload>.<base64_signature>"
        const int dotIdx = licenseKey.indexOf(".");
        if (dotIdx < 0) return false;

        auto payloadB64  = licenseKey.substring(0, dotIdx);
        auto signatureB64 = licenseKey.substring(dotIdx + 1);

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

        // TODO: RSA signature verification against public key
        // For now, accept keys with valid JSON structure.
        // In production, use juce::RSAKey or an external crypto lib
        // to verify the signature against publicKeyPem.
        //
        // juce::RSAKey pubKey(publicKeyPem);
        // juce::BigInteger sig;
        // sig.parseString(signatureB64, 16);
        // ... verify ...

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
