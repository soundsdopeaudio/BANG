#pragma once
#include <JuceHeader.h>
#include "MidiGenerator.h"   // <-- include the generator
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include "CommonTypes.h"

struct AdvancedHarmonySettings;

class BANGAudioProcessor : public juce::AudioProcessor
{
public:
    BANGAudioProcessor();
    ~BANGAudioProcessor() override;

    juce::UndoManager& getUndoManager() noexcept { return undoManager; }
    juce::ValueTree& getStateTree() noexcept { return state; }

    // --- Octave parameter IDs (keep with your other param IDs)
    static constexpr const char* PARAM_OCTAVE_BASE_ID = "octaveBase";
    static constexpr const char* PARAM_OCTAVE_BASE_NAME = "Octave";

    // --- Octave choice (C1..C5) controlled by the main-page combo box ---
    void setOctaveChoiceIndex(int idx) noexcept;   // expects 0..4
    int  getOctaveChoiceIndex() const noexcept;    // returns 0..4
    int  getOctaveShiftSemitones() const noexcept; // -24..+24 relative to C3

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

    AdvancedHarmonySettings getAdvancedHarmonySettings() const;

    AdvancedHarmonyOptions* getAdvancedOptionsPtr() noexcept { return &advancedOptsShadow; }

    void refreshAdvancedOptionsFromApvts();

    juce::AudioProcessorValueTreeState apvts{
        *this, nullptr, "PARAMS", createParameterLayout()

    };

private:

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Shadow copy of the APVTS-backed advanced harmony options
    AdvancedHarmonyOptions advancedOptsShadow{};

    // Pull current APVTS values into advancedOptsShadow


    MidiGenerator generator;  // 👈 The actual generator instance

    std::atomic<int> octaveChoiceIndex { 2 }; // default = C3 (index 2)

    juce::UndoManager undoManager;
    juce::ValueTree   state { "BangState" };  // root of your plugin state

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BANGAudioProcessor)
};
