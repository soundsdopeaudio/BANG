#include "PluginEditor.h"
#include "BinaryData.h"


// ===== image loading helper that tries multiple BinaryData names =====
static juce::Image loadImageAny(std::initializer_list<const char*> names)
{
    for (auto* nm : names)
    {
        if (nm == nullptr) continue;
        int dataSize = 0;
        const void* data = BinaryData::getNamedResource(nm, dataSize);
        if (data != nullptr && dataSize > 0)
        {
            juce::MemoryInputStream mis(data, (size_t)dataSize, false);
            if (auto img = juce::ImageFileFormat::loadFrom(mis); img.isValid())
                return img;
        }
    }
    return {}; // empty image
}

// Overload so you can pass std::vector<const char*> too
static juce::Image loadImageAny(const std::vector<const char*>& names)
{
    for (auto* nm : names)
    {
        if (nm == nullptr) continue;
        int dataSize = 0;
        const void* data = BinaryData::getNamedResource(nm, dataSize);
        if (data != nullptr && dataSize > 0)
        {
            juce::MemoryInputStream mis(data, (size_t)dataSize, false);
            if (auto img = juce::ImageFileFormat::loadFrom(mis); img.isValid())
                return img;
        }
    }
    return {};
}

// ===== set exactly 3 images on an ImageButton (normal/over/down) =====
// Some JUCE versions need the "overlay colour" argument after each opacity.
// If you pass 11 args, you’ll get the exact error you saw. This wrapper
// calls the signature your headers expect.
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


// --- helper for 3-state image buttons (up/over/down) ---
static juce::Image loadImageAny(const std::initializer_list<const char*>& names)
{
    for (auto* name : names)
    {
        if (name == nullptr || *name == '\0') continue;

        int dataSize = 0;
        const void* ptr = BinaryData::getNamedResource(name, dataSize);
        if (ptr != nullptr && dataSize > 0)
        {
            auto img = juce::ImageFileFormat::loadFrom(ptr, (size_t)dataSize);
            if (img.isValid())
            {
                DBG("Loaded image: " << name << "  (" << img.getWidth() << "x" << img.getHeight() << ")");
                return img;
            }
        }
    }
    return juce::Image();
}

// Set images for a juce::ImageButton with all hover/down variants if available.
// Usage: setImageButton3(generateBtn, {"GenerateBtn", "GenerateBtn_normal.png"});
static void setImageButton3(juce::ImageButton& btn,
    std::initializer_list<const char*> baseNameCandidates)
{
    // Build the candidate lists for normal/over/down using all basenames you pass in.
    std::vector<const char*> normalList, overList, downList;

    auto pushAll = [](std::vector<const char*>& dst, const char* base)
    {
        // exact filename as-is
        dst.push_back(base);
        // common patterns
        // base.png
        {
            static thread_local std::string s;
            s = base; s += ".png";              dst.push_back(s.c_str());
        }
        // base_normal.png
        {
            static thread_local std::string s;
            s = base; s += "_normal.png";       dst.push_back(s.c_str());
        }
        // base_over.png
        {
            static thread_local std::string s;
            s = base; s += "_over.png";         dst.push_back(s.c_str());
        }
        // base_hover.png
        {
            static thread_local std::string s;
            s = base; s += "_hover.png";        dst.push_back(s.c_str());
        }
        // base_down.png
        {
            static thread_local std::string s;
            s = base; s += "_down.png";         dst.push_back(s.c_str());
        }
        // base_pressed.png
        {
            static thread_local std::string s;
            s = base; s += "_pressed.png";      dst.push_back(s.c_str());
        }
        // base_on.png (some sets use “on/off”)
        {
            static thread_local std::string s;
            s = base; s += "_on.png";           dst.push_back(s.c_str());
        }
        // base_off.png
        {
            static thread_local std::string s;
            s = base; s += "_off.png";          dst.push_back(s.c_str());
        }
    };

    // For each candidate base name you pass, add all plausible variants.
    for (auto* base : baseNameCandidates)
    {
        pushAll(normalList, base);
        pushAll(overList, base);
        pushAll(downList, base);
    }

    // Try to load an actual image for each state.
    auto normal = loadImageAny(normalList);
    auto over = loadImageAny(overList);
    auto down = loadImageAny(downList);

    // Fallbacks so the button is at least visible if only one image exists.
    if (!normal.isValid() && over.isValid())  normal = over;
    if (!normal.isValid() && down.isValid())  normal = down;
    if (!over.isValid())                      over = normal;
    if (!down.isValid())                      down = normal;

    // If still nothing, fill a visible placeholder so you SEE the button area.
    if (!normal.isValid())
    {
        normal = juce::Image(juce::Image::ARGB, 160, 48, true);
        juce::Graphics g(normal);
        g.fillAll(juce::Colours::darkred);
        g.setColour(juce::Colours::black);
        g.drawRect(normal.getBounds(), 2);
        g.setColour(juce::Colours::white);
        g.drawText("MISSING IMG", normal.getBounds(), juce::Justification::centred);
    }
    if (!over.isValid()) over = normal;
    if (!down.isValid()) down = normal;
}

// ======== BinaryData image helpers ========

// fuzzy lookup: tries <hint>.png, <hint>_hover.png, etc. and also partial matches in BinaryData.
static juce::Image loadBinaryImage(const juce::String& nameLike)
{
    // Try exact common suffixes first
    const char* suffixes[] = { ".png", ".jpg", ".jpeg" };
    for (auto* s : suffixes)
    {
        auto nm = (nameLike + s);
        int dataSize = 0;
        if (const auto* data = BinaryData::getNamedResource(nm.toRawUTF8(), dataSize))
            return juce::ImageFileFormat::loadFrom(data, (size_t)dataSize);
    }

    // Fallback: scan the resource list for any match that contains the hint
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        juce::String nm(BinaryData::namedResourceList[i]);
        if (nm.containsIgnoreCase(nameLike) && (nm.endsWithIgnoreCase(".png") || nm.endsWithIgnoreCase(".jpg") || nm.endsWithIgnoreCase(".jpeg")))
        {
            int dataSize = 0;
            if (const auto* data = BinaryData::getNamedResource(nm.toRawUTF8(), dataSize))
                return juce::ImageFileFormat::loadFrom(data, (size_t)dataSize);
        }
    }

    return {};
}

// ======== Editor ========

BANGAudioProcessorEditor::BANGAudioProcessorEditor(BANGAudioProcessor& p)
    : AudioProcessorEditor(&p),
    audioProcessor(p)
{
    // Make the editor resizable
    setResizable(true, true);
    setResizeLimits(900, 560, 2400, 1800);
    setSize(1200, 720);

    // ----- Theme colours (your palette) -----
    const auto bg = juce::Colour::fromRGB(0xA5, 0xDD, 0x00); // #a5dd00
    const auto pickerBg = juce::Colour::fromRGB(0xF9, 0xBF, 0x00); // #f9bf00
    const auto accent = juce::Colour::fromRGB(0xDD, 0x4F, 0x02); // #dd4f02
    const auto outline = juce::Colours::black;

    setColour(juce::ResizableWindow::backgroundColourId, bg);

    // ----- Logo -----
    addAndMakeVisible(logoImg);
    if (auto img = loadImageByHint("bangnewlogo"); img.isValid())
        logoImg.setImage(img); // use placement separately
    logoImg.setImagePlacement(juce::RectanglePlacement::centred);

    // ----- Engine title image -----
    addAndMakeVisible(engineTitleImg);
    if (auto eimg = loadImageByHint("engine"); eimg.isValid())
        engineTitleImg.setImage(eimg);
    engineTitleImg.setImagePlacement(juce::RectanglePlacement::centred);

    // ----- Left: Key / Scale / TS / Bars -----
    auto stylePicker = [pickerBg, outline](juce::ComboBox& cb)
    {
        cb.setColour(juce::ComboBox::backgroundColourId, pickerBg);
        cb.setColour(juce::ComboBox::textColourId, outline);
        cb.setColour(juce::ComboBox::outlineColourId, outline);
        cb.setColour(juce::ComboBox::arrowColourId, outline);
    };

    keyLbl.setText("KEY", juce::dontSendNotification);
    scaleLbl.setText("SCALE", juce::dontSendNotification);
    tsLbl.setText("TIME SIG", juce::dontSendNotification);
    barsLbl.setText("BARS", juce::dontSendNotification);
    restLbl.setText("REST %", juce::dontSendNotification);

    for (auto* l : { &keyLbl, &scaleLbl, &tsLbl, &barsLbl, &restLbl })
    {
        l->setColour(juce::Label::textColourId, juce::Colours::black);
        l->setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(*l);
    }

    // Keys
    const char* keys[] = { "C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B" };
    for (int i = 0; i < 12; ++i) keyBox.addItem(keys[i], i + 1);
    keyBox.setSelectedId(1);
    stylePicker(keyBox); keyBox.addListener(this); addAndMakeVisible(keyBox);

    // Scales (your long list)
    const char* scales[] = {
        "Major","Natural Minor","Harmonic Minor","Dorian","Phrygian","Lydian","Mixolydian","Aeolian","Locrian",
        "Locrian Nat6","Ionian #5","Dorian #4","Phrygian Dom","Lydian #2","Super Locrian","Dorian b2",
        "Lydian Aug","Lydian Dom","Mixo b6","Locrian #2","Ethiopian Min","8 Tone Spanish","Phrygian Nat3",
        "Blues","Hungarian Min","Harmonic Maj","Pentatonic Maj","Pentatonic Min","Neopolitan Maj",
        "Neopolitan Min","Spanish Gypsy","Romanian Minor","Chromatic","Bebop Major","Bebop Minor"
    };
    for (int i = 0; i < (int)std::size(scales); ++i) scaleBox.addItem(scales[i], i + 1);
    scaleBox.setSelectedId(1);
    stylePicker(scaleBox); scaleBox.addListener(this); addAndMakeVisible(scaleBox);

    // Time signatures (incl. additive)
    {
        int id = 1;
        auto add = [&](juce::String t) { tsBox.addItem(t, id++); };
        add("2/4"); add("3/4"); add("4/4"); add("5/4"); add("7/4"); add("9/4");
        add("5/8"); add("6/8"); add("7/8"); add("9/8"); add("10/8"); add("11/8");
        add("12/8"); add("13/8"); add("15/8"); add("17/8"); add("19/8"); add("21/8");
        add("5/16"); add("7/16"); add("9/16"); add("11/16"); add("13/16"); add("15/16"); add("17/16"); add("19/16");
        add("3+2/8"); add("2+3/8");
        add("2+2+3/8"); add("3+2+2/8"); add("2+3+2/8");
        add("3+3+2/8"); add("3+2+3/8"); add("2+3+3/8");
        add("4+3/8"); add("3+4/8");
        add("3+2+2+3/8");
        tsBox.setSelectedId(3); // 4/4
    }
    stylePicker(tsBox); tsBox.addListener(this); addAndMakeVisible(tsBox);

    // Bars: 4 or 8 only
    barsBox.addItem("4", 1);
    barsBox.addItem("8", 2);
    barsBox.setSelectedId(2);
    stylePicker(barsBox); barsBox.addListener(this); addAndMakeVisible(barsBox);

    // Rest density slider
    restSl.setRange(0.0, 100.0, 1.0);
    restSl.setValue(15.0);
    restSl.setColour(juce::Slider::trackColourId, accent);
    restSl.setColour(juce::Slider::thumbColourId, juce::Colours::black);
    restSl.setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
    restSl.setTextValueSuffix(" %");
    restSl.addListener(this);
    addAndMakeVisible(restSl);

    // ----- Right: Humanize -----
    humanizeTitle.setText("HUMANIZE", juce::dontSendNotification);
    humanizeTitle.setColour(juce::Label::textColourId, juce::Colours::black);
    addAndMakeVisible(humanizeTitle);

    auto styleHumanSlider = [accent](juce::Slider& s)
    {
        s.setRange(0.0, 100.0, 1.0);
        s.setColour(juce::Slider::trackColourId, accent);
        s.setColour(juce::Slider::thumbColourId, juce::Colours::black);
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
    };

    addAndMakeVisible(timingLbl);   timingLbl.setText("HUMANIZE TIMING", juce::dontSendNotification);
    addAndMakeVisible(velocityLbl); velocityLbl.setText("VELOCITY", juce::dontSendNotification);
    addAndMakeVisible(swingLbl);    swingLbl.setText("SWING", juce::dontSendNotification);
    addAndMakeVisible(feelLbl);     feelLbl.setText("FEEL", juce::dontSendNotification);
    for (auto* l : { &timingLbl, &velocityLbl, &swingLbl, &feelLbl })
        l->setColour(juce::Label::textColourId, juce::Colours::black);

    styleHumanSlider(timingSl);   timingSl.setValue(40.0); addAndMakeVisible(timingSl);   timingSl.addListener(this);
    styleHumanSlider(velocitySl); velocitySl.setValue(35.0); addAndMakeVisible(velocitySl); velocitySl.addListener(this);
    styleHumanSlider(swingSl);    swingSl.setValue(25.0); addAndMakeVisible(swingSl);    swingSl.addListener(this);
    styleHumanSlider(feelSl);     feelSl.setValue(30.0); addAndMakeVisible(feelSl);     feelSl.addListener(this);

    // ----- Engine buttons (image buttons with 3 states) -----
    addAndMakeVisible(engineChordsBtn);   setImageButton3(engineChordsBtn, "chords");
    addAndMakeVisible(engineMixtureBtn);  setImageButton3(engineMixtureBtn, "mixture");
    addAndMakeVisible(engineMelodyBtn);   setImageButton3(engineMelodyBtn, "melody");

    for (auto* b : { &engineChordsBtn, &engineMixtureBtn, &engineMelodyBtn })
    {
        b->setClickingTogglesState(true);
        b->setRadioGroupId(1001);
    }
    engineMixtureBtn.setToggleState(true, juce::dontSendNotification);
    currentEngineIndex = 1;

    engineChordsBtn.onClick = [this] { onEngineChanged(); };
    engineMixtureBtn.onClick = [this] { onEngineChanged(); };
    engineMelodyBtn.onClick = [this] { onEngineChanged(); };

    // ----- Main action buttons -----
    addAndMakeVisible(generateBtn); setImageButton3(generateBtn, "generate");
    addAndMakeVisible(dragBtn);     setImageButton3(dragBtn, "drag");
    addAndMakeVisible(advancedBtn); setImageButton3(advancedBtn, "advanced");
    addAndMakeVisible(polyrBtn);    setImageButton3(polyrBtn, "polyr");
    addAndMakeVisible(reharmBtn);   setImageButton3(reharmBtn, "reharm");
    addAndMakeVisible(adjustBtn);   setImageButton3(adjustBtn, "adjust");
    addAndMakeVisible(diceBtn);     setImageButton3(diceBtn, "dice");

    generateBtn.onClick = [this] { regenerate(); };
    dragBtn.onClick = [this] { performDragExport(); };
    advancedBtn.onClick = [this] { openAdvanced(); };
    polyrBtn.onClick = [this] { openPolyrhythm(); };
    reharmBtn.onClick = [this] { openReharmonize(); };
    adjustBtn.onClick = [this] { openAdjust(); };
    diceBtn.onClick = [this] { randomizeAll(); regenerate(); };

    // ----- Piano roll (scrollable) -----
    addAndMakeVisible(rollView);
    rollView.setViewedComponent(&pianoRoll, false);   // don't own it; you already own pianoRoll
    rollView.setScrollBarsShown(true, true);          // <-- horizontal + vertical
#if JUCE_MAJOR_VERSION >= 8
    rollView.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
#else
    rollView.setScrollOnDragEnabled(true);
#endif

    engineChordsBtn.toFront(true);
    engineMixtureBtn.toFront(true);
    engineMelodyBtn.toFront(true);

    generateBtn.toFront(true);
    dragBtn.toFront(true);
    polyrBtn.toFront(true);
    reharmBtn.toFront(true);
    adjustBtn.toFront(true);
    diceBtn.toFront(true);

    pianoRoll.toBack(); // roll under the buttons just in case

    // Example: pass *your* base names. The helper will try base.png, base_normal.png, base_over.png, base_down.png, etc.
    setImageButton3(engineChordsBtn, { "enginechords", "chordsBtn", "ChordsBtn", "chords" });
    setImageButton3(engineMixtureBtn, { "enginemixture", "mixtureBtn", "MixtureBtn", "mixture" });
    setImageButton3(engineMelodyBtn, { "enginemelody", "melodyBtn", "MelodyBtn", "melody" });

    setImageButton3(advancedBtn, { "AdvancedHarmonyBtn", "advancedBtn", "ADVANCED_HARMONY" });
    setImageButton3(polyrBtn, { "PolyrhythmBtn",     "polyrBtn",     "POLYRHYTHM" });
    setImageButton3(reharmBtn, { "reharmBtn",    "reharmBtn",    "REHARMONIZE" });
    setImageButton3(adjustBtn, { "adjustBtn",         "AdjustBtn",    "ADJUST" });
    setImageButton3(generateBtn, { "generateBtn",       "generateBtn",  "GENERATE" });
    setImageButton3(dragBtn, { "dragBtn",           "dragBtn",      "DRAG" });
    setImageButton3(diceBtn, { "DiceBtn",           "diceBtn",      "DICE" });

    // Piano palette (your colors)
    {
        PianoRollComponent::Palette pal;
        pal.background = juce::Colour(0xFF13220C);
        pal.gridLine = juce::Colour(0xFF314025);
        pal.keyWhiteFill = juce::Colour(0xFFF2AE01);
        pal.keyBlackFill = juce::Colours::black;
        pal.barNumberStrip = juce::Colour(0xFFF2AE01);
        pal.barNumberText = juce::Colours::black;
        pianoRoll.setPalette(pal);
    }

    // initial generator sync
    pushSettingsToGenerator();
    updateRollContentSize();
}

juce::Image loadImageByHint(const juce::String& hint)
{
    return loadBinaryImage(hint);
}

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
    auto r = getLocalBounds().reduced(16);

    // ---- Top: Logo centered, “engine” row under it ----
    auto top = r.removeFromTop(120);
    logoImg.setBounds(top.withSizeKeepingCentre(360, 90));

    // Area for left column (selectors) and right column (humanize)
    auto columns = r.removeFromTop(200);

    // ---- LEFT column (Key/Scale/TS/Bars/Rest) ----
    auto left = columns.removeFromLeft(420);
    auto row = left.removeFromTop(38); keyLbl.setBounds(row.removeFromLeft(90));
    keyBox.setBounds(row.reduced(4)); // full row for combo

    row = left.removeFromTop(38);      scaleLbl.setBounds(row.removeFromLeft(90));
    scaleBox.setBounds(row.reduced(4));

    row = left.removeFromTop(38);      tsLbl.setBounds(row.removeFromLeft(90));
    tsBox.setBounds(row.reduced(4));

    row = left.removeFromTop(38);      barsLbl.setBounds(row.removeFromLeft(90));
    barsBox.setBounds(row.reduced(4));

    row = left.removeFromTop(38);      restLbl.setBounds(row.removeFromLeft(90));
    restSl.setBounds(row.reduced(4));

    // Adjust button aligned with Rest row (never under the roll)
    adjustBtn.setBounds(left.removeFromTop(40).removeFromLeft(160));
    adjustBtn.toFront(false);

    // ---- RIGHT column (Humanize title + 4 sliders) ----
    auto right = columns;
    humanizeTitle.setBounds(right.removeFromTop(24));
    auto slRow = right.removeFromTop(40); timingLbl.setBounds(slRow.removeFromLeft(120)); timingSl.setBounds(slRow.reduced(4));
    slRow = right.removeFromTop(40);      velocityLbl.setBounds(slRow.removeFromLeft(120)); velocitySl.setBounds(slRow.reduced(4));
    slRow = right.removeFromTop(40);      swingLbl.setBounds(slRow.removeFromLeft(120));    swingSl.setBounds(slRow.reduced(4));
    slRow = right.removeFromTop(40);      feelLbl.setBounds(slRow.removeFromLeft(120));     feelSl.setBounds(slRow.reduced(4));

    // Dice button at the top-right
    diceBtn.setBounds(right.removeFromTop(36).removeFromRight(40));
    diceBtn.toFront(false);

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

    // ---- Engine selector row (3 image buttons under the “engine” title image) ----
    auto engineRow = r.removeFromTop(60);
    const int eW = 56, eH = 46, eGap = 16;
    juce::Rectangle<int> engineBar(0, 0, eW * 3 + eGap * 2, eH);
    engineBar = engineBar.withCentre(engineRow.getCentre());
    engineChordsBtn.setBounds(engineBar.removeFromLeft(eW));
    engineBar.removeFromLeft(eGap);
    engineMixtureBtn.setBounds(engineBar.removeFromLeft(eW));
    engineBar.removeFromLeft(eGap);
    engineMelodyBtn.setBounds(engineBar.removeFromLeft(eW));

    // ---- Piano roll (header height ≈ 28) ----
    const int headerH = 28;
    auto header = r.removeFromTop(headerH);
    // if you draw bar numbers yourself, set bounds here for that component

    // Viewport area for the roll; give the rest of the space except bottom buttons
    auto spaceForRoll = r.removeFromBottom(r.getHeight() - 100);
    rollView.setBounds(spaceForRoll);
    rollView.toBack();

    // Update roll content width to match bars × beats (you already have these helpers)
    updateRollContentSize();   // keeps the content wider than the viewport as needed :contentReference[oaicite:1]{index=1}

    // ---- Bottom big buttons ----
    const int bigW = 300, bigH = 72;
    auto bottom = r;
    auto leftBig = bottom.removeFromLeft(getWidth() / 2).removeFromRight(bigW).withSizeKeepingCentre(bigW, bigH);
    auto rightBig = bottom.removeFromRight(bigW).withSizeKeepingCentre(bigW, bigH);
    generateBtn.setBounds(leftBig);
    dragBtn.setBounds(rightBig);

    // Ensure all buttons sit above the roll
    for (auto* b : { &engineChordsBtn, &engineMixtureBtn, &engineMelodyBtn,
                     &generateBtn, &dragBtn, &polyrBtn, &reharmBtn,
                     &adjustBtn, &diceBtn })
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
    const float timingAmt = (float)lim01(timingSl.getValue());
    const float velAmt = (float)lim01(velocitySl.getValue());
    const float feelAmt = (float)lim01(feelSl.getValue());
    const float swingAmt = (float)lim01(swingSl.getValue());

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
    case EngineSel::Chords:  lastChords = gen.generateChordTrack();                  break;
    case EngineSel::Melody:  lastMelody = gen.generateMelody();                      break;
    case EngineSel::Mixture:
    default:
    {
        auto mix = gen.generateMelodyAndChords(true);
        lastMelody = std::move(mix.melody);
        lastChords = std::move(mix.chords);
    } break;
    }

    // If your PianoRoll has an API to set notes, call it here (keep names you already use)
    // pianoRoll.setNotes(lastMelody, lastChords);
    pianoRoll.repaint();
}

void BANGAudioProcessorEditor::randomizeAll()
{
    // Randomize Key
    keyBox.setSelectedItemIndex(juce::Random::getSystemRandom().nextInt(keyBox.getNumItems()));

    // Randomize Scale
    scaleBox.setSelectedItemIndex(juce::Random::getSystemRandom().nextInt(scaleBox.getNumItems()));

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
    auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("BANG_Export", ".mid");

    juce::MidiMessageSequence seq;

    auto addNotes = [&](const std::vector<Note>& src)
    {
        for (const auto& n : src)
        {
            const double startBeat = n.startBeats;       // Note struct in your project uses .beat / .len / .pitch / .vel
            const double lenBeat = n.lengthBeats;
            const int    note = n.pitch;
            const int    vel = juce::jlimit(1, 127, n.velocity);

            const double qnPerBeat = 1.0; // if your generator is beat==quarter notes; adjust if needed
            const double t0 = startBeat * qnPerBeat;
            const double t1 = (startBeat + lenBeat) * qnPerBeat;

            seq.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)vel), t0);
            seq.addEvent(juce::MidiMessage::noteOff(1, note), t1);
        }
    };

    addNotes(lastChords);
    addNotes(lastMelody);

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote(960);
    mf.addTrack(seq);

    juce::FileOutputStream out(temp);
    if (out.openedOk())
    {
        mf.writeTo(out);
        out.flush();
    }
    return temp;
}

void BANGAudioProcessorEditor::performDragExport()
{
    auto f = writeTempMidiForDrag();
    if (f.existsAsFile())
        performExternalDragDropOfFiles({ f.getFullPathName() }, true);
}

void BANGAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
}

void BANGAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
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
