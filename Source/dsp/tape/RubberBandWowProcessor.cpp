#include "RubberBandWowProcessor.h"
#include <cmath>
#include <memory>
#include <vector>

#if defined(CASSETTE_HAS_RUBBERBAND)
#include <rubberband/RubberBandStretcher.h>
#endif

namespace cassette
{

#if defined(CASSETTE_HAS_RUBBERBAND)
struct RubberBandWowProcessor::RubberBandState
{
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;
    std::vector<std::vector<float>> channelScratch;
    std::vector<const float*> inPtrs;
    std::vector<float*> outPtrs;
    int blockSize = 512;
};
#endif

RubberBandWowProcessor::RubberBandWowProcessor() = default;

RubberBandWowProcessor::~RubberBandWowProcessor() = default;

void RubberBandWowProcessor::prepare(double sr, int maxBlockSize, int numChannels)
{
    sampleRate = sr;
    maxBlock = juce::jmax(256, maxBlockSize);
    channels = juce::jmax(1, numChannels);

    fallback.prepare(sr, maxBlock, channels);

#if defined(CASSETTE_HAS_RUBBERBAND)
    rb = std::make_unique<RubberBandState>();
    rb->blockSize = juce::jmin(512, maxBlock);
    rb->stretcher = std::make_unique<RubberBand::RubberBandStretcher>(
        static_cast<size_t>(sampleRate),
        static_cast<size_t>(channels),
        RubberBand::RubberBandStretcher::OptionProcessOffline
            | RubberBand::RubberBandStretcher::OptionChannelsTogether);

    rb->stretcher->setMaxProcessSize(static_cast<size_t>(rb->blockSize));
    rb->channelScratch.resize(static_cast<size_t>(channels));
    rb->inPtrs.resize(static_cast<size_t>(channels));
    rb->outPtrs.resize(static_cast<size_t>(channels));

    for (int ch = 0; ch < channels; ++ch)
        rb->channelScratch[static_cast<size_t>(ch)].resize(static_cast<size_t>(rb->blockSize));
#endif
}

void RubberBandWowProcessor::setProfile(const CassetteProfile& profile)
{
    const bool heavy = profile.formulation == TapeFormulation::CheapPortable;
    wowDepth = heavy ? 0.0048f : 0.0032f;
    flutterDepth = heavy ? 0.0018f : 0.0011f;
    wowRateHz = heavy ? 0.55f : 0.72f;
    flutterRateHz = heavy ? 9.8f : 11.5f;
    fallback.setProfile(profile);
}

#if defined(CASSETTE_HAS_RUBBERBAND)
void RubberBandWowProcessor::processRubberBand(juce::AudioBuffer<float>& buffer)
{
    if (rb == nullptr || rb->stretcher == nullptr)
    {
        fallback.process(buffer);
        return;
    }

    const int n = buffer.getNumSamples();
    const int ch = juce::jmin(channels, buffer.getNumChannels());
    juce::AudioBuffer<float> output;
    output.setSize(ch, n);
    output.clear();

    const int block = rb->blockSize;
    int writePos = 0;

    for (int start = 0; start < n; start += block)
    {
        const int thisBlock = juce::jmin(block, n - start);
        const bool isFinal = start + thisBlock >= n;

        const float wow = std::sin(wowPhase);
        const float flutter = std::sin(flutterPhase);
        const double pitch = 1.0 + static_cast<double>(wow) * wowDepth + static_cast<double>(flutter) * flutterDepth;

        wowPhase += juce::MathConstants<float>::twoPi * wowRateHz / static_cast<float>(sampleRate)
                    * static_cast<float>(thisBlock);
        flutterPhase += juce::MathConstants<float>::twoPi * flutterRateHz / static_cast<float>(sampleRate)
                        * static_cast<float>(thisBlock);

        rb->stretcher->reset();
        rb->stretcher->setPitchScale(pitch);
        rb->stretcher->setTimeRatio(1.0);

        for (int c = 0; c < ch; ++c)
        {
            auto& scratch = rb->channelScratch[static_cast<size_t>(c)];
            if (static_cast<int>(scratch.size()) < thisBlock)
                scratch.resize(static_cast<size_t>(thisBlock));

            std::copy(buffer.getReadPointer(c) + start,
                      buffer.getReadPointer(c) + start + thisBlock,
                      scratch.begin());
            rb->inPtrs[static_cast<size_t>(c)] = scratch.data();
            rb->outPtrs[static_cast<size_t>(c)] = scratch.data();
        }

        rb->stretcher->process(rb->inPtrs.data(), static_cast<size_t>(thisBlock), isFinal);

        const int avail = rb->stretcher->available();
        const int retrieveN = juce::jmin(thisBlock, avail, n - writePos);
        if (retrieveN > 0)
        {
            rb->stretcher->retrieve(rb->outPtrs.data(), static_cast<size_t>(retrieveN));
            for (int c = 0; c < ch; ++c)
                output.copyFrom(c, writePos, rb->channelScratch[static_cast<size_t>(c)].data(), retrieveN);
            writePos += retrieveN;
        }

        if (writePos >= n)
            break;
    }

    if (writePos < n)
        fallback.process(buffer);
    else
        for (int c = 0; c < ch; ++c)
            buffer.copyFrom(c, 0, output, c, 0, n);
}
#endif

void RubberBandWowProcessor::process(juce::AudioBuffer<float>& buffer)
{
#if defined(CASSETTE_HAS_RUBBERBAND)
    processRubberBand(buffer);
#else
    fallback.process(buffer);
#endif
}

}
