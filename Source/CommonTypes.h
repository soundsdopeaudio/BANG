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
struct AdvancedHarmonySettings
{
    // Extensions / Other (probabilistic via density)
    bool enableExt7 = true;
    bool enableExt9 = false;
    bool enableExt11 = false;
    bool enableExt13 = false;
    bool enableSus24 = false;   // single toggle for Sus2/4 variants
    bool enableAltChords = false;   // altered dominant variants
    bool enableSlashChords = false;   // all slash forms

    // 0..1 density controlling the probability Extensions/Other are applied
    float extensionDensity01 = 0.25f;

    // Advanced chords (if any ON, include exactly ONE per progression)
    bool enableSecondaryDominants = true;
    bool enableBorrowed = false; // modal interchange umbrella
    bool enableChromaticMediants = false;
    bool enableNeapolitan = false;
    bool enableTritoneSub = false;
};