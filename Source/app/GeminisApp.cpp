// Géminis: el programa independiente de la suite.
//
// Al abrirlo muestra la pantalla de inicio con los cinco modos (Guitarra, Voz,
// Sintetizador, Batería e Instrumentos) y abre el módulo elegido en la misma
// ventana. Dentro lleva los dos plugins: Geminis FX para guitarra y voz, y
// Geminis Instrument para sintetizador, batería e instrumentos. El botón Inicio
// de cada módulo vuelve aquí.

#include <juce_audio_utils/juce_audio_utils.h>
#include "../common/AppHooks.h"
#include "../common/Splash.h"
#include "../common/Theme.h"
#include "../fx/FxProcessor.h"
#include "../inst/InstProcessor.h"
#include "SharedRateDevices.h"

#ifndef GEMINIS_VERSION
 #define GEMINIS_VERSION "0.0.0"
#endif

namespace geminis::app
{
namespace
{
constexpr int launcherW = 1240, launcherH = 820;
constexpr double preferredSampleRate = 44100.0;

enum Choice { guitar = 0, voice, synth, drums, instruments };

/** Textos del panel de ajustes de audio de JUCE, en español. */
const char* spanishStrings = R"es(language: Spanish
countries: es
"Audio device type:" = "Tipo de dispositivo:"
"Output:" = "Salida:"
"Device:" = "Dispositivo:"
"Input:" = "Entrada:"
"Test" = "Probar"
"Plays a test tone" = "Hace sonar un tono de prueba"
"Active output channels:" = "Canales de salida:"
"Active input channels:" = "Canales de entrada:"
"Sample rate:" = "Frecuencia de muestreo:"
"Audio buffer size:" = "Tamaño del búfer:"
"Active MIDI inputs:" = "Entradas MIDI:"
"No MIDI inputs available" = "No hay entradas MIDI"
"MIDI Output:" = "Salida MIDI:"
"Control Panel" = "Panel de control"
"Opens the device's own control panel" = "Abre el panel de control del dispositivo"
"Reset Device" = "Reiniciar dispositivo"
"Resets the audio interface - sometimes needed after changing a device's properties in its custom control panel" = "Reinicia la tarjeta de sonido. A veces hace falta después de cambiarla en su panel de control"
"Show advanced settings..." = "Más ajustes..."
"none" = "ninguno"
"(no audio output channels found)" = "(no hay canales de salida)"
"(no audio input channels found)" = "(no hay canales de entrada)"
"Error when trying to open audio device!" = "No se ha podido abrir la tarjeta de sonido"
"OK" = "Aceptar"
"Bluetooth MIDI" = "MIDI por Bluetooth"
"Scan for bluetooth MIDI devices" = "Buscar dispositivos MIDI por Bluetooth"
)es";

//==============================================================================
/** La pantalla de inicio se dibuja a 1240 x 820 y se escala al tamaño de la ventana. */
class Launcher : public juce::Component
{
public:
    Launcher()
        : splash (U ("¿Qué vas a crear hoy?"),
                  { Splash::Card { "Guitarra", U ("Amplis y 27 efectos."), U ("Toca en directo."), "Entrar", colours::guitar, Icon::guitar },
                    Splash::Card { "Voz", U ("Cadena vocal y afinador."), U ("Canta en directo."), "Entrar", colours::voice, Icon::microphone },
                    Splash::Card { "Sintetizador", U ("Ondas que se curvan."), U ("66 presets y efectos."), "Entrar", colours::violet, Icon::synth },
                    Splash::Card { U ("Batería"), U ("16 kits por estilos."), U ("Secuenciador y pads."), "Entrar", colours::rose, Icon::drums },
                    Splash::Card { "Instrumentos", U ("Piano, cuerdas, bajos..."), U ("Vientos y guitarras."), "Entrar", colours::cyan, Icon::keys } })
    {
        splash.setBounds (0, 0, launcherW, launcherH);
        addAndMakeVisible (splash);
    }

    void resized() override
    {
        splash.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) launcherW));
    }

    Splash splash;
};

//==============================================================================
/** Ajustes de audio: tarjeta de sonido, frecuencia, búfer, MIDI y silencio de la entrada. */
class AudioSettings : public juce::Component
{
public:
    AudioSettings (juce::AudioDeviceManager& dm, juce::Value& muteInput, juce::LookAndFeel& lnf)
        : selector (dm, 0, 2, 0, 2, true, false, true, false)
    {
        setLookAndFeel (&lnf);
        muteToggle.setButtonText (U ("Silenciar la entrada de audio para que no se acople"));
        muteToggle.getToggleStateValue().referTo (muteInput);
        hint.setText (U ("Si tocas con el micro y los altavoces del portátil, usa auriculares."), juce::dontSendNotification);
        hint.setColour (juce::Label::textColourId, colours::textDim);
        hint.setFont (Fonts::ui (13.0f));
        addAndMakeVisible (muteToggle);
        addAndMakeVisible (hint);
        addAndMakeVisible (selector);
        setSize (580, 660);
    }

    ~AudioSettings() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override { g.fillAll (colours::bg2); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 12);
        muteToggle.setBounds (r.removeFromTop (30));
        hint.setBounds (r.removeFromTop (22).withTrimmedLeft (4));
        r.removeFromTop (8);
        selector.setBounds (r);
    }

private:
    juce::ToggleButton muteToggle;
    juce::Label hint;
    juce::AudioDeviceSelectorComponent selector;
};

//==============================================================================
/** Aviso sobre el módulo cuando la entrada está silenciada en Guitarra o Voz. */
class MuteNotice : public juce::Component
{
public:
    static constexpr int height = 40;

    MuteNotice()
    {
        message.setText (U ("La entrada de audio está silenciada para que no se acople. Ponte auriculares y actívala."),
                         juce::dontSendNotification);
        message.setColour (juce::Label::textColourId, colours::text);
        message.setFont (Fonts::ui (14.0f));
        enable.setButtonText (U ("Activar entrada"));
        addAndMakeVisible (message);
        addAndMakeVisible (enable);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::goldDeep.withAlpha (0.28f).overlaidWith (colours::bg2.withAlpha (0.6f)));
        g.setColour (colours::gold.withAlpha (0.5f));
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 6);
        enable.setBounds (r.removeFromRight (150));
        message.setBounds (r);
    }

    juce::TextButton enable;

private:
    juce::Label message;
};

//==============================================================================
class MainComponent : public juce::Component,
                      private juce::AudioIODeviceCallback,
                      private juce::Value::Listener
{
public:
    MainComponent (juce::PropertiesFile& s, juce::LookAndFeel& l)
        : settings (s), lnf (l)
    {
        setOpaque (true);

        launcher.splash.onChoose = [this] (int index) { enter (index); };
        addAndMakeVisible (launcher);
        addChildComponent (notice);
        notice.enable.onClick = [this] { muteValue = false; };

        geminis::homeHandler() = [safe = juce::Component::SafePointer<MainComponent> (this)]
        {
            // Más tarde, porque el botón Inicio pertenece al módulo que se va a cerrar.
            juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->showLauncher(); });
        };

        muteValue = settings.getBoolValue ("silenciarEntrada", true);
        muted = (bool) muteValue.getValue();
        muteValue.addListener (this);

        startAudio();
        showLauncher();
    }

    ~MainComponent() override
    {
        geminis::homeHandler() = nullptr;
        saveState();
        midiListConnection.reset();
        deviceManager.removeMidiInputDeviceCallback ({}, &player);
        deviceManager.removeAudioCallback (this);
        closeEditor();
        player.setProcessor (nullptr);
        deviceManager.closeAudioDevice();
    }

    void paint (juce::Graphics& g) override { g.fillAll (colours::bg); }

    void resized() override
    {
        auto r = getLocalBounds();
        if (notice.isVisible())
            notice.setBounds (r.removeFromTop (MuteNotice::height));

        if (editor != nullptr)
            editor->setTopLeftPosition (r.getTopLeft());
        else
            launcher.setBounds (r);
    }

    void childBoundsChanged (juce::Component* child) override
    {
        if (child == editor.get())
            updateSize();
    }

    //==========================================================================
    void showLauncher()
    {
        saveState();
        closeEditor();
        player.setProcessor (nullptr);
        current = -1;
        launcher.setVisible (true);
        updateNotice();
        updateSize();
    }

    void enter (int choice)
    {
        closeEditor();

        juce::AudioProcessor* processor = nullptr;
        if (choice == guitar || choice == voice)
        {
            auto& fx = fxProcessor();
            fx.setMode (choice == guitar ? geminis::fx::Mode::guitar : geminis::fx::Mode::voice, true);
            processor = &fx;
        }
        else
        {
            auto& inst = instProcessor();
            inst.setMode (choice == synth ? geminis::inst::Mode::synth
                        : choice == drums ? geminis::inst::Mode::drums
                                          : geminis::inst::Mode::instruments);
            processor = &inst;
        }

        current = choice;
        player.setProcessor (processor);
        editor.reset (processor->createEditorIfNeeded());
        launcher.setVisible (false);

        if (editor != nullptr)
        {
            addAndMakeVisible (*editor);
            editor->grabKeyboardFocus();
        }

        updateNotice();
        updateSize();
    }

    void openAudioSettings()
    {
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned (new AudioSettings (deviceManager, muteValue, lnf));
        o.dialogTitle = U ("Ajustes de audio");
        o.dialogBackgroundColour = colours::bg2;
        o.componentToCentreAround = this;
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.launchAsync();
    }

    void saveState()
    {
        auto store = [this] (juce::AudioProcessor* p, const char* key)
        {
            if (p == nullptr) return;
            juce::MemoryBlock block;
            p->getStateInformation (block);
            settings.setValue (key, block.toBase64Encoding());
        };
        store (fx.get(), "estadoGuitarraVoz");
        store (inst.get(), "estadoInstrumentos");

        if (auto xml = deviceManager.createStateXml())
            settings.setValue ("audio", xml.get());

        settings.setValue ("silenciarEntrada", (bool) muteValue.getValue());
        settings.saveIfNeeded();
    }

private:
    //==========================================================================
    void startAudio()
    {
        auto saved = settings.getXmlValue ("audio");

        // Las versiones anteriores guardaron 48000 Hz porque Windows Audio no ofrecía otra.
        // Se pasa a 44100 Hz una vez; después se respeta lo que elijas.
        if (! settings.containsKey ("frecuencia44100"))
        {
            if (saved != nullptr)
                saved->setAttribute ("audioDeviceRate", preferredSampleRate);
            settings.setValue ("frecuencia44100", true);
        }

        juce::AudioDeviceManager::AudioDeviceSetup preferred;
        preferred.sampleRate = preferredSampleRate;
        deviceManager.initialise (2, 2, saved.get(), true, {}, &preferred);

        // Todas las entradas MIDI (Maschine, teclados...) activas, también las que se conecten después.
        enableNewMidiInputs();
        midiListConnection = std::make_unique<juce::MidiDeviceListConnection> (
            juce::MidiDeviceListConnection::make ([this] { enableNewMidiInputs(); }));

        deviceManager.addMidiInputDeviceCallback ({}, &player);
        deviceManager.addAudioCallback (this);
    }

    void enableNewMidiInputs()
    {
        for (const auto& device : juce::MidiInput::getAvailableDevices())
            if (! knownMidiInputs.contains (device.identifier))
            {
                knownMidiInputs.add (device.identifier);
                deviceManager.setMidiInputDeviceEnabled (device.identifier, true);
            }
    }

    template <typename Processor>
    void restore (Processor& p, const char* key)
    {
        juce::MemoryBlock block;
        if (block.fromBase64Encoding (settings.getValue (key)) && block.getSize() > 0)
            p.setStateInformation (block.getData(), (int) block.getSize());
    }

    GeminisFxProcessor& fxProcessor()
    {
        if (fx == nullptr)
        {
            fx = std::make_unique<GeminisFxProcessor>();
            restore (*fx, "estadoGuitarraVoz");
        }
        return *fx;
    }

    GeminisInstrumentProcessor& instProcessor()
    {
        if (inst == nullptr)
        {
            inst = std::make_unique<GeminisInstrumentProcessor>();
            restore (*inst, "estadoInstrumentos");
        }
        return *inst;
    }

    void closeEditor()
    {
        if (editor != nullptr)
        {
            removeChildComponent (editor.get());
            editor.reset();
        }
    }

    void updateNotice()
    {
        notice.setVisible (muted.load() && (current == guitar || current == voice));
    }

    void updateSize()
    {
        int w = 0, h = 0;
        if (editor != nullptr)
        {
            w = editor->getWidth();
            h = editor->getHeight();
        }
        else
        {
            double scale = 1.0;
            if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
                scale = juce::jlimit (0.55, 1.0, juce::jmin ((display->userArea.getWidth() - 40) / (double) launcherW,
                                                             (display->userArea.getHeight() - 80) / (double) launcherH));
            w = juce::roundToInt (launcherW * scale);
            h = juce::roundToInt (launcherH * scale);
        }

        if (notice.isVisible())
            h += MuteNotice::height;

        setSize (w, h);
        resized();
        keepWindowOnScreen();
    }

    void keepWindowOnScreen()
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (window->getBounds()))
            {
                const auto area = display->userArea;
                const auto b = window->getBounds();
                window->setTopLeftPosition (juce::jlimit (area.getX(), juce::jmax (area.getX(), area.getRight() - b.getWidth()), b.getX()),
                                            juce::jlimit (area.getY(), juce::jmax (area.getY(), area.getBottom() - b.getHeight()), b.getY()));
            }
    }

    void valueChanged (juce::Value&) override
    {
        muted = (bool) muteValue.getValue();
        settings.setValue ("silenciarEntrada", muted.load());
        updateNotice();
        updateSize();
    }

    //==========================================================================
    // La entrada se silencia aquí, antes de llegar al módulo.
    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                           float* const* outputs, int numOutputs, int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override
    {
        if (muted.load() && numInputs > 0 && numSamples <= (int) silence.size())
        {
            const int n = juce::jmin (numInputs, (int) silentInputs.size());
            player.audioDeviceIOCallbackWithContext (silentInputs.data(), n, outputs, numOutputs, numSamples, context);
            return;
        }
        player.audioDeviceIOCallbackWithContext (inputs, numInputs, outputs, numOutputs, numSamples, context);
    }

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        silence.assign ((size_t) juce::jmax (8192, device->getCurrentBufferSizeSamples() * 4), 0.0f);
        silentInputs.fill (silence.data());
        player.audioDeviceAboutToStart (device);
    }

    void audioDeviceStopped() override              { player.audioDeviceStopped(); }
    void audioDeviceError (const juce::String& e) override { player.audioDeviceError (e); }

    //==========================================================================
    juce::PropertiesFile& settings;
    juce::LookAndFeel& lnf;

    GeminisDeviceManager deviceManager;
    juce::AudioProcessorPlayer player;
    std::unique_ptr<juce::MidiDeviceListConnection> midiListConnection;
    juce::StringArray knownMidiInputs;

    std::unique_ptr<GeminisFxProcessor> fx;
    std::unique_ptr<GeminisInstrumentProcessor> inst;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    int current = -1;

    Launcher launcher;
    MuteNotice notice;
    juce::Value muteValue;
    std::atomic<bool> muted { true };
    std::vector<float> silence;
    std::array<const float*, 32> silentInputs {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

//==============================================================================
class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (juce::PropertiesFile& settings)
        : juce::DocumentWindow (U ("Géminis"), colours::bg, juce::DocumentWindow::minimiseButton | juce::DocumentWindow::closeButton),
          content (settings, lnf)
    {
        setLookAndFeel (&lnf);
        setUsingNativeTitleBar (false);
        setTitleBarHeight (32);
        setResizable (false, false);
        setContentNonOwned (&content, true);

        settingsButton.setButtonText (U ("Ajustes de audio"));
        settingsButton.setTooltip (U ("Tarjeta de sonido, frecuencia, MIDI y entrada"));
        settingsButton.onClick = [this] { content.openAudioSettings(); };
        juce::Component::addAndMakeVisible (settingsButton);

        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    ~MainWindow() override
    {
        clearContentComponent();
        setLookAndFeel (nullptr);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    void resized() override
    {
        juce::DocumentWindow::resized();
        settingsButton.setBounds (8, 4, 150, getTitleBarHeight() - 8);
    }

private:
    GeminisLookAndFeel lnf;
    MainComponent content;
    juce::TextButton settingsButton;
};
} // namespace

//==============================================================================
class GeminisApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return U ("Géminis"); }
    const juce::String getApplicationVersion() override    { return GEMINIS_VERSION; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void anotherInstanceStarted (const juce::String&) override
    {
        if (window != nullptr)
            window->toFront (true);
    }

    void initialise (const juce::String&) override
    {
        juce::LocalisedStrings::setCurrentMappings (new juce::LocalisedStrings (juce::String::fromUTF8 (spanishStrings), false));

        juce::PropertiesFile::Options options;
        options.applicationName     = "Geminis";
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #endif
        properties.setStorageParameters (options);

        window = std::make_unique<MainWindow> (*properties.getUserSettings());
    }

    void shutdown() override
    {
        window = nullptr;
        properties.saveIfNeeded();
        juce::LocalisedStrings::setCurrentMappings (nullptr);
    }

    void systemRequestedQuit() override
    {
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
            return;
        }
        quit();
    }

private:
    juce::ApplicationProperties properties;
    std::unique_ptr<MainWindow> window;
};
} // namespace geminis::app

START_JUCE_APPLICATION (geminis::app::GeminisApplication)
