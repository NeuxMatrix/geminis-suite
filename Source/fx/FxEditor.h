#pragma once
#include "FxProcessor.h"
#include "../common/Theme.h"
#include "../common/Splash.h"
#include "../common/Widgets.h"

namespace geminis::fx
{
/** Un módulo del rack dibujado como una unidad con sus controles. */
class SlotPanel : public juce::Component
{
public:
    SlotPanel (GeminisFxProcessor&, int slot, int position, juce::Colour modeAccent);
    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (int slot)> onMoveUp, onMoveDown, onRemove;

private:
    GeminisFxProcessor& processor;
    int slot, type, position;
    juce::Colour colour;
    juce::ToggleButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    juce::TextButton up { U ("▲") }, down { U ("▼") }, remove { U ("✕") }, irButton;
    juce::OwnedArray<juce::Component> controls;
    juce::OwnedArray<juce::Label> labels;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
    juce::OwnedArray<NormalisedChoiceAttachment> choiceAttachments;
    std::vector<bool> isChoice;
    std::unique_ptr<juce::FileChooser> chooser;
};

/** Lista de módulos disponibles para el modo actual. */
class ModuleBrowser : public juce::Component
{
public:
    explicit ModuleBrowser (Mode mode);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    int getIdealHeight() const;

    std::function<void (int type)> onAdd;

private:
    struct Row { bool header; juce::String text; int type; juce::Colour colour; };
    std::vector<Row> rows;
    int hovered = -1;
    static constexpr int rowH = 26, headerH = 30;
    int rowAt (int y) const;
};

class TunerDisplay : public juce::Component
{
public:
    void setReading (const Tuner::Reading& r) { reading = r; repaint(); }
    void paint (juce::Graphics&) override;
private:
    Tuner::Reading reading;
};
} // namespace geminis::fx

class GeminisFxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GeminisFxEditor (GeminisFxProcessor&);
    ~GeminisFxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showSplash (bool);
    void enterMode (geminis::fx::Mode);
    void rebuildRack();
    void rebuildBrowser();
    void showPresetMenu();
    void stepPreset (int delta);
    void updatePresetLabel();
    juce::Colour accent() const;

    GeminisFxProcessor& processor;
    geminis::GeminisLookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    juce::Image emblemSmall;
    juce::TextButton homeButton { "Inicio" }, presetButton, prevButton { "<" }, nextButton { ">" }, tunerButton { "Afinador" };
    juce::ComboBox inputBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> inputAttachment;
    geminis::ParamKnob inKnob, outKnob;
    geminis::LevelMeter inMeter { "IN" }, outMeter { "OUT" };
    geminis::fx::TunerDisplay tunerDisplay;

    juce::Viewport browserView, rackView;
    std::unique_ptr<geminis::fx::ModuleBrowser> browser;
    juce::Component rackContent;
    juce::OwnedArray<geminis::fx::SlotPanel> panels;
    juce::Label emptyHint;

    geminis::Splash splash;
    std::unique_ptr<juce::FileChooser> fileChooser;
    int tunerCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeminisFxEditor)
};
