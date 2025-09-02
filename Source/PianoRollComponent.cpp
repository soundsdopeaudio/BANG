#include "PianoRollComponent.h"
#include "MidiGenerator.h" // for Note struct

PianoRollComponent::PianoRollComponent()
{
    setOpaque(true);
}

void PianoRollComponent::setGrid(int newBars, int beatsPerBar) noexcept
{
    bars = juce::jlimit(1, 64, newBars);
    beats = juce::jlimit(1, 24, beatsPerBar);
    repaint();
}

void PianoRollComponent::setNotes(const std::vector<Note>& notesIn)
{
    melody = notesIn;
    repaint();
}

void PianoRollComponent::setOverlayNotes(const std::vector<Note>& notesIn)
{
    overlay = notesIn;
    repaint();
}

void PianoRollComponent::setPitchRange(int lowMidi, int highMidi) noexcept
{
    pitchMin = juce::jlimit(0, 126, juce::jmin(lowMidi, highMidi));
    pitchMax = juce::jlimit(1, 127, juce::jmax(lowMidi, highMidi));
    repaint();
}

bool PianoRollComponent::isBlack(int midi)
{
    static const bool table[12] = { false,true,false,true,false,false,true,false,true,false,true,false };
    return table[midi % 12];
}

juce::Rectangle<int> PianoRollComponent::headerArea() const { return { keybedWidth, 0, getWidth() - keybedWidth, headerHeight }; }
juce::Rectangle<int> PianoRollComponent::keybedArea()  const { return { 0, headerHeight, keybedWidth, getHeight() - headerHeight }; }
juce::Rectangle<int> PianoRollComponent::gridArea()    const { return { keybedWidth, headerHeight, getWidth() - keybedWidth, getHeight() - headerHeight }; }

float PianoRollComponent::rowHeight() const
{
    const int rows = pitchMax - pitchMin + 1;
    return rows > 0 ? (float)gridArea().getHeight() / (float)rows : 12.0f;
}

float PianoRollComponent::yForPitch(int midi) const
{
    midi = juce::jlimit(pitchMin, pitchMax, midi);
    const int rowsFromTop = (pitchMax - midi);
    return (float)gridArea().getY() + rowsFromTop * rowHeight();
}

float PianoRollComponent::xForBeat(double beat) const
{
    const double totalBeats = (double)bars * (double)beats;
    const auto ga = gridArea();
    if (totalBeats <= 0.0) return (float)ga.getX();
    return (float)ga.getX() + (float)((beat / totalBeats) * (double)ga.getWidth());
}

float PianoRollComponent::widthForBeats(double beatsIn) const
{
    const double totalBeats = (double)bars * (double)beats;
    const auto ga = gridArea();
    if (totalBeats <= 0.0) return 1.0f;
    return (float)((beatsIn / totalBeats) * (double)ga.getWidth());
}

void PianoRollComponent::resized()
{
    // If you have internal children, layout them here.
    // Then set our own size for the viewport:
    setSize(juce::jmax(desiredW, getParentWidth()),
        juce::jmax(desiredH, getParentHeight()));
}

void PianoRollComponent::paint(juce::Graphics& g)
{
    g.fillAll(palette.background);
    g.setColour(palette.gridLine);

    // header (bar numbers)
    {
        auto h = headerArea();
        g.setColour(colours.gridStrong.withAlpha(0.6f));
        g.fillRect(h);

        g.setColour(colours.barNumber);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        for (int bar = 0; bar < bars; ++bar)
        {
            auto x = xForBeat((double)bar * beats);
            auto nx = xForBeat((double)(bar + 1) * beats);
            g.drawText(juce::String(bar + 1),
                juce::Rectangle<int>((int)x, h.getY(), (int)(nx - x), h.getHeight()),
                juce::Justification::centredLeft, false);
        }
    }

    // left piano keybed
    {
        auto kb = keybedArea();
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRect(kb);

        for (int p = pitchMax; p >= pitchMin; --p)
        {
            auto y = yForPitch(p);
            auto r = juce::Rectangle<float>((float)kb.getX(), y, (float)kb.getWidth(), rowHeight());
            const bool black = isBlack(p);
            g.setColour(black ? colours.keyboardBlack : colours.keyboardWhite);
            g.fillRect(r);

            // octave guides on C
            if (p % 12 == 0)
            {
                g.setColour(colours.gridStrong);
                g.drawLine((float)kb.getRight() - 1, y, (float)getWidth(), y, 1.0f);
            }
        }
        g.setColour(juce::Colours::black); g.drawRect(kb);
    }

    auto ga = gridArea();

    // vertical lines (beats + bars)
    {
        const int totalBeats = bars * beats;
        for (int b = 0; b <= totalBeats; ++b)
        {
            const float x = xForBeat((double)b);
            const bool isBarLine = (b % beats == 0);
            g.setColour(isBarLine ? colours.gridStrong : colours.gridWeak);
            g.drawLine(x, (float)ga.getY(), x, (float)ga.getBottom(), isBarLine ? 2.0f : 1.0f);
        }
    }

    // horizontal rows
    for (int p = pitchMax; p >= pitchMin; --p)
    {
        auto y = yForPitch(p);
        g.setColour(colours.gridWeak.withAlpha(isBlack(p) ? 0.25f : 0.15f));
        g.drawLine((float)ga.getX(), y, (float)ga.getRight(), y);
    }

    auto drawNotes = [&](const std::vector<Note>& list, juce::Colour fill, juce::Colour outline)
    {
        for (const auto& n : list)
        {
            const float x = xForBeat(n.startBeats);
            const float w = juce::jmax(3.0f, widthForBeats(n.lengthBeats));
            const float y = yForPitch(n.pitch);
            const float h = rowHeight() - 1.0f;

            juce::Rectangle<float> rect(x, y + 1.0f, w, h - 2.0f);
            g.setColour(fill);    g.fillRoundedRectangle(rect, 3.0f);
            g.setColour(outline); g.drawRoundedRectangle(rect, 3.0f, 1.2f);
        }
    };

    drawNotes(melody, colours.noteFill, colours.noteOutline);
    drawNotes(overlay, colours.overlayFill, colours.overlayOutline);
}
