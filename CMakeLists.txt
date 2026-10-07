cmake_minimum_required(VERSION 3.22)
project(ArcaneEclipse VERSION 1.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Optimisation flags (critical for real-time NAM inference)
if (MSVC)
    add_compile_options(/O2 /arch:AVX2)
elseif (APPLE)
    # macOS: -march=native is invalid on Apple clang and breaks the build.
    add_compile_options(-O3)
    # Apple Silicon (arm64) only; arm64 macOS requires deployment target >= 11.0.
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
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
    COMPANY_NAME        "Amari Labs"
    COMPANY_COPYRIGHT   "Copyright (c) 2026 Amari Labs"
    ICON_BIG            "Resources/app_icon_512.png"
    PLUGIN_MANUFACTURER_CODE Amar
    PLUGIN_CODE         NmPl
    FORMATS             AU VST3 Standalone
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
    Source/AELicense.h
    Source/AELicense.cpp
    Source/AEActivationDialog.h
    Source/AEActivationDialog.cpp
    Source/Compressor.h
    Source/Overdrive.h
    Source/Modulation.h
    Source/RoomReverb.h
    Source/StandardDelay.h
    Source/Doubler.h
    Source/AecapLoader.h
)

target_compile_definitions(ArcaneEclipse PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    NAM_SAMPLE_FLOAT
    DSP_SAMPLE_FLOAT
)

# ---- ASIO support for the Standalone (Windows only) ----
# Pass -DASIOSDK_DIR=<path-to-asiosdk> at configure time (the CI does this).
# When present, the Standalone app gets a low-latency ASIO driver option.
if(WIN32 AND ASIOSDK_DIR AND EXISTS "${ASIOSDK_DIR}/common/iasiodrv.h")
    message(STATUS "ASIO SDK found at ${ASIOSDK_DIR} - enabling ASIO")
    target_compile_definitions(ArcaneEclipse PUBLIC JUCE_ASIO=1)
    target_include_directories(ArcaneEclipse PUBLIC "${ASIOSDK_DIR}/common")
else()
    message(STATUS "ASIO SDK not provided - building without ASIO")
endif()

# .aecap secret key: injected from the environment (GitHub Actions secret
# AECAP_SECRET_KEY) so it never lives in this public repo.
if(NOT "$ENV{AECAP_SECRET_KEY}" STREQUAL "")
    target_compile_definitions(ArcaneEclipse PRIVATE AECAP_SECRET_KEY="$ENV{AECAP_SECRET_KEY}")
    message(STATUS "AECAP secret key: provided via environment")
else()
    message(STATUS "AECAP secret key: NOT set - .aecap files will not open in this build")
endif()

juce_add_binary_data(ArcaneEclipseAssets SOURCES
    Resources/background.png
    Resources/knobs/knob_v11.png
    Resources/logo_amari_mark_v2.png
    Resources/fonts/Cinzel-SemiBold.ttf
    Resources/cabs/cab_eclipse_1x12.wav
    Resources/cabs/cab_eclipse_2x12.wav
    Resources/cabs/cab_eclipse_4x12.wav
    Resources/cabs/cab_eclipse_1x12_v2.wav
    Resources/cabs/cab_eclipse_1x12_v3.wav)

target_link_libraries(ArcaneEclipse PRIVATE
    ArcaneEclipseAssets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_cryptography
    NeuralAudio
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_include_directories(ArcaneEclipse PRIVATE
    NeuralAudio/NeuralAudio
)

# Build Fri Sep  4 03:21:44 UTC 2026
