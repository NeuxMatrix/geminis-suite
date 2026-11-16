#include "Splash.h"

#ifndef GEMINIS_VERSION
 #define GEMINIS_VERSION "0.0.0"
#endif

namespace geminis
{
//==============================================================================
namespace draw
{
void icon (juce::Graphics& g, Icon type, juce::Rectangle<float> area, juce::Colour colour)
{
    // Los iconos se dibujan en una caja de 100x100 y se escalan al área pedida.
    const float s = juce::jmin (area.getWidth(), area.getHeight()) / 100.0f;
    const auto tr = juce::AffineTransform::scale (s).translated (area.getCentreX() - 50.0f * s, area.getCentreY() - 50.0f * s);
    const auto dark = colours::bg.withAlpha (0.85f);
    auto strokeOf = [s] (float w) { return juce::PathStrokeType (w * s, juce::PathStrokeType::curved, juce::PathStrokeType::rounded); };

    switch (type)
    {
        case Icon::guitar:
        {
            juce::Path body;
            body.addRoundedRectangle (45.0f, 2.0f, 10.0f, 15.0f, 3.0f);
            body.addRectangle (47.5f, 14.0f, 5.0f, 44.0f);
            body.addEllipse (35.0f, 48.0f, 30.0f, 24.0f);
            body.addEllipse (30.0f, 62.0f, 40.0f, 34.0f);
            const auto rot = juce::AffineTransform::rotation (0.62f, 50.0f, 50.0f).followedBy (tr);
            g.setColour (colour);
            g.fillPath (body, rot);
            juce::Path detail;
            detail.addRoundedRectangle (40.0f, 66.0f, 20.0f, 4.0f, 1.5f);
            detail.addRoundedRectangle (40.0f, 74.0f, 20.0f, 4.0f, 1.5f);
            detail.addRoundedRectangle (42.0f, 84.0f, 16.0f, 3.5f, 1.5f);
            g.setColour (dark);
            g.fillPath (detail, rot);
            juce::Path strings;
            for (float x : { 48.6f, 50.0f, 51.4f })
            {
                strings.startNewSubPath (x, 6.0f);
                strings.lineTo (x, 86.0f);
            }
            g.setColour (dark.withAlpha (0.55f));
            g.strokePath (strings, juce::PathStrokeType (0.6f * s), rot);
            break;
        }
        case Icon::microphone:
        {
            juce::Path capsule;
            capsule.addRoundedRectangle (37.0f, 8.0f, 26.0f, 44.0f, 13.0f);
            g.setColour (colour);
            g.fillPath (capsule, tr);
            juce::Path grille;
            for (float y : { 18.0f, 25.0f, 32.0f, 39.0f })
            {
                grille.startNewSubPath (41.0f, y);
                grille.lineTo (59.0f, y);
            }
            g.setColour (dark.withAlpha (0.6f));
            g.strokePath (grille, strokeOf (1.6f), tr);
            juce::Path holder;
            holder.startNewSubPath (29.0f, 36.0f);
            holder.cubicTo (29.0f, 60.0f, 71.0f, 60.0f, 71.0f, 36.0f);
            holder.startNewSubPath (50.0f, 59.0f);
            holder.lineTo (50.0f, 82.0f);
            holder.startNewSubPath (35.0f, 86.0f);
            holder.lineTo (65.0f, 86.0f);
            g.setColour (colour);
            g.strokePath (holder, strokeOf (4.5f), tr);
            break;
        }
        case Icon::pads:
        {
            const float cell = 15.0f, gap = 5.0f, x0 = 50.0f - (4 * cell + 3 * gap) * 0.5f;
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 4; ++c)
                {
                    juce::Path pad;
                    pad.addRoundedRectangle (x0 + (float) c * (cell + gap), x0 + (float) r * (cell + gap), cell, cell, 3.0f);
                    const bool lit = (r + c) % 3 == 0 || (r == 1 && c == 2);
                    g.setColour (colour.withAlpha (lit ? 1.0f : 0.32f));
                    g.fillPath (pad, tr);
                }
            break;
        }
        case Icon::synth:
        {
            juce::Path wave;
            wave.startNewSubPath (8.0f, 50.0f);
            for (int i = 0; i <= 84; ++i)
            {
                const float x = 8.0f + (float) i;
                const float ph = (float) i / 84.0f * 2.0f;
                const float morph = (float) i / 84.0f;
                const float sine = std::sin (ph * 6.2831853f);
                const float saw = 2.0f * (ph - std::floor (ph)) - 1.0f;
                wave.lineTo (x, 50.0f - 30.0f * (sine * (1.0f - morph) + saw * morph));
            }
            g.setColour (colour.withAlpha (0.25f));
            g.strokePath (wave, strokeOf (10.0f), tr);
            g.setColour (colour);
            g.strokePath (wave, strokeOf (4.5f), tr);
            break;
        }
        case Icon::drums:
        {
            juce::Path drum;
            drum.addRoundedRectangle (18.0f, 46.0f, 64.0f, 34.0f, 6.0f);
            g.setColour (colour.withAlpha (0.85f));
            g.fillPath (drum, tr);
            juce::Path top;
            top.addEllipse (18.0f, 38.0f, 64.0f, 18.0f);
            g.setColour (colour);
            g.fillPath (top, tr);
            juce::Path lines;
            for (float x : { 30.0f, 50.0f, 70.0f })
            {
                lines.startNewSubPath (x, 56.0f);
                lines.lineTo (x, 78.0f);
            }
            g.setColour (dark.withAlpha (0.5f));
            g.strokePath (lines, strokeOf (2.0f), tr);
            juce::Path sticks;
            sticks.startNewSubPath (20.0f, 10.0f);
            sticks.lineTo (52.0f, 42.0f);
            sticks.startNewSubPath (82.0f, 12.0f);
            sticks.lineTo (52.0f, 42.0f);
            g.setColour (colour);
            g.strokePath (sticks, strokeOf (4.5f), tr);
            break;
        }
        case Icon::keys:
        {
            const float x0 = 12.0f, w = 76.0f / 7.0f;
            for (int i = 0; i < 7; ++i)
            {
                juce::Path key;
                key.addRoundedRectangle (x0 + (float) i * w + 0.8f, 22.0f, w - 1.6f, 58.0f, 2.5f);
                g.setColour (colour);
                g.fillPath (key, tr);
            }
            for (int i : { 0, 1, 3, 4, 5 })
            {
                juce::Path key;
                key.addRoundedRectangle (x0 + (float) (i + 1) * w - w * 0.3f, 22.0f, w * 0.6f, 34.0f, 2.0f);
                g.setColour (colours::bg);
                g.fillPath (key, tr);
            }
            break;
        }
    }
}
} // namespace draw

//==============================================================================
Splash::Splash (const juce::String& sub, std::vector<Card> c)
    : subtitle (sub), cards (std::move (c)), hover (cards.size(), 0.0f)
{
    footer = U ("Geminis Audio · v") + GEMINIS_VERSION;
    for (int i = 0; i < 260; ++i)
    {
        const float layer = (float) (i % 3);
        stars.push_back ({ rng.nextFloat(), rng.nextFloat(),
                           0.5f + rng.nextFloat() * (0.8f + layer * 0.7f),
                           rng.nextFloat() * 6.28f, 0.6f + rng.nextFloat() * 2.2f, layer });
    }

    emblemImage = draw::emblem();
    glowImage = draw::emblemGlow();
    startTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

Splash::~Splash() { stopTimer(); }

void Splash::visibilityChanged()
{
    if (isVisible()) startTimerHz (45);
    else stopTimer();
}

void Splash::resized()
{
    const int size = juce::roundToInt (juce::jmin ((float) getHeight() * 0.32f, 250.0f));
    emblemSmall = emblemImage.rescaled (size * 2, size * 2, juce::Graphics::highResamplingQuality);
}

void Splash::timerCallback()
{
    t = juce::Time::getMillisecondCounterHiRes() * 0.001 - startTime;

    for (int i = 0; i < (int) cards.size(); ++i)
    {
        const float target = hovered == i ? 1.0f : 0.0f;
        hover[(size_t) i] += (target - hover[(size_t) i]) * 0.18f;
    }

    if (shooting.life <= 0.0f && rng.nextFloat() < 0.006f)
        shooting = { 0.15f + rng.nextFloat() * 0.6f, 0.03f + rng.nextFloat() * 0.25f, 1.0f };
    if (shooting.life > 0.0f)
        shooting.life -= 0.022f;

    repaint();
}

juce::Rectangle<float> Splash::cardBounds (int index) const
{
    const float n = (float) cards.size();
    const float gap = cards.size() > 3 ? 18.0f : 26.0f, h = 210.0f;
    const float w = juce::jmin (300.0f, ((float) getWidth() - 120.0f - (n - 1.0f) * gap) / n);
    const float total = n * w + (n - 1.0f) * gap;
    const float y = (float) getHeight() - h - 64.0f - 10.0f * hover[(size_t) index];
    return { ((float) getWidth() - total) * 0.5f + (float) index * (w + gap), y, w, h };
}

void Splash::paintBackground (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float tf = (float) t;

    juce::ColourGradient sky (juce::Colour (0xff1c1446), b.getCentreX(), b.getHeight() * 0.28f,
                              juce::Colour (0xff04040a), b.getCentreX(), b.getHeight() * 1.15f, true);
    sky.addColour (0.45, juce::Colour (0xff0b0920));
    g.setGradientFill (sky);
    g.fillAll();

    // Nebulosas que se desplazan lentamente
    auto nebula = [&] (float cx, float cy, float r, juce::Colour col)
    {
        juce::ColourGradient neb (col, cx, cy, col.withAlpha (0.0f), cx + r, cy, true);
        g.setGradientFill (neb);
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    };
    nebula (b.getWidth() * (0.22f + 0.03f * std::sin (tf * 0.07f)), b.getHeight() * 0.30f, b.getWidth() * 0.34f, juce::Colour (0xff2b7fb8).withAlpha (0.10f));
    nebula (b.getWidth() * (0.80f + 0.03f * std::cos (tf * 0.05f)), b.getHeight() * 0.26f, b.getWidth() * 0.32f, juce::Colour (0xffc88a28).withAlpha (0.08f));
    nebula (b.getWidth() * 0.5f, b.getHeight() * (0.40f + 0.02f * std::sin (tf * 0.09f)), b.getWidth() * 0.30f, juce::Colour (0xff7b5cff).withAlpha (0.13f));

    // Estrellas en tres capas con parpadeo y deriva
    for (const auto& s : stars)
    {
        float x = s.x + tf * 0.0018f * (s.layer + 1.0f);
        x -= std::floor (x);
        const float y = s.y;
        const float a = (0.35f + 0.65f * (0.5f + 0.5f * std::sin (tf * s.speed + s.phase))) * (0.35f + s.layer * 0.25f);
        const float sz = s.size;
        g.setColour ((s.layer > 1.5f ? juce::Colour (0xfffff3d6) : juce::Colour (0xffd9ecff)).withAlpha (a));
        g.fillEllipse (x * b.getWidth() - sz * 0.5f, y * b.getHeight() - sz * 0.5f, sz, sz);
        if (sz > 2.0f && a > 0.6f)
        {
            g.setColour (juce::Colours::white.withAlpha ((a - 0.6f) * 0.6f));
            g.drawHorizontalLine ((int) (y * b.getHeight()), x * b.getWidth() - sz * 2.5f, x * b.getWidth() + sz * 2.5f);
            g.drawVerticalLine ((int) (x * b.getWidth()), y * b.getHeight() - sz * 2.5f, y * b.getHeight() + sz * 2.5f);
        }
    }

    // Estrella fugaz
    if (shooting.life > 0.0f)
    {
        const float prog = 1.0f - shooting.life;
        const float x = (shooting.x + prog * 0.25f) * b.getWidth();
        const float y = (shooting.y + prog * 0.12f) * b.getHeight();
        juce::ColourGradient tail (juce::Colours::white.withAlpha (0.9f * shooting.life), x, y,
                                   juce::Colours::white.withAlpha (0.0f), x - 140.0f, y - 68.0f, false);
        g.setGradientFill (tail);
        g.drawLine (x - 140.0f, y - 68.0f, x, y, 1.6f);
    }
}

void Splash::paintEmblem (juce::Graphics& g, juce::Point<float> c, float size)
{
    const float tf = (float) t;
    const float pulse = 0.5f + 0.5f * std::sin (tf * 1.3f);

    // Halo
    const float glowSize = size * 1.55f;
    g.setOpacity (0.45f + 0.35f * pulse);
    g.drawImage (glowImage, juce::Rectangle<float> (c.x - glowSize * 0.5f, c.y - glowSize * 0.5f, glowSize, glowSize));
    g.setOpacity (1.0f);

    // Anillo exterior que gira despacio
    const float ringR = size * 0.62f;
    for (int i = 0; i < 48; ++i)
    {
        const float a = (float) i / 48.0f * juce::MathConstants<float>::twoPi + tf * 0.05f;
        const bool major = i % 4 == 0;
        const float r1 = ringR, r2 = ringR + (major ? 9.0f : 4.0f);
        g.setColour ((major ? colours::gold : colours::textFaint).withAlpha (major ? 0.7f : 0.5f));
        g.drawLine (c.x + std::cos (a) * r1, c.y + std::sin (a) * r1, c.x + std::cos (a) * r2, c.y + std::sin (a) * r2, major ? 1.4f : 1.0f);
    }
    g.setColour (colours::gold.withAlpha (0.25f));
    g.drawEllipse (c.x - ringR, c.y - ringR, ringR * 2.0f, ringR * 2.0f, 1.0f);

    // Dos "gemelos" orbitando en sentidos opuestos
    for (int k = 0; k < 2; ++k)
    {
        const float a = tf * 0.35f * (k == 0 ? 1.0f : -1.0f) + (k == 0 ? 0.0f : 3.1416f);
        const float r = ringR + 18.0f;
        const auto col = k == 0 ? colours::cyan : colours::gold;
        const float x = c.x + std::cos (a) * r, y = c.y + std::sin (a) * r * 0.98f;
        g.setColour (col.withAlpha (0.25f));
        g.fillEllipse (x - 7.0f, y - 7.0f, 14.0f, 14.0f);
        g.setColour (col);
        g.fillEllipse (x - 2.5f, y - 2.5f, 5.0f, 5.0f);
    }

    g.drawImage (emblemSmall, juce::Rectangle<float> (c.x - size * 0.5f, c.y - size * 0.5f, size, size));
}

void Splash::paintCard (juce::Graphics& g, int index)
{
    const auto& card = cards[(size_t) index];
    const float h = hover[(size_t) index];
    const auto r = cardBounds (index);

    // Halo de color bajo la tarjeta
    if (h > 0.01f)
    {
        juce::ColourGradient halo (card.colour.withAlpha (0.28f * h), r.getCentreX(), r.getCentreY(),
                                   card.colour.withAlpha (0.0f), r.getCentreX() + r.getWidth() * 0.75f, r.getCentreY(), true);
        g.setGradientFill (halo);
        g.fillEllipse (r.expanded (r.getWidth() * 0.25f, r.getHeight() * 0.35f));
    }

    juce::ColourGradient fill (juce::Colour (0xff1b1c34).withAlpha (0.82f), r.getX(), r.getY(),
                               juce::Colour (0xff0b0b18).withAlpha (0.9f), r.getX(), r.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (r, 16.0f);

    juce::ColourGradient border (card.colour.withAlpha (0.35f + 0.6f * h), r.getX(), r.getY(),
                                 card.colour.withAlpha (0.05f + 0.25f * h), r.getRight(), r.getBottom(), false);
    g.setGradientFill (border);
    g.drawRoundedRectangle (r.reduced (0.5f), 16.0f, 1.0f + h);

    // Brillo superior
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.07f), r.getX(), r.getY(),
                                juce::Colours::white.withAlpha (0.0f), r.getX(), r.getY() + 60.0f, false);
    g.setGradientFill (sheen);
    g.fillRoundedRectangle (r.withHeight (60.0f).reduced (1.0f), 15.0f);

    const bool compact = cards.size() > 3;
    auto inner = r.reduced (compact ? 18.0f : 22.0f, 20.0f);
    auto iconArea = inner.removeFromTop (58.0f).removeFromLeft (58.0f);
    juce::ColourGradient disc (card.colour.withAlpha (0.28f + 0.2f * h), iconArea.getCentreX(), iconArea.getY(),
                               card.colour.withAlpha (0.05f), iconArea.getCentreX(), iconArea.getBottom(), false);
    g.setGradientFill (disc);
    g.fillRoundedRectangle (iconArea, 14.0f);
    g.setColour (card.colour.withAlpha (0.5f));
    g.drawRoundedRectangle (iconArea.reduced (0.5f), 14.0f, 1.0f);
    draw::icon (g, card.icon, iconArea.reduced (11.0f), card.colour);

    inner.removeFromTop (14.0f);
    g.setColour (colours::text);
    g.setFont (Fonts::ui (compact ? 21.0f : 23.0f, 700));
    g.drawFittedText (card.title, inner.removeFromTop (30.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    inner.removeFromTop (4.0f);
    g.setColour (colours::textDim);
    g.setFont (Fonts::ui (compact ? 13.0f : 13.5f));
    g.drawFittedText (card.line1, inner.removeFromTop (19.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
    g.drawFittedText (card.line2, inner.removeFromTop (19.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);

    auto actionRow = inner.removeFromBottom (22.0f);
    g.setColour (card.colour.withAlpha (0.75f + 0.25f * h));
    draw::spacedText (g, upper (card.action), Fonts::ui (12.0f, 700), actionRow, juce::Justification::centredLeft, 0.12f);
    const float arrowX = actionRow.getX() + Fonts::ui (12.0f, 700).withExtraKerningFactor (0.12f).getStringWidthFloat (upper (card.action)) + 10.0f + 6.0f * h;
    juce::Path arrow;
    arrow.startNewSubPath (arrowX, actionRow.getCentreY());
    arrow.lineTo (arrowX + 14.0f, actionRow.getCentreY());
    arrow.startNewSubPath (arrowX + 9.0f, actionRow.getCentreY() - 4.5f);
    arrow.lineTo (arrowX + 14.0f, actionRow.getCentreY());
    arrow.lineTo (arrowX + 9.0f, actionRow.getCentreY() + 4.5f);
    g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void Splash::paint (juce::Graphics& g)
{
    paintBackground (g);

    const auto b = getLocalBounds().toFloat();
    const float emblemSize = juce::jmin (b.getHeight() * 0.32f, 250.0f);
    const juce::Point<float> ec (b.getCentreX(), 40.0f + emblemSize * 0.62f);
    paintEmblem (g, ec, emblemSize);

    const float wordY = ec.y + emblemSize * 0.5f + 34.0f;
    juce::ColourGradient gold (juce::Colour (0xfffff4d2), 0.0f, wordY, juce::Colour (0xffc88a28), 0.0f, wordY + 54.0f, false);
    gold.addColour (0.5, colours::gold);
    auto brand = Fonts::brand (54.0f).withExtraKerningFactor (0.18f);
    draw::textWithGradient (g, U ("GÉMINIS"), brand, { 0.0f, wordY, b.getWidth(), 60.0f }, juce::Justification::centred, gold);

    g.setColour (colours::textDim);
    draw::spacedText (g, upper (subtitle), Fonts::ui (14.0f, 600), { 0.0f, wordY + 64.0f, b.getWidth(), 22.0f },
                      juce::Justification::centred, 0.28f);

    for (int i = 0; i < (int) cards.size(); ++i)
        paintCard (g, i);

    g.setColour (colours::textFaint);
    g.setFont (Fonts::ui (12.0f));
    g.drawText (footer, getLocalBounds().removeFromBottom (30), juce::Justification::centred);

    if (messageVisible)
    {
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillAll();
        auto box = b.withSizeKeepingCentre (520.0f, 220.0f);
        draw::glassPanel (g, box, colours::violet, 0.8f, 16.0f);
        auto in = box.reduced (28.0f, 24.0f);
        g.setColour (colours::text);
        g.setFont (Fonts::ui (20.0f, 700));
        g.drawText (messageTitle, in.removeFromTop (30.0f), juce::Justification::centredLeft);
        in.removeFromTop (8.0f);
        messageButton = in.removeFromBottom (36.0f).removeFromRight (120.0f);
        g.setColour (colours::textDim);
        g.setFont (Fonts::ui (14.5f));
        g.drawFittedText (messageBody, in.toNearestInt(), juce::Justification::topLeft, 5);
        g.setColour (colours::violet);
        g.fillRoundedRectangle (messageButton, 8.0f);
        g.setColour (colours::bg);
        g.setFont (Fonts::ui (14.0f, 700));
        g.drawText ("Entendido", messageButton, juce::Justification::centred);
    }
}

void Splash::showMessage (const juce::String& title, const juce::String& body)
{
    messageTitle = title;
    messageBody = body;
    messageVisible = true;
    repaint();
}

void Splash::mouseMove (const juce::MouseEvent& e)
{
    int now = -1;
    if (! messageVisible)
        for (int i = 0; i < (int) cards.size(); ++i)
            if (cardBounds (i).expanded (0.0f, 12.0f).contains (e.position))
                now = i;
    hovered = now;
    setMouseCursor (now >= 0 || messageVisible ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void Splash::mouseExit (const juce::MouseEvent&) { hovered = -1; }

void Splash::mouseUp (const juce::MouseEvent& e)
{
    if (messageVisible)
    {
        if (messageButton.contains (e.position) || ! getLocalBounds().toFloat().withSizeKeepingCentre (520.0f, 220.0f).contains (e.position))
            messageVisible = false;
        repaint();
        return;
    }

    for (int i = 0; i < (int) cards.size(); ++i)
        if (cardBounds (i).expanded (0.0f, 12.0f).contains (e.position) && onChoose)
        {
            onChoose (i);
            return;
        }
}
} // namespace geminis
