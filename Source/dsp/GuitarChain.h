#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Biquad.h"

namespace geminis
{
/** Ajustes de la cadena de guitarra para un bloque de audio. */
struct GuitarSettings
{
    bool gateOn = true;   float gateThreshDb = -60.0f;
    bool compOn = false;  float compThreshDb = -20.0f, compRatio = 3.0f, compMakeupDb = 4.0f;
    bool driveOn = false; float driveGain = 0.4f, driveTone = 0.5f, driveLevel = 0.5f;
    bool ampOn = true;    int ampModel = 0;
    float ampGain = 0.5f, ampBass = 0.5f, ampMid = 0.5f, ampTreble = 0.5f, ampPresence = 0.5f, ampMaster = 0.5f;
    bool cabOn = true;    int cabType = 0; float cabMic = 0.5f;
    bool tremOn = false;  float tremRateHz = 4.0f, tremDepth = 0.5f;
    bool delayOn = false; float delayMs = 380.0f, delayFeedback = 0.35f, delayMix = 0.25f;
    bool reverbOn = false; float reverbSize = 0.4f, reverbDamp = 0.5f, reverbMix = 0.2f;
};

/** Carácter de cada modelo de amplificador. Son diseños propios, no réplicas. */
struct AmpVoicing
{
    float tightHz;                          // recorte de graves antes de saturar
    float preMidHz, preMidDb, preMidQ;      // realce previo a la saturación
    float gainMinDb, gainMaxDb;             // rango del control de ganancia
    float bias1, bias2;                     // asimetría de cada etapa
    float interHpHz;                        // filtro entre etapas
    float stage2Db;                         // ganancia añadida en la segunda etapa
    bool  thirdStage; float stage3Db;       // tercera etapa para alta ganancia
    float fizzLpHz;                         // suavizado de agudos tras saturar
    float midHz;                            // frecuencia del control de medios
    float powerDb;                          // empuje de la etapa de potencia
    float trimDb;                           // compensación de volumen
};

inline const AmpVoicing& ampVoicing (int model) noexcept
{
    static const AmpVoicing voicings[] = {
        // Cristal: limpio americano
        {  60.0f, 1500.0f, 2.0f, 0.5f,  -6.0f, 18.0f, 0.10f, 0.05f,  30.0f,  0.0f, false, 0.0f, 9000.0f, 500.0f, 0.0f,  10.0f },
        // Britannia: stack británico
        { 110.0f,  900.0f, 5.0f, 0.7f,   6.0f, 36.0f, 0.25f, 0.15f, 120.0f,  8.0f, false, 0.0f, 7000.0f, 750.0f, 4.0f,  -8.0f },
        // Titán: alta ganancia moderna
        { 160.0f, 1400.0f, 4.0f, 0.7f,  14.0f, 46.0f, 0.20f, 0.10f, 180.0f, 12.0f, true,  8.0f, 6200.0f, 650.0f, 3.0f, -10.0f },
    };
    return voicings[juce::jlimit (0, 2, model)];
}

/** Carácter de cada pantalla, hecho con filtros (sin latencia). */
struct CabVoicing
{
    float hpHz, lowHz, lowDb, midHz, midDb, midQ, presHz, presDb, notchHz, notchDb, lpHz;
};

inline const CabVoicing& cabVoicing (int type) noexcept
{
    static const CabVoicing voicings[] = {
        // 1x12 abierta
        {  95.0f, 140.0f, 2.0f, 450.0f, -2.0f, 1.0f, 2800.0f, 4.0f, 4200.0f, -5.0f, 5600.0f },
        // 2x12 vintage
        {  85.0f, 120.0f, 3.0f, 400.0f, -3.0f, 1.0f, 2400.0f, 3.5f, 3900.0f, -6.0f, 5200.0f },
        // 4x12 Britannia
        {  75.0f, 110.0f, 4.0f, 500.0f, -2.5f, 0.9f, 2600.0f, 5.0f, 4400.0f, -7.0f, 5000.0f },
        // 4x12 Titán
        {  70.0f, 100.0f, 5.0f, 380.0f, -5.0f, 0.8f, 3000.0f, 5.5f, 4700.0f, -6.0f, 5400.0f },
    };
    return voicings[juce::jlimit (0, 3, type)];
}

static constexpr int kUserIrCabType = 4;

//==============================================================================
/** Cadena completa del modo Guitarra. Procesa en mono y entrega estéreo. */
class GuitarChain
{
public:
    void prepare (double sampleRate, int maxBlockSize)
    {
        sr = sampleRate;
        juce::dsp::ProcessSpec mono { sampleRate, (juce::uint32) maxBlockSize, 1 };

        gate.prepare (mono);
        gate.setRatio (10.0f);
        gate.setAttack (1.0f);
        gate.setRelease (120.0f);

        comp.prepare (mono);
        comp.setAttack (8.0f);
        comp.setRelease (150.0f);

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampler->initProcessing ((size_t) maxBlockSize);
        osRate = sampleRate * 4.0;

        convolution.prepare (mono);

        delayLine.prepare (mono);
        delayLine.setMaximumDelayInSamples ((int) (sampleRate * 2.0) + 8);
        delayTime.reset (sampleRate, 0.15);

        reverb.setSampleRate (sampleRate);

        driveGainS.reset (osRate, 0.03);
        driveLevelS.reset (osRate, 0.03);
        ampGainS.reset (osRate, 0.03);
        ampMasterS.reset (osRate, 0.03);
        for (auto* s : { &driveGainS, &driveLevelS, &ampGainS, &ampMasterS })
            s->setCurrentAndTargetValue (0.0f);

        reset();
    }

    void reset()
    {
        gate.reset();
        comp.reset();
        if (oversampler != nullptr) oversampler->reset();
        convolution.reset();
        delayLine.reset();
        reverb.reset();
        for (auto* f : { &drvHp, &drvMid, &drvTone, &tight, &preMid, &interHp, &interHp2, &fizz, &fizz2,
                         &bass, &mid, &treble, &presence, &dcBlock,
                         &cabHp, &cabLow, &cabMid, &cabPres, &cabNotch, &cabLp1, &cabLp2, &delayLp })
            f->reset();
        tremPhase = 0.0;
    }

    /** Latencia que introduce el sobremuestreo, en muestras. */
    int getLatencySamples() const
    {
        return oversampler != nullptr ? (int) std::lround (oversampler->getLatencyInSamples()) : 0;
    }

    /** Carga una respuesta a impulso del usuario. Se llama desde el hilo de la interfaz. */
    void loadUserIr (const juce::File& file)
    {
        convolution.loadImpulseResponse (file, juce::dsp::Convolution::Stereo::no,
                                         juce::dsp::Convolution::Trim::yes, 0,
                                         juce::dsp::Convolution::Normalise::yes);
    }

    /** Procesa `mono` en el sitio y deja el resultado estéreo en left y right. */
    void process (float* mono, float* left, float* right, int numSamples, const GuitarSettings& s)
    {
        updateCoefficients (s);

        // Puerta de ruido y compresor
        if (s.gateOn)
        {
            gate.setThreshold (s.gateThreshDb);
            for (int i = 0; i < numSamples; ++i)
                mono[i] = gate.processSample (0, mono[i]);
        }

        if (s.compOn)
        {
            comp.setThreshold (s.compThreshDb);
            comp.setRatio (s.compRatio);
            const float makeup = dbToGain (s.compMakeupDb);
            for (int i = 0; i < numSamples; ++i)
                mono[i] = comp.processSample (0, mono[i]) * makeup;
        }

        // Pedal y amplificador, a cuatro veces la frecuencia de muestreo para evitar aliasing
        {
            float* chans[] = { mono };
            juce::dsp::AudioBlock<float> block (chans, 1, (size_t) numSamples);
            auto up = oversampler->processSamplesUp (block);
            float* d = up.getChannelPointer (0);
            const int n = (int) up.getNumSamples();

            if (s.driveOn) processDrive (d, n);
            if (s.ampOn)   processAmp (d, n, s);

            oversampler->processSamplesDown (block);
        }

        // Pantalla
        if (s.cabOn)
        {
            if (s.cabType == kUserIrCabType && convolution.getCurrentIRSize() > 0)
            {
                float* chans[] = { mono };
                juce::dsp::AudioBlock<float> block (chans, 1, (size_t) numSamples);
                juce::dsp::ProcessContextReplacing<float> ctx (block);
                convolution.process (ctx);
            }
            else
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    float x = cabHp.process (mono[i]);
                    x = cabLow.process (x);
                    x = cabMid.process (x);
                    x = cabPres.process (x);
                    x = cabNotch.process (x);
                    x = cabLp1.process (x);
                    mono[i] = cabLp2.process (x);
                }
            }
        }

        // Trémolo
        if (s.tremOn)
        {
            const double inc = juce::MathConstants<double>::twoPi * s.tremRateHz / sr;
            for (int i = 0; i < numSamples; ++i)
            {
                const float lfo = 0.5f + 0.5f * (float) std::sin (tremPhase);
                mono[i] *= 1.0f - s.tremDepth * lfo;
                tremPhase += inc;
                if (tremPhase > juce::MathConstants<double>::twoPi)
                    tremPhase -= juce::MathConstants<double>::twoPi;
            }
        }

        // Delay
        if (s.delayOn)
        {
            delayTime.setTargetValue (s.delayMs * 0.001f * (float) sr);
            for (int i = 0; i < numSamples; ++i)
            {
                const float wet = delayLine.popSample (0, delayTime.getNextValue());
                delayLine.pushSample (0, mono[i] + softLimit (delayLp.process (wet) * s.delayFeedback));
                mono[i] += wet * s.delayMix;
            }
        }
        else
        {
            delayTime.setCurrentAndTargetValue (s.delayMs * 0.001f * (float) sr);
        }

        // Salida estéreo y reverb
        std::copy (mono, mono + numSamples, left);
        std::copy (mono, mono + numSamples, right);

        if (s.reverbOn)
        {
            juce::Reverb::Parameters p;
            p.roomSize = 0.25f + 0.7f * s.reverbSize;
            p.damping = s.reverbDamp;
            p.wetLevel = s.reverbMix * 0.5f;
            p.dryLevel = 0.5f * (1.0f - 0.4f * s.reverbMix);
            p.width = 1.0f;
            reverb.setParameters (p);
            reverb.processStereo (left, right, numSamples);
        }

        // Limitador suave de seguridad: por mucho que se suban los controles, no satura el DAW.
        for (int i = 0; i < numSamples; ++i)
        {
            left[i] = softLimit (left[i]);
            right[i] = softLimit (right[i]);
        }
    }

private:
    static inline float softLimit (float x) noexcept
    {
        const float a = std::abs (x);
        if (a <= 0.7f)
            return x;
        return std::copysign (0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f), x);
    }

    static inline float shape (float x, float bias) noexcept
    {
        return std::tanh (x + bias) - std::tanh (bias);
    }

    void updateCoefficients (const GuitarSettings& s)
    {
        // Pedal
        drvHp.setHighpass (osRate, 250.0);
        drvMid.setPeak (osRate, 800.0, 0.7, 5.0);
        drvTone.setLowpass (osRate, 1000.0 + 7000.0 * s.driveTone * s.driveTone);
        driveGainS.setTargetValue (dbToGain (6.0f + 36.0f * s.driveGain));
        driveLevelS.setTargetValue (dbToGain (-24.0f + 30.0f * s.driveLevel));

        // Amplificador
        const auto& v = ampVoicing (s.ampModel);
        tight.setHighpass (osRate, v.tightHz);
        preMid.setPeak (osRate, v.preMidHz, v.preMidQ, v.preMidDb);
        interHp.setHighpass (osRate, v.interHpHz);
        interHp2.setHighpass (osRate, v.interHpHz * 0.6);
        fizz.setLowpass (osRate, v.fizzLpHz);
        fizz2.setLowpass (osRate, v.fizzLpHz * 1.3);
        bass.setLowShelf (osRate, 120.0, (s.ampBass - 0.5f) * 24.0f);
        mid.setPeak (osRate, v.midHz, 0.7, (s.ampMid - 0.5f) * 20.0f);
        treble.setHighShelf (osRate, 3000.0, (s.ampTreble - 0.5f) * 24.0f);
        presence.setPeak (osRate, 4200.0, 0.8, (s.ampPresence - 0.5f) * 16.0f);
        dcBlock.setHighpass (osRate, 20.0);
        ampGainS.setTargetValue (dbToGain (v.gainMinDb + (v.gainMaxDb - v.gainMinDb) * s.ampGain));
        ampMasterS.setTargetValue (s.ampMaster * s.ampMaster * 4.0f * dbToGain (v.trimDb));
        stage2Gain = dbToGain (v.stage2Db * (0.4f + 0.6f * s.ampGain));
        stage3Gain = dbToGain (v.stage3Db * (0.4f + 0.6f * s.ampGain));
        powerGain = dbToGain (v.powerDb);

        // Pantalla: el control de micro va del centro del cono (brillante) al borde (oscuro)
        const auto& c = cabVoicing (s.cabType);
        const float bright = 1.0f - s.cabMic;
        cabHp.setHighpass (sr, c.hpHz, 0.9);
        cabLow.setPeak (sr, c.lowHz, 1.2, c.lowDb);
        cabMid.setPeak (sr, c.midHz, c.midQ, c.midDb);
        cabPres.setPeak (sr, c.presHz, 1.4, c.presDb * (0.3f + 1.2f * bright));
        cabNotch.setPeak (sr, c.notchHz, 3.0, c.notchDb);
        const double lp = c.lpHz * (0.7 + 0.6 * bright);
        cabLp1.setLowpass (sr, lp, 0.8);
        cabLp2.setLowpass (sr, lp * 1.1, 0.6);

        delayLp.setLowpass (sr, 3800.0);
    }

    void processDrive (float* d, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float x = d[i];
            const float hp = drvHp.process (x);
            const float lows = x - hp;
            const float clipped = std::tanh (drvMid.process (hp) * driveGainS.getNextValue());
            d[i] = (drvTone.process (clipped) * 0.6f + lows) * driveLevelS.getNextValue() * 4.0f;
        }
    }

    void processAmp (float* d, int n, const GuitarSettings& s) noexcept
    {
        const auto& v = ampVoicing (s.ampModel);

        for (int i = 0; i < n; ++i)
        {
            float x = preMid.process (tight.process (d[i]));

            x = shape (x * ampGainS.getNextValue(), v.bias1);
            x = interHp.process (x);
            x = shape (x * stage2Gain, v.bias2);

            if (v.thirdStage)
            {
                x = interHp2.process (x);
                x = shape (x * stage3Gain, v.bias1 * 0.5f);
            }

            x = fizz2.process (fizz.process (x)) * 0.5f;
            x = treble.process (mid.process (bass.process (x)));
            x = std::tanh (x * powerGain);
            x = dcBlock.process (presence.process (x));
            d[i] = x * ampMasterS.getNextValue();
        }
    }

    double sr = 44100.0, osRate = 176400.0;

    juce::dsp::NoiseGate<float> gate;
    juce::dsp::Compressor<float> comp;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    Biquad drvHp, drvMid, drvTone;
    Biquad tight, preMid, interHp, interHp2, fizz, fizz2, bass, mid, treble, presence, dcBlock;
    juce::SmoothedValue<float> driveGainS, driveLevelS, ampGainS, ampMasterS;
    float stage2Gain = 1.0f, stage3Gain = 1.0f, powerGain = 1.0f;

    Biquad cabHp, cabLow, cabMid, cabPres, cabNotch, cabLp1, cabLp2;
    juce::dsp::Convolution convolution;

    double tremPhase = 0.0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayLine { 96000 };
    juce::SmoothedValue<float> delayTime;
    Biquad delayLp;

    juce::Reverb reverb;
};
} // namespace geminis
