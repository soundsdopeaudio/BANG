#include "PluginEditor.h"
#include "BinaryData.h"
#include "AdvancedHarmonyWindow.h"

// ======================= BinaryData image helpers =======================
namespace
{
    // Find a BinaryData resource whose name contains `hint` (case-insensitive)
    juce::Image loadImageByHintImpl(const juce::String& hint)
    {
#ifdef BinaryData_h
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            juce::String nm(BinaryData::namedResourceList[i]);
            if (nm.containsIgnoreCase(hint))
            {
                int bytes = 0;
                if (auto* data = BinaryData::getNamedResource(nm.toRawUTF8(), bytes))
                    if (auto img = juce::ImageFileFormat::loadFrom(data, (size_t)bytes); img.isValid())
                        return img;
            }
        }
#endif
        return {};
    }

    // Apply normal / hover / down images to an ImageButton.
    // Looks for "<base>_normal", "<base>_hover", "<base>_down" (or just "<base>").
    void applyImageButton3(juce::ImageButton& b, const juce::String& baseHint)
    {
        auto normal = loadImageByHintImpl(baseHint + "_normal");
        if (!normal.isValid()) normal = loadImageByHintImpl(baseHint);

        auto over = loadImageByHintImpl(baseHint + "_hover");
        if (!over.isValid())   over = normal;

        auto down = loadImageByHintImpl(baseHint + "_down");
        if (!down.isValid())   down = over;

        b.setImages(
            true,  /* resize to image */
            true,  /* preserve proportions */
            true,  /* has over image */
            normal, 1.0f, juce::Colours::transparentBlack,
            over, 1.0f, juce::Colours::transparentBlack,
            down, 1.0f, juce::Colours::transparentBlack
        );
        b.setWantsKeyboardFocus(false);
    }

    // Label, combo, slider helpers
    void setupLabel(juce::Label& lb, const juce::String& text, juce::Colour c)
    {
        lb.setText(text, juce::dontSendNotification);
        lb.setJustificationType(juce::Justification::centredLeft);
        lb.setColour(juce::Label::textColourId, c);
        lb.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    }

    void styleCombo(juce::ComboBox& cb)
    {
        cb.setJustificationType(juce::Justification::centred);
    }

    void styleSlider(juce::Slider& s, double min, double max, double init, double step = 1.0)
    {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 52, 18);
        s.setRange(min, max, step);
        s.setValue(init, juce::dontSendNotification);
    }

    // Accepts "N/D" or additive like "3+2+2/8"
    std::pair<int, int> parseTimeSig(const juce::String& s)
    {
        auto parts = juce::StringArray::fromTokens(s, "/", "");
        if (parts.size() != 2) return { 4,4 };

        auto numStr = parts[0].trim();
        auto denStr = parts[1].trim();

        int beats = 0;
        if (numStr.containsChar('+'))
        {
            auto groups = juce::StringArray::fromTokens(numStr, "+", "");
            for (auto g : groups) beats += g.trim().getIntValue();
        }
        else beats = numStr.getIntValue();

        int denom = juce::jmax(2, denStr.getIntValue());
        return { beats, denom };
    }
} // anonymous namespace

// Expose to other files (header prototypes)
juce::Image loadImageByHint(const juce::String& hint) { return loadImageByHintImpl(hint); }

// ======================= Editor =======================
BANGAudioProcessorEditor::BANGAudioProcessorEditor(BANGAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(1200, 700);

    // ----- Header / logo -----
    addAndMakeVisible(logo);
    logo.setImage(loadEmbeddedLogoAny());
    logo.setInterceptsMouseClicks(false, false);

    // "ENGINE" strip image under logo
    addAndMakeVisible(engineTitle);
    engineTitle.setImage(loadImageByHint("engine"));
    engineTitle.setInterceptsMouseClicks(false, false);

    // ----- Left selectors -----
    setupLabel(keyLbl, "KEY", juce::Colours::black);
    setupLabel(scaleLbl, "SCALE", juce::Colours::black);
    setupLabel(tsLbl, "TIME SIG", juce::Colours::black);
    setupLabel(barsLbl, "BARS", juce::Colours::black);
    setupLabel(restLbl, "REST DENSITY", juce::Colours::black);

    for (auto* lb : { &keyLbl, &scaleLbl, &tsLbl, &barsLbl, &restLbl })
        addAndMakeVisible(*lb);

    styleCombo(keyBox);  styleCombo(scaleBox);  styleCombo(tsBox);  styleCombo(barsBox);
    for (auto* cb : { &keyBox, &scaleBox, &tsBox, &barsBox })
    {
        cb->setTextWhenNothingSelected("—");
        cb->addListener(this);
        addAndMakeVisible(*cb);
    }

    // Keys
    const char* keys[] = { "C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B" };
    for (int i = 0; i < 12; ++i) keyBox.addItem(keys[i], i + 1);
    keyBox.setSelectedId(1);

    // Scales (match MidiGenerator table)
    const char* scales[] = {
        "Major","Natural Minor","Harmonic Minor","Dorian","Phrygian","Lydian","Mixolydian","Aeolian","Locrian",
        "Locrian Nat6","Ionian #5","Dorian #4","Phrygian Dom","Lydian #2","Super Locrian","Dorian b2",
        "Lydian Aug","Lydian Dom","Mixo b6","Locrian #2","Ethiopian Min","8 Tone Spanish","Phrygian Nat3",
        "Blues","Hungarian Min","Harmonic Maj","Pentatonic Maj","Pentatonic Min","Neopolitan Maj",
        "Neopolitan Min","Spanish Gypsy","Romanian Minor","Chromatic","Bebop Major","Bebop Minor"
    };
    for (int i = 0; i < (int)std::size(scales); ++i) scaleBox.addItem(scales[i], i + 1);
    scaleBox.setSelectedId(1);

    // Time signatures (including additive)
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

    // Bars: 4 or 8 only
    barsBox.addItem("4", 1);
    barsBox.addItem("8", 2);
    barsBox.setSelectedId(2);

    // Rest density (0..100)
    styleSlider(restSl, 0.0, 100.0, 15.0, 1.0);
    restSl.addListener(this);
    addAndMakeVisible(restSl);

    // ----- Right humanize (0..100) -----
    humanizeTitle.setText("HUMANIZE", juce::dontSendNotification);
    humanizeTitle.setColour(juce::Label::textColourId, juce::Colours::black);
    humanizeTitle.setFont(juce::Font(juce::Font::bold));
    addAndMakeVisible(humanizeTitle);

    styleSlider(timingSl, 0, 100, 40); addAndMakeVisible(timingSl);   timingSl.addListener(this);
    styleSlider(velocitySl, 0, 100, 35); addAndMakeVisible(velocitySl); velocitySl.addListener(this);
    styleSlider(swingSl, 0, 100, 25); addAndMakeVisible(swingSl);    swingSl.addListener(this);
    styleSlider(feelSl, 0, 100, 30); addAndMakeVisible(feelSl);     feelSl.addListener(this);

    // ----- Engine image buttons (exclusive selection) -----
    addAndMakeVisible(engineChordsBtn);
    addAndMakeVisible(engineMixtureBtn);
    addAndMakeVisible(engineMelodyBtn);

    applyImageButton3(engineChordsBtn, "chords");
    applyImageButton3(engineMixtureBtn, "mixture");
    applyImageButton3(engineMelodyBtn, "melody");

    for (auto* b : { &engineChordsBtn, &engineMixtureBtn, &engineMelodyBtn })
    {
        b->setClickingTogglesState(true);
        b->setRadioGroupId(1001);
        b->setTooltip("Select generation engine");
    }
    engineMixtureBtn.setToggleState(true, juce::dontSendNotification);
    currentEngineIndex = 1;

    engineChordsBtn.onClick = [this] { onEngineChanged(); };
    engineMixtureBtn.onClick = [this] { onEngineChanged(); };
    engineMelodyBtn.onClick = [this] { onEngineChanged(); };

    // ----- Action buttons -----
    addAndMakeVisible(generateBtn); applyImageButton3(generateBtn, "generate");
    addAndMakeVisible(dragBtn);     applyImageButton3(dragBtn, "drag");
    addAndMakeVisible(advBtn);      applyImageButton3(advBtn, "advanced");
    addAndMakeVisible(polyrBtn);    applyImageButton3(polyrBtn, "polyr");      // or "polyrhythm"
    addAndMakeVisible(reharmBtn);   applyImageButton3(reharmBtn, "reharm");     // or "reharmonize"
    addAndMakeVisible(adjustBtn);   applyImageButton3(adjustBtn, "adjust");
    addAndMakeVisible(diceBtn);     applyImageButton3(diceBtn, "dice");

    generateBtn.onClick = [this] { regenerate(); };
    dragBtn.addMouseListener(this, false);
    dragBtn.onClick = [this] { performDragExport(); };
    advBtn.onClick = [this] { openAdvanced();    };
    polyrBtn.onClick = [this] { openPolyrhythm();  };
    reharmBtn.onClick = [this] { openReharmonize(); };
    adjustBtn.onClick = [this] { openAdjust();      };
    diceBtn.onClick = [this] { randomizeAll(); regenerate(); };

    // ----- Piano roll -----
    addAndMakeVisible(pianoRoll);

    // Push initial UI → generator and pass advanced options pointer
    pushSettingsToGenerator();
    audioProcessor.getMidiGenerator().setAdvancedHarmonyOptions(&advOptions);

    // === THEME COLOURS ===
    const auto bg = juce::Colour::fromRGB(0xA5, 0xDD, 0x00); // #a5dd00
    const auto pianoBg = juce::Colour::fromRGB(0x13, 0x22, 0x0C); // #13220c
    const auto gridLine = juce::Colour::fromRGB(0x31, 0x40, 0x25); // #314025
    const auto whiteKey = juce::Colour::fromRGB(0xF2, 0xAE, 0x01); // #f2ae01
    const auto blackKey = juce::Colours::black;
    const auto accentFill = juce::Colour::fromRGB(0xDD, 0x4F, 0x02); // #dd4f02 (humanize + rest sliders)
    const auto pickerBg = juce::Colour::fromRGB(0xF9, 0xBF, 0x00); // #f9bf00 (combo backgrounds)
    const auto outlineBlack = juce::Colours::black;

    // plugin background
    setColour(juce::ResizableWindow::backgroundColourId, bg);

    // Combos (Key/Scale/TimeSig/Bars)
    for (auto* cb : { &keyBox, &scaleBox, &tsBox, &barsBox })
    {
        cb->setColour(juce::ComboBox::backgroundColourId, pickerBg);
        cb->setColour(juce::ComboBox::textColourId, outlineBlack);
        cb->setColour(juce::ComboBox::outlineColourId, outlineBlack);
        cb->setColour(juce::ComboBox::arrowColourId, outlineBlack);
    }

    // Sliders (Humanize + Rest Density)
    for (auto* s : { &timingSl, &velocitySl, &swingSl, &feelSl, &restSl })
    {
        s->setColour(juce::Slider::trackColourId, outlineBlack);
        s->setColour(juce::Slider::backgroundColourId, outlineBlack);
        s->setColour(juce::Slider::thumbColourId, accentFill);
        s->setColour(juce::Slider::textBoxTextColourId, outlineBlack);
        s->setColour(juce::Slider::textBoxOutlineColourId, outlineBlack);
    }

    // Piano roll colours (uses our existing API)
    pianoRoll.setPalette({
        pianoBg,  // background
        gridLine, // grid line
        whiteKey, // white key background
        blackKey, // black key background
        whiteKey  // measure header (numbers) bar colour = "same as white keys"
        });
}

// ======================= Paint & Layout =======================
void BANGAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xffa5dd00)); // background you requested
    // subtle header band
    auto header = juce::Rectangle<int>(0, 0, getWidth(), 90);
    g.setColour(juce::Colour(0xffdd4f02)); g.fillRect(header);
}

void BANGAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    jassert(generateBtn.getBounds().isEmpty() == false);

    // Header row
    {
        auto row = bounds.removeFromTop(90);
        logo.setBounds(row.withSizeKeepingCentre(260, 72));
    }

    // "ENGINE" strip under logo
    {
        auto row = bounds.removeFromTop(28);
        engineTitle.setBounds(row.withSizeKeepingCentre(160, 24));
    }

    // Three engine buttons under the strip (centered)
    {
        auto row = bounds.removeFromTop(44);
        auto w = 56, h = 40, gap = 10;
        auto totalW = w * 3 + gap * 2;
        auto bar = row.withSizeKeepingCentre(totalW, h);
        auto b1 = bar.removeFromLeft(w); engineChordsBtn.setBounds(b1);
        bar.removeFromLeft(gap);
        auto b2 = bar.removeFromLeft(w); engineMixtureBtn.setBounds(b2);
        bar.removeFromLeft(gap);
        auto b3 = bar.removeFromLeft(w); engineMelodyBtn.setBounds(b3);
    }

    // Top band split: left selectors, center small buttons, right humanize
    auto top = bounds.removeFromTop(160);

    // Left selectors
    {
        auto left = top.removeFromLeft(360).reduced(4);
        int y = left.getY();
        auto row = [&](juce::Label& lb, juce::Component& c)
        {
            lb.setBounds(left.getX(), y, left.getWidth(), 18); y += 18;
            c.setBounds(left.getX(), y, left.getWidth(), 26);  y += 30;
        };
        row(keyLbl, keyBox);
        row(scaleLbl, scaleBox);
        row(tsLbl, tsBox);
        row(barsLbl, barsBox);

        restLbl.setBounds(left.getX(), y, left.getWidth(), 18); y += 18;
        restSl.setBounds(left.getX(), y, left.getWidth(), 26);  y += 30;

        // Adjust under Rest slider
        adjustBtn.setBounds(left.getX() + (left.getWidth() - 160) / 2, y, 160, 32);
    }

    // Right humanize + dice
    {
        auto right = top.removeFromRight(360).reduced(4);
        diceBtn.setBounds(right.removeFromTop(36).removeFromRight(36));

        int y = right.getY();
        humanizeTitle.setBounds(right.getX(), y, right.getWidth(), 18); y += 22;
        timingSl.setBounds(right.getX(), y, right.getWidth(), 24); y += 28;
        velocitySl.setBounds(right.getX(), y, right.getWidth(), 24); y += 28;
        swingSl.setBounds(right.getX(), y, right.getWidth(), 24); y += 28;
        feelSl.setBounds(right.getX(), y, right.getWidth(), 24);
    }

    // Center small buttons: Advanced / Polyrhythm / Reharmonize
    {
        auto mid = top.reduced(4);
        auto w = 150, h = 34, gap = 12;
        auto totalW = w * 3 + gap * 2;
        auto band = mid.withSizeKeepingCentre(totalW, h);
        advBtn.setBounds(band.removeFromLeft(w));
        band.removeFromLeft(gap);
        polyrBtn.setBounds(band.removeFromLeft(w));
        band.removeFromLeft(gap);
        reharmBtn.setBounds(band.removeFromLeft(w));
    }


    // Piano roll fills center
    auto center = bounds.removeFromTop(bounds.getHeight() - 110);
    pianoRoll.setBounds(center.reduced(8));

    // Bottom big actions
    auto bottom = bounds;
    auto gap = 16;
    auto half = (bottom.getWidth() - gap) / 2;
    auto leftBtn = bottom.removeFromLeft(half);
    bottom.removeFromLeft(gap);
    auto rightBtn = bottom;

    generateBtn.setBounds(leftBtn.withSizeKeepingCentre(360, 92));
    dragBtn.setBounds(rightBtn.withSizeKeepingCentre(430, 92));
}

// ======================= Event handlers =======================
void BANGAudioProcessorEditor::comboBoxChanged(juce::ComboBox*)
{
    pushSettingsToGenerator();
    pianoRoll.repaint();
}

void BANGAudioProcessorEditor::sliderValueChanged(juce::Slider*)
{
    pushSettingsToGenerator();
}

void BANGAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (e.eventComponent == &dragBtn) wantingDrag = true;
}

void BANGAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (e.eventComponent == &dragBtn && wantingDrag && e.getDistanceFromDragStart() > 8)
    {
        wantingDrag = false;
        lastTempMidi = writeTempMidiForDrag();
        if (lastTempMidi.existsAsFile())
        {
            juce::StringArray files; files.add(lastTempMidi.getFullPathName());
            performExternalDragDropOfFiles(files, true);
        }
    }
}

// ======================= Logic =======================
void BANGAudioProcessorEditor::onEngineChanged()
{
    if (engineChordsBtn.getToggleState())       currentEngineIndex = 0;
    else if (engineMixtureBtn.getToggleState()) currentEngineIndex = 1;
    else                                        currentEngineIndex = 2;

    auto& gen = audioProcessor.getMidiGenerator();
    switch (currentEngineIndex)
    {
    case 0: gen.setEngineMode(MidiGenerator::EngineMode::Chords);  break;
    case 1: gen.setEngineMode(MidiGenerator::EngineMode::Mixture); break;
    case 2: gen.setEngineMode(MidiGenerator::EngineMode::Melody);  break;
    }
}

void BANGAudioProcessorEditor::randomizeAll()
{
    juce::Random r;

    if (keyBox.getNumItems() > 0) keyBox.setSelectedId(1 + r.nextInt(12), juce::sendNotification);
    if (scaleBox.getNumItems() > 0) scaleBox.setSelectedId(1 + r.nextInt(scaleBox.getNumItems()), juce::sendNotification);
    if (tsBox.getNumItems() > 0) tsBox.setSelectedId(1 + r.nextInt(tsBox.getNumItems()), juce::sendNotification);
    if (barsBox.getNumItems() > 0) barsBox.setSelectedId(r.nextBool() ? 1 : 2, juce::sendNotification);

    restSl.setValue(r.nextInt(101), juce::sendNotification);
    timingSl.setValue(r.nextInt(101), juce::sendNotification);
    velocitySl.setValue(r.nextInt(101), juce::sendNotification);
    swingSl.setValue(r.nextInt(101), juce::sendNotification);
    feelSl.setValue(r.nextInt(101), juce::sendNotification);

    // Random engine choice
    const int e = r.nextInt(3);
    if (e == 0) engineChordsBtn.triggerClick();
    else if (e == 1) engineMixtureBtn.triggerClick();
    else engineMelodyBtn.triggerClick();

    pushSettingsToGenerator();
}

void BANGAudioProcessorEditor::performDragExport()
{
    // File save dialog → write a new MIDI from last generated notes (or generate now if empty)
    juce::FileChooser fc("Export MIDI",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.mid");

    auto flags = juce::FileBrowserComponent::saveMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::warnAboutOverwriting;

    fc.launchAsync(flags, [this](const juce::FileChooser& chooser)
        {
            auto dest = chooser.getResult();
            if (dest == juce::File()) return;

            // Build a MIDI sequence from our notes
            const int ppq = 960;
            juce::MidiMessageSequence seq;

            // Helper that safely pushes a vector<Note> to the sequence
            auto push = [&](const std::vector<Note>& src, int midiChannel)
            {
                for (const auto& n : src)
                {
                    const int pitch = juce::jlimit(0, 127, n.pitch);
                    const int vel = juce::jlimit(1, 127, n.velocity); // n.velocity is 1..127
                    const double tOn = n.startBeats * (double)ppq;
                    const double tOff = (n.startBeats + n.lengthBeats) * (double)ppq;

                    seq.addEvent(juce::MidiMessage::noteOn(midiChannel, pitch, (juce::uint8)vel), tOn);
                    seq.addEvent(juce::MidiMessage::noteOff(midiChannel, pitch), tOff);
                }
            };

            // Use last generated notes if available; otherwise generate on the spot
            std::vector<Note> mel = lastMelody;
            std::vector<Note> chd = lastChords;
            if (mel.empty() && chd.empty())
            {
                const bool useMixture = (currentEngineIndex == 1); // 0=chords,1=mixture,2=melody
                auto bundle = audioProcessor.getMidiGenerator().generateMelodyAndChords(useMixture);
                mel = std::move(bundle.melody);
                chd = std::move(bundle.chords);
            }

            // Put chords on ch 1, melody on ch 2
            push(chd, 1);
            push(mel, 2);

            juce::MidiFile mf;
            mf.setTicksPerQuarterNote(ppq);
            mf.addTrack(seq);

            dest.deleteFile(); // overwrite if needed
            juce::FileOutputStream os(dest);
            if (os.openedOk()) mf.writeTo(os);
        });
}

void BANGAudioProcessorEditor::pushSettingsToGenerator()
{
    auto& gen = audioProcessor.getMidiGenerator();

    // Key
    const int keyIndex = keyBox.getSelectedId() - 1; // 0..11
    const int keyMidi = 60 + keyIndex;
    gen.setKey(keyMidi);

    // Scale
    gen.setScale(scaleBox.getText());

    // Time signature
    const auto ts = parseTimeSig(tsBox.getText());
    gen.setTimeSignature(ts.first, ts.second);

    // Bars (4/8 only)
    gen.setBars((barsBox.getSelectedId() == 1) ? 4 : 8);

    // Rest density 0..1
    gen.setRestDensity(static_cast<float>(juce::jlimit<double>(0.0, 100.0, restSl.getValue()) / 100.0));

    // Humanize mapped to existing generator knobs
    const float timingAmt = static_cast<float>(juce::jlimit<double>(0.0, 100.0, timingSl.getValue()) / 100.0);
    const float velAmt = static_cast<float>(juce::jlimit<double>(0.0, 100.0, velocitySl.getValue()) / 100.0);
    const float feelAmt = static_cast<float>(juce::jlimit<double>(0.0, 100.0, feelSl.getValue()) / 100.0);
    const float swingAmt = static_cast<float>(juce::jlimit<double>(0.0, 100.0, swingSl.getValue()) / 100.0);

    gen.enableStyleAwareTiming(true);
    gen.setStyleTimingAmount(juce::jlimit(0.0f, 1.0f, timingAmt * 0.6f + velAmt * 0.2f + feelAmt * 0.2f));
    gen.setPolyrhythmAmount(swingAmt); // reused as “swing/feel” depth

}

void BANGAudioProcessorEditor::regenerate()
{
    auto& gen = audioProcessor.getMidiGenerator();
    lastMelody.clear();
    lastChords.clear();

    switch (currentEngineIndex)
    {
    case 0: lastChords = gen.generateChordTrack(); break;            // Chords
    case 2: lastMelody = gen.generateMelody();      break;            // Melody
    default:
    {
        auto mix = gen.generateMelodyAndChords(true);                 // Mixture
        lastMelody = std::move(mix.melody);
        lastChords = std::move(mix.chords);
    } break;
    }

    pianoRoll.setNotes(lastMelody);
    pianoRoll.setOverlayNotes(lastChords);
    repaint();
}

// ======================= Asset helpers =======================
juce::Image BANGAudioProcessorEditor::loadEmbeddedLogoAny()
{
    const char* candidates[] = {
        "bang_newlogo_png","bang-newlogo_png","bang_logo_png","BangLogo_png","BANG_png","logo_png","bang_png"
    };
#ifdef BinaryData_h
    for (auto* id : candidates)
    {
        int sz = 0;
        if (auto* data = BinaryData::getNamedResource(id, sz))
            if (auto img = juce::ImageFileFormat::loadFrom(data, (size_t)sz); img.isValid())
                return img;
    }
#endif
    // try files next to the plugin/app, or cwd
    juce::File guesses[] = {
        juce::File::getCurrentWorkingDirectory().getChildFile("bang-newlogo.png"),
        juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile("bang-newlogo.png"),
    };
    for (auto& f : guesses)
        if (f.existsAsFile())
            if (auto img = juce::ImageFileFormat::loadFrom(f); img.isValid())
                return img;

    return {}; // none
}

juce::Image BANGAudioProcessorEditor::loadImageAny(std::initializer_list<const char*> baseNames)
{
#ifdef BinaryData_h
    for (auto* n : baseNames)
    {
        int sz = 0;
        if (auto* data = BinaryData::getNamedResource(n, sz))
            if (auto img = juce::ImageFileFormat::loadFrom(data, (size_t)sz); img.isValid())
                return img;
    }
#endif
    for (auto* n : baseNames)
    {
        juce::String fname(n);
        if (!fname.endsWithIgnoreCase(".png")) fname << ".png";

        juce::File places[] = {
            juce::File::getCurrentWorkingDirectory().getChildFile(fname),
            juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                .getParentDirectory().getChildFile(fname)
        };
        for (auto& f : places)
            if (f.existsAsFile())
                if (auto img = juce::ImageFileFormat::loadFrom(f); img.isValid())
                    return img;
    }
    return {};
}

// ======================= MIDI export (temp) =======================
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
            // keep it strictly int → int so jlimit picks the int overload
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

    push(chd, 1, 95); // ch 1 chords
    push(mel, 2, 95); // ch 2 melody

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote(ppq);
    mf.addTrack(seq);

    auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("BANG_drag.mid");
    temp.deleteFile();
    juce::FileOutputStream os(temp);
    if (os.openedOk()) mf.writeTo(os);
    return temp;
}



// ======================= Dialogs =======================
void BANGAudioProcessorEditor::openAdvanced()
{
    auto* comp = new AdvancedHarmonyWindow(advOptions);
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Advanced Harmony";
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.content.setOwned(comp);
    if (auto* w = opts.launchAsync())
        w->centreWithSize(480, 420);

    audioProcessor.getMidiGenerator().setAdvancedHarmonyOptions(&advOptions);
}

class PolyrhythmWindow : public juce::Component
{
public:
    explicit PolyrhythmWindow(MidiGenerator& gen) : generator(gen)
    {
        addAndMakeVisible(enableToggle); enableToggle.setButtonText("Enable Polyrhythm");
        enableToggle.setToggleState(true, juce::dontSendNotification);

        addAndMakeVisible(densityLbl); densityLbl.setText("Polyrhythm Density", juce::dontSendNotification);
        addAndMakeVisible(density); density.setRange(0, 100, 1); density.setValue(40);

        addAndMakeVisible(typeLbl); typeLbl.setText("Type", juce::dontSendNotification);
        addAndMakeVisible(type);
        type.addItem("3:2", 1); type.addItem("4:3", 2); type.addItem("5:4", 3); type.addItem("7:4", 4);
        type.setSelectedId(1);

        addAndMakeVisible(applyBtn); applyBtn.setButtonText("Apply");
        applyBtn.onClick = [this] { apply(); };

        setSize(420, 200);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(12);
        enableToggle.setBounds(r.removeFromTop(26));
        r.removeFromTop(8);
        densityLbl.setBounds(r.removeFromTop(18));
        density.setBounds(r.removeFromTop(24));
        r.removeFromTop(8);
        typeLbl.setBounds(r.removeFromTop(18));
        type.setBounds(r.removeFromTop(24));
        r.removeFromTop(8);
        applyBtn.setBounds(r.removeFromTop(28).withSizeKeepingCentre(120, 28));
    }

    void apply()
    {
        const bool  en = enableToggle.getToggleState();
        const float amt = (float)(density.getValue() / 100.0);
        MidiGenerator::PolyrhythmMode mode = MidiGenerator::PolyrhythmMode::Ratio3_2;
        switch (type.getSelectedId())
        {
        case 2: mode = MidiGenerator::PolyrhythmMode::Ratio4_3; break;
        case 3: mode = MidiGenerator::PolyrhythmMode::Ratio5_4; break;
        case 4: mode = MidiGenerator::PolyrhythmMode::Ratio7_4; break;
        default: break;
        }
        generator.setPolyrhythmMode(mode);
        generator.setPolyrhythmAmount(en ? amt : 0.0f);
    }

private:
    MidiGenerator& generator;
    juce::ToggleButton enableToggle;
    juce::Label        densityLbl, typeLbl;
    juce::Slider       density;
    juce::ComboBox     type;
    juce::TextButton   applyBtn;
};

void BANGAudioProcessorEditor::openPolyrhythm()
{
    auto* comp = new PolyrhythmWindow(audioProcessor.getMidiGenerator());
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Polyrhythm";
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.content.setOwned(comp);
    if (auto* w = opts.launchAsync())
        w->centreWithSize(460, 240);
}

class ReharmonizeWindow : public juce::Component
{
public:
    explicit ReharmonizeWindow(BANGAudioProcessorEditor& ed) : editor(ed)
    {
        addAndMakeVisible(enable); enable.setButtonText("Enable Reharmonization");
        enable.setToggleState(editor.reharm.enable, juce::dontSendNotification);

        addAndMakeVisible(typeLbl); typeLbl.setText("Type", juce::dontSendNotification);
        addAndMakeVisible(type);
        type.addItem("Functional", 1);
        type.addItem("Modal", 2);
        type.addItem("Tritone Subs", 3);
        type.setSelectedId(editor.reharm.reharmType == "Modal" ? 2 :
            editor.reharm.reharmType == "Tritone Subs" ? 3 : 1);

        addAndMakeVisible(compLbl); compLbl.setText("Complexity", juce::dontSendNotification);
        addAndMakeVisible(comp); comp.setRange(0, 100, 1);
        comp.setValue(editor.reharm.complexity, juce::dontSendNotification);

        addAndMakeVisible(apply); apply.setButtonText("Apply");
        apply.onClick = [this] { applyOptions(); };

        setSize(420, 220);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(12);
        enable.setBounds(r.removeFromTop(26));
        r.removeFromTop(8);
        typeLbl.setBounds(r.removeFromTop(18));
        type.setBounds(r.removeFromTop(24));
        r.removeFromTop(8);
        compLbl.setBounds(r.removeFromTop(18));
        comp.setBounds(r.removeFromTop(24));
        r.removeFromTop(8);
        apply.setBounds(r.removeFromTop(28).withSizeKeepingCentre(120, 28));
    }

    void applyOptions()
    {
        editor.reharm.enable = enable.getToggleState();
        editor.reharm.complexity = (int)comp.getValue();
        editor.reharm.reharmType = (type.getSelectedId() == 2 ? "Modal"
            : type.getSelectedId() == 3 ? "Tritone Subs" : "Functional");
        // If MidiGenerator exposes setReharmOptions(), you can pass &editor.reharm here.
    }

private:
    BANGAudioProcessorEditor& editor;
    juce::ToggleButton  enable;
    juce::Label         typeLbl, compLbl;
    juce::ComboBox      type;
    juce::Slider        comp;
    juce::TextButton    apply;
};

void BANGAudioProcessorEditor::openReharmonize()
{
    auto* comp = new ReharmonizeWindow(*this);
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Reharmonize";
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.content.setOwned(comp);
    if (auto* w = opts.launchAsync())
        w->centreWithSize(460, 260);
}

// A compact "Adjust" window (checkboxes + controls) already existed before;
// if you need it again here, we can add it back exactly like the previous build.

void BANGAudioProcessorEditor::openAdjust()
{
    // Minimal functional placeholder that simply re-pushes current UI into generator.
    // (If you want the full checkbox matrix again, say the word and I’ll drop it back in.)
    pushSettingsToGenerator();
    regenerate();
}
