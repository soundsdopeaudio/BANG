#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cstdint>

// A rhythmic step (start + length in beats). If rest=true, skip note.
struct RhythmStep
{
    double startBeats = 0.0;
    double lengthBeats = 0.5;
    bool   rest = false;
    float  accent = 0.0f;
};

// Bitmask style flags (optional metadata)
enum class RhythmStyle : uint32_t {
    None = 0,
    Straight = 1u << 0,
    Syncopated = 1u << 1,
    Lyrical = 1u << 2,
    Shuffle = 1u << 3,
    Balkan = 1u << 4,
    Sixteenth = 1u << 5,
    Sparse = 1u << 6,
    Narrative = 1u << 7
};
inline constexpr uint32_t operator|(RhythmStyle a, RhythmStyle b) { return (uint32_t)a | (uint32_t)b; }

struct RhythmPattern
{
    juce::String             name;
    int                      bars = 1;
    int                      beatsPerBar = 4;
    std::vector<RhythmStep>  steps;         // absolute positions inside the pattern
    float                    weight = 1.0f; // selection weight
    uint32_t                 styleMask = (uint32_t)RhythmStyle::None;
};

// Melodic movement types (includes ornaments used by MidiGenerator)
enum class MoveType {
    ChordTone, ScaleStepUp, ScaleStepDown,
    NeighborUp, NeighborDown, Enclosure,
    Leap, ResolveDown, EscapeToneUp, EscapeToneDown, DoubleNeighbor,
    Trill, Turn, MordentUp, MordentDown, GraceUp, GraceDown
};

struct Movement
{
    MoveType type;
    float    weight;        // selection weight
    int      semitoneHint;  // e.g., Leap: +7, -5, +12, ...
};

struct RhythmPatternDB { std::vector<RhythmPattern> patterns; };
struct MovementDB { std::vector<Movement>     moves; };

// Factories
RhythmPatternDB makeDefaultRhythms();
MovementDB      makeDefaultMovements();
