#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "CassetteTapePrepProcessor.h"

namespace cassette
{

class CassetteTapePrepEditor : public juce::AudioProcessorEditor
{
public:
    explicit CassetteTapePrepEditor(CassetteTapePrepProcessor& processor);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    CassetteTapePrepProcessor& proc;
    juce::ComboBox profileBox;
    juce::Slider hfSlider;
    juce::ToggleButton cleanTransfer { "Clean Transfer" };
    juce::ToggleButton bypass { "Bypass" };
    juce::Label title { {}, "Cassette Tape Prep" };
    juce::Label hint { {}, "Offline bounce = full analysis · realtime = periodic refresh" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ComboAttachment> profileAttachment;
    std::unique_ptr<SliderAttachment> hfAttachment;
    std::unique_ptr<ButtonAttachment> cleanAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
};

}
