#include "CassetteTapePrepProcessor.h"
#include "CassetteTapePrepEditor.h"

namespace cassette
{
namespace
{
constexpr int kAnalysisIntervalSamples = 48000;

TapeFormulation profileFromIndex(int idx)
{
    switch (idx)
    {
        case 0: return TapeFormulation::TypeI;
        case 1: return TapeFormulation::TypeII;
        case 2: return TapeFormulation::TypeIV;
        case 3: return TapeFormulation::HighEndDeck;
        case 4: return TapeFormulation::VintageWalkman;
        case 5: return TapeFormulation::CheapPortable;
        default: return TapeFormulation::TypeII;
    }
}
}

CassetteTapePrepProcessor::CassetteTapePrepProcessor()
    : apvts(*this,
            nullptr,
            "Parameters",
            { std::make_unique<juce::AudioParameterChoice>(
                  "profile",
                  "Tape profile",
                  juce::StringArray { "Type I", "Type II", "Type IV", "High-end Deck", "Walkman", "Cheap Portable" },
                  2),
              std::make_unique<juce::AudioParameterFloat>("hfSafety",
                                                          "HF Safety",
                                                          juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
                                                          0.75f),
              std::make_unique<juce::AudioParameterBool>("cleanTransfer", "Clean Transfer", false),
              std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false) })
{
    features.integratedLUFS = -12.0f;
    features.truePeakDbfs = -1.0f;
    features.lfStereoCorrelation = 0.8f;
    features.noiseFloorDbfs = -60.0f;
    features.psycho.sharpnessAcum = 1.4f;
    features.psycho.roughnessAsper = 0.12f;
    features.psycho.hfAboveMaskingDb = 3.0f;
    features.psycho.hfTamerStrength = 0.35f;
}

void CassetteTapePrepProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    analysisCooldownSamples = 0;
    mastering.prepare(sampleRate, samplesPerBlock);
}

void CassetteTapePrepProcessor::releaseResources() {}

CassetteProfile CassetteTapePrepProcessor::currentProfile() const
{
    const int idx = static_cast<int>(*apvts.getRawParameterValue("profile"));
    return CassetteProfile::fromFormulation(profileFromIndex(idx));
}

MasteringOptions CassetteTapePrepProcessor::currentOptions() const
{
    MasteringOptions opts;
    opts.maximumDigital = *apvts.getRawParameterValue("cleanTransfer") >= 0.5f;
    opts.hfTamerIntensity = *apvts.getRawParameterValue("hfSafety");
    opts.perceptualAutoFallback = false;
    return opts;
}

void CassetteTapePrepProcessor::refreshFeatures(const juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() < 512)
        return;

    features = EssentiaAnalyzer::extractFeatures(buffer, sampleRateHz);
}

void CassetteTapePrepProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;

    if (*apvts.getRawParameterValue("bypass") >= 0.5f)
        return;

    const bool offline = isNonRealtime() || buffer.getNumSamples() > 4096;
    if (offline)
        refreshFeatures(buffer);
    else
    {
        analysisCooldownSamples -= buffer.getNumSamples();
        if (analysisCooldownSamples <= 0)
        {
            refreshFeatures(buffer);
            analysisCooldownSamples = kAnalysisIntervalSamples;
        }
    }

    mastering.processTrack(buffer, currentProfile(), features, currentOptions());
}

juce::AudioProcessorEditor* CassetteTapePrepProcessor::createEditor()
{
    return new CassetteTapePrepEditor(*this);
}

void CassetteTapePrepProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState().createXml())
        copyXmlToBinary(*state, destData);
}

void CassetteTapePrepProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new cassette::CassetteTapePrepProcessor();
}
