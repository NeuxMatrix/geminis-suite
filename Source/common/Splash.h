#pragma once
#include "Theme.h"
#include <array>

namespace geminis
{
enum class Icon { guitar, microphone, pads, synth, drums, keys };

namespace draw
{
    void icon (juce::Graphics&, Icon, juce::Rectangle<float> area, juce::Colour colour);
}

/** Pantalla de inicio animada: cielo estrellado, constelación de Géminis,
    emblema con halo y tres tarjetas para elegir el modo. */
class Splash : public juce::Component, private juce::Timer
{
public:
    struct Card
    {
        juce::String title, line1, line2, action;
        juce::Colour colour;
        Icon icon;
    };

    Splash (const juce::String& subtitle, std::array<Card, 3> cards);
    ~Splash() override;

    std::function<void (int)> onChoose;
    void setFooter (const juce::String& text) { footer = text; }

    /** Muestra un aviso sobre la pantalla con un botón para cerrarlo. */
    void showMessage (const juce::String& title, const juce::String& body);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void visibilityChanged() override;

private:
    void timerCallback() override;
    juce::Rectangle<float> cardBounds (int index) const;
    void paintBackground (juce::Graphics&);
    void paintEmblem (juce::Graphics&, juce::Point<float> centre, float size);
    void paintCard (juce::Graphics&, int index);

    struct Star { float x, y, size, phase, speed, layer; };
    struct Shooting { float x = -1.0f, y = 0.0f, life = 0.0f; };

    juce::String subtitle, footer;
    std::array<Card, 3> cards;
    std::array<float, 3> hover {};
    int hovered = -1;
    std::vector<Star> stars;
    Shooting shooting;
    juce::Random rng { 20251115 };
    double startTime = 0.0, t = 0.0;
    juce::Image emblemImage, emblemSmall, glowImage;

    juce::String messageTitle, messageBody;
    bool messageVisible = false;
    juce::Rectangle<float> messageButton;
};
} // namespace geminis
