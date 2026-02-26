#pragma once
#include "SampleVoice.h"
#include <juce_gui_basics/juce_gui_basics.h>

struct RecordedPoint {
	float time;
	juce::Point<float> pos;
};

class SoundCircle {
public:
	SoundCircle();

	void update(double currentTime);
	void paint(juce::Graphics & g, bool isHovered, juce::Rectangle<float> contentRect);
	void paintRecordingPath(juce::Graphics& g, juce::Rectangle<float> contentRect);

	bool hitTest(float mx, float my, juce::Rectangle<float> contentRect) const;
	void moveTo(float mx, float my, juce::Rectangle<float> contentRect);
	void moveBy(float dx, float dy, juce::Rectangle<float> contentRect);
	juce::Rectangle<float> getBounds(juce::Rectangle<float> contentRect) const;
	static juce::Colour getPaletteColour(int index);

	float getDiameter() const;
	float getRadius() const;

	// Movement recording
	void startRecording(double currentTime);
	void stopRecording(double currentTime);
	void recordPosition(double currentTime);
	void applyRecordedMovement(double currentTime);
	void pauseMovement(double currentTime);
	void resumeMovement(double currentTime);
	void removeMovement();

	// Audio (managed by processor, referenced here)
	int voiceIndex = -1; // index into processor's voice array
	juce::String filePath;

	// Input circle properties (for routing audio from other tracks)
	int inputBusIndex = -1; // -1 = sample, 0+ = input bus index
	bool isActive = false; // for input circles - whether receiving audio
	bool isInputCircle = false;

	juce::Point<float> pos { 0.5f, 0.5f };

	bool isPlaying = false;
	bool isLoaded = false;
	bool isSelected = false;
	bool isRecording = false;
	bool hasRecording = false;

	juce::String name;
	juce::Colour circleColour { 180, 180, 180 };
	int colourIndex = 0;

	float playbackPosition = 0.0f; // 0-1, updated from voice

	// Highpass filter cutoff (20Hz = off, up to 18000Hz)
	float hpCutoffHz = 20.0f;

	// Movement recording
	std::vector<RecordedPoint> recording;
	float recordStartTime = 0;
	float recordDuration = 0;
	float playbackStartTime = 0;
	float movementSpeed = 1.0f;
	bool movementPaused = false;
	float pausedElapsed = 0;

	// Smoothed audio params (computed from position)
	float smoothVolume = 0.5f;
	float smoothPan = 0.0f;

private:
	static constexpr float minDiameter = 30.0f;
	static constexpr float maxDiameter = 100.0f;
	static constexpr float labelOffsetY = 4.0f;
};
