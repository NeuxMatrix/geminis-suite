cmake_minimum_required(VERSION 3.22)
project(GeminisSuite VERSION 0.3.0 LANGUAGES C CXX)

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

#==============================================================================
# Recursos comunes: logo y tipografías (Inter y Cinzel, licencia SIL OFL)
juce_add_binary_data(GeminisAssets
  NAMESPACE BinaryData
  SOURCES
    Assets/logo/logo-emblema.png
    Assets/fonts/Inter-Regular.otf
    Assets/fonts/Inter-SemiBold.otf
    Assets/fonts/Inter-Bold.otf
    Assets/fonts/Cinzel-500.ttf
    Assets/fonts/Cinzel-700.ttf)
set_target_properties(GeminisAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(GEMINIS_COMMON_SOURCES
  Source/common/Theme.cpp
  Source/common/Splash.cpp)

set(GEMINIS_DEFINES
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_REPORT_APP_USAGE=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

function(geminis_configure target)
  target_include_directories(${target} PRIVATE Source)
  if(MSVC)
    target_compile_options(${target} PRIVATE /utf-8 /bigobj)
  endif()
  target_compile_definitions(${target} PUBLIC ${GEMINIS_DEFINES})
  # Aplicación independiente propia (pide 44100 Hz por defecto).
  target_sources(${target} PRIVATE ${PROJECT_SOURCE_DIR}/Source/common/StandaloneApp.cpp)
  target_compile_definitions(${target} PUBLIC JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
  target_link_libraries(${target}
    PRIVATE
      GeminisAssets
      juce::juce_audio_utils
      juce::juce_dsp
    PUBLIC
      juce::juce_recommended_config_flags
      juce::juce_recommended_lto_flags)
endfunction()

#==============================================================================
# Geminis FX: guitarra y voz (efecto de audio)
juce_add_plugin(GeminisFX
  PRODUCT_NAME "Geminis FX"
  COMPANY_NAME "Geminis Audio"
  BUNDLE_ID "com.geminisaudio.geminisfx"
  PLUGIN_MANUFACTURER_CODE Gmns
  PLUGIN_CODE Gfx1
  FORMATS ${GEMINIS_FORMATS}
  ICON_BIG Assets/logo/icono-512.png
  ICON_SMALL Assets/logo/icono-256.png
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

set(GEMINIS_FX_DSP_SOURCES
  Source/fx/Modules.cpp
  Source/fx/FxPresets.cpp)

target_sources(GeminisFX PRIVATE
  ${GEMINIS_COMMON_SOURCES}
  ${GEMINIS_FX_DSP_SOURCES}
  Source/fx/Rack.cpp
  Source/fx/FxProcessor.cpp
  Source/fx/FxEditor.cpp)
geminis_configure(GeminisFX)

option(GEMINIS_BUILD_TESTS "Compila las pruebas de audio sin interfaz" OFF)

#==============================================================================
# Geminis Instrument: sintetizador, batería e instrumentos (MIDI)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/Source/inst/InstProcessor.cpp)
  include(Source/inst/Instrument.cmake)
endif()

#==============================================================================
# Prueba sin interfaz: pasa audio de prueba por todos los módulos y presets.
if(GEMINIS_BUILD_TESTS)
  juce_add_console_app(GeminisDspTest PRODUCT_NAME "GeminisDspTest")
  target_sources(GeminisDspTest PRIVATE tests/DspTest.cpp ${GEMINIS_FX_DSP_SOURCES})
  target_include_directories(GeminisDspTest PRIVATE Source)
  if(MSVC)
    target_compile_options(GeminisDspTest PRIVATE /utf-8 /bigobj)
  endif()
  target_compile_definitions(GeminisDspTest PRIVATE ${GEMINIS_DEFINES})
  target_link_libraries(GeminisDspTest PRIVATE
    juce::juce_audio_formats juce::juce_audio_processors juce::juce_dsp juce::juce_recommended_config_flags)
endif()
