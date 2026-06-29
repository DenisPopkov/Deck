#include <chrono>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <thread>
#include "../Source/io/PiTapeControlClient.h"
#include "../Source/io/PiTapeTrace.h"
#include "../Source/io/PiTapeUploader.h"
#include "../Source/util/AppLog.h"

namespace
{

using Clock = std::chrono::steady_clock;

double msSince(Clock::time_point t0)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

juce::String argValue(int argc, char** argv, const char* flag, const juce::String& fallback = {})
{
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String(argv[i]) == flag)
            return argv[i + 1];
    return fallback;
}

cassette::PiTapeSettings loadSettings(int argc, char** argv)
{
    cassette::PiTapeSettings settings;
    settings.enabled = true;
    settings.host = argValue(argc, argv, "--host");
    if (settings.host.isEmpty())
        if (const char* envHost = std::getenv("PI_TAPE_HOST"))
            settings.host = envHost;
    settings.port = argValue(argc, argv, "--port", "21").getIntValue();
    settings.controlPort = argValue(argc, argv, "--control-port", "8765").getIntValue();
    settings.username = argValue(argc, argv, "--user", "deck");
    settings.password = argValue(argc, argv, "--password", "deck");
    settings.remoteDir = "inbox";
    return settings;
}

int runStep(const juce::String& name, const std::function<bool()>& fn)
{
    std::cout << "\n=== " << name.toStdString() << " ===\n";
    const auto t0 = Clock::now();
    const bool ok = fn();
    std::cout << (ok ? "OK" : "FAIL") << " in " << msSince(t0) << " ms\n";
    return ok ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    cassette::initLogging();
    std::getenv("DECK_PI_TRACE"); // ensure env read
    if (! cassette::piTapeTraceEnabled())
        std::cout << "Tip: set DECK_PI_TRACE=1 for detailed [pi-tape] timing lines\n";

    const auto settings = loadSettings(argc, argv);
    const auto sideAPath = argValue(argc, argv, "--side-a", "Tests/Fixtures/mini-album/01 Alpha.wav");

    if (! settings.isConfigured())
    {
        std::cerr << "Usage: PiTapeFlowTest --host 192.168.1.119 [--side-a path.wav]\n";
        return 1;
    }

    const juce::File sideA(sideAPath);
    if (! sideA.existsAsFile())
    {
        std::cerr << "Side A file not found: " << sideA.getFullPathName() << '\n';
        return 1;
    }

    cassette::PiTapeControlClient control;
    int failures = 0;

    failures += runStep("cleanup (pre)", [&] {
        return control.cleanupSession(settings).success;
    });

    failures += runStep("FTP upload", [&] {
        cassette::PiTapeQueueSource source;
        source.sideA = sideA;
        source.projectName = "automation-test";
        const auto result = cassette::PiTapeUploader::uploadQueue(settings, source);
        if (! result.success)
            std::cerr << result.error.toStdString() << '\n';
        return result.success;
    });

    failures += runStep("status after upload", [&] {
        const auto status = control.fetchStatus(settings);
        if (! status.online)
        {
            std::cerr << status.error.toStdString() << '\n';
            return false;
        }
        std::cout << "sideA ready=" << (status.sideA.ready ? "yes" : "no")
                  << " duration=" << status.sideA.durationSec << "s\n";
        return status.sideA.ready;
    });

    failures += runStep("play side A", [&] {
        const auto result = control.playSide(settings, "A");
        if (! result.success)
            std::cerr << result.error.toStdString() << " body=" << result.body.toStdString() << '\n';
        return result.success;
    });

    std::cout << "\n=== status poll (10x, simulates UI timer) ===\n";
    double pollTotalMs = 0.0;
    double pollMaxMs = 0.0;
    juce::String lastState;
    for (int i = 0; i < 10; ++i)
    {
        const auto t0 = Clock::now();
        const auto status = control.fetchStatus(settings);
        const double ms = msSince(t0);
        pollTotalMs += ms;
        pollMaxMs = juce::jmax(pollMaxMs, ms);
        lastState = status.playback.state;
        std::cout << "  poll " << (i + 1) << ": " << ms << " ms  state=" << lastState.toStdString()
                  << " pos=" << status.playback.positionSec << "s\n";
        if (status.playback.isPlaying())
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    std::cout << "poll avg=" << (pollTotalMs / 10.0) << " ms max=" << pollMaxMs << " ms\n";

    failures += runStep("stop playback", [&] {
        const auto result = control.stopPlayback(settings);
        if (! result.success)
            std::cerr << result.error.toStdString() << '\n';
        return result.success;
    });

    failures += runStep("status after stop", [&] {
        const auto status = control.fetchStatus(settings);
        std::cout << "state=" << status.playback.state.toStdString()
                  << " pos=" << status.playback.positionSec << "s\n";
        return status.online && (status.playback.isStopped() || status.playback.isStarting()
                                 || status.playback.state.equalsIgnoreCase("idle")
                                 || status.playback.state.equalsIgnoreCase("finished"));
    });

    failures += runStep("cleanup (post)", [&] {
        return control.cleanupSession(settings).success;
    });

    std::cout << "\n=== SUMMARY ===\n";
    std::cout << "failures=" << failures << " last_playback_state=" << lastState.toStdString() << '\n';
    return failures > 0 ? 10 : 0;
}
