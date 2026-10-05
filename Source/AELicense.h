#pragma once
#include <juce_cryptography/juce_cryptography.h>
#include <array>
#include <atomic>

// ── Ed25519-derived public key bytes (32) — used as the signing salt ──────────
// This is the PUBLIC salt only; it cannot mint licenses. It must byte-match
// PUB_BYTES in tools/keygen.py.
static constexpr std::array<uint8_t,32> kAEPublicKey = {
    0xfa,0xa6,0x1c,0xd9,0x1b,0xc7,0x56,0x33,
    0x2f,0x5c,0x73,0xba,0x60,0x77,0xe2,0x2f,
    0xca,0x0e,0x48,0x17,0x42,0xfd,0xed,0xce,
    0x66,0x2b,0x61,0x2e,0x3c,0xf5,0xb5,0xc9
};

// ── v1.1 free trial ───────────────────────────────────────────────────────────
static constexpr int kAETrialDays = 7;

// Where the BUY LICENSE button goes. Swap for the store / Messenger / website
// link when one is ready (any http(s) or mailto: URL works).
static constexpr const char* kAEBuyURL =
    "mailto:amarilabs.audio@gmail.com?subject=Arcane%20Eclipse%20license";

// What the user is allowed to do right now.
enum class AEAccess
{
    Licensed,        // activated with a valid serial + code
    TrialAvailable,  // never trialled on this machine — can start the 7-day trial
    TrialActive,     // inside the 7 days — fully working
    TrialExpired     // trial used up (or its record was tampered with) — audio muted
};

struct AELicense {
    juce::String name, email, serial, code;
    juce::StringArray machineIDs;   // up to 2
    bool valid = false;
    static constexpr int kMaxMachines = 2;
};

class AELicenseManager
{
public:
    static AELicenseManager& getInstance()
    { static AELicenseManager inst; return inst; }

    bool loadFromDisk();
    bool activate(const juce::String& name, const juce::String& email,
                  const juce::String& serial, const juce::String& code,
                  juce::String& errorMsg);

    bool isActivated()              const { return licensedFlag.load(); }
    juce::String getLicensedName()  const { return lic.name; }
    juce::String getLicensedEmail() const { return lic.email; }
    juce::String getSerial()        const { return lic.serial; }

    // ── Trial ────────────────────────────────────────────────────────────────
    // Re-reads the license and the trial record from disk (message thread).
    void refresh()                         { refreshAt (juce::Time::currentTimeMillis()); }
    // Starts the 7-day trial. Only works once per machine; false if a trial
    // record already exists.
    bool startTrial()                      { return startTrialAt (juce::Time::currentTimeMillis()); }
    AEAccess getAccess() const             { return getAccessAt (juce::Time::currentTimeMillis()); }
    int  trialDaysLeft() const             { return trialDaysLeftAt (juce::Time::currentTimeMillis()); }

    // Audio-thread safe: true when licensed or inside an active trial.
    bool isAudioEnabled() const noexcept
    {
        return licensedFlag.load (std::memory_order_relaxed)
            || juce::Time::currentTimeMillis() < trialEnd.load (std::memory_order_relaxed);
    }

    // Time-injectable versions (used by the tests).
    void     refreshAt (juce::int64 nowMs);
    bool     startTrialAt (juce::int64 nowMs);
    AEAccess getAccessAt (juce::int64 nowMs) const;
    int      trialDaysLeftAt (juce::int64 nowMs) const;
    static void setStorageRootForTesting (const juce::File& dir) { testRoot() = dir; }

private:
    AELicenseManager() = default;
    AELicense lic;

    std::atomic<bool>        licensedFlag { false };
    std::atomic<juce::int64> trialEnd     { 0 };      // 0 = no running trial
    juce::int64 trialStart  = 0;
    bool        trialRecord = false;                  // a trial was started on this machine
    bool        trialBad    = false;                  // record tampered / clock rolled back

    static juce::String saltHex();                       // zero-padded hex of kAEPublicKey
    static juce::String expectedHash(const juce::String& name,
                                     const juce::String& email,
                                     const juce::String& serial);
    static bool codeMatches(const juce::String& name, const juce::String& email,
                            const juce::String& serial, const juce::String& code);
    juce::String getMachineID() const;
    juce::File   getLicenseFile() const;
    bool saveToDisk();

    // trial record storage (two copies, so deleting one does not reset it)
    static juce::File& testRoot() { static juce::File f; return f; }
    juce::Array<juce::File> getTrialFiles() const;
    juce::String trialSignature (juce::int64 start, juce::int64 seen) const;
    void writeTrialRecord (juce::int64 start, juce::int64 seen);
    juce::MemoryBlock xorWithMachine (const void* data, size_t size) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AELicenseManager)
};
