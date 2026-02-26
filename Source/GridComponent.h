#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "SoundCircle.h"

class SoundGridProcessor;

class GridComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public juce::Timer
{
public:
    GridComponent (SoundGridProcessor& p);
    ~GridComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override {}

    // Mouse
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // Keyboard
    bool keyPressed (const juce::KeyPress& key) override;

    // File drag and drop
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    // Timer for animation updates
    void timerCallback() override;

    // For sidebar communication
    void clearSelection();

    std::function<void()> onCirclesChanged;

private:
    SoundGridProcessor& processor;

    SoundCircle* draggedCircle = nullptr;
    SoundCircle* hoveredCircle = nullptr;

    bool isSelecting = false;
    juce::Point<float> selectStart;
    juce::Point<float> selectEnd;
    bool draggingSelection = false;
    juce::Point<float> lastMousePos;

    juce::Rectangle<float> getContentRect() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GridComponent)
};
