#pragma once
#include <JuceHeader.h>
#include "BangToggleLookAndFeel.h"

// inside class AdvancedHarmonyWindow (private:)

// Options shared with MidiGenerator
struct AdvancedHarmonyOptions
{
    // Existing toggles (kept so nothing else breaks)
    bool useExtensions = true;
    bool ext7 = true;
    bool ext9 = true;
    bool ext11 = false;
    bool ext13 = false;
    bool sus2 = false;
    bool sus4 = false;
    bool altered = false;
    double extDensity = 0.5; // 0..1

    bool  secondaryDominants = false; double secDomDensity = 0.25;
    bool  borrowedChords = false;     double borrowedDensity = 0.20;
    bool  chromaticMediants = false;  double chromDensity = 0.15;
    bool  neapolitan = false;         double neapolitanDensity = 0.15;

    // NEW: Tritone substitutions
    bool   tritoneSubs = false;
    double tritoneDensity = 0.35; // probability per eligible dominant
};

class AdvancedHarmonyWindow : public juce::Component
{
public:
    explicit AdvancedHarmonyWindow(AdvancedHarmonyOptions& opts);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AdvancedHarmonyOptions& options;

    juce::ImageButton diceButton;

    BangToggleLookAndFeel bangToggleLAF;

    juce::ImageComponent advancedLbl;
    juce::ImageComponent enableLbl;

    juce::ImageComponent label7ths;
    juce::ImageComponent label9ths;
    juce::ImageComponent label11ths;
    juce::ImageComponent label13ths;
    juce::ImageComponent labelSus;
    juce::ImageComponent labelAlt;
    juce::ImageComponent labelSlash;

    juce::ToggleButton checkbox7ths;
    juce::ToggleButton checkbox9ths;
    juce::ToggleButton checkbox11ths;
    juce::ToggleButton checkbox13ths;
    juce::ToggleButton checkboxSus;
    juce::ToggleButton checkboxAlt;
    juce::ToggleButton checkboxSlash;

    juce::ImageComponent densityLbl;
    juce::Slider densitySlider;

    juce::ImageComponent advancedChordsLbl;

    // Part 2: Advanced Chord Controls

    // Secondary Dominants
    juce::ImageComponent secondaryDomsLbl;
    juce::ToggleButton secDomCheck;
    juce::ImageComponent secDomDensityLbl;
    juce::Slider secDomDen;

    // Borrowed Chords
    juce::ImageComponent borrowedChordsLbl;
    juce::ToggleButton borrowCheck;
    juce::ImageComponent borrowDensityLbl;
    juce::Slider borrowDen;

    // Chromatic Mediants
    juce::ImageComponent chromaticMediantsLbl;
    juce::ToggleButton chromMedCheck;
    juce::ImageComponent chromMedDensityLbl;
    juce::Slider chromDen;

    // Neapolitan Chords
    juce::ImageComponent neapolitanChordsLbl;
    juce::ToggleButton neapolCheck;
    juce::ImageComponent neapolDensityLbl;
    juce::Slider neapolDen;

    // Tritone Subs
    juce::ImageComponent tritoneSubsLbl;
    juce::ToggleButton tritoneCheck;
    juce::ImageComponent tritoneDensityLbl;
    juce::Slider tritoneDen;
};
