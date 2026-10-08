/*
    AecapLoader.h  --  reader for the .aecap capture container (format v1)
    Amari Labs / Arcane Eclipse.  Header-only, JUCE-based.

    Parses a .aecap file, verifies its CRC, deobfuscates + decompresses the
    NAM model (and optional cabinet IR), and hands you:
        - meta   : parsed metadata (tone name, license, sample rate, ...)
        - model  : the raw .nam bytes  -> feed to NeuralAudio
        - ir     : the raw .wav bytes  -> feed to your IR loader (or decode here)
        - preset : parsed default param map (optional)

    The keystream + CRC32 below are byte-for-byte verified against the Python
    packer (aecap_pack.py). Decompression uses JUCE's bundled zlib via
    GZIPDecompressorInputStream(zlibFormat) -- no extra dependencies.

    IMPORTANT: kAecapSecretKey MUST match SECRET_KEY in aecap_common.py.
*/

#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>

// ==========================================================================
//  SECRET KEY -- keep identical to the Python packer; keep out of public repos.
// ==========================================================================
// The real key is NOT stored in this (public) repo. CI injects it at build time
// from the AECAP_SECRET_KEY GitHub Actions secret (see CMakeLists.txt). It must
// match SECRET_KEY in tools/aecap_common.py (kept private).
#ifndef AECAP_SECRET_KEY
 #define AECAP_SECRET_KEY "AMARI-LABS-AECAP-v1-REPLACE-ME"
#endif
static const char* const kAecapSecretKey = AECAP_SECRET_KEY;

// ==========================================================================
class AecapLoader
{
public:
    struct Contents
    {
        bool ok = false;
        juce::String error;

        // ---- metadata (from META) ----
        juce::var    meta;                 // full parsed JSON object
        juce::String toneName, gear, author, license, notes, created;
        double       sampleRate = 48000.0;

        // ---- model (from MODL) ----
        bool             hasModel = false;
        juce::MemoryBlock modelData;       // raw .nam (UTF-8 JSON) bytes

        // ---- IR (from IRWV, optional) ----
        bool             hasIR = false;
        juce::MemoryBlock irWav;           // raw .wav file bytes

        // ---- preset (from PRST, optional) ----
        bool      hasPreset = false;
        juce::var preset;                  // parsed param map

        // Convenience: model bytes as a String (for a from-string NAM API).
        juce::String modelJsonString() const
        {
            return juce::String::fromUTF8(static_cast<const char*>(modelData.getData()),
                                          (int) modelData.getSize());
        }

        // Write the model to a real .nam file so it can be loaded by the
        // common NeuralAudio::NeuralModel::CreateFromFile(path) API.
        // cacheDir is created if needed; returns the written file (or a
        // non-existent File on failure).
        juce::File materializeModelTo (const juce::File& cacheDir) const
        {
            if (! hasModel) return {};
            cacheDir.createDirectory();
            auto f = cacheDir.getChildFile ("aecap_" + juce::String (juce::Time::getHighResolutionTicks())
                                            + ".nam");
            if (f.replaceWithData (modelData.getData(), modelData.getSize()))
                return f;
            return {};
        }

        // Decode the bundled IR wav into an AudioBuffer (optional helper).
        // Returns true on success; fills buffer + irSampleRate.
        bool decodeIR (juce::AudioBuffer<float>& buffer, double& irSampleRate) const
        {
            if (! hasIR) return false;
            juce::WavAudioFormat wav;
            auto* mis = new juce::MemoryInputStream (irWav.getData(), irWav.getSize(), false);
            std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (mis, true));
            if (reader == nullptr) return false;
            buffer.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
            reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
            irSampleRate = reader->sampleRate;
            return true;
        }
    };

    // ----------------------------------------------------------------------
    static Contents loadFile (const juce::File& file)
    {
        Contents c;
        juce::MemoryBlock blob;
        if (! file.loadFileAsData (blob))
        {
            c.error = "cannot read file: " + file.getFullPathName();
            return c;
        }
        return loadData (blob);
    }

    static Contents loadData (const juce::MemoryBlock& blob)
    {
        Contents c;
        const auto* b = static_cast<const juce::uint8*> (blob.getData());
        const size_t n = blob.getSize();

        if (n < 12 || std::memcmp (b, "AECP", 4) != 0)
        {
            c.error = "not a .aecap file (bad magic)";
            return c;
        }

        // CRC over all but the trailing 4 bytes
        const juce::uint32 crcStored = rd32 (b + n - 4);
        const juce::uint32 crcCalc   = crc32ieee (b, n - 4);
        if (crcStored != crcCalc)
        {
            c.error = "CRC mismatch (file corrupt or tampered)";
            return c;
        }

        size_t pos = 8;
        const size_t end = n - 4;
        while (pos + 9 <= end)
        {
            char type[4];   std::memcpy (type, b + pos, 4);
            const juce::uint8  flags = b[pos + 4];
            const juce::uint32 slen  = rd32 (b + pos + 5);
            pos += 9;
            if (pos + slen > end) break;

            juce::MemoryBlock stored (b + pos, slen);
            pos += slen;

            const bool obf  = (flags & 0x02) != 0;
            const bool comp = (flags & 0x01) != 0;

            if (std::memcmp (type, "META", 4) == 0)
            {
                auto raw = decodeSection ("META", stored, obf, comp);
                c.meta = juce::JSON::parse (blockToString (raw));
                c.toneName   = c.meta.getProperty ("tone_name", "").toString();
                c.gear       = c.meta.getProperty ("gear", "").toString();
                c.author     = c.meta.getProperty ("author", "").toString();
                c.license    = c.meta.getProperty ("license", "").toString();
                c.notes      = c.meta.getProperty ("notes", "").toString();
                c.created    = c.meta.getProperty ("created", "").toString();
                c.sampleRate = (double) c.meta.getProperty ("sample_rate", 48000.0);
            }
            else if (std::memcmp (type, "MODL", 4) == 0)
            {
                c.modelData = decodeSection ("MODL", stored, obf, comp);
                c.hasModel  = c.modelData.getSize() > 0;
            }
            else if (std::memcmp (type, "IRWV", 4) == 0)
            {
                c.irWav = decodeSection ("IRWV", stored, obf, comp);
                c.hasIR = c.irWav.getSize() > 0;
            }
            else if (std::memcmp (type, "PRST", 4) == 0)
            {
                auto raw = decodeSection ("PRST", stored, obf, comp);
                c.preset = juce::JSON::parse (blockToString (raw));
                c.hasPreset = ! c.preset.isVoid();
            }
            // unknown section types are skipped (forward compatibility)
        }

        if (! c.hasModel)
        {
            c.error = "no model section in file";
            return c;
        }
        c.ok = true;
        return c;
    }

private:
    // ---- little-endian reads ----
    static juce::uint32 rd32 (const juce::uint8* p)
    {
        return (juce::uint32) p[0] | ((juce::uint32) p[1] << 8)
             | ((juce::uint32) p[2] << 16) | ((juce::uint32) p[3] << 24);
    }

    // ---- FNV-1a 64 ----
    static juce::uint64 fnv1a64 (const juce::uint8* data, size_t n,
                                 juce::uint64 h = 0xcbf29ce484222325ULL)
    {
        for (size_t i = 0; i < n; ++i) { h ^= data[i]; h *= 0x100000001b3ULL; }
        return h;
    }

    // ---- xorshift64* keystream XOR (symmetric: same call encodes/decodes) ----
    static void obfuscate (const char type[4], juce::MemoryBlock& buf)
    {
        juce::uint64 seed = fnv1a64 ((const juce::uint8*) kAecapSecretKey,
                                     std::strlen (kAecapSecretKey));
        seed = fnv1a64 ((const juce::uint8*) type, 4, seed);
        if (seed == 0) seed = 0x9E3779B97F4A7C15ULL;

        juce::uint64 state = seed;
        auto* p = static_cast<juce::uint8*> (buf.getData());
        const size_t len = buf.getSize();
        size_t i = 0;
        while (i < len)
        {
            juce::uint64 x = state;
            x ^= x >> 12; x ^= x << 25; x ^= x >> 27; state = x;
            juce::uint64 out = x * 0x2545F4914F6CDD1DULL;
            for (int bshift = 0; bshift < 8 && i < len; ++bshift, ++i)
                p[i] ^= (juce::uint8) (out >> (8 * bshift));
        }
    }

    // ---- CRC-32 IEEE (matches Python zlib.crc32) ----
    static juce::uint32 crc32ieee (const juce::uint8* data, size_t n)
    {
        static juce::uint32 table[256];
        static bool init = false;
        if (! init)
        {
            for (juce::uint32 i = 0; i < 256; ++i)
            {
                juce::uint32 cc = i;
                for (int k = 0; k < 8; ++k)
                    cc = (cc & 1) ? (0xEDB88320u ^ (cc >> 1)) : (cc >> 1);
                table[i] = cc;
            }
            init = true;
        }
        juce::uint32 cc = 0xFFFFFFFFu;
        for (size_t i = 0; i < n; ++i)
            cc = table[(cc ^ data[i]) & 0xFF] ^ (cc >> 8);
        return cc ^ 0xFFFFFFFFu;
    }

    // ---- zlib inflate via JUCE (zlibFormat == Python zlib.compress) ----
    static juce::MemoryBlock zlibInflate (const juce::MemoryBlock& comp)
    {
        juce::MemoryInputStream mis (comp.getData(), comp.getSize(), false);
        juce::GZIPDecompressorInputStream gz (&mis, false,
            juce::GZIPDecompressorInputStream::zlibFormat);
        juce::MemoryBlock out;
        gz.readIntoMemoryBlock (out);
        return out;
    }

    static juce::MemoryBlock decodeSection (const char type[4],
                                            juce::MemoryBlock stored,
                                            bool obf, bool comp)
    {
        if (obf)  obfuscate (type, stored);
        if (comp) return zlibInflate (stored);
        return stored;
    }

    static juce::String blockToString (const juce::MemoryBlock& mb)
    {
        return juce::String::fromUTF8 (static_cast<const char*> (mb.getData()),
                                       (int) mb.getSize());
    }
};
