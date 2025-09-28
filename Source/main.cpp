#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

class BANGStandaloneApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "BANG"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    void initialise(const juce::String&) override
    {
        mainWindow.reset(new MainWindow(getApplicationName()));
    }
    void shutdown() override { mainWindow = nullptr; }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name) : DocumentWindow(name,
            juce::Colours::darkgrey,
            DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new BANGAudioProcessorEditor(processor), true);
            centreWithSize(800, 600);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        BANGAudioProcessor processor;
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(BANGStandaloneApplication)
