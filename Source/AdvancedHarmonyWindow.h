#pragma once
#include <JuceHeader.h>

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

    juce::ImageComponent advancedLblImg;

    juce::ImageComponent enableLblImg;

    juce::ToggleButton extButton  {};
    juce::ToggleButton ext7       {"7th"};
    juce::ToggleButton ext9       {"9th"};
    juce::ToggleButton ext11      {"11th"};
    juce::ToggleButton ext13      {"13th"};
    juce::ToggleButton sus2       {"Sus2"};
    juce::ToggleButton sus4       {"Sus4"};
    juce::ToggleButton altered    {"Altered"};
    juce::ToggleButton slash      {"Slash"};

    juce::Slider      extDensity;
    juce::Label       extDensityLabel { {}, "Extensions/Other Chords"};

    juce::ToggleButton secDom      {"Secondary Dominants"};
    juce::Slider      secDomDen;   juce::Label secDomLbl   { {}, "SecDom Density" };

    juce::ToggleButton borrow      {"Borrowed Chords"};
    juce::Slider      borrowDen;   juce::Label borrowLbl   { {}, "Borrow Density" };

    juce::ToggleButton chromMed    {"Chromatic Mediants"};
    juce::Slider      chromDen;    juce::Label chromLbl    { {}, "Chrom Density" };

    juce::ToggleButton neapol      {"Neapolitan"};
    juce::Slider      neapolDen;   juce::Label neapolLbl   { {}, "Neapolitan Density" };

    // NEW controls
    juce::ToggleButton tritoneBtn  {"Enable Tritone Subs"};
    juce::Slider      tritoneDen;
    juce::Label       tritoneLbl   { {}, "Tritone Density" };
};
