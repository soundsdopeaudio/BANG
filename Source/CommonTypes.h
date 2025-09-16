#pragma once

#include <vector>
#include <string>

// Note structure
struct Note {
    int pitch;          // MIDI note number
    float startTime;    // in beats or seconds
    float duration;     // note length
    bool isRest;        // true if rest
};

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

