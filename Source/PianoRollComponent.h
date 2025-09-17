#pragma once
#include <JuceHeader.h>
#include <vector>

// Forward decl to avoid heavy include
struct Note;

class PianoRollComponent : public juce::Component
{
public:
    PianoRollComponent();

    // Grid/time
    void setGrid(int bars, int beatsPerBar) noexcept;

    // Data
    void setNotes(const std::vector<Note>& notesIn);
    void setOverlayNotes(const std::vector<Note>& notesIn);

    // Pitch view
    void setPitchRange(int lowMidi, int highMidi) noexcept;

    // Colours (optional theming)
    struct Colours {
        juce::Colour background      { 0xff0f0f12 };
        juce::Colour gridStrong      { 0xff30333a };
        juce::Colour gridWeak        { 0xff24262c };
        juce::Colour barNumber       { juce::Colours::whitesmoke };
        juce::Colour keyboardWhite   { 0xffe7e7ea };
        juce::Colour keyboardBlack   { 0xff3b3e46 };
        juce::Colour noteFill        { 0xffff7a20 }; // melody
        juce::Colour noteOutline     { 0xffb24a00 };
        juce::Colour overlayFill     { juce::Colours::cornflowerblue.withAlpha(0.85f) };
        juce::Colour overlayOutline  { juce::Colours::darkslateblue };
    } colours;

    // Component
    void paint(juce::Graphics&) override;

    struct Palette
    {
        juce::Colour background;
        juce::Colour gridLine;
        juce::Colour keyWhiteFill;
        juce::Colour keyBlackFill;
        juce::Colour barNumberStrip;
        juce::Colour barNumberText;
    };

    void setPalette(const Palette& p) { palette = p; repaint(); }

    void setDesiredSize(int w, int h) { desiredW = w; desiredH = h; resized(); }

    void setTimeSignature(int num, int den);
    void setBars(int bars);


private:
    std::vector<Note> melody, overlay;

    Palette palette;

    // time
    int bars = 8;
    int beats = 4;

    // pitch
    int pitchMin = 36; // C2
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
