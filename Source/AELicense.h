#pragma once
#include <juce_cryptography/juce_cryptography.h>
#include <array>

// ── Ed25519-derived public key bytes (32) — used as the signing salt ──────────
// This is the PUBLIC salt only; it cannot mint licenses. It must byte-match
// PUB_BYTES in tools/keygen.py.
static constexpr std::array<uint8_t,32> kAEPublicKey = {
    0xfa,0xa6,0x1c,0xd9,0x1b,0xc7,0x56,0x33,
    0x2f,0x5c,0x73,0xba,0x60,0x77,0xe2,0x2f,
    0xca,0x0e,0x48,0x17,0x42,0xfd,0xed,0xce,
    0x66,0x2b,0x61,0x2e,0x3c,0xf5,0xb5,0xc9
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

    bool isActivated()              const { return lic.valid; }
    juce::String getLicensedName()  const { return lic.name; }
    juce::String getLicensedEmail() const { return lic.email; }
    juce::String getSerial()        const { return lic.serial; }

private:
    AELicenseManager() = default;
    AELicense lic;

    static juce::String saltHex();                       // zero-padded hex of kAEPublicKey
    static juce::String expectedHash(const juce::String& name,
                                     const juce::String& email,
                                     const juce::String& serial);
    static bool codeMatches(const juce::String& name, const juce::String& email,
                            const juce::String& serial, const juce::String& code);
    juce::String getMachineID() const;
    juce::File   getLicenseFile() const;
    bool saveToDisk();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AELicenseManager)
};
