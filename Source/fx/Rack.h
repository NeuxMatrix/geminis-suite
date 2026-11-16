#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Modules.h"

namespace geminis::fx
{
/** Rack de módulos: 10 ranuras en serie. Cada ranura tiene tipo, interruptor y
    8 controles genéricos que el DAW puede automatizar. Los módulos se crean en
    el hilo de la interfaz y se entregan al hilo de audio bajo un cerrojo breve. */
class Rack : private juce::AsyncUpdater,
             private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr int numSlots = 10;
    static constexpr int paramsPerSlot = 8;

    static juce::String typeId (int slot)            { return "s" + juce::String (slot) + "_type"; }
    static juce::String onId (int slot)              { return "s" + juce::String (slot) + "_on"; }
    static juce::String paramId (int slot, int p)    { return "s" + juce::String (slot) + "_p" + juce::String (p); }

    /** Añade los parámetros del rack. `typeOf` devuelve el tipo actual de una ranura
        para mostrar los valores con su unidad real en el DAW. */
    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout&, std::function<int (int)> typeOf);

    explicit Rack (juce::AudioProcessorValueTreeState&);
    ~Rack() override;

    void prepare (double sampleRate, int maxBlock);
    void process (float* L, float* R, int n, Context& ctx);

    /** Crea los módulos cuyo tipo ha cambiado. Hilo de la interfaz. */
    void syncModules();

    int getLatency() const noexcept { return latency.load(); }
    int getType (int slot) const;
    bool isOn (int slot) const;
    int countUsed() const;

    void setUserIr (const juce::File&);
    juce::File getUserIr() const { return userIr; }

    // Edición del rack (hilo de la interfaz). Notifican al DAW.
    void setSlot (int slot, int type, bool on, const std::vector<float>& realValues = {});
    void swapSlots (int a, int b);
    void removeSlot (int slot);
    int addModule (int type);   // devuelve la ranura usada o -1 si el rack está lleno
    void clearAll();

    std::array<std::atomic<int>, numSlots> currentTypes {};
    std::function<void()> onLatencyChanged, onStructureChanged;

private:
    struct Slot
    {
        juce::SpinLock lock;
        std::unique_ptr<Module> module;
        int type = 0;
        std::atomic<float>* typeParam = nullptr;
        std::atomic<float>* onParam = nullptr;
        std::array<std::atomic<float>*, paramsPerSlot> p {};
        dsp::Ramp wet;
        // Retardo del directo para módulos con latencia (la afinación): así, apagado o
        // encendido, el sonido sale siempre con el mismo retraso que se comunica al DAW.
        std::vector<float> compL, compR;
        int compPos = 0;
    };

    struct RawSlot { float type, on; std::array<float, paramsPerSlot> p; };
    RawSlot readRaw (int slot) const;
    void writeRaw (int slot, const RawSlot&);

    void handleAsyncUpdate() override;
    void parameterChanged (const juce::String&, float) override;
    void updateLatency();

    juce::AudioProcessorValueTreeState& state;
    std::array<Slot, numSlots> slots;
    double sampleRate = 0.0;
    int maxBlockSize = 512;
    std::vector<float> dryL, dryR;
    std::atomic<int> latency { 0 };
    juce::File userIr;
};
} // namespace geminis::fx
