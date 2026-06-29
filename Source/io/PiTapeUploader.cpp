#include "PiTapeUploader.h"
#include "PiTapeTrace.h"

namespace cassette
{

namespace
{

juce::String remotePathFor(const PiTapeSettings& settings, const juce::String& fileName)
{
    auto dir = settings.remoteDir.trim();
    while (dir.endsWithChar('/'))
        dir = dir.dropLastCharacters(1);
    return dir.isEmpty() ? fileName : dir + "/" + fileName;
}

juce::String makeJobId(const juce::String& projectName)
{
    const auto stamp = juce::Time::getCurrentTime().toISO8601(true);
    const auto safeName = projectName.replaceCharacter(' ', '_');
    return stamp + "_" + safeName;
}

} // namespace

juce::String PiTapeUploader::buildManifestJson(const juce::String& jobId,
                                               const juce::String& remoteWavPath,
                                               const juce::String& sideLabel)
{
    auto* o = new juce::DynamicObject();
    o->setProperty("job_id", jobId);
    o->setProperty("remote_path", remoteWavPath);
    o->setProperty("side", sideLabel);
    o->setProperty("normalize", false);
    o->setProperty("target_format", "wav44k16stereo");
    o->setProperty("play_mode", "replace");
    o->setProperty("start_mode", "manual");
    return juce::JSON::toString(juce::var(o), true);
}

juce::String PiTapeUploader::buildQueueJson(const juce::String& jobId,
                                            const PiTapeQueueSource& source,
                                            const juce::String& remoteSideA,
                                            const juce::String& remoteSideB)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("job_id", jobId);
    root->setProperty("project_name", source.projectName);

    auto* sideA = new juce::DynamicObject();
    sideA->setProperty("remote_path", remoteSideA);
    sideA->setProperty("filename", source.sideA.getFileName());
    sideA->setProperty("played", false);
    root->setProperty("side_a", juce::var(sideA));

    if (source.hasSideB && source.sideB.existsAsFile())
    {
        auto* sideB = new juce::DynamicObject();
        sideB->setProperty("remote_path", remoteSideB);
        sideB->setProperty("filename", source.sideB.getFileName());
        sideB->setProperty("played", false);
        root->setProperty("side_b", juce::var(sideB));
    }

    return juce::JSON::toString(juce::var(root), true);
}

PiTapeUploadResult PiTapeUploader::uploadTextFile(const PiTapeSettings& settings,
                                                  const juce::String& remotePath,
                                                  const juce::String& text,
                                                  std::function<void(const PiTapeUploadProgress&)> onProgress)
{
    PiTapeUploadResult result;
    const auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory)
                          .getChildFile("deck-pi-tape-upload.json");
    if (!temp.replaceWithText(text))
    {
        result.error = "Could not write temp upload file";
        return result;
    }

    FtpClient ftp;
    FtpUploadResult upload;
    {
        PiTapeScopedTimer timer("upload/text", remotePath);
        upload = ftp.uploadFile(settings.host,
                                settings.port,
                                settings.username,
                                settings.password,
                                temp,
                                remotePath,
                                [onProgress](const FtpUploadProgress& progress) {
                                    if (onProgress != nullptr)
                                    {
                                        PiTapeUploadProgress stage;
                                        stage.stage = PiTapeUploadProgress::Stage::Queue;
                                        stage.bytesSent = progress.bytesSent;
                                        stage.totalBytes = progress.totalBytes;
                                        onProgress(stage);
                                    }
                                });
    }
    temp.deleteFile();

    result.success = upload.success;
    result.error = upload.error;
    return result;
}

PiTapeUploadResult PiTapeUploader::uploadPreparedSide(const PiTapeSettings& settings,
                                                      const juce::File& wavFile,
                                                      const juce::String& sideLabel)
{
    PiTapeUploadResult result;

    if (!settings.isConfigured())
    {
        result.error = "Pi tape FTP is not configured";
        return result;
    }

    const auto remoteWav = remotePathFor(settings, wavFile.getFileName());
    const auto jobId = makeJobId(wavFile.getFileNameWithoutExtension()) + "_side_" + sideLabel;
    result.jobId = jobId;

    FtpClient ftp;
    const auto wavUpload = ftp.uploadFile(settings.host,
                                          settings.port,
                                          settings.username,
                                          settings.password,
                                          wavFile,
                                          remoteWav);
    if (!wavUpload.success)
    {
        result.error = wavUpload.error;
        return result;
    }

    const auto manifestText = buildManifestJson(jobId, remoteWav, sideLabel);
    const auto manifestUpload = uploadTextFile(settings, remotePathFor(settings, "manifest.json"), manifestText);
    if (!manifestUpload.success)
    {
        result.error = manifestUpload.error;
        return result;
    }

    result.success = true;
    return result;
}

PiTapeUploadResult PiTapeUploader::uploadQueue(const PiTapeSettings& settings,
                                               const PiTapeQueueSource& source,
                                               std::function<void(const PiTapeUploadProgress&)> onProgress)
{
    PiTapeUploadResult result;
    const PiTapeScopedTimer totalTimer("upload/queue", source.projectName);

    if (!settings.isConfigured())
    {
        result.error = "Pi tape FTP is not configured";
        return result;
    }

    if (!source.sideA.existsAsFile())
    {
        result.error = "Side A file missing";
        return result;
    }

    const auto jobId = makeJobId(source.projectName);
    result.jobId = jobId;

    const auto remoteSideA = remotePathFor(settings, source.sideA.getFileName());
    const auto remoteSideB = source.hasSideB && source.sideB.existsAsFile()
                                 ? remotePathFor(settings, source.sideB.getFileName())
                                 : juce::String();

    FtpClient ftp;
    const auto reportProgress = [&](PiTapeUploadProgress::Stage stage, const FtpUploadProgress& progress) {
        if (onProgress != nullptr)
        {
            PiTapeUploadProgress update;
            update.stage = stage;
            update.bytesSent = progress.bytesSent;
            update.totalBytes = progress.totalBytes;
            onProgress(update);
        }
    };

    if (onProgress != nullptr)
    {
        PiTapeUploadProgress start;
        start.stage = PiTapeUploadProgress::Stage::SideA;
        start.totalBytes = source.sideA.getSize();
        onProgress(start);
    }

    piTapeLog("upload side A " + source.sideA.getFileName() + " ("
              + juce::String(static_cast<double>(source.sideA.getSize()) / (1024.0 * 1024.0), 1) + " MB)");
    {
        PiTapeScopedTimer sideTimer("upload/side-a", source.sideA.getFileName());
        const auto sideAUpload = ftp.uploadFile(settings.host,
                                                settings.port,
                                                settings.username,
                                                settings.password,
                                                source.sideA,
                                                remoteSideA,
                                                [&](const FtpUploadProgress& progress) {
                                                    reportProgress(PiTapeUploadProgress::Stage::SideA, progress);
                                                });
        if (!sideAUpload.success)
        {
            result.error = sideAUpload.error;
            return result;
        }
    }

    if (remoteSideB.isNotEmpty())
    {
        piTapeLog("upload side B " + source.sideB.getFileName() + " ("
                  + juce::String(static_cast<double>(source.sideB.getSize()) / (1024.0 * 1024.0), 1) + " MB)");
        if (onProgress != nullptr)
        {
            PiTapeUploadProgress start;
            start.stage = PiTapeUploadProgress::Stage::SideB;
            start.totalBytes = source.sideB.getSize();
            onProgress(start);
        }

        PiTapeScopedTimer sideTimer("upload/side-b", source.sideB.getFileName());
        const auto sideBUpload = ftp.uploadFile(settings.host,
                                                settings.port,
                                                settings.username,
                                                settings.password,
                                                source.sideB,
                                                remoteSideB,
                                                [&](const FtpUploadProgress& progress) {
                                                    reportProgress(PiTapeUploadProgress::Stage::SideB, progress);
                                                });
        if (!sideBUpload.success)
        {
            result.error = sideBUpload.error;
            return result;
        }
    }

    if (onProgress != nullptr)
    {
        PiTapeUploadProgress start;
        start.stage = PiTapeUploadProgress::Stage::Queue;
        onProgress(start);
    }

    const auto queueText = buildQueueJson(jobId, source, remoteSideA, remoteSideB);
    {
        PiTapeScopedTimer queueTimer("upload/tape-queue-json", "tape_queue.json");
        const auto queueUpload = uploadTextFile(settings, remotePathFor(settings, "tape_queue.json"), queueText, onProgress);
        if (!queueUpload.success)
        {
            result.error = queueUpload.error;
            return result;
        }
    }

    result.success = true;
    return result;
}

} // namespace cassette
