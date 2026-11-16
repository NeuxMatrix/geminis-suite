#include "Sampler.h"

namespace geminis::inst
{
namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }

    enum Model { bassFinger = 0, bassPick, guitarAcoustic, guitarNylon, guitarElectric, guitarMuted };

    InstrumentInfo sampled (const char* name, const char* group, const char* desc, float gain = 1.0f)
    {
        InstrumentInfo i; i.name = U (name); i.group = group; i.description = U (desc); i.gain = gain; return i;
    }
    InstrumentInfo modeled (const char* name, int model, const char* desc, float gain = 1.0f)
    {
        InstrumentInfo i; i.name = U (name); i.modeled = true; i.model = model; i.description = U (desc); i.gain = gain; return i;
    }
}

const std::vector<InstrumentInfo>& instrumentList()
{
    static const std::vector<InstrumentInfo> list = {
        sampled ("Piano vertical", "piano", "Piano acústico de pared, muestreado nota a nota."),
        sampled ("Piano eléctrico FM", "pianofm", "Piano eléctrico de síntesis FM de los ochenta.", 0.88f),
        sampled ("Órgano de tubos", "organo", "Órgano de iglesia con el registro abierto."),
        sampled ("Cuerdas", "cuerdas", "Sección de violines y chelos con vibrato.", 0.9f),
        sampled ("Cuerdas pizzicato", "pizzicato", "Cuerdas pulsadas con los dedos."),
        sampled ("Contrabajo", "contrabajo", "Contrabajo de jazz pulsado (pizzicato)."),
        modeled ("Bajo eléctrico (dedos)", bassFinger, "Bajo modelado con cuerda física, tocado con los dedos.", 2.7f),
        modeled ("Bajo eléctrico (púa)", bassPick, "Bajo modelado con púa: más ataque y brillo.", 2.05f),
        modeled ("Guitarra acústica", guitarAcoustic, "Cuerdas de acero con caja de resonancia.", 1.5f),
        modeled ("Guitarra de nylon", guitarNylon, "Guitarra española: cálida y redonda.", 1.8f),
        modeled ("Guitarra eléctrica limpia", guitarElectric, "Pastilla simple y amplificador limpio.", 1.7f),
        modeled ("Guitarra muteada", guitarMuted, "Palm mute para riffs y rítmicas.", 2.4f),
        sampled ("Arpa", "arpa", "Arpa de concierto."),
        sampled ("Metales", "metales", "Trombón y trompeta sostenidos.", 0.8f),
        sampled ("Flauta", "flauta", "Flauta travesera con vibrato.", 0.9f),
        sampled ("Saxo tenor", "saxo", "Saxo tenor sin vibrato.", 0.9f),
        sampled ("Marimba", "marimba", "Marimba de láminas de madera."),
        sampled ("Vibráfono", "vibrafono", "Vibráfono con baquetas blandas.", 0.76f),
        sampled ("Glockenspiel", "celesta", "Láminas metálicas brillantes."),
        sampled ("Kalimba", "kalimba", "Piano de pulgar africano (Tanzania)."),
    };
    return list;
}

const juce::StringArray& instrumentNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray n;
        for (const auto& i : instrumentList()) n.add (i.name);
        return n;
    }();
    return names;
}

//==============================================================================
struct SamplerEngine::Voice
{
    bool active = false, released = false;
    int note = 60;
    float vel = 1.0f, pan = 0.0f;
    juce::uint32 age = 0;
    // envolvente
    float env = 0.0f, attackInc = 1.0f, releaseCoef = 0.999f;
    // muestra
    const DecodedSample* sample = nullptr;
    double pos = 0.0, baseInc = 1.0;
    // filtro de tono
    dsp::Svf tone;
    // cuerda modelada
    bool isModel = false;
    int model = 0;
    std::vector<float> line;
    int writePos = 0;
    float period = 100.0f, loopGain = 0.99f, damp = 0.3f, lpState = 0.0f, dcX = 0.0f, dcY = 0.0f;
    float releaseLoopGain = 0.9f;
    std::array<dsp::Biquad, 3> body;
    juce::uint32 seed = 1u;

    float noise() noexcept { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 8388608.0f - 1.0f; }

    void startModel (int m, int n, float velocity, float character, double sr)
    {
        isModel = true;
        model = m;
        sample = nullptr;
        std::fill (line.begin(), line.end(), 0.0f);
        const float hz = dsp::midiToHz ((float) n);
        period = (float) (sr / hz);
        writePos = 0;
        lpState = dcX = dcY = 0.0f;

        // Sostenido (T60) y brillo de cada modelo
        float t60 = 4.0f, brightness = 0.5f, pickPos = 0.18f;
        switch (m)
        {
            case bassFinger:     t60 = 5.0f; brightness = 0.22f + character * 0.25f; pickPos = 0.2f + character * 0.1f; break;
            case bassPick:       t60 = 4.0f; brightness = 0.5f + character * 0.35f; pickPos = 0.1f + character * 0.08f; break;
            case guitarAcoustic: t60 = 3.5f; brightness = 0.55f + character * 0.35f; pickPos = 0.12f + character * 0.1f; break;
            case guitarNylon:    t60 = 2.8f; brightness = 0.3f + character * 0.3f; pickPos = 0.2f + character * 0.1f; break;
            case guitarElectric: t60 = 4.5f; brightness = 0.5f + character * 0.35f; pickPos = 0.15f + character * 0.1f; break;
            default:             t60 = 0.22f; brightness = 0.35f + character * 0.3f; pickPos = 0.15f; break;
        }
        brightness = juce::jlimit (0.05f, 0.98f, brightness * (0.75f + 0.35f * velocity));
        // Las notas agudas se apagan antes
        t60 *= juce::jlimit (0.25f, 1.4f, std::pow (110.0f / hz, 0.35f));
        loopGain = std::pow (10.0f, -3.0f * period / (t60 * (float) sr));
        releaseLoopGain = std::pow (10.0f, -3.0f * period / (0.08f * (float) sr));
        damp = 1.0f - brightness;      // promedio del filtro del lazo

        // Excitación: ruido filtrado con la posición de la púa
        const int len = juce::jmin ((int) period, (int) line.size() - 2);
        float lp = 0.0f;
        const float exLp = 0.15f + brightness * 0.8f;
        for (int i = 0; i < len; ++i)
        {
            lp += exLp * (noise() - lp);
            line[(size_t) i] = lp;
        }
        // Peine de la posición de pulsación (hacia atrás para usar los valores originales)
        const int pp = juce::jmax (1, (int) (pickPos * (float) len));
        for (int i = len - 1; i >= pp; --i) line[(size_t) i] -= line[(size_t) (i - pp)];
        float mean = 0.0f;
        for (int i = 0; i < len; ++i) mean += line[(size_t) i];
        mean /= (float) len;
        float peak = 1.0e-6f;
        for (int i = 0; i < len; ++i) { line[(size_t) i] -= mean; peak = std::max (peak, std::abs (line[(size_t) i])); }
        for (int i = 0; i < len; ++i) line[(size_t) i] *= 0.9f / peak;
        writePos = len;

        for (auto& b : body) b.reset();
        switch (m)
        {
            case bassFinger:
            case bassPick:
                body[0].setLowShelf (sr, 120.0, 3.0);
                body[1].setLowpass (sr, m == bassFinger ? 2200.0 : 4200.0, 0.7);
                body[2].setPeak (sr, 800.0, 1.0, m == bassFinger ? -2.0 : 2.0);
                break;
            case guitarAcoustic:
                body[0].setPeak (sr, 105.0, 1.4, 7.0);
                body[1].setPeak (sr, 210.0, 1.6, 4.0);
                body[2].setPeak (sr, 2800.0, 0.8, 3.0);
                break;
            case guitarNylon:
                body[0].setPeak (sr, 95.0, 1.4, 6.0);
                body[1].setPeak (sr, 190.0, 1.5, 3.0);
                body[2].setLowpass (sr, 4500.0, 0.7);
                break;
            case guitarElectric:
                body[0].setPeak (sr, 2600.0, 1.2, 4.0);
                body[1].setLowpass (sr, 5500.0, 0.8);
                body[2].setHighpass (sr, 80.0, 0.7);
                break;
            default:
                body[0].setPeak (sr, 140.0, 1.2, 5.0);
                body[1].setLowpass (sr, 2400.0, 0.7);
                body[2].setHighpass (sr, 70.0, 0.7);
                break;
        }
    }

    inline float readLine (float delay) const noexcept
    {
        const int size = (int) line.size();
        float rp = (float) writePos - delay;
        while (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp;
        const float f = rp - (float) i0;
        const float a = line[(size_t) (i0 % size)], b = line[(size_t) ((i0 + 1) % size)];
        return a + (b - a) * f;
    }

    inline float nextModel (float bendRatio) noexcept
    {
        const float d = juce::jlimit (2.0f, (float) line.size() - 4.0f, period / bendRatio);
        const float y = readLine (d);
        // Filtro del lazo: media ponderada (más peso al pasado = más oscuro)
        lpState = y * (1.0f - damp * 0.5f) + lpState * (damp * 0.5f);
        const float g = released ? releaseLoopGain : loopGain;
        line[(size_t) writePos] = lpState * g;
        writePos = (writePos + 1) % (int) line.size();
        // Quitar continua
        const float out = y - dcX + 0.995f * dcY;
        dcX = y; dcY = out;
        float x = out;
        for (auto& b : body) x = b.process (x);
        if (model == guitarElectric) x = std::tanh (x * 1.4f) * 0.8f;
        return x;
    }
};

//==============================================================================
SamplerEngine::SamplerEngine (juce::AudioProcessorValueTreeState& s) : state (s)
{
    auto raw = [&s] (const char* id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };
    pType = raw ("ins_type"); pAttack = raw ("ins_attack"); pRelease = raw ("ins_release"); pTone = raw ("ins_tone");
    pChar = raw ("ins_char"); pTune = raw ("ins_tune"); pVel = raw ("ins_vel"); pVol = raw ("ins_vol");
    for (int i = 0; i < maxVoices; ++i)
    {
        voices.push_back (std::make_unique<Voice>());
        voices.back()->line.assign (8192, 0.0f);
    }
}

SamplerEngine::~SamplerEngine() = default;

void SamplerEngine::prepare (double sampleRate, int) { sr = sampleRate; reset(); }

void SamplerEngine::reset()
{
    for (auto& v : voices) v->active = false;
    sustained.fill (false);
    sustainPedal = false;
}

void SamplerEngine::updateInstrument()
{
    const int type = juce::jlimit (0, (int) instrumentList().size() - 1, juce::roundToInt (pType->load()));
    {
        const juce::SpinLock::ScopedLockType l (setLock);
        retired.erase (std::remove_if (retired.begin(), retired.end(), [] (const std::shared_ptr<Set>& r) { return r.use_count() == 1; }), retired.end());
    }
    if (type == loaded.load()) return;

    auto set = std::make_shared<Set>();
    set->instrument = type;
    const auto& info = instrumentList()[(size_t) type];
    if (! info.modeled)
    {
        for (const auto& e : sampleManifest())
            if (info.group == e.group)
                if (auto d = decodeSample (e))
                    set->zones.push_back ({ d, 0, 127 });
        std::sort (set->zones.begin(), set->zones.end(), [] (const Zone& a, const Zone& b) { return a.sample->root < b.sample->root; });
        for (size_t i = 0; i < set->zones.size(); ++i)
        {
            set->zones[i].lo = i == 0 ? 0 : (set->zones[i - 1].sample->root + set->zones[i].sample->root) / 2 + 1;
            set->zones[i].hi = i + 1 == set->zones.size() ? 127 : (set->zones[i].sample->root + set->zones[i + 1].sample->root) / 2;
        }
    }
    {
        const juce::SpinLock::ScopedLockType l (setLock);
        if (liveSet != nullptr) retired.push_back (liveSet);
        liveSet = set;
    }
    loaded.store (type);
}

void SamplerEngine::noteOn (int note, float vel)
{
    if (audioSet == nullptr) return;
    const auto& info = instrumentList()[(size_t) juce::jlimit (0, (int) instrumentList().size() - 1, audioSet->instrument)];
    const int played = juce::jlimit (0, 127, note + juce::roundToInt (pTune->load()));

    // Misma nota: se libera la anterior para no amontonar
    for (auto& v : voices)
        if (v->active && v->note == note && ! v->released) { v->released = true; }

    Voice* use = nullptr;
    for (auto& v : voices) if (! v->active) { use = v.get(); break; }
    if (use == nullptr)
        for (auto& v : voices) if (use == nullptr || v->age < use->age) use = v.get();

    auto& v = *use;
    v.active = true;
    v.released = false;
    v.note = note;
    v.vel = vel;
    v.age = ++counter;
    v.seed = 0x2545F491u * counter + 7u;
    v.pan = juce::jlimit (-0.3f, 0.3f, ((float) played - 64.0f) / 100.0f);
    v.env = 0.0f;
    v.attackInc = 1.0f / (float) (juce::jmax (0.0005f, pAttack->load()) * sr);
    v.tone.reset();

    if (info.modeled)
    {
        v.startModel (info.model, played, vel, pChar->load(), sr);
        if (info.model == 5) v.attackInc = 1.0f;   // la muteada ataca siempre en seco
    }
    else
    {
        v.isModel = false;
        const DecodedSample* best = nullptr;
        for (const auto& z : audioSet->zones)
            if (played >= z.lo && played <= z.hi) { best = z.sample.get(); break; }
        if (best == nullptr) { v.active = false; return; }
        v.sample = best;
        v.pos = 0.0;
        v.baseInc = best->sampleRate / sr * std::exp2 ((double) (played - best->root) / 12.0);
    }
}

void SamplerEngine::noteOff (int note)
{
    for (auto& v : voices)
        if (v->active && v->note == note && ! v->released)
        {
            if (sustainPedal) sustained[(size_t) note] = true;
            else v->released = true;
        }
}

void SamplerEngine::render (float* L, float* R, int numSamples, const juce::MidiBuffer& midi)
{
    {
        const juce::SpinLock::ScopedTryLockType l (setLock);
        if (l.isLocked() && liveSet.get() != audioSet.get())
        {
            // Cambio de instrumento: se cortan las voces del anterior
            for (auto& v : voices) v->active = false;
            audioSet = liveSet;
        }
    }

    const float tone = pTone->load();
    const float velSens = pVel->load();
    const float vol = juce::Decibels::decibelsToGain (pVol->load(), -60.0f);
    const float relCoef = (float) std::exp (-4.6 / (juce::jmax (0.01f, pRelease->load()) * sr));
    const float gainInfo = audioSet != nullptr ? instrumentList()[(size_t) juce::jlimit (0, (int) instrumentList().size() - 1, audioSet->instrument)].gain : 1.0f;

    auto it = midi.begin();
    int pos = 0;
    while (pos < numSamples)
    {
        while (it != midi.end() && (*it).samplePosition <= pos)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff()) noteOff (m.getNoteNumber());
            else if (m.isPitchWheel()) pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
            else if (m.isSustainPedalOn()) sustainPedal = true;
            else if (m.isSustainPedalOff())
            {
                sustainPedal = false;
                for (auto& v : voices)
                    if (v->active && sustained[(size_t) v->note]) v->released = true;
                sustained.fill (false);
            }
            else if (m.isAllNotesOff() || m.isAllSoundOff()) for (auto& v : voices) v->released = true;
            ++it;
        }
        int n = numSamples - pos;
        if (it != midi.end()) n = juce::jmin (n, juce::jmax (1, (*it).samplePosition - pos));

        const float bendRatio = std::exp2 (pitchBend * 2.0f / 12.0f);
        for (auto& vp : voices)
        {
            auto& v = *vp;
            if (! v.active) continue;
            const float cutoff = juce::jlimit (60.0f, (float) sr * 0.45f, 400.0f * std::exp2 (tone * 5.6f) * (0.55f + 0.45f * v.vel));
            v.tone.set (sr, cutoff, 0.7);
            const float velGain = (1.0f - velSens + velSens * std::pow (v.vel, 1.5f)) * 0.55f * gainInfo;
            const float gl = std::min (1.0f, 1.0f - v.pan) * velGain, gr = std::min (1.0f, 1.0f + v.pan) * velGain;
            for (int i = 0; i < n; ++i)
            {
                float x;
                if (v.isModel) x = v.nextModel (bendRatio);
                else
                {
                    const auto* s = v.sample;
                    const int len = s->data.getNumSamples() - 4;
                    if (v.pos >= len)
                    {
                        if (s->loopStart >= 0) v.pos -= (double) (len - s->loopStart);
                        else { v.active = false; break; }
                    }
                    const int ip = (int) v.pos;
                    const float f = (float) (v.pos - ip);
                    const float* d = s->data.getReadPointer (0);
                    x = d[ip] + (d[ip + 1] - d[ip]) * f;
                    v.pos += v.baseInc * bendRatio;
                }
                if (v.released) { v.env *= relCoef; if (v.env < 1.0e-4f) { v.active = false; break; } }
                else if (v.env < 1.0f) v.env = std::min (1.0f, v.env + v.attackInc);
                x = v.tone.process (x).lp * v.env;
                L[pos + i] += x * gl * vol;
                R[pos + i] += x * gr * vol;
            }
        }
        pos += n;
    }
    while (it != midi.end()) ++it;
}
} // namespace geminis::inst
