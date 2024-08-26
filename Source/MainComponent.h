#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent : public juce::Component,
                      public juce::Button::Listener,
                      public juce::Slider::Listener
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
    double sampleRate = 48000.0;
    void loadAudioFile(const juce::File &file);
    void applyFrequencyBandGains(juce::AudioBuffer<float> &buffer);
    void removeVocals(juce::AudioBuffer<float> &buffer);

    void processAudio();
    void bandpassFilter(std::vector<float> &signal, double sampleRate, double lowcut, double highcut);
    void normalizeBass(juce::AudioBuffer<float> &buffer, float targetLevel);
    void createSpectrogram();
    void inverseFFT();
    void exportAudio();
    void matrixTranspose(std::vector<std::vector<float>>& matrix);
    void isolateVocals();
    void isolateVocalsUsingStereoSeperation();

    bool isTransient(const float *data, int index);

    void applyWindow(float *data, int size);
    void sliderValueChanged(juce::Slider *slider);
    void isolateVocals(std::vector<float> &magnitudes, int fftSize, float sampleRate);

    juce::TextButton loadButton;
    juce::TextButton exportButton;
    juce::TextButton isolateButton;
    juce::Slider band1Knob;
    juce::Slider band2Knob;
    juce::Slider band3Knob;
    juce::Label band1Label;
    juce::Label band2Label;
    juce::Label band3Label;

    // In your MainComponent.h file, add this to the private section:
    std::unique_ptr<juce::FileChooser> exportFileChooser;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioSampleBuffer audioBuffer;
    juce::dsp::FFT fft;
    std::vector<float> fftData;
    std::vector<std::vector<float>> amplitudeEnvelope;
    juce::Image spectrogramImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
