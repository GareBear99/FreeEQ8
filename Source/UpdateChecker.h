#pragma once
#include <juce_core/juce_core.h>
#include "Config.h"
#include <atomic>

// Checks GitHub Releases API for newer versions in a background thread.
// Non-blocking, rate-limited to once per 24 hours.
class UpdateChecker : private juce::Thread
{
public:
    UpdateChecker() : juce::Thread("UpdateChecker") {}
    ~UpdateChecker() override { stopThread(2000); }

    // Call once from the editor constructor. Spawns a background thread.
    void checkAsync()
    {
        if (alreadyChecked.exchange(true))
            return;  // only check once per plugin instance

        startThread(juce::Thread::Priority::low);
    }

    bool isUpdateAvailable() const { return updateAvailable.load(); }
    juce::String getLatestVersion() const { return latestVersion; }
    juce::String getDownloadURL()   const { return downloadURL; }

private:
    std::atomic<bool> updateAvailable { false };
    std::atomic<bool> alreadyChecked  { false };
    juce::String latestVersion;
    juce::String downloadURL;

    void run() override
    {
        // Rate limit: check at most once per 24 hours (using a temp file timestamp)
        auto stampFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                             .getChildFile("freeeq8_update_check.stamp");
        if (stampFile.existsAsFile())
        {
            auto lastCheck = stampFile.getLastModificationTime();
            auto now = juce::Time::getCurrentTime();
            if ((now - lastCheck).inHours() < 24)
                return;
        }

        // Hit the GitHub releases API
        juce::URL url("https://api.github.com/repos/" + juce::String(kGitHubOwner)
                      + "/" + juce::String(kGitHubRepo) + "/releases/latest");

        auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                           .withConnectionTimeoutMs(5000);

        std::unique_ptr<juce::InputStream> stream(url.createInputStream(options));
        if (!stream || threadShouldExit())
            return;

        auto responseText = stream->readEntireStreamAsString();
        if (responseText.isEmpty())
            return;

        // Parse JSON
        auto json = juce::JSON::parse(responseText);
        if (!json.isObject())
            return;

        auto tagName = json.getProperty("tag_name", "").toString();
        auto htmlUrl = json.getProperty("html_url", "").toString();

        if (tagName.isEmpty())
            return;

        // Strip leading 'v' if present
        auto remoteVersion = tagName.startsWith("v") ? tagName.substring(1) : tagName;
        auto localVersion  = juce::String(kVersion);

        // Simple string comparison (works for semver with same digit counts)
        if (remoteVersion.compareNatural(localVersion) > 0)
        {
            latestVersion = remoteVersion;
            downloadURL   = htmlUrl;
            updateAvailable.store(true);
        }

        // Update timestamp
        stampFile.create();
        stampFile.setLastModificationTime(juce::Time::getCurrentTime());
    }
};
