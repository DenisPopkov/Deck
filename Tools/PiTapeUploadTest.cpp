#include <chrono>
#include <cstdlib>
#include <iostream>
#include "../Source/io/PiTapeControlClient.h"
#include "../Source/io/PiTapeUploader.h"

namespace
{

juce::String argValue(int argc, char** argv, const char* flag, const juce::String& fallback = {})
{
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String(argv[i]) == flag)
            return argv[i + 1];
    return fallback;
}

bool hasFlag(int argc, char** argv, const char* flag)
{
    for (int i = 1; i < argc; ++i)
        if (juce::String(argv[i]) == flag)
            return true;
    return false;
}

void printUsage()
{
    std::cout << "PiTapeUploadTest — FTP upload + Pi status check\n\n"
              << "Usage:\n"
              << "  PiTapeUploadTest --host 192.168.1.119 --side-a path.wav [--side-b path.wav]\n"
              << "                   [--user deck] [--password deck] [--port 21] [--control-port 8765]\n\n"
              << "Set PI_TAPE_HOST to skip --host.\n";
}

} // namespace

int main(int argc, char** argv)
{
    if (hasFlag(argc, argv, "--help") || hasFlag(argc, argv, "-h"))
    {
        printUsage();
        return 0;
    }

    cassette::PiTapeSettings settings;
    settings.enabled = true;
    settings.host = argValue(argc, argv, "--host");
    if (settings.host.isEmpty())
    {
        if (const char* envHost = std::getenv("PI_TAPE_HOST"))
            settings.host = envHost;
    }
    settings.port = argValue(argc, argv, "--port", "21").getIntValue();
    settings.controlPort = argValue(argc, argv, "--control-port", "8765").getIntValue();
    settings.username = argValue(argc, argv, "--user", "deck");
    settings.password = argValue(argc, argv, "--password", "deck");
    settings.remoteDir = "inbox";

    const auto sideAPath = argValue(argc, argv, "--side-a");
    const auto sideBPath = argValue(argc, argv, "--side-b");

    if (!settings.isConfigured() || sideAPath.isEmpty())
    {
        printUsage();
        return 1;
    }

    const juce::File sideA(sideAPath);
    if (!sideA.existsAsFile())
    {
        std::cerr << "Side A file not found: " << sideAPath << '\n';
        return 1;
    }

    cassette::PiTapeQueueSource source;
    source.sideA = sideA;
    source.projectName = sideA.getFileNameWithoutExtension();
    if (sideBPath.isNotEmpty())
    {
        source.sideB = juce::File(sideBPath);
        source.hasSideB = source.sideB.existsAsFile();
        if (!source.hasSideB)
            std::cerr << "Warning: Side B missing, uploading Side A only\n";
    }

    std::cout << "Host: " << settings.host.toStdString() << '\n';
    std::cout << "Side A: " << sideA.getFullPathName() << " (" << sideA.getSize() << " bytes)\n";
    if (source.hasSideB)
        std::cout << "Side B: " << source.sideB.getFullPathName() << " (" << source.sideB.getSize() << " bytes)\n";

    cassette::PiTapeControlClient control;
    const auto before = control.fetchStatus(settings);
    if (!before.online)
    {
        std::cerr << "Pi control offline: " << before.error.toStdString() << '\n';
        return 2;
    }
    std::cout << "Pi control: online\n";

    const auto started = std::chrono::steady_clock::now();
    const auto result = cassette::PiTapeUploader::uploadQueue(settings, source, [](const cassette::PiTapeUploadProgress& progress) {
        const char* stage = "Side A";
        if (progress.stage == cassette::PiTapeUploadProgress::Stage::SideB)
            stage = "Side B";
        else if (progress.stage == cassette::PiTapeUploadProgress::Stage::Queue)
            stage = "Queue";

        if (progress.totalBytes > 0)
        {
            const int pct = static_cast<int>(100.0 * progress.bytesSent / progress.totalBytes);
            std::cout << "\r" << stage << ": " << pct << "% (" << progress.bytesSent << "/" << progress.totalBytes << ")"
                      << std::flush;
        }
        else
        {
            std::cout << "\r" << stage << "..." << std::flush;
        }
    });

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    std::cout << "\nUpload " << (result.success ? "OK" : "FAILED") << " in " << elapsed.count() << " ms\n";
    if (!result.success)
    {
        std::cerr << result.error.toStdString() << '\n';
        return 3;
    }

    const auto after = control.fetchStatus(settings);
    if (!after.online)
    {
        std::cerr << "Status check failed after upload: " << after.error.toStdString() << '\n';
        return 4;
    }

    std::cout << "Side A ready: " << (after.sideA.ready ? "yes" : "no")
              << "  Side B ready: " << (after.sideB.ready ? "yes" : "no") << '\n';
    std::cout << "Job: " << result.jobId.toStdString() << '\n';
    return after.sideA.ready ? 0 : 5;
}
