#include "FtpClient.h"
#include "PiTapeTrace.h"

namespace cassette
{

namespace
{

constexpr int kConnectTimeoutMs = 8000;
constexpr int kIoTimeoutMs = 120000;

juce::String ftpPathJoin(const juce::String& dir, const juce::String& name)
{
    auto base = dir.trim();
    while (base.endsWithChar('/'))
        base = base.dropLastCharacters(1);
    return base.isEmpty() ? name : base + "/" + name;
}

double elapsedMs(double t0)
{
    return juce::Time::getMillisecondCounterHiRes() - t0;
}

void logFtpPhase(const juce::String& phase, double t0, const juce::String& detail = {})
{
    piTapeLogTiming("ftp/" + phase, elapsedMs(t0), detail);
}

} // namespace

juce::String FtpClient::normaliseRemotePath(const juce::String& path)
{
    auto p = path.trim();
    while (p.startsWithChar('/'))
        p = p.substring(1);
    return p.replaceCharacter('\\', '/');
}

bool FtpClient::parsePassiveEndpoint(const juce::String& response, juce::String& outHost, int& outPort)
{
    const int open = response.indexOfChar('(');
    const int close = response.indexOfChar(')');
    if (open < 0 || close <= open)
        return false;

    juce::StringArray parts;
    parts.addTokens(response.substring(open + 1, close), ",", "");
    if (parts.size() < 6)
        return false;

    outHost = parts[0].trim() + "." + parts[1].trim() + "." + parts[2].trim() + "." + parts[3].trim();
    const int p1 = parts[4].getIntValue();
    const int p2 = parts[5].getIntValue();
    outPort = p1 * 256 + p2;
    return outPort > 0 && outPort < 65536;
}

bool FtpClient::readResponse(juce::StreamingSocket& socket, int& code, juce::String& message) const
{
    message.clear();
    code = 0;

    juce::MemoryBlock pending;
    const double deadline = juce::Time::getMillisecondCounterHiRes() + static_cast<double>(kIoTimeoutMs);

    while (juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        char chunk[512];
        const int read = socket.read(chunk, sizeof(chunk), false);
        if (read > 0)
            pending.append(chunk, static_cast<size_t>(read));
        else if (read < 0)
            return false;

        juce::String text(static_cast<const char*>(pending.getData()),
                          static_cast<int>(pending.getSize()));
        text = text.replaceCharacter('\r', '\n');
        const int newline = text.indexOfChar('\n');
        if (newline < 0)
        {
            if (read <= 0)
                juce::Thread::sleep(5);
            continue;
        }

        const auto line = text.substring(0, newline).trim();
        pending.removeSection(0, static_cast<size_t>(newline + 1));

        if (line.length() < 3)
            continue;

        const int lineCode = line.substring(0, 3).getIntValue();
        if (code == 0)
            code = lineCode;

        message += line + "\n";

        if (line.length() >= 4 && line[3] == ' ')
            return true;
    }

    return false;
}

bool FtpClient::sendCommandExpectRange(juce::StreamingSocket& socket,
                                       const juce::String& command,
                                       int minCode,
                                       int maxCode,
                                       juce::String* responseOut) const
{
    if (!socket.write(command.toRawUTF8(), static_cast<int>(command.getNumBytesAsUTF8())))
        return false;

    int code = 0;
    juce::String message;
    if (!readResponse(socket, code, message))
        return false;

    if (responseOut != nullptr)
        *responseOut = message;

    return code >= minCode && code <= maxCode;
}

bool FtpClient::sendCommand(juce::StreamingSocket& socket, const juce::String& command, int expectCode) const
{
    return sendCommandExpectRange(socket, command, expectCode, expectCode);
}

FtpUploadResult FtpClient::uploadFile(const juce::String& host,
                                      int port,
                                      const juce::String& username,
                                      const juce::String& password,
                                      const juce::File& localFile,
                                      const juce::String& remotePath,
                                      ProgressCallback onProgress) const
{
    FtpUploadResult result;
    const PiTapeScopedTimer totalTimer("ftp/upload", localFile.getFileName() + " → " + remotePath);

    if (!localFile.existsAsFile())
    {
        result.error = "Local file not found";
        return result;
    }

    const juce::int64 totalBytes = localFile.getSize();
    piTapeLog("ftp file size " + juce::String(static_cast<double>(totalBytes) / (1024.0 * 1024.0), 1) + " MB");

    juce::StreamingSocket control;
    double t0 = juce::Time::getMillisecondCounterHiRes();
    if (!control.connect(host, port, kConnectTimeoutMs))
    {
        result.error = "Could not connect to " + host + ":" + juce::String(port);
        piTapeLog("ftp connect FAILED: " + result.error);
        return result;
    }
    logFtpPhase("connect", t0, host + ":" + juce::String(port));

    int code = 0;
    juce::String message;
    t0 = juce::Time::getMillisecondCounterHiRes();
    if (!readResponse(control, code, message) || code / 100 != 2)
    {
        result.error = "FTP greeting failed";
        piTapeLog("ftp greeting FAILED code=" + juce::String(code));
        return result;
    }
    logFtpPhase("greeting", t0);

    t0 = juce::Time::getMillisecondCounterHiRes();
    const auto userCmd = "USER " + username + "\r\n";
    if (!control.write(userCmd.toRawUTF8(), userCmd.getNumBytesAsUTF8()))
    {
        result.error = "FTP USER send failed";
        return result;
    }

    if (!readResponse(control, code, message) || (code != 331 && code != 230))
    {
        result.error = "FTP USER rejected";
        return result;
    }

    if (code == 331)
    {
        if (!sendCommand(control, "PASS " + password + "\r\n", 230))
        {
            result.error = "FTP login failed";
            return result;
        }
    }
    logFtpPhase("login", t0);

    t0 = juce::Time::getMillisecondCounterHiRes();
    if (!sendCommand(control, "TYPE I\r\n", 200))
    {
        result.error = "FTP TYPE I failed";
        return result;
    }

    if (!sendCommandExpectRange(control, "PASV\r\n", 200, 299, &message))
    {
        result.error = "FTP PASV failed";
        return result;
    }
    logFtpPhase("pasv", t0);

    juce::String dataHost;
    int dataPort = 0;
    if (!parsePassiveEndpoint(message, dataHost, dataPort))
    {
        result.error = "FTP PASV parse failed";
        return result;
    }

    if (dataHost.startsWith("127.") || dataHost.startsWith("0."))
        dataHost = host;

    piTapeLog("ftp data channel " + dataHost + ":" + juce::String(dataPort));

    t0 = juce::Time::getMillisecondCounterHiRes();
    juce::StreamingSocket data;
    if (!data.connect(dataHost, dataPort, kConnectTimeoutMs))
    {
        result.error = "FTP data connection failed";
        piTapeLog("ftp data connect FAILED");
        return result;
    }
    logFtpPhase("data-connect", t0);

    const auto remote = normaliseRemotePath(remotePath);
    t0 = juce::Time::getMillisecondCounterHiRes();
    if (!sendCommandExpectRange(control, "STOR " + remote + "\r\n", 100, 199))
    {
        result.error = "FTP STOR rejected";
        return result;
    }
    logFtpPhase("stor-accept", t0, remote);

    juce::FileInputStream input(localFile);
    if (!input.openedOk())
    {
        result.error = "Could not read local file";
        return result;
    }

    juce::MemoryBlock buffer(64 * 1024);
    juce::int64 bytesSent = 0;
    int lastLoggedPct = -1;
    t0 = juce::Time::getMillisecondCounterHiRes();

    while (!input.isExhausted())
    {
        const auto read = input.read(buffer.getData(), static_cast<int>(buffer.getSize()));
        if (read <= 0)
            break;

        int writtenTotal = 0;
        while (writtenTotal < read)
        {
            const int written = data.write(static_cast<const char*>(buffer.getData()) + writtenTotal,
                                           read - writtenTotal);
            if (written <= 0)
            {
                result.error = "FTP upload interrupted";
                piTapeLog("ftp transfer interrupted at "
                          + juce::String(static_cast<double>(bytesSent) / (1024.0 * 1024.0), 1) + " MB");
                return result;
            }
            writtenTotal += written;
        }

        bytesSent += read;
        if (onProgress != nullptr)
            onProgress({ bytesSent, totalBytes });

        if (totalBytes > 0 && piTapeTraceEnabled())
        {
            const int pct = static_cast<int>((100 * bytesSent) / totalBytes);
            if (pct >= lastLoggedPct + 10)
            {
                lastLoggedPct = pct - (pct % 10);
                piTapeLog("ftp transfer " + juce::String(lastLoggedPct) + "% ("
                          + juce::String(static_cast<double>(bytesSent) / (1024.0 * 1024.0), 1) + " MB)");
            }
        }
    }

    logFtpPhase("transfer", t0, juce::String(static_cast<double>(bytesSent) / (1024.0 * 1024.0), 1) + " MB");

    data.close();

    t0 = juce::Time::getMillisecondCounterHiRes();
    if (!readResponse(control, code, message) || code / 100 != 2)
    {
        result.error = "FTP transfer failed code=" + juce::String(code);
        piTapeLog("ftp complete response FAILED: " + message.trim());
        return result;
    }
    logFtpPhase("complete-response", t0, "code " + juce::String(code));

    sendCommandExpectRange(control, "QUIT\r\n", 200, 299);
    result.success = true;
    return result;
}

} // namespace cassette
