#include "FxProcessor.h"
#include "FxEditor.h"

using namespace geminis;
using namespace geminis::fx;

juce::AudioProcessorValueTreeState::ParameterLayout GeminisFxProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "inputSel", 1 }, "Entrada",
        juce::StringArray { "Mono izquierda", "Mono derecha", "Mono suma", juce::String::fromUTF8 ("Estéreo") }, 0));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "inGain", 1 }, "Ganancia de entrada",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "outGain", 1 }, "Volumen de salida",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, juce::AudioParameterFloatAttributes().withLabel ("dB")));

    // El tipo de cada ranura se lee del propio rack cuando el DAW pide el texto de un valor.
    Rack::addParameters (layout, [this] (int slot) { return rack.currentTypes[(size_t) slot].load(); });
    return layout;
}

GeminisFxProcessor::GeminisFxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Entrada", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Salida", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GeminisFx", createLayout()),
      rack (apvts)
{
    inputSel = apvts.getRawParameterValue ("inputSel");
    inGain = apvts.getRawParameterValue ("inGain");
    outGain = apvts.getRawParameterValue ("outGain");
    rack.onLatencyChanged = [this] { setLatencySamples (rack.getLatency()); };
}

GeminisFxProcessor::~GeminisFxProcessor()
{
    rack.onLatencyChanged = nullptr;
}

bool GeminisFxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    auto ok = [] (const juce::AudioChannelSet& s) { return s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo(); };
    return ok (in) && ok (out);
}

void GeminisFxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int block = juce::jmax (32, samplesPerBlock);
    work.setSize (2, block);
    rack.prepare (sampleRate, block);
    tuner.setSampleRate (sampleRate);
    setLatencySamples (rack.getLatency());
}

void GeminisFxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    if (getMode() == Mode::none)
    {
        const float peak = buffer.getMagnitude (0, n);
        inputPeak.store (juce::jmax (inputPeak.load(), peak));
        outputPeak.store (juce::jmax (outputPeak.load(), peak));
        return;
    }

    if (work.getNumSamples() < n)
        work.setSize (2, n, false, false, true);

    float* L = work.getWritePointer (0);
    float* R = work.getWritePointer (1);
    const int sel = juce::roundToInt (inputSel->load());
    const float gIn = juce::Decibels::decibelsToGain (inGain->load());
    const float* inL = numIn > 0 ? buffer.getReadPointer (0) : nullptr;
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;

    Context ctx;
    ctx.sampleRate = getSampleRate();
    ctx.stereo = sel == 3 && numIn > 1;

    if (inL == nullptr)
    {
        juce::FloatVectorOperations::clear (L, n);
        juce::FloatVectorOperations::clear (R, n);
    }
    else if (ctx.stereo)
    {
        juce::FloatVectorOperations::copyWithMultiply (L, inL, gIn, n);
        juce::FloatVectorOperations::copyWithMultiply (R, inR, gIn, n);
    }
    else
    {
        if (sel == 1)      juce::FloatVectorOperations::copyWithMultiply (L, inR, gIn, n);
        else if (sel == 2) { juce::FloatVectorOperations::copyWithMultiply (L, inL, gIn * 0.5f, n);
                             juce::FloatVectorOperations::addWithMultiply (L, inR, gIn * 0.5f, n); }
        else               juce::FloatVectorOperations::copyWithMultiply (L, inL, gIn, n);
        juce::FloatVectorOperations::copy (R, L, n);
    }

    tuner.push (L, n);
    inputPeak.store (juce::jmax (inputPeak.load(), juce::FloatVectorOperations::findMaximum (L, n),
                                 -juce::FloatVectorOperations::findMinimum (L, n)));

    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
            if (auto bpm = pos->getBpm())
                ctx.bpm = *bpm;

    rack.process (L, R, n, ctx);

    // Con el afinador abierto la salida se silencia, como en un pedal afinador
    if (tuner.enabled.load() && tuner.muteOutput.load())
    {
        juce::FloatVectorOperations::clear (L, n);
        juce::FloatVectorOperations::clear (R, n);
    }

    const float gOut = juce::Decibels::decibelsToGain (outGain->load());
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        float l = L[i] * gOut, r = R[i] * gOut;
        if (! std::isfinite (l)) l = 0.0f;
        if (! std::isfinite (r)) r = 0.0f;
        // Protección suave por encima de 0 dBFS
        if (std::abs (l) > 0.95f) l = std::copysign (0.95f + 0.05f * std::tanh ((std::abs (l) - 0.95f) * 20.0f), l);
        if (std::abs (r) > 0.95f) r = std::copysign (0.95f + 0.05f * std::tanh ((std::abs (r) - 0.95f) * 20.0f), r);
        L[i] = l; R[i] = r;
        peak = juce::jmax (peak, std::abs (l), std::abs (r));
    }
    outputPeak.store (juce::jmax (outputPeak.load(), peak));

    if (numOut >= 2)
    {
        buffer.copyFrom (0, 0, L, n);
        buffer.copyFrom (1, 0, R, n);
    }
    else if (numOut == 1)
    {
        auto* out = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i) out[i] = 0.5f * (L[i] + R[i]);
    }
}

//==============================================================================
void GeminisFxProcessor::setMode (Mode m, bool loadDefaultPreset)
{
    const auto previous = getMode();
    mode.store ((int) m);
    if (loadDefaultPreset && m != Mode::none && (m != previous || rack.countUsed() == 0))
    {
        const auto& presets = fxPresets();
        for (int i = 0; i < (int) presets.size(); ++i)
            if (presets[(size_t) i].mode == m) { applyPreset (i); break; }
    }
}

void GeminisFxProcessor::applyPreset (int index)
{
    const auto& presets = fxPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    const auto& p = presets[(size_t) index];

    for (int s = 0; s < Rack::numSlots; ++s)
    {
        if (s < (int) p.slots.size())
            rack.setSlot (s, p.slots[(size_t) s].type, p.slots[(size_t) s].on, p.slots[(size_t) s].values);
        else
            rack.setSlot (s, empty, true);
    }
    currentPreset = index;
    presetName = juce::String::fromUTF8 (p.name);
    if (p.mode != Mode::none) mode.store ((int) p.mode);
    if (onPresetChanged) onPresetChanged();
}

int GeminisFxProcessor::getNumPrograms() { return (int) fxPresets().size(); }

const juce::String GeminisFxProcessor::getProgramName (int index)
{
    const auto& presets = fxPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return {};
    const auto& p = presets[(size_t) index];
    return juce::String (p.mode == Mode::voice ? "Voz - " : "Guitarra - ") + juce::String::fromUTF8 (p.name);
}

void GeminisFxProcessor::setCurrentProgram (int index)
{
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
        && juce::MessageManager::getInstance()->isThisTheMessageThread())
        applyPreset (index);
    else
        juce::MessageManager::callAsync ([safe = juce::WeakReference<GeminisFxProcessor> (this), index]
                                         { if (auto* p = safe.get()) p->applyPreset (index); });
}

juce::File GeminisFxProcessor::userPresetFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Geminis").getChildFile ("Presets FX");
    dir.createDirectory();
    return dir;
}

bool GeminisFxProcessor::saveUserPreset (const juce::File& file)
{
    auto state = apvts.copyState();
    state.setProperty ("mode", mode.load(), nullptr);
    state.setProperty ("presetName", file.getFileNameWithoutExtension(), nullptr);
    if (auto xml = state.createXml())
        return xml->writeTo (file);
    return false;
}

bool GeminisFxProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType())) return false;

    // Se copian los valores parámetro a parámetro para que el DAW se entere
    for (auto* param : getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            auto child = state.getChildWithProperty ("id", p->getParameterID());
            if (child.isValid())
                p->setValueNotifyingHost (p->convertTo0to1 ((float) child.getProperty ("value")));
        }
    rack.syncModules();
    mode.store (juce::jlimit (1, 2, (int) state.getProperty ("mode", 1)));
    presetName = file.getFileNameWithoutExtension();
    if (onPresetChanged) onPresetChanged();
    return true;
}

void GeminisFxProcessor::loadUserIr (const juce::File& file)
{
    if (file.existsAsFile())
        rack.setUserIr (file);
}

void GeminisFxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("mode", mode.load(), nullptr);
    state.setProperty ("preset", currentPreset, nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("userIr", rack.getUserIr().getFullPathName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void GeminisFxProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || ! state.hasType (apvts.state.getType())) return;

    mode.store (juce::jlimit (0, 2, (int) state.getProperty ("mode", 0)));
    currentPreset = (int) state.getProperty ("preset", 0);
    presetName = state.getProperty ("presetName", "Sin preset").toString();
    const juce::String ir = state.getProperty ("userIr", "").toString();
    apvts.replaceState (state);
    if (ir.isNotEmpty()) loadUserIr (juce::File (ir));

    juce::MessageManager::callAsync ([safe = juce::WeakReference<GeminisFxProcessor> (this)]
    {
        if (auto* p = safe.get())
        {
            p->rack.syncModules();
            if (p->onPresetChanged) p->onPresetChanged();
        }
    });
}

juce::AudioProcessorEditor* GeminisFxProcessor::createEditor() { return new GeminisFxEditor (*this); }

#if ! GEMINIS_PROGRAMA  // el programa Géminis crea los dos procesadores por su cuenta
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GeminisFxProcessor(); }
#endif
