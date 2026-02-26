#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "SampleVoice.h"
#include "SoundCircle.h"

struct CircleFilter
{
    juce::dsp::IIR::Filter<float> filterL;
    juce::dsp::IIR::Filter<float> filterR;
    float lastCutoffHz = 20.0f;
};

class SoundGridProcessor : public juce::AudioProcessor
{
public:
    SoundGridProcessor();
    ~SoundGridProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "soundGrid"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Thread-safe interface for UI
    int addSample (const juce::String& filePath);
    void removeAllSamples();
    void playSample (int index);
    void stopSample (int index);
    void stopAllSamples();
    void playAllSamples();
    // Circles and voices - accessed from UI thread with lock
    juce::CriticalSection lock;
    std::vector<std::unique_ptr<SoundCircle>> circles;
    std::vector<std::unique_ptr<SampleVoice>> voices;  // parallel to circles (nullptr for input circles)
    std::vector<std::unique_ptr<CircleFilter>> filters; // parallel to circles
    
    // Input circles (stored in circles vector with isInputCircle=true)
    int addInputCircle (int busIndex);
    void removeCircle (int index);  // works for both sample and input circles

    juce::AudioFormatManager formatManager;

    // Available files (from folder drops, used by sidebar)
    std::vector<juce::String> availableFiles;

    double getCurrentTime() const;

    static bool isAudioFile (const juce::String& path)
    {
        auto ext = juce::File (path).getFileExtension().toLowerCase();
        return ext == ".wav" || ext == ".mp3" || ext == ".aif" ||
               ext == ".aiff" || ext == ".ogg" || ext == ".flac";
    }

private:
    double currentSampleRate = 44100.0;
    double timeAtStart = 0.0;
    juce::AudioBuffer<float> voiceBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundGridProcessor)
};
