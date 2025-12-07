#include "MidiExporter.h"
#include "MidiGenerator.h"
#include <objbase.h>
#pragma comment(lib, "ole32.lib")
#include <cmath>
#include <algorithm>

// Convert MixBundle.chords -> MidiFile using PPQ ticks.
// Caller is responsible for writing the MidiFile to disk or returning it for drag/drop.
void exportMixBundleToMidiFile(const MidiGenerator::MixBundle& bundle,
    juce::MidiFile& outFile,
    int ppq,
    int midiChannel)
{
    juce::MidiMessageSequence seq;
    seq.clear();

    for (const auto& n : bundle.chords)
    {
        const int onTick  = (int)std::llround(n.startBeats * (double)ppq);
        const int lenTick = std::max(1, (int)std::llround(n.lengthBeats * (double)ppq)); // >=1 tick
        const int offTick = onTick + lenTick;

        juce::MidiMessage on  = juce::MidiMessage::noteOn (midiChannel, n.pitch, (juce::uint8)n.velocity);
        juce::MidiMessage off = juce::MidiMessage::noteOff(midiChannel, n.pitch);

        on .setTimeStamp ((double) onTick);
        off.setTimeStamp ((double)offTick);

        seq.addEvent(on);
        seq.addEvent(off);
    }

    seq.updateMatchedPairs();
    seq.sort();

    outFile.clear();
    outFile.setTicksPerQuarterNote(ppq);
    outFile.addTrack(seq); 
}