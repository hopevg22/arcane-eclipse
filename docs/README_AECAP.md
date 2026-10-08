# `.aecap` toolkit — Arcane Eclipse Capture

Your own container format that wraps a **NAM model + optional cabinet IR +
preset** into a single file your plugins own. One format for Arcane Eclipse
factory tones today, future updates, artist signature packs, and per-slot tone
sharing.

**Contents**

```
AECAP_FORMAT.md         the byte-level spec
Source/AecapLoader.h    header-only JUCE reader — drop into your plugin
tools/aecap_common.py   shared codec (keystream, CRC, container r/w)
tools/aecap_pack.py     CLI: make a .aecap from .nam (+ .wav IR + preset)
tools/aecap_unpack.py   CLI: inspect / extract a .aecap (QA tool)
tools/aecap_ctest.cpp   standalone C++ codec test (already verified)
```

Verified: the Python packer and the C++ codec agree byte-for-byte (CRC,
keystream, zlib), and packing → unpacking reproduces the original `.nam` and
`.wav` exactly.

---

## 0. Before anything — set your secret key

Open **`tools/aecap_common.py`** and **`Source/AecapLoader.h`** and change this
to the same value in both, then keep it out of any public repo:

```
SECRET_KEY = b"AMARI-LABS-AECAP-v1-REPLACE-ME"      # aecap_common.py
kAecapSecretKey = "AMARI-LABS-AECAP-v1-REPLACE-ME"  // AecapLoader.h
```

Pick it once. Changing it later makes previously packed `.aecap` files
unreadable by the new build.

---

## 1. Packing tones (your machine / Colab)

Only pack captures you **own or are licensed** to ship.

```bash
python tools/aecap_pack.py \
    --model "Amari - Lead V4.nam" \
    --ir    "MESA - 4x12.wav" \
    --preset lead_v4.json \
    --name  "Amari - Lead V4" \
    --gear  "Custom high-gain head" \
    --author "Amari Labs" \
    --license "Proprietary - Amari Labs. Not for redistribution." \
    --out   "Amari - Lead V4.aecap"
```

`--ir` and `--preset` are optional. `preset.json` is just a param map, e.g.
`{"gain":0.72,"bass":0.5,"master":0.7}`. Inspect any file with:

```bash
python tools/aecap_unpack.py "Amari - Lead V4.aecap"
```

For your 4 factory presets, pack four `.aecap` files and ship them where the
installer currently drops the `.nam`/`.wav` (e.g. `C:\ProgramData\Amari Labs\
Arcane Eclipse\Models`). One `.aecap` replaces the model+IR+params trio.

---

## 2. Loading in the plugin

Add `Source/AecapLoader.h` to your CMake sources (it's header-only; it needs
`juce_audio_formats`, which you already use for IR loading).

```cpp
#include "AecapLoader.h"

void ArcaneEclipseAudioProcessor::loadAecap (const juce::File& file)
{
    auto cap = AecapLoader::loadFile (file);
    if (! cap.ok) { /* show cap.error in the MODEL box */ return; }

    // ---- model -> NeuralAudio ----
    // Path A (if your NeuralAudio build can load from a string/buffer):
    //     auto* m = NeuralAudio::NeuralModel::CreateFromString (cap.modelJsonString());
    //
    // Path B (works with the common CreateFromFile API — recommended default):
    auto cacheDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                        .getChildFile ("ArcaneEclipse").getChildFile ("cache");
    juce::File namFile = cap.materializeModelTo (cacheDir);
    auto* model = NeuralAudio::NeuralModel::CreateFromFile (namFile.getFullPathName().toStdString());
    // (optionally namFile.deleteFile() after the model has loaded)

    // ---- cabinet IR (optional) ----
    if (cap.hasIR)
    {
        juce::AudioBuffer<float> irBuf; double irSr = 0;
        if (cap.decodeIR (irBuf, irSr))
            loadCabinetIR (irBuf, irSr);         // your existing IR path
        // or feed cap.irWav (raw .wav bytes) straight to your current loader
    }

    // ---- default params (optional) ----
    if (cap.hasPreset)
    {
        auto set = [&](const char* id) {
            if (cap.preset.hasProperty (id))
                if (auto* p = apvts.getParameter (id))
                    p->setValueNotifyingHost ((float) cap.preset.getProperty (id, p->getValue()));
        };
        set ("gain"); set ("bass"); set ("mid"); set ("treble");
        set ("presence"); set ("master"); set ("reverbMix");
        // ...whatever param IDs you packed
    }

    // metadata for the UI:  cap.toneName, cap.license, cap.sampleRate
}
```

**Which model path?** Path B (temp `.nam` in a private cache dir) is the safe
default and works with the standard `CreateFromFile`. If your NeuralAudio
version exposes a create-from-string/buffer call, use Path A and the `.nam`
never touches disk — better protection for paid packs. Check your NeuralAudio
headers for a `CreateFrom...` that takes a string/`std::vector<char>`; if there
is one, switch to it and delete the Path B block.

### Wiring the button

Point your existing **LOAD MODEL** button at `.aecap` (add the extension to the
`FileChooser` filter, e.g. `"*.aecap;*.nam"`). Keep plain `.nam` support too so
users can still load their own captures — the loader is additive.

---

## 3. Reusing this for future products

- **Artist signature packs:** pack each artist tone as a `.aecap` with
  `author`/`license` set to the artist; your pack installer just copies files.
- **Per-slot tone sharing (`.aetone`):** a single-slot share is a `.aecap` with
  only `META` + `PRST` (and optionally `MODL`/`IRWV`). Same loader reads it.
- **Dual-amp / new features:** add new section types (e.g. `MOD2`, `IR2R`) —
  older builds skip unknown sections, so old files and new files stay
  compatible in both directions.

---

## Honest note on protection

The key ships inside the plugin, so a determined owner can still recover the
model — this is obfuscation, not unbreakable DRM. It stops casual one-click
copying and keeps your factory/artist tones from being trivially re-shared as
raw `.nam`. That's the same realistic posture as your offline signed-key
licensing, and it's the right bar for an affordable product. It does **not**
change anyone's rights in a capture: only package tones you own or are licensed
to distribute.
