#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "DspCommon.h"
#include <vector>
#include <cmath>

namespace geminis
{
/** Afinador: el hilo de audio deja muestras en una cola y la interfaz las analiza. */
class Tuner
{
public:
    Tuner() : fifo (16384) { buffer.resize (16384); }

    void setSampleRate (double sr) { sampleRate = sr; }

    /** Hilo de audio. */
    void push (const float* data, int n)
    {
        if (! enabled.load()) return;
        int s1, n1, s2, n2;
        fifo.prepareToWrite (n, s1, n1, s2, n2);
        if (n1 > 0) std::copy (data, data + n1, buffer.data() + s1);
        if (n2 > 0) std::copy (data + n1, data + n1 + n2, buffer.data() + s2);
        fifo.finishedWrite (n1 + n2);
    }

    struct Reading { float hz = 0.0f; int midiNote = 0; float cents = 0.0f; bool valid = false; };

    /** Hilo de la interfaz: analiza lo último recibido. */
    Reading analyse()
    {
        int s1, n1, s2, n2;
        const int ready = fifo.getNumReady();
        fifo.prepareToRead (ready, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) pushHistory (buffer[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) pushHistory (buffer[(size_t) (s2 + i)]);
        fifo.finishedRead (n1 + n2);

        // Diezmado sencillo para analizar entre 30 Hz y 1,2 kHz
        const int decim = sampleRate > 60000.0 ? 8 : 4;
        const double dsr = sampleRate / decim;
        const int H = (int) history.size();
        std::vector<float> lin ((size_t) H);   // historia en orden, de la más antigua a la más reciente
        for (int i = 0; i < H; ++i) lin[(size_t) i] = history[(size_t) ((histPos + i) % H)];

        // Filtro paso bajo antes de diezmar: sin él, el desfase de medio sample entre el periodo real y el
        // entero más cercano hace que en cuerdas brillantes gane el doble del periodo (error de octava).
        dsp::Biquad lp1, lp2;
        const double cutoff = std::min (1500.0, dsr * 0.3);
        lp1.setLowpass (sampleRate, cutoff, 0.54);
        lp2.setLowpass (sampleRate, cutoff, 1.31);
        const int N = H / decim;
        std::vector<float> x ((size_t) N);
        for (int i = 0, j = 0; i < H; ++i)
        {
            const float y = lp2.process (lp1.process (lin[(size_t) i]));
            if (i % decim == decim - 1 && j < N) x[(size_t) j++] = y;
        }

        float energy = 0.0f;
        for (float v : x) energy += v * v;
        Reading r;
        if (energy / (float) N < 1.0e-7f) return last = r;

        const int tauMax = std::min (N / 2 - 1, (int) (dsr / 30.0));
        const int W = N - tauMax - 1;
        std::vector<float> d ((size_t) tauMax + 2, 1.0f);
        float running = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0.0f;
            for (int i = 0; i < W; ++i) { const float e = x[(size_t) i] - x[(size_t) (i + tau)]; s += e * e; }
            running += s;
            d[(size_t) tau] = running > 0.0f ? s * (float) tau / running : 1.0f;
        }
        int best = -1;
        const int tauMin = (int) (dsr / 1200.0);
        for (int tau = juce::jmax (2, tauMin); tau < tauMax; ++tau)
            if (d[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        if (best < 0)
        {
            // Sin un mínimo claro bajo el umbral: el mínimo global, si es razonable
            int m = juce::jmax (2, tauMin);
            for (int tau = m; tau < tauMax; ++tau)
                if (d[(size_t) tau] < d[(size_t) m]) m = tau;
            if (d[(size_t) m] < 0.35f) best = m;
        }
        if (best < 2) return last = r;

        // Afinado fino a la frecuencia de muestreo original, alrededor del periodo encontrado
        const int centre = best * decim;
        const int lo = juce::jmax (2, centre - decim - 2), hi = centre + decim + 2;
        const int Wf = H - hi - 2;
        std::vector<float> df ((size_t) (hi + 2), 0.0f);
        for (int t = lo - 1; t <= hi + 1; ++t)
        {
            float s = 0.0f;
            for (int i = 0; i < Wf; ++i) { const float e = lin[(size_t) i] - lin[(size_t) (i + t)]; s += e * e; }
            df[(size_t) t] = s;
        }
        int bt = lo;
        for (int t = lo; t <= hi; ++t)
            if (df[(size_t) t] < df[(size_t) bt]) bt = t;
        const float a = df[(size_t) bt - 1], b = df[(size_t) bt], c = df[(size_t) bt + 1];
        const float den = a - 2.0f * b + c;
        const float period = (float) bt + (std::abs (den) > 1.0e-12f ? 0.5f * (a - c) / den : 0.0f);
        r.hz = (float) sampleRate / period;
        const float note = 69.0f + 12.0f * std::log2 (r.hz / 440.0f);
        r.midiNote = (int) std::round (note);
        r.cents = (note - (float) r.midiNote) * 100.0f;
        r.valid = true;
        return last = r;
    }

    static juce::String noteName (int midi)
    {
        static const char* names[] = { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
        return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
    }

    std::atomic<bool> enabled { false };
    /** Silencia la salida mientras el afinador está abierto, como un pedal afinador. */
    std::atomic<bool> muteOutput { true };

private:
    void pushHistory (float v)
    {
        history[(size_t) histPos] = v;
        histPos = (histPos + 1) % (int) history.size();
    }

    juce::AbstractFifo fifo;
    std::vector<float> buffer;
    std::vector<float> history = std::vector<float> (8192, 0.0f);
    int histPos = 0;
    double sampleRate = 44100.0;
    Reading last;
};
} // namespace geminis
