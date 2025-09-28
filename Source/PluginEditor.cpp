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

// Shift a vector of Note pitches by semitones (clamped to 0..127)
static inline void shiftNotesBySemis(std::vector<Note>& v, int semis)
{
    if (semis == 0) return;
    for (auto& n : v)
        n.pitch = juce::jlimit(0, 127, n.pitch + semis);
}

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
// ======== Filesystem image helpers ========
static juce::Image loadBinaryImage(const juce::String& nameLike)
{
    // FIX: use the Resources directory itself, not its parent
    auto resourcesDir = getResourceFile("");  // <-- was getParentDirectory()

    if (!resourcesDir.isDirectory())
        return {};

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
        if (file.getFileName().containsIgnoreCase(nameLike))
            if (auto img = juce::ImageFileFormat::loadFrom(file); img.isValid())
                return img;

    return {};
}

// ======== Editor ========

BANGAudioProcessorEditor::BANGAudioProcessorEditor(BANGAudioProcessor& p)
    : AudioProcessorEditor(&p),
    audioProcessor(p)
{

    // Initial size (host/standalone can override later; this just sets the first open size)
// Make the editor resizable again

    // Set safe min/max sizes so the piano roll remains visible and the UI doesn't cramp

    // Initial size (host/standalone can override later; this just sets the first open size)

    // ===== soundsDope logo (replaces undo/redo) =====
    addAndMakeVisible(soundsDopeLogo);
    if (auto sdl = loadImageByHint("soundsDopeLbl"); sdl.isValid())
        soundsDopeLogo.setImage(sdl);
    soundsDopeLogo.setImagePlacement(juce::RectanglePlacement::centred);


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

    dragBtn.setTooltip("Click or drag this to your DAW to drop the generated MIDI.");
    dragBtn.setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    dragBtn.addMouseListener(this, true); // <-- IMPORTANT: forward dragBtn's mouse events to the editor

    // Refactored to use loadImageByHint for consistency and to fix missing images.
    auto setupLabelImage = [&](juce::ImageComponent& comp, const juce::String& hint, juce::RectanglePlacement placement)
    {
        if (auto img = loadImageByHint(hint); img.isValid())
        {
            comp.setImage(img);
            comp.setImagePlacement(placement);
            addAndMakeVisible(comp);
        }
    };

    setupLabelImage(keyLblImg, "keyLbl", juce::RectanglePlacement::xRight);
    setupLabelImage(scaleLblImg, "scaleLbl", juce::RectanglePlacement::xRight);
    setupLabelImage(timeSigLblImg, "timesigLbl", juce::RectanglePlacement::xRight);
    setupLabelImage(barsLblImg, "barsLbl", juce::RectanglePlacement::xRight);
    setupLabelImage(restDensityLblImg, "restDensityLbl", juce::RectanglePlacement::xRight);
    setupLabelImage(octaveLblImg, "octaveLbl", juce::RectanglePlacement::xRight);
    setupLabelImage(humanizeLblImg, "humanizeLbl", juce::RectanglePlacement::centred);


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

    // make sure these two lines are present before updateRollContentSize():
    pianoRoll.setTimeSignature(currentTSNumerator(), 4);
    pianoRoll.setBars(currentBars());

    pianoRoll.setVerticalZoom(0.75f);

    // ----- Piano roll (scrollable) -----
    addAndMakeVisible(rollView);
    rollView.setViewedComponent(&pianoRoll, false);   // don't own it; you already own pianoRoll
    rollView.setScrollBarsShown(true, true);          // <-- horizontal + vertical
#if JUCE_MAJOR_VERSION >= 8
    rollView.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
#else
    rollView.setScrollOnDragEnabled(false);
#endif

    pianoRoll.setPitchRange(12, 84);
    pianoRoll.setGrid(currentBars(), currentTSNumerator());

    engineChordsBtn.toFront(true);
    engineMixtureBtn.toFront(true);
    engineMelodyBtn.toFront(true);

    generateBtn.toFront(false);
    dragBtn.toFront(false);
    polyrBtn.toFront(true);
    reharmBtn.toFront(true);
    adjustBtn.toFront(true);
    diceBtn.toFront(true);

    // roll under the buttons just in case

    // Piano palette (your colors)
    {
        PianoRollComponent::Palette pal;
        pal.background = juce::Colour(0xff12210a); // roll dark bg
        pal.gridLine = juce::Colour(0xffdf480f);
        pal.gridWeak = juce::Colour(0xff14230b);
        pal.gridStrong = juce::Colour(0xfff4b701);
        pal.barNumberStrip = juce::Colour(0xfff4b701);
        pal.barNumberText = juce::Colour(0xffd84d02);
        pal.keyboardWhite = juce::Colour(0xffffb607);
        pal.keyboardBlack = juce::Colour(0xff132209);
        pal.noteFill = juce::Colour(0xffa8de00);
        pal.noteOutline = juce::Colour(0xff4b5f0e);
        pal.overlayFill = juce::Colour(0xffde4d00);
        pal.overlayOutline = juce::Colour(0xffe5550a);
        pianoRoll.setPalette(pal);
    }

    // Make our L&F the default so all toggles use the image art automatically
    juce::LookAndFeel::setDefaultLookAndFeel(&bangToggleLAF);

    // OPTIONAL: if you want to enforce consistent sizes for specific toggles,
    // call bangToggleLAF.sizeToggleToArt(toggle, desiredHeight);
    // Example (use your real toggle variables):
    // bangToggleLAF.sizeToggleToArt(enableHumanizeToggle, 72);
    // bangToggleLAF.sizeToggleToArt(chordLockToggle,     72);

    // initial generator sync
    pushSettingsToGenerator();
    updateRollContentSize();
    resized();
    // The initial layout is now triggered from the paint() method
    // to ensure the component has valid bounds.
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
    if (!initialLayoutDone)
    {
        resized();
        initialLayoutDone = true;
    }
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}

void BANGAudioProcessorEditor::resized()
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

    const int toggleH = 200;
    const int toggleW = toggleWidthForHeight(toggleH);

    // --- polyrhythm toggles ---
    poly23Btn.setBounds(poly23Btn.getX(), poly23Btn.getY(), toggleW, toggleH);
    poly32Btn.setBounds(poly32Btn.getX(), poly32Btn.getY(), toggleW, toggleH);
    poly34Btn.setBounds(poly34Btn.getX(), poly34Btn.getY(), toggleW, toggleH);
    poly43Btn.setBounds(poly43Btn.getX(), poly43Btn.getY(), toggleW, toggleH);

    // --- keep* toggles ---
    keepBarsToggle.setBounds(keepBarsToggle.getX(), keepBarsToggle.getY(), toggleW, toggleH);
    keepAdvancedToggle.setBounds(keepAdvancedToggle.getX(), keepAdvancedToggle.getY(), toggleW, toggleH);
    keepPolyrToggle.setBounds(keepPolyrToggle.getX(), keepPolyrToggle.getY(), toggleW, toggleH);
    keepReharmToggle.setBounds(keepReharmToggle.getX(), keepReharmToggle.getY(), toggleW, toggleH);

    // ---- Layout constants ----
    const int pad = 5;
    const int rowGap = 5;
    const int controlColWidth = 240; // Width for left (combos) and right (sliders) columns
    const int harmonyAreaWidth = 800;

    auto bounds = getLocalBounds().reduced(pad);

    // ---- Top row: Undo/Redo, Title, Dice ----
// ---- Top row: Undo/Redo, Title, Dice ----
// ---- Top row: Undo/Redo, Title, Dice ----
// ---- Top row: soundsDope logo (left), Title image (center), Dice (right) ----
    auto topRow = bounds.removeFromTop(60);
    {
        const int iconSize = 44;   // dice size
        const int hPad = 16;    // horizontal padding/gap

        // Right: dice (unchanged)
        auto diceArea = topRow.removeFromRight(iconSize);
        diceBtn.setBounds(diceArea.withSizeKeepingCentre(iconSize, iconSize));

        // Left: soundsDope label image
        // Give it a nice, fixed height and let the width follow the image aspect.
        const int logoH = 44;
        int logoW = 160; // sensible default in case image isn't loaded yet
        if (auto img = soundsDopeLogo.getImage(); img.isValid())
        {
            const double ar = img.getWidth() > 0 && img.getHeight() > 0
                ? (double)img.getWidth() / (double)img.getHeight()
                : 3.0;
            logoW = juce::roundToInt(ar * logoH);
        }

        auto logoSlot = topRow.removeFromLeft(logoW + hPad);
        soundsDopeLogo.setBounds(logoSlot.withSizeKeepingCentre(logoW, logoH));

        // Center: the “Select Engine” title image spans the remaining top row
        topRow.reduce(hPad, 0);
        selectEngineImg.setBounds(topRow);
    }

    
    // ---- Row 1: Engine selector row ----
    auto engineRow = bounds.removeFromTop(60);
    {
        const int eW = 160, eH = 44, eGap = 16;
        juce::Rectangle<int> engineBar(0, 0, eW * 3 + eGap * 2, eH);
        engineBar = engineBar.withCentre(engineRow.getCentre());
        engineMelodyBtn.setBounds(engineBar.removeFromLeft(eW));
        engineBar.removeFromLeft(eGap);
        engineChordsBtn.setBounds(engineBar.removeFromLeft(eW));
        engineBar.removeFromLeft(eGap);
        engineMixtureBtn.setBounds(engineBar.removeFromLeft(eW));
    }
    bounds.removeFromTop(rowGap);

    // ---- Row 2: Main Controls (Left Combos, Center Logo, Right Humanize) ----
    auto mainControlsRow = bounds.removeFromTop(290);
    auto leftCol = mainControlsRow.removeFromLeft(controlColWidth);
    auto rightCol = mainControlsRow.removeFromRight(controlColWidth);
    auto logoCol = mainControlsRow; // What remains in the middle

    // ---- Left Column: Combo boxes and labels ----
    {
        const int labelImgW = 75;
        const int labelImgH = 25;
        const int comboW = 160;
        const int comboH = 17;
        const int itemGap = 5;
        
        auto comboArea = leftCol.withTrimmedTop(48);

        auto createComboRow = [&](juce::ImageComponent& label, juce::Component& combo)
        {
            auto row = comboArea.removeFromTop(labelImgH);
            label.setBounds(row.removeFromLeft(labelImgW));
            combo.setBounds(row.withLeft(label.getRight() + 10).withWidth(comboW).withSizeKeepingCentre(comboW, comboH));
            comboArea.removeFromTop(itemGap);
        };

        createComboRow(keyLblImg, keyBox);
        createComboRow(scaleLblImg, scaleBox);
        createComboRow(timeSigLblImg, tsBox);
        createComboRow(barsLblImg, barsBox);
        createComboRow(restDensityLblImg, restSl);
        createComboRow(octaveLblImg, octaveBox);
    }

    octaveBox.clear();
    octaveBox.addItemList(juce::StringArray{ "C1", "C2", "C3", "C4", "C5" }, 1);
    addAndMakeVisible(octaveBox);

    // initialize UI from processor value (no callback)
    octaveBox.setSelectedItemIndex(audioProcessor.getOctaveChoiceIndex(), juce::dontSendNotification);

    // push UI changes back to the processor
    octaveBox.onChange = [this]
    {
        audioProcessor.setOctaveChoiceIndex(octaveBox.getSelectedItemIndex());

        const int semis = audioProcessor.getOctaveShiftSemitones();

        switch (engineSel)
        {
        case EngineSel::Chords:
        {
            auto view = lastChords; // copy
            for (auto& n : view) n.pitch = juce::jlimit(0, 127, n.pitch + semis);
            pianoRoll.setNotes(view);
            break;
        }
        case EngineSel::Melody:
        {
            auto view = lastMelody; // copy
            for (auto& n : view) n.pitch = juce::jlimit(0, 127, n.pitch + semis);
            pianoRoll.setNotes(view);
            break;
        }
        case EngineSel::Mixture:
        default:
        {
            std::vector<Note> combined;
            combined.reserve(lastMelody.size() + lastChords.size());
            combined.insert(combined.end(), lastMelody.begin(), lastMelody.end());
            combined.insert(combined.end(), lastChords.begin(), lastChords.end());
            for (auto& n : combined) n.pitch = juce::jlimit(0, 127, n.pitch + semis);
            pianoRoll.setNotes(combined);
            break;
        }
        }

        pianoRoll.repaint();
    };

    // ---- Right Column: Humanize sliders and Polyrhythm button ----
    {
        const int humanizeSliderWidth = 250;
        const int sliderHeight = 30;
        const int humanizeW = 180, humanizeH = 28, humanizeTopPad = 8;
        
        auto humanizeArea = rightCol.withTrimmedTop(20);
        
        // Humanize Label
        humanizeLblImg.setBounds(humanizeArea.getCentreX() - humanizeW / 2, humanizeArea.getY(), humanizeW, humanizeH);
        humanizeArea.removeFromTop(humanizeH + humanizeTopPad);

        auto layOutHumanizeSlider = [&](juce::Slider& slider) {
            auto slRow = humanizeArea.removeFromTop(40);
            slider.setBounds(slRow.withSizeKeepingCentre(humanizeSliderWidth, sliderHeight));
        };

        layOutHumanizeSlider(timingSl);
        layOutHumanizeSlider(velocitySl);
        layOutHumanizeSlider(swingSl);
        layOutHumanizeSlider(feelSl);

        // Polyrhythm button below sliders
        polyrBtn.setBounds(humanizeArea.withSizeKeepingCentre(140, 44));
    }

    // ---- Center Logo ----
    {
        int logoW = 350;
        int logoH = 450;
        if (auto img = logoImg.getImage(); img.isValid())
        {
            const double ar = img.getWidth() > 0 ? (double)img.getWidth() / (double)img.getHeight() : 4.0;
            logoH = juce::roundToInt(logoW / ar);
        }
        logoImg.setBounds(logoCol.withSizeKeepingCentre(logoW, logoH).translated(0, -20)); // Nudge down a bit
    }
   

    // ---- Row 3: Harmony Buttons (Reharmonize, Advanced, Adjust) ----
    auto harmonyRow = bounds.removeFromTop(62);
    bounds.removeFromTop(rowGap - 4); // Add some space below the harmony buttons
    {
        const int smallBtnW = 140;
        const int smallBtnH = 44;
        const int advBtnW = 210;
        const int advBtnH = 62; // Taller as in the mockup

        auto harmonyButtonArea = harmonyRow.withSizeKeepingCentre(harmonyAreaWidth, advBtnH);

        // Center the smaller buttons vertically in the harmonyRow
        const int smallBtnY = harmonyButtonArea.getCentreY() - smallBtnH / 2;

        reharmBtn.setBounds(harmonyButtonArea.getX(), smallBtnY, smallBtnW, smallBtnH);
        adjustBtn.setBounds(harmonyButtonArea.getRight() - smallBtnW, smallBtnY, smallBtnW, smallBtnH);
        advancedBtn.setBounds(harmonyButtonArea.getCentreX() - advBtnW / 2, harmonyButtonArea.getY(), advBtnW, advBtnH);
    }

    // ---- Row 5 (Bottom): Generate and Drag buttons ----
    auto bottomArea = bounds.removeFromBottom(140);
    {
        const int buttonHeight = 130;
        const int bigGap = 50;

        // Calculate widths based on aspect ratio and a fixed height
        float genNatW = generateBtnNaturalBounds.getWidth();
        float genNatH = generateBtnNaturalBounds.getHeight();
        int genW = (genNatH > 0) ? (int)(genNatW * buttonHeight / genNatH) : 200; // Default width if height is 0

        float dragNatW = dragBtnNaturalBounds.getWidth();
        float dragNatH = dragBtnNaturalBounds.getHeight();
        int dragW = (dragNatH > 0) ? (int)(dragNatW * buttonHeight / dragNatH) : 200; // Default width if height is 0

        int totalWidth = genW + dragW + bigGap;

        // Center the whole button row
        juce::Rectangle<int> buttonRow (0, 0, totalWidth, buttonHeight);
        buttonRow = buttonRow.withCentre(bottomArea.getCentre());

        generateBtn.setBounds(buttonRow.removeFromLeft(genW));
        buttonRow.removeFromLeft(bigGap);
        dragBtn.setBounds(buttonRow.removeFromLeft(dragW));

        auto skinToggle = [this](juce::ToggleButton& b)
        {
            b.setLookAndFeel(&bangToggleLAF);
            b.setButtonText({});
            b.setClickingTogglesState(true);
        };

        // Call skinToggle(...) for whichever of these are members of AdvancedHarmonyWindow:
        skinToggle(poly23Btn);
        skinToggle(poly32Btn);
        skinToggle(poly34Btn);
        skinToggle(poly43Btn);
        skinToggle(keepBarsToggle);
        skinToggle(keepAdvancedToggle);
        skinToggle(keepPolyrToggle);
        skinToggle(keepReharmToggle);

        poly23Btn.setLookAndFeel(nullptr);
        poly32Btn.setLookAndFeel(nullptr);
        poly34Btn.setLookAndFeel(nullptr);
        poly43Btn.setLookAndFeel(nullptr);

        keepBarsToggle.setLookAndFeel(nullptr);
        keepAdvancedToggle.setLookAndFeel(nullptr);
        keepPolyrToggle.setLookAndFeel(nullptr);
        keepReharmToggle.setLookAndFeel(nullptr);

    }
    
    bounds.removeFromTop(rowGap);
    bounds.removeFromBottom(rowGap);

    // ---- Row 4: Piano roll (explicit area calculation; never starve the roll) ----
    {
        // These constants must match the ones used earlier in this function
        const int pad = 16;   // already defined above in your code
        const int rowGap = 5;  // already defined above in your code

        // Start from the full content area (don’t “consume” it further)
        auto full = getLocalBounds().reduced(pad);

        // Sum the fixed-height rows we placed ABOVE the roll:
        // 60 (topRow) + 60 (engineRow) + rowGap
        // + 290 (mainControlsRow) + rowGap
        // + 62 (harmonyRow) + rowGap
        const int topUsed =
            60   // topRow
            + 60   // engineRow
            + rowGap
            + 290  // mainControlsRow
            + rowGap
            + 62   // harmonyRow
            + rowGap;

        // Sum the fixed-height rows we placed BELOW the roll:
        // 140 (bottomArea) + rowGap
        const int bottomUsed =
            110   // bottom buttons row
            + rowGap;

        // Whatever remains is the roll area
        auto rollArea = full.withTrimmedTop(topUsed)
            .withTrimmedBottom(bottomUsed);
        
        rollArea = rollArea.withSizeKeepingCentre(harmonyAreaWidth, rollArea.getHeight());

        // If for any reason the math left too little space, clamp to a visible minimum
        if (rollArea.getHeight() < 180)
            rollArea.setHeight(180);

        rollView.setBounds(rollArea);
        updateRollContentSize(); // this sizes the pianoRoll content to fill horizontally and be tall enough
    }

    // ---- Ensure all buttons sit above the roll ----
    
    // ---- Tooltips ----
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
    pianoRoll.setGrid(currentBars(), currentTSNumerator());
    pushSettingsToGenerator();
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
    const int pxPerBeat = 48;

    // Width based on musical length
    const int contentW = juce::jlimit(800, 20000, bars * beats * pxPerBeat);

    // Preferred height from the roll; fall back to a computed viewport height
    int prefH = pianoRoll.getPreferredHeight();
    int vpH = rollView.getHeight();
    if (vpH <= 0)
        // This fallback calculation must mirror the layout logic in resized()
        vpH = getHeight()
            - 60   // topRow
            - 60   // engineRow
            - 18   // rowGap
            - 290  // mainControlsRow
            - 18   // rowGap
            - 62   // harmonyRow
            - 18   // rowGap
            - 140  // bottomArea
            - 18   // rowGap
            - 32;  // margins (pad * 2)

    const int contentH = juce::jmax(prefH, vpH);

    pianoRoll.setTopLeftPosition(0, 0);   // keep content origin sane
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
    {
        // Keep this in EXACT sync with the Mixture preamble.
        gen.setEngineMode(MidiGenerator::EngineMode::Chords);
        gen.setHumanizeTiming(timingSl.getValue());
        gen.setHumanizeVelocity(velocitySl.getValue());
        gen.setSwingAmount(swingSl.getValue());
        gen.setFeelAmount(feelSl.getValue());

        gen.setTimeSignature(currentTSNumerator(), 4);
        gen.setBars(currentBars());
        gen.setKey(rootBoxToSemitone(keyBox) + 60); // force to C4-based root
        gen.setScaleIndex(scaleBox.getSelectedId());
        gen.setRestDensity(restSl.getValue() / 100.0);

        // Advanced flags refresh before generating chords
        audioProcessor.refreshAdvancedOptionsFromApvts();
        gen.setAdvancedHarmonyOptions(audioProcessor.getAdvancedOptionsPtr());

        auto notes = gen.generateChords();
        lastChords = std::move(notes);
        // show just chords on the roll
        std::vector<Note> display = lastChords;
        for (auto& n : display)
            n.pitch = juce::jlimit(0, 127, n.pitch + audioProcessor.getOctaveShiftSemitones());
        pianoRoll.setNotes(display);
    }
}
 break;

    case EngineSel::Melody:
    {
        // Push current engine + humanize into the generator
        {
            // Keep this in EXACT sync with the Mixture preamble.
            gen.setEngineMode(MidiGenerator::EngineMode::Melody);
            gen.setHumanizeTiming(timingSl.getValue());
            gen.setHumanizeVelocity(velocitySl.getValue());
            gen.setSwingAmount(swingSl.getValue());
            gen.setFeelAmount(feelSl.getValue());

            gen.setTimeSignature(currentTSNumerator(), 4);
            gen.setBars(currentBars());
            gen.setKey(rootBoxToSemitone(keyBox) + 60); // force to C4-based root
            gen.setScaleIndex(scaleBox.getSelectedId());
            gen.setRestDensity(restSl.getValue() / 100.0);

            // Advanced flags refresh (even if you don't plan to use them here, keeps behavior consistent)
            audioProcessor.refreshAdvancedOptionsFromApvts();
            gen.setAdvancedHarmonyOptions(audioProcessor.getAdvancedOptionsPtr());

            auto notes = gen.generateMelody();
            lastMelody = std::move(notes);
            // show just melody on the roll (respect octave shift if you do this elsewhere)
            std::vector<Note> display = lastMelody;
            for (auto& n : display)
                n.pitch = juce::jlimit(0, 127, n.pitch + audioProcessor.getOctaveShiftSemitones());
            pianoRoll.setNotes(display);
        }
    }
    break;

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
        for (auto& n : combined) n.pitch = juce::jlimit(0, 127, n.pitch + audioProcessor.getOctaveShiftSemitones());
        pianoRoll.setNotes(combined);
    } break;
    }

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

    const int semis = audioProcessor.getOctaveShiftSemitones();

    const int ppq = 960;
    juce::MidiMessageSequence seq;

    // Helper to push notes into the sequence (with a light velocity scale)
    auto push = [&](const std::vector<Note>& src, int midiChannel, int velScalePercent)
    {
        for (const auto& n : src)
        {
            const int pitchRaw = juce::jlimit(0, 127, n.pitch);
            const int pitchShifted = juce::jlimit(0, 127, pitchRaw + semis);

            const int scaled = (n.velocity * velScalePercent) / 100;
            const int vel = juce::jlimit(1, 127, scaled);

            const double tOn = n.startBeats * (double)ppq;
            const double tOff = (n.startBeats + n.lengthBeats) * (double)ppq;

            seq.addEvent(juce::MidiMessage::noteOn(midiChannel, pitchShifted, (juce::uint8)vel), tOn);
            seq.addEvent(juce::MidiMessage::noteOff(midiChannel, pitchShifted), tOff);
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
    // Only arm a potential external drag if the user pressed the drag button (or its image)
    auto* src = e.originalComponent;
    preparingExternalDrag = (src == &dragBtn);
    if (preparingExternalDrag)
        dragStartPos = e.getEventRelativeTo(this).position.toInt();
}

void BANGAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (!preparingExternalDrag)
        return;

    const auto inEditor = e.getEventRelativeTo(this).position.toInt();
    const int dx = std::abs(inEditor.x - dragStartPos.x);
    const int dy = std::abs(inEditor.y - dragStartPos.y);
    const bool surpassedThreshold = (dx >= 5 || dy >= 5);

    if (surpassedThreshold)
    {
        preparingExternalDrag = false; // only once

        // Build the temp MIDI file and start the OS-level file drag
        auto f = writeTempMidiForDrag(); // you already have this helper
        if (f.existsAsFile())
        {
            dragBtn.setState(juce::Button::buttonNormal); // cancel the pressed visual
            performExternalDragDropOfFiles({ f.getFullPathName() }, /*canMoveFiles*/ true);
        }
    }
}

// ======== Popups ========

void BANGAudioProcessorEditor::openAdvanced()
{
    auto* comp = new AdvancedHarmonyWindow(audioProcessor);
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Advanced Harmony";
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.content.setOwned(comp);
    if (auto* w = opts.launchAsync())
        w->centreWithSize(820, 680);
}

void BANGAudioProcessorEditor::openPolyrhythm()
{


    // Lightweight dialog that writes straight into the generator's polyrhythm amount
    struct PolyComp : public juce::Component
    {
        BANGAudioProcessorEditor& editor;
        juce::Slider densitySlider;

        juce::ToggleButton poly23Btn, poly32Btn, poly54Btn, poly74Btn;

        juce::ImageButton diceBtn;
        juce::ImageButton enablePassBtn;

        juce::ImageComponent polyLbl;
        juce::ImageComponent typeLbl;
        juce::ImageComponent twentyThreeLbl;
        juce::ImageComponent thirtyTwoLbl;
        juce::ImageComponent fiftyFourLbl;
        juce::ImageComponent seventyFourLbl;
        juce::ImageComponent densityLbl;

    private:
        // Choose mode based on UI
        MidiGenerator::PolyrhythmMode selectedMode() const
        {
            if (poly23Btn.getToggleState()) return MidiGenerator::PolyrhythmMode::Ratio2_3;
            if (poly32Btn.getToggleState()) return MidiGenerator::PolyrhythmMode::Ratio3_2;
            if (poly54Btn.getToggleState()) return MidiGenerator::PolyrhythmMode::Ratio5_4;
            if (poly74Btn.getToggleState()) return MidiGenerator::PolyrhythmMode::Ratio7_4;
            return MidiGenerator::PolyrhythmMode::None;
        }

        void applyModeToGenerator()
        {
            auto& gen = editor.audioProcessor.getMidiGenerator();
            gen.setPolyrhythmMode(selectedMode());
        }

        void applyAndRefresh()
        {
            applyModeToGenerator();
            const float amt01 = (float)(densitySlider.getValue() * 0.01);
            editor.audioProcessor.getMidiGenerator().setPolyrhythmAmount(amt01);

            // Push normal settings and regenerate so the user sees/hears it immediately
            editor.pushSettingsToGenerator();
            editor.regenerate();
        }
    public:
        PolyComp(BANGAudioProcessorEditor& ed) : editor(ed)
        {
            poly23Btn.setClickingTogglesState(true);
            poly32Btn.setClickingTogglesState(true);
            poly54Btn.setClickingTogglesState(true);
            poly74Btn.setClickingTogglesState(true);

            poly23Btn.setLookAndFeel(&editor.bangToggleLAF);
            poly32Btn.setLookAndFeel(&editor.bangToggleLAF);
            poly54Btn.setLookAndFeel(&editor.bangToggleLAF);
            poly74Btn.setLookAndFeel(&editor.bangToggleLAF);

            // === DENSITY: maps 0..100 to generator polyrhythm amount 0..1 ===
            addAndMakeVisible(densitySlider);
            densitySlider.setRange(0.0, 100.0, 1.0);

            // Initialize from a reasonable default (use current feel/swing as a hint if you want)
            densitySlider.setValue(juce::jlimit(0.0, 100.0, editor.swingSl.getValue()));

            // IMPORTANT: drive the generator directly (no more re-routing to swing)
            densitySlider.onValueChange = [this]
            {
                auto& gen = editor.audioProcessor.getMidiGenerator();
                const float amt01 = (float)(densitySlider.getValue() * 0.01);
                gen.setPolyrhythmAmount(amt01);

                // Keep the main engine in sync and show the change immediately in the roll
                editor.pushSettingsToGenerator();
                editor.regenerate();
            };

            const auto accent = juce::Colour::fromRGB(0xDD, 0x4F, 0x02);
            densitySlider.setColour(juce::Slider::trackColourId, accent);
            densitySlider.setColour(juce::Slider::thumbColourId, accent);
            densitySlider.setColour(juce::Slider::textBoxTextColourId, accent);
            densitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);



            addAndMakeVisible(poly23Btn);
            addAndMakeVisible(poly32Btn);
            addAndMakeVisible(poly54Btn);
            addAndMakeVisible(poly74Btn);
            
            poly23Btn.setRadioGroupId(2002);
            poly32Btn.setRadioGroupId(2002);
            poly54Btn.setRadioGroupId(2002);
            poly74Btn.setRadioGroupId(2002);
            
            poly23Btn.setToggleState(true, juce::dontSendNotification);

            // === Toggle callbacks: set the generator mode immediately ===
            auto wireToggle = [this](juce::ToggleButton& tb)
            {
                tb.onClick = [this]
                {
                    // Ensure radio behavior (one at a time is already guaranteed by groupId)
                    applyAndRefresh();
                };
            };
            wireToggle(poly23Btn);
            wireToggle(poly32Btn);
            wireToggle(poly54Btn);
            wireToggle(poly74Btn);


            polyLbl.setImage(loadImageByHint("polyrhythmLbl"));
            typeLbl.setImage(loadImageByHint("typeLbl"));
            twentyThreeLbl.setImage(loadImageByHint("23Lbl"));
            thirtyTwoLbl.setImage(loadImageByHint("32Lbl"));
            fiftyFourLbl.setImage(loadImageByHint("54Lbl"));
            seventyFourLbl.setImage(loadImageByHint("74Lbl"));
            densityLbl.setImage(loadImageByHint("densityLbl"));

            setImageButton3(diceBtn, "diceBtn");

            diceBtn.onClick = [this]
            {
                juce::Random r{ juce::Time::getMillisecondCounter() };

                // Pick one of the 4
                const int pick = r.nextInt({ 0,3 });
                poly23Btn.setToggleState(pick == 0, juce::dontSendNotification);
                poly32Btn.setToggleState(pick == 1, juce::dontSendNotification);
                poly54Btn.setToggleState(pick == 2, juce::dontSendNotification);
                poly74Btn.setToggleState(pick == 3, juce::dontSendNotification);

                // Random density 10..90%
                const double dens = 10.0 + r.nextDouble() * 80.0;
                densitySlider.setValue(dens, juce::sendNotificationSync);

                // Apply immediately
                applyAndRefresh();
            };


            setImageButton3(enablePassBtn, "enablePassBtn");

            enablePassBtn.onClick = [this]
            {
                applyAndRefresh();

                if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                    dw->exitModalState(0); // closes the window
            };

            addAndMakeVisible(polyLbl);
            addAndMakeVisible(typeLbl);
            addAndMakeVisible(twentyThreeLbl);
            addAndMakeVisible(thirtyTwoLbl);
            addAndMakeVisible(fiftyFourLbl);
            addAndMakeVisible(seventyFourLbl);
            addAndMakeVisible(densityLbl);
            addAndMakeVisible(diceBtn);
            addAndMakeVisible(enablePassBtn);

            polyLbl.setImagePlacement(juce::RectanglePlacement::centred);
            typeLbl.setImagePlacement(juce::RectanglePlacement::centred);
            twentyThreeLbl.setImagePlacement(juce::RectanglePlacement::centred);
            thirtyTwoLbl.setImagePlacement(juce::RectanglePlacement::centred);
            fiftyFourLbl.setImagePlacement(juce::RectanglePlacement::centred);
            seventyFourLbl.setImagePlacement(juce::RectanglePlacement::centred);
            densityLbl.setImagePlacement(juce::RectanglePlacement::centred);
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            const int pad = 20;

            auto topBar = bounds.removeFromTop(80);
            topBar.reduce(pad, 0);
            
            diceBtn.setBounds(topBar.removeFromRight(50).withSizeKeepingCentre(44, 44));
            
            polyLbl.setBounds(topBar);
            polyLbl.setImagePlacement(juce::RectanglePlacement::centred);
            // Nudge the label to the right for better visual alignment
            polyLbl.setBounds(polyLbl.getBounds().translated(25, 0));

            bounds.reduce(pad, pad);

            auto typeArea = bounds.removeFromTop(60);
            typeLbl.setBounds(typeArea.withSizeKeepingCentre(150, 50));
            typeLbl.setImagePlacement(juce::RectanglePlacement::centred);

            auto togglesArea = bounds.removeFromTop(60);
            
            const int numToggles = 4;
            const int labelWidth = 60;
            
            juce::Image toggleImage = loadImageByHint("toggleBtnOn");
            double toggleAspectRatio = toggleImage.isValid() ? (double)toggleImage.getWidth() / (double)toggleImage.getHeight() : 2.816; // 352x125
            const int toggleHeight = 45;
            const int toggleWidth = (int)(toggleHeight * toggleAspectRatio);

            const int itemWidth = labelWidth + toggleWidth;
            const int totalTogglesWidth = itemWidth * numToggles;
            
            togglesArea.reduce((togglesArea.getWidth() - totalTogglesWidth) / 2, 0);

            auto createToggleRow = [&](juce::ImageComponent& label, juce::ToggleButton& button)
            {
                auto itemBounds = togglesArea.removeFromLeft(itemWidth);
                label.setBounds(itemBounds.removeFromLeft(labelWidth));
                button.setBounds(itemBounds.removeFromLeft(toggleWidth).withSizeKeepingCentre(toggleWidth, toggleHeight));
                label.setImagePlacement(juce::RectanglePlacement::centred);
            };

            createToggleRow(twentyThreeLbl, poly23Btn);
            createToggleRow(thirtyTwoLbl, poly32Btn);
            createToggleRow(fiftyFourLbl, poly54Btn);
            createToggleRow(seventyFourLbl, poly74Btn);
            
            bounds.removeFromTop(pad);

            auto densityArea = bounds.removeFromTop(60);
            const int densityLabelWidth = 120;
            const int sliderWidth = 300;
            const int totalDensityWidth = densityLabelWidth + sliderWidth;

            densityArea.reduce((densityArea.getWidth() - totalDensityWidth) / 2, 0);

            densityLbl.setBounds(densityArea.removeFromLeft(densityLabelWidth));
            densityLbl.setImagePlacement(juce::RectanglePlacement::xRight | juce::RectanglePlacement::yMid);

            densitySlider.setBounds(densityArea.removeFromLeft(sliderWidth).reduced(0, 10));

            bounds.removeFromTop(pad * 2);
            auto enableArea = bounds.removeFromTop(150); // Increased height for the area
            
            juce::Image enableImage = loadImageByHint("enablePassBtn");
            double enableAspectRatio = enableImage.isValid() ? (double)enableImage.getWidth() / (double)enableImage.getHeight() : 4.0;
            const int enableHeight = 140; // Increased button height
            const int enableWidth = (int)(enableHeight * enableAspectRatio);

            enablePassBtn.setBounds(enableArea.withSizeKeepingCentre(enableWidth, enableHeight));
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
        w->centreWithSize(720, 560);
    

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
            editor.bangToggleLAF.sizeToggleToArt(keepBarsToggle, 44);


            // Configure buttons
            addAndMakeVisible(regenerateBtn);
            setImageButton3(regenerateBtn, "regenerateBtn");

            addAndMakeVisible(diceBtn);
            setImageButton3(diceBtn, "diceBtn");

            // === Dice button: randomize ONLY the Reharmonize window’s key/scale ===
            diceBtn.onClick = [this]
            {
                auto& r = juce::Random::getSystemRandom();
                if (keyBox.getNumItems() > 0)
                    keyBox.setSelectedItemIndex(r.nextInt(keyBox.getNumItems()));  // 0..N-1
                if (scaleBox.getNumItems() > 0)
                    scaleBox.setSelectedItemIndex(r.nextInt(scaleBox.getNumItems()));
            };

            // === Regenerate: reharmonize using window selections, optionally keeping first 4 bars ===
            regenerateBtn.onClick = [this]
            {
                auto& gen = editor.audioProcessor.getMidiGenerator();

                // 1) Push normal settings from the main UI first (time sig, bars, rest, humanize, etc.)
                editor.pushSettingsToGenerator(); // keeps all your usual settings in sync

                // 2) Override key/scale with the Reharmonize window choices (do NOT touch the main UI)
                const int newRootSemitone = rootBoxToSemitone(keyBox) + 60;     // helper already in this TU
                const int newScaleIndex = scaleBox.getSelectedId();      // your generator supports index
                gen.setKey(newRootSemitone);
                gen.setScaleIndex(newScaleIndex);

                // Keep the generator’s engine in sync with the main editor selection
                using EM = MidiGenerator::EngineMode;
                switch (editor.engineSel)
                {
                case BANGAudioProcessorEditor::EngineSel::Chords:  gen.setEngineMode(EM::Chords);  break;
                case BANGAudioProcessorEditor::EngineSel::Melody:  gen.setEngineMode(EM::Melody);  break;
                case BANGAudioProcessorEditor::EngineSel::Mixture: gen.setEngineMode(EM::Mixture); break;
                }

                // Advanced harmony options (match your normal regenerate path)
                editor.audioProcessor.refreshAdvancedOptionsFromApvts();
                gen.setAdvancedHarmonyOptions(editor.audioProcessor.getAdvancedOptionsPtr());

                // 3) Generate fresh material in the NEW key/scale
                std::vector<Note> freshMel, freshChd;
                if (editor.engineSel == BANGAudioProcessorEditor::EngineSel::Chords)
                {
                    freshChd = gen.generateChords();
                }
                else if (editor.engineSel == BANGAudioProcessorEditor::EngineSel::Melody)
                {
                    freshMel = gen.generateMelody();
                }
                else // Mixture
                {
                    auto mix = gen.generateMelodyAndChords();
                    freshMel = std::move(mix.melody);
                    freshChd = std::move(mix.chords);
                }

                // 4) If requested, keep the FIRST HALF of the current loop (bars/2)
                const bool   keepFirst4 = keepBarsToggle.getToggleState();
                const double beatsPerBar = static_cast<double>(editor.currentTSNumerator());
                const double cutoffBeats = (beatsPerBar * static_cast<double>(editor.currentBars())) * 0.5;

                auto spliceKeepFirst4 = [&](const std::vector<Note>& oldV, const std::vector<Note>& newV)
                {
                    if (!keepFirst4 || oldV.empty()) return newV;
                    std::vector<Note> out; out.reserve(oldV.size() + newV.size());
                    for (const auto& n : oldV) if (n.startBeats < cutoffBeats) out.push_back(n);
                    for (const auto& n : newV) if (n.startBeats >= cutoffBeats) out.push_back(n);
                    return out;
                };

                // 5) Commit to editor cache and update the piano roll (respect octave shift)
                const int semis = editor.audioProcessor.getOctaveShiftSemitones();

                if (editor.engineSel == BANGAudioProcessorEditor::EngineSel::Chords)
                {
                    editor.lastChords = spliceKeepFirst4(editor.lastChords, freshChd);
                    auto view = editor.lastChords;
                    for (auto& n : view) n.pitch = juce::jlimit(0, 127, n.pitch + semis);
                    editor.pianoRoll.setNotes(view);
                }
                else if (editor.engineSel == BANGAudioProcessorEditor::EngineSel::Melody)
                {
                    editor.lastMelody = spliceKeepFirst4(editor.lastMelody, freshMel);
                    auto view = editor.lastMelody;
                    for (auto& n : view) n.pitch = juce::jlimit(0, 127, n.pitch + semis);
                    editor.pianoRoll.setNotes(view);
                }
                else // Mixture
                {
                    editor.lastMelody = spliceKeepFirst4(editor.lastMelody, freshMel);
                    editor.lastChords = spliceKeepFirst4(editor.lastChords, freshChd);

                    std::vector<Note> combined;
                    combined.reserve(editor.lastMelody.size() + editor.lastChords.size());
                    combined.insert(combined.end(), editor.lastMelody.begin(), editor.lastMelody.end());
                    combined.insert(combined.end(), editor.lastChords.begin(), editor.lastChords.end());
                    for (auto& n : combined) n.pitch = juce::jlimit(0, 127, n.pitch + semis);
                    editor.pianoRoll.setNotes(combined);
                }

                editor.pianoRoll.repaint();
            };

        }

       
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour::fromRGB(0xA5, 0xDD, 0x00));
        }

        void resized() override
        {
            auto bounds = getLocalBounds();
            
            // Top-right dice button
            diceBtn.setBounds(getWidth() - 50, 10, 40, 40);

            auto topArea = bounds.removeFromTop(100);
            titleLbl.setBounds(topArea.withSizeKeepingCentre(300, 80));

            bounds = bounds.reduced(30);

            auto createRow = [&](juce::ImageComponent& label, juce::Component& control)
            {
                auto row = bounds.removeFromTop(50);
                label.setBounds(row.removeFromLeft(180).reduced(5));
                control.setBounds(row.reduced(5));
                bounds.removeFromTop(20);
            };

            createRow(newKeyLbl, keyBox);
            createRow(newScaleLbl, scaleBox);

            auto keepRow = bounds.removeFromTop(50);
            keepFirst4BarsLbl.setBounds(keepRow.removeFromLeft(180).reduced(5));
            keepBarsToggle.setBounds(keepRow.withSizeKeepingCentre(keepBarsToggle.getWidth(), keepBarsToggle.getHeight()));
            
            bounds.removeFromTop(40);

            // Centered Regenerate button at the bottom
            const int btnHeight = 150;
            auto regenImg = regenerateBtn.getNormalImage();
            int btnWidth = btnHeight;
            if (regenImg.isValid() && regenImg.getHeight() > 0)
                btnWidth = regenImg.getWidth() * btnHeight / regenImg.getHeight();

            regenerateBtn.setBounds(bounds.getCentreX() - btnWidth / 2,
                                    bounds.getBottom() - btnHeight - 20,
                                    btnWidth,
                                    btnHeight);
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

// ======== Adjust popup ========
void BANGAudioProcessorEditor::openAdjust()
{
    struct AdjustWindow : public juce::Component
    {
        BANGAudioProcessorEditor& editor;

        // ——— components (your exact names) ———
        juce::ImageComponent adjustLblImg;
        juce::ImageButton    diceBtn;

        juce::ImageComponent keyLblImg, scaleLblImg, timeSigLblImg, barsLblImg, restDensityLblImg, octaveLblImg;
        juce::ComboBox       keyBox, scaleBox, tsBox, barsBox, octaveBox;
        juce::Slider         restSl;

        juce::ImageComponent keepAdvancedLbl, keepPolyrLbl, keepReharmLbl;
        juce::ToggleButton   keepAdvancedToggle, keepPolyrToggle, keepReharmToggle;

        juce::ImageButton    regenerateBtn;

        AdjustWindow(BANGAudioProcessorEditor& ed) : editor(ed)
        {
            // ----- images for labels -----
            adjustLblImg.setImage(loadImageByHint("adjustLbl"));        adjustLblImg.setImagePlacement(juce::RectanglePlacement::centred);
            keyLblImg.setImage(loadImageByHint("keyLbl"));               keyLblImg.setImagePlacement(juce::RectanglePlacement::xRight);
            scaleLblImg.setImage(loadImageByHint("scaleLbl"));           scaleLblImg.setImagePlacement(juce::RectanglePlacement::xRight);
            timeSigLblImg.setImage(loadImageByHint("timesigLbl"));       timeSigLblImg.setImagePlacement(juce::RectanglePlacement::xRight);
            barsLblImg.setImage(loadImageByHint("barsLbl"));             barsLblImg.setImagePlacement(juce::RectanglePlacement::xRight);
            restDensityLblImg.setImage(loadImageByHint("restDensityLbl")); restDensityLblImg.setImagePlacement(juce::RectanglePlacement::xRight);
            octaveLblImg.setImage(loadImageByHint("octaveLbl"));         octaveLblImg.setImagePlacement(juce::RectanglePlacement::xRight);

            keepAdvancedLbl.setImage(loadImageByHint("keepAdvancedLbl")); keepAdvancedLbl.setImagePlacement(juce::RectanglePlacement::xRight);
            keepPolyrLbl.setImage(loadImageByHint("keepPolyrLbl"));       keepPolyrLbl.setImagePlacement(juce::RectanglePlacement::xRight);
            keepReharmLbl.setImage(loadImageByHint("keepReharmLbl"));     keepReharmLbl.setImagePlacement(juce::RectanglePlacement::xRight);

            // ----- buttons (image states) -----
            setImageButton3(diceBtn, "diceBtn");
            setImageButton3(regenerateBtn, "regenerateBtn");

            // ----- add to UI -----
            std::initializer_list<juce::Component*> comps = {
                &adjustLblImg, &diceBtn,
                &keyLblImg, &scaleLblImg, &timeSigLblImg, &barsLblImg, &restDensityLblImg, &octaveLblImg,
                &keyBox, &scaleBox, &tsBox, &barsBox, &octaveBox, &restSl,
                &keepAdvancedLbl, &keepPolyrLbl, &keepReharmLbl,
                &keepAdvancedToggle, &keepPolyrToggle, &keepReharmToggle,
                &regenerateBtn
            };
            for (auto* c : comps)
                addAndMakeVisible(c);

            // ----- style pickers for combo boxes & slider -----
            auto stylePicker = [](juce::ComboBox& cb)
            {
                const auto pickerBg = juce::Colour::fromRGB(0xF9, 0xBF, 0x00);
                const auto outline = juce::Colours::black;
                cb.setColour(juce::ComboBox::backgroundColourId, pickerBg);
                cb.setColour(juce::ComboBox::textColourId, outline);
                cb.setColour(juce::ComboBox::outlineColourId, outline);
                cb.setColour(juce::ComboBox::arrowColourId, outline);
            };
            auto styleSlider = [](juce::Slider& s)
            {
                const auto accent = juce::Colour::fromRGB(0xDD, 0x4F, 0x02);
                s.setRange(0.0, 100.0, 1.0);
                s.setColour(juce::Slider::trackColourId, accent);
                s.setColour(juce::Slider::thumbColourId, accent);
                s.setColour(juce::Slider::textBoxTextColourId, accent);
                s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 22);
            };

            stylePicker(keyBox);
            stylePicker(scaleBox);
            stylePicker(tsBox);
            stylePicker(barsBox);
            stylePicker(octaveBox);
            styleSlider(restSl);

            // ----- populate combo boxes to match main UI -----
            // Keys
            for (int i = 0; i < editor.keyBox.getNumItems(); ++i)
                keyBox.addItem(editor.keyBox.getItemText(i), editor.keyBox.getItemId(i));

            // Scales
            for (int i = 0; i < editor.scaleBox.getNumItems(); ++i)
                scaleBox.addItem(editor.scaleBox.getItemText(i), editor.scaleBox.getItemId(i));

            // Time Signatures
            for (int i = 0; i < editor.tsBox.getNumItems(); ++i)
                tsBox.addItem(editor.tsBox.getItemText(i), editor.tsBox.getItemId(i));

            // Bars (4/8)
            for (int i = 0; i < editor.barsBox.getNumItems(); ++i)
                barsBox.addItem(editor.barsBox.getItemText(i), editor.barsBox.getItemId(i));

            // Octave
            for (int i = 0; i < editor.octaveBox.getNumItems(); ++i)
                octaveBox.addItem(editor.octaveBox.getItemText(i), editor.octaveBox.getItemId(i));

            // ----- default “keep” toggles ON -----
            keepAdvancedToggle.setClickingTogglesState(true);
            keepPolyrToggle.setClickingTogglesState(true);
            keepReharmToggle.setClickingTogglesState(true);
            keepAdvancedToggle.setToggleState(true, juce::dontSendNotification);
            keepPolyrToggle.setToggleState(true, juce::dontSendNotification);
            keepReharmToggle.setToggleState(true, juce::dontSendNotification);

            // ===== sync helpers =====
            auto syncFromEditor = [&]()
            {
                keyBox.setSelectedId(editor.keyBox.getSelectedId(), juce::dontSendNotification);
                scaleBox.setSelectedId(editor.scaleBox.getSelectedId(), juce::dontSendNotification);
                tsBox.setSelectedId(editor.tsBox.getSelectedId(), juce::dontSendNotification);
                barsBox.setSelectedId(editor.barsBox.getSelectedId(), juce::dontSendNotification);
                octaveBox.setSelectedId(editor.octaveBox.getSelectedId(), juce::dontSendNotification);
                restSl.setValue(editor.restSl.getValue(), juce::dontSendNotification);
            };

            auto applyToEditor = [&]()
            {
                editor.keyBox.setSelectedId(keyBox.getSelectedId(), juce::sendNotification);
                editor.scaleBox.setSelectedId(scaleBox.getSelectedId(), juce::sendNotification);
                editor.tsBox.setSelectedId(tsBox.getSelectedId(), juce::sendNotification);
                editor.barsBox.setSelectedId(barsBox.getSelectedId(), juce::sendNotification);
                editor.octaveBox.setSelectedId(octaveBox.getSelectedId(), juce::sendNotification);
                editor.restSl.setValue(restSl.getValue(), juce::sendNotification);
            };

            // ===== discard helpers (when toggles are OFF) =====
            auto discardAdvancedHarmony = [&]()
            {
                if (auto* adv = editor.audioProcessor.getAdvancedOptionsPtr())
                {
                    *adv = AdvancedHarmonyOptions{}; // reset to defaults
                    editor.audioProcessor.getMidiGenerator().setAdvancedHarmonyOptions(adv);
                }
            };
            auto discardPolyrhythm = [&]()
            {
                auto& gen = editor.audioProcessor.getMidiGenerator();
                gen.setPolyrhythmMode(MidiGenerator::PolyrhythmMode::None);
                gen.setPolyrhythmAmount(0.0f);
            };
            auto discardReharm = [&]()
            {
                // No persistent reharm state to reset — new generation uses current key/scale
            };

            // ===== dice & regenerate =====
            diceBtn.onClick = [&, syncFromEditor]()
            {
                editor.randomizeAll();   // uses main UI randomizer (keeps everything in sync)
                editor.regenerate();     // show the new result
                syncFromEditor();        // mirror the new main UI state into this window
            };

            regenerateBtn.onClick = [&, applyToEditor, discardAdvancedHarmony, discardPolyrhythm, discardReharm]()
            {
                // Push Adjust choices back to main UI controls
                applyToEditor();

                // Apply discard rules
                if (!keepAdvancedToggle.getToggleState()) discardAdvancedHarmony();
                if (!keepPolyrToggle.getToggleState())    discardPolyrhythm();
                if (!keepReharmToggle.getToggleState())   discardReharm();

                // Regenerate with the resulting state
                editor.pushSettingsToGenerator();
                editor.regenerate();
            };

            // Initial mirror
            syncFromEditor();
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced(16);

            // ---- Top bar: title + dice ----
            const int topH = juce::jlimit(48, 96, r.getHeight() / 10);
            auto top = r.removeFromTop(topH);
            const int diceSize = juce::jlimit(36, 56, topH - 16);
            diceBtn.setBounds(top.removeFromRight(diceSize).withSizeKeepingCentre(diceSize, diceSize));
            adjustLblImg.setBounds(top);
            r.removeFromTop(8);

            // ---- Bottom: Regenerate button ----
            const int btnH = juce::jlimit(48, 80, getHeight() / 10);
            const int btnW = juce::jlimit(180, 320, btnH * 3);
            auto bottomArea = r.removeFromBottom(btnH + 8);
            regenerateBtn.setBounds(bottomArea.withSizeKeepingCentre(btnW, btnH));
            r.removeFromBottom(16);

            // ---- Main content area (single column) ----
            auto content = r.withWidth(r.getWidth() * 0.8f).withX(r.getCentreX() - (r.getWidth() * 0.8f) / 2);

            // ---- Combo boxes ----
            {
                const int labelW = 140;
                const int rowH = 30;
                const int gap = 8;
                auto createRow = [&](juce::ImageComponent& lbl, juce::Component& ctrl)
                {
                    auto rowBounds = content.removeFromTop(rowH);
                    lbl.setBounds(rowBounds.removeFromLeft(labelW));
                    ctrl.setBounds(rowBounds.reduced(4));
                    content.removeFromTop(gap);
                };
                createRow(keyLblImg, keyBox);
                createRow(scaleLblImg, scaleBox);
                createRow(timeSigLblImg, tsBox);
                createRow(barsLblImg, barsBox);
                createRow(restDensityLblImg, restSl);
                createRow(octaveLblImg, octaveBox);
            }

            content.removeFromTop(24); // More space before toggles

            // ---- Toggles with larger labels ----
            {
                auto toggleWidthForHeight = [](int h) -> int
                {
                    const int baseW = 352; const int baseH = 125;
                    return juce::roundToInt((baseW / (float)baseH) * (float)h);
                };

                const int rowH = 60; // Increased row height
                const int gap = 12;
                const int labelW = 200; // Increased label width
                const int toggleH = rowH - 4;
                const int toggleW = toggleWidthForHeight(toggleH);

                auto createToggleRow = [&](juce::ImageComponent& lbl, juce::ToggleButton& tb)
                {
                    auto rowBounds = content.removeFromTop(rowH);
                    lbl.setBounds(rowBounds.removeFromLeft(labelW));
                    tb.setBounds(rowBounds.withSizeKeepingCentre(toggleW, toggleH));
                    content.removeFromTop(gap);
                };

                createToggleRow(keepAdvancedLbl, keepAdvancedToggle);
                createToggleRow(keepPolyrLbl, keepPolyrToggle);
                createToggleRow(keepReharmLbl, keepReharmToggle);
            }
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour::fromRGB(0xA5, 0xDD, 0x00)); // same neon bg as main
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
        w->centreWithSize(820, 680);
}