#pragma once

#include <JuceHeader.h>
#include <vector>
#include <string>

// Enum for engine types
enum class EngineType
{
    Chords,
    Melody,
    Mixture,
    SurpriseMe
};

// Note structure for melody
struct Note
{
    int midiNoteNumber;
    float startBeat;   // start position in beats
    float duration;    // duration in beats
    bool isRest;
};

// Chord structure
struct Chord
{
    std::string name;
    std::vector<int> notes; // semitone offsets from root (e.g., {0, 4, 7} for major triad)
    int rootNote;           // MIDI note number of the root
    int bar;                // which bar in the progression
    int beat;               // which beat
    bool isRest;            // true = silence, for rest density logic
};

class BANGAudioProcessor : public juce::AudioProcessor
{
public:
    BANGAudioProcessor();
    ~BANGAudioProcessor() override;

    // Basic AudioProcessor overrides
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Engine setter/getter
    void setEngineType(EngineType newEngine);
    EngineType getEngineType() const;

    // Generation methods
    std::vector<Chord> generateChordProgression();
    std::vector<Note> generateMelody();
    std::vector<Note> generateMixture();

private:
    EngineType currentEngine = EngineType::Chords;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BANGAudioProcessor)
};
