#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent : public juce::Component,
                      public juce::Button::Listener
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;
    void buttonClicked(juce::Button *button) override;

private:
    //==============================================================================
    // Your private member variables go here...
    void loadAudioFile(const juce::File &file);
    void processAudio();
    void createSpectrogram();
    void inverseFFT();
    void exportAudio();

    juce::TextButton loadButton;
    juce::TextButton exportButton;
    // In your MainComponent.h file, add this to the private section:
    std::unique_ptr<juce::FileChooser> exportFileChooser;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioSampleBuffer audioBuffer;
    juce::dsp::FFT fft;
    std::vector<float> fftData;
    juce::Image spectrogramImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
