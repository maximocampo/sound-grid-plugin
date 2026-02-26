#pragma once
#include <juce_audio_formats/juce_audio_formats.h>

class SampleVoice
{
public:
    SampleVoice();

    bool loadFromBuffer (juce::AudioBuffer<float>& decoded, double fileSampleRate);

    void play();
    void stop();

    void getNextBlock (juce::AudioBuffer<float>& output, int numSamples);

    void setGain (float g);
    void setPan (float p);

    bool isPlaying() const { return playing; }
    bool isLoaded() const { return loaded; }

    // Returns playback position 0-1
    float getPlaybackPosition() const;

private:
    juce::AudioBuffer<float> buffer;
    int64_t readPosition = 0;
    bool playing = false;
    bool loaded = false;

    float gain = 1.0f;
    float pan = 0.0f;        // -1 left, 0 center, 1 right
    float smoothGain = 1.0f;
    float smoothPan = 0.0f;

    double sampleRate = 44100.0;
    int numSourceSamples = 0;
};
