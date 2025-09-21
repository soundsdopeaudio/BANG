#pragma once
#include <JuceHeader.h>
#include "MidiGenerator.h"   // <-- include the generator
#include <juce_data_structures/juce_data_structures.h>

class BANGAudioProcessor : public juce::AudioProcessor
{
public:
    BANGAudioProcessor();
    ~BANGAudioProcessor() override;

    juce::UndoManager& getUndoManager() noexcept { return undoManager; }
    juce::ValueTree& getStateTree() noexcept { return state; }

    void setStatePropertyWithUndo(const juce::Identifier& key, const juce::var& newValue)
    {
        // This will create an undo transaction name like "Change key"
        undoManager.beginNewTransaction("Change " + key.toString());
        state.setProperty(key, newValue, &undoManager); // <- records undo automatically
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // 👇 Add accessors so PluginEditor can call processor.getMidiGenerator()
    MidiGenerator& getMidiGenerator()       noexcept { return generator; }
    const MidiGenerator& getMidiGenerator() const noexcept { return generator; }

private:
    MidiGenerator generator;  // 👈 The actual generator instance

    juce::UndoManager undoManager;
    juce::ValueTree   state { "BangState" };  // root of your plugin state

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BANGAudioProcessor)
};
