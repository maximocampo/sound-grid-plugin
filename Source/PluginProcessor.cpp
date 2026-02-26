#include "PluginProcessor.h"
#include "PluginEditor.h"

SoundGridProcessor::SoundGridProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                        .withInput ("Sidechain 1", juce::AudioChannelSet::stereo(), false)
                        .withInput ("Sidechain 2", juce::AudioChannelSet::stereo(), false)
                        .withInput ("Sidechain 3", juce::AudioChannelSet::stereo(), false)
                        .withInput ("Sidechain 4", juce::AudioChannelSet::stereo(), false)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();
    timeAtStart = juce::Time::getMillisecondCounterHiRes() / 1000.0;
}

SoundGridProcessor::~SoundGridProcessor() {}

void SoundGridProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    voiceBuffer.setSize (2, samplesPerBlock);

    juce::ScopedLock sl (lock);
    for (auto& f : filters)
    {
        if (f)
        {
            f->filterL.reset();
            f->filterR.reset();
            f->lastCutoffHz = 0.0f;
        }
    }
}

void SoundGridProcessor::releaseResources() {}

bool SoundGridProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Output must be stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Main input can be disabled, mono, or stereo
    auto mainIn = layouts.getMainInputChannelSet();
    if (!mainIn.isDisabled() && mainIn != juce::AudioChannelSet::mono() 
                             && mainIn != juce::AudioChannelSet::stereo())
        return false;

    // Sidechain inputs can be disabled, mono, or stereo
    for (int i = 1; i < layouts.inputBuses.size(); ++i)
    {
        auto sc = layouts.inputBuses[i];
        if (!sc.isDisabled() && sc != juce::AudioChannelSet::mono() 
                             && sc != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void SoundGridProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    int numSamples = buffer.getNumSamples();

    // Clear output channels beyond inputs
    for (int ch = totalNumInputChannels; ch < totalNumOutputChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    // Create a mix buffer starting from silence
    juce::AudioBuffer<float> mixBuffer (2, numSamples);
    mixBuffer.clear();

    juce::ScopedLock sl (lock);

    // Ensure voiceBuffer is large enough
    if (voiceBuffer.getNumSamples() < numSamples)
        voiceBuffer.setSize (2, numSamples, false, false, true);

    // Process sample voices (skip input circles which have nullptr voices)
    for (size_t i = 0; i < circles.size(); ++i)
    {
        auto& circle = circles[i];
        auto& voice = voices[i];

        if (!circle || circle->isInputCircle) continue;
        if (!voice) continue;

        if (voice->isPlaying())
        {
            voiceBuffer.clear();
            voice->setGain (circle->smoothVolume * circle->smoothVolume);
            voice->setPan (circle->smoothPan);
            voice->getNextBlock (voiceBuffer, numSamples);

            // Apply per-circle HP filter
            if (i < filters.size() && filters[i] && circle->hpCutoffHz > 21.0f)
            {
                auto& filt = filters[i];
                if (std::abs (filt->lastCutoffHz - circle->hpCutoffHz) > 0.5f)
                {
                    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
                        currentSampleRate, (double) circle->hpCutoffHz);
                    *filt->filterL.coefficients = *coeffs;
                    *filt->filterR.coefficients = *coeffs;
                    filt->lastCutoffHz = circle->hpCutoffHz;
                }

                auto* bufL = voiceBuffer.getWritePointer (0);
                auto* bufR = voiceBuffer.getWritePointer (1);
                for (int s = 0; s < numSamples; ++s)
                {
                    bufL[s] = filt->filterL.processSample (bufL[s]);
                    bufR[s] = filt->filterR.processSample (bufR[s]);
                }
            }

            // Mix into output
            for (int ch = 0; ch < 2; ++ch)
                mixBuffer.addFrom (ch, 0, voiceBuffer, ch, 0, numSamples);
        }

        circle->isPlaying = voice->isPlaying();
        circle->playbackPosition = voice->getPlaybackPosition();
    }

    // Process input circles (sidechain inputs) - now in main circles vector
    int numInputBuses = getBusCount (true);

    for (size_t i = 0; i < circles.size(); ++i)
    {
        auto& c = circles[i];
        if (!c || !c->isInputCircle || !c->isActive) continue;

        int busIdx = c->inputBusIndex;

        if (busIdx < 0 || busIdx >= numInputBuses)
        {
            c->isPlaying = false;
            continue;
        }

        auto* inputBus = getBus (true, busIdx);
        if (!inputBus || !inputBus->isEnabled())
        {
            c->isPlaying = false;
            continue;
        }

        auto inputBuffer = getBusBuffer (buffer, true, busIdx);
        int numInputChannels = inputBuffer.getNumChannels();

        if (numInputChannels < 1 || inputBuffer.getNumSamples() < numSamples)
        {
            c->isPlaying = false;
            continue;
        }

        // Copy input to voiceBuffer for filtering
        voiceBuffer.clear();
        float gain = c->smoothVolume * c->smoothVolume;
        float pan = c->smoothPan;
        float leftGain = gain * (1.0f - std::max (0.0f, pan));
        float rightGain = gain * (1.0f + std::min (0.0f, pan));

        const float* inL = inputBuffer.getReadPointer (0);
        const float* inR = numInputChannels > 1 ? inputBuffer.getReadPointer (1) : inL;

        auto* vbL = voiceBuffer.getWritePointer (0);
        auto* vbR = voiceBuffer.getWritePointer (1);

        for (int s = 0; s < numSamples; ++s)
        {
            vbL[s] = inL[s] * leftGain;
            vbR[s] = inR[s] * rightGain;
        }

        // Apply per-circle HP filter
        if (i < filters.size() && filters[i] && c->hpCutoffHz > 21.0f)
        {
            auto& filt = filters[i];
            if (std::abs (filt->lastCutoffHz - c->hpCutoffHz) > 0.5f)
            {
                auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
                    currentSampleRate, (double) c->hpCutoffHz);
                *filt->filterL.coefficients = *coeffs;
                *filt->filterR.coefficients = *coeffs;
                filt->lastCutoffHz = c->hpCutoffHz;
            }

            for (int s = 0; s < numSamples; ++s)
            {
                vbL[s] = filt->filterL.processSample (vbL[s]);
                vbR[s] = filt->filterR.processSample (vbR[s]);
            }
        }

        // Mix into output
        for (int ch = 0; ch < 2; ++ch)
            mixBuffer.addFrom (ch, 0, voiceBuffer, ch, 0, numSamples);

        c->isPlaying = true;
    }

    // Copy mix to output
    for (int ch = 0; ch < std::min (2, totalNumOutputChannels); ++ch)
        buffer.copyFrom (ch, 0, mixBuffer, ch, 0, numSamples);
}

juce::AudioProcessorEditor* SoundGridProcessor::createEditor()
{
    return new SoundGridEditor (*this);
}

double SoundGridProcessor::getCurrentTime() const
{
    return juce::Time::getMillisecondCounterHiRes() / 1000.0 - timeAtStart;
}

int SoundGridProcessor::addSample (const juce::String& filePath)
{
    juce::File file (filePath);
    if (! file.existsAsFile())
        return -1;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (! reader)
        return -1;

    juce::AudioBuffer<float> decoded ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&decoded, 0, (int) reader->lengthInSamples, 0, true, true);

    auto voice = std::make_unique<SampleVoice>();
    if (! voice->loadFromBuffer (decoded, reader->sampleRate, currentSampleRate))
        return -1;

    auto circle = std::make_unique<SoundCircle>();
    circle->isLoaded = true;
    circle->filePath = filePath;

    juce::String baseName = file.getFileNameWithoutExtension();
    circle->name = baseName;
    circle->colourIndex = (int) circles.size();
    circle->circleColour = SoundCircle::getPaletteColour (circle->colourIndex);

    auto filter = std::make_unique<CircleFilter>();
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, 20.0);
    *filter->filterL.coefficients = *coeffs;
    *filter->filterR.coefficients = *coeffs;

    juce::ScopedLock sl (lock);
    int index = (int) circles.size();
    circle->voiceIndex = index;
    circles.push_back (std::move (circle));
    voices.push_back (std::move (voice));
    filters.push_back (std::move (filter));
    return index;
}

void SoundGridProcessor::removeSample (int index)
{
    removeCircle (index);  // unified removal for both sample and input circles
}

void SoundGridProcessor::removeAllSamples()
{
    juce::ScopedLock sl (lock);
    for (auto& v : voices)
        if (v) v->stop();
    circles.clear();
    voices.clear();
    filters.clear();
}

void SoundGridProcessor::playSample (int index)
{
    juce::ScopedLock sl (lock);
    if (index < 0 || index >= (int) voices.size()) return;
    voices[index]->play();
}

void SoundGridProcessor::stopSample (int index)
{
    juce::ScopedLock sl (lock);
    if (index < 0 || index >= (int) voices.size()) return;
    voices[index]->stop();
}

void SoundGridProcessor::stopAllSamples()
{
    juce::ScopedLock sl (lock);
    for (auto& v : voices)
        v->stop();
}

void SoundGridProcessor::playAllSamples()
{
    juce::ScopedLock sl (lock);
    for (auto& v : voices)
        if (! v->isPlaying())
            v->play();
}

void SoundGridProcessor::updateVoiceParams (int index, float gain, float pan)
{
    juce::ScopedLock sl (lock);
    if (index < 0 || index >= (int) voices.size()) return;
    voices[index]->setGain (gain);
    voices[index]->setPan (pan);
}

int SoundGridProcessor::addInputCircle (int busIndex)
{
    // Bus names: 0 = Main Input, 1-4 = Sidechain 1-4
    juce::String busName;
    if (busIndex == 0)
        busName = "Main In";
    else
        busName = "SC " + juce::String (busIndex);

    auto circle = std::make_unique<SoundCircle>();
    circle->isInputCircle = true;
    circle->inputBusIndex = busIndex;
    circle->isActive = true;
    circle->isLoaded = true;
    circle->name = busName;
    circle->colourIndex = -1;
    circle->circleColour = juce::Colour (100, 190, 175);

    // Random position
    juce::Random rng;
    circle->pos.x = rng.nextFloat() * 0.84f + 0.08f;
    circle->pos.y = rng.nextFloat() * 0.70f + 0.10f;

    auto filter = std::make_unique<CircleFilter>();
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, 20.0);
    *filter->filterL.coefficients = *coeffs;
    *filter->filterR.coefficients = *coeffs;

    juce::ScopedLock sl (lock);
    int index = (int) circles.size();
    circle->voiceIndex = index;
    circles.push_back (std::move (circle));
    voices.push_back (nullptr);  // No voice for input circles
    filters.push_back (std::move (filter));
    return index;
}

void SoundGridProcessor::removeCircle (int index)
{
    juce::ScopedLock sl (lock);
    if (index < 0 || index >= (int) circles.size()) return;

    // Stop voice if it's a sample circle
    if (voices[index])
        voices[index]->stop();

    circles.erase (circles.begin() + index);
    voices.erase (voices.begin() + index);
    if (index < (int) filters.size())
        filters.erase (filters.begin() + index);

    // Re-index
    for (int i = 0; i < (int) circles.size(); ++i)
        circles[i]->voiceIndex = i;
}

// --- State Save/Restore ---

void SoundGridProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ScopedLock sl (lock);

    auto xml = std::make_unique<juce::XmlElement> ("soundGridState");

    for (size_t i = 0; i < circles.size(); ++i)
    {
        auto& c = circles[i];
        
        if (c->isInputCircle)
        {
            // Save input circle
            auto* inputXml = xml->createNewChildElement ("inputCircle");
            inputXml->setAttribute ("busIndex", c->inputBusIndex);
            inputXml->setAttribute ("posX", (double) c->pos.x);
            inputXml->setAttribute ("posY", (double) c->pos.y);
            inputXml->setAttribute ("hpCutoff", (double) c->hpCutoffHz);
        }
        else
        {
            // Save sample circle
            auto* circleXml = xml->createNewChildElement ("circle");
            circleXml->setAttribute ("filePath", c->filePath);
            circleXml->setAttribute ("posX", (double) c->pos.x);
            circleXml->setAttribute ("posY", (double) c->pos.y);
            circleXml->setAttribute ("playing", c->isPlaying);
            circleXml->setAttribute ("colourIndex", c->colourIndex);
            circleXml->setAttribute ("hpCutoff", (double) c->hpCutoffHz);

            // Movement recording
            if (c->hasRecording)
            {
                auto* recXml = circleXml->createNewChildElement ("recording");
                recXml->setAttribute ("duration", (double) c->recordDuration);
                recXml->setAttribute ("speed", (double) c->movementSpeed);
                recXml->setAttribute ("paused", c->movementPaused);

                for (auto& rp : c->recording)
                {
                    auto* ptXml = recXml->createNewChildElement ("point");
                    ptXml->setAttribute ("time", (double) rp.time);
                    ptXml->setAttribute ("x", (double) rp.pos.x);
                    ptXml->setAttribute ("y", (double) rp.pos.y);
                }
            }
        }
    }

    // Available files
    for (auto& f : availableFiles)
    {
        auto* fileXml = xml->createNewChildElement ("availableFile");
        fileXml->setAttribute ("path", f);
    }

    copyXmlToBinary (*xml, destData);
}

void SoundGridProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (! xml || xml->getTagName() != "soundGridState")
        return;

    // Clear existing
    {
        juce::ScopedLock sl (lock);
        circles.clear();
        voices.clear();
        filters.clear();
        availableFiles.clear();
    }

    // Restore sample circles
    for (auto* circleXml : xml->getChildWithTagNameIterator ("circle"))
    {
        juce::String filePath = circleXml->getStringAttribute ("filePath");
        int idx = addSample (filePath);
        if (idx < 0) continue;

        juce::ScopedLock sl (lock);
        auto& c = circles[idx];
        c->pos.x = (float) circleXml->getDoubleAttribute ("posX", 0.5);
        c->pos.y = (float) circleXml->getDoubleAttribute ("posY", 0.5);
        c->colourIndex = circleXml->getIntAttribute ("colourIndex", idx);
        c->circleColour = SoundCircle::getPaletteColour (c->colourIndex);
        c->hpCutoffHz = (float) circleXml->getDoubleAttribute ("hpCutoff", 20.0);

        bool wasPlaying = circleXml->getBoolAttribute ("playing", false);
        if (wasPlaying && voices[idx])
            voices[idx]->play();

        // Restore recording
        auto* recXml = circleXml->getChildByName ("recording");
        if (recXml)
        {
            c->recordDuration = (float) recXml->getDoubleAttribute ("duration", 0);
            c->movementSpeed = (float) recXml->getDoubleAttribute ("speed", 1.0);
            c->movementPaused = recXml->getBoolAttribute ("paused", false);

            for (auto* ptXml : recXml->getChildWithTagNameIterator ("point"))
            {
                RecordedPoint rp;
                rp.time = (float) ptXml->getDoubleAttribute ("time");
                rp.pos.x = (float) ptXml->getDoubleAttribute ("x");
                rp.pos.y = (float) ptXml->getDoubleAttribute ("y");
                c->recording.push_back (rp);
            }

            if (c->recording.size() > 1 && c->recordDuration > 0)
            {
                c->hasRecording = true;
                c->playbackStartTime = (float) getCurrentTime();
                if (c->movementPaused)
                    c->pausedElapsed = 0;
            }
        }
    }

    // Restore input circles
    for (auto* inputXml : xml->getChildWithTagNameIterator ("inputCircle"))
    {
        int busIndex = inputXml->getIntAttribute ("busIndex", 0);
        int idx = addInputCircle (busIndex);
        if (idx < 0) continue;

        juce::ScopedLock sl (lock);
        auto& c = circles[idx];
        c->pos.x = (float) inputXml->getDoubleAttribute ("posX", 0.5);
        c->pos.y = (float) inputXml->getDoubleAttribute ("posY", 0.5);
        c->hpCutoffHz = (float) inputXml->getDoubleAttribute ("hpCutoff", 20.0);
    }

    // Restore available files
    for (auto* fileXml : xml->getChildWithTagNameIterator ("availableFile"))
    {
        juce::String path = fileXml->getStringAttribute ("path");
        if (path.isNotEmpty())
            availableFiles.push_back (path);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoundGridProcessor();
}
