#include "MainComponent.h"

MainComponent::MainComponent() : fft(12) // 2^12 = 4096 points
{
    setSize(800, 600);

    addAndMakeVisible(loadButton);
    loadButton.setButtonText("Load Audio File");
    loadButton.addListener(this);

    addAndMakeVisible(exportButton);
    exportButton.setButtonText("Export Audio");
    exportButton.addListener(this);

    addAndMakeVisible(isolateButton);
    isolateButton.setButtonText("Isolate Audio");
    isolateButton.addListener(this);

    // Knobs and labels
    band1Knob.setSliderStyle(juce::Slider::Rotary);
    band1Knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    band1Knob.setRange(0.0, 1.0, 0.01);
    band1Knob.setValue(1.0);
    band1Knob.addListener(this);
    addAndMakeVisible(band1Knob);

    band2Knob.setSliderStyle(juce::Slider::Rotary);
    band2Knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    band2Knob.setRange(0.0, 1.0, 0.01);
    band2Knob.setValue(0.0);
    band2Knob.addListener(this);
    addAndMakeVisible(band2Knob);

    band3Knob.setSliderStyle(juce::Slider::Rotary);
    band3Knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
    band3Knob.setRange(0.0, 1.0, 0.01);
    band3Knob.setValue(0.0);
    band3Knob.addListener(this);
    addAndMakeVisible(band3Knob);

    band1Label.setText("Low Band", juce::dontSendNotification);
    addAndMakeVisible(band1Label);

    band2Label.setText("Mid Band", juce::dontSendNotification);
    addAndMakeVisible(band2Label);

    band3Label.setText("High Band", juce::dontSendNotification);
    addAndMakeVisible(band3Label);

    formatManager.registerBasicFormats();
}

MainComponent::~MainComponent()
{
}

void MainComponent::sliderValueChanged(juce::Slider *slider)
{
    if (slider == &band1Knob)
    {
        float band1Value = band1Knob.getValue();
        // Handle band 1 value change
    }
    else if (slider == &band2Knob)
    {
        float band2Value = band2Knob.getValue();
        // Handle band 2 value change
    }
    else if (slider == &band3Knob)
    {
        float band3Value = band3Knob.getValue();
        // Handle band 3 value change
    }
}

void MainComponent::paint(juce::Graphics &g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setFont(juce::Font(32.0f));
    g.setColour(juce::Colours::white);
    g.drawText("FFT Project", getLocalBounds(), juce::Justification::centredTop, true);

    if (spectrogramImage.isValid())
    {
        g.drawImageAt(spectrogramImage, 0, 50);
    }
}

void MainComponent::resized()
{
    loadButton.setBounds(10, 50, 150, 30);
    exportButton.setBounds(170, 50, 150, 30);
    isolateButton.setBounds(330, 50, 150, 30);
    band1Knob.setBounds(10, 350, 150, 100);
    band1Label.setBounds(85, 450, 150, 30);
    band2Knob.setBounds(170, 350, 150, 100);
    band2Label.setBounds(245, 450, 150, 30);

    band3Knob.setBounds(330, 350, 150, 100);
    band3Label.setBounds(405, 450, 150, 30);
}

void MainComponent::buttonClicked(juce::Button *button)
{
    if (button == &loadButton)
    {
        fileChooser = std::make_unique<juce::FileChooser>("Select an audio file to load...",
                                                          juce::File{},
                                                          "*.wav;*.mp3");
        auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser &fc)
                                 {
            auto file = fc.getResult();
            if (file != juce::File{})
            {
                loadAudioFile(file);
            } });
    }
    else if (button == &exportButton)
    {
        exportAudio();
    }
    else if (button == &isolateButton)
    {
        isolateVocalsUsingStereoSeperation();
    }
}

void MainComponent::loadAudioFile(const juce::File &file)
{
    auto *reader = formatManager.createReaderFor(file);
    if (reader != nullptr)
    {
        std::unique_ptr<juce::AudioFormatReaderSource> newSource(new juce::AudioFormatReaderSource(reader, true));
        readerSource.reset(newSource.release());

        auto numSamples = reader->lengthInSamples;
        if (numSamples > 0)
        {
            audioBuffer.setSize((int)reader->numChannels, (int)numSamples);
            reader->read(&audioBuffer, 0, (int)numSamples, 0, true, true);
            processAudio();
        }
        else
        {
            // Handle empty audio file
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                   "Error",
                                                   "The audio file appears to be empty.");
        }
    }
    else
    {
        // Handle file reading error
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Error",
                                               "Could not read the audio file.");
    }
}

void MainComponent::processAudio()
{
    const int fftSize = fft.getSize();
    const int numChannels = audioBuffer.getNumChannels();
    const int numSamples = audioBuffer.getNumSamples();

    fftData.resize(fftSize * 2);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        const float *channelData = audioBuffer.getReadPointer(channel);

        for (int sample = 0; sample + fftSize <= numSamples; sample += fftSize)
        {
            juce::FloatVectorOperations::clear(fftData.data(), fftSize * 2);
            juce::FloatVectorOperations::copy(fftData.data(), channelData + sample, fftSize);

            fft.performRealOnlyForwardTransform(fftData.data());

            createSpectrogram();
        }
    }

    repaint();
}

void MainComponent::createSpectrogram()
{
    const int fftSize = fft.getSize();
    const int height = 300;
    const int width = getWidth();

    if (!spectrogramImage.isValid())
        spectrogramImage = juce::Image(juce::Image::RGB, width, height, true);

    // Shift the existing image to the left
    juce::Image tempImage(juce::Image::RGB, width - 1, height, true);
    juce::Graphics tempG(tempImage);
    tempG.drawImageAt(spectrogramImage, -1, 0);

    juce::Graphics g(spectrogramImage);
    g.drawImageAt(tempImage, 0, 0);

    g.setOpacity(1.0f);
    const float maxLevel = juce::FloatVectorOperations::findMaximum(fftData.data(), fftSize / 2);

    if (maxLevel > 0.0f) // Add this check
    {
        for (int y = 0; y < height; ++y)
        {
            const int fftDataIndex = juce::jmap(y, 0, height, 0, fftSize / 2);
            const float level = juce::jmap(fftData[fftDataIndex], 0.0f, maxLevel, 0.0f, 1.0f);

            g.setColour(juce::Colour::fromHSV(1.0f - level, 1.0f, level, 1.0f));
            g.drawLine(width - 1, height - y, width, height - y);
        }
    }
}

void MainComponent::isolateVocalsUsingStereoSeperation()
{
    const int fftSize = fft.getSize();
    const int numChannels = audioBuffer.getNumChannels();
    const int numSamples = audioBuffer.getNumSamples();

    if (numChannels < 2)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Error",
                                               "Stereo audio is required for vocal isolation.");
        return;
    }

    fftData.resize(fftSize * 2);
    std::vector<float> leftChannel(fftSize);
    std::vector<float> rightChannel(fftSize);

    juce::AudioBuffer<float> processedBuffer(numChannels, numSamples);

    // Get knob values (assuming they're in range 0.0 to 1.0)
    float lowBandGain = band1Knob.getValue();
    float midBandGain = band2Knob.getValue();
    float highBandGain = band3Knob.getValue();

    // Calculate frequency bin indices
    int lowMidCutoff = (250.0f * fftSize / sampleRate);
    int midHighCutoff = (5000.0f * fftSize / sampleRate);

    for (int sample = 0; sample + fftSize <= numSamples; sample += fftSize)
    {
        // Copy data from both channels
        juce::FloatVectorOperations::copy(leftChannel.data(), audioBuffer.getReadPointer(0) + sample, fftSize);
        juce::FloatVectorOperations::copy(rightChannel.data(), audioBuffer.getReadPointer(1) + sample, fftSize);

        // Process left channel
        juce::FloatVectorOperations::clear(fftData.data(), fftSize * 2);
        juce::FloatVectorOperations::copy(fftData.data(), leftChannel.data(), fftSize);
        fft.performRealOnlyForwardTransform(fftData.data());

        std::vector<float> leftSpectrum(fftData.begin(), fftData.begin() + fftSize);

        // Process right channel
        juce::FloatVectorOperations::clear(fftData.data(), fftSize * 2);
        juce::FloatVectorOperations::copy(fftData.data(), rightChannel.data(), fftSize);
        fft.performRealOnlyForwardTransform(fftData.data());

        std::vector<float> rightSpectrum(fftData.begin(), fftData.begin() + fftSize);

        // Add left and right to get center channel (vocals) and apply band gains
        for (int i = 0; i < fftSize / 2; ++i)
        {
            float centerChannel = (leftSpectrum[i] - rightSpectrum[i]);

            if (i < lowMidCutoff)
                fftData[i] = centerChannel * lowBandGain;
            else if (i < midHighCutoff)
                fftData[i] = centerChannel * midBandGain;
            else
                fftData[i] = centerChannel * highBandGain;

            // Mirror for negative frequencies (second half of FFT)
            fftData[fftSize - i] = fftData[i];
        }

        // Inverse FFT
        fft.performRealOnlyInverseTransform(fftData.data());

        // Copy the processed data back to both channels
        for (int channel = 0; channel < numChannels; ++channel)
        {
            juce::FloatVectorOperations::copy(processedBuffer.getWritePointer(channel) + sample, fftData.data(), fftSize);
        }
    }

    // Replace the original audio buffer with the processed buffer
    audioBuffer = processedBuffer;

    createSpectrogram();
    repaint();
}

// bool MainComponent::isTransient(const float *data, int index)
// {
//     // Simple transient detection (you can make this more sophisticated)
//     const float threshold = 0.02f; // Adjust as needed
//     return std::abs(data[index] - data[index - 1]) > threshold;
// }

// void MainComponent::isolateVocals()
// {
//     if (audioBuffer.getNumChannels() < 2)
//     {
//         juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
//                                                "Processing Error",
//                                                "The audio file must have at least 2 channels for vocal isolation.");
//         return;
//     }

//     int numSamples = audioBuffer.getNumSamples();
//     auto *leftChannel = audioBuffer.getReadPointer(0);
//     auto *rightChannel = audioBuffer.getReadPointer(1);

//     // Create a new buffer to store the vocal-isolated data
//     juce::AudioBuffer<float> vocalBuffer(1, numSamples);
//     auto *vocalData = vocalBuffer.getWritePointer(0);

//     for (int i = 0; i < numSamples; ++i)
//     {
//         // Average the left and right channels to emphasize the center-panned vocals
//         vocalData[i] = (leftChannel[i] + rightChannel[i]) * 0.5f;
//     }

//     // Apply a band-pass filter to focus on vocal frequencies
//     juce::IIRFilter bandPassFilter;
//     bandPassFilter.setCoefficients(juce::IIRCoefficients::makeBandPass(44100, 1000.0, 1.0)); // Center frequency at 1kHz
//     bandPassFilter.processSamples(vocalData, numSamples);

//     // Replace the original audio buffer with the vocal-isolated buffer
//     audioBuffer = vocalBuffer;

//     // Process and display the result as needed
//     processAudio();
// }

void MainComponent::exportAudio()
{
    // First, perform the inverse FFT
    // inverseFFT();

    // Check if the audio buffer is too large
    if (audioBuffer.getNumSamples() > 48000 * 60 * 10) // Example: More than 10 minutes of audio at 48kHz
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Export Error",
                                               "Audio buffer is too large for export.");
        return;
    }

    // Create a file chooser for saving the file
    exportFileChooser = std::make_unique<juce::FileChooser>("Save Audio File",
                                                            juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
                                                            "*.wav");

    auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

    exportFileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser &fc)
                                   {
        auto file = fc.getResult();
        if (file != juce::File{})
        {
            DBG("Selected file: " << file.getFullPathName());

            if (audioBuffer.getNumChannels() == 0 || audioBuffer.getNumSamples() == 0)
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                       "Export Error",
                                                       "Audio buffer is empty.");
                return;
            }

            // Assuming single-channel export (mono)
            std::vector<float> singleChannelAudio(audioBuffer.getNumSamples());

            // Copy the first channel to the singleChannelAudio vector
            auto* channelData = audioBuffer.getReadPointer(0);
            std::copy(channelData, channelData + audioBuffer.getNumSamples(), singleChannelAudio.begin());

            // Create a new AudioBuffer to hold the single channel audio data
            juce::AudioBuffer<float> buffer(1, (int)singleChannelAudio.size());
            auto* ptr = buffer.getWritePointer(0);

            for (int i = 0; i < (int)singleChannelAudio.size(); ++i)
                ptr[i] = singleChannelAudio[i];

            // Write the buffer to a WAV file
            juce::WavAudioFormat format;
            std::unique_ptr<juce::AudioFormatWriter> writer;
            writer.reset(format.createWriterFor(new juce::FileOutputStream(file),
                                                48000,    // Sample rate
                                                1,        // Number of channels
                                                16,       // Bit depth
                                                {},       // Metadata
                                                0));      // Quality option (ignored for WAV)

            if (writer != nullptr)
            {
                DBG("Writing " << buffer.getNumSamples() << " samples");
                if (writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                                                           "Export Successful",
                                                           "Audio file has been exported to: " + file.getFullPathName());
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                           "Export Error",
                                                           "Failed to write audio data to file.");
                }
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                                       "Export Error",
                                                       "Couldn't create the WAV file writer.");
            }
        } });
}
