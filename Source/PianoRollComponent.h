#pragma once
#include <JuceHeader.h>
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>

// Forward decl to avoid heavy include
struct Note;

class PianoRollComponent : public juce::Component
{
public:
    PianoRollComponent();

public:
    void setVerticalZoom(float factor) noexcept;
    float getVerticalZoom() const noexcept { return verticalZoom_; }
    void setNotes(const std::vector<Note>& notesIn);
    void setPitchRange(int lowMidi, int highMidi) noexcept;

    struct Palette
    {
        juce::Colour background      { 0xff12210a };
        juce::Colour gridLine        { 0xffe75108 };
        juce::Colour gridWeak        { 0xff14230d };
        juce::Colour gridStrong      { 0xfffcb807 };
        juce::Colour barNumber       { juce::Colours::black };
        juce::Colour keyboardWhite   { 0xffffb607 };
        juce::Colour keyboardBlack   { 0xff132209 };
        juce::Colour barNumberStrip  { 0xfff29b00 };
        juce::Colour barNumberText   { 0xffd84d02 };
        juce::Colour noteFill        { 0xffa8de00 }; // melody
        juce::Colour noteOutline     { 0x4b5f0e };
        juce::Colour overlayFill     { 0xffde4d00 };
        juce::Colour overlayOutline  { 0xffe5550a };
    };

    void setPalette(const Palette& p) { palette = p; repaint(); }

    // If you already have a rowHeight() helper, modify it there instead of adding this.
private:

    // Grid/time
    void setGrid(int bars, int beatsPerBar) noexcept;

    // Data
    void setOverlayNotes(const std::vector<Note>& notesIn);

    // Pitch view

    int getPitchLow()  const noexcept { return pitchMin; }
    int getPitchHigh() const noexcept { return pitchMax; }

    // Colours (optional theming)
    struct Colours {
        juce::Colour background      { 0xff12210a };
        juce::Colour gridLine        { 0xffe75108 };
        juce::Colour gridWeak        { 0xff14230d };
        juce::Colour gridStrong      { 0xfff4a202 };
        juce::Colour barNumber       { juce::Colours::black };
        juce::Colour keyboardWhite   { 0xffffb607 };
        juce::Colour keyboardBlack   { 0xff132209 };
        juce::Colour barNumberStrip  { 0xfff29b00 };
        juce::Colour barNumberText   { 0xffd84d02 };
        juce::Colour noteFill        { 0xffa8de00 }; // melody
        juce::Colour noteOutline     { 0x4b5f0e };
        juce::Colour overlayFill     { 0xffde4d00 };
        juce::Colour overlayOutline  { 0xff64290a };
    } colours;

    // Component
    void paint(juce::Graphics&) override;

    void setDesiredSize(int w, int h) { desiredW = w; desiredH = h; resized(); }

    void setTimeSignature(int num, int den);
    void setBars(int bars);

    int getPreferredHeight() const noexcept
    {
        // base grid height target per row (e.g., 16 px per row at zoom 1.0)
        constexpr float basePixelsPerRow = 16.0f;
        const int rows = (pitchMin - pitchMax + 1);
        return (int)std::ceil(rows * basePixelsPerRow * verticalZoom_);
    }


private:
    std::vector<Note> melody, overlay;

    Palette palette;

    float verticalZoom_ = 1.0f; // 1.0 = normal, 1.25 = 25% taller, etc.

    float computeRowHeight(int gridPixelHeight) const noexcept
    {
        const int rows = (pitchMin - pitchMax + 1);
        if (rows <= 0) return 0.0f;
        return (gridPixelHeight / (float)rows) * verticalZoom_;
    }

    // time
    int bars = 8;
    int beats = 4;

    // pitch
    int pitchMin = 12; // C2
    int pitchMax = 84; // C6

    // layout
    int headerHeight = 24;
    int keybedWidth = 64;

    // helpers
    static bool isBlack(int midiNote);
    float xForBeat(double beat) const;
    float widthForBeats(double beatsIn) const;
    float yForPitch(int midi) const;
    float rowHeight() const;
    juce::Rectangle<int> gridArea() const;
    juce::Rectangle<int> keybedArea() const;
    juce::Rectangle<int> headerArea() const;

    int desiredW = 1200, desiredH = 320;

    void resized() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PianoRollComponent)
};
