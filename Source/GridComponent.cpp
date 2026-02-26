#include "GridComponent.h"
#include "PluginProcessor.h"

GridComponent::GridComponent (SoundGridProcessor& p) : processor (p)
{
    setWantsKeyboardFocus (true);
    startTimerHz (60);
}

GridComponent::~GridComponent()
{
    stopTimer();
}

juce::Rectangle<float> GridComponent::getContentRect() const
{
    return getLocalBounds().toFloat();
}

void GridComponent::timerCallback()
{
    double currentTime = processor.getCurrentTime();

    {
        juce::ScopedLock sl (processor.lock);
        for (auto& c : processor.circles)
            c->update (currentTime);
    }

    repaint();
}

void GridComponent::paint (juce::Graphics& g)
{
    auto content = getContentRect();

    // Background
    g.fillAll (juce::Colours::white);

    {
        juce::ScopedLock sl (processor.lock);

        // Draw recording paths first (underneath circles)
        for (auto& c : processor.circles)
            c->paintRecordingPath (g, content);

        // Draw circles
        for (auto& c : processor.circles)
        {
            bool hovered = (c.get() == hoveredCircle) || (c.get() == draggedCircle);
            c->paint (g, hovered, content);
        }

        // Selection box
        if (isSelecting)
        {
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            float left = std::min (selectStart.x, selectEnd.x);
            float top = std::min (selectStart.y, selectEnd.y);
            float w = std::abs (selectEnd.x - selectStart.x);
            float h = std::abs (selectEnd.y - selectStart.y);
            g.drawRect (left, top, w, h, 1.0f);
        }

        // Axis labels
        auto font = juce::Font (juce::FontOptions (12.0f));
        g.setFont (font);

        g.setColour (juce::Colour (180, 180, 180));
        g.drawText ("L", 4, (int) content.getHeight() - 16, 20, 14, juce::Justification::left);
        g.drawText ("R", (int) content.getWidth() - 20, (int) content.getHeight() - 16, 16, 14, juce::Justification::right);
        g.drawText ("loud", 4, 2, 40, 14, juce::Justification::left);
        g.drawText ("quiet", 4, (int) content.getHeight() - 28, 40, 14, juce::Justification::left);

        // Status
        g.setColour (juce::Colour (150, 150, 150));
        int playing = 0;
        for (auto& c : processor.circles)
            if (c->isPlaying) playing++;

        juce::String status = juce::String ((int) processor.circles.size());
        if (playing > 0)
            status += "/" + juce::String (playing);
        g.drawText (status, (int) content.getWidth() - 60, 2, 56, 14, juce::Justification::right);

        // Empty state
        if (processor.circles.empty())
        {
            g.setColour (juce::Colour (180, 180, 180));
            g.drawText ("drop audio files here", content, juce::Justification::centred);
        }
    }
}

void GridComponent::clearSelection()
{
    juce::ScopedLock sl (processor.lock);
    for (auto& c : processor.circles)
        c->isSelected = false;
}

void GridComponent::mouseDown (const juce::MouseEvent& e)
{
    auto content = getContentRect();
    float mx = (float) e.x;
    float my = (float) e.y;

    juce::ScopedLock sl (processor.lock);

    // Check if clicked on a circle (reverse order)
    for (int i = (int) processor.circles.size() - 1; i >= 0; --i)
    {
        auto& c = processor.circles[i];
        if (c->hitTest (mx, my, content))
        {
            // Right-click: play/stop for samples, toggle active for inputs
            if (e.mods.isRightButtonDown())
            {
                if (c->isInputCircle)
                {
                    c->isActive = !c->isActive;  // toggle input on/off
                }
                else if (processor.voices[i])
                {
                    if (c->isPlaying)
                        processor.voices[i]->stop();
                    else
                        processor.voices[i]->play();
                }
                return;
            }

            // Left-click: start dragging
            if (c->isSelected)
            {
                // Drag all selected
                draggingSelection = true;
                lastMousePos = { mx, my };
            }
            else
            {
                // Drag single circle
                clearSelection();
                draggedCircle = c.get();
            }
            return;
        }
    }

    // Clicked empty space - start selection box
    if (e.mods.isLeftButtonDown())
    {
        clearSelection();
        isSelecting = true;
        selectStart = { mx, my };
        selectEnd = selectStart;
    }
}

void GridComponent::mouseDrag (const juce::MouseEvent& e)
{
    float mx = (float) e.x;
    float my = (float) e.y;
    auto content = getContentRect();

    juce::ScopedLock sl (processor.lock);

    // Dragging single circle
    if (draggedCircle)
    {
        draggedCircle->moveTo (mx, my, content);
        return;
    }

    // Dragging selection
    if (draggingSelection)
    {
        float dx = mx - lastMousePos.x;
        float dy = my - lastMousePos.y;
        for (auto& c : processor.circles)
            if (c->isSelected)
                c->moveBy (dx, dy, content);
        lastMousePos = { mx, my };
        return;
    }

    // Selection box
    if (isSelecting)
    {
        selectEnd = { mx, my };

        float left = std::min (selectStart.x, selectEnd.x);
        float top = std::min (selectStart.y, selectEnd.y);
        float right = std::max (selectStart.x, selectEnd.x);
        float bottom = std::max (selectStart.y, selectEnd.y);
        juce::Rectangle<float> selectRect (left, top, right - left, bottom - top);

        for (auto& c : processor.circles)
            c->isSelected = selectRect.intersects (c->getBounds (content));
    }
}

void GridComponent::mouseUp (const juce::MouseEvent&)
{
    draggedCircle = nullptr;
    draggingSelection = false;
    isSelecting = false;
}

void GridComponent::mouseMove (const juce::MouseEvent& e)
{
    float mx = (float) e.x;
    float my = (float) e.y;
    auto content = getContentRect();

    juce::ScopedLock sl (processor.lock);

    // Update hover
    hoveredCircle = nullptr;
    for (int i = (int) processor.circles.size() - 1; i >= 0; --i)
    {
        if (processor.circles[i]->hitTest (mx, my, content))
        {
            hoveredCircle = processor.circles[i].get();
            break;
        }
    }
}

void GridComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    float mx = (float) e.x;
    float my = (float) e.y;
    auto content = getContentRect();

    juce::ScopedLock sl (processor.lock);

    SoundCircle* target = nullptr;
    for (int i = (int) processor.circles.size() - 1; i >= 0; --i)
    {
        if (processor.circles[i]->hitTest (mx, my, content))
        {
            target = processor.circles[i].get();
            break;
        }
    }

    if (target == nullptr)
        return;

    float logMin = std::log (20.0f);
    float logMax = std::log (18000.0f);
    float logCurrent = std::log (target->hpCutoffHz);

    float step = (logMax - logMin) * 0.05f * wheel.deltaY;
    logCurrent += step;

    target->hpCutoffHz = juce::jlimit (20.0f, 18000.0f, std::exp (logCurrent));
}

bool GridComponent::keyPressed (const juce::KeyPress& key)
{
    auto ch = key.getTextCharacter();

    if (key == juce::KeyPress::spaceKey)
    {
        processor.stopAllSamples();
        return true;
    }

    if (ch == 'c')
    {
        processor.removeAllSamples();
        draggedCircle = nullptr;
        hoveredCircle = nullptr;
        if (onCirclesChanged) onCirclesChanged();
        return true;
    }

    if (ch == 'p')
    {
        processor.playAllSamples();
        return true;
    }

    if (ch == 'o')
    {
        double currentTime = processor.getCurrentTime();
        juce::ScopedLock sl (processor.lock);

        // Toggle recording on hovered/selected circles
        if (hoveredCircle)
        {
            if (hoveredCircle->isRecording)
                hoveredCircle->stopRecording (currentTime);
            else
                hoveredCircle->startRecording (currentTime);
        }
        else
        {
            for (auto& c : processor.circles)
            {
                if (c->isSelected)
                {
                    if (c->isRecording)
                        c->stopRecording (currentTime);
                    else
                        c->startRecording (currentTime);
                }
            }
        }
        return true;
    }

    if (ch == 'x')
    {
        juce::ScopedLock sl (processor.lock);

        if (hoveredCircle)
            hoveredCircle->removeMovement();

        for (auto& c : processor.circles)
            if (c->isSelected)
                c->removeMovement();

        if (onCirclesChanged) onCirclesChanged();
        return true;
    }

    // Add input circle
    if (ch == 'i')
    {
        juce::PopupMenu menu;
        menu.addItem (1, "Main Input");
        menu.addItem (2, "Sidechain 1");
        menu.addItem (3, "Sidechain 2");
        menu.addItem (4, "Sidechain 3");
        menu.addItem (5, "Sidechain 4");

        menu.showMenuAsync (juce::PopupMenu::Options(),
            [this] (int result) {
                if (result > 0)
                {
                    processor.addInputCircle (result - 1);  // 0 = Main, 1-4 = Sidechains
                    if (onCirclesChanged) onCirclesChanged();
                }
            });
        return true;
    }

    // Delete hovered or selected circles with backspace/delete
    if (key == juce::KeyPress::backspaceKey || key == juce::KeyPress::deleteKey)
    {
        juce::ScopedLock sl (processor.lock);
        
        // Collect indices to delete (in reverse order to not invalidate indices)
        std::vector<int> toDelete;
        
        for (int i = (int) processor.circles.size() - 1; i >= 0; --i)
        {
            if (processor.circles[i].get() == hoveredCircle || processor.circles[i]->isSelected)
                toDelete.push_back (i);
        }
        
        for (int idx : toDelete)
            processor.removeCircle (idx);
        
        hoveredCircle = nullptr;
        draggedCircle = nullptr;
        
        if (onCirclesChanged) onCirclesChanged();
        return true;
    }

    return false;
}

bool GridComponent::isAudioFile (const juce::String& path) const
{
    auto ext = juce::File (path).getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".mp3" || ext == ".aif" ||
           ext == ".aiff" || ext == ".ogg" || ext == ".flac";
}

bool GridComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (isAudioFile (f))
            return true;
    return false;
}

void GridComponent::filesDropped (const juce::StringArray& files, int /*x*/, int /*y*/)
{
    for (auto& f : files)
    {
        if (isAudioFile (f))
        {
            int idx = processor.addSample (f);
            if (idx >= 0)
            {
                juce::ScopedLock sl (processor.lock);
                auto& c = processor.circles[idx];
                juce::Random rng;
                c->pos.x = rng.nextFloat() * 0.84f + 0.08f;
                c->pos.y = rng.nextFloat() * 0.70f + 0.10f;
            }
        }
    }
    if (onCirclesChanged) onCirclesChanged();
}
