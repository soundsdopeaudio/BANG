#include "AdvancedHarmonyWindow.h"

static void styleLabel(juce::Label& l)
{
    l.setJustificationType(juce::Justification::centredLeft);
    l.setColour(juce::Label::textColourId, juce::Colours::white);
}

AdvancedHarmonyWindow::AdvancedHarmonyWindow(AdvancedHarmonyOptions& opts)
    : options(opts)
{
    // Extensions
    addAndMakeVisible(extButton); extButton.setToggleState(options.useExtensions, juce::dontSendNotification);
    extButton.onClick = [this] { options.useExtensions = extButton.getToggleState(); };

    for (auto* tb : { &ext7, &ext9, &ext11, &ext13, &sus2, &sus4, &altered })
        addAndMakeVisible(*tb);

    ext7.setToggleState(options.ext7, juce::dontSendNotification);
    ext9.setToggleState(options.ext9, juce::dontSendNotification);
    ext11.setToggleState(options.ext11, juce::dontSendNotification);
    ext13.setToggleState(options.ext13, juce::dontSendNotification);
    sus2.setToggleState(options.sus2, juce::dontSendNotification);
    sus4.setToggleState(options.sus4, juce::dontSendNotification);
    altered.setToggleState(options.altered, juce::dontSendNotification);

    addAndMakeVisible(extDensity); addAndMakeVisible(extDensityLabel); styleLabel(extDensityLabel);
    extDensity.setRange(0.0, 1.0, 0.01); extDensity.setValue(options.extDensity);
    extDensity.onValueChange = [this] { options.extDensity = extDensity.getValue(); };

    // Other spice
    addAndMakeVisible(secDom);  secDom.setToggleState(options.secondaryDominants, juce::dontSendNotification);
    addAndMakeVisible(secDomDen); addAndMakeVisible(secDomLbl); styleLabel(secDomLbl);
    secDomDen.setRange(0.0, 1.0, 0.01); secDomDen.setValue(options.secDomDensity);
    secDomDen.onValueChange = [this] { options.secDomDensity = secDomDen.getValue(); };
    secDom.onClick = [this] { options.secondaryDominants = secDom.getToggleState(); };

    addAndMakeVisible(borrow);  borrow.setToggleState(options.borrowedChords, juce::dontSendNotification);
    addAndMakeVisible(borrowDen); addAndMakeVisible(borrowLbl); styleLabel(borrowLbl);
    borrowDen.setRange(0.0, 1.0, 0.01); borrowDen.setValue(options.borrowedDensity);
    borrowDen.onValueChange = [this] { options.borrowedDensity = borrowDen.getValue(); };
    borrow.onClick = [this] { options.borrowedChords = borrow.getToggleState(); };

    addAndMakeVisible(chromMed); chromMed.setToggleState(options.chromaticMediants, juce::dontSendNotification);
    addAndMakeVisible(chromDen); addAndMakeVisible(chromLbl); styleLabel(chromLbl);
    chromDen.setRange(0.0, 1.0, 0.01); chromDen.setValue(options.chromDensity);
    chromDen.onValueChange = [this] { options.chromDensity = chromDen.getValue(); };
    chromMed.onClick = [this] { options.chromaticMediants = chromMed.getToggleState(); };

    addAndMakeVisible(neapol); neapol.setToggleState(options.neapolitan, juce::dontSendNotification);
    addAndMakeVisible(neapolDen); addAndMakeVisible(neapolLbl); styleLabel(neapolLbl);
    neapolDen.setRange(0.0, 1.0, 0.01); neapolDen.setValue(options.neapolitanDensity);
    neapolDen.onValueChange = [this] { options.neapolitanDensity = neapolDen.getValue(); };
    neapol.onClick = [this] { options.neapolitan = neapol.getToggleState(); };

    // NEW: Tritone substitution
    addAndMakeVisible(tritoneBtn); tritoneBtn.setToggleState(options.tritoneSubs, juce::dontSendNotification);
    tritoneBtn.onClick = [this] { options.tritoneSubs = tritoneBtn.getToggleState(); };
    addAndMakeVisible(tritoneDen); addAndMakeVisible(tritoneLbl); styleLabel(tritoneLbl);
    tritoneDen.setRange(0.0, 1.0, 0.01); tritoneDen.setValue(options.tritoneDensity);
    tritoneDen.onValueChange = [this] { options.tritoneDensity = tritoneDen.getValue(); };

    setSize(480, 420);
}

void AdvancedHarmonyWindow::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(0xA5, 0xDD, 0x00));
    g.setColour(juce::Colours::black);
    g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    g.drawText("Advanced Harmony", getLocalBounds().removeFromTop(28), juce::Justification::centred);
}

void AdvancedHarmonyWindow::resized()
{
    auto r = getLocalBounds().reduced(12);

    auto row = [&](juce::Component& a, juce::Component& b, int h = 24)
    {
        auto rr = r.removeFromTop(h); a.setBounds(rr.removeFromLeft(200).reduced(2));
        b.setBounds(rr.reduced(2));
        r.removeFromTop(6);
    };
    auto rowOne = [&](juce::Component& a, int h = 24)
    {
        a.setBounds(r.removeFromTop(h).reduced(2));
        r.removeFromTop(6);
    };

    rowOne(extButton, 26);
    for (auto* tb : { &ext7,&ext9,&ext11,&ext13,&sus2,&sus4,&altered })
        rowOne(*tb, 22);
    row(*static_cast<juce::Component*>(&extDensityLabel), *static_cast<juce::Component*>(&extDensity));

    rowOne(secDom, 24); row(secDomLbl, secDomDen);
    rowOne(borrow, 24); row(borrowLbl, borrowDen);
    rowOne(chromMed, 24); row(chromLbl, chromDen);
    rowOne(neapol, 24); row(neapolLbl, neapolDen);

    rowOne(tritoneBtn, 24); row(tritoneLbl, tritoneDen);
}
