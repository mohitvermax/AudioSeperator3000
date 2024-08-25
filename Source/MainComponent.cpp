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

    formatManager.registerBasicFormats();
}

MainComponent::~MainComponent()
{
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

void MainComponent::inverseFFT()
{
    const int fftSize = fft.getSize();
    const int numChannels = audioBuffer.getNumChannels();
    const int numSamples = audioBuffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
    {
        DBG("Invalid audio buffer in inverseFFT");
        return;
    }

    // Create a new buffer for the inverse FFT result
    juce::AudioSampleBuffer inversedBuffer(numChannels, numSamples);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        float *channelData = inversedBuffer.getWritePointer(channel);
        const float *originalData = audioBuffer.getReadPointer(channel);

        for (int sample = 0; sample + fftSize <= numSamples; sample += fftSize)
        {
            // Copy original data to fftData
            juce::FloatVectorOperations::copy(fftData.data(), originalData + sample, fftSize);

            // Zero out the imaginary part
            juce::FloatVectorOperations::clear(fftData.data() + fftSize, fftSize);

            // Perform inverse FFT
            fft.performRealOnlyInverseTransform(fftData.data());

            // Copy the real part of the inverse FFT result to the output buffer
            juce::FloatVectorOperations::copy(channelData + sample, fftData.data(), fftSize);
        }
    }

    // Replace the original audio buffer with the inverse FFT result
    audioBuffer = inversedBuffer;
}

void MainComponent::exportAudio()
{
    // First, perform the inverse FFT
    inverseFFT();

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

            // Create a WAV writer
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(new juce::FileOutputStream(file),
                                                            audioBuffer.getNumChannels(),
                                                            44100.0,  // Sample rate
                                                            16,       // Bit depth
                                                            {},       // Metadata
                                                            0         // Quality option (ignored for WAV)
                                                            ));

            if (writer != nullptr)
            {
                // Write the audio data to the file
                if (writer->writeFromAudioSampleBuffer(audioBuffer, 0, audioBuffer.getNumSamples()))
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
