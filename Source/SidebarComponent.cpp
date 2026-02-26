#include "SidebarComponent.h"
#include "PluginProcessor.h"

SidebarComponent::SidebarComponent (SoundGridProcessor& p) : processor (p) {}

void SidebarComponent::refresh()
{
    repaint();
}

float SidebarComponent::maxScroll() const
{
    juce::ScopedLock sl (processor.lock);
    int loadedCount = (int) processor.circles.size();
    float totalHeight = (float) loadedCount * ROW_HEIGHT;

    if (! processor.availableFiles.empty())
    {
        totalHeight += ROW_HEIGHT; // divider
        totalHeight += (float) processor.availableFiles.size() * ROW_HEIGHT;
    }

    float max = totalHeight - (float) getHeight();
    return max > 0 ? max : 0;
}

void SidebarComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (240, 240, 240));

    juce::ScopedLock sl (processor.lock);

    float w = (float) getWidth();
    float h = (float) getHeight();
    int loadedCount = (int) processor.circles.size();

    auto font = juce::Font (juce::FontOptions (12.0f));
    g.setFont (font);

    // --- Section 1: loaded sounds ---
    for (int i = 0; i < loadedCount; ++i)
    {
        float rowY = i * ROW_HEIGHT - scrollOffset;

        if (rowY + ROW_HEIGHT < 0 || rowY > h) continue;

        auto& sc = processor.circles[i];

        // Selected row highlight
        if (i == selectedIndex)
        {
            g.setColour (juce::Colour (180, 200, 230));
            g.fillRect (0.0f, rowY, w, (float) ROW_HEIGHT);
        }

        // Color dot matching the grid circle
        g.setColour (sc->circleColour);
        g.fillEllipse (6.0f, rowY + ROW_HEIGHT / 2.0f - 4.0f, 8.0f, 8.0f);

        // Sound name (adjust width based on what else is in the row)
        g.setColour (juce::Colours::black);
        int nameWidth;
        if (sc->hasRecording)
            nameWidth = 130;
        else if (sc->hpCutoffHz > 21.0f)
            nameWidth = (int) w - 80;
        else
            nameWidth = (int) w - 20;
        g.drawText (sc->name, 18, (int) rowY, nameWidth, ROW_HEIGHT, juce::Justification::centredLeft);

        // Movement controls (only if has recording)
        if (sc->hasRecording)
        {
            // Speed control: < 1.0x >
            g.setColour (juce::Colour (80, 80, 80));
            g.drawText ("<", 150, (int) rowY, 10, ROW_HEIGHT, juce::Justification::centred);

            juce::String speedStr = juce::String (sc->movementSpeed, 1) + "x";
            g.drawText (speedStr, 160, (int) rowY, 40, ROW_HEIGHT, juce::Justification::centredLeft);

            int speedStrW = (int) speedStr.length() * 8;
            g.drawText (">", 160 + speedStrW + 2, (int) rowY, 10, ROW_HEIGHT, juce::Justification::centred);

            // Pause/play toggle at x=240
            if (sc->movementPaused)
            {
                // Green play triangle
                g.setColour (juce::Colour (60, 180, 60));
                juce::Path tri;
                tri.addTriangle (240.0f, rowY + 4.0f,
                                 240.0f, rowY + ROW_HEIGHT - 4.0f,
                                 248.0f, rowY + ROW_HEIGHT / 2.0f);
                g.fillPath (tri);
            }
            else
            {
                // Pause bars
                g.setColour (juce::Colour (80, 80, 80));
                g.fillRect (240.0f, rowY + 4.0f, 3.0f, (float) ROW_HEIGHT - 8.0f);
                g.fillRect (245.0f, rowY + 4.0f, 3.0f, (float) ROW_HEIGHT - 8.0f);
            }

            // Red X to remove at x=270
            g.setColour (juce::Colour (200, 60, 60));
            g.drawLine (270.0f, rowY + 4.0f, 278.0f, rowY + ROW_HEIGHT - 4.0f);
            g.drawLine (278.0f, rowY + 4.0f, 270.0f, rowY + ROW_HEIGHT - 4.0f);
        }

        // HP cutoff indicator
        if (sc->hpCutoffHz > 21.0f)
        {
            juce::String hpText;
            if (sc->hpCutoffHz >= 1000.0f)
                hpText = juce::String (sc->hpCutoffHz / 1000.0f, 1) + "k";
            else
                hpText = juce::String ((int) sc->hpCutoffHz);

            g.setColour (juce::Colour (100, 100, 100));
            g.drawText ("HP:" + hpText, (int) w - 60, (int) rowY, 56, ROW_HEIGHT,
                        juce::Justification::centredRight);
        }

        // Separator
        g.setColour (juce::Colour (200, 200, 200));
        g.drawLine (0, rowY + ROW_HEIGHT, w, rowY + ROW_HEIGHT);
    }

    // --- Divider between sections ---
    if (! processor.availableFiles.empty())
    {
        float dividerY = loadedCount * ROW_HEIGHT - scrollOffset;
        if (dividerY + ROW_HEIGHT > 0 && dividerY < h)
        {
            g.setColour (juce::Colour (200, 200, 200));
            g.fillRect (0.0f, dividerY, w, (float) ROW_HEIGHT);
            g.setColour (juce::Colour (120, 120, 120));
            g.drawText ("-- available --", 10, (int) dividerY, 200, ROW_HEIGHT, juce::Justification::centredLeft);
        }

        // --- Section 2: available files ---
        int availStart = loadedCount * ROW_HEIGHT + ROW_HEIGHT;
        for (int i = 0; i < (int) processor.availableFiles.size(); ++i)
        {
            float rowY = availStart + i * ROW_HEIGHT - scrollOffset;

            if (rowY + ROW_HEIGHT < 0 || rowY > h) continue;

            // Icon: outline square
            g.setColour (juce::Colour (120, 120, 120));
            g.drawRect (6.0f, rowY + 4.0f, 6.0f, (float) ROW_HEIGHT - 8.0f, 1.0f);

            // File name
            g.setColour (juce::Colours::black);
            juce::String fname = juce::File (processor.availableFiles[i]).getFileNameWithoutExtension();
            if (fname.length() > 28) fname = fname.substring (0, 28);
            g.drawText (fname, 18, (int) rowY, (int) w - 20, ROW_HEIGHT, juce::Justification::centredLeft);

            // Separator
            g.setColour (juce::Colour (200, 200, 200));
            g.drawLine (0, rowY + ROW_HEIGHT, w, rowY + ROW_HEIGHT);
        }
    }

    // Empty state
    if (processor.circles.empty() && processor.availableFiles.empty())
    {
        g.setColour (juce::Colour (150, 150, 150));
        g.drawText ("no sounds loaded", 20, (int) (h / 2.0f) - 7, (int) w - 40, 14, juce::Justification::centredLeft);
    }
}

void SidebarComponent::mouseDown (const juce::MouseEvent& e)
{
    int loadedCount;
    int availStartY;
    float clickY = (float) e.y + scrollOffset;

    {
        juce::ScopedLock sl (processor.lock);
        loadedCount = (int) processor.circles.size();
        availStartY = loadedCount * ROW_HEIGHT + ROW_HEIGHT;

        // --- Check loaded sounds section ---
        if (clickY < loadedCount * ROW_HEIGHT)
        {
            int clickedRow = (int) (clickY / ROW_HEIGHT);
            if (clickedRow >= 0 && clickedRow < loadedCount)
            {
                auto& sc = processor.circles[(size_t) clickedRow];

                // Movement control hit zones
                if (sc->hasRecording)
                {
                    int x = e.x;
                    double currentTime = processor.getCurrentTime();

                    // Speed decrease
                    if (x >= 146 && x <= 158)
                    {
                        float oldSpeed = sc->movementSpeed;
                        sc->movementSpeed = juce::jlimit (0.1f, 5.0f, sc->movementSpeed - 0.1f);
                        if (! sc->movementPaused)
                        {
                            float elapsed = ((float) currentTime - sc->playbackStartTime) * oldSpeed;
                            sc->playbackStartTime = (float) currentTime - elapsed / sc->movementSpeed;
                        }
                        repaint();
                        return;
                    }

                    // Speed increase
                    juce::String speedStr = juce::String (sc->movementSpeed, 1) + "x";
                    int gtX = 160 + (int) speedStr.length() * 8 + 2;
                    if (x >= gtX - 4 && x <= gtX + 10)
                    {
                        float oldSpeed = sc->movementSpeed;
                        sc->movementSpeed = juce::jlimit (0.1f, 5.0f, sc->movementSpeed + 0.1f);
                        if (! sc->movementPaused)
                        {
                            float elapsed = ((float) currentTime - sc->playbackStartTime) * oldSpeed;
                            sc->playbackStartTime = (float) currentTime - elapsed / sc->movementSpeed;
                        }
                        repaint();
                        return;
                    }

                    // Pause/play toggle
                    if (x >= 236 && x <= 254)
                    {
                        if (sc->movementPaused)
                            sc->resumeMovement (currentTime);
                        else
                            sc->pauseMovement (currentTime);
                        repaint();
                        return;
                    }

                    // Remove movement
                    if (x >= 266 && x <= 282)
                    {
                        sc->removeMovement();
                        repaint();
                        return;
                    }
                }

                selectedIndex = clickedRow;

                // Right-click: play
                if (e.mods.isRightButtonDown())
                {
                    if (sc->isPlaying)
                        processor.voices[(size_t) clickedRow]->stop();
                    else
                        processor.voices[(size_t) clickedRow]->play();
                }
            }
            repaint();
            return;
        }
    } // release lock

    // --- Check available files section (lock released so addSample can acquire it) ---
    juce::String fileToLoad;
    {
        juce::ScopedLock sl (processor.lock);
        if (! processor.availableFiles.empty() && clickY >= availStartY)
        {
            int fileRow = (int) ((clickY - availStartY) / ROW_HEIGHT);
            if (fileRow >= 0 && fileRow < (int) processor.availableFiles.size())
            {
                fileToLoad = processor.availableFiles[(size_t) fileRow];
                processor.availableFiles.erase (processor.availableFiles.begin() + fileRow);
            }
        }
    }

    if (fileToLoad.isNotEmpty())
    {
        processor.addSample (fileToLoad);
        repaint();
    }
}

void SidebarComponent::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    scrollOffset -= wheel.deltaY * 54.0f;  // ~3 rows
    scrollOffset = juce::jlimit (0.0f, maxScroll(), scrollOffset);
    repaint();
}

void SidebarComponent::scanFolder (const juce::File& folder)
{
    for (auto& child : folder.findChildFiles (juce::File::findFilesAndDirectories, false))
    {
        if (child.isDirectory())
            scanFolder (child);
        else if (SoundGridProcessor::isAudioFile (child.getFullPathName()))
            processor.availableFiles.push_back (child.getFullPathName());
    }
}

bool SidebarComponent::isInterestedInFileDrag (const juce::StringArray&)
{
    return true;
}

void SidebarComponent::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& path : files)
    {
        juce::File f (path);
        if (f.isDirectory())
            scanFolder (f);
        else if (SoundGridProcessor::isAudioFile (path))
            processor.availableFiles.push_back (path);
    }
    repaint();
}
