cmake_minimum_required(VERSION 3.22)
project(ArcaneEclipse VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Optimisation flags (critical for real-time NAM inference)
if (MSVC)
    add_compile_options(/O2 /arch:AVX2)
else()
    add_compile_options(-O3 -march=native)
endif()

# --- JUCE
add_subdirectory(JUCE)

# --- Disable RTNeural's problematic AVX512 FP16 path on GCC
add_definitions(-DEIGEN_DONT_VECTORIZE_FP16)
add_definitions(-DXSIMD_NO_SUPPORTED_ARCHITECTURE=0)

# --- NeuralAudio (NAM inference engine)
set(BUILD_NAMCORE ON CACHE BOOL "" FORCE)
set(NAM_ENABLE_A2_FAST ON CACHE BOOL "" FORCE)
set(BUILD_INTERNAL_STATIC_WAVENET ON CACHE BOOL "" FORCE)
set(BUILD_STATIC_INTERNAL_NAMA2 ON CACHE BOOL "" FORCE)
set(BUILD_UTILS OFF CACHE BOOL "" FORCE)
add_subdirectory(NeuralAudio)

# --- Plugin target
juce_add_plugin(ArcaneEclipse
    PRODUCT_NAME        "Arcane Eclipse"
    PLUGIN_MANUFACTURER_CODE Dpec
    PLUGIN_CODE         NmPl
    FORMATS             VST3 Standalone
    IS_SYNTH            FALSE
    NEEDS_MIDI_INPUT    FALSE
    NEEDS_MIDI_OUTPUT   FALSE
    IS_MIDI_EFFECT      FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES     "Fx" "Distortion"
)

target_sources(ArcaneEclipse PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Compressor.h
    Source/Overdrive.h
    Source/Modulation.h
    Source/RoomReverb.h
    Source/StandardDelay.h
)

target_compile_definitions(ArcaneEclipse PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    NAM_SAMPLE_FLOAT
    DSP_SAMPLE_FLOAT
)

juce_add_binary_data(ArcaneEclipseAssets SOURCES
    Resources/background.png
    Resources/knobs/knob_strip.png
    Resources/knobs/knob_amp.png
    Resources/knobs/knob_fx.png
    Resources/icons/00_GATE.png
    Resources/icons/01_COMP.png
    Resources/icons/02_DRIVE.png
    Resources/icons/03_AMP.png
    Resources/icons/04_CAB.png
    Resources/icons/05_EQ.png
    Resources/icons/06_MOD.png
    Resources/icons/07_DELAY.png
    Resources/icons/08_REVERB.png)

target_link_libraries(ArcaneEclipse PRIVATE
    ArcaneEclipseAssets
    juce::juce_audio_utils
    juce::juce_dsp
    NeuralAudio
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_include_directories(ArcaneEclipse PRIVATE
    NeuralAudio/NeuralAudio
)

# Build Fri Sep  4 03:21:44 UTC 2026
