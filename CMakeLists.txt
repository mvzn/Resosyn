cmake_minimum_required(VERSION 3.22)

project(Resosyn VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS deployment target")

# ─────────────────────────────────────────────────────────────────────────────
# JUCE via FetchContent
# Pin to a specific tag for reproducible CI builds. Bump when ready.
# ─────────────────────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(JUCE)

# ─────────────────────────────────────────────────────────────────────────────
# Plugin target — mirrors Resosyn.jucer
# ─────────────────────────────────────────────────────────────────────────────
juce_add_plugin(Resosyn
    PRODUCT_NAME                "Resosyn"
    VERSION                     "1.0.0"
    COMPANY_NAME                "yourcompany"
    COMPANY_WEBSITE             "www.yourcompany.com"
    BUNDLE_ID                   "com.yourcompany.Resosyn"
    PLUGIN_MANUFACTURER_CODE    "Manu"        # 0x4d616e75
    PLUGIN_CODE                 "P5ct"        # 0x50356374
    FORMATS                     VST3 Standalone AU
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Instrument" "Synth"
    AU_MAIN_TYPE                "kAudioUnitType_MusicDevice"
)

# Generates JuceHeader.h — all Source/ files include it.
juce_generate_juce_header(Resosyn)

target_sources(Resosyn PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# Listed for IDE visibility; not compiled directly (included header-only via .cpp).
set(RESOSYN_HEADERS
    Source/PluginProcessor.h
    Source/PluginEditor.h
    Source/HarmonicEditorContent.h
    Source/FilterResponseDisplay.h
    Source/DSP/FilterBank.h
    Source/DSP/NoiseGenerator.h
    Source/DSP/Voice.h
    Source/DSP/VoiceManager.h
    Source/DSP/VoiceParameters.h
    Source/DSP/WavetableOscillator.h
)
target_sources(Resosyn PRIVATE ${RESOSYN_HEADERS})

target_compile_definitions(Resosyn PRIVATE
    JUCE_WEB_BROWSER=0                # avoids webkit2gtk dependency on Linux
    JUCE_USE_CURL=0                   # not needed; skips libcurl link
    JUCE_VST3_CAN_REPLACE_VST2=0      # from JUCEOPTIONS in .jucer
    JUCE_STRICT_REFCOUNTEDPOINTER=1   # from JUCEOPTIONS in .jucer
)

target_link_libraries(Resosyn
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_midi_ci
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
