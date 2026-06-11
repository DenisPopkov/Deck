#include "TapeAwareSoftClipper.h"
#include "StnGridModel.h"
#include "OnnxStnRunner.h"

namespace cassette
{

namespace
{

OnnxStnRunner& sharedOnnxRunner()
{
    static OnnxStnRunner* runner = new OnnxStnRunner();
    static bool attempted = false;
    if (!attempted)
    {
        attempted = true;
        runner->tryLoadDefaultModel();
    }
    return *runner;
}

}

void TapeAwareSoftClipper::prepare(double sr, int oversamplingFactor)
{
    sampleRate = sr;
    osFactor = oversamplingFactor;
    state = 0.0f;
}

void TapeAwareSoftClipper::setProfile(const CassetteProfile& p) { profile = p; }

float TapeAwareSoftClipper::processSampleStn(float x, float drive) const
{
    const float dx = x - state;
    const float hidden = std::tanh(dx * 2.5f + state * 0.35f);
    return std::tanh((x + hidden * 0.2f) * drive);
}

float TapeAwareSoftClipper::tanhSaturation(float x, float drive) const
{
    return std::tanh(x * drive);
}

void TapeAwareSoftClipper::process(juce::AudioBuffer<float>& buffer)
{
#if defined(CASSETTE_HAS_ONNX)
    auto& onnx = sharedOnnxRunner();
    if (onnx.isLoaded())
    {
        onnx.process(buffer, profile, sampleRate);
        return;
    }
#endif

    StnGridModel grid;
    grid.reset();
    grid.process(buffer, profile, sampleRate);
}

}
