#pragma once

#include <functional>
#include <juce_core/juce_core.h>

namespace cassette
{

struct FtpUploadProgress
{
    juce::int64 bytesSent = 0;
    juce::int64 totalBytes = 0;
};

struct FtpUploadResult
{
    bool success = false;
    juce::String error;
};

class FtpClient
{
public:
    using ProgressCallback = std::function<void(const FtpUploadProgress&)>;

    FtpUploadResult uploadFile(const juce::String& host,
                               int port,
                               const juce::String& username,
                               const juce::String& password,
                               const juce::File& localFile,
                               const juce::String& remotePath,
                               ProgressCallback onProgress = {}) const;

private:
    static juce::String normaliseRemotePath(const juce::String& path);
    static bool parsePassiveEndpoint(const juce::String& response, juce::String& outHost, int& outPort);

    bool readResponse(juce::StreamingSocket& socket, int& code, juce::String& message) const;
    bool sendCommand(juce::StreamingSocket& socket, const juce::String& command, int expectCode) const;
    bool sendCommandExpectRange(juce::StreamingSocket& socket, const juce::String& command, int minCode, int maxCode, juce::String* responseOut = nullptr) const;
};

} // namespace cassette
