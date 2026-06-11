#include "MeterPanel.h"

namespace cassette
{
namespace
{
int readinessScore(const AudioFeatures& f, const CassetteProfile& profile)
{
    int score = 0;
    if (f.integratedLUFS >= -17.0f && f.integratedLUFS <= -10.0f)
        ++score;
    if (f.truePeakDbfs <= profile.truePeakCeilingDbfs + 0.5f)
        ++score;
    if (f.psycho.hfAboveMaskingDb < 6.0f && f.psycho.sharpnessAcum < 2.2f && f.psycho.roughnessAsper < 0.35f)
        ++score;
    if (f.lfStereoCorrelation > 0.3f)
        ++score;
    if (f.crestFactorDb > 6.0f)
        ++score;
    if (f.clippingPercent < 0.01f)
        ++score;
    if (f.loudnessRangeDb > 2.0f && f.loudnessRangeDb < 20.0f)
        ++score;
    if (f.noiseFloorDbfs < -50.0f)
        ++score;
    return score;
}

void drawBar(juce::Graphics& g,
             juce::Rectangle<float> area,
             float proportion,
             juce::Colour fill,
             juce::Colour track)
{
    proportion = juce::jlimit(0.0f, 1.0f, proportion);
    g.setColour(track);
    g.fillRoundedRectangle(area, 3.0f);
    if (proportion > 0.001f)
    {
        auto fillArea = area.withWidth(area.getWidth() * proportion);
        g.setColour(fill);
        g.fillRoundedRectangle(fillArea, 3.0f);
    }
}

void drawMetric(juce::Graphics& g,
                juce::Rectangle<int> area,
                const juce::String& label,
                const juce::String& value,
                juce::Colour accent,
                float bar01 = -1.0f)
{
    auto r = area.reduced(2);
    g.setColour(juce::Colour(0xff1e1e1e));
    g.fillRoundedRectangle(r.toFloat(), 6.0f);

    auto inner = r.reduced(10, 8);
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText(label, inner.removeFromTop(16), juce::Justification::centredLeft);

    g.setColour(accent);
    g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    g.drawText(value, inner.removeFromTop(26), juce::Justification::centredLeft);

    if (bar01 >= 0.0f)
        drawBar(g, inner.removeFromTop(8).toFloat(), bar01, accent, juce::Colours::white.withAlpha(0.1f));
}
}

MeterPanel::MeterPanel()
{
    initAudioDropForwarding(this);
}

void MeterPanel::clearFeatures()
{
    hasFeatures = false;
    hasBaseline = false;
    repaint();
}

void MeterPanel::setBaselineFeatures(const AudioFeatures& baseline)
{
    baselineFeatures = baseline;
    hasBaseline = true;
    repaint();
}

void MeterPanel::clearBaselineFeatures()
{
    hasBaseline = false;
    repaint();
}

void MeterPanel::setFeatures(const AudioFeatures& f, const juce::String& title, TapeFormulation tape)
{
    features = f;
    panelTitle = title;
    tapeType = tape;
    hasFeatures = true;
    repaint();
}

void MeterPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(2);
    g.setColour(juce::Colour(0xff161616));
    g.fillRoundedRectangle(bounds.toFloat(), 10.0f);
    g.setColour(juce::Colours::white.withAlpha(0.12f));
    g.drawRoundedRectangle(bounds.toFloat(), 10.0f, 1.0f);

    auto r = bounds.reduced(14);
    g.setColour(juce::Colours::white.withAlpha(0.92f));
    g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    g.drawText(panelTitle, r.removeFromTop(24), juce::Justification::centredLeft);

    r.removeFromTop(8);

    if (!hasFeatures)
    {
        g.setColour(juce::Colours::white.withAlpha(0.45f));
        g.setFont(juce::Font(juce::FontOptions(13.0f)));
        g.drawFittedText(
            "Load a file and run Process to see loudness, peaks, and cassette readiness.",
            r,
            juce::Justification::centred,
            4);
        return;
    }

    const auto profile = CassetteProfile::fromFormulation(tapeType);
    const int score = readinessScore(features, profile);
    const int maxScore = 8;
    const float lufsBar = juce::jlimit(0.0f, 1.0f, (features.integratedLUFS + 24.0f) / 18.0f);
    const float tpBar = juce::jlimit(0.0f, 1.0f, (features.truePeakDbfs + 12.0f) / 12.0f);
    const float readyBar = static_cast<float>(score) / static_cast<float>(maxScore);

    drawMetric(g,
               r.removeFromTop(72),
               "Integrated LUFS",
               juce::String(features.integratedLUFS, 1) + " LUFS",
               juce::Colour(0xff50e6ff),
               lufsBar);
    r.removeFromTop(6);

    drawMetric(g,
               r.removeFromTop(72),
               "True peak (4×)",
               juce::String(features.truePeakDbfs, 1) + " dBFS",
               juce::Colour(0xffff8a50),
               tpBar);
    r.removeFromTop(6);

    drawMetric(g,
               r.removeFromTop(58),
               "Loudness range",
               juce::String(features.loudnessRangeDb, 1) + " LU",
               juce::Colours::white.withAlpha(0.85f));
    r.removeFromTop(6);

    drawMetric(g,
               r.removeFromTop(72),
               "Cassette readiness",
               juce::String(score) + " / " + juce::String(maxScore),
               score >= 6 ? juce::Colour(0xff5fd38d) : (score >= 4 ? juce::Colour(0xffffc857) : juce::Colour(0xffff6b6b)),
               readyBar);

    r.removeFromTop(4);
    g.setColour(juce::Colours::white.withAlpha(0.4f));
    g.setFont(juce::Font(juce::FontOptions(10.5f)));

    juce::String footer = "Sharpness " + juce::String(features.psycho.sharpnessAcum, 1) + " acum  |  Rough "
                              + juce::String(features.psycho.roughnessAsper, 2) + " asper  |  Ring "
                              + juce::String(features.psycho.streamingRingingIndex * 100.0f, 0) + "%";

    if (hasBaseline)
    {
        const auto dLufs = features.integratedLUFS - baselineFeatures.integratedLUFS;
        const auto dTp = features.truePeakDbfs - baselineFeatures.truePeakDbfs;
        const auto dSharp = features.psycho.sharpnessAcum - baselineFeatures.psycho.sharpnessAcum;
        footer += "\nΔ LUFS " + juce::String(dLufs, 1) + "  |  Δ TP " + juce::String(dTp, 1) + " dB  |  Δ sharp "
                  + juce::String(dSharp, 2);
    }

    footer += "\nWow " + juce::String(features.wowFlutter.wowPercent, 1) + "%  |  Flutter "
              + juce::String(features.wowFlutter.flutterPercent, 1) + "%";
    if (features.wowFlutter.hasTestTone3150)
        footer += "  |  3150 Hz tone";

    g.drawFittedText(footer, r.removeFromTop(hasBaseline ? 52 : 40), juce::Justification::centredLeft, 4);
}

void MeterPanel::resized()
{
}

}
