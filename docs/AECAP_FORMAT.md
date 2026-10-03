# `.aecap` — Arcane Eclipse Capture container format

**Version 1** · Amari Labs · a self-contained container that wraps a NAM model
(plus an optional cabinet IR and a preset) into one file your plugins own.

The goal is **packaging you control**, not unbreakable DRM. It lets you ship
factory tones and future artist packs as a single file that isn't a plain
`.nam` sitting on disk, and gives every future Amari Labs product one format to
read. A determined user who owns the plugin can still recover the model (the key
ships inside the binary) — this deters casual copy/re-share, nothing more. Only
package captures you **own or are licensed** to distribute.

---

## Byte layout

All integers are **little-endian**. Offsets are from the start of the file.

### File header (8 bytes)

| Offset | Size | Field        | Value                            |
|-------:|-----:|--------------|----------------------------------|
| 0      | 4    | Magic        | ASCII `AECP` (`41 45 43 50`)     |
| 4      | 2    | Version      | `uint16` = `1`                   |
| 6      | 1    | Reserved     | `0`                              |
| 7      | 1    | Reserved     | `0`                              |

### Sections (repeat until CRC)

Each section is a 9-byte header followed by its payload:

| Size | Field        | Notes                                                        |
|-----:|--------------|--------------------------------------------------------------|
| 4    | Type         | 4 ASCII chars (see below)                                    |
| 1    | Flags        | bit0 = zlib-compressed, bit1 = obfuscated                    |
| 4    | Stored length| `uint32` — length of the payload bytes **as stored**         |
| N    | Payload      | `Stored length` bytes                                        |

Reader loop: read 9-byte header; if the 4 type bytes are not a known section,
this is the CRC trailer — stop. Otherwise read `Stored length` bytes and advance.

**Section types**

| Type   | Required | Contents (raw, before flags applied)                          |
|--------|----------|---------------------------------------------------------------|
| `META` | yes      | UTF-8 JSON: tone metadata (see below). Kept plain for browsing.|
| `MODL` | yes      | The `.nam` file bytes (JSON). Compressed + obfuscated.         |
| `IRWV` | no       | A `.wav` cabinet IR, verbatim file bytes. Compressed + obfusc. |
| `PRST` | no       | UTF-8 JSON: default knob/param values for this tone. Plain.    |

Unknown section types are **skipped** by readers (forward compatibility): a
future product can add e.g. `MOD2`/`IR2R` for dual-amp packs and old readers
ignore them.

### CRC trailer (4 bytes)

`uint32` **CRC-32 (IEEE, same as `zlib.crc32`)** computed over **every byte from
offset 0 up to but not including these 4 bytes**. Readers recompute and compare;
mismatch = corrupt/tampered file → refuse to load.

---

## `META` JSON schema

```json
{
  "format": "aecap",
  "format_version": 1,
  "tone_name": "Amari - Lead V4",
  "gear": "Custom high-gain head",
  "author": "Amari Labs",
  "license": "Proprietary - Amari Labs. Not for redistribution.",
  "sample_rate": 48000,
  "has_ir": true,
  "has_preset": true,
  "created": "2026-09-26",
  "notes": ""
}
```

`sample_rate` is the NAM model's native rate (NAM A2 captures are typically
48 kHz — matches Arcane Eclipse's 48 kHz path). Everything except `format`,
`format_version`, and `tone_name` is optional.

## `PRST` JSON schema (optional)

Free-form map of your plugin's parameter IDs to default values, e.g.:

```json
{ "gain": 0.72, "bass": 0.5, "mid": 0.55, "treble": 0.6,
  "presence": 0.5, "master": 0.7, "reverbMix": 0.3 }
```

This is exactly the shape your deferred per-slot `.aetone` sharing feature
needs — a single-slot `.aecap` is just META + PRST (+ optional MODL/IRWV).

---

## Compression + obfuscation

Applied only to `MODL` and `IRWV` payloads (flags bit0 + bit1 set). `META` and
`PRST` stay plain so a library browser can read them cheaply.

**Write order:** raw bytes → zlib compress → obfuscate → store.
**Read order:** stored → deobfuscate → zlib decompress → raw bytes.

- **Compression:** zlib (RFC 1950 — Python `zlib.compress`, JUCE
  `GZIPDecompressorInputStream` with `zlibFormat`).
- **Obfuscation:** XOR against a keystream from a 64-bit xorshift64\* PRNG whose
  seed is `FNV-1a-64(secret_key_bytes ++ section_type_bytes)`. Defined precisely
  below; identical in Python and C++.

### Keystream (must match byte-for-byte in both languages)

```
seed = FNV1a64( SECRET_KEY_BYTES followed by the 4 section-type bytes )
    FNV1a64: h = 0xcbf29ce484222325
             for each byte b: h = ((h XOR b) * 0x100000001b3) mod 2^64
if seed == 0: seed = 0x9E3779B97F4A7C15

state = seed
next 8 keystream bytes:
    x = state
    x ^= x >> 12
    x ^= x << 25          (all math mod 2^64)
    x ^= x >> 27
    state = x
    out = (x * 0x2545F4914F6CDD1D) mod 2^64
    emit out as 8 little-endian bytes
XOR payload[i] with keystream[i].
```

### The secret key

`SECRET_KEY` is a shared constant defined **identically** in the packer and in
the plugin loader. Change it once, in both places, before you ship — and keep it
out of any public repo. Rotating it makes old `.aecap` files unreadable by the
new build, so pick it before your first pack.

Default placeholder (REPLACE THIS): `AMARI-LABS-AECAP-v1-REPLACE-ME`
