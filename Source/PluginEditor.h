#pragma once

#include <JuceHeader.h>
#include <vector>

#include "PluginProcessor.h"
#include "PianoRollComponent.h"
#include "MidiGenerator.h"
#include "AdvancedHarmonyWindow.h"

// Small helpers provided in PluginEditor.cpp
juce::Image loadImageByHint(const juce::String& hint);
void setImageButton3(juce::ImageButton& b, const juce::String& baseHint);

class BANGAudioProcessorEditor : public juce::AudioProcessorEditor,
    public juce::DragAndDropContainer,
    private juce::ComboBox::Listener,
    private juce::Slider::Listener
{
public:
    explicit BANGAudioProcessorEditor(BANGAudioProcessor& p);
    ~BANGAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Drag-out support
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    struct ReharmOptions
    {
        bool        enable = false;
        int         complexity = 50;            // 0..100
        juce::String reharmType = "Functional";  // "Functional" | "Modal" | "Tritone Subs"
    } reharm;

private:
    // ===== Model / processor =====
    BANGAudioProcessor& audioProcessor;

    // ===== Left selectors =====
    juce::Label   keyLbl, scaleLbl, tsLbl, barsLbl, restLbl;
    juce::ComboBox keyBox, scaleBox, tsBox, barsBox;
    juce::Slider   restSl;              // 0..100 (percent)

    // ===== Right humanize =====
    juce::Label   humanizeTitle;
    juce::Slider  timingSl, velocitySl, swingSl, feelSl; // 0..100

    // ===== Center piano roll =====
    PianoRollComponent pianoRoll;

    // ===== Header / decorative =====
    juce::ImageComponent logo;         // BANG logo
    juce::ImageComponent engineTitle;  // "ENGINE" strip image

    // ===== Engine image buttons (radio group) =====
    juce::ImageButton engineChordsBtn, engineMixtureBtn, engineMelodyBtn;
    int currentEngineIndex = 1; // 0=Chords, 1=Mixture (default), 2=Melody

    // ===== Main action buttons =====
    juce::ImageButton generateBtn, dragBtn, advBtn, polyrBtn, reharmBtn, adjustBtn, diceBtn;

    // ===== Working sets of notes =====
    std::vector<Note> lastMelody;
    std::vector<Note> lastChords;

    // ===== Advanced/Reharm options kept in editor and passed to generator =====
    AdvancedHarmonyOptions advOptions;

    // ===== Drag-temp =====
    bool        wantingDrag = false;
    juce::File  lastTempMidi;

    // ===== Logic =====
    void onEngineChanged();
    void randomizeAll();
    void performDragExport();
    void pushSettingsToGenerator();
    void regenerate();

    // Handlers
    void comboBoxChanged(juce::ComboBox* box) override;
    void sliderValueChanged(juce::Slider* s) override;

    // Asset helpers
    juce::Image loadEmbeddedLogoAny();
    juce::Image loadImageAny(std::initializer_list<const char*> baseNames);
    juce::File  writeTempMidiForDrag();

    // Dialog launchers
    void openAdvanced();
    void openPolyrhythm();
    void openReharmonize();
    void openAdjust();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BANGAudioProcessorEditor)
};
