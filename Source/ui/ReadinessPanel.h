#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <optional>
#include <vector>
#include "../analysis/AudioFeatures.h"
#include "../analysis/PerceptualQualityGuard.h"
#include "../dsp/CassetteProfile.h"

namespace cassette
{

enum class ReadinessLevel
{
    Ok,
    Warn,
    Fail
};

class ReadinessPanel : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    ReadinessPanel();

    void setAdvancedMode(bool advanced);
    void setSourceFeatures(const AudioFeatures& f, TapeFormulation tape);
    void setProcessedFeatures(const AudioFeatures& f,
                              TapeFormulation tape,
                              const std::optional<PerceptualQualityReport>& quality = std::nullopt);
    void setBaselineFeatures(const AudioFeatures& baseline);
    void clear();

    int readinessScore() const { return score; }
    int readinessMax() const { return maxScore; }

    void paint(juce::Graphics& g) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;

private:
    struct Row
    {
        juce::String name;
        juce::String value;
        juce::String tooltip;
        ReadinessLevel level = ReadinessLevel::Ok;
        juce::Rectangle<int> bounds;
    };

    void rebuildRows();
    static ReadinessLevel levelFromBool(bool ok, bool warn = false);
    juce::Colour colourFor(ReadinessLevel level) const;
    int rowAtY(int y) const;

    bool advancedMode = false;
    bool hasProcessed = false;
    AudioFeatures sourceFeatures;
    AudioFeatures processedFeatures;
    AudioFeatures baselineFeatures;
    bool hasBaseline = false;
    std::optional<PerceptualQualityReport> qualityReport;
    TapeFormulation tapeType = TapeFormulation::TypeIV;

    int score = 0;
    int maxScore = 8;
    std::vector<Row> rows;
    int headerHeight = 86;
};

}
