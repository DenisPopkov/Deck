#include "ReadinessPanel.h"
#include "UiTheme.h"

namespace cassette
{
namespace
{

int computeScore(const AudioFeatures& f, const CassetteProfile& profile)
{
    int s = 0;
    if (f.integratedLUFS >= -17.0f && f.integratedLUFS <= -10.0f)
        ++s;
    if (f.truePeakDbfs <= profile.truePeakCeilingDbfs + 0.5f)
        ++s;
    if (f.psycho.hfAboveMaskingDb < 6.0f && f.psycho.sharpnessAcum < 2.2f && f.psycho.roughnessAsper < 0.35f)
        ++s;
    if (f.lfStereoCorrelation > 0.3f)
        ++s;
    if (f.crestFactorDb > 6.0f)
        ++s;
    if (f.clippingPercent < 0.01f)
        ++s;
    if (f.loudnessRangeDb > 2.0f && f.loudnessRangeDb < 20.0f)
        ++s;
    if (f.noiseFloorDbfs < -50.0f)
        ++s;
    return s;
}

}

ReadinessPanel::ReadinessPanel()
{
    setRepaintsOnMouseActivity(true);
}

void ReadinessPanel::setAdvancedMode(bool advanced)
{
    advancedMode = advanced;
    rebuildRows();
    repaint();
}

void ReadinessPanel::setSourceFeatures(const AudioFeatures& f, TapeFormulation tape)
{
    sourceFeatures = f;
    tapeType = tape;
    hasProcessed = false;
    qualityReport.reset();
    rebuildRows();
    repaint();
}

void ReadinessPanel::setProcessedFeatures(const AudioFeatures& f,
                                          TapeFormulation tape,
                                          const std::optional<PerceptualQualityReport>& quality)
{
    processedFeatures = f;
    tapeType = tape;
    qualityReport = quality;
    hasProcessed = true;
    rebuildRows();
    repaint();
}

void ReadinessPanel::setBaselineFeatures(const AudioFeatures& baseline)
{
    baselineFeatures = baseline;
    hasBaseline = true;
    rebuildRows();
    repaint();
}

void ReadinessPanel::clear()
{
    hasProcessed = false;
    hasBaseline = false;
    qualityReport.reset();
    score = 0;
    rows.clear();
    setTooltip({});
    repaint();
}

ReadinessLevel ReadinessPanel::levelFromBool(bool ok, bool warn)
{
    if (ok)
        return ReadinessLevel::Ok;
    if (warn)
        return ReadinessLevel::Warn;
    return ReadinessLevel::Fail;
}

void ReadinessPanel::rebuildRows()
{
    rows.clear();
    const auto& f = hasProcessed ? processedFeatures : sourceFeatures;
    const auto profile = CassetteProfile::fromFormulation(tapeType);
    score = computeScore(f, profile);

    const bool loudOk = f.integratedLUFS >= -17.0f && f.integratedLUFS <= -10.0f;
    const bool tpOk = f.truePeakDbfs <= profile.truePeakCeilingDbfs + 0.5f;
    const bool hfOk = f.psycho.hfAboveMaskingDb < 6.0f && f.psycho.sharpnessAcum < 2.2f;
    const bool roughOk = f.psycho.roughnessAsper < 0.35f;
    const bool bassOk = f.lfStereoCorrelation > 0.3f;

    rows.push_back({ "Loudness",
                     juce::String(f.integratedLUFS, 1) + " LUFS",
                     "Integrated loudness (BS.1770 K-weight). Target ~−12 to −14 LUFS for Type II/IV.",
                     levelFromBool(loudOk, f.integratedLUFS > -10.5f) });
    rows.push_back({ "True Peak",
                     juce::String(f.truePeakDbfs, 1) + " dBTP",
                     "Inter-sample true peak (4× OS). Must stay below tape deck headroom.",
                     levelFromBool(tpOk) });
    rows.push_back({ "HF Safety",
                     "Sharpness " + juce::String(f.psycho.sharpnessAcum, 1) + " acum",
                     "Zwicker sharpness + GMT masking. High values risk self-erasure on tape.",
                     levelFromBool(hfOk) });
    rows.push_back({ "Stereo Bass",
                     "LF corr " + juce::String(f.lfStereoCorrelation, 2),
                     "Low-frequency stereo correlation. Low/negative = risky for cassette crosstalk.",
                     levelFromBool(bassOk, f.lfStereoCorrelation > 0.0f) });
    rows.push_back({ "Harshness",
                     juce::String(f.psycho.roughnessAsper, 2) + " asper",
                     "Roughness (Daniel–Weber). High modulation sounds harsh on ferric/chrome.",
                     levelFromBool(roughOk) });

    if (qualityReport.has_value())
    {
        const auto odg = qualityReport->objectiveDifferenceGrade;
        rows.push_back({ "ODG",
                         juce::String(odg, 1),
                         "Objective Difference Grade (PEAQ-like). ≥ −1.2 transparent, < −1.8 triggers auto-guard.",
                         levelFromBool(odg >= -1.2f, odg >= -1.8f) });
    }
    else if (hasProcessed)
    {
        rows.push_back({ "ODG", "-", "Quality delta not available for this render.", ReadinessLevel::Warn });
    }

    if (advancedMode)
    {
        rows.push_back({ "Streaming Harshness",
                         juce::String(f.psycho.streamingRingingIndex * 100.0f, 0) + "%",
                         "Composite ringing index from bright streaming masters.",
                         levelFromBool(f.psycho.streamingRingingIndex < 0.45f) });
        rows.push_back({ "LRA",
                         juce::String(f.loudnessRangeDb, 1) + " LU",
                         "Loudness range (EBU). Very flat or very wide may need review.",
                         levelFromBool(f.loudnessRangeDb > 2.0f && f.loudnessRangeDb < 20.0f) });
        rows.push_back({ "Wow / Flutter",
                         juce::String(f.wowFlutter.wowPercent, 1) + "% / "
                             + juce::String(f.wowFlutter.flutterPercent, 1) + "%",
                         "Wow/flutter analysis (lo-fi profiles).",
                         ReadinessLevel::Ok });

        if (hasBaseline)
        {
            const auto dLufs = f.integratedLUFS - baselineFeatures.integratedLUFS;
            const auto dTp = f.truePeakDbfs - baselineFeatures.truePeakDbfs;
            const auto dSharp = f.psycho.sharpnessAcum - baselineFeatures.psycho.sharpnessAcum;
            rows.push_back({ "Delta metrics",
                             juce::String(dLufs, 1) + " / " + juce::String(dTp, 1) + " / "
                                 + juce::String(dSharp, 2),
                             "Change in LUFS, true peak, and sharpness after Prepare for Tape.",
                             ReadinessLevel::Ok });
        }
    }
}

juce::Colour ReadinessPanel::colourFor(ReadinessLevel level) const
{
    switch (level)
    {
        case ReadinessLevel::Ok: return ui::Theme::okGreen();
        case ReadinessLevel::Warn: return ui::Theme::warnAmber();
        case ReadinessLevel::Fail: return ui::Theme::failRed();
    }
    return ui::Theme::textSecondary();
}

int ReadinessPanel::rowAtY(int y) const
{
    const int localY = y - 32 - 14 - headerHeight;
    if (localY < 0)
        return -1;
    const int idx = localY / 30;
    return idx >= 0 && idx < static_cast<int>(rows.size()) ? idx : -1;
}

void ReadinessPanel::mouseMove(const juce::MouseEvent& e)
{
    const int idx = rowAtY(e.y);
    if (idx >= 0)
        setTooltip(rows[static_cast<size_t>(idx)].tooltip);
    else
        setTooltip({});
}

void ReadinessPanel::mouseExit(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    setTooltip({});
}

void ReadinessPanel::paint(juce::Graphics& g)
{
    ui::Theme::drawCard(g, getLocalBounds(), "Cassette Readiness");

    auto r = getLocalBounds().reduced(14).withTrimmedTop(32);
    if (rows.empty())
    {
        g.setColour(ui::Theme::textMuted());
        g.setFont(ui::Theme::bodyFont());
        g.drawFittedText("Load audio and run Prepare for Tape to see readiness.",
                         r,
                         juce::Justification::centred,
                         4);
        return;
    }

    const bool ready = score >= 6;
    g.setColour(ready ? ui::Theme::okGreen() : (score >= 4 ? ui::Theme::warnAmber() : ui::Theme::failRed()));
    g.setFont(ui::Theme::scoreFont());
    g.drawText(juce::String(score) + " / " + juce::String(maxScore),
               r.removeFromTop(36),
               juce::Justification::centredLeft);

    g.setFont(ui::Theme::bodyFont());
    g.setColour(ui::Theme::textSecondary());
    g.drawText(ready ? "Ready for tape" : "Review before dubbing",
               r.removeFromTop(20),
               juce::Justification::centredLeft);

    r.removeFromTop(10);
    headerHeight = getHeight() - r.getHeight() - 32 - 14;

    for (auto& row : rows)
    {
        auto rowArea = r.removeFromTop(26);
        row.bounds = rowArea;

        g.setColour(colourFor(row.level));
        g.setFont(ui::Theme::captionFont().boldened());
        g.drawText(row.level == ReadinessLevel::Ok ? "OK" : (row.level == ReadinessLevel::Warn ? "!" : "X"),
                   rowArea.removeFromLeft(22),
                   juce::Justification::centredLeft);

        g.setColour(ui::Theme::textSecondary());
        g.setFont(ui::Theme::captionFont());
        g.drawText(row.name, rowArea.removeFromLeft(120), juce::Justification::centredLeft);

        g.setColour(ui::Theme::textPrimary());
        g.setFont(ui::Theme::metricFont());
        g.drawText(row.value, rowArea, juce::Justification::centredRight);

        r.removeFromTop(4);
    }
}

}
