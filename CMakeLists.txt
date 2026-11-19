cmake_minimum_required(VERSION 3.22)

project(Mutaform VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(MSVC)
    # Static CRT so the plugin has no VC++ redistributable dependency
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/MP /utf-8)
endif()

option(MUTAFORM_BUILD_TESTS "Build unit tests" ON)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# ---------------------------------------------------------------------------
# JUCE 8
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.17-security
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# WebView2 SDK (Windows only) — JUCE's FindWebView2 looks for a
# *Microsoft.Web.WebView2* directory inside JUCE_WEBVIEW2_PACKAGE_LOCATION.
if(WIN32)
    set(MUTAFORM_WEBVIEW2_VERSION 1.0.3485.44)
    FetchContent_Declare(webview2
        URL https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${MUTAFORM_WEBVIEW2_VERSION}
        DOWNLOAD_NAME webview2.zip
        SOURCE_DIR ${CMAKE_BINARY_DIR}/nuget/Microsoft.Web.WebView2.${MUTAFORM_WEBVIEW2_VERSION}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(webview2)
    set(JUCE_WEBVIEW2_PACKAGE_LOCATION ${CMAKE_BINARY_DIR}/nuget CACHE PATH "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Plugin
juce_add_plugin(Mutaform
    COMPANY_NAME "Mutaform Audio"
    COMPANY_WEBSITE "https://github.com/mogenlord23/vst"
    BUNDLE_ID com.mutaformaudio.mutaform
    PLUGIN_MANUFACTURER_CODE Mtfm
    PLUGIN_CODE Mtf1
    PRODUCT_NAME "Mutaform"
    DESCRIPTION "One-shot sample evolver instrument"
    FORMATS VST3 Standalone
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    VST3_CATEGORIES Instrument Sampler
    COPY_PLUGIN_AFTER_BUILD FALSE)

# UI resources embedded into the binary. The JUCE frontend helper library is taken
# straight from the fetched JUCE sources (not copied into this repository).
set(MUTAFORM_UI_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/index.html
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/style.css
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/app.js
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/creature3d.js
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/vendor/three.module.min.js
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/fonts/PixelifySans.ttf
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/fonts/Silkscreen-Regular.ttf
    ${juce_SOURCE_DIR}/modules/juce_gui_extra/native/javascript/index.js
    ${juce_SOURCE_DIR}/modules/juce_gui_extra/native/javascript/check_native_interop.js)
juce_add_binary_data(MutaformUI
    HEADER_NAME MutaformUI.h
    NAMESPACE MutaformUI
    SOURCES ${MUTAFORM_UI_FILES})
set_target_properties(MutaformUI PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(MUTAFORM_CORE_SOURCES
    src/core/Params.cpp
    src/core/SampleData.cpp
    src/core/SamplerEngine.cpp
    src/core/Evolver.cpp
    src/core/FxChain.cpp
    src/core/SoundFeatures.cpp
    src/core/Melody.cpp
    src/core/Genoform.cpp
    src/core/Sequencer.cpp
    src/core/Presets.cpp
    src/core/FactoryPresets.cpp)

function(mutaform_configure tgt isfx)
target_sources(${tgt} PRIVATE
    ${MUTAFORM_CORE_SOURCES}
    src/plugin/PluginProcessor.cpp
    src/plugin/PluginEditor.cpp)

target_include_directories(${tgt} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(${tgt}
    PRIVATE
        MUTAFORM_FX=${isfx}
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_USE_FLAC=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_SILENCE_XCODE_15_LINKER_WARNING=1)

target_link_libraries(${tgt}
    PRIVATE
        MutaformUI
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

endfunction()

mutaform_configure(Mutaform 0)

# Effect edition: same code, audio input + capture ("feed it from a track")
juce_add_plugin(MutaformFX
    COMPANY_NAME "Mutaform Audio"
    COMPANY_WEBSITE "https://github.com/mogenlord23/vst"
    BUNDLE_ID com.mutaformaudio.mutaformfx
    PLUGIN_MANUFACTURER_CODE Mtfm
    PLUGIN_CODE Mtf2
    PRODUCT_NAME "Mutaform FX"
    DESCRIPTION "Captures audio from a track and turns it into melodies"
    FORMATS VST3
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    VST3_CATEGORIES Fx Generator
    COPY_PLUGIN_AFTER_BUILD FALSE)
mutaform_configure(MutaformFX 1)

# ---------------------------------------------------------------------------
# Tests
if(MUTAFORM_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.16.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)

    enable_testing()
    add_executable(MutaformTests
        tests/TestMain.cpp
        tests/TestHelpers.h
        tests/SamplerTests.cpp
        tests/EvolverTests.cpp
        tests/FxTests.cpp
        tests/PresetTests.cpp
        tests/GameplayTests.cpp
        tests/MelodyTests.cpp
        tests/StateTests.cpp)
    target_link_libraries(MutaformTests PRIVATE Mutaform Catch2::Catch2)
    # Reuse the plugin's compiled shared code; inherit its JUCE include paths/definitions.
    target_include_directories(MutaformTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        $<TARGET_PROPERTY:Mutaform,INCLUDE_DIRECTORIES>)
    target_compile_definitions(MutaformTests PRIVATE
        $<TARGET_PROPERTY:Mutaform,COMPILE_DEFINITIONS>)
    target_compile_definitions(MutaformTests PRIVATE MUTAFORM_TESTS=1)

    include(${catch2_SOURCE_DIR}/extras/Catch.cmake)
    catch_discover_tests(MutaformTests DISCOVERY_MODE PRE_TEST)
endif()
