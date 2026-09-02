# NAM Player — VST3

A JUCE-based VST3 plugin that loads Neural Amp Modeler (NAM) `.nam` captures
for real-time amp simulation, combined with a convolution IR loader for cabinet simulation.

## Features

- **NAM amp loader** — loads `.nam` files including A2 "SlimmableContainer" format via the NeuralAudio library
- **IR cab loader** — loads any `.wav` impulse response for cabinet simulation via JUCE's built-in convolution engine
- **Noise gate** — simple envelope-follower gate before the amp stage
- **3-band EQ** — Bass / Mid / Treble post-amp, pre-cab
- **Input / Output gain** — independent gain staging around the amp model
- **Cab bypass** — bypass the IR convolution to use the amp model dry or with your own cab

## Where to get NAM models

Thousands of free A2 captures from real amps are available on:
- [ToneHunt](https://tonehunt.org) — free community NAM models
- [TONE3000](https://tone3000.com) — another large free library

## Building via GitHub Actions (recommended)

Push this project to a GitHub repo — the `.github/workflows/build.yml` workflow
builds the Windows and macOS VST3 automatically. Download the artifact zip from
the Actions tab once the run finishes (green checkmark), extract, and install.

## Signal chain

```
Input → Input Gain → Noise Gate → NAM Model → 3-Band EQ → IR Cab Sim → Output Gain
```

Each stage is independent. The NAM model internally resamples to 48kHz (its
native rate) and back, so it works correctly at any DAW sample rate.

## Dependencies

- JUCE 8.0.12
- NeuralAudio (mikeoliphant) — included as a subfolder with its own submodules
