#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "PianoRollComponent.h"
#include "AdvancedHarmonyWindow.h"

using namespace juce;

//============================================================
// Small utility: style helpers
void BANGAudioProcessorEditor::styleCombo(ComboBox& c, Colour bg, Colour txt)
{
    c.setColour(ComboBox::backgroundColourId, bg);
    c.setColour(ComboBox::outlineColourId, Colours::black);
    c.setColour(ComboBox::textColourId, txt);
}

void BANGAudioProcessorEditor::styleSlider(Slider& s, Colour track, Colour thumb, Colour txt)
{
    s.setTextBoxStyle(Slider::TextBoxBelow, false, 60, 18);
    s.setColour(Slider::trackColourId, track);
    s.setColour(Slider::thumbColourId, thumb);
    s.setColour(Slider::textBoxTextColourId, txt);
    s.setColour(Slider::backgroundColourId, Colours::black);
    s.setColour(Slider::textBoxOutlineColourId, Colours::black);
}

//============================================================
BANGAudioProcessorEditor::BANGAudioProcessorEditor(BANGAudioProcessor& proc)
    : AudioProcessorEditor(&proc)
    , audioProcessor(proc)
{
    // ---------- window / resizable ----------
    setOpaque(true);
    setResizeLimits(900, 560, 3000, 2000);
    setResizable(true, true);
    setSize(1180, 720);

    // ---------- hidden DnD helper ----------
    dndHelper = std::make_unique<DnDHelper>();
    addAndMakeVisible(*dndHelper);
    dndHelper->setBounds(0, 0, 1, 1); // invisible

    // ---------- logo ----------
    {
        const Image img = loadImageAny({
            "bang-newlogo.png", "BANG_logo.png", "banglogo.png", "logo.png"
            });
        if (img.isValid())
        {
            logoImg.setImage(logo);
            logoImg.setImagePlacement(juce::RectanglePlacement::centred);
            addAndMakeVisible(logoImg);
        }
    }
    // "ENGINE" title image under the logo
    {
        const Image img = loadImageAny({ "engine.png", "engineTitle.png" });
        if (img.isValid())
        {
            engineTitleImg.setImage(img);
            engineTitleImg.setImagePlacement(juce::RectanglePlacement::centred);
            addAndMakeVisible(engineTitleImg);
        }
    }

    // ---------- engine buttons (image) ----------
    auto setBtn3 = [this](ImageButton& btn,
        std::initializer_list<const char*> n,
        std::initializer_list<const char*> h,
        std::initializer_list<const char*> d)
    {
        const Image normal = loadImageAny(n);
        const Image over = loadImageAny(h);
        const Image down = loadImageAny(d);
        btn.setClickingTogglesState(true);
        btn.setTriggeredOnMouseDown(false);
        btn.setImages(
            /*resizeToFit*/ true,
            /*preserveProportions*/ true,
            /*opaque*/ true,
            normal, 1.0f, juce::Colours::transparentBlack,
            over, 1.0f, juce::Colours::transparentBlack,
            down, 1.0f, juce::Colours::transparentBlack
        );
        addAndMakeVisible(btn);
    };

    setBtn3(engineChordsBtn,
        { "chordsBtn.png",  "engine_chords.png" },
        { "chordsBtn_hover.png", "engine_chords_hover.png" },
        { "chordsBtn_down.png",  "engine_chords_down.png" });

    setBtn3(engineMixtureBtn,
        { "mixtureBtn.png", "engine_mixture.png" },
        { "mixtureBtn_hover.png", "engine_mixture_hover.png" },
        { "mixtureBtn_down.png", "engine_mixture_down.png" });

    setBtn3(engineMelodyBtn,
        { "melodyBtn.png", "engine_melody.png" },
        { "melodyBtn_hover.png", "engine_melody_hover.png" },
        { "melodyBtn_down.png", "engine_melody_down.png" });

    // exactly one selected at all times
    auto updateEngineSelection = [this](EngineMode m)
    {
        currentEngine = m;
        engineChordsBtn.setToggleState(m == EngineMode::Chords, dontSendNotification);
        engineMixtureBtn.setToggleState(m == EngineMode::Mixture, dontSendNotification);
        engineMelodyBtn.setToggleState(m == EngineMode::Melody, dontSendNotification);
        pushSettingsToGenerator();
    };

    engineChordsBtn.onClick = [updateEngineSelection, this] { updateEngineSelection(EngineMode::Chords);  };
    engineMixtureBtn.onClick = [updateEngineSelection, this] { updateEngineSelection(EngineMode::Mixture); };
    engineMelodyBtn.onClick = [updateEngineSelection, this] { updateEngineSelection(EngineMode::Melody);  };
    updateEngineSelection(EngineMode::Mixture); // default

    // ---------- action buttons ----------
    auto addBtn3 = [this, setBtn3](ImageButton& btn,
        std::initializer_list<const char*> n,
        std::initializer_list<const char*> h,
        std::initializer_list<const char*> d)
    {
        setBtn3(btn, n, h, d);
    };

    addBtn3(generateBtn, { "generateBtn.png" }, { "generateBtn_hover.png" }, { "generateBtn_down.png" });
    addBtn3(dragBtn, { "dragBtn.png" }, { "dragBtn_hover.png" }, { "dragBtn_down.png" });
    addBtn3(advancedBtn, { "advancedBtn.png" }, { "advancedBtn_hover.png" }, { "advancedBtn_down.png" });
    addBtn3(polyrBtn, { "polyrBtn.png" }, { "polyrBtn_hover.png" }, { "polyrBtn_down.png" });
    addBtn3(reharmBtn, { "reharmBtn.png" }, { "reharmBtn_hover.png" }, { "reharmBtn_down.png" });
    addBtn3(adjustBtn, { "adjustBtn.png" }, { "adjustBtn_hover.png" }, { "adjustBtn_down.png" });
    addBtn3(diceBtn, { "diceBtn.png" }, { "diceBtn_hover.png" }, { "diceBtn_down.png" });

    generateBtn.onClick = [this] { pushSettingsToGenerator(); regenerate(); };
    dragBtn.onClick = [this] { performDragExport(); };
    advancedBtn.onClick = [this] { openAdvanced(); };
    polyrBtn.onClick = [this] { openPolyrhythm(); };
    reharmBtn.onClick = [this] { openReharmonize(); };
    adjustBtn.onClick = [this] { openAdjust(); };
    diceBtn.onClick = [this] { randomizeAll(); regenerate(); };

    // ---------- selectors ----------
    keyLbl.setText("KEY", dontSendNotification);
    scaleLbl.setText("SCALE", dontSendNotification);
    tsLbl.setText("TIME SIG", dontSendNotification);
    barsLbl.setText("BARS", dontSendNotification);
    restLbl.setText("REST DENS", dontSendNotification);

    for (auto* L : { &keyLbl, &scaleLbl, &tsLbl, &barsLbl, &restLbl })
    {
        L->setJustificationType(Justification::centred);
        L->setColour(Label::textColourId, colText);
        L->setFont(Font(Font::FontOptions(18.0f, Font::bold)));
        addAndMakeVisible(*L);
    }

    // keys (plain ascii names to avoid codepage issues)
    const char* keys[] = { "C","C#","Db","D","D#","Eb","E","F","F#","Gb","G","G#","Ab","A","A#","Bb","B" };
    for (auto k : keys) keyBox.addItem(k, keyBox.getNumItems() + 1);

    // scales – you'll push the selected index into the generator; the full list already lives in your code
    // (Editor doesn’t need to duplicate formulas; it only selects one)
    const String scales[]{
        "Major","Natural Minor","Harmonic Minor","Dorian","Phrygian","Lydian","Mixolydian","Aeolian","Locrian",
        "Locrian ♮6","Ionian #5","Dorian #4","Phrygian Dom","Lydian #2","Super Locrian","Dorian b2","Lydian Aug",
        "Lydian Dom","Mixo b6","Locrian #2","Ethiopian Min","8 Tone Spanish","Phrygian ♮3","Blues","Hungarian Min",
        "Harmonic Maj","Pentatonic Maj","Pentatonic Min","Neopolitan Maj","Neopolitan Min","Spanish Gypsy",
        "Romanian Minor","Chromatic","Bebop Major","Bebop Minor"
    };
    for (auto& s : scales) scaleBox.addItem(s, scaleBox.getNumItems() + 1);

    // time signatures (expanded)
    const String timeSigs[]{
        "1/4","2/4","3/4","4/4","5/4","7/4","2/2","3/2",
        "5/8","6/8","7/8","9/8","10/8","11/8","12/8","13/8","15/8",
        "5/16","7/16","9/16","11/16","13/16","15/16"
    };
    for (auto& t : timeSigs) tsBox.addItem(t, tsBox.getNumItems() + 1);

    // bars: ONLY 4 or 8 (per your spec)
    barsBox.addItem("4", 1);
    barsBox.addItem("8", 2);

    // default selections
    keyBox.setSelectedId(1, dontSendNotification); // C
    scaleBox.setSelectedId(1, dontSendNotification); // Major
    tsBox.setSelectedId(4, dontSendNotification); // 4/4
    barsBox.setSelectedId(1, dontSendNotification); // 4 bars

    for (auto* C : { &keyBox, &scaleBox, &tsBox, &barsBox })
    {
        addAndMakeVisible(*C);
        styleCombo(*C, colDropBg, colText);
        C->addListener(this);
    }

    // ---------- sliders ----------
    auto prepSlider = [this](Slider& s, Label& lab, const char* text)
    {
        s.setRange(0.0, 100.0, 1.0);
        s.setValue(0.0);
        addAndMakeVisible(s);
        styleSlider(s, colAccent, colAccent, colText);
        s.addListener(this);

        lab.setText(text, dontSendNotification);
        lab.setJustificationType(Justification::centred);
        lab.setColour(Label::textColourId, colText);
        lab.setFont(Font(Font::FontOptions(16.0f, Font::bold)));
        addAndMakeVisible(lab);
    };

    prepSlider(restSl, restLbl, "REST DENS");
    prepSlider(timingSl, timingLbl, "TIMING");
    prepSlider(velocitySl, velocityLbl, "VELOCITY");
    prepSlider(swingSl, swingLbl, "SWING");
    prepSlider(feelSl, feelLbl, "FEEL");

    // ---------- piano roll + viewport ----------
    pianoRoll = std::make_unique<PianoRollComponent>();
    addAndMakeVisible(pianoViewport);
    pianoViewport.setViewedComponent(pianoRoll.get(), false);
    pianoViewport.setScrollBarsShown(true, true);
    pianoViewport.setScrollOnDragEnabled(true);

    // apply palette to the roll (you added setPalette earlier)
    struct PRPal { juce::Colour rollBg, whiteKey, blackKey, grid; };
    pianoRoll->setPalette({
        colRollBg, colWhiteKey, colBlackKey, colRollGrid
        });

    updateRollContentSize();
}

BANGAudioProcessorEditor::~BANGAudioProcessorEditor() = default;

//============================================================
void BANGAudioProcessorEditor::paint(Graphics& g)
{
    g.fillAll(colBg);
}

void BANGAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    const int topPad = 12;
    const int sidePad = 14;

    // --- top row: logo centered, humanize sliders right, selectors left ---
    auto top = r.removeFromTop(160).reduced(sidePad, topPad);

    // left block: key / scale / TS / bars + rest slider
    auto left = top.removeFromLeft(int(getWidth() * 0.33)).reduced(8);
    const int rowH = 26;
    const int labW = 100;
    const int comboW = jmax(140, left.getWidth() - labW - 14);

    auto putCombo = [&](Label& L, ComboBox& C)
    {
        auto row = left.removeFromTop(rowH);
        L.setBounds(row.removeFromLeft(labW));
        C.setBounds(row.removeFromLeft(comboW));
        left.removeFromTop(6);
    };

    putCombo(keyLbl, keyBox);
    putCombo(scaleLbl, scaleBox);
    putCombo(tsLbl, tsBox);
    putCombo(barsLbl, barsBox);

    // rest slider occupies 2 rows
    auto restArea = left.removeFromTop(rowH * 2);
    restLbl.setBounds(restArea.removeFromTop(rowH));
    restSl.setBounds(restArea.reduced(0, 4));

    // right block: humanize sliders
    auto right = top.removeFromRight(int(getWidth() * 0.33)).reduced(8);
    auto putSlider = [&](Label& L, Slider& S)
    {
        auto rr = right.removeFromTop(56);
        L.setBounds(rr.removeFromTop(18));
        S.setBounds(rr.reduced(0, 4));
        right.removeFromTop(4);
    };
    putSlider(timingLbl, timingSl);
    putSlider(velocityLbl, velocitySl);
    putSlider(swingLbl, swingSl);
    putSlider(feelLbl, feelSl);

    // middle: logo centered + 'ENGINE' title + three engine buttons
    auto mid = top.reduced(8); // remaining middle
    const int logoH = 78;
    logoImg.setBounds(mid.removeFromTop(logoH));
    engineTitleImg.setBounds(mid.removeFromTop(28));

    const int engineW = 320;
    auto engineRow = mid.removeFromTop(64);
    engineRow = engineRow.withSizeKeepingCentre(engineW, engineRow.getHeight());
    {
        auto third = engineRow.withWidth(engineW / 3);
        engineChordsBtn.setBounds(third);
        third = third.withX(third.getRight());
        engineMixtureBtn.setBounds(third);
        third = third.withX(third.getRight());
        engineMelodyBtn.setBounds(third);
    }

    // row of ADVANCED / POLYR / REHARM centered beneath
    auto smallRow = mid.removeFromTop(48);
    smallRow = smallRow.withSizeKeepingCentre(480, smallRow.getHeight());
    const int smallW = 150, smallPad = 15;
    auto s1 = smallRow.removeFromLeft(smallW);
    advancedBtn.setBounds(s1.reduced(4));
    smallRow.removeFromLeft(smallPad);
    auto s2 = smallRow.removeFromLeft(smallW);
    polyrBtn.setBounds(s2.reduced(4));
    smallRow.removeFromLeft(smallPad);
    auto s3 = smallRow.removeFromLeft(smallW);
    reharmBtn.setBounds(s3.reduced(4));

    // DRAG + GENERATE under that, centered
    auto bigRow = mid.removeFromTop(56);
    bigRow = bigRow.withSizeKeepingCentre(360, bigRow.getHeight());
    auto leftBig = bigRow.removeFromLeft(180);
    dragBtn.setBounds(leftBig.reduced(6));
    generateBtn.setBounds(bigRow.reduced(6));

    // ADJUST button goes bottom-left under selectors, not hidden by roll
    auto adjustArea = r.removeFromTop(40).removeFromLeft(260).reduced(6);
    adjustBtn.setBounds(adjustArea);

    // DICE button goes top-right corner
    auto diceArea = getLocalBounds().removeFromTop(60).removeFromRight(80).reduced(8);
    diceBtn.setBounds(diceArea);

    // piano viewport fills remaining
    pianoViewport.setBounds(r.reduced(sidePad, 4));
    updateRollContentSize();
}

//============================================================
// selectors / sliders
void BANGAudioProcessorEditor::comboBoxChanged(ComboBox* box)
{
    if (box == &tsBox || box == &barsBox)
    {
        updateRollContentSize();
        if (pianoRoll)
            pianoRoll->setTimeSignature(currentTSNumerator(), currentTSDenominator());
    }

    pushSettingsToGenerator();
}

void BANGAudioProcessorEditor::sliderValueChanged(Slider* s)
{
    // just push to generator – generation happens on Generate / Dice
    pushSettingsToGenerator();
}

//============================================================
// resource helpers
Image BANGAudioProcessorEditor::loadImageAny(std::initializer_list<const char*> tryNames) const
{
    for (auto* name : tryNames)
    {
        int dataSize = 0;
        if (auto* data = BinaryData::getNamedResource(name, dataSize))
        {
            auto img = ImageCache::getFromMemory(data, dataSize);
            if (img.isValid())
                return img;
        }
    }
    return {};
}

std::unique_ptr<Drawable> BANGAudioProcessorEditor::makeDrawable(const Image& img)
{
    if (!img.isValid()) return {};
    auto p = std::make_unique<DrawableImage>();
    p->setImage(img);
    return p;
}

void BANGAudioProcessorEditor::applyImageButton3(ImageButton& btn,
    std::initializer_list<const char*> normalNames,
    std::initializer_list<const char*> overNames,
    std::initializer_list<const char*> downNames,
    Rectangle<int> bounds)
{
    const Image normal = loadImageAny(normalNames);
    const Image over = loadImageAny(overNames);
    const Image down = loadImageAny(downNames);
    btn.setClickingTogglesState(false);
    btn.setTriggeredOnMouseDown(false);
    btn.setImages(
        /*resizeToFit*/ true,
        /*preserveProportions*/ true,
        /*opaque*/ true,
        normal, 1.0f, juce::Colours::transparentBlack,
        over, 1.0f, juce::Colours::transparentBlack,
        down, 1.0f, juce::Colours::transparentBlack
    );
        Image(), 1.0f, Colours::transparentBlack
    );
    btn.setBounds(bounds);
    addAndMakeVisible(btn);
}

//============================================================
// mapping / parsing
int BANGAudioProcessorEditor::rootBoxToSemitone(const ComboBox& key)
{
    const String txt = key.getText().trim();
    if (txt == "C")  return 0;
    if (txt == "C#" || txt == "Db") return 1;
    if (txt == "D")  return 2;
    if (txt == "D#" || txt == "Eb") return 3;
    if (txt == "E")  return 4;
    if (txt == "F")  return 5;
    if (txt == "F#" || txt == "Gb") return 6;
    if (txt == "G")  return 7;
    if (txt == "G#" || txt == "Ab") return 8;
    if (txt == "A")  return 9;
    if (txt == "A#" || txt == "Bb") return 10;
    if (txt == "B")  return 11;
    return 0;
}

int BANGAudioProcessorEditor::currentBars() const
{
    const int id = barsBox.getSelectedId();
    return id == 2 ? 8 : 4;
}

static std::pair<int, int> parseTS(const String& s)
{
    const auto parts = StringArray::fromTokens(s, "/", "");
    if (parts.size() == 2)
        return { parts[0].getIntValue(), parts[1].getIntValue() };
    return { 4, 4 };
}

int BANGAudioProcessorEditor::currentTSNumerator() const
{
    return parseTS(tsBox.getText()).first;
}
int BANGAudioProcessorEditor::currentTSDenominator() const
{
    return parseTS(tsBox.getText()).second;
}

void BANGAudioProcessorEditor::updateRollContentSize()
{
    if (!pianoRoll) return;

    const int bars = currentBars();
    const int num = currentTSNumerator();
    const int den = currentTSDenominator();

    // You already implemented bar numbering and spacing in PianoRollComponent;
    // here we just update its content size generously to allow scrolling.
    const int pixelsPerBeat = 120; // wide on purpose so scroll is meaningful
    const float beatsPerBar = (den == 8 ? num * 0.5f : (float)num);
    const int width = int(beatsPerBar * bars * pixelsPerBeat);

    pianoRoll->setTimeSignature(num, den);
    pianoRoll->setBars(bars);
    pianoRoll->setSize(jmax(width, pianoViewport.getWidth() + 1),
        jmax(pianoViewport.getHeight() + 200, 600));
}

//============================================================
// Generator wiring
void BANGAudioProcessorEditor::pushSettingsToGenerator()
{
    auto& g = audioProcessor.getMidiGenerator();

    // engine
    const int engineMode = (currentEngine == EngineMode::Chords ? 0
        : currentEngine == EngineMode::Mixture ? 1 : 2);
    g.setEngineMode(static_cast<MidiGenerator::EngineMode>(currentEngineIndex));;

    // musical context
    g.setScaleIndex(scaleBox.getSelectedId() - 1); // 0-based for your scale list
    g.setTimeSignature(currentTSNumerator(), currentTSDenominator());
    g.setBars(currentBars());

    // density / humanize
    g.setRestDensity((float)restSl.getValue() / 100.0f);
    g.setHumanizeTiming((float)timingSl.getValue() / 100.0f);
    g.setHumanizeVelocity((float)velocitySl.getValue() / 100.0f);
    g.setSwingAmount((float)swingSl.getValue() / 100.0f);
    g.setFeelAmount((float)feelSl.getValue() / 100.0f);
}

void BANGAudioProcessorEditor::regenerate()
{
    pushSettingsToGenerator();
    auto& g = audioProcessor.getMidiGenerator();

    // Call the unified generator; you already implemented this to honor options.
    (void)g.generateMelodyAndChords(true);

    // The piano roll pulls from the generator’s internal note buffer or is fed externally.
    // If your PianoRoll exposes setNotes, you can push them here; otherwise repaint.
    pianoRoll->repaint();
}

//============================================================
// External MIDI drag (writes a temp file then starts OS drag)
void BANGAudioProcessorEditor::performDragExport()
{
    // Write a temp MIDI file using your processor/generator helper
    File temp = File::createTempFile("BANG_DragExport.mid");
    if (auto stream = temp.createOutputStream())
    {
        performDragExport(); // <- you already had this helper, or similar
        stream->flush();
    }

    if (temp.existsAsFile() && dndHelper)
    {
        StringArray files; files.add(temp.getFullPathName());
        dndHelper->performExternalDragDropOfFiles(files, true, nullptr);
    }
}

//============================================================
// Windows
void BANGAudioProcessorEditor::openAdvanced()
{
    // Your AdvancedHarmonyWindow already exists.
    DialogWindow::LaunchOptions o;
    o.content.setOwned(new AdvancedHarmonyWindow(audioProcessor.getMidiGenerator()), true);
    o.dialogTitle = "Advanced Harmony";
    o.dialogBackgroundColour = colBg;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.runModal();
}

void BANGAudioProcessorEditor::openPolyrhythm()
{
    // Reuse a small DialogWindow with your polyrhythm component (you’ve coded it before).
    // Assuming you exposed a factory on the generator; otherwise wrap your existing component.
    Component* poly = audioProcessor.createPolyrhythmEditor(); // you have this from before
    if (poly == nullptr) return;

    DialogWindow::LaunchOptions o;
    o.content.setOwned(poly, true);
    o.dialogTitle = "Polyrhythm";
    o.dialogBackgroundColour = colBg;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.runModal();
}

void BANGAudioProcessorEditor::openReharmonize()
{
    Component* reh = audioProcessor.createReharmEditor(); // you wired this earlier
    if (reh == nullptr) return;

    DialogWindow::LaunchOptions o;
    o.content.setOwned(reh, true);
    o.dialogTitle = "Reharmonize";
    o.dialogBackgroundColour = colBg;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.runModal();
}

void BANGAudioProcessorEditor::openAdjust()
{
    Component* adj = audioProcessor.createAdjustEditor(); // your “ADJUST” window (checkboxes + dropdowns)
    if (adj == nullptr) return;

    DialogWindow::LaunchOptions o;
    o.content.setOwned(adj, true);
    o.dialogTitle = "Adjust";
    o.dialogBackgroundColour = colBg;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.runModal();
}

//============================================================
// Dice (non-minimal: actually randomizes all basic selectors + toggles)
void BANGAudioProcessorEditor::randomizeAll()
{
    Random r(Time::getMillisecondCounter());

    // Key / Scale
    keyBox.setSelectedId(1 + r.nextInt(keyBox.getNumItems()), dontSendNotification);
    scaleBox.setSelectedId(1 + r.nextInt(scaleBox.getNumItems()), dontSendNotification);

    // Time signature
    tsBox.setSelectedId(1 + r.nextInt(tsBox.getNumItems()), dontSendNotification);

    // Bars (only 4 or 8)
    barsBox.setSelectedId(1 + r.nextInt(barsBox.getNumItems()), dontSendNotification);

    // Humanize + rest density
    restSl.setValue(r.nextInt({ 0, 100 }), dontSendNotification);
    timingSl.setValue(r.nextInt({ 0, 100 }), dontSendNotification);
    velocitySl.setValue(r.nextInt({ 0, 100 }), dontSendNotification);
    swingSl.setValue(r.nextInt({ 0, 100 }), dontSendNotification);
    feelSl.setValue(r.nextInt({ 0, 100 }), dontSendNotification);

    // Engine
    const int e = r.nextInt(3);
    currentEngine = (e == 0 ? EngineMode::Chords : e == 1 ? EngineMode::Mixture : EngineMode::Melody);
    engineChordsBtn.setToggleState(currentEngine == EngineMode::Chords, dontSendNotification);
    engineMixtureBtn.setToggleState(currentEngine == EngineMode::Mixture, dontSendNotification);
    engineMelodyBtn.setToggleState(currentEngine == EngineMode::Melody, dontSendNotification);

    // Optional: randomly open/enable advanced windows (we keep it simple – the window is user-driven)
    updateRollContentSize();
    pushSettingsToGenerator();
}
