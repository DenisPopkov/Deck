#include "PiTapeControlClient.h"
#include "PiTapeTrace.h"

namespace cassette
{

namespace
{

constexpr int kConnectTimeoutMs = 4000;
constexpr int kReadTimeoutMs = 8000;

double elapsedMs(double t0)
{
    return juce::Time::getMillisecondCounterHiRes() - t0;
}

} // namespace

PiTapeHttpResult PiTapeControlClient::request(const PiTapeSettings& settings,
                                              const juce::String& method,
                                              const juce::String& path,
                                              const juce::String& body) const
{
    PiTapeHttpResult result;
    const PiTapeScopedTimer totalTimer("http", method + " " + path);

    if (!settings.isConfigured())
    {
        result.error = "Pi not configured";
        return result;
    }

    double t0 = juce::Time::getMillisecondCounterHiRes();
    juce::StreamingSocket socket;
    if (!socket.connect(settings.host, settings.controlPort, kConnectTimeoutMs))
    {
        result.error = "Could not connect to Pi control port";
        piTapeLog("http connect FAILED " + settings.host + ":" + juce::String(settings.controlPort));
        return result;
    }
    piTapeLogTiming("http/connect", elapsedMs(t0), settings.host + ":" + juce::String(settings.controlPort));

    juce::String requestLine = method + " " + path + " HTTP/1.1\r\n";
    requestLine += "Host: " + settings.host + "\r\n";
    requestLine += "Connection: close\r\n";
    if (body.isNotEmpty())
    {
        requestLine += "Content-Type: application/json\r\n";
        requestLine += "Content-Length: " + juce::String(body.getNumBytesAsUTF8()) + "\r\n";
    }
    requestLine += "\r\n";
    requestLine += body;

    t0 = juce::Time::getMillisecondCounterHiRes();
    if (!socket.write(requestLine.toRawUTF8(), requestLine.getNumBytesAsUTF8()))
    {
        result.error = "HTTP write failed";
        return result;
    }
    piTapeLogTiming("http/write", elapsedMs(t0), juce::String(requestLine.getNumBytesAsUTF8()) + " bytes");

    juce::MemoryBlock buffer;
    const double readStart = juce::Time::getMillisecondCounterHiRes();
    const double deadline = readStart + static_cast<double>(kReadTimeoutMs);
    double firstByteMs = -1.0;

    const auto responseComplete = [&buffer]() -> bool {
        if (buffer.getSize() < 4)
            return false;

        const juce::String response(static_cast<const char*>(buffer.getData()), static_cast<int>(buffer.getSize()));
        const int headerEnd = response.indexOf("\r\n\r\n");
        if (headerEnd < 0)
            return false;

        const auto header = response.substring(0, headerEnd).toLowerCase();
        const int contentLength = [&header]() {
            const int idx = header.indexOf("content-length:");
            if (idx < 0)
                return -1;
            return header.substring(idx + 15).upToFirstOccurrenceOf("\r\n", false, false).trim().getIntValue();
        }();

        const int bodyBytes = static_cast<int>(buffer.getSize()) - (headerEnd + 4);
        if (contentLength >= 0)
            return bodyBytes >= contentLength;

        return header.contains("connection: close");
    };

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        if (responseComplete())
            break;

        char chunk[1024];
        const int read = socket.read(chunk, sizeof(chunk), false);
        if (read > 0)
        {
            if (firstByteMs < 0.0)
                firstByteMs = elapsedMs(readStart);
            buffer.append(chunk, static_cast<size_t>(read));
            if (responseComplete())
                break;
        }
        else if (read < 0)
        {
            break;
        }
        else if (buffer.getSize() > 0)
        {
            juce::Thread::sleep(2);
        }
        else
        {
            juce::Thread::sleep(5);
        }
    }

    socket.close();
    piTapeLogTiming("http/read", elapsedMs(readStart),
                    juce::String(static_cast<int>(buffer.getSize())) + " bytes"
                        + (firstByteMs >= 0.0 ? ", TTFB " + juce::String(firstByteMs, 1) + " ms" : ", no data"));

    if (buffer.isEmpty())
    {
        result.error = "Empty HTTP response";
        return result;
    }

    const juce::String response(static_cast<const char*>(buffer.getData()), static_cast<int>(buffer.getSize()));
    const int headerEnd = response.indexOf("\r\n\r\n");
    if (headerEnd < 0)
    {
        result.error = "Malformed HTTP response";
        return result;
    }

    const auto header = response.substring(0, headerEnd);
    const auto statusLine = header.upToFirstOccurrenceOf("\r\n", false, false);
    if (statusLine.length() >= 12)
        result.statusCode = statusLine.substring(9, 12).getIntValue();

    auto bodyText = response.substring(headerEnd + 4);
    const int contentLength = [&header]() {
        const juce::String lower = header.toLowerCase();
        const int idx = lower.indexOf("content-length:");
        if (idx < 0)
            return -1;
        return lower.substring(idx + 15).upToFirstOccurrenceOf("\r\n", false, false).trim().getIntValue();
    }();

    if (contentLength >= 0 && bodyText.getNumBytesAsUTF8() > contentLength)
        bodyText = bodyText.substring(0, contentLength);

    result.body = bodyText.trim();
    result.success = result.statusCode >= 200 && result.statusCode < 300;
    if (!result.success && result.error.isEmpty())
        result.error = "HTTP " + juce::String(result.statusCode);

    piTapeLog("http response " + juce::String(result.statusCode) + " body "
              + juce::String(result.body.getNumBytesAsUTF8()) + " bytes");
    if (! result.success)
        piTapeLog("http error: " + result.error);

    return result;
}

PiTapeRemoteStatus PiTapeControlClient::fetchStatus(const PiTapeSettings& settings) const
{
    const auto http = request(settings, "GET", "/api/status", {});
    if (!http.success)
    {
        PiTapeRemoteStatus offline;
        offline.error = http.error.isNotEmpty() ? http.error : "Pi offline";
        return offline;
    }

    return PiTapeRemoteStatus::fromJson(http.body);
}

PiTapeHttpResult PiTapeControlClient::playSide(const PiTapeSettings& settings,
                                               const juce::String& sideLabel,
                                               double offsetSec) const
{
    auto* o = new juce::DynamicObject();
    o->setProperty("side", sideLabel.equalsIgnoreCase("B") ? "B" : "A");
    if (offsetSec >= 0.0)
        o->setProperty("offset_sec", offsetSec);
    const auto body = juce::JSON::toString(juce::var(o), false);
    piTapeLog("play side " + sideLabel + (offsetSec >= 0.0 ? " offset " + juce::String(offsetSec, 1) + "s" : ""));
    return request(settings, "POST", "/api/play", body);
}

PiTapeHttpResult PiTapeControlClient::stopPlayback(const PiTapeSettings& settings) const
{
    piTapeLog("stop playback");
    return request(settings, "POST", "/api/stop", {});
}

PiTapeHttpResult PiTapeControlClient::cleanupSession(const PiTapeSettings& settings) const
{
    piTapeLog("cleanup session");
    return request(settings, "POST", "/api/cleanup", {});
}

} // namespace cassette
