#pragma once

#include <JuceHeader.h>

class MidiGenerator;                 // in PluginProcessor / MidiGenerator
class BANGAudioProcessor;            // your processor
class PianoRollComponent;            // your piano roll component
class AdvancedHarmonyWindow;         // your existing window class

//================================================================================
class BANGAudioProcessorEditor
    : public juce::AudioProcessorEditor,
    public juce::ComboBox::Listener,
    public juce::Slider::Listener
{
public:
    explicit BANGAudioProcessorEditor(BANGAudioProcessor& proc);
    ~BANGAudioProcessorEditor() override;

    // juce::Component
    void paint(juce::Graphics& g) override;
    void resized() override;

    // listeners (used only for selectors & sliders – buttons use lambdas)
    void comboBoxChanged(juce::ComboBox* box) override;
    void sliderValueChanged(juce::Slider* s) override;

private:
    // ---------- helpers ----------
    // load an Image from BinaryData by trying several possible names (case/variant tolerant)
    juce::Image loadImageAny(std::initializer_list<const char*> tryNames) const;

    // set a 3-state image button (normal / hover / down)
    void applyImageButton3(juce::ImageButton& btn,
        std::initializer_list<const char*> normalNames,
        std::initializer_list<const char*> overNames,
        std::initializer_list<const char*> downNames,
        juce::Rectangle<int> bounds);

    // make a Drawable from an Image (used for the engine radio images, if needed)
    static std::unique_ptr<juce::Drawable> makeDrawable(const juce::Image& img);

    // UI wiring
    void pushSettingsToGenerator();     // send current UI -> MidiGenerator
    void regenerate();                  // call generator and refresh roll
    void performDragExport();           // write temp MIDI & start external drag

    // layout helpers
    int currentBars() const;            // read bars from barsBox (only 4 or 8)
    int currentTSNumerator() const;     // parse “n/d”
    int currentTSDenominator() const;   // parse “n/d”
    void updateRollContentSize();       // set pianoRoll content size to match bars/TS

    // open sub-windows
    void openAdvanced();
    void openPolyrhythm();
    void openReharmonize();
    void openAdjust();

    // randomize all selectors + maybe advanced toggles, then regenerate
    void randomizeAll();

    // key helpers
    static int rootBoxToSemitone(const juce::ComboBox& keyBox);

    // ---------- members ----------
    BANGAudioProcessor& audioProcessor;

    // palette / colors (your spec)
    const juce::Colour colBg         { 0xFFA5DD00 }; // plugin background
    const juce::Colour colWhiteKey   { 0xFFF2AE01 };
    const juce::Colour colBlackKey   { juce::Colours::black };
    const juce::Colour colRollBg     { 0xFF13220C };
    const juce::Colour colRollGrid   { 0xFF314025 };
    const juce::Colour colAccent     { 0xFFDD4F02 }; // sliders
    const juce::Colour colAccent2    { 0xFFD44C02 }; // alias if needed
    const juce::Colour colDropBg     { 0xFFF9BF00 }; // combobox bg
    const juce::Colour colText       { juce::Colours::black };

    // logo + title images
    juce::ImageComponent logoImg;
    juce::ImageComponent engineTitleImg;

    // ENGINE buttons (image buttons – one selected at all times)
    juce::ImageButton engineChordsBtn;
    juce::ImageButton engineMixtureBtn;
    juce::ImageButton engineMelodyBtn;

    // action buttons
    juce::ImageButton generateBtn;
    juce::ImageButton dragBtn;
    juce::ImageButton advancedBtn;
    juce::ImageButton polyrBtn;
    juce::ImageButton reharmBtn;
    juce::ImageButton adjustBtn;
    juce::ImageButton diceBtn;

    // selectors
    juce::ComboBox keyBox, scaleBox, tsBox, barsBox;

    // labels
    juce::Label keyLbl, scaleLbl, tsLbl, barsLbl, restLbl;
    juce::Label timingLbl, velocityLbl, swingLbl, feelLbl;

    // sliders
    juce::Slider restSl;     // 0..100
    juce::Slider timingSl;   // 0..100
    juce::Slider velocitySl; // 0..100
    juce::Slider swingSl;    // 0..100
    juce::Slider feelSl;     // 0..100

    // piano roll & scrolling
    juce::Viewport pianoViewport;
    std::unique_ptr<PianoRollComponent> pianoRoll;

    // tiny hidden DragAndDropContainer to initiate external file drags
    struct DnDHelper : public juce::Component, public juce::DragAndDropContainer {};
    std::unique_ptr<DnDHelper> dndHelper;

    // engine state (mirrors MidiGenerator::EngineMode)
    enum class EngineMode { Chords = 0, Mixture, Melody };
    EngineMode currentEngine = EngineMode::Mixture;

    // utility
    static void styleCombo(juce::ComboBox& c, juce::Colour bg, juce::Colour txt);
    static void styleSlider(juce::Slider& s, juce::Colour track, juce::Colour thumb, juce::Colour txt);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BANGAudioProcessorEditor)
};
