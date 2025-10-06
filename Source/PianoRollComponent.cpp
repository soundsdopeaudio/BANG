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
    if (lowMidi > highMidi) std::swap(lowMidi, highMidi);
    pitchMin = juce::jlimit(0, 127, lowMidi);
    pitchMax = juce::jlimit(0, 127, highMidi);
    repaint();
}

void PianoRollComponent::setTimeSignature(int num, int /*den*/)
{
    beats = juce::jlimit(1, 24, num);
    repaint();
}

void PianoRollComponent::setBars(int newBars)
{
    bars = juce::jlimit(1, 64, newBars);
    repaint();
}

void PianoRollComponent::setVerticalZoom(float factor) noexcept
{
    // keep it sane: 0.5x .. 3x
    verticalZoom_ = juce::jlimit(0.5f, 3.0f, factor);
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
    const auto grid = gridArea();  // whatever rect you use for the roll grid
    const int gridH = grid.getHeight();
    return rows > 0 ? (float)gridArea().getHeight() / (float)rows : computeRowHeight(gridH);
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

int PianoRollComponent::getPreferredHeight() const noexcept
{
    // How many MIDI rows we draw
    const int rows = (pitchMax - pitchMin + 1);

    // Base pixels per row before zoom; tweak if you want denser/sparser lanes
    constexpr float kBasePxPerRow = 10.0f;

    // Include the header strip height so numbers/labels have room
    const float gridH = rows > 0 ? rows * kBasePxPerRow * verticalZoom_ : (float)desiredH;
    return juce::roundToInt(gridH + (float)headerHeight);
}

float PianoRollComponent::computeRowHeight(int gridPixelHeight) const noexcept
{
    const int rows = (pitchMax - pitchMin + 1);
    if (rows <= 0) return 0.0f;
    return (gridPixelHeight / (float)rows) * verticalZoom_;
}

void PianoRollComponent::resized()
{
    // Ensure a sensible initial size even before parent resizes
 //   int parentW = getParentWidth();
  //   int parentH = getParentHeight();

 //    if (parentW <= 0) parentW = desiredW;
  //   if (parentH <= 0) parentH = desiredH;

  //   setSize(juce::jmax(desiredW, parentW),
   //      juce::jmax(desiredH, parentH));
}


void PianoRollComponent::paint(juce::Graphics& g)
{
    g.fillAll(palette.background);
    g.setColour(palette.gridLine);

    // header (bar numbers)
// header (bar numbers)
// header (per-beat numbers that follow the selected time signature)
    {
        auto h = headerArea();
        g.setColour(palette.barNumberStrip);
        g.fillRect(h);

        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));

        for (int bar = 0; bar < bars; ++bar)
        {
            for (int b = 0; b < beats; ++b)
            {
                const double beatStart = (double)bar * (double)beats + (double)b;
                const double beatEnd = beatStart + 1.0;

                const auto x = xForBeat(beatStart);
                const auto nx = xForBeat(beatEnd);

                g.drawText(juce::String(b + 1),
                    juce::Rectangle<int>((int)x, h.getY(), (int)(nx - x), h.getHeight()),
                    juce::Justification::centred, false);
            }
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

            // lay a full-width white key base
            juce::Rectangle<float> fullKey((float)kb.getX(), y, (float)kb.getWidth(), rowHeight());
            g.setColour(palette.keyboardWhite);
            g.fillRect(fullKey);

            if (black)
            {
                // half-width black key, FLUSH-LEFT (starts where the white keys begin)
                const float blackW = (float)kb.getWidth() * 0.5f;
                const float xLeft = (float)kb.getX();
                juce::Rectangle<float> blackKey(xLeft, y, blackW, rowHeight());
                g.setColour(palette.keyboardBlack);
                g.fillRect(blackKey);
            }

            // octave guides on C
            if (p % 12 == 0)
            {
                g.setColour(palette.gridStrong);
                g.drawLine((float)kb.getRight() - 1, y, (float)getWidth(), y, 1.0f);
            }
        }
        g.setColour(juce::Colours::black);
        g.drawRect(kb);
    }

    auto ga = gridArea();

    // vertical lines (beats + bars) — force both to the same color #314125
    {
        const int totalBeats = bars * beats;
        const juce::Colour vline = juce::Colour::fromRGB(0x31, 0x41, 0x25); // #314125

        for (int b = 0; b <= totalBeats; ++b)
        {
            const float x = xForBeat((double)b);
            const bool isBarLine = (b % beats == 0);

            g.setColour(vline);
            g.drawLine(x, (float)ga.getY(), x, (float)ga.getBottom(), isBarLine ? 2.0f : 1.0f);
        }
    }


// horizontal rows + background stripes aligned to the piano keys
    for (int p = pitchMax; p >= pitchMin; --p)
    {
        const float y = yForPitch(p);
        const float h = rowHeight();
        const bool  black = isBlack(p);

        // 1) fill the stripe (full grid width)
        juce::Rectangle<float> rowRect((float)ga.getX(), y, (float)ga.getWidth(), h);
        // brighter stripes that lift the background instead of darkening it
        const auto brightBase = palette.barNumberStrip;            // your bright yellow
        const auto fillCol = brightBase.withAlpha(black ? 0.14f : 0.08f);
        g.setColour(fillCol);
        g.fillRect(rowRect);

        // make the separator line a touch clearer too
        g.setColour(palette.gridWeak.withAlpha(0.45f));
        g.drawLine((float)ga.getX(), y, (float)ga.getRight(), y, 1.0f);

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
            g.setColour(fill);
            g.fillRoundedRectangle(rect, 6.0f);          // radius 6
            g.setColour(outline);
            g.drawRoundedRectangle(rect, 6.0f, 2.0f);    // thicker outline
        }
    };

    drawNotes(melody, palette.noteFill, palette.noteOutline);
    drawNotes(overlay, palette.overlayFill, palette.overlayOutline);
}

