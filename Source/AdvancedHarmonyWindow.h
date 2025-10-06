#pragma once
#include <JuceHeader.h>
#include "BangSliderLookAndFeel.h"

class BANGAudioProcessor; // forward declare

class AdvancedHarmonyWindow : public juce::Component
{
public:
    explicit AdvancedHarmonyWindow(BANGAudioProcessor& processor);

    void paint(juce::Graphics&) override;
    void resized() override;
    ~AdvancedHarmonyWindow() override;

private:
    BANGAudioProcessor& processor;

    // ===== IMAGE LABELS (PNG assets in your Resources) ====================
    juce::ImageComponent titleImg; // advancedHarmonyLbl.png
    juce::ImageComponent extImg;   // extensionsLbl.png
    juce::ImageComponent advImg;   // advancedChordsLbl.png

    juce::ImageComponent sevethsLbl, ninthsLbl, eleventhsLbl, thirteenthsLbl, susLbl, altLbl, slashLbl;
    juce::ImageComponent secDomLbl, borrowedLbl, chromMedLbl, neapolLbl, tritoneLbl;


    // ===== DICE BUTTON (IMAGE BUTTON, like the rest of your app) =========
    juce::ImageButton diceBtn;
    juce::ImageButton enablePassBtn;
    juce::ImageComponent densityLbl;

public:
    // ===== YOUR CONTROL NAMES (do not rename) ============================
    juce::ToggleButton checkbox7ths;
    juce::ToggleButton checkbox9ths;
    juce::ToggleButton checkbox11ths;
    juce::ToggleButton checkbox13ths;

    juce::ToggleButton checkboxSus;    // Sus2/4
    juce::ToggleButton checkboxAlt;    // Altered
    juce::ToggleButton checkboxSlash;  // Slash chords

    juce::Slider       densitySlider;  // 0..100%

    juce::ToggleButton secDomCheck;
    juce::ToggleButton borrowCheck;
    juce::ToggleButton chromMedCheck;
    juce::ToggleButton neapolCheck;
    juce::ToggleButton tritoneCheck;

    BangSliderLookAndFeel densitySliderLAF;



private:
    // ===== APVTS attachments =============================================
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    // --- BEGIN: commit/revert guard + snapshot of APVTS values ---
    struct AdvSnapshot
    {
        bool ext7 = false, ext9 = false, ext11 = false, ext13 = false;
        bool sus24 = false, alt = false, slash = false;
        float extDensity01to100 = 25.0f;

        bool advSecDom = false, advBorrowed = false, advChromMed = false;
        bool advNeapolitan = false, advTritone = false;
    };

    AdvSnapshot originalSnapshot_;
    bool committed_ = false; // set to true only when the user presses Enable

    // Read current APVTS values into a snapshot
    AdvSnapshot captureFromApvts() const;

    // Write a snapshot back into the APVTS (will drive the attachments)
    void applyToApvts(const AdvSnapshot& s);
    // --- END: commit/revert guard + snapshot ---



    std::unique_ptr<ButtonAttachment> att7, att9, att11, att13;
    std::unique_ptr<ButtonAttachment> attSus, attAlt, attSlash;
    std::unique_ptr<SliderAttachment> attDensity;

    std::unique_ptr<ButtonAttachment> attSecDom, attBorrowed, attChromMed, attNeapol, attTritone;

    // ===== local resource helpers (standalone; no editor header needed) ===
    static juce::File  getResourceFile(const juce::String& name);
    static juce::Image loadImageByHint(const juce::String& base); // tries base.png, base_normal.png etc.
    static void        setImageButton3(juce::ImageButton& btn, const juce::String& baseHint);

    // host-automation friendly param setters
    void setBoolParam(const juce::String& paramID, bool on);
    void setFloatParam01to100(const juce::String& paramID, float valuePercent);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedHarmonyWindow)
};