#include "Drums.h"

namespace geminis::inst
{
namespace
{
    juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    const std::vector<int>& drumSampleIndices()
    {
        static const std::vector<int> idx = []
        {
            std::vector<int> v;
            const auto& m = sampleManifest();
            for (int i = 0; i < (int) m.size(); ++i)
                if (juce::String (m[(size_t) i].group) == "drum") v.push_back (i);
            return v;
        }();
        return idx;
    }

    // Mapa General MIDI de cada pad. Las notas 84-86 (fila 4) quedan fuera de la batería General MIDI
    // para que un archivo de batería no dispare la melodía, el acorde ni la voz.
    const int gmNotes[numPads] = { 36, 38, 42, 46,   35, 37, 44, 39,   45, 48, 49, 51,   84, 85, 86, 29 };

    // Identificadores internos de las voces: después de los modelos va la muestra y luego los sonidos nuevos
    constexpr int vSample = numDrumModels;
    constexpr int vPluck = numDrumModels + 1 + dlPluck;
    constexpr int vChord = numDrumModels + 1 + dlChord;
    constexpr int vVoice = numDrumModels + 1 + dlVoice;

    // Nota de partida de los sonidos melódicos: do central (la afinación los transporta en semitonos)
    constexpr float melodicRoot = 261.63f;

    inline float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }
}

const juce::StringArray& drumSoundNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray n { U ("Bombo seco"), U ("Bombo profundo"), U ("Bombo duro"), U ("Caja eléctrica"), U ("Caja rota"),
                              U ("Palmas sintéticas"), U ("Charles cerrado sintético"), U ("Charles abierto sintético"),
                              U ("Tom sintético"), U ("Platillo sintético"), U ("Cencerro sintético"), U ("Aro sintético"),
                              U ("Maraca sintética"), U ("Láser"), U ("Bajo 808"), U ("Metal industrial"), U ("Ruido"), U ("Glitch") };
        for (int i : drumSampleIndices())
            n.add (drumSampleDisplayName (sampleManifest()[(size_t) i].resource));
        n.addArray ({ U ("Nota pulsada"), U ("Acorde"), U ("Voz") });   // en el orden de DrumLateModel
        jassert (n.size() == numDrumModels + (int) drumSampleIndices().size() + numLateDrumModels);
        return n;
    }();
    return names;
}

int numDrumSamples()      { return (int) drumSampleIndices().size(); }
int firstLateDrumSound()  { return numDrumModels + numDrumSamples(); }

int drumSoundIndex (const juce::String& name) { return drumSoundNames().indexOf (name); }

const juce::StringArray& padRoleNames()
{
    static const juce::StringArray n { "Bombo", "Caja", "Charles", "Abierto",
                                       "Bombo 2", "Caja 2", "Charles 2", "Perc",
                                       "Tom grave", "Tom agudo", "Perc 2", "Perc 3",
                                       U ("Melodía"), "Acorde", "Voz", "FX" };
    return n;
}

int padToNote (int pad, int map)
{
    pad = juce::jlimit (0, numPads - 1, pad);
    return map == 1 ? gmNotes[pad] : 36 + pad;
}

int noteToPad (int note, int map)
{
    if (map == 0) return note >= 36 && note < 36 + numPads ? note - 36 : -1;
    for (int p = 0; p < numPads; ++p)
        if (gmNotes[p] == note) return p;
    switch (note)
    {
        case 40: return 1;                                   // caja eléctrica
        case 41: case 43: return 8;                          // toms graves
        case 47: case 50: return 9;                          // toms agudos
        case 52: case 55: case 57: return 10;                // otros platillos
        case 53: case 59: case 56: case 75: case 76: case 77: return 11;   // campana, cencerro, claves, bloques
        case 54: case 69: case 70: case 82: return 6;        // pandereta, cabasa, maracas, shaker
        case 60: case 61: case 62: case 63: case 64: case 80: case 81: return 7;   // bongós, congas, triángulo
        case 27: case 28: case 30: case 31: return 15;       // efectos
        default: return -1;
    }
}

//==============================================================================
juce::String Pattern::toString() const
{
    juce::String s;
    for (int p = 0; p < numPads; ++p)
    {
        for (int k = 0; k < numSteps; ++k) s << juce::String::charToString ((juce::juce_wchar) ('0' + steps[(size_t) p][(size_t) k]));
        if (p < numPads - 1) s << "|";
    }
    return s;
}

Pattern Pattern::fromString (const juce::String& s)
{
    Pattern pt;
    juce::StringArray rows;
    rows.addTokens (s, "|", {});
    for (int p = 0; p < juce::jmin (numPads, rows.size()); ++p)
        for (int k = 0; k < juce::jmin (numSteps, rows[p].length()); ++k)
            pt.steps[(size_t) p][(size_t) k] = (juce::uint8) juce::jlimit (0, 3, (int) rows[p][k] - (int) '0');
    return pt;
}

bool Pattern::isEmpty() const
{
    for (const auto& r : steps) for (auto v : r) if (v) return false;
    return true;
}

//==============================================================================
struct DrumEngine::Voice
{
    bool active = false;
    int pad = -1, sound = 0;
    float vel = 1.0f, tune = 0, decay = 0.5f, tone = 0.5f, snap = 0.5f, drive = 0, level = 0.75f, pan = 0;
    int choke = 0;
    juce::uint32 age = 0;
    double t = 0.0, endT = 1.0;
    float ph[6] {};
    juce::uint32 seed = 22222u;
    dsp::Biquad f1, f2, f3, toneLp, toneHp;
    bool useLp = false, useHp = false;
    dsp::OnePole sweep;
    float chokeGain = 1.0f, chokeStep = 0.0f;
    const DecodedSample* sample = nullptr;
    double pos = 0.0, rate = 1.0;
    float sampleTau = 0.0f;
    float hold = 0.0f; int holdLeft = 0;

    inline float noise() noexcept { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 8388608.0f - 1.0f; }

    void start (double sr)
    {
        active = true;
        t = 0.0;
        for (auto& p : ph) p = 0.0f;
        chokeGain = 1.0f; chokeStep = 0.0f;
        f1.reset(); f2.reset(); f3.reset(); toneLp.reset(); toneHp.reset(); sweep.reset();
        hold = 0.0f; holdLeft = 0;
        const float tr = std::exp2 (tune / 12.0f);
        const float dm = 0.25f * std::pow (16.0f, decay);
        float tau = 0.3f;
        switch (sound)
        {
            case dmKick:       tau = 0.32f * dm; f1.setHighpass (sr, 900.0, 0.7); break;
            case dmKickDeep:   tau = 0.9f * dm; break;
            case dmKickHard:   tau = 0.38f * dm; f1.setHighpass (sr, 1500.0, 0.7); break;
            case dmSnare:      tau = 0.17f * dm; f1.setBandpass (sr, 4200.0 * tr, 0.7); break;
            case dmSnareNoise: tau = 0.24f * dm; f1.setBandpass (sr, 2000.0 * tr, 0.9); break;
            case dmClap:       tau = 0.14f * dm; f1.setBandpass (sr, 1150.0 * tr, 1.6); break;
            case dmHatClosed:  tau = 0.045f * dm; f1.setBandpass (sr, 10000.0, 1.0); f2.setHighpass (sr, 6500.0 * tr, 0.7); break;
            case dmHatOpen:    tau = 0.35f * dm; f1.setBandpass (sr, 10000.0, 1.0); f2.setHighpass (sr, 6500.0 * tr, 0.7); break;
            case dmTom:        tau = 0.32f * dm; break;
            case dmCymbal:     tau = 1.1f * dm; f1.setBandpass (sr, 6800.0 * tr, 0.8); f2.setHighpass (sr, 4500.0, 0.7); break;
            case dmCowbell:    tau = 0.22f * dm; f1.setBandpass (sr, 2600.0 * tr, 1.8); break;
            case dmRim:        tau = 0.03f * dm; f1.setBandpass (sr, 3200.0 * tr, 2.0); break;
            case dmShaker:     tau = 0.07f * dm; f1.setHighpass (sr, 5500.0 * tr, 0.8); break;
            case dmZap:        tau = 0.16f * dm; break;
            case dmBass808:    tau = 1.4f * dm; break;
            case dmMetal:      tau = 0.45f * dm; f1.setBandpass (sr, 3000.0 * tr, 1.2); break;
            case dmNoise:      tau = 0.7f * dm; break;
            case dmGlitch:     tau = 0.12f * dm; break;
            case vPluck:       tau = 0.45f * dm; break;
            case vChord:       tau = 0.55f * dm; break;
            case vVoice:       // formantes de una «a» abierta
                tau = 0.7f * dm;
                f1.setBandpass (sr, 800.0, 6.0); f2.setBandpass (sr, 1200.0, 8.0); f3.setBandpass (sr, 2800.0, 10.0);
                break;
            default: break;
        }
        endT = (double) tau * 7.0 + 0.05;

        if (sample != nullptr)
        {
            rate = sample->sampleRate / sr * tr;
            sampleTau = decay < 0.5f ? 0.02f * std::pow (30.0f, decay * 2.0f) : 0.0f;
            pos = 0.0;
            endT = sample->data.getNumSamples() / (sample->sampleRate * tr) + 0.01;
            if (sampleTau > 0.0f) endT = std::min (endT, (double) sampleTau * 7.0);
        }

        useLp = tone < 0.48f;
        useHp = tone > 0.52f;
        if (useLp) toneLp.setLowpass (sr, 300.0 * std::pow (20000.0 / 300.0, tone / 0.48), 0.7);
        if (useHp) toneHp.setHighpass (sr, 20.0 * std::pow (100.0, (tone - 0.52) / 0.48), 0.7);
    }

    float metallic (float tr, double sr) noexcept
    {
        static const float freqs[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
        float m = 0.0f;
        for (int k = 0; k < 6; ++k)
        {
            ph[k] += freqs[k] * 1.6f * tr / (float) sr;
            if (ph[k] >= 1.0f) ph[k] -= 1.0f;
            m += ph[k] < 0.5f ? 1.0f : -1.0f;
        }
        return m / 6.0f;
    }

    inline float osc (int k, float hz, double sr) noexcept
    {
        ph[k] += hz / (float) sr;
        ph[k] -= std::floor (ph[k]);
        return std::sin (twoPi * ph[k]);
    }

    /** Suma de armónicos de una fase (1/k) en la que los agudos se apagan antes, como una cuerda
        pulsada. Los senos de cada armónico salen por recurrencia, sin un seno por armónico. */
    static float harmonics (float phase, int count, float decayPerHarmonic, float hz, double sr) noexcept
    {
        const float th = twoPi * phase;
        const float c2 = 2.0f * std::cos (th);
        float sPrev = 0.0f, s = std::sin (th), amp = 1.0f, sum = 0.0f;
        for (int k = 1; k <= count && hz * (float) k < (float) sr * 0.45f; ++k)
        {
            sum += s * amp / (float) k;
            const float next = c2 * s - sPrev;
            sPrev = s; s = next;
            amp *= decayPerHarmonic;
        }
        return sum;
    }

    inline float advance (int k, float hz, double sr) noexcept
    {
        ph[k] += hz / (float) sr;
        ph[k] -= std::floor (ph[k]);
        return ph[k];
    }

    float nextModel (double sr) noexcept
    {
        const float T = (float) (t / sr);
        const float tr = std::exp2 (tune / 12.0f);
        const float dm = 0.25f * std::pow (16.0f, decay);
        switch (sound)
        {
            case dmKick:
            {
                const float f = 50.0f * tr * (1.0f + (2.5f + snap * 6.0f) * std::exp (-T / 0.03f));
                const float body = osc (0, f, sr) * std::exp (-T / (0.32f * dm));
                const float click = f1.process (noise()) * std::exp (-T / 0.0015f) * snap * 0.8f;
                return body * 1.1f + click;
            }
            case dmKickDeep:
            {
                const float f = 44.0f * tr * (1.0f + (0.8f + snap * 2.5f) * std::exp (-T / 0.045f));
                const float att = std::min (1.0f, T / 0.002f);
                return osc (0, f, sr) * std::exp (-T / (0.9f * dm)) * att * 1.1f;
            }
            case dmKickHard:
            {
                const float f = 56.0f * tr * (1.0f + (5.0f + snap * 6.0f) * std::exp (-T / 0.018f));
                const float body = osc (0, f, sr) * std::exp (-T / (0.38f * dm));
                const float click = f1.process (noise()) * std::exp (-T / 0.004f) * (0.3f + snap * 0.6f);
                return std::tanh ((body + click) * 2.5f) * 0.9f;
            }
            case dmSnare:
            {
                const float pe = 1.0f + 0.35f * std::exp (-T / 0.01f);
                const float body = (osc (0, 185.0f * tr * pe, sr) * 0.6f + osc (1, 330.0f * tr * pe, sr) * 0.4f) * std::exp (-T / (0.09f * dm));
                const float nz = f1.process (noise()) * std::exp (-T / (0.17f * dm)) * 1.6f;
                const float click = noise() * std::exp (-T / 0.0012f) * snap * 0.5f;
                return body * 0.75f + nz + click;
            }
            case dmSnareNoise:
            {
                const float body = osc (0, 220.0f * tr, sr) * std::exp (-T / (0.06f * dm)) * 0.4f;
                float nz = f1.process (noise()) * std::exp (-T / (0.24f * dm)) * 2.2f;
                nz = std::round (nz * 8.0f) / 8.0f;
                return body + nz + noise() * std::exp (-T / 0.002f) * snap * 0.4f;
            }
            case dmClap:
            {
                float env = 0.0f;
                for (float d : { 0.0f, 0.011f, 0.023f })
                    if (T >= d) env += std::exp (-(T - d) / 0.005f);
                if (T >= 0.03f) env += std::exp (-(T - 0.03f) / (0.14f * dm)) * 0.75f;
                return f1.process (noise()) * env * 2.4f;
            }
            case dmHatClosed:
            case dmHatOpen:
            {
                const float tau = sound == dmHatClosed ? 0.045f : 0.35f;
                const float m = f2.process (f1.process (metallic (tr, sr) + noise() * 0.25f));
                const float click = noise() * std::exp (-T / 0.0008f) * snap * 0.3f;
                return m * std::exp (-T / (tau * dm)) * 1.8f + click;
            }
            case dmTom:
            {
                const float f = 105.0f * tr * (1.0f + 0.7f * std::exp (-T / 0.06f));
                return osc (0, f, sr) * std::exp (-T / (0.32f * dm)) + noise() * std::exp (-T / 0.004f) * snap * 0.3f;
            }
            case dmCymbal:
            {
                const float m = f1.process (metallic (tr * 1.2f, sr)) + f2.process (noise()) * 0.4f;
                const float att = std::min (1.0f, T / 0.0015f);
                return m * std::exp (-T / (1.1f * dm)) * att * 1.4f;
            }
            case dmCowbell:
            {
                ph[0] += 562.0f * tr / (float) sr; ph[0] -= std::floor (ph[0]);
                ph[1] += 845.0f * tr / (float) sr; ph[1] -= std::floor (ph[1]);
                const float sq = (ph[0] < 0.5f ? 1.0f : -1.0f) + (ph[1] < 0.5f ? 1.0f : -1.0f);
                const float env = 0.6f * std::exp (-T / 0.015f) + 0.4f * std::exp (-T / (0.22f * dm));
                return f1.process (sq * 0.5f) * env * 1.6f;
            }
            case dmRim:
                return osc (0, 1700.0f * tr, sr) * std::exp (-T / (0.012f * dm)) * 0.6f + f1.process (noise()) * std::exp (-T / 0.004f) * 1.5f;
            case dmShaker:
            {
                const float att = std::min (1.0f, T / 0.006f);
                return f1.process (noise()) * att * std::exp (-T / (0.07f * dm)) * 1.2f;
            }
            case dmZap:
            {
                const float f = 80.0f * tr + 3500.0f * tr * std::exp (-T / (0.04f * dm));
                return osc (0, f, sr) * std::exp (-T / (0.16f * dm)) * 0.8f;
            }
            case dmBass808:
            {
                const float f = 55.0f * tr * (1.0f + 0.12f * std::exp (-T / 0.04f));
                const float att = std::min (1.0f, T / 0.002f);
                return osc (0, f, sr) * std::exp (-T / (1.4f * dm)) * att + noise() * std::exp (-T / 0.001f) * snap * 0.2f;
            }
            case dmMetal:
            {
                const float fc = 180.0f * tr;
                ph[1] += fc * 1.414f / (float) sr; ph[1] -= std::floor (ph[1]);
                const float index = 6.0f * std::exp (-T / 0.08f) + 1.0f;
                ph[0] += fc / (float) sr; ph[0] -= std::floor (ph[0]);
                const float y = std::sin (twoPi * ph[0] + index * std::sin (twoPi * ph[1]));
                const float nz = f1.process (noise()) * std::exp (-T / 0.05f) * 0.5f;
                return std::tanh ((y + nz) * 2.5f) * std::exp (-T / (0.45f * dm)) * 0.8f;
            }
            case dmNoise:
            {
                if (((int) t & 31) == 0) sweep.setCutoff (sr, 1000.0 + 11000.0 * std::exp (-T / (0.25f * dm)));
                return sweep.lowpass (noise()) * std::exp (-T / (0.7f * dm)) * 1.3f;
            }
            case vPluck:
            {
                // Nota pulsada: dos osciladores un poco desafinados; GOLPE da más brillo al ataque
                const float f = melodicRoot * tr;
                const float fade = std::exp (-T * (8.0f + (1.0f - snap) * 30.0f));
                const float a = harmonics (advance (0, f, sr), 10, fade, f, sr);
                const float b = harmonics (advance (1, f * 1.003f, sr), 10, fade, f, sr);
                const float att = std::min (1.0f, T / 0.002f);
                return (a + b) * 0.5f * att * std::exp (-T / (0.45f * dm)) * 0.95f;
            }
            case vChord:
            {
                // Acorde menor con séptima (do, mi bemol, sol, si bemol), que encaja en casi cualquier estilo
                static const float ratios[4] = { 1.0f, 1.189207f, 1.498307f, 1.781797f };
                const float fade = std::exp (-T * (3.0f + (1.0f - snap) * 14.0f));
                float x = 0.0f;
                for (int k = 0; k < 4; ++k)
                {
                    const float f = melodicRoot * tr * ratios[k];
                    x += harmonics (advance (k, f, sr), 6, fade, f, sr);
                }
                const float att = std::min (1.0f, T / 0.003f);
                return x * 0.32f * att * std::exp (-T / (0.55f * dm));
            }
            case vVoice:
            {
                // Voz cantando «aa»: diente de sierra con vibrato que entra poco a poco, por tres formantes
                const float vibAmount = juce::jlimit (0.0f, 1.0f, (T - 0.15f) / 0.25f);
                const float f = melodicRoot * tr * (1.0f + 0.011f * vibAmount * std::sin (twoPi * 5.2f * T));
                const float dt = f / (float) sr;
                ph[0] += dt;
                if (ph[0] >= 1.0f) ph[0] -= 1.0f;
                const float src = 2.0f * ph[0] - 1.0f - polyBlep (ph[0], dt) + noise() * 0.04f;
                const float y = f1.process (src) + f2.process (src) * 0.7f + f3.process (src) * 0.35f;
                const float att = std::min (1.0f, T / 0.03f);
                return y * att * std::exp (-T / (0.7f * dm)) * 2.8f;
            }
            case dmGlitch:
            default:
            {
                if (--holdLeft <= 0)
                {
                    hold = std::round (noise() * 3.0f) / 3.0f;
                    holdLeft = 1 + (int) ((0.5f + 0.5f * noise()) * 40.0f / tr);
                }
                const float gate = std::sin (twoPi * T * (18.0f + 30.0f * snap)) > -0.2f ? 1.0f : 0.0f;
                return hold * gate * std::exp (-T / (0.12f * dm)) * 0.7f;
            }
        }
    }

    void render (float* L, float* R, int n, double sr) noexcept
    {
        const float gain = level * level * 1.4f * vel;
        const float gl = std::min (1.0f, 1.0f - pan) * gain, gr = std::min (1.0f, 1.0f + pan) * gain;
        const float driveGain = 1.0f + drive * 10.0f, driveComp = 1.0f / std::sqrt (driveGain);
        for (int i = 0; i < n; ++i)
        {
            float x;
            if (sample != nullptr)
            {
                const int len = sample->data.getNumSamples() - 4;
                const int ip = (int) pos;
                if (ip >= len) { active = false; return; }
                const float* d = sample->data.getReadPointer (0);
                const float f = (float) (pos - ip);
                x = d[ip] + (d[ip + 1] - d[ip]) * f;
                pos += rate;
                const float T = (float) (t / sr);
                if (sampleTau > 0.0f) x *= std::exp (-T / sampleTau);
                x *= 1.0f + (snap - 0.5f) * 1.6f * std::exp (-T / 0.012f);
            }
            else x = nextModel (sr);

            if (useLp) x = toneLp.process (x);
            if (useHp) x = toneHp.process (x);
            if (drive > 0.001f) x = dsp::fastTanh (x * driveGain) * driveComp * 1.3f;
            if (chokeStep > 0.0f) { chokeGain -= chokeStep; if (chokeGain <= 0.0f) { active = false; return; } x *= chokeGain; }

            L[i] += x * gl;
            R[i] += x * gr;
            t += 1.0;
        }
        if (t / sr > endT) active = false;
    }
};

//==============================================================================
DrumEngine::DrumEngine (juce::AudioProcessorValueTreeState& s) : state (s)
{
    auto raw = [&s] (const juce::String& id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };
    for (int p = 0; p < numPads; ++p)
    {
        auto& pp = padP[(size_t) p];
        pp.snd = raw (padParamId (p, "snd")); pp.tune = raw (padParamId (p, "tune")); pp.decay = raw (padParamId (p, "decay"));
        pp.tone = raw (padParamId (p, "tone")); pp.snap = raw (padParamId (p, "snap")); pp.drive = raw (padParamId (p, "drive"));
        pp.level = raw (padParamId (p, "level")); pp.pan = raw (padParamId (p, "pan")); pp.choke = raw (padParamId (p, "choke"));
    }
    seqMode = raw ("seq_mode"); seqPattern = raw ("seq_pattern"); seqSwing = raw ("seq_swing"); seqLen = raw ("seq_len");
    seqBpm = raw ("seq_bpm"); midiMap = raw ("midi_map"); volume = raw ("dr_vol");
    for (int i = 0; i < 40; ++i) voices.push_back (std::make_unique<Voice>());
}

void DrumEngine::loadSamples()
{
    if (samplesReady.load()) return;
    std::vector<std::shared_ptr<DecodedSample>> loaded;
    for (int i : drumSampleIndices())
        loaded.push_back (decodeSample (sampleManifest()[(size_t) i]));
    samples = std::move (loaded);
    samplesReady.store (true);
}

DrumEngine::~DrumEngine() = default;

void DrumEngine::prepare (double sampleRate, int) { sr = sampleRate; reset(); }

void DrumEngine::reset()
{
    for (auto& v : voices) v->active = false;
    internalPpq = 0.0;
}

void DrumEngine::triggerFromUi (int pad, float velocity)
{
    const auto scope = uiFifo.write (1);
    if (scope.blockSize1 > 0) uiEvents[(size_t) scope.startIndex1] = { pad, velocity };
    else if (scope.blockSize2 > 0) uiEvents[(size_t) scope.startIndex2] = { pad, velocity };
}

Pattern DrumEngine::getPattern (int index) const
{
    const juce::SpinLock::ScopedLockType l (patternLock);
    return patterns[(size_t) juce::jlimit (0, numPatterns - 1, index)];
}

void DrumEngine::setPattern (int index, const Pattern& p)
{
    const juce::SpinLock::ScopedLockType l (patternLock);
    patterns[(size_t) juce::jlimit (0, numPatterns - 1, index)] = p;
}

void DrumEngine::setStep (int pattern, int pad, int step, int value)
{
    const juce::SpinLock::ScopedLockType l (patternLock);
    patterns[(size_t) juce::jlimit (0, numPatterns - 1, pattern)].steps[(size_t) juce::jlimit (0, numPads - 1, pad)]
            [(size_t) juce::jlimit (0, numSteps - 1, step)] = (juce::uint8) juce::jlimit (0, 3, value);
}

void DrumEngine::trigger (int pad, float velocity, int)
{
    if (pad < 0 || pad >= numPads) return;
    const auto& pp = padP[(size_t) pad];
    const int sound = juce::roundToInt (pp.snd->load());
    const int choke = juce::roundToInt (pp.choke->load());

    // Grupo de corte: el charles cerrado apaga el abierto, etc.
    int samePad = 0;
    Voice* oldestSame = nullptr;
    for (auto& v : voices)
    {
        if (! v->active) continue;
        if (choke > 0 && v->choke == choke && v->pad != pad && v->chokeStep == 0.0f)
            v->chokeStep = 1.0f / (float) (0.006 * sr);
        if (v->pad == pad)
        {
            ++samePad;
            if (oldestSame == nullptr || v->age < oldestSame->age) oldestSame = v.get();
        }
    }
    if (samePad >= 3 && oldestSame != nullptr && oldestSame->chokeStep == 0.0f)
        oldestSame->chokeStep = 1.0f / (float) (0.004 * sr);

    Voice* use = nullptr;
    for (auto& v : voices)
        if (! v->active) { use = v.get(); break; }
    if (use == nullptr)
        for (auto& v : voices)
            if (use == nullptr || v->age < use->age) use = v.get();

    auto& v = *use;
    v.pad = pad;
    v.sound = sound;
    v.vel = juce::jlimit (0.0f, 1.0f, velocity);
    v.tune = pp.tune->load(); v.decay = pp.decay->load(); v.tone = pp.tone->load(); v.snap = pp.snap->load();
    v.drive = pp.drive->load(); v.level = pp.level->load(); v.pan = pp.pan->load(); v.choke = choke;
    v.sample = nullptr;
    if (sound >= firstLateDrumSound())
    {
        v.sound = vPluck + juce::jmin (numLateDrumModels - 1, sound - firstLateDrumSound());
    }
    else if (sound >= numDrumModels)
    {
        const int si = sound - numDrumModels;
        if (! samplesReady.load() || si >= (int) samples.size() || samples[(size_t) si] == nullptr) return;
        v.sample = samples[(size_t) si].get();
        v.sound = vSample;
    }
    v.seed = 0x9E3779B9u * (juce::uint32) (++counter);
    v.age = counter;
    v.start (sr);
    hitCount[(size_t) pad].fetch_add (1);
}

void DrumEngine::renderVoices (float* L, float* R, int start, int n)
{
    for (auto& v : voices)
        if (v->active) v->render (L + start, R + start, n, sr);
}

void DrumEngine::render (float* L, float* R, int numSamples, const juce::MidiBuffer& midi, juce::AudioPlayHead* playHead)
{
    struct Event { int offset, pad; float vel; };
    Event events[256];
    int numEvents = 0;
    auto add = [&] (int off, int pad, float vel) { if (numEvents < 256) events[numEvents++] = { juce::jlimit (0, numSamples - 1, off), pad, vel }; };

    // Pads pulsados en la pantalla
    {
        const auto scope = uiFifo.read (uiFifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i) add (0, uiEvents[(size_t) (scope.startIndex1 + i)].first, uiEvents[(size_t) (scope.startIndex1 + i)].second);
        for (int i = 0; i < scope.blockSize2; ++i) add (0, uiEvents[(size_t) (scope.startIndex2 + i)].first, uiEvents[(size_t) (scope.startIndex2 + i)].second);
    }

    // Notas MIDI
    const int map = juce::roundToInt (midiMap->load());
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            const int pad = noteToPad (m.getNoteNumber(), map);
            if (pad >= 0) add (meta.samplePosition, pad, m.getFloatVelocity());
        }
    }

    // Secuenciador
    const int mode = juce::roundToInt (seqMode->load());
    double bpm = seqBpm->load(), ppq = 0.0;
    bool hostPlaying = false;
    if (playHead != nullptr)
        if (auto pos = playHead->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            hostPlaying = pos->getIsPlaying();
            if (auto p = pos->getPpqPosition()) ppq = *p;
        }
    const double ppqPerSample = bpm / 60.0 / sr;
    bool run = false;
    if (mode == 1 && hostPlaying) run = true;
    if (mode == 2) { run = true; if (! hostPlaying) ppq = internalPpq; }
    if (mode == 2 && ! hostPlaying) internalPpq += numSamples * ppqPerSample;
    else if (! hostPlaying) internalPpq = 0.0;

    if (run)
    {
        const int len = juce::roundToInt (seqLen->load()) == 1 ? 32 : 16;
        const int pat = juce::jlimit (0, numPatterns - 1, juce::roundToInt (seqPattern->load()));
        const double stepLen = 0.25;
        const double swingDelay = seqSwing->load() * 0.01 * stepLen * 0.66;
        const double ppqEnd = ppq + numSamples * ppqPerSample;
        Pattern current;
        {
            const juce::SpinLock::ScopedTryLockType l (patternLock);
            if (l.isLocked()) current = patterns[(size_t) pat];
        }
        const auto first = (juce::int64) std::floor (ppq / stepLen) - 1;
        const auto last = (juce::int64) std::floor (ppqEnd / stepLen);
        for (auto s = first; s <= last; ++s)
        {
            const double at = (double) s * stepLen + ((s & 1) != 0 ? swingDelay : 0.0);
            if (at < ppq || at >= ppqEnd) continue;
            const int step = (int) (((s % len) + len) % len);
            const int off = (int) ((at - ppq) / ppqPerSample);
            for (int p = 0; p < numPads; ++p)
            {
                const int v = current.steps[(size_t) p][(size_t) step];
                if (v > 0) add (off, p, v == 1 ? 0.45f : v == 2 ? 0.78f : 1.0f);
            }
        }
        currentStep.store ((int) ((((juce::int64) std::floor (ppqEnd / stepLen) % len) + len) % len));
    }
    else currentStep.store (-1);

    // Ordenar por posición y sonar
    std::sort (events, events + numEvents, [] (const Event& a, const Event& b) { return a.offset < b.offset; });
    float tmpL[512], tmpR[512];
    int pos = 0, e = 0;
    const float vol = juce::Decibels::decibelsToGain (volume->load(), -60.0f);
    while (pos < numSamples)
    {
        while (e < numEvents && events[e].offset <= pos) { trigger (events[e].pad, events[e].vel, events[e].offset); ++e; }
        int n = numSamples - pos;
        if (e < numEvents) n = juce::jmin (n, events[e].offset - pos);
        n = juce::jmin (n, 512);
        std::fill (tmpL, tmpL + n, 0.0f);
        std::fill (tmpR, tmpR + n, 0.0f);
        renderVoices (tmpL, tmpR, 0, n);
        for (int i = 0; i < n; ++i) { L[pos + i] += tmpL[i] * vol; R[pos + i] += tmpR[i] * vol; }
        pos += n;
    }
}

juce::File DrumEngine::exportMidi (int patternIndex, const juce::String& kitName) const
{
    const auto p = getPattern (patternIndex);
    const int len = juce::roundToInt (seqLen->load()) == 1 ? 32 : 16;
    const int map = juce::roundToInt (midiMap->load());
    const double swing = seqSwing->load() * 0.01 * 0.66;
    juce::MidiMessageSequence seq;
    const int tpq = 96, tickStep = tpq / 4;
    for (int bar = 0; bar < (len == 16 ? 2 : 1); ++bar)
        for (int s = 0; s < len; ++s)
            for (int pad = 0; pad < numPads; ++pad)
            {
                const int v = p.steps[(size_t) pad][(size_t) s];
                if (v == 0) continue;
                const double tick = (bar * len + s) * tickStep + ((s & 1) ? swing * tickStep : 0.0);
                const int vel = v == 1 ? 50 : v == 2 ? 95 : 125;
                seq.addEvent (juce::MidiMessage::noteOn (10, padToNote (pad, map), (juce::uint8) vel), tick);
                seq.addEvent (juce::MidiMessage::noteOff (10, padToNote (pad, map)), tick + tickStep * 0.5);
            }
    seq.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote (tpq);
    file.addTrack (seq);

    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Geminis").getChildFile ("MIDI");
    dir.createDirectory();
    const char letters[] = { 'A', 'B', 'C', 'D' };
    auto out = dir.getChildFile (juce::File::createLegalFileName (kitName + " " + juce::String::charToString (letters[juce::jlimit (0, 3, patternIndex)])) + ".mid");
    out.deleteFile();
    if (auto os = out.createOutputStream()) file.writeTo (*os);
    return out;
}
} // namespace geminis::inst
