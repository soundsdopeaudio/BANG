// ===============================
// BANG Loop Session Starter (JUCE/C++)
// Files in this single doc; split into separate .h/.cpp in your project.
// Requires JUCE modules: juce_core, juce_data_structures, juce_events, juce_audio_basics, juce_audio_utils, juce_gui_basics, juce_gui_extra
// ===============================

//==================== LoopSessionEngine.h ====================
#pragma once
#include <juce_core/juce_core.h>
#include <vector>
#include <array>

namespace bang
{
    enum class SectionType { Loop }; // kept for future-proofing

    struct ChordEvent { int bar{}, beat{}; juce::Array<int> midiNotes; juce::String roman; };
    struct MelodyEvent { int tick{}, duration{}, velocity{}, midiNote{}; bool isRest{}; };

    struct LoopPlan
    {
        int bars = 4; // 4 or 8
        float tension = 0.3f; // 0..1
        std::array<float, 8> energyPerBar { 0.5f, 0.5f, 0.6f, 0.7f, 0.6f, 0.7f, 0.7f, 0.8f };
        int liftBar = 4; // 4 or 8
    };

    struct VariantSpec
    {
        int index = 1;           // 1..4
        int seedOffset = 9973;   // deterministic offset
        float liftAmount = 0.0f; // extra energy at lift bar
        float motifVariation = 0.25f; // 0..1
        juce::String name;       // V1/V2/V3/V4
    };

    struct LoopOutput
    {
        VariantSpec spec;
        std::vector<ChordEvent> chords;
        std::vector<MelodyEvent> melody;
        std::vector<MelodyEvent> transitions;
    };

    struct GenParams
    {
        int bars = 4; int variants = 3; int liftBar = 4; int seed = 1234;
        float aiInfluence = 0.0f; float tension = 0.3f; float motifVar = 0.25f;
        int bpm = 140; int timeSigNum = 4; int timeSigDen = 4;
        juce::String keyName = "C"; juce::String scaleName = "major";
        std::array<float, 8> energyPerBar { 0.5f, 0.5f, 0.6f, 0.7f, 0.6f, 0.7f, 0.7f, 0.8f };
    };

    // ------------ Helpers: RNG -------------
    inline int randi(int& state)
    {
        // simple xorshift32 for determinism
        uint32_t x = (uint32_t)state; x ^= x << 13; x ^= x >> 17; x ^= x << 5; state = (int)x; return (int)x;
    }
    inline float rand01(int& state) { return (randi(state) & 0x7fffffff) / (float)0x7fffffff; }

    // ------------ HarmonyDirector (diatonic + advanced harmony) -------------
    enum class ChordQuality { Maj, Min, Dim, Aug, Dom7, Maj7, Min7, HalfDim7 };

    struct RomanChord {
        juce::String roman;     // e.g. "I", "V/ii", "bVI", "iv"
        int semitoneFromTonic;  // root relative to tonic in semitones (can be chromatic)
        ChordQuality quality;
        bool borrowed = false;  // from parallel mode
        bool secondary = false; // is secondary dominant
    };

    struct HarmonyDirector
    {
        // Supported scales/modes (relative to tonic)
        static const std::map<juce::String, std::array<int, 7>>& scaleMap()
        {
            static const std::map<juce::String, std::array<int, 7>> S = {
           {"Major",         {0, 2, 4, 5, 7, 9, 11}},
           {"Natural Minor", {0, 2, 3, 5, 7, 8, 10}},
           {"Harmonic Minor",{0, 2, 3, 5, 7, 8, 11}},
           {"Dorian",        {0, 2, 3, 5, 7, 9, 10}},
           {"Phrygian",      {0, 1, 3, 5, 7, 8, 10}},
           {"Lydian",        {0, 2, 4, 6, 7, 9, 11}},
           {"Mixolydian",    {0, 2, 4, 5, 7, 9, 10}},
           {"Aeolian",       {0, 2, 3, 5, 7, 8, 10}},
           {"Locrian",       {0, 1, 3, 5, 6, 8, 10}},
           {"Locrian ♮6",    {0, 1, 3, 5, 6, 9, 10}},
           {"Ionian #5",     {0, 2, 4, 6, 7, 9, 11}},
           {"Dorian #4",     {0, 2, 3, 6, 7, 9, 10}},
           {"Phrygian Dom",  {0, 1, 3, 5, 7, 9, 10}},
           {"Lydian #2",     {0, 3, 4, 6, 7, 9, 11}},
           {"Super Locrian", {0, 1, 3, 4, 6, 8, 10}},
           {"Dorian b2",     {0, 1, 3, 5, 7, 9, 10}},
           {"Lydian Aug",    {0, 2, 4, 6, 8, 9, 11}},
           {"Lydian Dom",    {0, 2, 4, 6, 7, 9, 10}},
           {"Mixo b6",       {0, 2, 4, 5, 7, 8, 10}},
           {"Locrian #2",    {0, 2, 3, 5, 6, 8, 10}},
           {"Ethiopian Min", {0, 2, 3, 5, 7, 8, 10}},
           {"8 Tone Spanish", {0, 1, 3, 4, 5, 6, 8, 10}},
           {"Phyrgian ♮3",   {0, 1, 4, 5, 7, 8, 10}},
           {"Blues",         {0, 3, 5, 6, 7, 10}},
           {"Hungarian Min", {0, 3, 5, 8, 11}},
           {"Harmonic Maj",  {0, 2, 4, 5, 7, 8, 11}},
           {"Pentatonic Maj", {0, 2, 5, 7, 8}},
           {"Pentatonic Min", {0, 3, 5, 7, 10}},
           {"Neopolitan Maj", {0, 1, 3, 5, 7, 9, 11}},
           {"Neopolitan Min", {0, 1, 3, 5, 7, 8, 10}},,
           {"Spanish Gypsy",  {0, 1, 4, 5, 7, 8, 10}},
           {"Romanian Minor", {0, 2, 3, 6, 7, 9, 10}},
           {"Chromatic",      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
           {"Bebop Major",   {0, 2, 4, 5, 7, 8, 9, 11}},
           {"Bebop Minor",   {0, 2, 3, 5, 7, 8, 9, 10}}
            };
            return S;
        }

        static int keyNameToSemitone(const juce::String& key)
        {
            static std::map<juce::String, int> k = {
                {"C",0},{"C#",1},{"Db",1},{"D",2},{"D#",3},{"Eb",3},{"E",4},{"F",5},{"F#",6},{"Gb",6},
                {"G",7},{"G#",8},{"Ab",8},{"A",9},{"A#",10},{"Bb",10},{"B",11}
            };
            auto it = k.find(key.trim());
            return (it == k.end()) ? 0 : it->second;
        }

        static int wrap12(int x) { x %= 12; if (x < 0) x += 12; return x; }

        // Build a diatonic triad/seventh chord for a diatonic degree index (0..6)
        static void buildDiatonicChord(int tonicPc, const std::array<int, 7>& scale, int degree,
            bool add7th, juce::Array<int>& outPitches,
            ChordQuality& outQuality)
        {
            const int d = juce::jlimit(0, 6, degree);
            // Triad notes as scale degrees d, d+2, d+4
            int degs[3] = { d, (d + 2) % 7, (d + 4) % 7 };
            int pcs[3];
            for (int i = 0; i < 3; ++i) pcs[i] = wrap12(tonicPc + scale[degs[i]]);

            // Determine triad quality from intervals
            int i1 = wrap12(pcs[1](stub) ------------ -
                struct Motif { std::vector<int> degreeSteps; std::vector<int> rhythmTicks; }; // e.g., steps relative to chord scale-degree

            struct MotifEngine
            {
                static Motif selectMotif(const GenParams& P, int& rng)
                {
                    // Choose a 1-bar motif: very simple placeholder patterns
                    Motif m;
                    int pick = (randi(rng) % 3);
                    if (pick == 0) { m.degreeSteps = { 0, 2, 4, 2 }; m.rhythmTicks = { 0, 120, 240, 360 }; }
                    else if (pick == 1) { m.degreeSteps = { 0, 1, -1, 0 }; m.rhythmTicks = { 0, 180, 300, 420 }; }
                    else { m.degreeSteps = { 0, 3, 2, 1 }; m.rhythmTicks = { 0, 120, 300, 420 }; }
                    return m;
                }

                static Motif vary(const Motif& base, float variation, int& rng)
                {
                    Motif v = base;
                    for (auto& s : v.degreeSteps)
                        if (rand01(rng) < variation) s += ((randi(rng) % 3) - 1); // -1,0,+1
                    return v;
                }
            };

            // ------------ MelodyImproviser (stub) -------------
            struct MelodyImproviser
            {
                static std::vector<MelodyEvent> improviseLoop(const std::vector<ChordEvent>& chords,
                    const Motif& motif,
                    const LoopPlan& plan,
                    const GenParams& P,
                    int& rng)
                {
                    std::vector<MelodyEvent> out;
                    const int ppq = 480; // ticks per quarter for internal calc; exporter will map

                    // crude chord-scale map in C major for demo
                    static int scaleCMaj[7] = { 0,2,4,5,7,9,11 };

                    for (int bar = 0; bar < plan.bars; ++bar)
                    {
                        const auto& ce = chords[(size_t)bar];
                        int chordRoot = ce.midiNotes[0];
                        bool isLift = (bar + 1) == plan.liftBar;

                        // Repeat motif inside the bar
                        for (size_t i = 0; i < motif.degreeSteps.size(); ++i)
                        {
                            int degStep = motif.degreeSteps[i];
                            int tickInBar = motif.rhythmTicks[i % motif.rhythmTicks.size()];
                            int degree = juce::jlimit(0, 6, (degStep % 7 + 7) % 7);
                            int note = chordRoot - (scaleCMaj[(ce.roman == "vi") ? 5 : 0]) + 60; // rough normalize
                            note = 60 + scaleCMaj[degree];

                            // Register lift on lift bar
                            if (isLift) note += 3;

                            MelodyEvent me; me.tick = bar * (ppq * P.timeSigNum) + tickInBar; me.duration = 120; me.velocity = 96; me.midiNote = note; me.isRest = false;
                            out.push_back(me);
                        }

                        // small pickup into next bar at lift
                        if (isLift)
                        {
                            MelodyEvent pick; pick.tick = (bar + 1) * (ppq * P.timeSigNum) - 90; pick.duration = 90; pick.velocity = 110; pick.midiNote = 62; pick.isRest = false;
                            out.push_back(pick);
                        }
                    }
                    return out;
                }
            };

            // ------------ TransitionCrafter (stub) -------------
            struct TransitionCrafter
            {
                static std::vector<MelodyEvent> makeLoopTransitions(const LoopPlan& plan, int liftBar,
                    const std::vector<ChordEvent>& chords,
                    const GenParams& P, int& rng)
                {
                    juce::ignoreUnused(plan, liftBar, chords, P, rng);
                    return {}; // kept for future elaboration
                }
            };

            // ------------ LoopSessionEngine (public API) -------------
            struct LoopSessionEngine
            {
                static std::vector<LoopOutput> generateLoopSet(const GenParams& P)
                {
                    int rng = P.seed;
                    LoopPlan plan; plan.bars = P.bars; plan.tension = P.tension; plan.energyPerBar = P.energyPerBar; plan.liftBar = P.liftBar;

                    auto baseProg = HarmonyDirector::progressionForLoop(plan, P, rng);
                    auto baseMotif = MotifEngine::selectMotif(P, rng);

                    std::vector<LoopOutput> outs;
                    const int N = juce::jlimit(1, 4, P.variants);

                    for (int v = 0; v < N; ++v)
                    {
                        VariantSpec VS; VS.index = v + 1; VS.seedOffset = (v + 1) * 9973; VS.name = juce::String("V") + juce::String(v + 1);
                        if (v == 1) VS.liftAmount = 0.15f; if (v == 2) VS.motifVariation = P.motifVar * 1.2f; if (v == 3) { VS.liftAmount = 0.1f; VS.motifVariation = P.motifVar * 1.5f; }

                        int localRng = P.seed + VS.seedOffset;
                        auto motifV = MotifEngine::vary(baseMotif, juce::jlimit(0.0f, 1.0f, P.motifVar * (0.7f + 0.3f * v)), localRng);

                        auto voiced = baseProg; // placeholder; call a voiceAndExtend using plan/P/VS if available
                        auto mel = MelodyImproviser::improviseLoop(voiced, motifV, plan, P, localRng);
                        auto trans = TransitionCrafter::makeLoopTransitions(plan, P.liftBar, voiced, P, localRng);

                        outs.push_back(LoopOutput{ VS, std::move(voiced), std::move(mel), std::move(trans) });
                    }
                    return outs;
                }
            };
        }

        //==================== MidiExporter.h ====================
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

        namespace bang
        {
            struct MidiExporter
            {
                // Writes either one file per variant or a single file with markers naming each variant.
                static bool writeVariantsToFolder(const juce::File& folder, const std::vector<LoopOutput>& outs,
                    int ppq = 480, int bpm = 140, int tsNum = 4, int tsDen = 4)
                {
                    if (!folder.createDirectory()) return false;
                    bool ok = true;
                    for (const auto& L : outs)
                    {
                        juce::File f = folder.getChildFile("BANG_loop_" + L.spec.name + ".mid");
                        ok &= writeSingle(f, L, ppq, bpm, tsNum, tsDen);
                    }
                    return ok;
                }

                static bool writeSingle(const juce::File& file, const LoopOutput& L,
                    int ppq = 480, int bpm = 140, int tsNum = 4, int tsDen = 4)
                {
                    juce::MidiMessageSequence chords, melody, markers;

                    const double qnPerMin = (double)bpm;
                    const double secPerQn = 60.0 / qnPerMin;
                    juce::ignoreUnused(secPerQn);

                    // Chords as short block chords on beat 1 of each bar
                    for (const auto& c : L.chords)
                    {
                        const int tick = c.bar * ppq * tsNum;
                        for (auto note : c.midiNotes)
                        {
                            chords.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)90), tick);
                            chords.addEvent(juce::MidiMessage::noteOff(1, note), tick + ppq); // 1 beat long stub
                        }
                    }

                    // Melody
                    for (const auto& m : L.melody)
                    {
                        if (m.isRest) continue;
                        melody.addEvent(juce::MidiMessage::noteOn(2, m.midiNote, (juce::uint8)juce::jlimit(1, 127, m.velocity)), m.tick);
                        melody.addEvent(juce::MidiMessage::noteOff(2, m.midiNote), m.tick + juce::jmax(1, m.duration));
                    }

                    // Marker naming the variant
                    markers.addEvent(juce::MidiMessage::textMetaEvent(juce::MidiMessage::MetaEventType::textEvent, L.spec.name), 0);

                    juce::MidiFile mf; mf.setTicksPerQuarterNote(ppq);
                    mf.addTrack(chords);
                    mf.addTrack(melody);
                    mf.addTrack(markers);

                    juce::FileOutputStream out(file);
                    if (!out.openedOk()) return false;
                    mf.writeTo(out);
                    return true;
                }
            };
        }

        //==================== EnergyGridComponent.h ====================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

        namespace bang
        {
            class EnergyGridComponent : public juce::Component
            {
            public:
                EnergyGridComponent() { for (auto& s : sliders) { addAndMakeVisible(s); s.setRange(0.0, 1.0, 0.01); s.setValue(0.6); s.setSliderStyle(juce::Slider::LinearBarVertical); s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 16); } }
                void setBars(int bars) { numBars = juce::jlimit(4, 8, bars); repaint(); }
                void getValuesInto(std::array<float, 8>& arr) const { for (int i = 0; i < numBars; ++i) arr[(size_t)i] = (float)sliders[i].getValue(); }
                void setValuesFrom(const std::array<float, 8>& arr) { for (int i = 0; i < numBars; ++i) sliders[i].setValue(arr[(size_t)i], juce::dontSendNotification); }
                void resized() override
                {
                    auto r = getLocalBounds().reduced(4); int w = r.getWidth() / numBars; for (int i = 0; i < numBars; ++i) sliders[i].setBounds(r.removeFromLeft(w).reduced(2));
                }
            private:
                int numBars = 4; std::array<juce::Slider, 8> sliders;
            };
        }

        //==================== VariantTabsComponent.h ====================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

        namespace bang
        {
            class VariantTabsComponent : public juce::TabbedComponent
            {
            public:
                VariantTabsComponent() : juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop) {}
                void setNumVariants(int n)
                {
                    n = juce::jlimit(1, 4, n);
                    clearTabs();
                    for (int i = 0; i < n; ++i)
                    {
                        auto* comp = new juce::Component();
                        comp->setName("V" + juce::String(i + 1));
                        addTab(comp->getName(), juce::Colours::darkgrey, comp, true);
                    }
                }
            };
        }

        //==================== Processor & Editor (minimal glue) ====================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

        namespace bang
        {
            class BangAudioProcessor : public juce::AudioProcessor
            {
            public:
                BangAudioProcessor() : apvts(*this, nullptr, "PARAMS", createLayout()) {}
                ~BangAudioProcessor() override = default;

                // AudioProcessor overrides (minimal since this is MIDI generator)
                const juce::String getName() const override { return "BANG"; }
                double getTailLengthSeconds() const override { return 0.0; }
                bool acceptsMidi() const override { return false; }
                bool producesMidi() const override { return false; }
                bool isMidiEffect() const override { return true; }
                juce::AudioProcessorEditor* createEditor() override;
                bool hasEditor() const override { return true; }
                void prepareToPlay(double, int) override {}
                void releaseResources() override {}
                void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}

                // Params access
                juce::AudioProcessorValueTreeState apvts;
                static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
                {
                    using P = juce::AudioProcessorValueTreeState;
                    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
                    params.push_back(std::make_unique<juce::AudioParameterInt>("bars", "Bars", 4, 8, 4));
                    params.push_back(std::make_unique<juce::AudioParameterInt>("variants", "Variants", 1, 4, 3));
                    params.push_back(std::make_unique<juce::AudioParameterFloat>("ai", "AI Influence", 0.0f, 1.0f, 0.0f));
                    params.push_back(std::make_unique<juce::AudioParameterFloat>("tension", "Harmonic Tension", 0.0f, 1.0f, 0.3f));
                    params.push_back(std::make_unique<juce::AudioParameterFloat>("motifVar", "Motif Variation", 0.0f, 1.0f, 0.25f));
                    params.push_back(std::make_unique<juce::AudioParameterInt>("liftBar", "Lift Bar", 4, 8, 4));
                    params.push_back(std::make_unique<juce::AudioParameterInt>("bpm", "BPM", 60, 200, 140));
                    params.push_back(std::make_unique<juce::AudioParameterInt>("seed", "Seed", 1, 1000000, 1234));
                    return { params.begin(), params.end() };
                }
            };

            // ---------------- Editor ----------------
            class BangAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Button::Listener
            {
            public:
                explicit BangAudioProcessorEditor(BangAudioProcessor& p)
                    : juce::AudioProcessorEditor(&p), proc(p)
                {
                    setSize(700, 420);
                    addAndMakeVisible(energy);
                    addAndMakeVisible(variantsTabs);
                    addAndMakeVisible(generateBtn);
                    addAndMakeVisible(exportBtn);

                    variantsTabs.setNumVariants(proc.apvts.getParameterAsValue("variants").getValue());

                    generateBtn.setButtonText("Generate Variants");
                    exportBtn.setButtonText("Export MIDIs");
                    generateBtn.addListener(this); exportBtn.addListener(this);
                }

                void resized() override
                {
                    auto r = getLocalBounds().reduced(10);
                    auto top = r.removeFromTop(140);
                    energy.setBounds(top.removeFromLeft(300));
                    generateBtn.setBounds(top.removeFromRight(160).reduced(10));
                    exportBtn.setBounds(r.removeFromBottom(40).removeFromRight(160).reduced(10));
                    variantsTabs.setBounds(r.reduced(4));
                }

                void buttonClicked(juce::Button* b) override
                {
                    if (b == &generateBtn) onGenerate();
                    if (b == &exportBtn) onExport();
                }

            private:
                void onGenerate()
                {
                    bang::GenParams P;
                    P.bars = proc.apvts.getRawParameterValue("bars")->load();
                    P.variants = proc.apvts.getRawParameterValue("variants")->load();
                    P.aiInfluence = proc.apvts.getRawParameterValue("ai")->load();
                    P.tension = proc.apvts.getRawParameterValue("tension")->load();
                    P.motifVar = proc.apvts.getRawParameterValue("motifVar")->load();
                    P.liftBar = proc.apvts.getRawParameterValue("liftBar")->load();
                    P.bpm = (int)proc.apvts.getRawParameterValue("bpm")->load();
                    P.seed = (int)proc.apvts.getRawParameterValue("seed")->load();
                    energy.getValuesInto(P.energyPerBar);

                    lastOutputs = bang::LoopSessionEngine::generateLoopSet(P);
                    variantsTabs.setNumVariants((int)lastOutputs.size());
                }

                void onExport()
                {
                    if (lastOutputs.empty()) { onGenerate(); }
                    juce::FileChooser fc("Choose export folder", juce::File::getSpecialLocation(juce::File::userDesktopDirectory), "*");
                    if (fc.browseForDirectory())
                        bang::MidiExporter::writeVariantsToFolder(fc.getResult(), lastOutputs, 480,
                            (int)proc.apvts.getRawParameterValue("bpm")->load(), 4, 4);
                }

                BangAudioProcessor& proc;
                EnergyGridComponent energy;
                VariantTabsComponent variantsTabs;
                juce::TextButton generateBtn, exportBtn;
                std::vector<bang::LoopOutput> lastOutputs;
            };

            inline juce::AudioProcessorEditor* BangAudioProcessor::createEditor() { return new BangAudioProcessorEditor(*this); }
        }

        // ================== END OF STARTER ==================
