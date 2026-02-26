#include "PluginEditor.h"

SoundGridEditor::SoundGridEditor (SoundGridProcessor& p)
    : AudioProcessorEditor (p),
      processorRef (p),
      gridComponent (p),
      sidebarComponent (p)
{
    addAndMakeVisible (gridComponent);
    addAndMakeVisible (sidebarComponent);

    gridComponent.onCirclesChanged = [this]() { sidebarComponent.refresh(); };

    setSize (900, 600);
    setResizable (true, true);
    setResizeLimits (600, 400, 2000, 1400);
}

SoundGridEditor::~SoundGridEditor() {}

void SoundGridEditor::resized()
{
    auto bounds = getLocalBounds();
    int sidebarWidth = 300;

    gridComponent.setBounds (bounds.removeFromLeft (bounds.getWidth() - sidebarWidth));
    sidebarComponent.setBounds (bounds);
}
