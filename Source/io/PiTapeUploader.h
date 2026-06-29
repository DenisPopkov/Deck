#pragma once

#include "FtpClient.h"
#include "PiTapeSettings.h"
#include <functional>

namespace cassette
{

struct PiTapeUploadResult
{
    bool success = false;
    juce::String error;
    juce::String jobId;
};

struct PiTapeUploadProgress
{
    enum class Stage { SideA, SideB, Queue };

    Stage stage = Stage::SideA;
    juce::int64 bytesSent = 0;
    juce::int64 totalBytes = 0;
};

struct PiTapeQueueSource
{
    juce::File sideA;
    juce::File sideB;
    juce::String projectName;
    bool hasSideB = false;
};

class PiTapeUploader
{
public:
    static PiTapeUploadResult uploadPreparedSide(const PiTapeSettings& settings,
                                                 const juce::File& wavFile,
                                                 const juce::String& sideLabel);

    static PiTapeUploadResult uploadQueue(const PiTapeSettings& settings,
                                          const PiTapeQueueSource& source,
                                          std::function<void(const PiTapeUploadProgress&)> onProgress = {});

private:
    static juce::String buildManifestJson(const juce::String& jobId,
                                          const juce::String& remoteWavPath,
                                          const juce::String& sideLabel);

    static juce::String buildQueueJson(const juce::String& jobId,
                                       const PiTapeQueueSource& source,
                                       const juce::String& remoteSideA,
                                       const juce::String& remoteSideB);

    static PiTapeUploadResult uploadTextFile(const PiTapeSettings& settings,
                                             const juce::String& remotePath,
                                             const juce::String& text,
                                             std::function<void(const PiTapeUploadProgress&)> onProgress = {});
};

} // namespace cassette
