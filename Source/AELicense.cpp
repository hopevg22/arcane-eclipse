#include "AELicense.h"

// ── Helpers ───────────────────────────────────────────────────────────────────
juce::String AELicenseManager::saltHex()
{
    // Zero-padded, lowercase hex of the 32 public-key bytes (matches keygen.py)
    return juce::String::toHexString(kAEPublicKey.data(), (int)kAEPublicKey.size(), 0);
}

juce::String AELicenseManager::expectedHash(const juce::String& name,
                                            const juce::String& email,
                                            const juce::String& serial)
{
    juce::String payload = name.trim() + "|" + email.trim().toLowerCase() + "|" + serial.trim();
    juce::String toHash  = payload + saltHex();
    juce::SHA256 sha(toHash.toUTF8(), toHash.getNumBytesAsUTF8());
    return sha.toHexString();   // 64-char lowercase
}

bool AELicenseManager::codeMatches(const juce::String& name, const juce::String& email,
                                   const juce::String& serial, const juce::String& code)
{
    juce::String prefix = serial.trim() + "-";
    if (!code.trim().startsWith(prefix)) return false;
    juce::String sigB64 = code.trim().substring(prefix.length());
    while (sigB64.length() % 4 != 0) sigB64 += "=";
    juce::MemoryOutputStream mos;
    if (!juce::Base64::convertFromBase64(mos, sigB64)) return false;
    juce::MemoryBlock decoded = mos.getMemoryBlock();
    juce::String decodedHex = juce::String::toHexString(decoded.getData(), (int)decoded.getSize(), 0);
    return decodedHex == expectedHash(name, email, serial);
}

juce::File AELicenseManager::getLicenseFile() const
{
    if (testRoot() != juce::File()) return testRoot().getChildFile("license.dat");
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
           .getChildFile("ArcaneEclipse").getChildFile("license.dat");
}

juce::String AELicenseManager::getMachineID() const
{
    juce::String id = juce::SystemStats::getFullUserName()
                    + juce::SystemStats::getComputerName()
                    + juce::String(juce::SystemStats::getNumCpus());
    juce::SHA256 sha(id.toUTF8(), id.getNumBytesAsUTF8());
    return sha.toHexString().substring(0, 32);
}

// ── Activation ────────────────────────────────────────────────────────────────
bool AELicenseManager::activate(const juce::String& name, const juce::String& email,
                                const juce::String& serial, const juce::String& code,
                                juce::String& errorMsg)
{
    if (name.trim().isEmpty())   { errorMsg = "Please enter your full name.";     return false; }
    if (email.trim().isEmpty())  { errorMsg = "Please enter your email address."; return false; }
    if (!email.contains("@"))    { errorMsg = "Please enter a valid email.";      return false; }
    if (serial.trim().isEmpty()) { errorMsg = "Please enter your serial number."; return false; }
    if (code.trim().isEmpty())   { errorMsg = "Please enter your license code.";  return false; }

    if (!codeMatches(name, email, serial, code))
    { errorMsg = "Invalid license details. Please check name, email, serial and code."; return false; }

    // Machine limit
    juce::String mid = getMachineID();
    if (!lic.machineIDs.contains(mid))
    {
        if (lic.machineIDs.size() >= AELicense::kMaxMachines)
        { errorMsg = "This license is already active on 2 machines.\nContact support to transfer."; return false; }
        lic.machineIDs.add(mid);
    }
    lic.name   = name.trim();
    lic.email  = email.trim().toLowerCase();
    lic.serial = serial.trim();
    lic.code   = code.trim();
    lic.valid  = true;
    licensedFlag.store(true);
    saveToDisk();
    return true;
}

// ── Persistence (XOR-obfuscated, keyed to machine ID) ─────────────────────────
bool AELicenseManager::saveToDisk()
{
    auto f = getLicenseFile();
    f.getParentDirectory().createDirectory();

    juce::XmlElement root("AELicense");
    root.setAttribute("name",     lic.name);
    root.setAttribute("email",    lic.email);
    root.setAttribute("serial",   lic.serial);
    root.setAttribute("code",     lic.code);
    root.setAttribute("machines", lic.machineIDs.joinIntoString(","));

    juce::String xml = root.toString();
    const char* raw = xml.toRawUTF8();
    int len = (int) strlen(raw);
    juce::String mid = getMachineID();
    const char* key = mid.toRawUTF8();
    int keyLen = (int) strlen(key);

    juce::MemoryBlock enc((size_t) len);
    auto* d = (uint8_t*) enc.getData();
    for (int i = 0; i < len; ++i)
        d[i] = (uint8_t) raw[i] ^ (uint8_t) key[i % keyLen];

    return f.replaceWithData(enc.getData(), enc.getSize());
}

bool AELicenseManager::loadFromDisk()
{
    lic.valid = false;
    licensedFlag.store(false);
    auto f = getLicenseFile();
    if (!f.existsAsFile()) return false;

    juce::MemoryBlock enc;
    if (!f.loadFileAsData(enc)) return false;

    juce::String mid = getMachineID();
    const char* key = mid.toRawUTF8();
    int keyLen = (int) strlen(key);

    juce::MemoryBlock dec(enc.getSize());
    auto* s = (const uint8_t*) enc.getData();
    auto* d = (uint8_t*) dec.getData();
    for (size_t i = 0; i < enc.getSize(); ++i)
        d[i] = s[i] ^ (uint8_t) key[(int)(i % (size_t) keyLen)];

    juce::String xml = juce::String::fromUTF8((const char*) dec.getData(), (int) dec.getSize());
    auto elem = juce::parseXML(xml);
    if (elem == nullptr || elem->getTagName() != "AELicense") return false;

    lic.name       = elem->getStringAttribute("name");
    lic.email      = elem->getStringAttribute("email");
    lic.serial     = elem->getStringAttribute("serial");
    lic.code       = elem->getStringAttribute("code");
    lic.machineIDs = juce::StringArray::fromTokens(elem->getStringAttribute("machines"), ",", "");

    if (!lic.machineIDs.contains(mid)) return false;
    lic.valid = codeMatches(lic.name, lic.email, lic.serial, lic.code);
    licensedFlag.store(lic.valid);
    return lic.valid;
}

// ── v1.1 free trial ───────────────────────────────────────────────────────────
// The trial start date is stored in two places, each copy signed with a hash of
// (start, last-seen, this machine's ID, the public salt) and obfuscated with the
// machine ID, so it can't be edited, copied from another PC, or reset by
// deleting a single file. Setting the clock back is caught by the "last seen"
// stamp. An offline trial can never be bullet-proof, but this stops casual resets.
namespace
{
    constexpr juce::int64 kDayMs        = 24LL * 60 * 60 * 1000;
    constexpr juce::int64 kClockSlackMs = 2LL * 60 * 60 * 1000;   // tolerate small clock corrections
}

juce::Array<juce::File> AELicenseManager::getTrialFiles() const
{
    if (testRoot() != juce::File())
        return { testRoot().getChildFile("trial.dat"),
                 testRoot().getChildFile(".amarilabs").getChildFile("ae-session.dat") };

    return { juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile("ArcaneEclipse").getChildFile("trial.dat"),
             juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                 .getChildFile(".amarilabs").getChildFile("ae-session.dat") };
}

juce::String AELicenseManager::trialSignature(juce::int64 start, juce::int64 seen) const
{
    juce::String s = juce::String(start) + "|" + juce::String(seen) + "|"
                   + getMachineID() + "|" + saltHex() + "|ae-trial-v1";
    juce::SHA256 sha(s.toUTF8(), s.getNumBytesAsUTF8());
    return sha.toHexString();
}

juce::MemoryBlock AELicenseManager::xorWithMachine(const void* data, size_t size) const
{
    juce::String mid = getMachineID();
    const char* key = mid.toRawUTF8();
    const size_t keyLen = strlen(key);
    juce::MemoryBlock out(size);
    auto* s = (const uint8_t*) data;
    auto* d = (uint8_t*) out.getData();
    for (size_t i = 0; i < size; ++i)
        d[i] = s[i] ^ (uint8_t) key[i % keyLen];
    return out;
}

void AELicenseManager::writeTrialRecord(juce::int64 start, juce::int64 seen)
{
    juce::XmlElement root("AETrial");
    root.setAttribute("start", juce::String(start));
    root.setAttribute("seen",  juce::String(seen));
    root.setAttribute("sig",   trialSignature(start, seen));
    juce::String xml = root.toString();
    auto enc = xorWithMachine(xml.toRawUTF8(), xml.getNumBytesAsUTF8());

    for (auto& f : getTrialFiles())
    {
        f.getParentDirectory().createDirectory();
        f.replaceWithData(enc.getData(), enc.getSize());
    }
}

void AELicenseManager::refreshAt(juce::int64 now)
{
    loadFromDisk();

    juce::int64 start = 0, seen = 0;
    bool found = false, bad = false;

    for (auto& f : getTrialFiles())
    {
        if (!f.existsAsFile()) continue;
        juce::MemoryBlock enc;
        if (!f.loadFileAsData(enc) || enc.getSize() == 0) { bad = true; continue; }
        auto dec = xorWithMachine(enc.getData(), enc.getSize());
        auto elem = juce::parseXML(juce::String::fromUTF8((const char*) dec.getData(), (int) dec.getSize()));
        if (elem == nullptr || elem->getTagName() != "AETrial") { bad = true; continue; }

        const juce::int64 s = elem->getStringAttribute("start").getLargeIntValue();
        const juce::int64 v = elem->getStringAttribute("seen").getLargeIntValue();
        if (s <= 0 || v < s || elem->getStringAttribute("sig") != trialSignature(s, v)) { bad = true; continue; }

        start = found ? juce::jmin(start, s) : s;    // earliest start wins
        seen  = found ? juce::jmax(seen, v)  : v;    // latest "last seen" wins
        found = true;
    }

    if (found && !bad)
    {
        if (start > now + kClockSlackMs || seen > now + kClockSlackMs)
            bad = true;                              // clock was set back
        else
            writeTrialRecord(start, juce::jmax(seen, now));   // refresh stamp + restore a deleted copy
    }

    trialRecord = found || bad;
    trialBad    = bad;
    trialStart  = start;
    trialEnd.store((found && !bad) ? start + (juce::int64) kAETrialDays * kDayMs : 0);
}

bool AELicenseManager::startTrialAt(juce::int64 now)
{
    if (trialRecord) return false;                   // one trial per machine
    writeTrialRecord(now, now);
    trialRecord = true;
    trialBad    = false;
    trialStart  = now;
    trialEnd.store(now + (juce::int64) kAETrialDays * kDayMs);
    return true;
}

AEAccess AELicenseManager::getAccessAt(juce::int64 now) const
{
    if (licensedFlag.load())  return AEAccess::Licensed;
    if (!trialRecord)         return AEAccess::TrialAvailable;
    if (trialBad || now >= trialEnd.load()) return AEAccess::TrialExpired;
    return AEAccess::TrialActive;
}

int AELicenseManager::trialDaysLeftAt(juce::int64 now) const
{
    const juce::int64 end = trialEnd.load();
    if (end <= now) return 0;
    return (int) ((end - now + kDayMs - 1) / kDayMs);    // whole days, rounded up
}
