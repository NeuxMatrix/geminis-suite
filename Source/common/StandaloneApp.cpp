// Aplicación independiente (Standalone) de Geminis FX y Geminis Instrument.
//
// Hace lo mismo que la que trae JUCE por defecto, con una diferencia: pide
// 44100 Hz a la tarjeta de sonido. Si el dispositivo no ofrece esa frecuencia
// (por ejemplo «Windows Audio» en modo compartido, que usa la de Windows),
// JUCE elige la más cercana que sí admita.

#include <juce_audio_utils/juce_audio_utils.h>

#if JucePlugin_Build_Standalone

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace
{
constexpr double preferredSampleRate = 44100.0;

/** Las instalaciones anteriores guardaron la frecuencia que eligió Windows.
    La primera vez que se abre esta versión se pasa a 44100 Hz; después se
    respeta lo que elija el usuario. */
void preferSampleRateOnce (juce::PropertiesFile& settings)
{
    static constexpr const char* doneKey = "geminisPreferredRate";

    if (settings.containsKey (doneKey))
        return;

    if (auto saved = settings.getXmlValue ("audioSetup"))
    {
        saved->setAttribute ("audioDeviceRate", preferredSampleRate);
        settings.setValue ("audioSetup", saved.get());
    }

    settings.setValue (doneKey, preferredSampleRate);
    settings.saveIfNeeded();
}

class GeminisStandaloneApp final : public juce::JUCEApplication
{
public:
    GeminisStandaloneApp()
    {
        // Mismas opciones que la aplicación de JUCE, para conservar los ajustes ya guardados.
        juce::PropertiesFile::Options options;
        options.applicationName     = juce::CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #endif
        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override              { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override           { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override                    { return true; }
    void anotherInstanceStarted (const juce::String&) override    {}

    void initialise (const juce::String&) override
    {
        auto* settings = appProperties.getUserSettings();
        preferSampleRateOnce (*settings);

        juce::AudioDeviceManager::AudioDeviceSetup preferred;
        preferred.sampleRate = preferredSampleRate;

        auto holder = std::make_unique<juce::StandalonePluginHolder> (settings, false, juce::String(), &preferred);

        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            headlessHolder = std::move (holder);
            return;
        }

        mainWindow = std::make_unique<juce::StandaloneFilterWindow> (
            getApplicationName(),
            juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
            std::move (holder));
        mainWindow->setVisible (true);
    }

    void shutdown() override
    {
        headlessHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (headlessHolder != nullptr)
            headlessHolder->savePluginState();

        if (mainWindow != nullptr)
            mainWindow->getPluginHolder()->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    juce::ApplicationProperties appProperties;
    std::unique_ptr<juce::StandaloneFilterWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> headlessHolder;
};
} // namespace

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new GeminisStandaloneApp(); }

#endif
