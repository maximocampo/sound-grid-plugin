#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "GridComponent.h"
#include "SidebarComponent.h"

class SoundGridEditor : public juce::AudioProcessorEditor
{
public:
    SoundGridEditor (SoundGridProcessor& p);
    ~SoundGridEditor() override;

    void resized() override;

private:
    SoundGridProcessor& processorRef;
    GridComponent gridComponent;
    SidebarComponent sidebarComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundGridEditor)
};
