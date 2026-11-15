#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace geminis;
using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
namespace geminis
{
const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Crunch Britannia", {
            { "ampModel", 1 }, { "ampGain", 0.45f }, { "ampBass", 0.5f }, { "ampMid", 0.65f }, { "ampTreble", 0.55f },
            { "ampPresence", 0.55f }, { "ampMaster", 0.5f }, { "cabType", 2 }, { "cabMic", 0.45f },
            { "revOn", 1 }, { "revSize", 0.3f }, { "revMix", 0.1f } } },
        { "Limpio cristalino", {
            { "ampModel", 0 }, { "ampGain", 0.35f }, { "ampBass", 0.5f }, { "ampMid", 0.45f }, { "ampTreble", 0.6f },
            { "ampPresence", 0.55f }, { "ampMaster", 0.5f }, { "cabType", 0 }, { "cabMic", 0.4f },
            { "compOn", 1 }, { "compThresh", -22.0f }, { "compRatio", 3.0f }, { "compMakeup", 4.0f },
            { "revOn", 1 }, { "revSize", 0.4f }, { "revMix", 0.18f } } },
        { "Solista con delay", {
            { "ampModel", 1 }, { "ampGain", 0.7f }, { "ampBass", 0.45f }, { "ampMid", 0.7f }, { "ampTreble", 0.5f },
            { "ampPresence", 0.5f }, { "ampMaster", 0.5f }, { "cabType", 2 }, { "cabMic", 0.5f },
            { "drvOn", 1 }, { "drvGain", 0.3f }, { "drvTone", 0.55f }, { "drvLevel", 0.6f },
            { "dlyOn", 1 }, { "dlyTime", 420.0f }, { "dlyFeedback", 0.35f }, { "dlyMix", 0.25f },
            { "revOn", 1 }, { "revSize", 0.5f }, { "revMix", 0.15f } } },
        { "Titán metal", {
            { "ampModel", 2 }, { "ampGain", 0.65f }, { "ampBass", 0.55f }, { "ampMid", 0.4f }, { "ampTreble", 0.6f },
            { "ampPresence", 0.6f }, { "ampMaster", 0.5f }, { "cabType", 3 }, { "cabMic", 0.4f },
            { "gateThresh", -52.0f }, { "revOn", 0 },
            { "drvOn", 1 }, { "drvGain", 0.1f }, { "drvTone", 0.6f }, { "drvLevel", 0.7f } } },
        { "Ambiente limpio", {
            { "ampModel", 0 }, { "ampGain", 0.3f }, { "ampBass", 0.45f }, { "ampMid", 0.5f }, { "ampTreble", 0.6f },
            { "ampPresence", 0.5f }, { "ampMaster", 0.5f }, { "cabType", 1 }, { "cabMic", 0.5f },
            { "compOn", 1 },
            { "dlyOn", 1 }, { "dlyTime", 520.0f }, { "dlyFeedback", 0.5f }, { "dlyMix", 0.35f },
            { "revOn", 1 }, { "revSize", 0.85f }, { "revDamp", 0.35f }, { "revMix", 0.45f } } },
        { "Surf con trémolo", {
            { "ampModel", 0 }, { "ampGain", 0.5f }, { "ampBass", 0.4f }, { "ampMid", 0.5f }, { "ampTreble", 0.7f },
            { "ampPresence", 0.6f }, { "ampMaster", 0.5f }, { "cabType", 0 }, { "cabMic", 0.35f },
            { "tremOn", 1 }, { "tremRate", 5.5f }, { "tremDepth", 0.6f },
            { "revOn", 1 }, { "revSize", 0.6f }, { "revDamp", 0.3f }, { "revMix", 0.35f } } },
    };
    return presets;
}
}

//==============================================================================
static juce::String zeroToTen (float v, int) { return juce::String (v * 10.0f, 1); }
static juce::String percent (float v, int)   { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

APVTS::ParameterLayout GeminisProcessor::createLayout()
{
    using FloatP = juce::AudioParameterFloat;
    using BoolP = juce::AudioParameterBool;
    using ChoiceP = juce::AudioParameterChoice;
    using Range = juce::NormalisableRange<float>;
    using Attr = juce::AudioParameterFloatAttributes;

    APVTS::ParameterLayout layout;
    auto id = [] (const char* s) { return juce::ParameterID { s, 1 }; };

    auto knob = [&] (const char* pid, const char* name, float def)
    {
        layout.add (std::make_unique<FloatP> (id (pid), name, Range (0.0f, 1.0f, 0.001f), def,
                                              Attr().withStringFromValueFunction (zeroToTen)));
    };
    auto mix = [&] (const char* pid, const char* name, float def)
    {
        layout.add (std::make_unique<FloatP> (id (pid), name, Range (0.0f, 1.0f, 0.001f), def,
                                              Attr().withStringFromValueFunction (percent)));
    };
    auto ranged = [&] (const char* pid, const char* name, Range range, float def, const char* unit)
    {
        layout.add (std::make_unique<FloatP> (id (pid), name, range, def, Attr().withLabel (unit)));
    };
    auto toggle = [&] (const char* pid, const char* name, bool def)
    {
        layout.add (std::make_unique<BoolP> (id (pid), name, def));
    };

    layout.add (std::make_unique<ChoiceP> (id ("inputSel"), "Entrada",
                                           juce::StringArray { "Izquierda", "Derecha", "Ambas" }, 0));
    ranged ("inGain", "Ganancia de entrada", Range (-24.0f, 24.0f, 0.1f), 0.0f, "dB");
    ranged ("outGain", "Volumen de salida", Range (-24.0f, 24.0f, 0.1f), 0.0f, "dB");

    toggle ("gateOn", "Puerta", true);
    ranged ("gateThresh", "Puerta umbral", Range (-80.0f, -20.0f, 0.1f), -60.0f, "dB");

    toggle ("compOn", "Compresor", false);
    ranged ("compThresh", "Compresor umbral", Range (-40.0f, 0.0f, 0.1f), -20.0f, "dB");
    ranged ("compRatio", "Compresor ratio", Range (1.0f, 10.0f, 0.1f), 3.0f, ":1");
    ranged ("compMakeup", "Compresor nivel", Range (0.0f, 18.0f, 0.1f), 4.0f, "dB");

    toggle ("drvOn", "Impulso", false);
    knob ("drvGain", "Impulso ganancia", 0.4f);
    knob ("drvTone", "Impulso tono", 0.5f);
    knob ("drvLevel", "Impulso nivel", 0.5f);

    toggle ("ampOn", "Amplificador", true);
    layout.add (std::make_unique<ChoiceP> (id ("ampModel"), "Modelo de ampli",
                                           juce::StringArray { "Cristal (limpio)", "Britannia (crunch)", juce::String::fromUTF8 ("Titán (alta ganancia)") }, 1));
    knob ("ampGain", "Ampli ganancia", 0.45f);
    knob ("ampBass", "Ampli graves", 0.5f);
    knob ("ampMid", "Ampli medios", 0.65f);
    knob ("ampTreble", "Ampli agudos", 0.55f);
    knob ("ampPresence", "Ampli presencia", 0.55f);
    knob ("ampMaster", "Ampli master", 0.5f);

    toggle ("cabOn", "Pantalla", true);
    layout.add (std::make_unique<ChoiceP> (id ("cabType"), "Tipo de pantalla",
                                           juce::StringArray { "1x12 abierta", "2x12 vintage", "4x12 Britannia", juce::String::fromUTF8 ("4x12 Titán"), "IR propia" }, 2));
    knob ("cabMic", "Posicion del micro", 0.45f);

    toggle ("tremOn", "Tremolo", false);
    ranged ("tremRate", "Tremolo velocidad", Range (0.5f, 12.0f, 0.01f), 4.0f, "Hz");
    mix ("tremDepth", "Tremolo profundidad", 0.5f);

    toggle ("dlyOn", "Delay", false);
    ranged ("dlyTime", "Delay tiempo", Range (20.0f, 1200.0f, 1.0f, 0.5f), 380.0f, "ms");
    layout.add (std::make_unique<FloatP> (id ("dlyFeedback"), "Delay repeticiones", Range (0.0f, 0.9f, 0.001f), 0.35f,
                                          Attr().withStringFromValueFunction (percent)));
    mix ("dlyMix", "Delay mezcla", 0.25f);

    toggle ("revOn", "Reverb", true);
    knob ("revSize", "Reverb tamano", 0.3f);
    knob ("revDamp", "Reverb tono", 0.5f);
    mix ("revMix", "Reverb mezcla", 0.1f);

    return layout;
}

//==============================================================================
GeminisProcessor::GeminisProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Entrada", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Salida", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GeminisState", createLayout())
{
}

bool GeminisProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk;
}

void GeminisProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    guitar.prepare (sampleRate, juce::jmax (16, samplesPerBlock));
    work.setSize (3, juce::jmax (16, samplesPerBlock));
    setLatencySamples (getMode() == Mode::guitar ? guitar.getLatencySamples() : 0);
}

GuitarSettings GeminisProcessor::readSettings() const
{
    GuitarSettings s;
    s.gateOn = p ("gateOn") > 0.5f;    s.gateThreshDb = p ("gateThresh");
    s.compOn = p ("compOn") > 0.5f;    s.compThreshDb = p ("compThresh");
    s.compRatio = p ("compRatio");     s.compMakeupDb = p ("compMakeup");
    s.driveOn = p ("drvOn") > 0.5f;    s.driveGain = p ("drvGain");
    s.driveTone = p ("drvTone");       s.driveLevel = p ("drvLevel");
    s.ampOn = p ("ampOn") > 0.5f;      s.ampModel = (int) p ("ampModel");
    s.ampGain = p ("ampGain");         s.ampBass = p ("ampBass");
    s.ampMid = p ("ampMid");           s.ampTreble = p ("ampTreble");
    s.ampPresence = p ("ampPresence"); s.ampMaster = p ("ampMaster");
    s.cabOn = p ("cabOn") > 0.5f;      s.cabType = (int) p ("cabType");
    s.cabMic = p ("cabMic");
    s.tremOn = p ("tremOn") > 0.5f;    s.tremRateHz = p ("tremRate");
    s.tremDepth = p ("tremDepth");
    s.delayOn = p ("dlyOn") > 0.5f;    s.delayMs = p ("dlyTime");
    s.delayFeedback = p ("dlyFeedback"); s.delayMix = p ("dlyMix");
    s.reverbOn = p ("revOn") > 0.5f;   s.reverbSize = p ("revSize");
    s.reverbDamp = p ("revDamp");      s.reverbMix = p ("revMix");
    return s;
}

void GeminisProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    if (n == 0)
        return;

    // Hasta que se elige un modo con cadena propia, el audio pasa sin tocar.
    if (getMode() != Mode::guitar)
    {
        for (int ch = numIn; ch < numOut; ++ch)
            buffer.clear (ch, 0, n);
        const float peak = buffer.getMagnitude (0, n);
        inputPeak.store (juce::jmax (inputPeak.load(), peak));
        outputPeak.store (juce::jmax (outputPeak.load(), peak));
        return;
    }

    if (work.getNumSamples() < n)
        work.setSize (3, n, false, false, true);

    // Entrada en mono según el selector
    float* mono = work.getWritePointer (0);
    const int sel = (int) p ("inputSel");
    const float inGain = juce::Decibels::decibelsToGain (p ("inGain"));

    if (numIn >= 2 && sel == 1)
        juce::FloatVectorOperations::copyWithMultiply (mono, buffer.getReadPointer (1), inGain, n);
    else if (numIn >= 2 && sel == 2)
    {
        juce::FloatVectorOperations::copyWithMultiply (mono, buffer.getReadPointer (0), inGain, n);
        juce::FloatVectorOperations::addWithMultiply (mono, buffer.getReadPointer (1), inGain, n);
    }
    else if (numIn >= 1)
        juce::FloatVectorOperations::copyWithMultiply (mono, buffer.getReadPointer (0), inGain, n);
    else
        juce::FloatVectorOperations::clear (mono, n);

    inputPeak.store (juce::jmax (inputPeak.load(), work.getMagnitude (0, 0, n)));

    float* left = work.getWritePointer (1);
    float* right = work.getWritePointer (2);
    guitar.process (mono, left, right, n, readSettings());

    const float outGain = juce::Decibels::decibelsToGain (p ("outGain"));

    if (numOut >= 2)
    {
        juce::FloatVectorOperations::copyWithMultiply (buffer.getWritePointer (0), left, outGain, n);
        juce::FloatVectorOperations::copyWithMultiply (buffer.getWritePointer (1), right, outGain, n);
        for (int ch = 2; ch < numOut; ++ch)
            buffer.clear (ch, 0, n);
    }
    else if (numOut == 1)
    {
        float* out = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i)
            out[i] = 0.5f * (left[i] + right[i]) * outGain;
    }

    // Protección: nunca dejar salir valores no válidos hacia el DAW.
    float peak = 0.0f;
    for (int ch = 0; ch < juce::jmin (2, numOut); ++ch)
    {
        float* out = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            if (! std::isfinite (out[i]))
                out[i] = 0.0f;
            peak = juce::jmax (peak, std::abs (out[i]));
        }
    }
    outputPeak.store (juce::jmax (outputPeak.load(), peak));
}

//==============================================================================
void GeminisProcessor::setMode (Mode m)
{
    mode.store ((int) m);
    setLatencySamples (m == Mode::guitar ? guitar.getLatencySamples() : 0);
}

void GeminisProcessor::loadUserIr (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    userIrPath = file.getFullPathName();
    guitar.loadUserIr (file);
}

int GeminisProcessor::getNumPrograms() { return (int) getFactoryPresets().size(); }

const juce::String GeminisProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? juce::String::fromUTF8 (presets[(size_t) index].name)
                                                                 : juce::String();
}

void GeminisProcessor::setCurrentProgram (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentPreset = index;

    // Parte de los valores por defecto y aplica encima lo que define el preset.
    static const juce::StringArray keep { "inputSel", "inGain", "outGain" };
    for (auto* param : getParameters())
        if (auto* withId = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (! keep.contains (withId->getParameterID()))
                withId->setValueNotifyingHost (withId->getDefaultValue());

    for (const auto& [pid, value] : presets[(size_t) index].values)
        if (auto* param = apvts.getParameter (pid))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void GeminisProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("mode", mode.load(), nullptr);
    state.setProperty ("preset", currentPreset, nullptr);
    state.setProperty ("userIr", userIrPath, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void GeminisProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.isValid() || ! state.hasType (apvts.state.getType()))
            return;

        const int savedMode = juce::jlimit (0, 3, (int) state.getProperty ("mode", 0));
        currentPreset = (int) state.getProperty ("preset", 0);
        const juce::String ir = state.getProperty ("userIr", "").toString();

        apvts.replaceState (state);
        setMode ((Mode) savedMode);

        if (ir.isNotEmpty())
            loadUserIr (juce::File (ir));
    }
}

juce::AudioProcessorEditor* GeminisProcessor::createEditor() { return new GeminisEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GeminisProcessor(); }
