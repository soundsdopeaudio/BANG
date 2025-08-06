class BANGAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    BANGAudioProcessorEditor (BANGAudioProcessor&);
    ~BANGAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    BANGAudioProcessor& processor;

    juce::TextButton chordsButton {"CHORDS"};
    juce::TextButton melodyButton {"MELODY"};
    juce::TextButton mixtureButton {"MIXTURE"};
    juce::TextButton surpriseButton {"SURPRISE ME"};

    void setEngine(EngineType engine);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BANGAudioProcessorEditor)
};
