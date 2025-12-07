#pragma once

#include <vector>
#include <string>

// Chord structure
struct Chord {
    std::string name;
    std::vector<int> notes; // semitone offsets from root (e.g., {0,4,7} for major triad)
    int rootNote;       // MIDI note number of root
    int bar;            // bar number in progression
    int beat;           // beat number in bar
    bool isRest;        // true if rest
};

// Enum for engine types
enum class EngineType {
    Chords,
    Melody,
    Mixture,
    SurpriseMe
};

// === AdvancedHarmonySettings (FINAL) =====================================
// ===== Advanced Harmony options (shared data model) ==========================
struct AdvancedHarmonyOptions
{
    // Extensions / other chords
    bool  enableExt7 = false;
    bool  enableExt9 = false;
    bool  enableExt11 = false;
    bool  enableExt13 = false;
    bool  enableSus24 = false;      // includes sus2 / sus4 / 7sus4 variants, etc
    bool  enableAltChords = false;  // altered chords
    bool  enableSlashChords = false;

    // 0..1 amount controlling how often to inject ext/other chords
    float extensionDensity01 = 0.0f;

    // Advanced chords
    bool enableSecondaryDominants = false;
    bool enableBorrowed = false;
    bool enableChromaticMediants = false;
    bool enableNeapolitan = false;
    bool enableTritoneSub = false;
};
