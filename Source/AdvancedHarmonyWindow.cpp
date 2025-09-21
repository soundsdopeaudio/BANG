#include "AdvancedHarmonyWindow.h"

// NOTE: We are assuming that the image assets have been added to the project
// and are available in the BinaryData namespace. This is a standard JUCE practice.
// The names might need adjustment if the Projucer used different identifiers.
#if !JucePlugin_Build_VST
#include "BinaryData.h"
#endif

static void styleLabel(juce::Label& l)
{
    l.setJustificationType(juce::Justification::centredLeft);
    l.setColour(juce::Label::textColourId, juce::Colours::black);
    l.setFont(juce::Font(16.0f, juce::Font::bold));
}

AdvancedHarmonyWindow::AdvancedHarmonyWindow(AdvancedHarmonyOptions& opts)
    : options(opts)
{
    // Dice Button
    auto diceImage = juce::ImageCache::getFromMemory(BinaryData::diceBtn_png, BinaryData::diceBtn_pngSize);
    diceButton.setImages(true, true, true,
        diceImage, 1.0f, juce::Colours::transparentBlack,
        diceImage, 1.0f, juce::Colours::transparentBlack,
        diceImage, 0.5f, juce::Colours::transparentBlack,
        0);
    addAndMakeVisible(diceButton);

    // Main Labels
    advancedLbl.setImage(juce::ImageCache::getFromMemory(BinaryData::advancedLbl_png, BinaryData::advancedLbl_pngSize));
    addAndMakeVisible(advancedLbl);
    enableLbl.setImage(juce::ImageCache::getFromMemory(BinaryData::enableLbl_png, BinaryData::enableLbl_pngSize));
    addAndMakeVisible(enableLbl);

    // Checkbox Labels
    label7ths.setImage(juce::ImageCache::getFromMemory(BinaryData::_7thsLbl_png, BinaryData::_7thsLbl_pngSize));
    label9ths.setImage(juce::ImageCache::getFromMemory(BinaryData::_9thsLbl_png, BinaryData::_9thsLbl_pngSize));
    label11ths.setImage(juce::ImageCache::getFromMemory(BinaryData::_11thsLbl_png, BinaryData::_11thsLbl_pngSize));
    label13ths.setImage(juce::ImageCache::getFromMemory(BinaryData::_13thsLbl_png, BinaryData::_13thsLbl_pngSize));
    labelSus.setImage(juce::ImageCache::getFromMemory(BinaryData::susLbl_png, BinaryData::susLbl_pngSize));
    labelAlt.setImage(juce::ImageCache::getFromMemory(BinaryData::altLbl_png, BinaryData::altLbl_pngSize));
    labelSlash.setImage(juce::ImageCache::getFromMemory(BinaryData::slashLbl_png, BinaryData::slashLbl_pngSize));
    for (auto* lbl : { &label7ths, &label9ths, &label11ths, &label13ths, &labelSus, &labelAlt, &labelSlash })
        addAndMakeVisible(*lbl);

    // Checkboxes
    checkbox7ths.setToggleState(options.ext7, juce::dontSendNotification);
    checkbox9ths.setToggleState(options.ext9, juce::dontSendNotification);
    checkbox11ths.setToggleState(options.ext11, juce::dontSendNotification);
    checkbox13ths.setToggleState(options.ext13, juce::dontSendNotification);
    checkboxSus.setToggleState(options.sus2 || options.sus4, juce::dontSendNotification);
    checkboxAlt.setToggleState(options.altered, juce::dontSendNotification);
    checkboxSlash.setToggleState(false, juce::dontSendNotification); // No option for this yet

    checkbox7ths.onClick = [this] { options.ext7 = checkbox7ths.getToggleState(); };
    checkbox9ths.onClick = [this] { options.ext9 = checkbox9ths.getToggleState(); };
    checkbox11ths.onClick = [this] { options.ext11 = checkbox11ths.getToggleState(); };
    checkbox13ths.onClick = [this] { options.ext13 = checkbox13ths.getToggleState(); };
    checkboxSus.onClick = [this] { options.sus2 = checkboxSus.getToggleState(); options.sus4 = checkboxSus.getToggleState(); };
    checkboxAlt.onClick = [this] { options.altered = checkboxAlt.getToggleState(); };

    for (auto* cb : { &checkbox7ths, &checkbox9ths, &checkbox11ths, &checkbox13ths, &checkboxSus, &checkboxAlt, &checkboxSlash })
        addAndMakeVisible(*cb);

    // Density Slider
    densityLbl.setImage(juce::ImageCache::getFromMemory(BinaryData::densityLbl_png, BinaryData::densityLbl_pngSize));
    addAndMakeVisible(densityLbl);
    addAndMakeVisible(densitySlider);
    densitySlider.setRange(0, 100, 1);
    densitySlider.setValue(options.extDensity * 100.0, juce::dontSendNotification);
    densitySlider.onValueChange = [this] { options.extDensity = densitySlider.getValue() / 100.0; };

    // Advanced Chords Label
    advancedChordsLbl.setImage(juce::ImageCache::getFromMemory(BinaryData::advancedChordsLbl_png, BinaryData::advancedChordsLbl_pngSize));
    addAndMakeVisible(advancedChordsLbl);

    // Part 2 Controls
    auto densityImg = juce::ImageCache::getFromMemory(BinaryData::densityLbl_png, BinaryData::densityLbl_pngSize);

    // Secondary Dominants
    addAndMakeVisible(secondaryDomsLbl); styleLabel(secondaryDomsLbl);
    addAndMakeVisible(secDomCheck);
    secDomCheck.setToggleState(options.secondaryDominants, juce::dontSendNotification);
    secDomCheck.onClick = [this] { options.secondaryDominants = secDomCheck.getToggleState(); };
    addAndMakeVisible(secDomDensityLbl); secDomDensityLbl.setImage(densityImg);
    addAndMakeVisible(secDomDen);
    secDomDen.setRange(0, 100, 1);
    secDomDen.setValue(options.secDomDensity * 100.0, juce::dontSendNotification);
    secDomDen.onValueChange = [this] { options.secDomDensity = secDomDen.getValue() / 100.0; };

    // Borrowed Chords
    borrowedChordsLbl.setImage(juce::ImageCache::getFromMemory(BinaryData::borrowedChordsLbl_png, BinaryData::borrowedChordsLbl_pngSize));
    addAndMakeVisible(borrowedChordsLbl);
    addAndMakeVisible(borrowCheck);
    borrowCheck.setToggleState(options.borrowedChords, juce::dontSendNotification);
    borrowCheck.onClick = [this] { options.borrowedChords = borrowCheck.getToggleState(); };
    addAndMakeVisible(borrowDensityLbl); borrowDensityLbl.setImage(densityImg);
    addAndMakeVisible(borrowDen);
    borrowDen.setRange(0, 100, 1);
    borrowDen.setValue(options.borrowedDensity * 100.0, juce::dontSendNotification);
    borrowDen.onValueChange = [this] { options.borrowedDensity = borrowDen.getValue() / 100.0; };

    // Chromatic Mediants
    chromaticMediantsLbl.setImage(juce::ImageCache::getFromMemory(BinaryData::chromaticMediantsLbl_png, BinaryData::chromaticMediantsLbl_pngSize));
    addAndMakeVisible(chromaticMediantsLbl);
    addAndMakeVisible(chromMedCheck);
    chromMedCheck.setToggleState(options.chromaticMediants, juce::dontSendNotification);
    chromMedCheck.onClick = [this] { options.chromaticMediants = chromMedCheck.getToggleState(); };
    addAndMakeVisible(chromMedDensityLbl); chromMedDensityLbl.setImage(densityImg);
    addAndMakeVisible(chromDen);
    chromDen.setRange(0, 100, 1);
    chromDen.setValue(options.chromDensity * 100.0, juce::dontSendNotification);
    chromDen.onValueChange = [this] { options.chromDensity = chromDen.getValue() / 100.0; };

    // Neapolitan Chords
    neapolitanChordsLbl.setImage(juce::ImageCache::getFromMemory(BinaryData::neopolitanChordsLbl_png, BinaryData::neopolitanChordsLbl_pngSize));
    addAndMakeVisible(neapolitanChordsLbl);
    addAndMakeVisible(neapolCheck);
    neapolCheck.setToggleState(options.neapolitan, juce::dontSendNotification);
    neapolCheck.onClick = [this] { options.neapolitan = neapolCheck.getToggleState(); };
    addAndMakeVisible(neapolDensityLbl); neapolDensityLbl.setImage(densityImg);
    addAndMakeVisible(neapolDen);
    neapolDen.setRange(0, 100, 1);
    neapolDen.setValue(options.neapolitanDensity * 100.0, juce::dontSendNotification);
    neapolDen.onValueChange = [this] { options.neapolitanDensity = neapolDen.getValue() / 100.0; };

    // Tritone Subs
    tritoneSubsLbl.setImage(juce::ImageCache::getFromMemory(BinaryData::tritoneSubsLbl_png, BinaryData::tritoneSubsLbl_pngSize));
    addAndMakeVisible(tritoneSubsLbl);
    addAndMakeVisible(tritoneCheck);
    tritoneCheck.setToggleState(options.tritoneSubs, juce::dontSendNotification);
    tritoneCheck.onClick = [this] { options.tritoneSubs = tritoneCheck.getToggleState(); };
    addAndMakeVisible(tritoneDensityLbl); tritoneDensityLbl.setImage(densityImg);
    addAndMakeVisible(tritoneDen);
    tritoneDen.setRange(0, 100, 1);
    tritoneDen.setValue(options.tritoneDensity * 100.0, juce::dontSendNotification);
    tritoneDen.onValueChange = [this] { options.tritoneDensity = tritoneDen.getValue() / 100.0; };

    setSize(570, 700);

    auto skinToggle = [this](juce::ToggleButton& b)
    {
        b.setLookAndFeel(&bangToggleLAF);
        b.setButtonText({});
        b.setClickingTogglesState(true);
    };

    // Call skinToggle(...) for whichever of these are members of AdvancedHarmonyWindow:
    skinToggle(checkbox7ths);
    skinToggle(checkbox9ths);
    skinToggle(checkbox11ths);
    skinToggle(checkbox13ths);
    skinToggle(checkboxSus);
    skinToggle(checkboxAlt);
    skinToggle(checkboxSlash);
    skinToggle(tritoneCheck);
    skinToggle(neapolCheck);
    skinToggle(chromMedCheck);
    skinToggle(borrowCheck);
    skinToggle(secDomCheck);

}

void AdvancedHarmonyWindow::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(0xA5, 0xDD, 0x00));
}

void AdvancedHarmonyWindow::resized()
{
    // --- keep PNG aspect ratio for height -> width ---
    const auto toggleWidthForHeight = [](int h) -> int
    {
        // load ON image to get the canonical aspect ratio
        static int baseW = 0, baseH = 0;
        if (baseW == 0 || baseH == 0)
        {
            juce::MemoryInputStream mis(BinaryData::toggleBtnOn_png,
                BinaryData::toggleBtnOn_pngSize, false);
            if (auto img = juce::ImageFileFormat::loadFrom(mis); img.isValid())
            {
                baseW = img.getWidth();
                baseH = img.getHeight();
            }
            else
            {
                // sensible fallback ratio if something's wrong with the resource
                baseW = 352;
                baseH = 125;
            }
        }
        return juce::roundToInt((baseW / (float)baseH) * (float)h);
    };

    const int toggleH = 44;
    const int toggleW = juce::roundToInt((352.0f / 125.0f) * toggleH); // preserve art ratio

    // Example row layout
    int y = 150;
    int x = 40;
    const int gapY = 55;

    tritoneCheck.setBounds(x, y, toggleW, toggleH); y += gapY;
    neapolCheck.setBounds(x, y, toggleW, toggleH); y += gapY;
    chromMedCheck.setBounds(x, y, toggleW, toggleH); y += gapY;
    borrowCheck.setBounds(x, y, toggleW, toggleH); y += gapY;
    secDomCheck.setBounds(x, y, toggleW, toggleH); y += gapY;

    // Extensions
    checkbox7ths.setBounds(200, 150, toggleW, toggleH);
    checkbox9ths.setBounds(200, 205, toggleW, toggleH);
    checkbox11ths.setBounds(200, 260, toggleW, toggleH);
    checkbox13ths.setBounds(200, 315, toggleW, toggleH);
    checkboxSus.setBounds(200, 370, toggleW, toggleH);
    checkboxAlt.setBounds(200, 425, toggleW, toggleH);
    checkboxSlash.setBounds(200, 480, toggleW, toggleH);

    auto bounds = getLocalBounds().reduced(10);

    // Top row: Dice button on the right
    auto topRow = bounds.removeFromTop(40);
    diceButton.setBounds(topRow.removeFromRight(40));

    bounds.removeFromTop(10);

    // Advanced Harmony Label
    advancedLbl.setBounds(bounds.removeFromTop(60).reduced(5));

    bounds.removeFromTop(10);

    // Enable Label
    enableLbl.setBounds(bounds.removeFromTop(30).reduced(5));

    bounds.removeFromTop(10);

    // Checkboxes row
    auto checkboxRowBounds = bounds.removeFromTop(30);
    juce::FlexBox flex;
    flex.flexDirection = juce::FlexBox::Direction::row;
    flex.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    flex.alignItems = juce::FlexBox::AlignItems::center;

    flex.items.add(juce::FlexItem(label7ths).withFlex(2));
    flex.items.add(juce::FlexItem(checkbox7ths).withFlex(1));
    flex.items.add(juce::FlexItem(label9ths).withFlex(2));
    flex.items.add(juce::FlexItem(checkbox9ths).withFlex(1));
    flex.items.add(juce::FlexItem(label11ths).withFlex(2));
    flex.items.add(juce::FlexItem(checkbox11ths).withFlex(1));
    flex.items.add(juce::FlexItem(label13ths).withFlex(2));
    flex.items.add(juce::FlexItem(checkbox13ths).withFlex(1));
    flex.items.add(juce::FlexItem(labelSus).withFlex(2));
    flex.items.add(juce::FlexItem(checkboxSus).withFlex(1));
    flex.items.add(juce::FlexItem(labelAlt).withFlex(2));
    flex.items.add(juce::FlexItem(checkboxAlt).withFlex(1));
    flex.items.add(juce::FlexItem(labelSlash).withFlex(2));
    flex.items.add(juce::FlexItem(checkboxSlash).withFlex(1));
    flex.performLayout(checkboxRowBounds);

    bounds.removeFromTop(10);

    // Density Slider row
    auto densityRowBounds = bounds.removeFromTop(30);
    juce::FlexBox densityFlex;
    densityFlex.flexDirection = juce::FlexBox::Direction::row;
    densityFlex.alignItems = juce::FlexBox::AlignItems::center;
    densityFlex.items.add(juce::FlexItem(densityLbl).withWidth(180));
    densityFlex.items.add(juce::FlexItem(densitySlider).withFlex(1.0f));
    densityFlex.performLayout(densityRowBounds);

    bounds.removeFromTop(20);

    // Advanced Chords Label
    advancedChordsLbl.setBounds(bounds.removeFromTop(40).reduced(5));

    bounds.removeFromTop(10);

    // Part 2 Layout
    auto createRow = [&](juce::Component& mainLbl, juce::Component& check, juce::Component& densityLbl, juce::Slider& slider)
    {
        auto rowBounds = bounds.removeFromTop(25);
        juce::FlexBox mainFlex;
        mainFlex.alignItems = juce::FlexBox::AlignItems::center;
        mainFlex.items.add(juce::FlexItem(mainLbl).withWidth(200));
        mainFlex.items.add(juce::FlexItem(check).withWidth(30));
        mainFlex.performLayout(rowBounds);
        
        auto densityRowBounds = bounds.removeFromTop(25);
        juce::FlexBox densityFlex;
        densityFlex.alignItems = juce::FlexBox::AlignItems::center;
        densityFlex.items.add(juce::FlexItem(densityLbl).withWidth(100));
        densityFlex.items.add(juce::FlexItem(slider).withFlex(1.0f));
        densityFlex.performLayout(densityRowBounds.reduced(0, 5));
        bounds.removeFromTop(10);
    };

    createRow(secondaryDomsLbl, secDomCheck, secDomDensityLbl, secDomDen);
    createRow(borrowedChordsLbl, borrowCheck, borrowDensityLbl, borrowDen);
    createRow(chromaticMediantsLbl, chromMedCheck, chromMedDensityLbl, chromDen);
    createRow(neapolitanChordsLbl, neapolCheck, neapolDensityLbl, neapolDen);
    createRow(tritoneSubsLbl, tritoneCheck, tritoneDensityLbl, tritoneDen);
}
