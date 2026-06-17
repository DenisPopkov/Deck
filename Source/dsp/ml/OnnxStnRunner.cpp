#include "OnnxStnRunner.h"
#include "StnGridModel.h"

#if defined(CASSETTE_HAS_ONNX)
#include <onnxruntime_cxx_api.h>
#endif

#include <array>
#include <cstdlib>
#include <vector>

namespace cassette
{

namespace
{
constexpr int kOnnxBlockSamples = 8192;
}

#if defined(CASSETTE_HAS_ONNX)
struct OnnxStnRunner::Impl
{
    Ort::Env env { ORT_LOGGING_LEVEL_WARNING, "Deck" };
    Ort::SessionOptions options;
    std::unique_ptr<Ort::Session> session;
    std::vector<std::string> nameStorage;
    std::vector<const char*> inputNames;
    std::vector<const char*> outputNames;
};
#endif

OnnxStnRunner::OnnxStnRunner() = default;

OnnxStnRunner::~OnnxStnRunner()
{
#if defined(CASSETTE_HAS_ONNX)
    delete impl;
    impl = nullptr;
#endif
}

void OnnxStnRunner::reset()
{
    stateL = stateR = 0.0f;
}

float OnnxStnRunner::driveNorm(const CassetteProfile& profile)
{
    return juce::jlimit(0.0f, 1.0f, (profile.saturationDrive - 0.85f) / 0.45f);
}

float OnnxStnRunner::biasNorm(const CassetteProfile& profile)
{
    return juce::jlimit(0.0f, 1.0f, profile.biasReductionOnHf / 0.12f);
}

juce::File OnnxStnRunner::resolveModelPath() const
{
    juce::Array<juce::File> candidates;
    const auto app = juce::File::getSpecialLocation(juce::File::currentApplicationFile);

    candidates.add(app.getChildFile("Contents/Resources/models/tape_stn.onnx"));
    candidates.add(app.getParentDirectory().getChildFile("models/tape_stn.onnx"));
    candidates.add(juce::File(juce::File::getCurrentWorkingDirectory()).getChildFile("models/tape_stn.onnx"));

    if (const char* envPath = std::getenv("CASSETTE_STN_MODEL"))
        candidates.add(juce::File(envPath));

    for (const auto& c : candidates)
        if (c.existsAsFile())
            return c;

    return {};
}

bool OnnxStnRunner::tryLoadDefaultModel()
{
#if defined(CASSETTE_HAS_ONNX)
    const auto modelPath = resolveModelPath();
    if (!modelPath.existsAsFile())
        return false;

    try
    {
        impl = new Impl();
        impl->options.SetIntraOpNumThreads(1);
        impl->options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

#ifdef _WIN32
        const std::wstring wpath(modelPath.getFullPathName().toWideCharPointer());
        impl->session = std::make_unique<Ort::Session>(impl->env, wpath.c_str(), impl->options);
#else
        const auto path = modelPath.getFullPathName().toStdString();
        impl->session = std::make_unique<Ort::Session>(impl->env, path.c_str(), impl->options);
#endif

        Ort::AllocatorWithDefaultOptions allocator;
        const size_t numInputs = impl->session->GetInputCount();
        const size_t numOutputs = impl->session->GetOutputCount();

        impl->nameStorage.clear();
        impl->inputNames.clear();
        impl->outputNames.clear();
        impl->nameStorage.reserve(numInputs + numOutputs);

        for (size_t i = 0; i < numInputs; ++i)
            impl->nameStorage.emplace_back(impl->session->GetInputNameAllocated(i, allocator).get());
        for (size_t i = 0; i < numOutputs; ++i)
            impl->nameStorage.emplace_back(impl->session->GetOutputNameAllocated(i, allocator).get());

        for (size_t i = 0; i < numInputs; ++i)
            impl->inputNames.push_back(impl->nameStorage[i].c_str());
        for (size_t i = 0; i < numOutputs; ++i)
            impl->outputNames.push_back(impl->nameStorage[numInputs + i].c_str());

        profileConditioned = numInputs >= 4;
        loaded = true;
        return true;
    }
    catch (...)
    {
        delete impl;
        impl = nullptr;
        loaded = false;
        profileConditioned = false;
        return false;
    }
#else
    return false;
#endif
}

void OnnxStnRunner::process(juce::AudioBuffer<float>& buffer, const CassetteProfile& profile, double sampleRate)
{
#if defined(CASSETTE_HAS_ONNX)
    if (!loaded || impl == nullptr || impl->session == nullptr)
    {
        StnGridModel grid;
        grid.reset();
        grid.process(buffer, profile, sampleRate);
        return;
    }

    const int n = buffer.getNumSamples();
    const int ch = juce::jmin(2, buffer.getNumChannels());
    if (n == 0 || ch == 0)
        return;

    const float drive = driveNorm(profile);
    const float bias = biasNorm(profile);
    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    for (int offset = 0; offset < n; offset += kOnnxBlockSamples)
    {
        const int blockN = juce::jmin(kOnnxBlockSamples, n - offset);

        std::vector<float> input(static_cast<size_t>(ch * blockN));
        for (int c = 0; c < ch; ++c)
            for (int i = 0; i < blockN; ++i)
                input[static_cast<size_t>(c * blockN + i)] = buffer.getSample(c, offset + i);

        const std::array<int64_t, 3> audioShape { 1, ch, blockN };
        Ort::Value audioTensor = Ort::Value::CreateTensor<float>(memInfo,
                                                                   input.data(),
                                                                   input.size(),
                                                                   audioShape.data(),
                                                                   audioShape.size());

        std::vector<Ort::Value> inputTensors;
        inputTensors.push_back(std::move(audioTensor));

        std::array<float, 1> driveData { drive };
        std::array<float, 1> biasData { bias };
        std::array<float, 2> stateData { stateL, stateR };

        if (profileConditioned)
        {
            const std::array<int64_t, 1> scalarShape { 1 };
            const std::array<int64_t, 2> stateShape { 1, 2 };

            inputTensors.push_back(Ort::Value::CreateTensor<float>(memInfo,
                                                                    driveData.data(),
                                                                    driveData.size(),
                                                                    scalarShape.data(),
                                                                    scalarShape.size()));
            inputTensors.push_back(Ort::Value::CreateTensor<float>(memInfo,
                                                                    biasData.data(),
                                                                    biasData.size(),
                                                                    scalarShape.data(),
                                                                    scalarShape.size()));
            inputTensors.push_back(Ort::Value::CreateTensor<float>(memInfo,
                                                                    stateData.data(),
                                                                    stateData.size(),
                                                                    stateShape.data(),
                                                                    stateShape.size()));
        }

        auto outputTensors = impl->session->Run(Ort::RunOptions { nullptr },
                                                impl->inputNames.data(),
                                                inputTensors.data(),
                                                inputTensors.size(),
                                                impl->outputNames.data(),
                                                impl->outputNames.size());

        const float* outData = outputTensors[0].GetTensorData<float>();
        for (int c = 0; c < ch; ++c)
            for (int i = 0; i < blockN; ++i)
                buffer.setSample(c, offset + i, outData[c * blockN + i]);

        if (profileConditioned && outputTensors.size() > 1)
        {
            const float* stateOut = outputTensors[1].GetTensorData<float>();
            stateL = stateOut[0];
            stateR = stateOut[1];
        }
    }
#else
    StnGridModel grid;
    grid.reset();
    grid.process(buffer, profile, sampleRate);
    juce::ignoreUnused(profile);
#endif
    juce::ignoreUnused(sampleRate);
}

}