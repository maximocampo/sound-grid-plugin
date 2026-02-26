#include "SampleVoice.h"

SampleVoice::SampleVoice() {}

bool SampleVoice::loadFromBuffer (juce::AudioBuffer<float>& decoded, double fileSampleRate, double /*hostSampleRate*/)
{
    if (decoded.getNumSamples() == 0)
        return false;

    // For simplicity, store at file sample rate. Resampling could be added later.
    // If rates differ significantly, we'd resample here.
    buffer.makeCopyOf (decoded);
    sampleRate = fileSampleRate;
    numSourceSamples = buffer.getNumSamples();
    readPosition = 0;
    loaded = true;
    playing = false;
    return true;
}

void SampleVoice::play()
{
    if (! loaded) return;

    if (playing)
    {
        stop();
        return;
    }

    readPosition = 0;
    playing = true;
}

void SampleVoice::stop()
{
    playing = false;
    readPosition = 0;
}

void SampleVoice::setGain (float g)
{
    gain = g;
}

void SampleVoice::setPan (float p)
{
    pan = juce::jlimit (-1.0f, 1.0f, p);
}

float SampleVoice::getPlaybackPosition() const
{
    if (! loaded || numSourceSamples == 0) return 0.0f;
    return (float) readPosition / (float) numSourceSamples;
}

double SampleVoice::getDuration() const
{
    if (! loaded || sampleRate <= 0.0) return 0.0;
    return (double) numSourceSamples / sampleRate;
}

void SampleVoice::getNextBlock (juce::AudioBuffer<float>& output, int numSamples)
{
    if (! playing || ! loaded || numSourceSamples == 0)
        return;

    // Smooth gain and pan
    const float smoothCoeff = 0.005f;

    int numChannelsOut = output.getNumChannels();
    int numChannelsIn = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        if (! playing)
            break;

        // Smoothing
        smoothGain += (gain - smoothGain) * smoothCoeff;
        smoothPan += (pan - smoothPan) * smoothCoeff;

        // Read from source buffer
        float sampleL = 0.0f;
        float sampleR = 0.0f;

        if (numChannelsIn >= 2)
        {
            sampleL = buffer.getSample (0, (int) readPosition);
            sampleR = buffer.getSample (1, (int) readPosition);
        }
        else if (numChannelsIn == 1)
        {
            sampleL = sampleR = buffer.getSample (0, (int) readPosition);
        }

        // Apply gain
        sampleL *= smoothGain;
        sampleR *= smoothGain;

        // Apply pan (constant power approximation)
        float leftGain = (smoothPan <= 0.0f) ? 1.0f : (1.0f - smoothPan);
        float rightGain = (smoothPan >= 0.0f) ? 1.0f : (1.0f + smoothPan);

        sampleL *= leftGain;
        sampleR *= rightGain;

        // Mix into output
        if (numChannelsOut >= 2)
        {
            output.addSample (0, i, sampleL);
            output.addSample (1, i, sampleR);
        }
        else if (numChannelsOut == 1)
        {
            output.addSample (0, i, (sampleL + sampleR) * 0.5f);
        }

        // Advance read position
        readPosition++;
        if (readPosition >= numSourceSamples)
        {
            if (looping)
                readPosition = 0;
            else
            {
                playing = false;
                readPosition = 0;
            }
        }
    }
}
