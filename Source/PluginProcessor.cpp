// PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"


namespace
{
    // Shift all note-on/off by semitones, clamped to 0..127
    inline void applyGlobalOctaveShiftToBuffer(juce::MidiBuffer& buffer, int semitoneShift)
    {
        if (semitoneShift == 0) return;

        juce::MidiBuffer shifted;
        for (const auto meta : buffer)
        {
            const auto& m = meta.getMessage();
            if (m.isNoteOnOrOff())
            {
                const int newNote = juce::jlimit(0, 127, m.getNoteNumber() + semitoneShift);
                juce::MidiMessage copy = m;
                copy.setNoteNumber(newNote);
                shifted.addEvent(copy, meta.samplePosition);
            }
            else
            {
                shifted.addEvent(m, meta.samplePosition);
            }
        }
        buffer.swapWith(shifted);
    }
}

BANGAudioProcessor::BANGAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    if (!state.isValid())
        state = juce::ValueTree("BangState");

    // Set up any defaults you need for your generator
    generator.setBars(8);
}

BANGAudioProcessor::~BANGAudioProcessor() = default;

// ===== lifecycle =====
void BANGAudioProcessor::prepareToPlay(double, int) {}
void BANGAudioProcessor::releaseResources() {}

bool BANGAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

// ===== audio/midi =====
void BANGAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    // Global octave shift, driven by the main-page octaveBox
    const int semis = getOctaveShiftSemitones();
    applyGlobalOctaveShiftToBuffer(midiMessages, semis);
}

// ===== editor =====
juce::AudioProcessorEditor* BANGAudioProcessor::createEditor() { return new BANGAudioProcessorEditor(*this); }
bool BANGAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorValueTreeState::ParameterLayout BANGAudioProcessor::createParameterLayout()
{
    using APB = juce::AudioParameterBool;
    using APF = juce::AudioParameterFloat;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // === Extensions / Other =============================================
    params.push_back(std::make_unique<APB>("ext7", "7ths", true));
    params.push_back(std::make_unique<APB>("ext9", "9ths", false));
    params.push_back(std::make_unique<APB>("ext11", "11ths", false));
    params.push_back(std::make_unique<APB>("ext13", "13ths", false));
    params.push_back(std::make_unique<APB>("sus24", "Sus2/4", false));
    params.push_back(std::make_unique<APB>("alt", "Alt Chords", false));
    params.push_back(std::make_unique<APB>("slash", "Slash Chords", false));

    params.push_back(std::make_unique<APF>(
        "extDensity", "Extensions Density",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 25.0f,
        juce::String(), juce::AudioProcessorParameter::genericParameter,
        [](float v, int) { return juce::String(juce::roundToInt(v)) + "%"; },
        [](const juce::String& t) { return t.getFloatValue(); }
    ));

    // === Advanced (exactly ONE if any are enabled) ======================
    params.push_back(std::make_unique<APB>("advSecDom", "Secondary Dominants", true));
    params.push_back(std::make_unique<APB>("advBorrowed", "Borrowed Chords", false));
    params.push_back(std::make_unique<APB>("advChromMed", "Chromatic Mediants", false));
    params.push_back(std::make_unique<APB>("advNeapolitan", "Neapolitan Chords", false));
    params.push_back(std::make_unique<APB>("advTritone", "Tritone Substitutions", false));

    return { params.begin(), params.end() };
}

// ===== meta =====
const juce::String BANGAudioProcessor::getName() const { return "BANG"; }
bool BANGAudioProcessor::acceptsMidi() const { return true; }
bool BANGAudioProcessor::producesMidi() const { return true; }
bool BANGAudioProcessor::isMidiEffect() const { return false; }
double BANGAudioProcessor::getTailLengthSeconds() const { return 0.0; }

// ===== programs (unused) =====
int  BANGAudioProcessor::getNumPrograms() { return 1; }
int  BANGAudioProcessor::getCurrentProgram() { return 0; }
void BANGAudioProcessor::setCurrentProgram(int) {}
const juce::String BANGAudioProcessor::getProgramName(int) { return {}; }
void BANGAudioProcessor::changeProgramName(int, const juce::String&) {}

// ===== state =====
void BANGAudioProcessor::getStateInformation(juce::MemoryBlock& destData) { juce::ignoreUnused(destData); }
void BANGAudioProcessor::setStateInformation(const void* data, int sizeInBytes) { juce::ignoreUnused(data, sizeInBytes); }

// ===== octave API =====
void BANGAudioProcessor::setOctaveChoiceIndex(int idx) noexcept
{
    // expected 0..4 for C1..C5
    octaveChoiceIndex.store(juce::jlimit(0, 4, idx), std::memory_order_relaxed);
}

int BANGAudioProcessor::getOctaveChoiceIndex() const noexcept
{
    return octaveChoiceIndex.load(std::memory_order_relaxed);
}

int BANGAudioProcessor::getOctaveShiftSemitones() const noexcept
{
    // 0=C1,1=C2,2=C3,3=C4,4=C5 relative to C3 (C3=0)
    static constexpr int table[5] = { -24, -12, 0, +12, +24 };
    const int idx = juce::jlimit(0, 4, getOctaveChoiceIndex());
    return table[idx];
}