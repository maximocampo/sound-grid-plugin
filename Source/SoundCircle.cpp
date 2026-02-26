#include "SoundCircle.h"

SoundCircle::SoundCircle() {}

juce::Colour SoundCircle::getPaletteColour (int index)
{
    static const juce::Colour palette[] = {
        juce::Colour (  0, 255,  80),  //  0: lime
        juce::Colour (255, 120,   0),  //  1: orange
        juce::Colour (200,  70, 255),  //  2: electric purple
        juce::Colour ( 30, 220, 255),  //  3: cyan
        juce::Colour (255, 240,  30),  //  4: yellow
        juce::Colour (255,  30, 150),  //  5: magenta
        juce::Colour (255, 255, 255),  //  6: white
        juce::Colour (255,  80, 120),  //  7: hot pink
        juce::Colour ( 30, 255, 210),  //  8: mint
        juce::Colour (255, 180,  20),  //  9: amber
        juce::Colour (220, 100, 255),  // 10: violet
        juce::Colour (255,  70,  70),  // 11: red
    };
    return palette[((index % 12) + 12) % 12];
}

void SoundCircle::update (double currentTime)
{
    if (! isLoaded) return;

    if (hasRecording && ! isRecording && ! movementPaused)
        applyRecordedMovement (currentTime);

    if (isRecording)
        recordPosition (currentTime);

    float targetVolume = 1.0f - pos.y;
    float targetPan = (pos.x - 0.5f) * 2.0f;

    smoothVolume += (targetVolume - smoothVolume) * 0.1f;
    smoothPan += (targetPan - smoothPan) * 0.1f;
}

float SoundCircle::getDiameter() const
{
    return minDiameter + (maxDiameter - minDiameter) * smoothVolume;
}

float SoundCircle::getRadius() const
{
    return getDiameter() / 2.0f;
}

juce::Rectangle<float> SoundCircle::getBounds (juce::Rectangle<float> contentRect) const
{
    float d = getDiameter();
    float r = d / 2.0f;
    float cx = contentRect.getX() + pos.x * contentRect.getWidth();
    float cy = contentRect.getY() + pos.y * contentRect.getHeight();

    return { cx - r, cy - r, d, d };
}

void SoundCircle::paint (juce::Graphics& g, bool isHovered, juce::Rectangle<float> contentRect)
{
    if (! isLoaded) return;

    auto bounds = getBounds (contentRect);
    float cx = bounds.getCentreX();

    // Recording indicator - red ring outside the circle
    if (isRecording)
    {
        g.setColour (juce::Colour (200, 60, 60));
        g.drawEllipse (bounds.expanded (3.0f), 1.0f);
    }

    // Has recording indicator - small red dot at upper-left
    if (hasRecording && ! isRecording)
    {
        g.setColour (juce::Colour (200, 80, 80));
        g.fillEllipse (bounds.getX() - 3.0f, bounds.getY() - 3.0f, 6.0f, 6.0f);
    }

    // Compute alpha from HP cutoff (log scale: 20Hz=1.0, 18kHz=0.15)
    float hpNorm = std::log (hpCutoffHz / 20.0f) / std::log (18000.0f / 20.0f);
    hpNorm = juce::jlimit (0.0f, 1.0f, hpNorm);
    float alpha = 1.0f - hpNorm * 0.85f;

    // Filled circle background
    g.setColour (circleColour.withAlpha (alpha));
    g.fillEllipse (bounds);

    // Playback arc (clockwise radial fill from 12 o'clock)
    if (isPlaying)
    {
        float angle;
        if (isInputCircle)
            angle = isActive ? juce::MathConstants<float>::twoPi : 0.0f;
        else
            angle = playbackPosition * juce::MathConstants<float>::twoPi;

        if (angle > 0.0f)
        {
            juce::Path arc;
            arc.addPieSegment (bounds.reduced (1.0f),
                               -juce::MathConstants<float>::halfPi,
                               -juce::MathConstants<float>::halfPi + angle,
                               0.0f);

            g.setColour (circleColour.darker (0.3f).withAlpha (alpha));
            g.fillPath (arc);
        }
    }

    // Selection / hover — no border in normal state
    if (isSelected)
    {
        g.setColour (juce::Colours::black);
        g.drawEllipse (bounds, 1.0f);
    }
    else if (isHovered)
    {
        g.setColour (juce::Colours::black);
        g.drawEllipse (bounds, 0.75f);
    }
}

bool SoundCircle::hitTest (float mx, float my, juce::Rectangle<float> contentRect) const
{
    float ccx = contentRect.getX() + pos.x * contentRect.getWidth();
    float ccy = contentRect.getY() + pos.y * contentRect.getHeight();

    float dx = mx - ccx;
    float dy = my - ccy;
    float hitR = getRadius() + 2.0f;
    return (dx * dx + dy * dy) <= (hitR * hitR);
}

void SoundCircle::moveTo (float mx, float my, juce::Rectangle<float> contentRect)
{
    pos.x = juce::jlimit (0.02f, 0.98f, (mx - contentRect.getX()) / contentRect.getWidth());
    pos.y = juce::jlimit (0.05f, 0.95f, (my - contentRect.getY()) / contentRect.getHeight());
}

void SoundCircle::moveBy (float dx, float dy, juce::Rectangle<float> contentRect)
{
    pos.x = juce::jlimit (0.02f, 0.98f, pos.x + dx / contentRect.getWidth());
    pos.y = juce::jlimit (0.05f, 0.95f, pos.y + dy / contentRect.getHeight());
}

// --- Movement Recording ---

void SoundCircle::startRecording (double currentTime)
{
    recording.clear();
    recordStartTime = (float) currentTime;
    isRecording = true;
    hasRecording = false;
}

void SoundCircle::stopRecording (double currentTime)
{
    if (! isRecording) return;
    isRecording = false;

    if (recording.size() > 1)
    {
        recordDuration = (float) currentTime - recordStartTime;
        playbackStartTime = (float) currentTime;
        hasRecording = true;
        movementSpeed = 1.0f;
        movementPaused = false;
        pausedElapsed = 0;
    }
}

void SoundCircle::recordPosition (double currentTime)
{
    if (! isRecording) return;

    RecordedPoint p;
    p.time = (float) currentTime - recordStartTime;
    p.pos = pos;
    recording.push_back (p);
}

void SoundCircle::applyRecordedMovement (double currentTime)
{
    if (! hasRecording || recording.size() < 2 || recordDuration <= 0)
        return;

    float elapsed = ((float) currentTime - playbackStartTime) * movementSpeed;
    float loopTime = std::fmod (elapsed, recordDuration);

    int idx = 0;
    for (int i = 0; i < (int) recording.size() - 1; i++)
    {
        if (recording[i + 1].time > loopTime)
        {
            idx = i;
            break;
        }
        idx = i;
    }

    int nextIdx = (idx + 1) % (int) recording.size();

    float t1 = recording[idx].time;
    float t2 = recording[nextIdx].time;
    if (nextIdx == 0)
        t2 = recordDuration;

    float t = 0;
    if (t2 > t1)
        t = (loopTime - t1) / (t2 - t1);

    auto& p1 = recording[idx].pos;
    auto& p2 = recording[nextIdx].pos;
    pos = { p1.x + (p2.x - p1.x) * t, p1.y + (p2.y - p1.y) * t };
}

void SoundCircle::pauseMovement (double currentTime)
{
    if (! hasRecording || movementPaused) return;
    pausedElapsed = ((float) currentTime - playbackStartTime) * movementSpeed;
    movementPaused = true;
}

void SoundCircle::resumeMovement (double currentTime)
{
    if (! hasRecording || ! movementPaused) return;
    playbackStartTime = (float) currentTime - pausedElapsed / movementSpeed;
    movementPaused = false;
}

void SoundCircle::removeMovement()
{
    hasRecording = false;
    isRecording = false;
    recording.clear();
    recordDuration = 0;
    playbackStartTime = 0;
    movementSpeed = 1.0f;
    movementPaused = false;
    pausedElapsed = 0;
}

void SoundCircle::paintRecordingPath (juce::Graphics& g, juce::Rectangle<float> contentRect)
{
    if (! hasRecording || recording.size() < 2)
        return;

    juce::Path path;

    float firstX = contentRect.getX() + recording[0].pos.x * contentRect.getWidth();
    float firstY = contentRect.getY() + recording[0].pos.y * contentRect.getHeight();
    path.startNewSubPath (firstX, firstY);

    for (size_t i = 1; i < recording.size(); ++i)
    {
        float px = contentRect.getX() + recording[i].pos.x * contentRect.getWidth();
        float py = contentRect.getY() + recording[i].pos.y * contentRect.getHeight();
        path.lineTo (px, py);
    }

    path.lineTo (firstX, firstY);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.strokePath (path, juce::PathStrokeType (0.75f));
}
