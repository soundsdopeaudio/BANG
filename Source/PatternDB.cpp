#include "PatternDB.h"
#include <initializer_list>

static RhythmPattern makePattern(const juce::String& n, int bars, int bpb,
    std::initializer_list<RhythmStep> s, float w)
{
    RhythmPattern p;
    p.name = n; p.bars = bars; p.beatsPerBar = bpb; p.steps = s; p.weight = w;
    return p;
}

RhythmPatternDB makeDefaultRhythms()
{
    RhythmPatternDB db;

    // -------- 2/4 --------
    db.patterns.push_back(makePattern("2/4 march 8ths", 1, 2, {
        {0.0,0.5,false,0.8f},{0.5,0.5,false,0.6f},
        {1.0,0.5,false,0.7f},{1.5,0.5,false,0.9f},
        }, 1.0f));

    db.patterns.push_back(makePattern("2/4 sync push", 2, 2, {
        {0.0,0.5,false,0.8f},{0.5,0.5,true,0.0f},
        {1.0,0.5,false,0.7f},{1.5,0.5,false,0.9f},
        {2.0,0.5,true,0.0f},{2.5,0.5,false,0.8f},
        {3.0,0.5,false,0.7f},{3.5,0.5,false,1.0f},
        }, 0.9f));

    // -------- 3/4 --------
    db.patterns.push_back(makePattern("3/4 waltz simple", 1, 3, {
        {0.0,0.5,false,0.8f},{0.5,0.5,false,0.6f},{1.0,0.5,false,0.8f},
        {1.5,0.5,false,0.6f},{2.0,0.5,false,0.8f},{2.5,0.5,false,0.9f},
        }, 0.9f));

    db.patterns.push_back(makePattern("3/4 offbeat lilt", 2, 3, {
        {0.0,0.5,false,0.8f},{0.5,0.5,true,0.0f},{1.0,0.5,false,0.7f},
        {1.5,0.5,false,0.7f},{2.0,0.5,false,0.8f},{2.5,0.5,false,0.9f},
        {3.0,0.5,false,0.8f},{3.5,0.5,true,0.0f},{4.0,0.5,false,0.7f},
        {4.5,0.5,false,0.7f},{5.0,0.5,false,0.8f},{5.5,0.5,false,1.0f},
        }, 0.85f));

    // -------- 4/4 --------
    db.patterns.push_back(makePattern("4/4 8ths straight", 1, 4, {
        {0.0,0.5,false,0.7f},{0.5,0.5,false,0.5f},
        {1.0,0.5,false,0.7f},{1.5,0.5,false,0.5f},
        {2.0,0.5,false,0.7f},{2.5,0.5,false,0.5f},
        {3.0,0.5,false,0.7f},{3.5,0.5,false,0.8f},
        }, 1.0f));

    db.patterns.push_back(makePattern("4/4 offbeat syncop", 1, 4, {
        {0.0,0.25,true,0.0f},
        {0.5,0.5,false,0.9f},
        {1.5,0.5,false,0.7f},
        {2.0,0.25,false,0.4f},
        {2.5,0.5,false,0.7f},
        {3.5,0.5,false,0.9f},
        }, 1.0f));

    // -------- 5/4 --------
    db.patterns.push_back(makePattern("5/4 pulse long-short", 1, 5, {
        {0.0,1.0,false,0.8f},
        {1.0,0.5,false,0.6f},{1.5,0.5,false,0.6f},
        {2.0,0.5,false,0.7f},{2.5,0.5,false,0.7f},
        {3.0,0.5,false,0.8f},{3.5,0.5,false,0.7f},
        {4.0,1.0,false,0.9f},
        }, 0.8f));

    db.patterns.push_back(makePattern("5/4 sync spread", 2, 5, {
        {0.0,0.5,false,0.8f},{0.5,0.5,false,0.6f},{1.0,0.5,false,0.7f},
        {1.5,0.5,false,0.7f},{2.0,0.5,false,0.8f},{2.5,0.5,false,0.6f},
        {3.0,0.5,false,0.7f},{3.5,0.5,false,0.7f},{4.0,0.5,false,0.9f},
        {5.0,0.5,false,0.8f},{5.5,0.5,false,0.6f},{6.0,0.5,false,0.7f},
        {6.5,0.5,false,0.7f},{7.0,0.5,false,0.8f},{7.5,0.5,false,1.0f},
        {8.0,0.5,false,0.8f},{8.5,0.5,false,0.7f},{9.0,0.5,false,0.9f},
        }, 0.75f));

    // -------- 5/8 --------
    db.patterns.push_back(makePattern("5/8 Balkan feel", 1, 5, {
        {0.0,0.5,false,0.8f},{0.5,0.5,false,0.7f},
        {1.0,0.5,false,0.8f},{1.5,0.5,false,0.7f},
        {2.0,1.0,false,0.9f},
        }, 0.7f));

    // -------- 7/8 --------
    db.patterns.push_back(makePattern("7/8 long-short-short", 1, 7, {
        {0.0,1.0,false,0.8f},{1.0,0.5,false,0.6f},{1.5,0.5,false,0.7f},
        {2.0,1.0,false,0.8f},{3.0,0.5,false,0.6f},{3.5,0.5,false,0.7f},
        {4.0,1.0,false,0.9f},
        }, 0.7f));

    // -------- 7/4 --------
    db.patterns.push_back(makePattern("7/4 driving", 1, 7, {
        {0.0,0.5,false,0.8f},{0.5,0.5,false,0.6f},{1.0,0.5,false,0.7f},
        {2.0,0.5,false,0.8f},{3.0,0.5,false,0.6f},{4.0,0.5,false,0.7f},
        {5.0,0.5,false,0.8f},{6.0,0.5,false,0.9f},
        }, 0.6f));

    // -------- 9/8 --------
    db.patterns.push_back(makePattern("9/8 compound", 1, 9, {
        {0.0,0.5,false,0.8f},{0.5,0.5,false,0.6f},{1.0,0.5,false,0.7f},
        {2.0,0.5,false,0.7f},{3.0,0.5,false,0.6f},{4.0,0.5,false,0.7f},
        {5.0,0.5,false,0.8f},{6.0,0.5,false,0.6f},{7.0,0.5,false,0.9f},
        }, 0.6f));

    // -------- 12/8 --------
    db.patterns.push_back(makePattern("12/8 shuffle", 1, 12, {
        {0.0,0.75,false,0.8f},{0.75,0.25,true,0.0f},{1.0,0.75,false,0.7f},{1.75,0.25,true,0.0f},
        {2.0,0.75,false,0.8f},{2.75,0.25,true,0.0f},{3.0,0.75,false,0.7f},{3.75,0.25,true,0.0f},
        {4.0,0.75,false,0.8f},{4.75,0.25,true,0.0f},{5.0,0.75,false,0.7f},{5.75,0.25,true,0.0f},
        }, 0.65f));

    // -------- 11/8 --------
    db.patterns.push_back(makePattern("11/8 3+3+3+2", 1, 11, {
        {0.0,0.75,false,0.9f},{0.75,0.25,true,0.0f},{1.0,0.75,false,0.7f},{1.75,0.25,true,0.0f},
        {2.0,0.75,false,0.7f},{2.75,0.25,true,0.0f},{3.0,0.5,false,0.9f},{3.5,0.5,false,1.0f},
        }, 0.65f));

    // -------- 13/8 --------
    db.patterns.push_back(makePattern("13/8 airy", 1, 13, {
        {0.0,0.5,false,0.8f},{0.5,0.5,false,0.6f},{1.0,0.75,false,0.75f},{1.75,0.25,true,0.0f},
        {2.0,0.75,false,0.8f},{2.75,0.25,true,0.0f},{3.0,0.75,false,0.85f},{3.75,0.25,true,0.0f},
        }, 0.65f));

    return db;
}

MovementDB makeDefaultMovements()
{
    MovementDB db; using MT = MoveType;
    db.moves = {
        { MT::ChordTone,      2.0f,  0 },
        { MT::ChordTone,      1.5f,  0 },
        { MT::ScaleStepUp,    1.4f, +2 }, { MT::ScaleStepDown, 1.4f, -2 },
        { MT::NeighborUp,     1.0f, +1 }, { MT::NeighborDown,  1.0f, -1 },
        { MT::Enclosure,      0.8f,  0 },
        { MT::Leap,           0.6f, +7 }, { MT::Leap, 0.5f, -5 }, { MT::Leap, 0.4f, +12 }, { MT::Leap, 0.3f, -12 },
        { MT::EscapeToneUp,   0.5f, +3 }, { MT::EscapeToneDown, 0.5f, -3 },
        { MT::DoubleNeighbor, 0.6f,  0 },
        { MT::ResolveDown,    1.1f, -2 }, { MT::ResolveDown, 0.9f, -1 },

        // Ornaments (used by MidiGenerator::emitOrnament)
        { MT::Trill,          0.35f, +1 },
        { MT::Turn,           0.30f,  0 },
        { MT::MordentUp,      0.28f, +1 }, { MT::MordentDown, 0.28f, -1 },
        { MT::GraceUp,        0.40f, +1 }, { MT::GraceDown,  0.40f, -1 },
    };
    return db;
}
