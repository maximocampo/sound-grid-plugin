#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class SoundGridProcessor;

class SidebarComponent : public juce::Component,
                         public juce::FileDragAndDropTarget
{
public:
    SidebarComponent (SoundGridProcessor& p);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    // File drag and drop (for folders)
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    void refresh();

    int selectedIndex = -1;

private:
    SoundGridProcessor& processor;
    float scrollOffset = 0.0f;

    static constexpr int ROW_HEIGHT = 18;

    float maxScroll() const;
    void scanFolder (const juce::File& folder);
    bool isAudioFile (const juce::String& path) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidebarComponent)
};
