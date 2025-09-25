#include "AdvancedHarmonyWindow.h"
#include "PluginProcessor.h"

// ======================================================================
// Resource helpers (same searching approach you use elsewhere)
juce::File AdvancedHarmonyWindow::getResourceFile(const juce::String& resourceName)
{
    // 1) next to the plugin
    auto app = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
    auto here = app.getParentDirectory();
#if JUCE_MAC
    // walk up out of .vst/.component bundle
    for (int i = 0; i < 5; ++i) here = here.getParentDirectory();
#endif
    auto try1 = here.getChildFile("Resources").getChildFile(resourceName);
    if (try1.existsAsFile()) return try1;

    // 2) current working dir /Resources
    auto try2 = juce::File::getCurrentWorkingDirectory().getChildFile("Resources").getChildFile(resourceName);
    if (try2.existsAsFile()) return try2;

    // 3) working dir directly
    auto try3 = juce::File::getCurrentWorkingDirectory().getChildFile(resourceName);
    return try3;
}

juce::Image AdvancedHarmonyWindow::loadImageByHint(const juce::String& base)
{
    // Try the common naming patterns you’ve been using
    const juce::StringArray candidates {
        base + ".png",
            base + "_normal.png",
            base + "_over.png",
            base + "_down.png",
            base + "_lbl.png",
            base + "Lbl.png"
    };

    for (auto& name : candidates)
        if (auto f = getResourceFile(name); f.existsAsFile())
            if (auto img = juce::ImageFileFormat::loadFrom(f); img.isValid())
                return img;

    return {};
}

void AdvancedHarmonyWindow::setImageButton3(juce::ImageButton& btn, const juce::String& baseHint)
{
    // normal / over / down
    auto n = loadImageByHint(baseHint);                // e.g. diceBtn.png  or diceBtn_normal.png
    auto o = loadImageByHint(baseHint + "_over");      // e.g. diceBtn_over.png
    auto d = loadImageByHint(baseHint + "_down");      // e.g. diceBtn_down.png

    if (!o.isValid()) o = n;
    if (!d.isValid()) d = n;

    btn.setImages(
        /*resizeNow*/ true,
        /*preserveProportions*/ true,
        /*doToggle*/ false,
        /*normal*/ n, 1.0f, juce::Colours::transparentBlack,
        /*over  */ o, 1.0f, juce::Colours::transparentBlack,
        /*down  */ d, 1.0f, juce::Colours::transparentBlack
    );
}

// ======================================================================
// APVTS setters (automation-friendly)
void AdvancedHarmonyWindow::setBoolParam(const juce::String& paramID, bool on)
{
    if (auto* rp = processor.apvts.getParameter(paramID))
        if (auto* pb = dynamic_cast<juce::AudioParameterBool*>(rp))
        {
            pb->beginChangeGesture();
            pb->setValueNotifyingHost(on ? 1.0f : 0.0f);
            pb->endChangeGesture();
        }
}

void AdvancedHarmonyWindow::setFloatParam01to100(const juce::String& paramID, float valuePercent)
{
    if (auto* rp = processor.apvts.getParameter(paramID))
        if (auto* pf = dynamic_cast<juce::AudioParameterFloat*>(rp))
        {
            const float clamped = juce::jlimit(0.0f, 100.0f, valuePercent);
            const float norm = pf->convertTo0to1(clamped);
            pf->beginChangeGesture();
            pf->setValueNotifyingHost(norm);
            pf->endChangeGesture();
        }
}

// ======================================================================

AdvancedHarmonyWindow::AdvancedHarmonyWindow(BANGAudioProcessor& p)
    : processor(p)
{
    setOpaque(false);

    // ===== IMAGE LABELS (no text labels anywhere) ========================
    if (auto img = loadImageByHint("advancedHarmonyLbl"); img.isValid()) titleImg.setImage(img);
    titleImg.setImagePlacement(juce::RectanglePlacement::centred);
    addAndMakeVisible(titleImg);

    if (auto img = loadImageByHint("extensionsLbl"); img.isValid()) extImg.setImage(img);
    extImg.setImagePlacement(juce::RectanglePlacement::centred);
    addAndMakeVisible(extImg);

    if (auto img = loadImageByHint("advancedChordsLbl"); img.isValid()) advImg.setImage(img);
    advImg.setImagePlacement(juce::RectanglePlacement::centred);
    addAndMakeVisible(advImg);

    // ===== DICE (ImageButton; same style as the rest of the app) =========
    addAndMakeVisible(diceBtn);
    setImageButton3(diceBtn, "diceBtn"); // will use diceBtn.png / _over / _down
    diceBtn.onClick = [this]
    {
        juce::Random rng{ juce::Time::getMillisecondCounter() };

        // Randomize Extensions / Other
        setBoolParam("ext7", rng.nextBool());
        setBoolParam("ext9", rng.nextBool());
        setBoolParam("ext11", rng.nextBool());
        setBoolParam("ext13", rng.nextBool());
        setBoolParam("sus24", rng.nextBool());
        setBoolParam("alt", rng.nextBool());
        setBoolParam("slash", rng.nextBool());
        setFloatParam01to100("extDensity", 10.0f + rng.nextFloat() * 80.0f); // 10..90%

        // Randomize Advanced (generator enforces “1 each” vs “pick 2 at random” rule)
        setBoolParam("advSecDom", rng.nextBool());
        setBoolParam("advBorrowed", rng.nextBool());
        setBoolParam("advChromMed", rng.nextBool());
        setBoolParam("advNeapolitan", rng.nextBool());
        setBoolParam("advTritone", rng.nextBool());
    };

    // ===== CONTROLS (your exact names) ===================================
    for (auto* b : { &checkbox7ths, &checkbox9ths, &checkbox11ths, &checkbox13ths,
                     &checkboxSus, &checkboxAlt, &checkboxSlash,
                     &secDomCheck, &borrowCheck, &chromMedCheck, &neapolCheck, &tritoneCheck })
    {
        b->setClickingTogglesState(true);
        addAndMakeVisible(*b);
    }

    densitySlider.setRange(0.0, 100.0, 0.01);
    densitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    densitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    densitySlider.setTextValueSuffix("%");
    addAndMakeVisible(densitySlider);

    // ===== APVTS attachments ============================================
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;

    att7 = std::make_unique<BA>(processor.apvts, "ext7", checkbox7ths);
    att9 = std::make_unique<BA>(processor.apvts, "ext9", checkbox9ths);
    att11 = std::make_unique<BA>(processor.apvts, "ext11", checkbox11ths);
    att13 = std::make_unique<BA>(processor.apvts, "ext13", checkbox13ths);
    attSus = std::make_unique<BA>(processor.apvts, "sus24", checkboxSus);
    attAlt = std::make_unique<BA>(processor.apvts, "alt", checkboxAlt);
    attSlash = std::make_unique<BA>(processor.apvts, "slash", checkboxSlash);
    attDensity = std::make_unique<SA>(processor.apvts, "extDensity", densitySlider);

    attSecDom = std::make_unique<BA>(processor.apvts, "advSecDom", secDomCheck);
    attBorrowed = std::make_unique<BA>(processor.apvts, "advBorrowed", borrowCheck);
    attChromMed = std::make_unique<BA>(processor.apvts, "advChromMed", chromMedCheck);
    attNeapol = std::make_unique<BA>(processor.apvts, "advNeapolitan", neapolCheck);
    attTritone = std::make_unique<BA>(processor.apvts, "advTritone", tritoneCheck);
}

void AdvancedHarmonyWindow::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::transparentBlack);
}

void AdvancedHarmonyWindow::resized()
{
    auto r = getLocalBounds().reduced(16);

    // Top row: title image left, dice button right
    auto top = r.removeFromTop(36);
    auto diceArea = top.removeFromRight(36);
    diceBtn.setBounds(diceArea.withSizeKeepingCentre(28, 28));

    // Title image (width depends on your PNG; give it some room)
    titleImg.setBounds(top.removeFromLeft(340)); // adjust width as needed

    r.removeFromTop(10);

    // === Extensions section (image header + grid)
    extImg.setBounds(r.removeFromTop(24).removeFromLeft(260)); // header image
    auto extArea = r.removeFromTop(114);
    {
        auto row1 = extArea.removeFromTop(28);
        checkbox7ths.setBounds(row1.removeFromLeft(140));
        checkbox9ths.setBounds(row1.removeFromLeft(140));
        checkbox11ths.setBounds(row1.removeFromLeft(140));
        checkbox13ths.setBounds(row1);

        auto row2 = extArea.removeFromTop(28);
        checkboxSus.setBounds(row2.removeFromLeft(140));
        checkboxAlt.setBounds(row2.removeFromLeft(140));
        checkboxSlash.setBounds(row2.removeFromLeft(140));

        auto row3 = extArea.removeFromTop(30);
        row3.removeFromLeft(80);          // left padding (your image mockup has the label baked-in)
        densitySlider.setBounds(row3.reduced(0, 2));
    }

    r.removeFromTop(8);

    // === Advanced section (image header + grid)
    advImg.setBounds(r.removeFromTop(24).removeFromLeft(240)); // header image
    auto advArea = r.removeFromTop(90);
    {
        auto row1 = advArea.removeFromTop(28);
        secDomCheck.setBounds(row1.removeFromLeft(220));
        borrowCheck.setBounds(row1.removeFromLeft(220));
        chromMedCheck.setBounds(row1.removeFromLeft(220));

        auto row2 = advArea.removeFromTop(28);
        neapolCheck.setBounds(row2.removeFromLeft(220));
        tritoneCheck.setBounds(row2.removeFromLeft(220));
    }
}