#include "PluginEditor.h"
#include "MidiGenerator.h"
#include "AdvancedHarmonyWindow.h"

// #include "BinaryData.h" // No longer needed, loading from filesystem.

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


// ===== image loading helper that tries multiple filenames =====
static juce::Image loadImageAny(std::initializer_list<const char*> names)
{
    for (auto* name : names)
    {
        if (name == nullptr || *name == '\0') continue;

        auto imageFile = getResourceFile(name);
        if (imageFile.existsAsFile())
        {
            if (auto img = juce::ImageFileFormat::loadFrom(imageFile); img.isValid())
            {
                DBG("Loaded image: " << name << "  (" << img.getWidth() << "x" << img.getHeight() << ")");
                return img;
            }
        }
    }
    return juce::Image();
}

// Overload so you can pass std::vector<const char*> too
static juce::Image loadImageAny(const std::vector<const char*>& names)
{
    for (auto* name : names)
    {
        if (name == nullptr || *name == '\0') continue;

        auto imageFile = getResourceFile(name);
        if (imageFile.existsAsFile())
        {
            if (auto img = juce::ImageFileFormat::loadFrom(imageFile); img.isValid())
            {
                DBG("Loaded image: " << name << "  (" << img.getWidth() << "x" << img.getHeight() << ")");
                return img;
            }
        }
    }
    return juce::Image();
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

    setImageButton3(btn, normal, over, down);
}

// ======== Filesystem image helpers ========

// fuzzy lookup: tries <hint>.png, <hint>_hover.png, etc. by scanning the Resources directory.
static juce::Image loadBinaryImage(const juce::String& nameLike)
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
    addAndMakeVisible(selectEngineImg);
    if (auto eimg = loadImageByHint("selectEngine"); eimg.isValid())
        selectEngineImg.setImage(eimg);
    selectEngineImg.setImagePlacement(juce::RectanglePlacement::centred);

    // ----- Left: Key / Scale / TS / Bars -----
    auto stylePicker = [pickerBg, outline](juce::ComboBox& cb)
    {
        cb.setColour(juce::ComboBox::backgroundColourId, pickerBg);
        cb.setColour(juce::ComboBox::textColourId, outline);
        cb.setColour(juce::ComboBox::outlineColourId, outline);
        cb.setColour(juce::ComboBox::arrowColourId, outline);
    };

    //keyLbl.setText("KEY", juce::dontSendNotification);
   // scaleLbl.setText("SCALE", juce::dontSendNotification);
    //tsLbl.setText("TIME SIG", juce::dontSendNotification);
   // barsLbl.setText("BARS", juce::dontSendNotification);
    //restLbl.setText("REST %", juce::dontSendNotification);

    //for (auto* l : { &keyLbl, &scaleLbl, &tsLbl, &barsLbl, &restLbl })
   // {
        //     l->setColour(juce::Label::textColourId, juce::Colours::black);
        //     l->setJustificationType(juce::Justification::centredRight);
        //     addAndMakeVisible(*l);
   // }

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
        "Blues","Hungarian Min","Harmonic Maj(Ethiopian)", "Dorian b5", "Phrygian b4", "Lydian b3", "Mixolydian b2",
        "Lydian Aug2", "Locrian bb7", "Pentatonic Maj","Pentatonic Min","Neopolitan Maj",
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
    restSl.setValue(15.0);
    restSl.addListener(this);
    addAndMakeVisible(restSl);

    // Octave Box
    octaveBox.addItem("C0", 1);
    octaveBox.addItem("C1", 2);
    octaveBox.addItem("C2", 3);
    octaveBox.addItem("C3", 4);
    octaveBox.addItem("C4", 5);
    octaveBox.addItem("C5", 6);
    octaveBox.setSelectedId(4);
    stylePicker(octaveBox); octaveBox.addListener(this); addAndMakeVisible(octaveBox);

    addAndMakeVisible(keyLblImg);
    addAndMakeVisible(scaleLblImg);
    addAndMakeVisible(timeSigLblImg);
    addAndMakeVisible(barsLblImg);
    addAndMakeVisible(restDensityLblImg);
    addAndMakeVisible(humanizeLblImg);
    addAndMakeVisible(octaveLblImg);

    keyLblImg.setInterceptsMouseClicks(false, false);
    scaleLblImg.setInterceptsMouseClicks(false, false);
    timeSigLblImg.setInterceptsMouseClicks(false, false);
    barsLblImg.setInterceptsMouseClicks(false, false);
    restDensityLblImg.setInterceptsMouseClicks(false, false);
    humanizeLblImg.setInterceptsMouseClicks(false, false);
    octaveLblImg.setInterceptsMouseClicks(false, false);


    // ----- Right: Humanize -----
    //humanizeTitle.setText("HUMANIZE", juce::dontSendNotification);
    humanizeTitle.setColour(juce::Label::textColourId, accent);
    addAndMakeVisible(humanizeTitle);

    auto styleHumanSlider = [accent](juce::Slider& s)
    {
        s.setRange(0.0, 100.0, 1.0);
        s.setColour(juce::Slider::trackColourId, accent);
        s.setColour(juce::Slider::thumbColourId, accent);
        s.setColour(juce::Slider::textBoxTextColourId, accent);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
    };

   // addAndMakeVisible(timingLbl);   timingLbl.setText("HUMANIZE TIMING", juce::dontSendNotification);
    //addAndMakeVisible(velocityLbl); velocityLbl.setText("VELOCITY", juce::dontSendNotification);
    //addAndMakeVisible(swingLbl);    swingLbl.setText("SWING", juce::dontSendNotification);
    //addAndMakeVisible(feelLbl);     feelLbl.setText("FEEL", juce::dontSendNotification);
    for (auto* l : { &timingLbl, &velocityLbl, &swingLbl, &feelLbl })
        l->setColour(juce::Label::textColourId, juce::Colours::black);

    styleHumanSlider(restSl);
    styleHumanSlider(timingSl);   timingSl.setValue(40.0); addAndMakeVisible(timingSl);   timingSl.addListener(this);
    styleHumanSlider(velocitySl); velocitySl.setValue(35.0); addAndMakeVisible(velocitySl); velocitySl.addListener(this);
    styleHumanSlider(swingSl);    swingSl.setValue(25.0); addAndMakeVisible(swingSl);    swingSl.addListener(this);
    styleHumanSlider(feelSl);     feelSl.setValue(30.0); addAndMakeVisible(feelSl);     feelSl.addListener(this);

    // ----- Engine buttons (image buttons with 3 states) -----
    addAndMakeVisible(engineChordsBtn);   setImageButton3(engineChordsBtn, "enginechords");
    addAndMakeVisible(engineMixtureBtn);  setImageButton3(engineMixtureBtn, "enginemixture");
    addAndMakeVisible(engineMelodyBtn);   setImageButton3(engineMelodyBtn, "enginemelody");

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
    addAndMakeVisible(diceBtn);     setImageButton3(diceBtn, "diceBtn");

    // helper to load an image from BinaryData by name
    auto loadPng = [](const void* data, int size)
    {
        return juce::ImageCache::getFromMemory(data, size);
    };

    // KEY
    {
        auto img = loadPng(BinaryData::keyLbl_png, BinaryData::keyLbl_pngSize);
        keyLblImg.setImage(img); // JUCE 7/8: single-arg setImage
        keyLblImg.setImagePlacement(juce::RectanglePlacement::xRight);
        addAndMakeVisible(keyLblImg);
    }

    // SCALE
    {
        auto img = loadPng(BinaryData::scaleLbl_png, BinaryData::scaleLbl_pngSize);
        scaleLblImg.setImage(img);
        scaleLblImg.setImagePlacement(juce::RectanglePlacement::xRight);
        addAndMakeVisible(scaleLblImg);
    }

    // TIME SIG
    {
        auto img = loadPng(BinaryData::timesigLbl_png, BinaryData::timesigLbl_pngSize);
        timeSigLblImg.setImage(img);
        timeSigLblImg.setImagePlacement(juce::RectanglePlacement::xRight);
        addAndMakeVisible(timeSigLblImg);
    }

    // BARS
    {
        auto img = loadPng(BinaryData::barsLbl_png, BinaryData::barsLbl_pngSize);
        barsLblImg.setImage(img);
        barsLblImg.setImagePlacement(juce::RectanglePlacement::xRight);
        addAndMakeVisible(barsLblImg);
    }

    // REST DENSITY
    {
        auto img = loadPng(BinaryData::restdensityLbl_png, BinaryData::restdensityLbl_pngSize);
        restDensityLblImg.setImage(img);
        restDensityLblImg.setImagePlacement(juce::RectanglePlacement::xRight);
        addAndMakeVisible(restDensityLblImg);
    }

    // OCTAVE
    {
        auto img = loadPng(BinaryData::octaveLbl_png, BinaryData::octaveLbl_pngSize);
        octaveLblImg.setImage(img);
        octaveLblImg.setImagePlacement(juce::RectanglePlacement::xRight);
        addAndMakeVisible(octaveLblImg);
    }

    // HUMANIZE header (centered above the sliders)
    {
        auto img = loadPng(BinaryData::humanizeLbl_png, BinaryData::humanizeLbl_pngSize);
        humanizeLblImg.setImage(img);
        humanizeLblImg.setImagePlacement(juce::RectanglePlacement::centred);
        addAndMakeVisible(humanizeLblImg);
    }


    // --- Generate button click: push UI -> generator, then generate, then show on roll
    generateBtn.onClick = [this]
    {
        // Make sure generator uses the latest UI settings (key/scale/TS/bars/rest/humanize/engine etc.)
        pushSettingsToGenerator();

        // Do the generation
        regenerate();
    };

    dragBtn.onClick = [this] { performDragExport(); };
    advancedBtn.onClick = [this] { openAdvanced(); };
    polyrBtn.onClick = [this] { openPolyrhythm(); };
    reharmBtn.onClick = [this] { openReharmonize(); };
    adjustBtn.onClick = [this] { openAdjust(); };
    diceBtn.onClick = [this] { randomizeAll(); regenerate(); };

    // Store natural bounds of buttons
    if (auto img = loadImageByHint("generate"); img.isValid())
        generateBtnNaturalBounds = img.getBounds();
    if (auto img = loadImageByHint("drag"); img.isValid())
        dragBtnNaturalBounds = img.getBounds();

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
    // ---- Compact combo layout constants (put at very top of resized()) ----
    const int pad = 12;          // general padding
    const int rowGap = 12;          // vertical gap between rows
      
    const int labelImgW = 92;          // width of the left orange image labels
    const int labelImgH = 36;          // height of the left orange image labels
    
    const int comboW = 160;
    const int comboH = 28;

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

    // ---- NEW: Define a responsive left column area ----
    const int leftColumnWidth = labelImgW + 10 + comboW;
    auto leftColumnArea = r.withX(r.getX() + pad + 6).withWidth(leftColumnWidth);

    // ---- Lay out the left column (combo boxes and labels) ----
    int yCombos = 120; // This can remain a fixed starting Y offset from the top of the component
    
    // KEY
    keyLblImg.setBounds(leftColumnArea.getX(), yCombos, labelImgW, labelImgH);
    keyBox.setBounds(leftColumnArea.getX() + labelImgW + 10, yCombos, comboW, comboH);
    yCombos += comboH + rowGap;

    // SCALE
    scaleLblImg.setBounds(leftColumnArea.getX(), yCombos, labelImgW, labelImgH);
    scaleBox.setBounds(leftColumnArea.getX() + labelImgW + 10, yCombos, comboW, comboH);
    yCombos += comboH + rowGap;

    // TIME SIG
    timeSigLblImg.setBounds(leftColumnArea.getX(), yCombos, labelImgW, labelImgH);
    tsBox.setBounds(leftColumnArea.getX() + labelImgW + 10, yCombos, comboW, comboH);
    yCombos += comboH + rowGap;

    // BARS
    barsLblImg.setBounds(leftColumnArea.getX(), yCombos, labelImgW, labelImgH);
    barsBox.setBounds(leftColumnArea.getX() + labelImgW + 10, yCombos, comboW, comboH);
    yCombos += comboH + rowGap;

    // REST DENSITY
    restDensityLblImg.setBounds(leftColumnArea.getX(), yCombos, labelImgW, labelImgH);
    restSl.setBounds(leftColumnArea.getX() + labelImgW + 10, yCombos, comboW, comboH);
    yCombos += comboH + rowGap;

    // OCTAVE
    octaveLblImg.setBounds(leftColumnArea.getX(), yCombos, labelImgW, labelImgH);
    octaveBox.setBounds(leftColumnArea.getX() + labelImgW + 10, yCombos, comboW, comboH);
    yCombos += comboH + rowGap;


    // ---- Place the logo centered horizontally and vertically between the control columns ----
    
    // --- Vertical Centering ---
    const int leftColTop = 120;
    const int lastRelevantRowY = 120 + 5 * (comboH + rowGap);
    const int leftColBottom = lastRelevantRowY + comboH;

    const int rightColTop = r.getY() + 4; // +4 for .reduced(4)
    const int rightColBottom = r.getY() + (4 * 40) - 4; // y + height of 4 rows - reduction

    const int overallTop = juce::jmin(leftColTop, rightColTop);
    const int overallBottom = juce::jmax(leftColBottom, rightColBottom);
    const int gapMidY = overallTop + (overallBottom - overallTop) / 2;

    // --- Horizontal Centering & Sizing ---
    const int leftColEndX = leftColumnArea.getRight(); // Use the new responsive area
    const int rightColStartX = r.getX() + (r.getWidth() / 2) + 120; // 120 for the invisible label
    
    int logoW = 300; // Fixed width for the logo
    int logoH = 100; // Default height

    if (auto img = logoImg.getImage(); img.isValid())
    {
        const double ar = img.getWidth() > 0 ? (double)img.getWidth() / (double)img.getHeight() : 3.0;
        logoH = juce::roundToInt(logoW / ar);
    }

    const int gapMidX = leftColEndX + (rightColStartX - leftColEndX) / 2;
    const int logoX = gapMidX - logoW / 2;

    logoImg.setBounds(logoX, gapMidY - logoH / 2, logoW, logoH);

    r.removeFromTop(40); // Some space

    // Area for right column (humanize)
    auto right = r.withX(rightColStartX).withRight(r.getRight());


    // ---- RIGHT column (Humanize title + 4 sliders) ----
    const int humanizeSliderWidth = 250;
    const int sliderHeight = 22;

    auto layOutHumanizeSlider = [&](juce::Slider& slider) {
        auto slRow = right.removeFromTop(40);
        auto labelBounds = slRow.removeFromLeft(120);
        // The label component itself is not visible, but we reserve its space.
        
        // Center the slider in the remaining space of the row.
        slider.setBounds(slRow.withSizeKeepingCentre(humanizeSliderWidth, sliderHeight));
    };

    layOutHumanizeSlider(timingSl);
    layOutHumanizeSlider(velocitySl);
    layOutHumanizeSlider(swingSl);
    layOutHumanizeSlider(feelSl);

    // --- Center HUMANIZE label image above the humanize sliders ---
    juce::Rectangle<int> humanizeArea = timingSl.getBounds().getUnion(feelSl.getBounds());

    const int humanizeH = 28;     // height of the HUMANIZE PNG
    const int humanizeW = 180;    // width of the HUMANIZE PNG
    const int humanizeTopPad = 8; // gap above first slider

    humanizeLblImg.setBounds(
        humanizeArea.getCentreX() - humanizeW / 2,
        humanizeArea.getY() - humanizeH - humanizeTopPad,
        humanizeW,
        humanizeH
    );

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
    float genNatW = generateBtnNaturalBounds.getWidth();
    float genNatH = generateBtnNaturalBounds.getHeight();
    if (genNatW == 0) genNatW = 2;
    if (genNatH == 0) genNatH = 2;

    float genScale = juce::jmin((float)maxBounds.getWidth() / genNatW, (float)maxBounds.getHeight() / genNatH);
    int genScaledW = (int)(genNatW * genScale);
    int genScaledH = (int)(genNatH * genScale);

    // --- Drag Button ---
    float dragNatW = dragBtnNaturalBounds.getWidth();
    float dragNatH = dragBtnNaturalBounds.getHeight();
    if (dragNatW == 0) dragNatW = 2;
    if (dragNatH == 0) dragNatH = 2;

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

    //TOOLTIPS
    diceBtn.setTooltip("Randomizes all settings (except for bars and octave) and generates MIDI based on those.");
    engineMelodyBtn.setTooltip("Generates a melody based on the selected settings.");
    engineChordsBtn.setTooltip("Generates a chord progression based on the selected settings.");
    engineMixtureBtn.setTooltip("Generates a mixture of melodies, pieces of chords, and chords based on the selected settings. Think of this as a riff generator.");
    keyBox.setTooltip("Select key.");
    scaleBox.setTooltip("Select scale.");
    tsBox.setTooltip("Select time signature.");
    barsBox.setTooltip("Select how long your melody/chord progression/mixture will be.");
    restSl.setTooltip("Select from 0-100 the amount of space between notes in the MIDI that will be generated. 0 is less, 100 is more.");
    octaveBox.setTooltip("Select the octave in which your MIDI will be *primarily* based in.");
    timingSl.setTooltip("Adjust the timing of the MIDI to be more natural, falling before and after beats randomly. 0 is less, 100 is more.");
    swingSl.setTooltip("Add some swing to the rhythm. 0 is less, 100 is more.");
    velocitySl.setTooltip("Adjust the velocity's dynamics to have more changes throughout the MIDI generated. 0 is less, 100 is more.");
    feelSl.setTooltip("Adds a shuffle-like rhythm to the MIDI generated. 0 is less, 100 is more.");
    advancedBtn.setTooltip("Add ext chords, sus chords, alt chords, slash chords, secondary dominants, borrowed chords, chromatic mediants, and tritone subs. All with a density slider. 0 is less, 100 is more.");
    polyrBtn.setTooltip("Pick a polyrhythm ratio and adjust the amount of it in the MIDI generated.");
    reharmBtn.setTooltip("Pick between reharmonizing 4 or all 8 bars of the MIDI generated to a different scale, mode, etc.");
    adjustBtn.setTooltip("Choose to keep settings you want, discard others you DON'T want, and regenerate!");

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
    const int octave = octaveBox.getSelectedId(); // keep whatever you already use for denominator
    const double humanTiming = timingSl.getValue();
    const double humanVel = velocitySl.getValue();
    const double swingAmt = swingSl.getValue();
    const double feelAmt  = feelSl.getValue();

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
}

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
        juce::Slider densitySlider;

        juce::ToggleButton poly23Btn, poly32Btn, poly34Btn, poly43Btn;

        juce::ImageButton diceBtn;
        juce::ImageButton enablepassLblBtn;

        juce::ImageComponent polyLbl;
        juce::ImageComponent typeLbl;
        juce::ImageComponent thirtyTwoLbl;
        juce::ImageComponent twentyThreeLbl;
        juce::ImageComponent fortyThreeLbl;
        juce::ImageComponent thirtyFourLbl;
        juce::ImageComponent densityLbl;

        PolyComp(BANGAudioProcessorEditor& ed) : editor(ed)
        {
            // Slider
            addAndMakeVisible(densitySlider);
            densitySlider.setRange(0.0, 100.0, 1.0);
            densitySlider.setValue(ed.swingSl.getValue());
            densitySlider.onValueChange = [this]
            {
                editor.swingSl.setValue(densitySlider.getValue(), juce::sendNotification);
                editor.pushSettingsToGenerator();
            };

            const auto accent = juce::Colour::fromRGB(0xDD, 0x4F, 0x02);
            densitySlider.setColour(juce::Slider::trackColourId, accent);
            densitySlider.setColour(juce::Slider::thumbColourId, accent);
            densitySlider.setColour(juce::Slider::textBoxTextColourId, accent);
            densitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);

            // Checkboxes
            addAndMakeVisible(poly23Btn);
            addAndMakeVisible(poly32Btn);
            addAndMakeVisible(poly34Btn);
            addAndMakeVisible(poly43Btn);
            
            poly23Btn.setRadioGroupId(2002);
            poly32Btn.setRadioGroupId(2002);
            poly34Btn.setRadioGroupId(2002);
            poly43Btn.setRadioGroupId(2002);
            
            poly23Btn.setToggleState(true, juce::dontSendNotification);

            // Images
            polyLbl.setImage(loadImageByHint("polyLbl"));
            typeLbl.setImage(loadImageByHint("typeLbl"));
            thirtyTwoLbl.setImage(loadImageByHint("32Lbl"));
            twentyThreeLbl.setImage(loadImageByHint("23Lbl"));
            fortyThreeLbl.setImage(loadImageByHint("43Lbl"));
            thirtyFourLbl.setImage(loadImageByHint("34Lbl"));
            densityLbl.setImage(loadImageByHint("densityLbl"));

            setImageButton3(diceBtn, "diceBtn");
            setImageButton3(enablepassLblBtn, "enablepassLbl");

            addAndMakeVisible(polyLbl);
            addAndMakeVisible(typeLbl);
            addAndMakeVisible(thirtyTwoLbl);
            addAndMakeVisible(twentyThreeLbl);
            addAndMakeVisible(fortyThreeLbl);
            addAndMakeVisible(thirtyFourLbl);
            addAndMakeVisible(densityLbl);
            addAndMakeVisible(diceBtn);
            addAndMakeVisible(enablepassLblBtn);

            polyLbl.setImagePlacement(juce::RectanglePlacement::centred);
            typeLbl.setImagePlacement(juce::RectanglePlacement::centred);
            thirtyTwoLbl.setImagePlacement(juce::RectanglePlacement::centred);
            twentyThreeLbl.setImagePlacement(juce::RectanglePlacement::centred);
            fortyThreeLbl.setImagePlacement(juce::RectanglePlacement::centred);
            thirtyFourLbl.setImagePlacement(juce::RectanglePlacement::centred);
            densityLbl.setImagePlacement(juce::RectanglePlacement::centred);
        }
        void resized() override
        {
            auto bounds = getLocalBounds();
            diceBtn.setBounds(bounds.removeFromTop(44).removeFromRight(44));
            bounds.reduce(20, 20);

            juce::FlexBox fb;
            fb.flexDirection = juce::FlexBox::Direction::column;
            fb.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
            fb.alignItems = juce::FlexBox::AlignItems::center;

            fb.items.add(juce::FlexItem(polyLbl).withWidth(150).withHeight(50));
            fb.items.add(juce::FlexItem(typeLbl).withWidth(150).withHeight(50));

            // Horizontal box for the polyrhythm types
            juce::FlexBox typesBox;
            typesBox.flexDirection = juce::FlexBox::Direction::row;
            typesBox.justifyContent = juce::FlexBox::JustifyContent::center;
            typesBox.alignItems = juce::FlexBox::AlignItems::center;
            
            typesBox.items.add(juce::FlexItem(twentyThreeLbl).withWidth(74).withHeight(43));
            typesBox.items.add(juce::FlexItem(poly23Btn).withWidth(30).withHeight(30));
            typesBox.items.add(juce::FlexItem(thirtyTwoLbl).withWidth(74).withHeight(43));
            typesBox.items.add(juce::FlexItem(poly32Btn).withWidth(30).withHeight(30));
            typesBox.items.add(juce::FlexItem(thirtyFourLbl).withWidth(74).withHeight(43));
            typesBox.items.add(juce::FlexItem(poly34Btn).withWidth(30).withHeight(30));
            typesBox.items.add(juce::FlexItem(fortyThreeLbl).withWidth(74).withHeight(43));
            typesBox.items.add(juce::FlexItem(poly43Btn).withWidth(30).withHeight(30));

            fb.items.add(juce::FlexItem(typesBox).withFlex(1).withMaxHeight(60));

            // Horizontal box for the density slider
            juce::FlexBox densityBox;
            densityBox.flexDirection = juce::FlexBox::Direction::row;
            densityBox.justifyContent = juce::FlexBox::JustifyContent::center;
            densityBox.alignItems = juce::FlexBox::AlignItems::center;
            densityBox.items.add(juce::FlexItem(densityLbl).withWidth(150).withHeight(50));
            densityBox.items.add(juce::FlexItem(densitySlider).withWidth(200).withHeight(50));

            fb.items.add(juce::FlexItem(densityBox).withFlex(1).withMaxHeight(50));
            fb.items.add(juce::FlexItem(enablepassLblBtn).withWidth(300).withHeight(200));

            fb.performLayout(bounds);
        }

        void paint(juce::Graphics& g)
        {
            g.fillAll(juce::Colour::fromRGB(0xA5, 0xDD, 0x00));
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
        w->centreWithSize(400, 500);
    

}

void BANGAudioProcessorEditor::openReharmonize()
{
    // Minimal reharm UI mapping to your existing controls (complexity via FEEL here)
    struct ReharmonizeComponent : public juce::Component
    {
        BANGAudioProcessorEditor& editor;

        juce::ImageComponent titleLbl, newKeyLbl, newScaleLbl, keepFirst4BarsLbl;
        juce::ComboBox keyBox, scaleBox;
        juce::ToggleButton keepBarsToggle;
        juce::ImageButton regenerateBtn, diceBtn;

        ReharmonizeComponent(BANGAudioProcessorEditor& ed) : editor(ed)
        {
            // Load images for labels

            // Load images for labels
            if (auto img = loadImageByHint("reharmLbl"); img.isValid())
                titleLbl.setImage(img);
            if (auto img = loadImageByHint("newKeyLbl"); img.isValid())
                newKeyLbl.setImage(img);
            if (auto img = loadImageByHint("newScaleLbl"); img.isValid())
                newScaleLbl.setImage(img);
            if (auto img = loadImageByHint("keepFirst4BarsLbl"); img.isValid())
                keepFirst4BarsLbl.setImage(img);

            addAndMakeVisible(titleLbl);
            addAndMakeVisible(newKeyLbl);
            addAndMakeVisible(newScaleLbl);
            addAndMakeVisible(keepFirst4BarsLbl);

            // Configure combo boxes
            auto stylePicker = [](juce::ComboBox& cb)
            {
                const auto pickerBg = juce::Colour::fromRGB(0xF9, 0xBF, 0x00);
                const auto outline = juce::Colours::black;
                cb.setColour(juce::ComboBox::backgroundColourId, pickerBg);
                cb.setColour(juce::ComboBox::textColourId, outline);
                cb.setColour(juce::ComboBox::outlineColourId, outline);
                cb.setColour(juce::ComboBox::arrowColourId, outline);
            };

            for (int i = 0; i < editor.keyBox.getNumItems(); ++i)
                keyBox.addItem(editor.keyBox.getItemText(i), editor.keyBox.getItemId(i));
            keyBox.setSelectedId(editor.keyBox.getSelectedId());
            stylePicker(keyBox);
            addAndMakeVisible(keyBox);

            for (int i = 0; i < editor.scaleBox.getNumItems(); ++i)
                scaleBox.addItem(editor.scaleBox.getItemText(i), editor.scaleBox.getItemId(i));
            scaleBox.setSelectedId(editor.scaleBox.getSelectedId());
            stylePicker(scaleBox);
            addAndMakeVisible(scaleBox);

            // Configure toggle button
            addAndMakeVisible(keepBarsToggle);

            // Configure buttons
            addAndMakeVisible(regenerateBtn);
            setImageButton3(regenerateBtn, "regenerateBtn");

            addAndMakeVisible(diceBtn);
            setImageButton3(diceBtn, "diceBtn");
        }

       
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour::fromRGB(0xA5, 0xDD, 0x00));
            g.setColour(juce::Colours::black);
            g.drawText("REHARMONIZE", getLocalBounds().removeFromTop(100), juce::Justification::centred);

        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            
            // Top-right dice button
            diceBtn.setBounds(getWidth() - 50, 10, 40, 40);

            auto topArea = bounds.removeFromTop(100);
            titleLbl.setBounds(topArea.withSizeKeepingCentre(300, 80));

            bounds = bounds.reduced(30);

            auto row = bounds.removeFromTop(50);
            newKeyLbl.setBounds(row.removeFromLeft(row.getWidth() / 2).reduced(5));
            keyBox.setBounds(row.reduced(5));

            bounds.removeFromTop(20);

            row = bounds.removeFromTop(50);
            newScaleLbl.setBounds(row.removeFromLeft(row.getWidth() / 2).reduced(5));
            scaleBox.setBounds(row.reduced(5));

            bounds.removeFromTop(20);

            row = bounds.removeFromTop(50);
            keepFirst4BarsLbl.setBounds(row.removeFromLeft(row.getWidth() / 2).reduced(5));
            keepBarsToggle.setBounds(row.withSizeKeepingCentre(30,30));
            
            bounds.removeFromTop(40);

            juce::Rectangle<int> newBounds(10, 20, 200, 300);

            regenerateBtn.setBounds(newBounds);
        }
    };

    auto* comp = new ReharmonizeComponent(*this);
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Reharmonize";
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.content.setOwned(comp);
    if (auto* w = opts.launchAsync())
        w->centreWithSize(500, 550);
}

void BANGAudioProcessorEditor::openAdjust()
{
    struct AdjustWindow : public juce::Component
    {
        BANGAudioProcessorEditor& editor;

        juce::ImageComponent adjustLblImg;
        juce::ImageButton diceBtn;

        juce::ImageComponent keyLblImg, scaleLblImg, timeSigLblImg, barsLblImg, restDensityLblImg, octaveLblImg;
        juce::ComboBox keyBox, scaleBox, tsBox, barsBox, octaveBox;
        juce::Slider restSl;

        juce::ImageComponent keepAdvancedLbl, keepPolyrLbl, keepReharmLbl;
        juce::ToggleButton keepAdvancedToggle, keepPolyrToggle, keepReharmToggle;

        juce::ImageButton regenerateBtn;

        AdjustWindow(BANGAudioProcessorEditor& ed) : editor(ed)
        {
            // Load images
            if (auto img = loadImageByHint("adjustLbl"); img.isValid())
                adjustLblImg.setImage(img);
            if (auto img = loadImageByHint("diceBtn"); img.isValid())
                diceBtn.setImages(true, true, true, img, 1.0f, juce::Colours::transparentBlack, img, 1.0f, juce::Colours::transparentBlack, img, 1.0f, juce::Colours::transparentBlack);
            if (auto img = loadImageByHint("keyLbl"); img.isValid())
                keyLblImg.setImage(img);
            if (auto img = loadImageByHint("scaleLbl"); img.isValid())
                scaleLblImg.setImage(img);
            if (auto img = loadImageByHint("timesigLbl"); img.isValid())
                timeSigLblImg.setImage(img);
            if (auto img = loadImageByHint("barsLbl"); img.isValid())
                barsLblImg.setImage(img);
            if (auto img = loadImageByHint("restDensityLbl"); img.isValid())
                restDensityLblImg.setImage(img);
            if (auto img = loadImageByHint("octaveLbl"); img.isValid())
                octaveLblImg.setImage(img);
            if (auto img = loadImageByHint("keepAdvancedLbl"); img.isValid())
                keepAdvancedLbl.setImage(img);
            if (auto img = loadImageByHint("keepPolyrLbl"); img.isValid())
                keepPolyrLbl.setImage(img);
            if (auto img = loadImageByHint("keepReharmLbl"); img.isValid())
                keepReharmLbl.setImage(img);

            setImageButton3(regenerateBtn, "regenerate");

            // Style and populate combo boxes
            auto stylePicker = [](juce::ComboBox& cb)
            {
                const auto pickerBg = juce::Colour::fromRGB(0xF9, 0xBF, 0x00);
                const auto outline = juce::Colours::black;
                cb.setColour(juce::ComboBox::backgroundColourId, pickerBg);
                cb.setColour(juce::ComboBox::textColourId, outline);
                cb.setColour(juce::ComboBox::outlineColourId, outline);
                cb.setColour(juce::ComboBox::arrowColourId, outline);
            };

            auto copyComboBoxItems = [](juce::ComboBox& dest, const juce::ComboBox& src)
            {
                for (int i = 0; i < src.getNumItems(); ++i)
                    dest.addItem(src.getItemText(i), src.getItemId(i));
                dest.setSelectedId(src.getSelectedId());
            };

            copyComboBoxItems(keyBox, editor.keyBox);
            stylePicker(keyBox);
            copyComboBoxItems(scaleBox, editor.scaleBox);
            stylePicker(scaleBox);
            copyComboBoxItems(tsBox, editor.tsBox);
            stylePicker(tsBox);
            copyComboBoxItems(barsBox, editor.barsBox);
            stylePicker(barsBox);
            copyComboBoxItems(octaveBox, editor.octaveBox);
            stylePicker(octaveBox);

            // Configure slider
            restSl.setSliderStyle(juce::Slider::LinearHorizontal);
            restSl.setRange(0.0, 100.0, 1.0);
            restSl.setValue(editor.restSl.getValue());
            restSl.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

            // Add all components
            addAndMakeVisible(adjustLblImg);
            addAndMakeVisible(diceBtn);
            addAndMakeVisible(keyLblImg);
            addAndMakeVisible(scaleLblImg);
            addAndMakeVisible(timeSigLblImg);
            addAndMakeVisible(barsLblImg);
            addAndMakeVisible(restDensityLblImg);
            addAndMakeVisible(octaveLblImg);
            addAndMakeVisible(keyBox);
            addAndMakeVisible(scaleBox);
            addAndMakeVisible(tsBox);
            addAndMakeVisible(barsBox);
            addAndMakeVisible(octaveBox);
            addAndMakeVisible(restSl);
            addAndMakeVisible(keepAdvancedLbl);
            addAndMakeVisible(keepPolyrLbl);
            addAndMakeVisible(keepReharmLbl);
            addAndMakeVisible(keepAdvancedToggle);
            addAndMakeVisible(keepPolyrToggle);
            addAndMakeVisible(keepReharmToggle);
            addAndMakeVisible(regenerateBtn);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour::fromRGB(0xA5, 0xDD, 0x00));
        }

        void resized() override
        {
            auto bounds = getLocalBounds();

            // Top bar
            auto topBar = bounds.removeFromTop(60);
            diceBtn.setBounds(topBar.removeFromRight(50).reduced(5));
            adjustLblImg.setBounds(topBar.reduced(5));
            adjustLblImg.setImagePlacement(juce::RectanglePlacement::centred);

            bounds.reduce(20, 0);

            // Main controls
            auto controlsBounds = bounds.removeFromTop(300);

            int rowHeight = 40;
            int labelWidth = 120;
            int controlWidth = 150;
            int rowGap = 10;

            auto createRow = [&]() {
                return controlsBounds.removeFromTop(rowHeight);
            };

            auto layoutRow = [&](juce::Component& label, juce::Component& control) {
                auto row = createRow();
                label.setBounds(row.removeFromLeft(labelWidth).reduced(2));
                control.setBounds(row.removeFromLeft(controlWidth).reduced(2));
            };

            layoutRow(keyLblImg, keyBox);
            layoutRow(scaleLblImg, scaleBox);
            layoutRow(timeSigLblImg, tsBox);
            layoutRow(barsLblImg, barsBox);
            layoutRow(restDensityLblImg, restSl);
            layoutRow(octaveLblImg, octaveBox);

            // 'Keep' settings
            auto keepBounds = bounds.removeFromTop(120);
            keepBounds.reduce(20, 0);
            int keepRowHeight = 30;

            auto layoutKeepRow = [&](juce::Component& label, juce::Component& toggle) {
                auto row = keepBounds.removeFromTop(keepRowHeight);
                label.setBounds(row.removeFromLeft(300).reduced(2));
                toggle.setBounds(row.removeFromLeft(30).reduced(2));
            };

            layoutKeepRow(keepAdvancedLbl, keepAdvancedToggle);
            layoutKeepRow(keepPolyrLbl, keepPolyrToggle);
            layoutKeepRow(keepReharmLbl, keepReharmToggle);

            // Bottom button
            auto buttonBounds = bounds.removeFromBottom(80);
            regenerateBtn.setBounds(buttonBounds.withSizeKeepingCentre(200, 60));
        }
    };

    auto* comp = new AdjustWindow(*this);
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Adjust";
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.content.setOwned(comp);
    if (auto* w = opts.launchAsync())
        w->centreWithSize(500, 600); // A reasonable size for the mockup
}