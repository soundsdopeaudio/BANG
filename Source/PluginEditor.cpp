#include "PluginEditor.h"
#include "MidiGenerator.h"
#include "AdvancedHarmonyWindow.h"

// #include "BinaryData.h" // No longer needed, loading from filesystem.

using juce::Rectangle;
using juce::RectanglePlacement;

// --- BEGIN: helper to convert key ComboBox selection to semitone (C=0..B=11)
static int rootBoxToSemitone(const juce::ComboBox& keyBox)
{
    // Works with either displayed text or selectedId (1..12). Adjust to your combobox setup.
    const juce::String txt = keyBox.getText().trim();

    if (txt.isNotEmpty())
    {
        // Accept both unicode sharp ♯ and ASCII '#'
        if (txt == "C") return 0;
        else if (txt == "C#" || txt == u8"C♯") return 1;
        else if (txt == "D") return 2;
        else if (txt == "D#" || txt == u8"D♯") return 3;
        else if (txt == "E") return 4;
        else if (txt == "F") return 5;
        else if (txt == "F#" || txt == u8"F♯") return 6;
        else if (txt == "G") return 7;
        else if (txt == "G#" || txt == u8"G♯") return 8;
        else if (txt == "A") return 9;
        else if (txt == "A#" || txt == u8"A♯") return 10;
        else if (txt == "B") return 11;
    }

    // If you set selectedId = 1..12 in order C..B, use that as fallback:
    const int id = keyBox.getSelectedId(); // 1..12 expected
    if (id >= 1 && id <= 12)
        return id - 1;

    return 0; // default to C
}
// --- END: helper

// Helper to find the 'Resources' directory.
// It checks relative to the plugin binary and the current working directory.
static juce::File getResourceFile(const juce::String& resourceName)
{
    // This is a common pattern for JUCE plugins.
    // On macOS, the bundle is a directory, so we go up a few levels.
    auto resourcesDir = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                                .getParentDirectory();

#if JUCE_MAC
    resourcesDir = resourcesDir.getParentDirectory().getParentDirectory().getChildFile("Resources");
#else
    resourcesDir = resourcesDir.getChildFile("Resources");
#endif


    // Fallback for development: check relative to current working directory
    if (!resourcesDir.isDirectory())
    {
        resourcesDir = juce::File::getCurrentWorkingDirectory().getChildFile("Resources");
    }

    return resourcesDir.getChildFile(resourceName);
}




// ===== set exactly 3 images on an ImageButton (normal/over/down) =====
// This wrapper calls the correct JUCE signature for setImages.
static void setImageButton3(juce::ImageButton& b,
    const juce::Image& normal,
    const juce::Image& over,
    const juce::Image& down)
{
    b.setImages(
        /*resizeNow*/ true,
        /*preserveProportions*/ true,
        /*doToggle*/ false,
        /*normal*/ normal, 1.0f, juce::Colours::transparentBlack,
        /*over  */ over, 1.0f, juce::Colours::transparentBlack,
        /*down  */ down, 1.0f, juce::Colours::transparentBlack
    );
}



// ======== Filesystem image helpers ========

// fuzzy lookup: tries <hint>.png, <hint>_hover.png, etc. by scanning the Resources directory.


// ======== Editor ========

BANGAudioProcessorEditor::BANGAudioProcessorEditor(BANGAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{

    // ---------- Window size (matches the mock-up proportions) ---------
    setResizable(false, false);
    setSize(980, 760); // width x height

    // ---------- Load images (NO SCALING, keep pixel-native size) ----------
    auto img = loadBinaryImage("selectEngineLbl"); { imgSelectEngine.setImage(img, RectanglePlacement::doNotResize); addAndMakeVisible(imgSelectEngine); }
    img = loadBinaryImage("toolTipsLbl"); { imgToolTips.setImage(img, RectanglePlacement::doNotResize); addAndMakeVisible(imgToolTips); }
    img = loadBinaryImage("octaveLbl"); { imgOctaveLbl.setImage(img, RectanglePlacement::doNotResize); addAndMakeVisible(imgOctaveLbl); }
    img = loadBinaryImage("soundsDopeLbl"); { imgSoundsDope.setImage(img, RectanglePlacement::doNotResize); addAndMakeVisible(imgSoundsDope); }


    imgSelectEngine.setInterceptsMouseClicks(false, false);
    imgToolTips.setInterceptsMouseClicks(false, false);
    imgOctaveLbl.setInterceptsMouseClicks(false, false);
    imgSoundsDope.setInterceptsMouseClicks(false, false);

    // ---------- Engine buttons ----------
    auto stylizeButton = [](juce::ImageButton& b)
    {
        b.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
        b.setButtonText(b.getButtonText().toUpperCase());
        b.setClickingTogglesState(false);
    };

    for (auto* b : { &engineMelodyBtn, &engineChordsBtn, &engineMixtureBtn, &advancedBtn, &polyrBtn, &reharmBtn, &adjustBtn, &generateBtn, &dragBtn })
    {
        stylizeButton(*b); addAndMakeVisible(*b);
    }

    // ---------- Left column selectors ----------
    addAndMakeVisible(keyBox);
    addAndMakeVisible(scaleBox);
    addAndMakeVisible(tsBox);
    addAndMakeVisible(barsBox);
    addAndMakeVisible(restSl);
    addAndMakeVisible(restDensityValue);
    addAndMakeVisible(octaveBox);

    keyBox.addItemList(juce::StringArray { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" }, 1);
    scaleBox.addItemList(juce::StringArray { "Major", "Natural Minor", "Harmonic Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian", "Locrian",
        "Locrian Nat6", "Ionian #5", "Dorian #4", "Phrygian Dom", "Lydian #2", "Super Locrian", "Dorian b2",
        "Lydian Aug", "Lydian Dom", "Mixo b6", "Locrian #2", "Ethiopian Min", "8 Tone Spanish", "Phrygian Nat3",
        "Blues", "Hungarian Min", "Harmonic Maj(Ethiopian)", "Dorian b5", "Phrygian b4", "Lydian b3", "Mixolydian b2",
        "Lydian Aug2", "Locrian bb7", "Pentatonic Maj", "Pentatonic Min", "Neopolitan Maj",
        "Neopolitan Min", "Spanish Gypsy", "Romanian Minor", "Chromatic", "Bebop Major", "Bebop Minor" }, 1);
    tsBox.addItemList(juce::StringArray { "2/4", "3/4", "4/4", "5/4", "7/4", "9/4",
        "5/8", "6/8", "7/8", "9/8", "10/8", "11/8",
        "12/8", "13/8", "15/8", "17/8", "19/8", "21/8",
        "5/16", "7/16", "9/16", "11/16", "13/16", "15/16", "17/16", "19/16" 
    "3+2/8", "2+3/8",
    "2+2+3/8", "3+2+2/8", "2+3+2/8",
    "3+3+2/8", "3+2+3/8", "2+3+3/8"
    "4+3/8", "3+4/8"
    "3+2+2+3/8" }, 3);
    barsBox.addItemList(juce::StringArray { "4", "8" }, 1);
    octaveBox.addItemList(juce::StringArray { "C0", "C1", "C2", "C3", "C4", "C5" }, 4);
    keyBox.setSelectedItemIndex(0, juce::dontSendNotification);
    scaleBox.setSelectedItemIndex(0, juce::dontSendNotification);
    tsBox.setSelectedItemIndex(0, juce::dontSendNotification);
    barsBox.setSelectedItemIndex(1, juce::dontSendNotification);
    octaveBox.setSelectedItemIndex(1, juce::dontSendNotification);

    for (auto* cb : { &keyBox, &scaleBox, &tsBox, &barsBox, &octaveBox })
    {
        cb->setEditableText(false);
        cb->setJustificationType(juce::Justification::centred);
    }

    restSl.setRange(0.0, 100.0, 1.0);
    restSl.setValue(15.0, juce::dontSendNotification);
    restSl.setSliderStyle(juce::Slider::LinearHorizontal);
    restSl.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    restDensityValue.setJustificationType(juce::Justification::centred);
    restDensityValue.setText("15 %", juce::dontSendNotification);

    // ---------- Humanize sliders (right column) ----------
    for (auto* s : { &timingSl, &velocitySl, &swingSl, &feelSl })
    {
        addAndMakeVisible(*s);
        s->setRange(0.0, 100.0, 1.0);
        s->setSliderStyle(juce::Slider::LinearHorizontal);
        s->setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        s->setValue(30.0, juce::dontSendNotification);
    }

    for (auto* l : { &timingValSl, &velocityValSl, &swingValSl, &feelValSl })
    {
        addAndMakeVisible(*l);
        l->setColour(juce::Label::textColourId, juce::Colours::black);
    }

    timingValSl.setButtonText("100");
    velocityValSl.setButtonText("50");
    swingValSl.setButtonText("25");
    feelValSl.setButtonText("0");

    // ---------- Piano roll ----------
    addAndMakeVisible(pianoRoll);

    // Bottom buttons already added via stylizeButton loop

    // Component IDs for possible external lookups / tests
    keyBox.setComponentID("keyBox");
    scaleBox.setComponentID("scaleBox");
    tsBox.setComponentID("timeSigBox");
    barsBox.setComponentID("barsBox");
    restSl.setComponentID("restDensitySlider");
    octaveBox.setComponentID("octaveBox");
    pianoRoll.setComponentID("pianoRoll");
    generateBtn.setComponentID("generateButton");
    dragBtn.setComponentID("dragDropButton");
    engineMelodyBtn.setComponentID("melodyButton");
    engineChordsBtn.setComponentID("chordsButton");
    engineMixtureBtn.setComponentID("mixtureButton");
    advancedBtn.setComponentID("advancedHarmonyButton");
    polyrBtn.setComponentID("polyrhythmButton");
    reharmBtn.setComponentID("reharmonizeButton");
    adjustBtn.setComponentID("adjustButton");

    dragBtn.onClick = [this] { performDragExport(); };
    advancedBtn.onClick = [this] { openAdvanced(); };
    polyrBtn.onClick = [this] { openPolyrhythm(); };
    reharmBtn.onClick = [this] { openReharmonize(); };
    adjustBtn.onClick = [this] { openAdjust(); };
    diceBtn.onClick = [this] { randomizeAll(); regenerate(); };

    generateBtn.onClick = [this]
    {
        // Make sure generator uses the latest UI settings (key/scale/TS/bars/rest/humanize/engine etc.)
        pushSettingsToGenerator();

        // Do the generation
        regenerate();
    };

    rollView.setViewedComponent(&pianoRoll, false);   // don't own it; you already own pianoRoll
    rollView.setScrollBarsShown(true, true);          // <-- horizontal + vertical
#if JUCE_MAJOR_VERSION >= 8
    rollView.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
#else
    rollView.setScrollOnDragEnabled(true);
#endif

    // Piano palette (your colors)



    PianoRollComponent::Palette pal;
    pal.background = juce::Colour(0xFF13220C);
    pal.gridLine = juce::Colour(0xFF314025);
    pal.keyWhiteFill = juce::Colour(0xFFF2AE01);
    pal.keyBlackFill = juce::Colours::black;
    pal.barNumberStrip = juce::Colour(0xFFF2AE01);
    pal.barNumberText = juce::Colours::black;
    pianoRoll.setPalette(pal);

    pushSettingsToGenerator();
    updateRollContentSize();

    const auto bg = juce::Colour::fromRGB(0xA5, 0xDD, 0x00); // #a5dd00
    const auto pickerBg = juce::Colour::fromRGB(0xF9, 0xBF, 0x00); // #f9bf00
    const auto accent = juce::Colour::fromRGB(0xDD, 0x4F, 0x02); // #dd4f02
    const auto outline = juce::Colours::black;


    auto styleHumanSlider = [accent](juce::Slider& s)
    {
        s.setRange(0.0, 100.0, 1.0);
        s.setColour(juce::Slider::trackColourId, accent);
        s.setColour(juce::Slider::thumbColourId, accent);
        s.setColour(juce::Slider::textBoxTextColourId, accent);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);


    };

};

   // }

juce::Image BANGAudioProcessorEditor::loadBinaryImage(const juce::String& nameLike)
{
    auto resourcesDir = getResourceFile("").getParentDirectory(); // Get the Resources dir itself

    if (!resourcesDir.isDirectory())
    {
        // Can't find resources, return empty image.
        // A jassertfalse would be good for debugging builds.
        // jassertfalse;
        return {};
    }

    // Try exact common suffixes first
    const char* suffixes[] = { ".png", ".jpg", ".jpeg" };
    for (auto* s : suffixes)
    {
        auto file = resourcesDir.getChildFile(nameLike + s);
        if (file.existsAsFile())
            if (auto img = juce::ImageFileFormat::loadFrom(file); img.isValid())
                return img;
    }

    // Fallback: scan the directory for any match that contains the hint
    juce::Array<juce::File> files;
    resourcesDir.findChildFiles(files, juce::File::findFiles, false, "*.png;*.jpg;*.jpeg");

    for (const auto& file : files)
    {
        if (file.getFileName().containsIgnoreCase(nameLike))
            if (auto img = juce::ImageFileFormat::loadFrom(file); img.isValid())
                return img;
    }

    return {};

}



   // addAndMakeVisible(timingLbl);   timingLbl.setText("HUMANIZE TIMING", juce::dontSendNotification);
    //addAndMakeVisible(velocityLbl); velocityLbl.setText("VELOCITY", juce::dontSendNotification);
    //addAndMakeVisible(swingLbl);    swingLbl.setText("SWING", juce::dontSendNotification);
    //addAndMakeVisible(feelLbl);     feelLbl.setText("FEEL", juce::dontSendNotification);
    // ----- Main action buttons -----

    // helper to load an image from BinaryData by name
    auto loadPng = [](const void* data, int size)
    {
        return juce::ImageCache::getFromMemory(data, size);
    };



    // --- Generate button click: push UI -> generator, then generate, then show on roll




    // Store natural bounds of buttons

    // ----- Piano roll (scrollable) -----


    // initial generator sync


void setImageButton3(juce::ImageButton& b, const juce::String& baseHint)
{
    const auto normal = loadImageByHint(baseHint);
    auto hover = loadImageByHint(baseHint + "_hover");
    auto down = loadImageByHint(baseHint + "_down");

    if (!hover.isValid()) hover = normal;
    if (!down.isValid())  down = normal;

    // now call the static helper
    setImageButton3(b, normal, hover, down);
}

void BANGAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}

void BANGAudioProcessorEditor::resized()
{
    // ---- Compact combo layout constants (put at very top of resized()) ----
    const int pad = 12;          // general padding
    const int rowGap = 12;          // vertical gap between rows
      
    const int labelImgW = 92;          // width of the left orange image labels
    const int labelImgH = 36;          // height of the left orange image labels
    const int leftColumnX = pad + 6;     // where the image labels start

    // This is the *new* narrower width you asked for (≈ half your current width).
    // If your window is very small, the jmin keeps things readable; and if
    // you stretch the window, they won’t balloon too wide.

    const int comboX = leftColumnX + labelImgW + 10; // combos sit to the right of the images

    // Y anchors for the two columns (left = combos, right = humanize)
    int yCombos = 120;   // starting Y for the "KEY / SCALE / TIME SIG / BARS / REST" stack
    int yHumanize = 120;   // starting Y for the Humanize sliders column (you already have them)

    auto r = getLocalBounds().reduced(16);

    // ---- Top right dice button ----
    diceBtn.setBounds(r.removeFromRight(44).removeFromTop(44));

    // ---- Engine selector row (3 image buttons above the logo) ----
    auto engineRow = r.removeFromTop(60);
    const int eW = 160, eH = 44, eGap = 16;
    juce::Rectangle<int> engineBar(0, 0, eW * 3 + eGap * 2, eH);
    engineBar = engineBar.withCentre(engineRow.getCentre());
    engineChordsBtn.setBounds(engineBar.removeFromLeft(eW));
    engineBar.removeFromLeft(eGap);
    engineMixtureBtn.setBounds(engineBar.removeFromLeft(eW));
    engineBar.removeFromLeft(eGap);
    engineMelodyBtn.setBounds(engineBar.removeFromLeft(eW));

    r.removeFromTop(20); // spacing

    // ---- Place the logo centered horizontally, halfway between combos and humanize ----
    const int gapTop = yCombos;      // bottom of left column
    const int gapBottom = yHumanize; // top of humanize column
    const int gapMidY = gapTop + (gapBottom - gapTop) / 2;

    int logoW = juce::jmin(juce::roundToInt(getWidth() * 0.28f), 420);
    int logoH = 120; // fallback
    if (auto img = logoImg.getImage(); img.isValid())
    {
        const double ar = img.getWidth() > 0 ? (double)img.getWidth() / (double)img.getHeight() : 3.0;
        logoH = juce::roundToInt(logoW / ar);
    }
    logoImg.setBounds((getWidth() - logoW) / 4, gapMidY - logoH / 4, logoW, logoH);

    r.removeFromTop(20); // Some space

    // Area for left column (selectors) and right column (humanize)
    auto columns = r.removeFromTop(200);

    const int comboW = 160;  // ~half width, enough for long scale names
    const int comboH = 28;

    // ---- LEFT column (Key/Scale/TS/Bars/Rest) ----
    auto left = columns.removeFromLeft(columns.getWidth() / 2);
    auto row = left.removeFromTop(38);
    keyBox.setBounds(row.reduced(4)); keyBox.setBounds(row.removeFromLeft(comboW));


    scaleBox.setBounds(row.reduced(4)); scaleBox.setBounds(row.removeFromLeft(comboW));


    tsBox.setBounds(row.reduced(4)); tsBox.setBounds(row.removeFromLeft(comboW));


    barsBox.setBounds(row.reduced(4)); barsBox.setBounds(row.removeFromLeft(comboW));


    restSl.setBounds(row.reduced(4)); restSl.setBounds(row.removeFromLeft(comboW));


    // --- Anchor each PNG label to the left of its combo box ---

    // KEY
    keyLblImg.setBounds(leftColumnX, yCombos, labelImgW, labelImgH);
    keyBox.setBounds(comboX, yCombos, comboW, comboH);
    yCombos += comboH + rowGap;

    // SCALE
    scaleLblImg.setBounds(leftColumnX, yCombos, labelImgW, labelImgH);
    scaleBox.setBounds(comboX, yCombos, comboW, comboH);
    yCombos += comboH + rowGap;

    // TIME SIG
    timeSigLblImg.setBounds(leftColumnX, yCombos, labelImgW, labelImgH);
    tsBox.setBounds(comboX, yCombos, comboW, comboH);
    yCombos += comboH + rowGap;

    // BARS
    barsLblImg.setBounds(leftColumnX, yCombos, labelImgW, labelImgH);
    barsBox.setBounds(comboX, yCombos, comboW, comboH);
    yCombos += comboH + rowGap;

    // REST DENSITY (label image to the left of *your* existing rest density strip)
    restdensityLblImg.setBounds(leftColumnX, yCombos, labelImgW, labelImgH);

    // If you have a small numeric box and a slider on this row, keep your existing controls,
    // just make sure their X/width respect the new combo column:
    restSl.setBounds(comboX, yCombos, 56, comboH);
    restSl.setBounds(restSl.getRight() + 10,
        yCombos + (comboH - 4) / 2, comboW - 56 - 10, 4);

    yCombos += comboH + rowGap;


    // Center the HUMANIZE header above the 4 sliders group
// --- Center HUMANIZE label image above the humanize sliders ---




    if (auto img = logoImg.getImage(); img.isValid())
    {
        const double ar = img.getWidth() > 0 ? (double)img.getWidth() / (double)img.getHeight() : 3.0;
        logoH = juce::roundToInt(logoW / ar);
    }
    logoImg.setBounds((getWidth() - logoW) / 2, gapMidY - logoH / 2, logoW, logoH);

    // ---- Small middle buttons (Advanced / Polyrhythm / Reharmonize) ----
    auto midRow = r.removeFromTop(52);
    const int smW = 160, smH = 44, smGap = 18;
    juce::Rectangle<int> smallRow(0, 0, smW * 3 + smGap * 2, smH);
    smallRow = smallRow.withCentre(midRow.getCentre());
    advancedBtn.setBounds(smallRow.removeFromLeft(smW));
    smallRow.removeFromLeft(smGap);
    polyrBtn.setBounds(smallRow.removeFromLeft(smW));
    smallRow.removeFromLeft(smGap);
    reharmBtn.setBounds(smallRow.removeFromLeft(smW));

    // ---- Adjust Button ----
    auto adjustRow = r.removeFromTop(52);
    adjustBtn.setBounds(adjustRow.withSizeKeepingCentre(140, 44));

    // ---- Piano roll ----
    auto pianoRollArea = r;
    auto bottomArea = pianoRollArea.removeFromBottom(80);
    rollView.setBounds(pianoRollArea);
    rollView.toBack();
    updateRollContentSize();

    // ---- Bottom big buttons ----
    const juce::Rectangle<int> maxBounds(0, 0, 160, 50);
    const int bigGap = 20;

    // --- Generate Button ---
    float genNatW = static_cast<float>(generateBtnNaturalBounds.getWidth());
    float genNatH = static_cast<float>(generateBtnNaturalBounds.getHeight());
    if (genNatW == 0) genNatW = 1;
    if (genNatH == 0) genNatH = 1;

    float genScale = juce::jmin((float)maxBounds.getWidth() / genNatW, (float)maxBounds.getHeight() / genNatH);
    int genScaledW = (int)(genNatW * genScale);
    int genScaledH = (int)(genNatH * genScale);

    // --- Drag Button ---
    float dragNatW = static_cast<float>(dragBtnNaturalBounds.getWidth());
    float dragNatH = static_cast<float>(dragBtnNaturalBounds.getHeight());
    if (dragNatW == 0) dragNatW = 1;
    if (dragNatH == 0) dragNatH = 1;

    float dragScale = juce::jmin((float)maxBounds.getWidth() / dragNatW, (float)maxBounds.getHeight() / dragNatH);
    int dragScaledW = (int)(dragNatW * dragScale);
    int dragScaledH = (int)(dragNatH * dragScale);

    const int totalWidth = genScaledW + dragScaledW + bigGap;
    const int maxHeight = juce::jmax(genScaledH, dragScaledH);

    juce::Rectangle<int> bigButtonRow(0, 0, totalWidth, maxHeight);
    bigButtonRow = bigButtonRow.withCentre(bottomArea.getCentre());

    generateBtn.setBounds(bigButtonRow.removeFromLeft(genScaledW).withSizeKeepingCentre(genScaledW, genScaledH));
    bigButtonRow.removeFromLeft(bigGap);
    dragBtn.setBounds(bigButtonRow.removeFromLeft(dragScaledW).withSizeKeepingCentre(dragScaledW, dragScaledH));

    // Ensure all buttons sit above the roll
    for (auto* b : { &engineChordsBtn, &engineMixtureBtn, &engineMelodyBtn,
                     &generateBtn, &dragBtn, &polyrBtn, &reharmBtn,
                     &adjustBtn, &diceBtn, &advancedBtn })
        b->toFront(false);
}


// ======== event handlers ========

void BANGAudioProcessorEditor::comboBoxChanged(juce::ComboBox* box)
{
    juce::ignoreUnused(box);
    pushSettingsToGenerator();
    updateRollContentSize();
}

void BANGAudioProcessorEditor::sliderValueChanged(juce::Slider* s)
{
    juce::ignoreUnused(s);
    pushSettingsToGenerator();
}

// ======== logic ========

int BANGAudioProcessorEditor::currentBars() const
{
    int v = barsBox.getText().getIntValue();
    if (v != 4 && v != 8) v = 4;
    return v;
}

int BANGAudioProcessorEditor::currentTSNumerator() const
{
    auto s = tsBox.getText();
    int slash = s.indexOfChar('/');
    if (slash > 0) return juce::jlimit(1, 32, s.substring(0, slash).getIntValue());
    // additive signatures (e.g. 3+2/8) -> sum the numerators
    int plus = s.indexOfChar('+');
    if (plus > 0)
    {
        int total = 0;
        juce::String tmp = s.upToFirstOccurrenceOf("/", false, false);
        for (auto part : juce::StringArray::fromTokens(tmp, "+", {}))
            total += part.getIntValue();
        return juce::jlimit(1, 32, total);
    }
    return 4;
}

void BANGAudioProcessorEditor::updateRollContentSize()
{
    const int beats = currentTSNumerator();
    const int bars = currentBars();
    const int pxPerBeat = 96;
    const int contentW = juce::jlimit(800, 20000, bars * beats * pxPerBeat);
    const int contentH = rollView.getHeight();

    pianoRoll.setSize(contentW, contentH);
    pianoRoll.repaint();
}

void BANGAudioProcessorEditor::onEngineChanged()
{
    if (engineChordsBtn.getToggleState())  engineSel = EngineSel::Chords;
    if (engineMixtureBtn.getToggleState()) engineSel = EngineSel::Mixture;
    if (engineMelodyBtn.getToggleState())  engineSel = EngineSel::Melody;

    // also let generator know if you keep it there
    auto& gen = audioProcessor.getMidiGenerator();
    using EM = MidiGenerator::EngineMode;
    switch (engineSel)
    {
    case EngineSel::Chords:  gen.setEngineMode(EM::Chords);  break;
    case EngineSel::Mixture: gen.setEngineMode(EM::Mixture); break;
    case EngineSel::Melody:  gen.setEngineMode(EM::Melody);  break;
    }
}

// Push UI → MidiGenerator (no PianoRoll methods that don’t exist)
void BANGAudioProcessorEditor::pushSettingsToGenerator()
{
    auto& gen = audioProcessor.getMidiGenerator();

    // Key
    const int keyIndex = keyBox.getSelectedId() - 1; // 0..11
    const int keyMidi = 60 + keyIndex;              // middle C = 60
    gen.setKey(keyMidi);

    // Scale
    gen.setScale(scaleBox.getText());

    // Time signature
    const auto ts = tsBox.getText();
    int beats = currentTSNumerator(), denom = 4;
    if (ts.containsChar('/'))
        denom = ts.fromFirstOccurrenceOf("/", false, false).getIntValue();
    gen.setTimeSignature(beats, denom);

    // Bars
    gen.setBars(currentBars());

    // Rest density (0..100 -> 0..1)
    gen.setRestDensity(restSl.getValue() / 100.0);

    // Humanize (0..100 -> 0..1)
    const auto lim01 = [](double v) { return juce::jlimit(0.0, 100.0, v) / 100.0; };
    const float timingAmt = static_cast<float>(lim01(timingSl.getValue()));
    const float velAmt = static_cast<float>(lim01(velocitySl.getValue()));
    const float feelAmt = static_cast<float>(lim01(feelSl.getValue()));
    const float swingAmt = static_cast<float>(lim01(swingSl.getValue()));

    gen.enableStyleAwareTiming(true);
    gen.setStyleTimingAmount(juce::jlimit(0.0f, 1.0f, timingAmt * 0.6f + velAmt * 0.2f + feelAmt * 0.2f));
    gen.setPolyrhythmAmount(swingAmt);
}

void BANGAudioProcessorEditor::regenerate()
{
    auto& gen = audioProcessor.getMidiGenerator();

    lastMelody.clear();
    lastChords.clear();

    switch (engineSel)
    {
    case EngineSel::Chords:
        // --- CHORDS ENGINE ---
    {
        // Collect the same settings you already pass to the other modes:
        const int bars = currentBars();
        const int tsNum = currentTSNumerator();
        const int tsDen = tsBox.getSelectedId(); // keep whatever you already use for denominator
        const int root = rootBoxToSemitone(keyBox); // <— use your existing helper that converts the key combobox to semitone
        const int scaleIndex = scaleBox.getSelectedId(); // whatever you already use
        const double restDensity = restSl.getValue() * 0.01; // if your generator expects 0..1
        const double humanTiming = timingSl.getValue();
        const double humanVel = velocitySl.getValue();
        const double swingAmt = swingSl.getValue();
        const double feelAmt = feelSl.getValue();

        // Push current engine + humanize into the generator (these calls should already exist):
        gen.setEngineMode(MidiGenerator::EngineMode::Chords);
        gen.setHumanizeTiming(humanTiming);
        gen.setHumanizeVelocity(humanVel);
        gen.setSwingAmount(swingAmt);
        gen.setFeelAmount(feelAmt);

        // Make sure time sig / bars are up to date for the generator if it stores them:
        gen.setTimeSignature(tsNum, tsDen);
        gen.setBars(bars);

        // Make sure key/scale/rest are set:
        gen.setKey(root);
        gen.setScaleIndex(scaleIndex);
        gen.setRestDensity(restDensity);

        // *** IMPORTANT: call the chords-only path and get ALL bars back ***
        auto mix = gen.generateMelodyAndChords(/*respectRests*/true);
        const auto& chordNotes = mix.chords;

        // Push to the roll exactly like you do for the other engines:
        pianoRoll.setNotes(chordNotes);
        pianoRoll.repaint();
    }
    break;

    case EngineSel::Melody:
    {
        lastMelody = gen.generateMelody();
        pianoRoll.setNotes(lastMelody);
    } break;

    case EngineSel::Mixture:
    default:
    {
        auto mix = gen.generateMelodyAndChords();
        lastMelody = std::move(mix.melody);
        lastChords = std::move(mix.chords);
        // show both on the roll
        std::vector<Note> combined;
        combined.reserve(lastMelody.size() + lastChords.size());
        combined.insert(combined.end(), lastMelody.begin(), lastMelody.end());
        combined.insert(combined.end(), lastChords.begin(), lastChords.end());
        pianoRoll.setNotes(combined);
    } break;
    }

    updateRollContentSize();
    pianoRoll.repaint();
};

void BANGAudioProcessorEditor::randomizeAll()
{
    // Randomize Key
    keyBox.setSelectedItemIndex(juce::Random::getSystemRandom().nextInt(keyBox.getNumItems()));

    // Randomize Scale
    scaleBox.setSelectedItemIndex(juce::Random::getSystemRandom().nextInt(scaleBox.getNumItems()));

    tsBox.setSelectedItemIndex(juce::Random::getSystemRandom().nextInt(tsBox.getNumItems()));

    // Randomize Humanize Sliders
    timingSl.setValue(juce::Random::getSystemRandom().nextDouble() * 100.0);
    velocitySl.setValue(juce::Random::getSystemRandom().nextDouble() * 100.0);
    swingSl.setValue(juce::Random::getSystemRandom().nextDouble() * 100.0);
    feelSl.setValue(juce::Random::getSystemRandom().nextDouble() * 100.0);
    restSl.setValue(juce::Random::getSystemRandom().nextDouble() * 100.0);

    pushSettingsToGenerator();
}

// ======== Drag export ========

juce::File BANGAudioProcessorEditor::writeTempMidiForDrag()
{
    const int ppq = 960;
    juce::MidiMessageSequence seq;

    // Helper to push notes into the sequence (with a light velocity scale)
    auto push = [&](const std::vector<Note>& src, int midiChannel, int velScalePercent)
    {
        for (const auto& n : src)
        {
            const int pitch = juce::jlimit(0, 127, n.pitch);
            const int scaled = (n.velocity * velScalePercent) / 100;
            const int vel = juce::jlimit(1, 127, scaled);

            const double tOn = n.startBeats * (double)ppq;
            const double tOff = (n.startBeats + n.lengthBeats) * (double)ppq;

            seq.addEvent(juce::MidiMessage::noteOn(midiChannel, pitch, (juce::uint8)vel), tOn);
            seq.addEvent(juce::MidiMessage::noteOff(midiChannel, pitch), tOff);
        }
    };

    // Use cached notes if present, otherwise generate now
    std::vector<Note> mel = lastMelody;
    std::vector<Note> chd = lastChords;
    if (mel.empty() && chd.empty())
    {
        const bool useMixture = (currentEngineIndex == 1); // 0=chords,1=mixture,2=melody
        auto bundle = audioProcessor.getMidiGenerator().generateMelodyAndChords(useMixture);
        mel = std::move(bundle.melody);
        chd = std::move(bundle.chords);
    }

    // ⬅️ THIS is the key change: call push(...), do NOT call lastChords(...)
    push(chd, 1, 95); // chords on ch 1
    push(mel, 2, 95); // melody on ch 2

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote(ppq);
    mf.addTrack(seq);

    auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("BANG_drag.mid");
    temp.deleteFile();
    juce::FileOutputStream os(temp);
    if (os.openedOk())
        mf.writeTo(os);

    return temp;
}

void BANGAudioProcessorEditor::performDragExport()
{
    auto f = writeTempMidiForDrag();
    if (f.existsAsFile())
        performExternalDragDropOfFiles({ f.getFullPathName() }, true);
}

// ======== Popups ========

void BANGAudioProcessorEditor::openAdvanced()
{
    auto* comp = new AdvancedHarmonyWindow(advOptions); // your existing editor for advanced harmony
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Advanced Harmony";
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.content.setOwned(comp);
    if (auto* w = opts.launchAsync())
        w->centreWithSize(720, 560);
}

void BANGAudioProcessorEditor::openPolyrhythm()
{
    // Lightweight dialog that writes straight into the generator's polyrhythm amount
    struct PolyComp : public juce::Component
    {
        BANGAudioProcessorEditor& editor;
        juce::Slider amount;
        PolyComp(BANGAudioProcessorEditor& ed) : editor(ed)
        {
            addAndMakeVisible(amount);
            amount.setRange(0.0, 100.0, 1.0);
            amount.setValue(ed.swingSl.getValue());
            amount.onValueChange = [this]
            {
                editor.swingSl.setValue(amount.getValue(), juce::sendNotification);
                editor.pushSettingsToGenerator();
            };
        }
        void resized() override { amount.setBounds(getLocalBounds().reduced(20)); }

        void paint(juce::Graphics& g)
        {
            g.fillAll(juce::Colour::fromRGB(0xA5, 0xDD, 0x00));
            g.setColour(juce::Colours::black);
            g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
            g.drawText("Polyrhythm", getLocalBounds().removeFromTop(28), juce::Justification::centred);

        }
    };


    auto* comp = new PolyComp(*this);
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Polyrhythm";
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.content.setOwned(comp);
    if (auto* w = opts.launchAsync())
        w->centreWithSize(400, 120);
    

}

void BANGAudioProcessorEditor::openReharmonize()
{
    // Minimal reharm UI mapping to your existing controls (complexity via FEEL here)
    struct RehComp : public juce::Component
    {
        BANGAudioProcessorEditor& editor;
        juce::Slider complexity;
        RehComp(BANGAudioProcessorEditor& ed) : editor(ed)
        {
            addAndMakeVisible(complexity);
            complexity.setRange(0.0, 100.0, 1.0);
            complexity.setValue(ed.feelSl.getValue());
            complexity.onValueChange = [this]
            {
                editor.feelSl.setValue(complexity.getValue(), juce::sendNotification);
                editor.pushSettingsToGenerator();
            };
        }
        void resized() override { complexity.setBounds(getLocalBounds().reduced(20)); }
       
        void paint(juce::Graphics& g)
        {
            g.fillAll(juce::Colour::fromRGB(0xA5, 0xDD, 0x00));
            g.setColour(juce::Colours::black);
            g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
            g.drawText("Reharmonize", getLocalBounds().removeFromTop(28), juce::Justification::centred);

        }
    };

    auto* comp = new RehComp(*this);
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Reharmonize";
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.content.setOwned(comp);
    if (auto* w = opts.launchAsync())
        w->centreWithSize(400, 120);
}

void BANGAudioProcessorEditor::openAdjust()
{

    // “Adjust” lets user batch-apply a few quick tweaks mapped to the main sliders/combos
    struct AdjustComp : public juce::Component
    {
        BANGAudioProcessorEditor& editor;
        juce::ToggleButton keyChk { "Key" }, scaleChk{ "Scale" }, tsChk{ "Time Sig" }, barsChk{ "Bars" }, restChk{ "Rest %" }, humanChk{ "Humanize" };
        juce::ComboBox keySel, scaleSel, tsSel, barsSel;
        juce::Slider restSlider, timing, velocity, swing, feel;


        AdjustComp(BANGAudioProcessorEditor& ed) : editor(ed)
        {
            addAndMakeVisible(keyChk);   addAndMakeVisible(scaleChk);
            addAndMakeVisible(tsChk);    addAndMakeVisible(barsChk);
            addAndMakeVisible(restChk);  addAndMakeVisible(humanChk);

            addAndMakeVisible(keySel);   addAndMakeVisible(scaleSel);
            addAndMakeVisible(tsSel);    addAndMakeVisible(barsSel);

            addAndMakeVisible(restSlider);
            addAndMakeVisible(timing); addAndMakeVisible(velocity); addAndMakeVisible(swing); addAndMakeVisible(feel);

            // mirror choices from editor
            auto copyComboBoxItems = [](juce::ComboBox& dest, const juce::ComboBox& src)
            {
                for (int i = 0; i < src.getNumItems(); ++i)
                    dest.addItem(src.getItemText(i), src.getItemId(i));
            };

            copyComboBoxItems(keySel, editor.keyBox);
            copyComboBoxItems(scaleSel, editor.scaleBox);
            copyComboBoxItems(tsSel, editor.tsBox);
            copyComboBoxItems(barsSel, editor.barsBox);

            restSlider.setRange(0.0, 100.0, 1.0);
            timing.setRange(0.0, 100.0, 1.0);
            velocity.setRange(0.0, 100.0, 1.0);
            swing.setRange(0.0, 100.0, 1.0);
            feel.setRange(0.0, 100.0, 1.0);

            // Apply now button
            auto* btn = new juce::TextButton("Apply + Generate");
            addAndMakeVisible(btn);
            btn->onClick = [this, btn] { applyThenGenerate(); juce::ignoreUnused(btn); };
        }

        void paint(juce::Graphics& g)
        {
            g.fillAll(juce::Colour::fromRGB(0xA5, 0xDD, 0x00));
            g.setColour(juce::Colours::black);
            g.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));

        }

        void resized() override
        {
            auto r = getLocalBounds().reduced(16);
            auto col = r.removeFromLeft(getWidth() / 2);

            auto place = [](juce::Component& c, juce::Rectangle<int>& row) { c.setBounds(row.removeFromTop(32).reduced(4)); row.removeFromTop(8); };

            auto left = col;
            place(keyChk, left);   place(keySel, left);
            place(scaleChk, left); place(scaleSel, left);
            place(tsChk, left);    place(tsSel, left);
            place(barsChk, left);  place(barsSel, left);

            auto right = r;
            place(restChk, right); place(restSlider, right);
            place(humanChk, right); place(timing, right); place(velocity, right); place(swing, right); place(feel, right);

            // button at bottom
            for (auto* c : getChildren())
                if (auto* b = dynamic_cast<juce::TextButton*>(c))
                    b->setBounds(getLocalBounds().removeFromBottom(40).withSizeKeepingCentre(200, 36));
        }

        void applyThenGenerate()
        {
            if (keyChk.getToggleState() && keySel.getSelectedId() > 0) editor.keyBox.setSelectedId(keySel.getSelectedId(), juce::sendNotification);
            if (scaleChk.getToggleState() && scaleSel.getSelectedId() > 0) editor.scaleBox.setSelectedId(scaleSel.getSelectedId(), juce::sendNotification);
            if (tsChk.getToggleState() && tsSel.getSelectedId() > 0) editor.tsBox.setSelectedId(tsSel.getSelectedId(), juce::sendNotification);
            if (barsChk.getToggleState() && barsSel.getSelectedId() > 0) editor.barsBox.setSelectedId(barsSel.getSelectedId(), juce::sendNotification);

            if (restChk.getToggleState())  editor.restSl.setValue(restSlider.getValue(), juce::sendNotification);
            if (humanChk.getToggleState())
            {
                editor.timingSl.setValue(timing.getValue());
                editor.velocitySl.setValue(velocity.getValue());
                editor.swingSl.setValue(swing.getValue());
                editor.feelSl.setValue(feel.getValue());
            }

            editor.pushSettingsToGenerator();
            editor.regenerate();
        }
    };

    auto* comp = new AdjustComp(*this);
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Adjust";
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.content.setOwned(comp);
    if (auto* w = opts.launchAsync())
        w->centreWithSize(720, 560);
}
