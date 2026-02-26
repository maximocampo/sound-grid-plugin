# MAXI Sound Grid

![ScreenRecording2026-02-25at8 11 51p m -ezgif com-video-to-gif-converter](https://github.com/user-attachments/assets/346cac5f-5c3a-4dfa-8006-f3582c4be9fa)

- X axis is pan, Y axis is volume
- Drop samples into the plugin, move them around
- Press "o" to record movements
- Right-click on each circle to play/stop the sample
- Scroll on each circle for highpass filter

## How to run:

Prerequisites

- CMake 3.22+
- JUCE framework
- A C++ compiler (Xcode/clang on macOS)
- Build


## Clone JUCE (if you don't have it already)
git clone https://github.com/juce-framework/JUCE.git libs/JUCE

## Configure and build
``cmake -B build -S .`` \
``cmake --build build`` 

Note: the CMakeLists.txt expects JUCE at ``../../../libs/JUCE`` relative to the project.

Place JUCE at that relative path, or
Update the add_subdirectory path in CMakeLists.txt to point to wherever JUCE is
Output formats: AU, VST3, and Standalone (line 10 of CMakeLists.txt).
