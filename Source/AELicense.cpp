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
    return lic.valid;
}
