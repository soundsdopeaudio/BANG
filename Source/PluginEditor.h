#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PianoRollComponent.h"
#include "MidiGenerator.h"
#include "AdvancedHarmonyWindow.h"
#include "BinaryData.h"

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

private:
    // ===== Helpers =====
    static juce::Image loadBinaryImage(const juce::String& baseName);
    static juce::Image loadBinaryImage(std::initializer_list<juce::String> tryNames);

    // ===== Images from BinaryData (no scaling, no stretching) =====
    juce::ImageComponent imgSelectEngine;
    juce::ImageComponent imgToolTips;
    juce::ImageComponent imgOctaveLbl;
    juce::ImageComponent imgSoundsDope;
    juce::ImageComponent logoImg;

    // ===== Top engine buttons =====
    juce::ImageButton engineChordsBtn;
    juce::ImageButton engineMixtureBtn;
    juce::ImageButton engineMelodyBtn;
    int currentEngineIndex = 1; // 0=chords, 1=mixture (default), 2=melody

    // ===== Left column selectors =====
    juce::ImageComponent keyLblImg;
    juce::ImageComponent scaleLblImg;
    juce::ImageComponent timeSigLblImg;
    juce::ImageComponent barsLblImg;
    juce::ImageComponent restdensityLblImg;
    juce::ImageComponent octaveLblImg;
    juce::ImageComponent humanizeLblImg;

    juce::ComboBox keyBox;
    juce::ComboBox scaleBox;
    juce::ComboBox tsBox;
    juce::ComboBox barsBox;
    juce::Slider   restSl;   // 0..100 (%) - horizontal
    juce::Label    restDensityValue;    // shows percent
    juce::ComboBox octaveBox;           // (NEW) not wired yet

    // ===== Mid / right controls =====
    juce::ImageButton advancedBtn;
    juce::ImageButton polyrBtn;
    juce::ImageButton reharmBtn;
    juce::ImageButton adjustBtn;
    juce::ImageButton diceBtn;
    juce::TextButton timingValSl;
    juce::TextButton velocityValSl;
    juce::TextButton swingValSl;
    juce::TextButton feelValSl;

    juce::Slider swingSl;        // top
    juce::Slider velocitySl;    
    juce::Slider timingSl;
    juce::Slider feelSl;        // bottom

    // ===== Piano Roll =====
    juce::Viewport      rollView;
    PianoRollComponent  pianoRoll;

    // ===== Bottom action buttons =====
    juce::ImageButton generateBtn;
    juce::ImageButton dragBtn;

    // ===================== model / processor =====================
    BANGAudioProcessor& audioProcessor;

    juce::ImageComponent engineTitleImg;

    // ===================== action buttons ========================

    juce::Rectangle<int> generateBtnNaturalBounds;
    juce::Rectangle<int> dragBtnNaturalBounds;


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
