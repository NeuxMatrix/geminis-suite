#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
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
        const int N = (int) history.size() / decim;
        std::vector<float> x ((size_t) N);
        for (int i = 0; i < N; ++i)
        {
            float acc = 0.0f;
            for (int k = 0; k < decim; ++k) acc += history[(size_t) ((histPos + i * decim + k) % history.size())];
            x[(size_t) i] = acc / (float) decim;
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
            if (d[(size_t) tau] < 0.12f)
            {
                while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        if (best < 2) return last = r;

        const float a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
        const float den = a - 2.0f * b + c;
        const float tau = (float) best + (std::abs (den) > 1.0e-6f ? 0.5f * (a - c) / den : 0.0f);
        r.hz = (float) dsr / tau;
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
