#pragma once
#include <juce_audio_devices/juce_audio_devices.h>

namespace geminis::app
{
/** «Windows Audio» (WASAPI compartido) solo anuncia la frecuencia que tenga
    Windows, normalmente 48000 Hz, aunque JUCE lo abre con conversión de
    frecuencia y funciona a cualquier otra. Este envoltorio anuncia también
    44100, 48000, 88200 y 96000 Hz; Windows convierte al abrir el dispositivo. */
class SharedRateDevice final : public juce::AudioIODevice
{
public:
    explicit SharedRateDevice (std::unique_ptr<juce::AudioIODevice> d)
        : juce::AudioIODevice (d->getName(), d->getTypeName()), inner (std::move (d)) {}

    juce::AudioIODevice& getInner() noexcept { return *inner; }

    juce::Array<double> getAvailableSampleRates() override
    {
        auto rates = inner->getAvailableSampleRates();
        if (rates.isEmpty())
            return rates;   // sin frecuencias el dispositivo no se puede abrir: no se inventan

        for (double r : { 44100.0, 48000.0, 88200.0, 96000.0 })
            if (! rates.contains (r))
                rates.addUsingDefaultSort (r);
        return rates;
    }

    juce::StringArray getOutputChannelNames() override                    { return inner->getOutputChannelNames(); }
    juce::StringArray getInputChannelNames() override                     { return inner->getInputChannelNames(); }
    std::optional<juce::BigInteger> getDefaultOutputChannels() const override { return inner->getDefaultOutputChannels(); }
    std::optional<juce::BigInteger> getDefaultInputChannels() const override  { return inner->getDefaultInputChannels(); }
    juce::Array<int> getAvailableBufferSizes() override                   { return inner->getAvailableBufferSizes(); }
    int getDefaultBufferSize() override                                   { return inner->getDefaultBufferSize(); }

    juce::String open (const juce::BigInteger& ins, const juce::BigInteger& outs, double rate, int bufferSize) override
    {
        return inner->open (ins, outs, rate, bufferSize);
    }

    void close() override                                     { inner->close(); }
    bool isOpen() override                                    { return inner->isOpen(); }
    void start (juce::AudioIODeviceCallback* cb) override     { inner->start (cb); }
    void stop() override                                      { inner->stop(); }
    bool isPlaying() override                                 { return inner->isPlaying(); }
    juce::String getLastError() override                      { return inner->getLastError(); }
    int getCurrentBufferSizeSamples() override                { return inner->getCurrentBufferSizeSamples(); }
    double getCurrentSampleRate() override                    { return inner->getCurrentSampleRate(); }
    int getCurrentBitDepth() override                         { return inner->getCurrentBitDepth(); }
    juce::BigInteger getActiveOutputChannels() const override { return inner->getActiveOutputChannels(); }
    juce::BigInteger getActiveInputChannels() const override  { return inner->getActiveInputChannels(); }
    int getOutputLatencyInSamples() override                  { return inner->getOutputLatencyInSamples(); }
    int getInputLatencyInSamples() override                   { return inner->getInputLatencyInSamples(); }
    juce::AudioWorkgroup getWorkgroup() const override        { return inner->getWorkgroup(); }
    bool hasControlPanel() const override                     { return inner->hasControlPanel(); }
    bool showControlPanel() override                          { return inner->showControlPanel(); }
    bool setAudioPreprocessingEnabled (bool on) override      { return inner->setAudioPreprocessingEnabled (on); }
    int getXRunCount() const noexcept override                { return inner->getXRunCount(); }

private:
    std::unique_ptr<juce::AudioIODevice> inner;
};

/** Tipo de dispositivo que envuelve a otro y crea SharedRateDevice. Conserva el nombre
    («Windows Audio»), así que los ajustes guardados siguen valiendo. */
class SharedRateDeviceType final : public juce::AudioIODeviceType,
                                   private juce::AudioIODeviceType::Listener
{
public:
    explicit SharedRateDeviceType (std::unique_ptr<juce::AudioIODeviceType> t)
        : juce::AudioIODeviceType (t->getTypeName()), inner (std::move (t))
    {
        inner->addListener (this);
    }

    ~SharedRateDeviceType() override { inner->removeListener (this); }

    void scanForDevices() override                                   { inner->scanForDevices(); }
    juce::StringArray getDeviceNames (bool wantInputNames) const override { return inner->getDeviceNames (wantInputNames); }
    int getDefaultDeviceIndex (bool forInput) const override         { return inner->getDefaultDeviceIndex (forInput); }
    bool hasSeparateInputsAndOutputs() const override                { return inner->hasSeparateInputsAndOutputs(); }

    int getIndexOfDevice (juce::AudioIODevice* device, bool asInput) const override
    {
        if (auto* wrapped = dynamic_cast<SharedRateDevice*> (device))
            device = &wrapped->getInner();
        return inner->getIndexOfDevice (device, asInput);
    }

    juce::AudioIODevice* createDevice (const juce::String& outputName, const juce::String& inputName) override
    {
        if (auto* d = inner->createDevice (outputName, inputName))
            return new SharedRateDevice (std::unique_ptr<juce::AudioIODevice> (d));
        return nullptr;
    }

private:
    void audioDeviceListChanged() override { callDeviceChangeListeners(); }

    std::unique_ptr<juce::AudioIODeviceType> inner;
};

/** Gestor de audio de Géminis: igual que el de JUCE, pero con «Windows Audio»
    ofreciendo 44100 Hz además de la frecuencia de Windows. */
class GeminisDeviceManager final : public juce::AudioDeviceManager
{
public:
    static constexpr const char* sharedWindowsType = "Windows Audio";

    void createAudioDeviceTypes (juce::OwnedArray<juce::AudioIODeviceType>& types) override
    {
        juce::AudioDeviceManager::createAudioDeviceTypes (types);

        for (int i = 0; i < types.size(); ++i)
            if (types[i]->getTypeName() == sharedWindowsType)
            {
                std::unique_ptr<juce::AudioIODeviceType> original (types.removeAndReturn (i));
                types.insert (i, new SharedRateDeviceType (std::move (original)));
            }
    }
};
} // namespace geminis::app
