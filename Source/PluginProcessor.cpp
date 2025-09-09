#include "PluginProcessor.h"
#include "PluginEditor.h"

BANGAudioProcessor::BANGAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output",
        juce::AudioChannelSet::stereo(), true))
{
    // You can set up default generator values here if you want
    generator.setBars(8);
}

BANGAudioProcessor::~BANGAudioProcessor() {}

void BANGAudioProcessor::prepareToPlay(double, int) {}
void BANGAudioProcessor::releaseResources() {}

bool BANGAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void BANGAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();
    juce::ignoreUnused(midiMessages);
    // We don’t do audio processing; MIDI is generated in the editor
}

juce::AudioProcessorEditor* BANGAudioProcessor::createEditor()
{
    return new BANGAudioProcessorEditor(*this);
}

bool BANGAudioProcessor::hasEditor() const { return true; }
const juce::String BANGAudioProcessor::getName() const { return "BANG"; }

bool BANGAudioProcessor::acceptsMidi() const { return true; }
bool BANGAudioProcessor::producesMidi() const { return true; }
bool BANGAudioProcessor::isMidiEffect() const { return false; }
double BANGAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int BANGAudioProcessor::getNumPrograms() { return 1; }
int BANGAudioProcessor::getCurrentProgram() { return 0; }
void BANGAudioProcessor::setCurrentProgram(int) {}
const juce::String BANGAudioProcessor::getProgramName(int) { return {}; }
void BANGAudioProcessor::changeProgramName(int, const juce::String&) {}

void BANGAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Save parameters here if needed
    juce::ignoreUnused(destData);
}

void BANGAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Restore parameters here if needed
    juce::ignoreUnused(data, sizeInBytes);
}
