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

    void setTimeSignature(int num, int den);
    void setBars(int bars);

    struct Palette
    {
        juce::Colour background      { 0xff12210a }; // dark grid bg (inside the roll)
        juce::Colour gridLine        { 0xffdf480f }; // accent orange (borders/accents if needed)
        juce::Colour gridWeak        { 0xff14230b }; // faint row/beat lines
        juce::Colour gridStrong      { 0xfff4b701 }; // barlines + header strip yellow
        juce::Colour barNumber       { juce::Colours::black }; // unused now (we use barNumberText)
        juce::Colour keyboardWhite   { 0xffffb607 };
        juce::Colour keyboardBlack   { 0xff132209 };
        juce::Colour barNumberStrip  { 0xfff4b701 }; // header strip fill
        juce::Colour barNumberText   { 0xffd84d02 }; // header numbers
        juce::Colour noteFill        { 0xffa8de00 }; // lime notes (match brand green)
        juce::Colour noteOutline     { 0xff4b5f0e }; // olive outline
        juce::Colour overlayFill     { 0xffde4d00 }; // optional overlays
        juce::Colour overlayOutline  { 0xffe5550a };
    };

    void setPalette(const Palette& p) { palette = p; repaint(); }


    // === Size helpers (needed by PluginEditor::updateRollContentSize) ===
    int   getPreferredHeight() const noexcept;
    float computeRowHeight(int gridPixelHeight) const noexcept;

    void setGrid(int bars, int beatsPerBar) noexcept;

    // If you already have a rowHeight() helper, modify it there instead of adding this.
private:

    // Grid/time


    // Data
    void setOverlayNotes(const std::vector<Note>& notesIn);

    // Pitch view

    int getPitchLow()  const noexcept { return pitchMin; }
    int getPitchHigh() const noexcept { return pitchMax; }

    // Component
    void paint(juce::Graphics&) override;

    void setDesiredSize(int w, int h) { desiredW = w; desiredH = h; resized(); }



private:
    std::vector<Note> melody, overlay;

    Palette palette;

    float verticalZoom_ = 1.0f; // 1.0 = normal, 1.25 = 25% taller, etc.

    // time
    int bars = 8;
    int beats = 4;

    // pitch
    int pitchMin = 12; // C2
    int pitchMax = 84; // C6

    // layout
    int headerHeight = 40; // was 24
    int keybedWidth = 78; // was 64

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
