#pragma once
#include "MidiGenerator.h"
#include <juce_audio_basics/juce_audio_basics.h>

// Defaults only on the declaration.
void exportMixBundleToMidiFile(const MidiGenerator::MixBundle& bundle,
    juce::MidiFile& outFile,
    int ppq = 960,
    int midiChannel = 1);
