// Prueba sin interfaz de la cadena de guitarra: genera notas de prueba, las pasa
// por cada modelo y comprueba que la salida es válida. Escribe un WAV por modelo.
#include <juce_audio_formats/juce_audio_formats.h>
#include "dsp/GuitarChain.h"
#include <iostream>

using namespace geminis;

static void pluck (std::vector<float>& out, double sr, double hz, int start, float amp)
{
    // Cuerda pulsada sencilla (Karplus-Strong) para tener algo parecido a una guitarra.
    const int period = (int) (sr / hz);
    std::vector<float> line ((size_t) period);
    juce::Random rng (1234 + (int) hz);
    for (auto& v : line) v = rng.nextFloat() * 2.0f - 1.0f;
    float prev = 0.0f;
    for (int i = start, k = 0; i < (int) out.size(); ++i, k = (k + 1) % period)
    {
        const float v = line[(size_t) k];
        out[(size_t) i] += v * amp;
        line[(size_t) k] = 0.996f * 0.5f * (v + prev);
        prev = v;
    }
}

int main (int argc, char** argv)
{
    const double sr = 48000.0;
    const int block = 256, total = (int) (sr * 4.0);
    const juce::File outDir = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    std::vector<float> dry ((size_t) total, 0.0f);
    const double notes[] = { 82.41, 110.0, 146.83, 196.0, 246.94, 329.63 };
    for (int i = 0; i < 6; ++i) pluck (dry, sr, notes[i], (int) (sr * 0.25 * i), 0.18f);
    for (int i = 0; i < 3; ++i) pluck (dry, sr, notes[i * 2], (int) (sr * 2.0), 0.14f);

    struct Case { const char* name; GuitarSettings s; };
    std::vector<Case> cases;
    for (int model = 0; model < 3; ++model)
    {
        GuitarSettings s;
        s.ampModel = model; s.cabType = model == 0 ? 0 : model + 1;
        s.ampGain = model == 0 ? 0.35f : 0.6f;
        static const char* names[] = { "cristal", "britannia", "titan" };
        cases.push_back ({ names[model], s });
    }
    {
        GuitarSettings s;
        s.ampModel = 1; s.cabType = 2; s.ampGain = 0.7f;
        s.compOn = s.driveOn = s.tremOn = s.delayOn = s.reverbOn = true;
        cases.push_back ({ "todo_activo", s });
    }
    {
        GuitarSettings s; // extremos: todo al máximo
        s.ampModel = 2; s.cabType = 3;
        s.ampGain = s.ampBass = s.ampMid = s.ampTreble = s.ampPresence = s.ampMaster = 1.0f;
        s.driveOn = true; s.driveGain = s.driveLevel = s.driveTone = 1.0f;
        s.delayOn = true; s.delayFeedback = 0.9f; s.delayMix = 1.0f;
        s.reverbOn = true; s.reverbSize = 1.0f; s.reverbMix = 1.0f;
        cases.push_back ({ "extremo", s });
    }

    bool ok = true;
    for (auto& c : cases)
    {
        GuitarChain chain;
        chain.prepare (sr, block);
        juce::AudioBuffer<float> out (2, total);
        std::vector<float> mono ((size_t) block);

        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            std::copy (dry.begin() + pos, dry.begin() + pos + n, mono.begin());
            chain.process (mono.data(), out.getWritePointer (0, pos), out.getWritePointer (1, pos), n, c.s);
        }

        bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < total; ++i)
                finite = finite && std::isfinite (out.getSample (ch, i));

        const float peak = out.getMagnitude (0, total);
        const float rms = out.getRMSLevel (0, 0, total);
        const bool caseOk = finite && rms > 0.001f && peak <= 1.001f;
        ok = ok && caseOk;

        std::cout << (caseOk ? "OK   " : "FALLO ") << c.name
                  << "  pico " << juce::Decibels::gainToDecibels (peak) << " dB"
                  << "  rms " << juce::Decibels::gainToDecibels (rms) << " dB"
                  << "  latencia " << chain.getLatencySamples() << " muestras\n";

        juce::WavAudioFormat wav;
        auto file = outDir.getChildFile (juce::String ("geminis_") + c.name + ".wav");
        file.deleteFile();
        if (auto stream = file.createOutputStream())
            if (auto* writer = wav.createWriterFor (stream.release(), sr, 2, 24, {}, 0))
            {
                std::unique_ptr<juce::AudioFormatWriter> w (writer);
                out.applyGain (peak > 0.98f ? 0.98f / peak : 1.0f);
                w->writeFromAudioSampleBuffer (out, 0, total);
            }
    }

    std::cout << (ok ? "TODAS LAS PRUEBAS PASAN\n" : "HAY FALLOS\n");
    return ok ? 0 : 1;
}
