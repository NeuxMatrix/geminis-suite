cmake_minimum_required(VERSION 3.22)
project(GeminisSuite VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE: usa una copia local con -DGEMINIS_JUCE_DIR=ruta, o se descarga sola.
set(GEMINIS_JUCE_DIR "" CACHE PATH "Ruta a una copia local de JUCE (opcional)")
if(GEMINIS_JUCE_DIR)
  add_subdirectory(${GEMINIS_JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

set(GEMINIS_FORMATS VST3 Standalone)
if(APPLE)
  list(APPEND GEMINIS_FORMATS AU)
endif()

juce_add_plugin(GeminisFX
  PRODUCT_NAME "Geminis FX"
  COMPANY_NAME "Geminis Audio"
  BUNDLE_ID "com.geminisaudio.geminisfx"
  PLUGIN_MANUFACTURER_CODE Gmns
  PLUGIN_CODE Gfx1
  FORMATS ${GEMINIS_FORMATS}
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  VST3_CATEGORIES Fx Distortion
  AU_MAIN_TYPE kAudioUnitType_Effect
  MICROPHONE_PERMISSION_ENABLED TRUE
  MICROPHONE_PERMISSION_TEXT "Geminis necesita la entrada de audio para procesar tu guitarra o tu voz."
  COPY_PLUGIN_AFTER_BUILD FALSE)

set(GEMINIS_SOURCES
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_sources(GeminisFX PRIVATE ${GEMINIS_SOURCES})
target_include_directories(GeminisFX PRIVATE Source)

if(MSVC)
  target_compile_options(GeminisFX PRIVATE /utf-8)
endif()

target_compile_definitions(GeminisFX PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_REPORT_APP_USAGE=0)

target_link_libraries(GeminisFX
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags)

# Prueba sin interfaz: procesa audio de prueba por la cadena y comprueba el resultado.
option(GEMINIS_BUILD_TESTS "Compila la prueba de audio sin interfaz" OFF)
if(GEMINIS_BUILD_TESTS)
  juce_add_console_app(GeminisDspTest PRODUCT_NAME "GeminisDspTest")
  target_sources(GeminisDspTest PRIVATE tests/DspTest.cpp)
  target_include_directories(GeminisDspTest PRIVATE Source)
  target_compile_definitions(GeminisDspTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  target_link_libraries(GeminisDspTest PRIVATE
    juce::juce_audio_formats juce::juce_dsp juce::juce_recommended_config_flags)
endif()
