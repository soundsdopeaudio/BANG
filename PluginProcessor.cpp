#include "PluginProcessor.h"
#include "PluginEditor.h"

BANGAudioProcessor::BANGAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
#endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
                       )
#endif
{
    currentEngine = EngineType::Chords;
}

BANGAudioProcessor::~BANGAudioProcessor() {}

void BANGAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Prepare anything here before playback starts
}

void BANGAudioProcessor::releaseResources()
{
    // Release resources here
}

bool BANGAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // Accept only stereo output
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}

void BANGAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (buffer);

    // Clear the buffer (no audio processing)
    buffer.clear();

    midiMessages.clear();

    switch (currentEngine)
    {
        case EngineType::Chords:
        {
            auto chords = midiGenerator.generateChordProgression(selectedKey, selectedScale, bars, restDensity);
            midiGenerator.addChordsToMidiBuffer(chords, midiMessages);
            break;
        }
        case EngineType::Melody:
        {
            auto melody = midiGenerator.generateMelody(selectedKey, selectedScale, bars, restDensity);
            midiGenerator.addMelodyToMidiBuffer(melody, midiMessages);
            break;
        }
        case EngineType::Mixture:
        {
            auto mixture = midiGenerator.generateMixture(selectedKey, selectedScale, bars, restDensity);
            midiGenerator.addMixtureToMidiBuffer(mixture, midiMessages);
            break;
        }
        case EngineType::SurpriseMe:
        {
            // Randomly pick an engine and generate
            auto randomEngine = static_cast<EngineType>(juce::Random::getSystemRandom().nextInt({0,2}));
            currentEngine = randomEngine;
            processBlock(buffer, midiMessages); // recurse with the chosen engine
            break;
        }
    }
}

bool BANGAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* BANGAudioProcessor::createEditor()
{
    return new BANGAudioProcessorEditor (*this);
}

const juce::String BANGAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool BANGAudioProcessor::acceptsMidi() const
{
    return true;
}

bool BANGAudioProcessor::producesMidi() const
{
    return true;
}

bool BANGAudioProcessor::isMidiEffect() const
{
    return true;
}

double BANGAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int BANGAudioProcessor::getNumPrograms()
{
    return 1;
}

int BANGAudioProcessor::getCurrentProgram()
{
    return 0;
}

void BANGAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused(index);
}

const juce::String BANGAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused(index);
    return {};
}

void BANGAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void BANGAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You can save plugin state here
    juce::ignoreUnused(destData);
}

void BANGAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You can load plugin state here
    juce::ignoreUnused(data, sizeInBytes);
}

void BANGAudioProcessor::setEngineType(EngineType newEngine)
{
    currentEngine = newEngine;
}

