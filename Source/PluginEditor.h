#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PianoRollComponent.h"
#include "MidiGenerator.h"
#include "AdvancedHarmonyWindow.h"
#include "BinaryData.h"

// === Small helpers implemented in PluginEditor.cpp ===
juce::Image loadImageByHint(const juce::String& hint);
void setImageButton3(juce::ImageButton& b, const juce::String& baseHint);

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

    // for drag-out
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    // ===================== model / processor =====================
    BANGAudioProcessor& audioProcessor;

    // ===================== left selectors ========================
    juce::ImageComponent keyLbl, scaleLbl, tsLbl, barsLbl, restLbl;
    juce::ComboBox keyBox, scaleBox, tsBox, barsBox;
    juce::Slider   restSl; // 0..100% rest density

    // ===================== right humanize ========================
    juce::ImageComponent humanizeTitle;
    juce::Slider timingSl, velocitySl, swingSl, feelSl;  // 0..100

    // ===================== center piano roll =====================
    juce::Viewport      rollView;
    PianoRollComponent  pianoRoll;

    // ===================== images / logo =========================
    juce::ImageComponent logoImg;
    juce::ImageComponent engineTitleImg;

    // ===================== engine buttons ========================
    juce::ImageButton engineChordsBtn;
    juce::ImageButton engineMixtureBtn;
    juce::ImageButton engineMelodyBtn;
    int currentEngineIndex = 1; // 0=chords, 1=mixture (default), 2=melody

    // ===================== action buttons ========================
    juce::ImageButton generateBtn;
    juce::ImageButton dragBtn;
    juce::ImageButton advancedBtn;
    juce::ImageButton polyrBtn;
    juce::ImageButton reharmBtn;
    juce::ImageButton adjustBtn;
    juce::ImageButton diceBtn;

    // ===================== editor state ==========================
    enum class EngineSel { Chords, Mixture, Melody };
    EngineSel engineSel{ EngineSel::Mixture };

    AdvancedHarmonyOptions advOptions;      // passed into generator

    double polyrhythmAmount = 25.0;
    double reharmonizeAmount = 30.0;

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
