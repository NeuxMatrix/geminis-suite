#include "InstProcessor.h"
#include "InstEditor.h"

using namespace geminis::inst;

namespace
{
    bool isFxParam (const juce::String& id) { return id.startsWith ("fx_"); }
    bool isPadParam (const juce::String& id) { return id.length() > 4 && id[0] == 'p' && juce::CharacterFunctions::isDigit (id[1]) && id[3] == '_'; }

    // Programa del DAW = sinte, después kits, después instrumentos
    int numSynthPrograms() { return (int) synthPresets().size(); }
    int numKitPrograms() { return (int) drumKits().size(); }
}

GeminisInstrumentProcessor::GeminisInstrumentProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Salida", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "GeminisInstrument", createLayout()),
      synth (apvts), drums (apvts), sampler (apvts), fx (apvts)
{
    masterVol = apvts.getRawParameterValue ("vol");
    drums.loadSamples();
    synth.updateTables();
    startTimerHz (15);
}

GeminisInstrumentProcessor::~GeminisInstrumentProcessor()
{
    stopTimer();
}

bool GeminisInstrumentProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void GeminisInstrumentProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int block = juce::jmax (32, samplesPerBlock);
    work.setSize (2, block);
    synth.prepare (sampleRate, block);
    drums.prepare (sampleRate, block);
    sampler.prepare (sampleRate, block);
    fx.prepare (sampleRate, block);
    masterGain.reset (sampleRate, 0.03);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterVol->load(), -60.0f));
    keyboardState.reset();
}

void GeminisInstrumentProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numOut = getTotalNumOutputChannels();
    buffer.clear();
    if (n == 0) return;

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    if (work.getNumSamples() < n)
        work.setSize (2, n, false, false, true);
    float* L = work.getWritePointer (0);
    float* R = work.getWritePointer (1);
    juce::FloatVectorOperations::clear (L, n);
    juce::FloatVectorOperations::clear (R, n);

    const int m = mode.load();
    if (m != lastAudioMode)
    {
        // Al cambiar de modo se cortan las voces del anterior y las colas de los efectos
        synth.reset();
        drums.reset();
        sampler.reset();
        fx.reset();
        lastAudioMode = m;
    }

    // Sin tempo del DAW (programa independiente) se usa el tempo propio
    double bpm = apvts.getRawParameterValue ("seq_bpm")->load();
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;

    switch ((Mode) m)
    {
        case Mode::synth:       synth.render (L, R, n, midi, bpm); break;
        case Mode::drums:       drums.render (L, R, n, midi, getPlayHead()); break;
        case Mode::instruments: sampler.render (L, R, n, midi); break;
        case Mode::none:        break;
    }

    if ((Mode) m != Mode::none)
        fx.process (L, R, n, bpm);

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (masterVol->load(), -60.0f));
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = masterGain.getNextValue();
        float l = L[i] * g, r = R[i] * g;
        if (! std::isfinite (l)) l = 0.0f;
        if (! std::isfinite (r)) r = 0.0f;
        // Limitador suave por encima de -0.5 dBFS para no saturar nunca el DAW
        if (std::abs (l) > 0.94f) l = std::copysign (0.94f + 0.06f * std::tanh ((std::abs (l) - 0.94f) * 16.0f), l);
        if (std::abs (r) > 0.94f) r = std::copysign (0.94f + 0.06f * std::tanh ((std::abs (r) - 0.94f) * 16.0f), r);
        L[i] = l; R[i] = r;
        peak = juce::jmax (peak, std::abs (l), std::abs (r));
    }
    outputPeak.store (juce::jmax (outputPeak.load(), peak));

    int w = scopeWrite.load();
    for (int i = 0; i < n; ++i)
    {
        scope[(size_t) w] = 0.5f * (L[i] + R[i]);
        w = (w + 1) % scopeSize;
    }
    scopeWrite.store (w);

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

void GeminisInstrumentProcessor::timerCallback()
{
    synth.updateTables();
    sampler.updateInstrument();
}

//==============================================================================
void GeminisInstrumentProcessor::setParam (const juce::String& id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

float GeminisInstrumentProcessor::getParam (const juce::String& id) const
{
    if (auto* v = apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

void GeminisInstrumentProcessor::resetParams (const std::function<bool (const juce::String&)>& which)
{
    for (const auto& d : allParamDefs())
        if (which (d.id))
            setParam (d.id, d.def);
}

void GeminisInstrumentProcessor::notifyChanged()
{
    if (onPresetChanged) onPresetChanged();
}

void GeminisInstrumentProcessor::storeFxSnapshot (Mode m)
{
    if (m == Mode::none) return;
    auto& snap = fxSnapshots[(size_t) m];
    snap.clear();
    for (const auto& d : allParamDefs())
        if (isFxParam (d.id))
            snap.emplace_back (d.id, getParam (d.id));
}

bool GeminisInstrumentProcessor::restoreFxSnapshot (Mode m)
{
    const auto& snap = fxSnapshots[(size_t) m];
    if (snap.empty()) return false;
    for (const auto& [id, v] : snap) setParam (id, v);
    return true;
}

void GeminisInstrumentProcessor::setMode (Mode m)
{
    const auto previous = getMode();
    if (m == previous) return;
    storeFxSnapshot (previous);
    mode.store ((int) m);

    const bool restored = restoreFxSnapshot (m);
    if (m == Mode::synth && synthPreset < 0)
    {
        // Primera vez: un sonido que enseña lo que hace la curva
        int start = 0;
        for (int i = 0; i < (int) synthPresets().size(); ++i)
            if (synthPresets()[(size_t) i].name == juce::String::fromUTF8 ("Pad curvo que respira")) start = i;
        applySynthPreset (start);
    }
    else if (m == Mode::drums && kit < 0)
        applyKit (0);
    else if (m == Mode::instruments && ! restored)
    {
        resetParams (isFxParam);
        setParam ("fx_rev_mix", 14.0f);
    }
    notifyChanged();
}

void GeminisInstrumentProcessor::applySynthPreset (int index)
{
    const auto& list = synthPresets();
    if (! juce::isPositiveAndBelow (index, (int) list.size())) return;
    const auto& p = list[(size_t) index];

    resetParams (isSynthParam);
    for (const auto& [id, v] : p.values) setParam (id, v);
    synth.setDrawnWave (0, DrawnWave::preset (p.drawn1 >= 0 ? p.drawn1 : 1));
    synth.setDrawnWave (1, DrawnWave::preset (p.drawn2 >= 0 ? p.drawn2 : 2));
    synth.updateTables();

    synthPreset = index;
    synthPresetName = p.name;
    currentProgram = index;
    if (getMode() != Mode::synth) mode.store ((int) Mode::synth);
    notifyChanged();
}

void GeminisInstrumentProcessor::applyKit (int index)
{
    const auto& kits = drumKits();
    if (! juce::isPositiveAndBelow (index, (int) kits.size())) return;
    const auto& k = kits[(size_t) index];

    resetParams ([] (const juce::String& id) { return isFxParam (id) || isPadParam (id); });
    for (int pad = 0; pad < numPads; ++pad)
    {
        const auto& pp = k.pads[(size_t) pad];
        setParam (padParamId (pad, "snd"), (float) juce::jmax (0, drumSoundIndex (pp.sound)));
        setParam (padParamId (pad, "tune"), pp.tune);
        setParam (padParamId (pad, "decay"), pp.decay);
        setParam (padParamId (pad, "tone"), pp.tone);
        setParam (padParamId (pad, "snap"), pp.snap);
        setParam (padParamId (pad, "drive"), pp.drive);
        setParam (padParamId (pad, "level"), pp.level);
        setParam (padParamId (pad, "pan"), pp.pan);
        setParam (padParamId (pad, "choke"), (float) pp.choke);
    }
    for (int i = 0; i < numPatterns; ++i)
        drums.setPattern (i, k.patterns[(size_t) i]);
    setParam ("seq_bpm", (float) k.bpm);
    setParam ("seq_swing", k.swing);
    setParam ("seq_pattern", 0.0f);
    for (const auto& [id, v] : k.extra) setParam (id, v);

    kit = index;
    kitName = k.name;
    currentProgram = numSynthPrograms() + index;
    if (getMode() != Mode::drums) mode.store ((int) Mode::drums);
    notifyChanged();
}

void GeminisInstrumentProcessor::applyInstrument (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) instrumentList().size())) return;
    setParam ("ins_type", (float) index);
    currentProgram = numSynthPrograms() + numKitPrograms() + index;
    if (getMode() != Mode::instruments) setMode (Mode::instruments);
    notifyChanged();
}

//==============================================================================
int GeminisInstrumentProcessor::getNumPrograms()
{
    return numSynthPrograms() + numKitPrograms() + (int) instrumentList().size();
}

const juce::String GeminisInstrumentProcessor::getProgramName (int index)
{
    if (index < 0) return {};
    if (index < numSynthPrograms()) return "Sinte - " + synthPresets()[(size_t) index].name;
    index -= numSynthPrograms();
    if (index < numKitPrograms()) return juce::String::fromUTF8 ("Batería - ") + drumKits()[(size_t) index].name;
    index -= numKitPrograms();
    if (index < (int) instrumentList().size()) return "Instrumento - " + instrumentList()[(size_t) index].name;
    return {};
}

void GeminisInstrumentProcessor::setCurrentProgram (int index)
{
    auto apply = [this, index]
    {
        int i = index;
        if (i < 0) return;
        if (i < numSynthPrograms()) { applySynthPreset (i); return; }
        i -= numSynthPrograms();
        if (i < numKitPrograms()) { applyKit (i); return; }
        i -= numKitPrograms();
        applyInstrument (i);
    };
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
        && juce::MessageManager::getInstance()->isThisTheMessageThread())
        apply();
    else
        juce::MessageManager::callAsync ([safe = juce::WeakReference<GeminisInstrumentProcessor> (this), index]
                                         { if (auto* p = safe.get()) p->setCurrentProgram (index); });
}

//==============================================================================
juce::File GeminisInstrumentProcessor::userSoundFolder()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Geminis").getChildFile ("Sonidos");
    dir.createDirectory();
    return dir;
}

bool GeminisInstrumentProcessor::saveUserSound (const juce::File& file)
{
    juce::XmlElement xml ("GeminisSound");
    xml.setAttribute ("version", 1);
    for (const auto& d : allParamDefs())
        if (isSynthParam (d.id))
        {
            auto* e = xml.createNewChildElement ("p");
            e->setAttribute ("id", d.id);
            e->setAttribute ("v", (double) getParam (d.id));
        }
    xml.setAttribute ("drawn1", synth.getDrawnWave (0).toString());
    xml.setAttribute ("drawn2", synth.getDrawnWave (1).toString());
    if (! xml.writeTo (file)) return false;
    synthPreset = -2;
    synthPresetName = file.getFileNameWithoutExtension();
    notifyChanged();
    return true;
}

bool GeminisInstrumentProcessor::loadUserSound (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("GeminisSound")) return false;
    resetParams (isSynthParam);
    for (auto* e : xml->getChildWithTagNameIterator ("p"))
    {
        const auto id = e->getStringAttribute ("id");
        if (isSynthParam (id)) setParam (id, (float) e->getDoubleAttribute ("v"));
    }
    synth.setDrawnWave (0, DrawnWave::fromString (xml->getStringAttribute ("drawn1")));
    synth.setDrawnWave (1, DrawnWave::fromString (xml->getStringAttribute ("drawn2")));
    synth.updateTables();
    synthPreset = -2;
    synthPresetName = file.getFileNameWithoutExtension();
    if (getMode() != Mode::synth) mode.store ((int) Mode::synth);
    notifyChanged();
    return true;
}

//==============================================================================
void GeminisInstrumentProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("mode", mode.load(), nullptr);
    state.setProperty ("synthPreset", synthPreset, nullptr);
    state.setProperty ("synthPresetName", synthPresetName, nullptr);
    state.setProperty ("kit", kit, nullptr);
    state.setProperty ("kitName", kitName, nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("drawn1", synth.getDrawnWave (0).toString(), nullptr);
    state.setProperty ("drawn2", synth.getDrawnWave (1).toString(), nullptr);
    for (int i = 0; i < numPatterns; ++i)
        state.setProperty ("pattern" + juce::String (i), drums.getPattern (i).toString(), nullptr);

    // Efectos guardados de los otros modos (el actual ya va en los parámetros)
    juce::ValueTree snaps ("FxSnapshots");
    for (int m = 1; m < 4; ++m)
    {
        juce::ValueTree s ("Mode" + juce::String (m));
        if (m != mode.load())
            for (const auto& [id, v] : fxSnapshots[(size_t) m]) s.setProperty (id, v, nullptr);
        snaps.appendChild (s, nullptr);
    }
    state.appendChild (snaps, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void GeminisInstrumentProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || ! state.hasType (apvts.state.getType())) return;

    mode.store (juce::jlimit (0, 3, (int) state.getProperty ("mode", 0)));
    synthPreset = (int) state.getProperty ("synthPreset", -1);
    synthPresetName = state.getProperty ("synthPresetName", "").toString();
    kit = (int) state.getProperty ("kit", -1);
    kitName = state.getProperty ("kitName", "").toString();
    currentProgram = (int) state.getProperty ("program", 0);
    if (state.hasProperty ("drawn1")) synth.setDrawnWave (0, DrawnWave::fromString (state.getProperty ("drawn1").toString()));
    if (state.hasProperty ("drawn2")) synth.setDrawnWave (1, DrawnWave::fromString (state.getProperty ("drawn2").toString()));
    for (int i = 0; i < numPatterns; ++i)
        if (state.hasProperty ("pattern" + juce::String (i)))
            drums.setPattern (i, Pattern::fromString (state.getProperty ("pattern" + juce::String (i)).toString()));

    auto snaps = state.getChildWithName ("FxSnapshots");
    for (int m = 1; m < 4; ++m)
    {
        fxSnapshots[(size_t) m].clear();
        auto s = snaps.getChildWithName ("Mode" + juce::String (m));
        for (int k = 0; k < s.getNumProperties(); ++k)
        {
            const auto id = s.getPropertyName (k).toString();
            fxSnapshots[(size_t) m].emplace_back (id, (float) s.getProperty (id));
        }
    }
    if (snaps.isValid()) state.removeChild (snaps, nullptr);
    apvts.replaceState (state);

    juce::MessageManager::callAsync ([safe = juce::WeakReference<GeminisInstrumentProcessor> (this)]
    {
        if (auto* p = safe.get())
        {
            p->synth.updateTables();
            p->sampler.updateInstrument();
            p->notifyChanged();
        }
    });
}

juce::AudioProcessorEditor* GeminisInstrumentProcessor::createEditor() { return new GeminisInstrumentEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GeminisInstrumentProcessor(); }
