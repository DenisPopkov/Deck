#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/CassetteAutoMaster.h"
#include "../dsp/CassetteProfile.h"
#include "../dsp/MasteringOptions.h"
#include "../analysis/AudioFeatures.h"
#include "../analysis/EssentiaAnalyzer.h"

namespace cassette
{

class CassetteTapePrepProcessor : public juce::AudioProcessor
{
public:
    CassetteTapePrepProcessor();
    ~CassetteTapePrepProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cassette Tape Prep"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getApvts() { return apvts; }

private:
    CassetteProfile currentProfile() const;
    MasteringOptions currentOptions() const;
    void refreshFeatures(const juce::AudioBuffer<float>& buffer);

    juce::AudioProcessorValueTreeState apvts;
    CassetteAutoMaster mastering;
    AudioFeatures features;
    MasteringOptions options;

    double sampleRateHz = 48000.0;
    int analysisCooldownSamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CassetteTapePrepProcessor)
};

}
