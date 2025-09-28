#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include "PatternDB.h"
#include "CommonTypes.h"

// Forward-declare to avoid dragging the UI header into every translation unit.
struct AdvancedHarmonyOptions;



// ===== Note (single definition) ============================================
struct Note
{
    int    pitch = 60;     // MIDI 0..127
    int    velocity = 96;     // 1..127
    double startBeats = 0.0;    // start position in beats
    double lengthBeats = 1.0;    // duration in beats
    bool   isOrnament = false;  // for drawing / humanize
};

struct Scale
{
    const char* name;
    std::vector<int> intervals; // semitone offsets from 0 (root)
};

// High-level contour for melody shaping
enum class ContourShape { Ascending, Descending, Arch, InvertedArch, Wave, Static, Terraced };

class MidiGenerator
{
public:
    MidiGenerator();

    enum class EngineMode { Chords, Mixture, Melody };

    void setEngineMode(EngineMode m);

    // ===== Scales / Key / Meter ============================================
    void setScaleIndex(int idx);
    void setScale(const juce::String& name);
    void setKey(int semitonesFromC);
    void setKeyIndex(int idx);
    void setTimeSignature(int beats, int unit);
    void setBars(int bars);
    void setRestDensity(double v) noexcept { restDensity = (float)juce::jlimit(0.0, 1.0, v); }
    void setNoteDensity(double v) noexcept { noteDensity = (float)juce::jlimit(0.0, 1.0, v); }
    void enableStyleAwareTiming(bool enable) noexcept { styleAwareTiming = enable; }
    void setStyleTimingAmount(float amt) noexcept { styleTimingAmount = juce::jlimit(0.0f, 1.0f, amt); }
    void setHumanizeTiming(float amt);   // micro-timing randomness
    void setHumanizeVelocity(float amt);  // velocity randomness
    void setSwingAmount(float amt);   // swing intensity
    void setFeelAmount(float amt);   // groove/feel blend

    void allScales (int);
    void kAllScales(int);

    void setTessitura(int low, int high) noexcept { tessituraLow = low; tessituraHigh = high; }
    void setContourShape(ContourShape s) noexcept { contourShape = s; }

    enum class PolyrhythmMode { None, Ratio3_2, Ratio4_3, Ratio5_4, Ratio7_4, Ratio2_3, };
    void setPolyrhythmMode(PolyrhythmMode m) noexcept { polyMode = m; }
    void setPolyrhythmAmount(float amt01);

    // Advanced harmony (owned elsewhere)
    void setAdvancedHarmonyOptions(AdvancedHarmonyOptions* opts);

    // Determinism
    void setSeed(int newSeed) noexcept { seed = newSeed; rng.seed((uint32_t)newSeed); }

    const std::vector<Note>& getNotes() const;

    // ===== Generate =========================================================
    std::vector<Note> generateMelody();
    std::vector<Note> generateChordTrack();
    std::vector<Note> generateChords();
    struct MixBundle { std::vector<Note> melody; std::vector<Note> chords; };
    MixBundle generateMelodyAndChords(bool avoidOverlaps = true);

    // --- BEGIN: getters required by MidiGenerator.cpp and PluginEditor.cpp ---
    int getBars() const;
    int getTSNumerator() const;
    int getTSDenominator() const;
    int getKeySemitone() const;
    int getScaleIndex() const;

    float getHumanizeTiming() const;
    float getHumanizeVelocity() const;
    float getSwingAmount() const;
    float getFeelAmount() const;
    float getRestDensity() const;

    // Return the Scale object by 0-based index (you already have kScales somewhere

    struct Scale
    {
        const char* name;
        std::vector<int> intervals; // semitone offsets from 0 (root)
    };

    // Returns the canonical list of all scales used by the plugin.
    static const std::vector<Scale>& allScales();

    // Safe accessor by index (clamped)
    static const Scale& getScaleByIndex(int index);

    // Random [0,1) helper used in generation code
    double rand01();
    // --- END: getters required by MidiGenerator.cpp and PluginEditor.cpp ---


private:

    std::vector<Note> genNotes(int count, int basePitch, double beatStart, double stepBeats, double lenBeats);

    // ===== Config / State ===================================================
    int keyRoot = 60;                   // absolute tonic midi
    juce::String currentScaleName = "Major";
    juce::String scaleName_ = "Major";
    std::vector<int> currentScale { 0, 2, 4, 5, 7, 9, 11 };

    int tsBeats = 4, tsNoteValue = 4;
    int totalBars = 8;

    float restDensity = 0.15f;
    float noteDensity = 0.55f;

    // --- BEGIN: state fields used by the getters ---
    int bars_{ 4 };
    int tsNum_{ 4 };
    int tsDen_{ 4 };
    int keySemitone_{ 0 }; // C=0, C#=1, ... B=11
    int scaleIndex_ = 0 ;

    std::uniform_real_distribution<double> dist01_ { 0.0, 1.0 };
    // --- END: state fields used by the getters ---

    // Randomness
    int seed = 0;
    std::mt19937 rng;

    // Databases
    RhythmPatternDB rhythmDB;
    MovementDB      movementDB;

    void applyExtensionsAndOthers(std::vector<Note>& triad, int chordRootMidi);
    void applyAdvancedChordFamilies(std::vector<std::vector<Note>>& progression,
        const std::vector<int>& chordRootsMidi);

    // Musical shaping
    int tessituraLow = 48, tessituraHigh = 79; // C3..G5-ish
    ContourShape contourShape = ContourShape::Static;

    // Polyrhythm / feel
    PolyrhythmMode polyMode = PolyrhythmMode::Ratio3_2;
    float polyAmount = 0.0f;

    AdvancedHarmonyOptions* advOpts_ = nullptr;

    std::mt19937
        rng_{ 0xBADC0DEu };

    std::vector<Note> lastOut_;

    bool  styleAwareTiming = true;
    float styleTimingAmount = 0.5f;

    // Chord track options
    enum class ChordExtensions { Triad = 3, Seventh = 4, Ninth = 5, Eleventh = 6 };
    enum class ChordCompStyle { Block, HalfNotes, ArpUp, ArpDown, Alberti, Anticipation };
    ChordExtensions chordExt = ChordExtensions::Eleventh;
    ChordCompStyle  chordStyle = ChordCompStyle::Block;
    float chordActivity = 0.4f;
    int   chordLow = 48, chordHigh = 76;

    EngineMode engineMode_ = EngineMode::Mixture;

    float humanizeTiming_ = 0.0f;  // 0..1
    float humanizeVelocity_ = 0.0f;  // 0..1
    float swingAmount_ = 0.0f;  // 0..1
    float feelAmount_ = 0.0f;  // 0..1
    float restDensity_ = 0.0f;

    PolyrhythmMode polyrMode_ = PolyrhythmMode::None;
    float          polyrAmt01_ = 0.0f; // 0..1

    const AdvancedHarmonyOptions* advOpts = nullptr;

    // ===== Helpers ==========================================================
    int clampPitch(int midi) const
    {
        return juce::jlimit(tessituraLow, tessituraHigh, midi);
    }
    void applyAdvancedHarmonyToChordNotes(std::vector<Note>& chordNotes) const;

    // Rhythm helpers
    std::vector<RhythmStep> expandPatternWithPolyrhythm(const RhythmPattern& pat, double barStartBeats) const;

    // Utility
    double totalBeats() const { return (double)tsBeats * (double)totalBars; }

  
};
