# Geminis Instrument: sintetizador modular, batería por estilos e instrumentos con muestras.
# Se incluye desde el CMakeLists.txt principal.

file(GLOB GEMINIS_SAMPLE_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Assets/samples/*.ogg)
juce_add_binary_data(GeminisSamples
  NAMESPACE GeminisSamples
  HEADER_NAME GeminisSamples.h
  SOURCES ${GEMINIS_SAMPLE_FILES})
set_target_properties(GeminisSamples PROPERTIES POSITION_INDEPENDENT_CODE ON)

juce_add_plugin(GeminisInstrument
  PRODUCT_NAME "Geminis Instrument"
  COMPANY_NAME "Geminis Audio"
  BUNDLE_ID "com.geminisaudio.geminisinstrument"
  PLUGIN_MANUFACTURER_CODE Gmns
  PLUGIN_CODE Gin1
  FORMATS ${GEMINIS_FORMATS}
  ICON_BIG ${PROJECT_SOURCE_DIR}/Assets/logo/icono-512.png
  ICON_SMALL ${PROJECT_SOURCE_DIR}/Assets/logo/icono-256.png
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  VST3_CATEGORIES Instrument Synth
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  COPY_PLUGIN_AFTER_BUILD FALSE)

set(GEMINIS_INST_DSP_SOURCES
  Source/fx/Modules.cpp
  Source/inst/InstParams.cpp
  Source/inst/Wavetable.cpp
  Source/inst/SynthEngine.cpp
  Source/inst/SynthPresets.cpp
  Source/inst/SampleLibrary.cpp
  Source/inst/Drums.cpp
  Source/inst/DrumKits.cpp
  Source/inst/Sampler.cpp)

target_sources(GeminisInstrument PRIVATE
  ${GEMINIS_COMMON_SOURCES}
  ${GEMINIS_INST_DSP_SOURCES}
  Source/inst/InstProcessor.cpp
  Source/inst/InstEditor.cpp)
geminis_configure(GeminisInstrument)
target_link_libraries(GeminisInstrument PRIVATE GeminisSamples juce::juce_audio_formats)

if(GEMINIS_BUILD_TESTS)
  juce_add_console_app(GeminisInstTest PRODUCT_NAME "GeminisInstTest")
  target_sources(GeminisInstTest PRIVATE
    tests/InstTest.cpp
    Source/app/SystemVolume.cpp
    ${GEMINIS_COMMON_SOURCES}
    ${GEMINIS_INST_DSP_SOURCES}
    Source/inst/InstProcessor.cpp
    Source/inst/InstEditor.cpp)
  target_include_directories(GeminisInstTest PRIVATE Source)
  if(MSVC)
    target_compile_options(GeminisInstTest PRIVATE /utf-8 /bigobj)
  endif()
  target_compile_definitions(GeminisInstTest PRIVATE ${GEMINIS_DEFINES})
  target_link_libraries(GeminisInstTest PRIVATE
    GeminisAssets GeminisSamples
    juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp juce::juce_recommended_config_flags)
endif()
