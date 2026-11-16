#include "SynthEngine.h"

namespace geminis::inst
{
namespace
{
    constexpr int maxUnison = 7;

    /** Envolvente ADSR con ataque lineal y caída/liberación exponenciales. */
    struct Env
    {
        enum Stage { idle, attack, decay, sustain, release };
        Stage stage = idle;
        float value = 0.0f, attackInc = 0.01f, decayCoef = 0.999f, sustainLevel = 1.0f, releaseCoef = 0.999f;

        void set (float a, float d, float s, float r, double sr) noexcept
        {
            attackInc = 1.0f / (float) (std::max (0.0005f, a) * sr);
            decayCoef = (float) std::exp (-4.6 / (std::max (0.001f, d) * sr));
            sustainLevel = s;
            releaseCoef = (float) std::exp (-4.6 / (std::max (0.001f, r) * sr));
        }
        void noteOn() noexcept { stage = attack; }
        void noteOff() noexcept { if (stage != idle) stage = release; }
        void kill() noexcept { stage = idle; value = 0.0f; }
        bool isActive() const noexcept { return stage != idle; }

        inline float next() noexcept
        {
            switch (stage)
            {
                case attack:  value += attackInc; if (value >= 1.0f) { value = 1.0f; stage = decay; } break;
                case decay:   value = sustainLevel + (value - sustainLevel) * decayCoef;
                              if (std::abs (value - sustainLevel) < 1.0e-4f) { value = sustainLevel; stage = sustain; } break;
                case sustain: value = sustainLevel; break;
                case release: value *= releaseCoef; if (value < 1.0e-5f) { value = 0.0f; stage = idle; } break;
                default: break;
            }
            return value;
        }
    };

    inline float hzFromPitch (float p) noexcept { return 440.0f * std::exp2 ((p - 69.0f) / 12.0f); }
}

//==============================================================================
struct SynthEngine::Snapshot
{
    struct Osc { int wave = 2, ctype = 0; float curve = 0; int oct = 0, semi = 0; float fine = 0, level = 0.8f; int uni = 1; float det = 0.25f; };
    std::array<Osc, 2> osc;
    float sub = 0, noise = 0, fm = 0, ring = 0; int subShape = 0;
    int fType = 0; float cut = 6000, res = 0.15f, drive = 0, fEnv = 0.3f, fKey = 0.4f, fVel = 0.2f;
    float env[3][4] {};
    int l1Shape = 0, l2Shape = 0; float l1Hz = 0.5f, l2Hz = 5.0f;
    struct Mod { int src = 0, dst = 0; float amt = 0; };
    std::array<Mod, numModSlots> mod;
    int voiceMode = 0; float glide = 0; int bend = 2; float velSens = 0.6f, width = 0.7f, vol = 1.0f;
    float modWheel = 0, aftertouch = 0, pitchBend = 0, lfo1 = 0;
};

//==============================================================================
struct SynthEngine::Voice
{
    int note = -1;
    float velocity = 0.0f;
    bool released = false;
    juce::uint32 age = 0;
    float pitch = 60.0f, targetPitch = 60.0f;
    Env envA, envF, envM;
    dsp::Lfo lfo2;
    float lfo2Value = 0.0f, randomValue = 0.0f;
    std::array<std::array<float, maxUnison>, 2> phase {};
    float subPhase = 0.0f;
    juce::uint32 noiseSeed = 1234567u;
    std::array<dsp::Svf, 2> fL, fR;

    bool isActive() const noexcept { return envA.isActive(); }

    void start (int n, float vel, float fromPitch, bool retrigger, juce::Random& rnd)
    {
        const bool wasActive = isActive();
        note = n;
        velocity = vel;
        released = false;
        targetPitch = (float) n;
        pitch = fromPitch;
        if (retrigger || ! wasActive)
        {
            envA.noteOn(); envF.noteOn(); envM.noteOn();
            lfo2.reset (0.0);
            randomValue = rnd.nextFloat() * 2.0f - 1.0f;
            if (! wasActive)
            {
                for (auto& o : phase) for (auto& p : o) p = rnd.nextFloat();
                for (auto& f : fL) f.reset();
                for (auto& f : fR) f.reset();
                envA.value = envF.value = envM.value = 0.0f;
            }
        }
    }

    void release() noexcept { released = true; envA.noteOff(); envF.noteOff(); envM.noteOff(); }

    void render (float* outL, float* outR, int n, const Snapshot& s, const std::array<const WaveSet*, 2>& tables, double sr)
    {
        const float srF = (float) sr;
        // Envolventes con los valores de este bloque de control
        envA.set (s.env[0][0], s.env[0][1], s.env[0][2], s.env[0][3], sr);
        envF.set (s.env[1][0], s.env[1][1], s.env[1][2], s.env[1][3], sr);
        envM.set (s.env[2][0], s.env[2][1], s.env[2][2], s.env[2][3], sr);

        // Portamento
        if (s.glide > 0.001f)
        {
            const float coef = 1.0f - std::exp (-(float) n / (s.glide * 0.35f * srF));
            pitch += (targetPitch - pitch) * coef;
        }
        else pitch = targetPitch;

        // Modulación
        float src[numModSources] {};
        src[srcLfo1] = s.lfo1;
        src[srcLfo2] = lfo2Value;
        src[srcEnvFilter] = envF.value;
        src[srcEnvMod] = envM.value;
        src[srcVelocity] = velocity;
        src[srcModWheel] = s.modWheel;
        src[srcAftertouch] = s.aftertouch;
        src[srcNote] = ((float) note - 60.0f) / 48.0f;
        src[srcRandom] = randomValue;
        float dst[numModDests] {};
        for (const auto& m : s.mod)
            if (m.src > 0 && m.dst > 0)
                dst[m.dst] += src[m.src] * m.amt;

        float lfoHz = s.l2Hz * std::exp2 (dst[dstLfo2Rate] * 3.0f);
        lfo2Value = lfo2.next ((double) lfoHz * n / sr, s.l2Shape);

        const float bendSemis = s.pitchBend * (float) s.bend;

        struct OscState
        {
            const float* ta = nullptr; const float* tb = nullptr; float ff = 0;
            int uni = 1; float inc[maxUnison] {}; float gl[maxUnison] {}, gr[maxUnison] {}; float norm = 1, level = 0;
            float baseInc = 0;
        } os[2];

        for (int o = 0; o < 2; ++o)
        {
            const auto& op = s.osc[(size_t) o];
            auto& st = os[o];
            st.level = juce::jlimit (0.0f, 1.5f, op.level + dst[o == 0 ? dstLevel1 : dstLevel2]);
            if (tables[(size_t) o] == nullptr) { st.level = 0; continue; }
            const float p = pitch + (float) (op.oct * 12 + op.semi) + op.fine * 0.01f + bendSemis
                          + dst[o == 0 ? dstPitch1 : dstPitch2] * 24.0f + dst[dstPitch] * 24.0f;
            const float inc = juce::jmin (0.45f, hzFromPitch (p) / srF);
            st.baseInc = inc;
            st.uni = juce::jlimit (1, maxUnison, op.uni);
            const float det = juce::jlimit (0.0f, 1.0f, op.det + dst[dstDetune]);
            const float detSemis = std::pow (det, 1.5f) * 0.7f;
            float maxInc = inc;
            for (int u = 0; u < st.uni; ++u)
            {
                const float spread = st.uni > 1 ? 2.0f * (float) u / (float) (st.uni - 1) - 1.0f : 0.0f;
                st.inc[u] = inc * std::exp2 (spread * detSemis / 12.0f);
                maxInc = std::max (maxInc, st.inc[u]);
                const float pan = spread * s.width;
                st.gl[u] = std::min (1.0f, 1.0f - pan);
                st.gr[u] = std::min (1.0f, 1.0f + pan);
            }
            st.norm = 1.0f / std::sqrt ((float) st.uni);
            const float c = juce::jlimit (0.0f, 1.0f, op.curve + dst[o == 0 ? dstCurve1 : dstCurve2]) * (float) (numFrames - 1);
            const int f0 = juce::jmin ((int) c, numFrames - 1), f1 = juce::jmin (f0 + 1, numFrames - 1);
            st.ff = c - (float) f0;
            const int mip = mipForIncrement (maxInc);
            st.ta = tables[(size_t) o]->table (f0, mip);
            st.tb = tables[(size_t) o]->table (f1, mip);
        }

        const float fm = juce::jlimit (0.0f, 1.0f, s.fm + dst[dstFm]) * 1.5f;
        const float ring = juce::jlimit (0.0f, 1.0f, s.ring + dst[dstRing]);
        const float subLevel = juce::jlimit (0.0f, 1.0f, s.sub + dst[dstSub]);
        const float noiseLevel = juce::jlimit (0.0f, 1.0f, s.noise + dst[dstNoise]) * 0.6f;
        const float subInc = os[0].baseInc > 0 ? os[0].baseInc * 0.5f : hzFromPitch (pitch - 12.0f) / srF;

        // Filtro
        const float cutSemis = s.fEnv * envF.value * 72.0f + s.fKey * ((float) note - 60.0f) + s.fVel * velocity * 24.0f + dst[dstCutoff] * 60.0f;
        const float cutHz = juce::jlimit (20.0f, srF * 0.45f, s.cut * std::exp2 (cutSemis / 12.0f));
        const float r = juce::jlimit (0.0f, 1.0f, s.res + dst[dstResonance]);
        const float q = 0.55f + r * r * (s.fType == 0 ? 11.0f : 16.0f);
        for (int k = 0; k < 2; ++k)
        {
            fL[(size_t) k].set (sr, cutHz, k == 0 ? q : 0.6);
            fR[(size_t) k].set (sr, cutHz, k == 0 ? q : 0.6);
        }
        const float driveGain = 1.0f + s.drive * 9.0f;
        const float driveComp = 1.0f / std::sqrt (driveGain);

        // Volumen
        const float velGain = 1.0f - s.velSens + s.velSens * std::pow (velocity, 1.3f);
        const float vol = juce::jlimit (0.0f, 2.0f, 1.0f + dst[dstVolume]) * velGain * 0.32f;
        const float pan = juce::jlimit (-1.0f, 1.0f, dst[dstPan]);
        const float panL = std::min (1.0f, 1.0f - pan), panR = std::min (1.0f, 1.0f + pan);
        const float bpK = fL[0].k;

        for (int i = 0; i < n; ++i)
        {
            // Oscilador 2 (también modula al 1)
            float o2L = 0, o2R = 0, o2m = 0;
            if (os[1].level > 0.0f || fm > 0.0f || ring > 0.0f)
            {
                auto& st = os[1];
                auto& ph = phase[1];
                for (int u = 0; u < st.uni; ++u)
                {
                    const float a = readTable (st.ta, ph[(size_t) u]);
                    const float v = a + (readTable (st.tb, ph[(size_t) u]) - a) * st.ff;
                    o2L += v * st.gl[u]; o2R += v * st.gr[u]; o2m += v;
                    ph[(size_t) u] += st.inc[u];
                    if (ph[(size_t) u] >= 1.0f) ph[(size_t) u] -= 1.0f;
                }
                o2L *= st.norm; o2R *= st.norm; o2m *= st.norm;
            }

            float o1L = 0, o1R = 0, o1m = 0;
            if (os[0].ta != nullptr)
            {
                auto& st = os[0];
                auto& ph = phase[0];
                const float fmOff = fm * o2m;
                for (int u = 0; u < st.uni; ++u)
                {
                    float pp = ph[(size_t) u] + fmOff;
                    pp -= std::floor (pp);
                    const float a = readTable (st.ta, pp);
                    const float v = a + (readTable (st.tb, pp) - a) * st.ff;
                    o1L += v * st.gl[u]; o1R += v * st.gr[u]; o1m += v;
                    ph[(size_t) u] += st.inc[u];
                    if (ph[(size_t) u] >= 1.0f) ph[(size_t) u] -= 1.0f;
                }
                o1L *= st.norm; o1R *= st.norm; o1m *= st.norm;
            }

            float extra = ring * o1m * o2m;
            if (subLevel > 0.0f)
            {
                const float sv = s.subShape == 0 ? std::sin (dsp::twoPi * subPhase) : (subPhase < 0.5f ? 0.8f : -0.8f);
                extra += sv * subLevel;
                subPhase += subInc;
                if (subPhase >= 1.0f) subPhase -= 1.0f;
            }
            if (noiseLevel > 0.0f)
            {
                noiseSeed = noiseSeed * 1664525u + 1013904223u;
                extra += ((float) (noiseSeed >> 8) / 8388608.0f - 1.0f) * noiseLevel;
            }

            float xl = o1L * os[0].level + o2L * os[1].level + extra;
            float xr = o1R * os[0].level + o2R * os[1].level + extra;
            if (s.drive > 0.0f) { xl = dsp::fastTanh (xl * driveGain) * driveComp * 1.6f; xr = dsp::fastTanh (xr * driveGain) * driveComp * 1.6f; }

            float yl, yr;
            switch (s.fType)
            {
                case 0:  yl = fL[1].process (fL[0].process (xl).lp).lp; yr = fR[1].process (fR[0].process (xr).lp).lp; break;
                case 1:  yl = fL[0].process (xl).lp; yr = fR[0].process (xr).lp; break;
                case 2:  yl = fL[0].process (xl).bp * bpK * 1.4f; yr = fR[0].process (xr).bp * bpK * 1.4f; break;
                case 3:  yl = fL[0].process (xl).hp; yr = fR[0].process (xr).hp; break;
                default: { const auto a = fL[0].process (xl); const auto b = fR[0].process (xr); yl = a.lp + a.hp; yr = b.lp + b.hp; } break;
            }

            const float amp = envA.next() * vol;
            envF.next();
            envM.next();
            outL[i] += yl * amp * panL;
            outR[i] += yr * amp * panR;
        }
    }
};

//==============================================================================
SynthEngine::SynthEngine (juce::AudioProcessorValueTreeState& s) : state (s), snap (std::make_unique<Snapshot>())
{
    auto raw = [&s] (const juce::String& id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };
    for (int o = 0; o < 2; ++o)
    {
        const juce::String p = "o" + juce::String (o + 1) + "_";
        auto& op = oscP[(size_t) o];
        op.wave = raw (p + "wave"); op.ctype = raw (p + "ctype"); op.curve = raw (p + "curve"); op.oct = raw (p + "oct");
        op.semi = raw (p + "semi"); op.fine = raw (p + "fine"); op.level = raw (p + "level"); op.uni = raw (p + "uni"); op.det = raw (p + "det");
    }
    pSub = raw ("sub"); pSubShape = raw ("sub_shape"); pNoise = raw ("noise"); pFm = raw ("fm"); pRing = raw ("ring");
    pFType = raw ("f_type"); pCut = raw ("f_cut"); pRes = raw ("f_res"); pDrive = raw ("f_drive"); pFEnv = raw ("f_env");
    pFKey = raw ("f_key"); pFVel = raw ("f_vel");
    const char* envs[] = { "ea", "ef", "em" };
    const char* stages[] = { "a", "d", "s", "r" };
    for (int e = 0; e < 3; ++e)
        for (int k = 0; k < 4; ++k)
            pEnv[(size_t) (e * 4 + k)] = raw (juce::String (envs[e]) + "_" + stages[k]);
    for (int l = 0; l < 2; ++l)
    {
        const juce::String p = "l" + juce::String (l + 1) + "_";
        pLfo[(size_t) (l * 3)] = raw (p + "shape"); pLfo[(size_t) (l * 3 + 1)] = raw (p + "rate"); pLfo[(size_t) (l * 3 + 2)] = raw (p + "sync");
    }
    for (int m = 0; m < numModSlots; ++m)
    {
        const juce::String p = "m" + juce::String (m + 1) + "_";
        pMod[(size_t) (m * 3)] = raw (p + "src"); pMod[(size_t) (m * 3 + 1)] = raw (p + "dst"); pMod[(size_t) (m * 3 + 2)] = raw (p + "amt");
    }
    pVoiceMode = raw ("voice_mode"); pGlide = raw ("glide"); pBend = raw ("bend"); pVelSens = raw ("vel_sens");
    pWidth = raw ("width"); pVol = raw ("sy_vol");

    for (int i = 0; i < maxVoices; ++i) voices.push_back (std::make_unique<Voice>());
    heldNotes.reserve (128);
    drawn[0] = DrawnWave::preset (1);
    drawn[1] = DrawnWave::preset (2);
}

SynthEngine::~SynthEngine() = default;

void SynthEngine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    reset();
}

void SynthEngine::reset()
{
    for (auto& v : voices) { v->envA.kill(); v->envF.kill(); v->envM.kill(); v->note = -1; }
    heldNotes.clear();
    sustained.fill (false);
    sustainPedal = false;
}

void SynthEngine::allNotesOff()
{
    for (auto& v : voices) v->release();
    heldNotes.clear();
    sustained.fill (false);
}

void SynthEngine::setDrawnWave (int osc, const DrawnWave& w)
{
    const juce::SpinLock::ScopedLockType l (drawnLock);
    drawn[(size_t) juce::jlimit (0, 1, osc)] = w;
}

DrawnWave SynthEngine::getDrawnWave (int osc) const
{
    const juce::SpinLock::ScopedLockType l (drawnLock);
    return drawn[(size_t) juce::jlimit (0, 1, osc)];
}

void SynthEngine::updateTables()
{
    for (int o = 0; o < 2; ++o)
    {
        const int w = juce::roundToInt (oscP[(size_t) o].wave->load());
        const int c = juce::roundToInt (oscP[(size_t) o].ctype->load());
        const auto d = getDrawnWave (o);
        const bool drawnMatters = w == waveDrawn;
        if (w != builtWave[(size_t) o] || c != builtCurve[(size_t) o] || (drawnMatters && ! (d == builtDrawn[(size_t) o])))
        {
            banks[(size_t) o].publish (buildWaveSet (w, c, d));
            builtWave[(size_t) o] = w;
            builtCurve[(size_t) o] = c;
            builtDrawn[(size_t) o] = d;
        }
        banks[(size_t) o].collectGarbage();
    }
}

void SynthEngine::readParams (Snapshot& s) const
{
    auto i = [] (std::atomic<float>* p) { return juce::roundToInt (p->load()); };
    for (int o = 0; o < 2; ++o)
    {
        const auto& op = oscP[(size_t) o];
        auto& so = s.osc[(size_t) o];
        so.wave = i (op.wave); so.ctype = i (op.ctype); so.curve = op.curve->load(); so.oct = i (op.oct); so.semi = i (op.semi);
        so.fine = op.fine->load(); so.level = op.level->load(); so.uni = i (op.uni); so.det = op.det->load();
    }
    s.sub = pSub->load(); s.subShape = i (pSubShape); s.noise = pNoise->load(); s.fm = pFm->load(); s.ring = pRing->load();
    s.fType = i (pFType); s.cut = pCut->load(); s.res = pRes->load(); s.drive = pDrive->load(); s.fEnv = pFEnv->load();
    s.fKey = pFKey->load(); s.fVel = pFVel->load();
    for (int e = 0; e < 3; ++e)
        for (int k = 0; k < 4; ++k)
            s.env[e][k] = pEnv[(size_t) (e * 4 + k)]->load();
    s.l1Shape = i (pLfo[0]); s.l2Shape = i (pLfo[3]);
    s.l1Hz = pLfo[1]->load(); s.l2Hz = pLfo[4]->load();
    for (int m = 0; m < numModSlots; ++m)
        s.mod[(size_t) m] = { i (pMod[(size_t) (m * 3)]), i (pMod[(size_t) (m * 3 + 1)]), pMod[(size_t) (m * 3 + 2)]->load() };
    s.voiceMode = i (pVoiceMode); s.glide = pGlide->load(); s.bend = i (pBend); s.velSens = pVelSens->load();
    s.width = pWidth->load(); s.vol = juce::Decibels::decibelsToGain (pVol->load(), -60.0f);
}

SynthEngine::Voice* SynthEngine::findVoiceToUse()
{
    Voice* best = nullptr;
    int playing = 0;
    for (auto& v : voices)
    {
        if (! v->isActive()) { if (best == nullptr || best->isActive()) best = v.get(); continue; }
        if (! v->released) ++playing;
    }
    if (best != nullptr && ! best->isActive() && playing < maxPolyphony) return best;

    // Robo de voz: primero la más antigua liberada, después la más antigua
    Voice* oldest = nullptr;
    for (auto& v : voices)
        if (v->released && (oldest == nullptr || v->age < oldest->age)) oldest = v.get();
    if (oldest == nullptr)
        for (auto& v : voices)
            if (oldest == nullptr || v->age < oldest->age) oldest = v.get();
    return oldest;
}

void SynthEngine::noteOn (int note, float velocity)
{
    const int mode = snap->voiceMode;
    if (mode > 0)
    {
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        heldNotes.push_back (note);
        auto& v = *voices[0];
        const bool legato = mode == 2 && v.isActive() && ! v.released && heldNotes.size() > 1;
        const float from = v.isActive() ? v.pitch : (snap->glide > 0.001f ? lastNotePitch : (float) note);
        v.start (note, velocity, from, ! legato, random);
        v.age = ++voiceCounter;
        lastNotePitch = (float) note;
        return;
    }

    // Misma nota sonando: se reutiliza su voz
    for (auto& v : voices)
        if (v->isActive() && v->note == note && ! v->released)
        {
            v->start (note, velocity, v->pitch, true, random);
            v->age = ++voiceCounter;
            return;
        }

    if (auto* v = findVoiceToUse())
    {
        const float from = snap->glide > 0.001f ? lastNotePitch : (float) note;
        v->start (note, velocity, from, true, random);
        v->age = ++voiceCounter;
    }
    lastNotePitch = (float) note;
}

void SynthEngine::noteOff (int note)
{
    if (snap->voiceMode > 0)
    {
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        auto& v = *voices[0];
        if (v.note != note) return;
        if (! heldNotes.empty())
        {
            v.targetPitch = (float) heldNotes.back();
            v.note = heldNotes.back();
            if (snap->glide <= 0.001f) v.pitch = v.targetPitch;
        }
        else if (sustainPedal) sustained[(size_t) note] = true;
        else v.release();
        return;
    }
    for (auto& v : voices)
        if (v->isActive() && v->note == note && ! v->released)
        {
            if (sustainPedal) sustained[(size_t) note] = true;
            else v->release();
        }
}

void SynthEngine::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) noteOff (m.getNoteNumber());
    else if (m.isPitchWheel()) pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isControllerOfType (1)) modWheel = (float) m.getControllerValue() / 127.0f;
    else if (m.isChannelPressure()) aftertouch = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch()) aftertouch = (float) m.getAfterTouchValue() / 127.0f;
    else if (m.isSustainPedalOn()) sustainPedal = true;
    else if (m.isSustainPedalOff())
    {
        sustainPedal = false;
        for (int n = 0; n < 128; ++n)
            if (sustained[(size_t) n])
            {
                sustained[(size_t) n] = false;
                if (std::find (heldNotes.begin(), heldNotes.end(), n) == heldNotes.end())
                    for (auto& v : voices)
                        if (v->isActive() && v->note == n) v->release();
            }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff();
}

void SynthEngine::render (float* L, float* R, int numSamples, const juce::MidiBuffer& midi, double bpm)
{
    for (int o = 0; o < 2; ++o)
        if (auto* t = banks[(size_t) o].acquire()) tables[(size_t) o] = t;

    auto& s = *snap;
    readParams (s);
    s.pitchBend = pitchBend; s.modWheel = modWheel; s.aftertouch = aftertouch;

    // LFO sincronizado al tempo
    for (int l = 0; l < 2; ++l)
    {
        const int div = juce::roundToInt (pLfo[(size_t) (l * 3 + 2)]->load());
        if (div > 0)
        {
            const double secs = divisionSeconds (div, bpm);
            (l == 0 ? s.l1Hz : s.l2Hz) = (float) (1.0 / juce::jmax (0.01, secs));
        }
    }

    float bufL[controlRate], bufR[controlRate];
    auto it = midi.begin();
    int pos = 0;
    int active = 0;
    while (pos < numSamples)
    {
        // Eventos MIDI hasta este punto
        while (it != midi.end() && (*it).samplePosition <= pos)
        {
            handleMidi ((*it).getMessage());
            ++it;
        }
        int n = juce::jmin (controlRate, numSamples - pos);
        if (it != midi.end()) n = juce::jmin (n, juce::jmax (1, (*it).samplePosition - pos));

        s.pitchBend = pitchBend; s.modWheel = modWheel; s.aftertouch = aftertouch;
        lfo1Value = lfo1.next ((double) s.l1Hz * n / sr, s.l1Shape);
        s.lfo1 = lfo1Value;

        std::fill (bufL, bufL + n, 0.0f);
        std::fill (bufR, bufR + n, 0.0f);
        active = 0;
        for (auto& v : voices)
            if (v->isActive())
            {
                v->render (bufL, bufR, n, s, tables, sr);
                ++active;
            }
        for (int i = 0; i < n; ++i)
        {
            L[pos + i] += bufL[i] * s.vol;
            R[pos + i] += bufR[i] * s.vol;
        }
        pos += n;
    }
    while (it != midi.end()) { handleMidi ((*it).getMessage()); ++it; }
    numActive.store (active);
}
} // namespace geminis::inst
