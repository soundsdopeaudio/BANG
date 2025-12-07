#include "MidiExporter.h"
#include <juce_core/juce_core.h>

// Helper to write a MixBundle to disk using the single exporter implementation.
// Ensures there's only one definition of exportMixBundleToMidiFile in the project.
bool saveMixBundleAsMidFile(const MidiGenerator::MixBundle& bundle, const juce::File& file)
{
    juce::MidiFile midi;
    exportMixBundleToMidiFile(bundle, midi, 960, 1); // call the single canonical exporter

    std::unique_ptr<juce::FileOutputStream> outStream(file.createOutputStream());
    if (!outStream) return false;

    midi.writeTo(*outStream);
    outStream->flush();
    return true;
}