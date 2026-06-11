#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace cassette
{

class TransientShaper
{
public:
    void setAttackDb(float db) { attackDb = db; }
    void setSustainDb(float db) { sustainDb = db; }
    void prepare(double sampleRate);
    void process(juce::AudioBuffer<float>& buffer);

private:
    float attackDb = 0.0f;
    float sustainDb = 0.0f;
    double sampleRate = 48000.0;
};

}
