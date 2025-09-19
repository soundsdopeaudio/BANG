#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PianoRollComponent.h"
#include "MidiGenerator.h"
#include "AdvancedHarmonyWindow.h"
#include "BinaryData.h"
#include <juce_gui_basics/juce_gui_basics.h>

// === Small helpers implemented in PluginEditor.cpp ===
juce::Image loadImageByHint(const juce::String& hint);            // fuzzy lookup in BinaryData
void setImageButton3(juce::ImageButton& b, const juce::String& baseHint); // normal/hover/down

class BANGAudioProcessorEditor : public juce::AudioProcessorEditor,
    public juce::DragAndDropContainer,
    private juce::ComboBox::Listener,
    private juce::Slider::Listener

{
public:
    explicit BANGAudioProcessorEditor(BANGAudioProcessor&);
    ~BANGAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    juce::TooltipWindow tooltipWindow { this, 500 }; // 500ms delay

    // for drag-out
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    juce::ImageComponent keyLblImg;
    juce::ImageComponent scaleLblImg;
    juce::ImageComponent timeSigLblImg;
    juce::ImageComponent barsLblImg;
    juce::ImageComponent restDensityLblImg;
    juce::ImageComponent humanizeLblImg;
    juce::ImageComponent octaveLblImg;

private:
    // ===================== model / processor =====================
    BANGAudioProcessor& audioProcessor;

    // ===================== left selectors ========================
    juce::Label   keyLbl, scaleLbl, tsLbl, barsLbl, restDensityLbl, octaveLbl;
    juce::ComboBox keyBox, scaleBox, tsBox, barsBox, octaveBox;
    juce::Slider   restSl; // 0..100% rest density

    // ===================== right humanize ========================
    juce::Label  humanizeTitle;
    juce::Label  timingLbl, velocityLbl, swingLbl, feelLbl; // headings
    juce::Slider timingSl, velocitySl, swingSl, feelSl;  // 0..100

    // ===================== center piano roll =====================
    juce::Viewport      rollView;
    PianoRollComponent  pianoRoll;

    // ===================== images / logo =========================
    juce::ImageComponent logoImg;       // plugin logo
    juce::ImageComponent selectEngineImg;

    // ===================== engine buttons ========================
    juce::ImageButton engineMelodyBtn;
    juce::ImageButton engineChordsBtn;
    juce::ImageButton engineMixtureBtn;
    int currentEngineIndex = 1; // 0=chords, 1=mixture (default), 2=melody

    // ===================== action buttons ========================
    juce::ImageButton generateBtn;
    juce::ImageButton dragBtn;
    juce::ImageButton advancedBtn;
    juce::ImageButton polyrBtn;
    juce::ImageButton reharmBtn;
    juce::ImageButton adjustBtn;
    juce::ImageButton diceBtn;

    juce::Rectangle<int> generateBtnNaturalBounds;
    juce::Rectangle<int> dragBtnNaturalBounds;

    juce::ImageButton undoButton;
    juce::ImageButton redoButton;

    juce::Image undoUp, undoHover, undoDown;
    juce::Image redoUp, redoHover, redoDown;

    // Layout constants to keep things consistent
    static constexpr int kIconSize = 28;       // button width/height in pixels
    static constexpr int kIconPadding = 6;     // space around and between buttons

    // ===================== editor state ==========================
    enum class EngineSel { Chords, Mixture, Melody };
    EngineSel engineSel{ EngineSel::Mixture };

    AdvancedHarmonyOptions advOptions;      // passed into generator

    std::vector<Note> lastMelody;
    std::vector<Note> lastChords;

    // ===================== internal helpers ======================
    void onEngineChanged();
    void pushSettingsToGenerator();
    void regenerate();
    void randomizeAll();
    void performDragExport();
    juce::File writeTempMidiForDrag();

    // popups (kept lightweight but functional)
    void openAdvanced();
    void openPolyrhythm();
    void openReharmonize();
    void openAdjust();

    // UI utils
    void comboBoxChanged(juce::ComboBox* box) override;
    void sliderValueChanged(juce::Slider* s) override;

    int  currentBars() const;
    int  currentTSNumerator() const;
    void updateRollContentSize();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BANGAudioProcessorEditor)
};
