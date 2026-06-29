#include "PiTapeControlDialog.h"
#include "ConfirmDialog.h"
#include "UiTheme.h"
#include "../util/AppLocale.h"
#include "../io/PiTapeControlClient.h"
#include "../io/PiTapeRemoteStatus.h"
#include "../io/PiTapeSessionState.h"
#include "../io/PiTapeUploader.h"
#include "../io/PiTapeTrace.h"

namespace cassette::ui
{

namespace
{

juce::String formatDuration(double sec)
{
    if (sec <= 0.0)
        return "--:--";
    const int total = static_cast<int>(sec);
    return juce::String::formatted("%02d:%02d", total / 60, total % 60);
}

juce::String sideStatusText(const PiTapeSideQueueState& side,
                            const PiTapePlaybackState& playback,
                            const juce::String& sideLetter)
{
    if (playback.isPlaying() && playback.side.equalsIgnoreCase(sideLetter))
        return tr("pi_tape.status.playing");

    if (playback.isStarting() && playback.side.equalsIgnoreCase(sideLetter))
        return tr("pi_tape.status.starting");

    if (playback.isStopped() && playback.side.equalsIgnoreCase(sideLetter))
        return tr("pi_tape.status.stopped");

    if (side.played)
        return tr("pi_tape.status.done");

    if (side.ready)
        return tr("pi_tape.status.ready");

    return tr("pi_tape.status.waiting");
}

void drawProgressBarCentredText(juce::Graphics& g,
                                const juce::Rectangle<int>& barArea,
                                const juce::String& text,
                                double progress)
{
    g.setFont(Theme::captionFont());
    const double clamped = juce::jlimit(0.0, 1.0, progress);
    const int fillW = juce::roundToInt(static_cast<float>(barArea.getWidth()) * static_cast<float>(clamped));

    if (fillW <= 0)
    {
        g.setColour(Theme::textPrimary());
        g.drawText(text, barArea, juce::Justification::centred);
        return;
    }

    if (fillW >= barArea.getWidth())
    {
        g.setColour(juce::Colours::white);
        g.drawText(text, barArea, juce::Justification::centred);
        return;
    }

    {
        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(barArea.withWidth(fillW));
        g.setColour(juce::Colours::white);
        g.drawText(text, barArea, juce::Justification::centred);
    }
    {
        juce::Graphics::ScopedSaveState clip(g);
        g.reduceClipRegion(barArea.withTrimmedLeft(fillW));
        g.setColour(Theme::textPrimary());
        g.drawText(text, barArea, juce::Justification::centred);
    }
}

} // namespace

class PiTapeControlPanel : public juce::Component,
                           private juce::Timer,
                           private juce::Button::Listener
{
public:
    PiTapeControlPanel(AnalysisWorker& workerIn, PiTapeControlSession sessionIn)
        : worker(workerIn),
          session(std::move(sessionIn))
    {
        addAndMakeVisible(titleLabel);
        addAndMakeVisible(connectionLabel);
        addAndMakeVisible(activeSideLabel);
        addAndMakeVisible(sideALabel);
        addAndMakeVisible(sideAStatus);
        addAndMakeVisible(sideBLabel);
        addAndMakeVisible(sideBStatus);
        addAndMakeVisible(playAButton);
        addAndMakeVisible(playBButton);
        addAndMakeVisible(stopButton);
        addAndMakeVisible(closeButton);

        Theme::applyLabel(titleLabel, Theme::titleFont(), Theme::textPrimary(), juce::Justification::centred);
        Theme::applyLabel(connectionLabel, Theme::captionFont(), Theme::textMuted());
        Theme::applyLabel(activeSideLabel, Theme::sectionFont(), Theme::accent());
        Theme::applyLabel(sideALabel, Theme::bodyFont(), Theme::textPrimary());
        Theme::applyLabel(sideBLabel, Theme::bodyFont(), Theme::textPrimary());
        Theme::applyLabel(sideAStatus, Theme::captionFont(), Theme::textSecondary());
        Theme::applyLabel(sideBStatus, Theme::captionFont(), Theme::textSecondary());

        Theme::styleExportButton(playAButton);
        Theme::styleExportButton(playBButton);
        Theme::styleRecButton(stopButton);
        Theme::styleNeutralButton(closeButton);

        playAButton.addListener(this);
        playBButton.addListener(this);
        stopButton.addListener(this);
        closeButton.addListener(this);

        if (!session.queue.hasSideB)
        {
            sideBLabel.setVisible(false);
            sideBStatus.setVisible(false);
            playBButton.setVisible(false);
        }

        refreshLocalisedText();
        setSize(480, singleSideLayout() ? 276 : 308);
        startTimerHz(30);
        requestStatusPoll();
    }

    std::function<void()> onClose;
    std::function<void(const ConfirmDialogOptions&, std::function<void(bool)>)> onShowConfirm;

    void paint(juce::Graphics& g) override
    {
        Theme::drawCard(g, getLocalBounds(), juce::String());

        auto barArea = progressBounds.reduced(0, 6);
        g.setColour(Theme::sidebarHighlight());
        g.fillRoundedRectangle(barArea.toFloat(), 4.0f);

        if (progress01 > 0.0)
        {
            auto fill = barArea.withWidth(juce::roundToInt(static_cast<float>(barArea.getWidth()) * progress01));
            g.setColour(Theme::accent());
            g.fillRoundedRectangle(fill.toFloat(), 4.0f);
        }

        g.setColour(Theme::textMuted());
        g.setFont(Theme::captionFont());
        if (uploadInFlight.load() && displayDurationSec > 0.0)
        {
            drawProgressBarCentredText(g,
                                       barArea,
                                       juce::String(displayPositionSec, 1) + " MB / "
                                           + juce::String(displayDurationSec, 1) + " MB",
                                       progress01);
        }
        else
        {
            drawProgressBarCentredText(g,
                                       barArea,
                                       formatDuration(displayPositionSec) + " / "
                                           + formatDuration(displayDurationSec),
                                       progress01);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(24, 20);

        titleLabel.setBounds(r.removeFromTop(28));
        r.removeFromTop(10);
        connectionLabel.setBounds(r.removeFromTop(18));
        r.removeFromTop(6);

        if (activeSideLabel.getText().isNotEmpty())
        {
            activeSideLabel.setVisible(true);
            activeSideLabel.setBounds(r.removeFromTop(22));
            r.removeFromTop(6);
        }
        else
        {
            activeSideLabel.setVisible(false);
        }

        progressBounds = r.removeFromTop(28);
        r.removeFromTop(14);

        auto sideRow = r.removeFromTop(24);
        sideALabel.setBounds(sideRow.removeFromLeft(72));
        sideAStatus.setBounds(sideRow);

        if (!singleSideLayout())
        {
            r.removeFromTop(6);
            sideRow = r.removeFromTop(24);
            sideBLabel.setBounds(sideRow.removeFromLeft(72));
            sideBStatus.setBounds(sideRow);
        }

        r.removeFromTop(16);
        auto buttons = r.removeFromTop(36);
        if (singleSideLayout())
        {
            playAButton.setBounds(buttons);
        }
        else
        {
            const int playGap = 8;
            const int playBtnW = juce::jmax(140, (buttons.getWidth() - playGap) / 2);
            playAButton.setBounds(buttons.removeFromLeft(playBtnW));
            buttons.removeFromLeft(playGap);
            playBButton.setBounds(buttons.removeFromLeft(playBtnW));
        }

        r.removeFromTop(10);
        buttons = r.removeFromTop(36);
        stopButton.setBounds(buttons.removeFromLeft(96));
        buttons.removeFromLeft(8);
        closeButton.setBounds(buttons.removeFromRight(96));
    }

private:
    AnalysisWorker& worker;
    PiTapeControlSession session;
    PiTapeRemoteStatus remoteStatus;

    juce::Label titleLabel;
    juce::Label connectionLabel;
    juce::Label activeSideLabel;
    juce::Label sideALabel;
    juce::Label sideAStatus;
    juce::Label sideBLabel;
    juce::Label sideBStatus;
    juce::TextButton playAButton { "Play A" };
    juce::TextButton playBButton { "Play B" };
    juce::TextButton stopButton { "Stop" };
    juce::TextButton closeButton { "Close" };

    juce::Rectangle<int> progressBounds;
    double progress01 = 0.0;
    double displayPositionSec = 0.0;
    double displayDurationSec = 0.0;
    double playbackAnchorPosSec = 0.0;
    juce::uint32 playbackSyncMs = 0;
    bool smoothPlaybackActive = false;
    int pollTickCounter = 0;
    juce::String pendingPlaySide;
    double pendingResumeOffsetSec = -1.0;
    bool stopRequestedLocally = false;
    double stoppedPositionSec = 0.0;

    std::atomic<bool> pollInFlight { false };
    std::atomic<bool> uploadInFlight { false };
    std::atomic<bool> commandInFlight { false };
    bool autoUploadChecked = false;

    using PanelPtr = juce::Component::SafePointer<PiTapeControlPanel>;

    void shutdownForClose()
    {
        stopTimer();
        pollInFlight = false;
        commandInFlight = false;
    }

    void refreshLocalisedText()
    {
        titleLabel.setText(tr("pi_tape.dialog.title"), juce::dontSendNotification);
        stopButton.setButtonText(tr("pi_tape.btn.stop"));
        closeButton.setButtonText(tr("btn.done"));
        sideALabel.setText(tr("track.side_a"), juce::dontSendNotification);
        sideBLabel.setText(tr("track.side_b"), juce::dontSendNotification);
        updatePlayButtonLabels();
    }

    bool singleSideLayout() const
    {
        return !session.queue.hasSideB;
    }

    bool canResumeSide(const juce::String& side) const
    {
        const auto& pb = remoteStatus.playback;
        return pb.isStopped()
               && pb.side.equalsIgnoreCase(side)
               && pb.positionSec > 0.5
               && pb.durationSec > 0.0
               && pb.positionSec < pb.durationSec - 0.5;
    }

    void updatePlayButtonLabels()
    {
        if (singleSideLayout())
        {
            playAButton.setButtonText(canResumeSide("A") ? tr("pi_tape.btn.resume") : tr("pi_tape.btn.play"));
            return;
        }

        playAButton.setButtonText(canResumeSide("A") ? tr("pi_tape.btn.resume_a") : tr("pi_tape.btn.play_a"));
        playBButton.setButtonText(canResumeSide("B") ? tr("pi_tape.btn.resume_b") : tr("pi_tape.btn.play_b"));
    }

    void applyPlaybackUiState(const PiTapePlaybackState& playback, bool resuming = false)
    {
        if (playback.isStarting() || pendingPlaySide.isNotEmpty())
        {
            smoothPlaybackActive = false;
            const auto& side = pendingPlaySide.isNotEmpty() ? pendingPlaySide : playback.side;
            if (resuming || pendingResumeOffsetSec >= 0.0)
            {
                activeSideLabel.setText(side == "B" ? tr("pi_tape.recording.resuming_b")
                                                      : tr("pi_tape.recording.resuming_a"),
                                        juce::dontSendNotification);
            }
            else
            {
                activeSideLabel.setText(side == "B" ? tr("pi_tape.recording.starting_b")
                                                      : tr("pi_tape.recording.starting_a"),
                                        juce::dontSendNotification);
            }
            if (playback.durationSec > 0.0)
                displayDurationSec = playback.durationSec;
            if (pendingResumeOffsetSec >= 0.0)
                displayPositionSec = pendingResumeOffsetSec;
            else
                displayPositionSec = playback.positionSec;
            progress01 = displayDurationSec > 0.0 ? displayPositionSec / displayDurationSec : 0.0;
            return;
        }

        pendingResumeOffsetSec = -1.0;

        if (playback.isPlaying())
        {
            activeSideLabel.setText(playback.side == "B" ? tr("pi_tape.recording.side_b")
                                                         : tr("pi_tape.recording.side_a"),
                                    juce::dontSendNotification);
            syncPlaybackAnchor(playback);
            updateSmoothPlaybackDisplay();
        }
        else if (playback.state.equalsIgnoreCase("finished"))
        {
            smoothPlaybackActive = false;
            activeSideLabel.setText(tr("pi_tape.recording.finished"), juce::dontSendNotification);
            progress01 = playback.progress01();
            displayPositionSec = playback.positionSec;
            displayDurationSec = playback.durationSec;
        }
        else if (playback.isStopped())
        {
            smoothPlaybackActive = false;
            activeSideLabel.setText(tr("pi_tape.recording.stopped"), juce::dontSendNotification);
            progress01 = playback.progress01();
            displayPositionSec = playback.positionSec;
            displayDurationSec = playback.durationSec;
        }
        else if (!uploadInFlight.load())
        {
            smoothPlaybackActive = false;
            activeSideLabel.setText(tr("pi_tape.recording.idle"), juce::dontSendNotification);
            progress01 = playback.progress01();
            displayPositionSec = playback.positionSec;
            displayDurationSec = playback.durationSec;
        }
    }

    void timerCallback() override
    {
        if (smoothPlaybackActive && !uploadInFlight.load() && !stopRequestedLocally)
        {
            updateSmoothPlaybackDisplay();
            repaint();
        }

        const int pollInterval = smoothPlaybackActive ? 3
                                  : (pendingPlaySide.isNotEmpty() || commandInFlight.load()
                                     || remoteStatus.playback.isStarting())
                                        ? 2
                                        : 15;
        if (++pollTickCounter >= pollInterval)
        {
            pollTickCounter = 0;
            requestStatusPoll();
        }
    }

    void syncPlaybackAnchor(const PiTapePlaybackState& playback)
    {
        playbackAnchorPosSec = playback.positionSec;
        playbackSyncMs = juce::Time::getMillisecondCounter();
        if (playback.durationSec > 0.0)
            displayDurationSec = playback.durationSec;
        smoothPlaybackActive = playback.isPlaying();
    }

    void updateSmoothPlaybackDisplay()
    {
        if (displayDurationSec <= 0.0)
            return;

        const double elapsedSec = (juce::Time::getMillisecondCounter() - playbackSyncMs) / 1000.0;
        displayPositionSec = juce::jmin(playbackAnchorPosSec + elapsedSec, displayDurationSec);
        progress01 = displayPositionSec / displayDurationSec;
    }

    void requestStatusPoll()
    {
        if (pollInFlight.load())
            return;

        pollInFlight = true;
        const auto settings = session.settings;
        const PanelPtr safe(this);

        worker.enqueue([safe, settings]() {
            PiTapeScopedTimer timer("ui/status-poll");
            PiTapeControlClient client;
            const auto status = client.fetchStatus(settings);

            juce::MessageManager::callAsync([safe, status]() {
                if (auto* panel = safe.getComponent())
                {
                    panel->pollInFlight = false;
                    panel->applyStatus(status);
                }
            });
        });
    }

    void applyStatus(const PiTapeRemoteStatus& status)
    {
        remoteStatus.online = status.online;
        remoteStatus.error = status.error;
        remoteStatus.sideA = status.sideA;
        remoteStatus.sideB = status.sideB;

        if (stopRequestedLocally)
        {
            if (!status.playback.isPlaying() && !status.playback.isStarting())
            {
                stopRequestedLocally = false;
                remoteStatus.playback = status.playback;
            }
        }
        else
        {
            remoteStatus.playback = status.playback;
        }

        if (!status.online)
        {
            if (!uploadInFlight.load())
            {
                connectionLabel.setText(trf("pi_tape.connection.offline", status.error), juce::dontSendNotification);
                connectionLabel.setColour(juce::Label::textColourId, Theme::failRed());
            }
            activeSideLabel.setText(tr("pi_tape.recording.idle"), juce::dontSendNotification);
            if (!uploadInFlight.load())
            {
                progress01 = 0.0;
                displayPositionSec = 0.0;
                displayDurationSec = 0.0;
            }
            updateSideLabels();
            updateButtons();
            repaint();
            return;
        }

        if (!uploadInFlight.load())
        {
            connectionLabel.setText(tr("pi_tape.connection.online"), juce::dontSendNotification);
            connectionLabel.setColour(juce::Label::textColourId, Theme::okGreen());

            if (status.playback.isPlaying() && !stopRequestedLocally)
            {
                if (pendingPlaySide.isEmpty() || status.playback.side.equalsIgnoreCase(pendingPlaySide))
                {
                    pendingPlaySide = {};
                    pendingResumeOffsetSec = -1.0;
                }
                applyPlaybackUiState(status.playback);
            }
            else if (stopRequestedLocally)
            {
                PiTapePlaybackState frozen = remoteStatus.playback;
                frozen.state = "stopped";
                frozen.positionSec = stoppedPositionSec;
                if (frozen.durationSec <= 0.0)
                    frozen.durationSec = displayDurationSec;
                remoteStatus.playback = frozen;
                smoothPlaybackActive = false;
                applyPlaybackUiState(frozen);
            }
            else if (pendingPlaySide.isNotEmpty())
            {
                auto uiState = status.playback;
                uiState.state = "starting";
                uiState.side = pendingPlaySide;
                if (uiState.durationSec <= 0.0 && displayDurationSec > 0.0)
                    uiState.durationSec = displayDurationSec;
                applyPlaybackUiState(uiState, pendingResumeOffsetSec >= 0.0);
            }
            else
            {
                applyPlaybackUiState(status.playback);
            }
        }

        updatePlayButtonLabels();
        updateSideLabels();
        updateButtons();
        maybeAutoUpload();
        resized();
        repaint();
    }

    void maybeAutoUpload()
    {
        if (autoUploadChecked || uploadInFlight.load() || !remoteStatus.online)
            return;

        autoUploadChecked = true;
        if (!remoteStatus.sideA.ready)
            beginUpload();
    }

    void updateSideLabels()
    {
        const auto& pb = remoteStatus.playback;
        sideAStatus.setText(sideStatusText(remoteStatus.sideA, pb, "A") + " · "
                                + formatDuration(remoteStatus.sideA.durationSec),
                            juce::dontSendNotification);
        if (!singleSideLayout())
        {
            sideBStatus.setText(sideStatusText(remoteStatus.sideB, pb, "B") + " · "
                                    + formatDuration(remoteStatus.sideB.durationSec),
                                juce::dontSendNotification);
        }
    }

    void updateButtons()
    {
        const bool busy = uploadInFlight.load() || commandInFlight.load();
        const bool playing = remoteStatus.playback.isPlaying();
        const bool starting = remoteStatus.playback.isStarting() || pendingPlaySide.isNotEmpty();
        const bool online = remoteStatus.online;

        playAButton.setEnabled(!busy && !playing && !starting && online && remoteStatus.sideA.ready);
        playBButton.setEnabled(!busy && !playing && !starting && online && session.queue.hasSideB && remoteStatus.sideB.ready);
        stopButton.setEnabled(online && (playing || starting || pendingPlaySide.isNotEmpty()));
    }

    void updateUploadProgress(const PiTapeUploadProgress& progress)
    {
        connectionLabel.setText(tr("pi_tape.upload.progress"), juce::dontSendNotification);
        connectionLabel.setColour(juce::Label::textColourId, Theme::warnAmber());
        activeSideLabel.setText({}, juce::dontSendNotification);

        if (progress.totalBytes > 0)
        {
            progress01 = static_cast<double>(progress.bytesSent) / static_cast<double>(progress.totalBytes);
            displayPositionSec = static_cast<double>(progress.bytesSent) / (1024.0 * 1024.0);
            displayDurationSec = static_cast<double>(progress.totalBytes) / (1024.0 * 1024.0);
        }

        resized();
        repaint();
    }

    void beginUpload()
    {
        if (uploadInFlight.load())
            return;

        uploadInFlight = true;
        updateUploadProgress({ PiTapeUploadProgress::Stage::SideA, 0, session.queue.sideA.getSize() });

        const auto settings = session.settings;
        const auto queue = session.queue;
        const PanelPtr safe(this);

        worker.enqueue([safe, settings, queue]() {
            PiTapeScopedTimer timer("ui/upload-queue", queue.projectName);
            const auto result = PiTapeUploader::uploadQueue(settings, queue, [safe](const PiTapeUploadProgress& progress) {
                juce::MessageManager::callAsync([safe, progress]() {
                    if (auto* panel = safe.getComponent())
                        panel->updateUploadProgress(progress);
                });
            });

            juce::MessageManager::callAsync([safe, result]() {
                if (auto* panel = safe.getComponent())
                {
                    panel->uploadInFlight = false;
                    if (!result.success)
                    {
                        panel->connectionLabel.setText(trf("pi_tape.upload_failed", result.error),
                                                       juce::dontSendNotification);
                        panel->connectionLabel.setColour(juce::Label::textColourId, Theme::failRed());
                        panel->progress01 = 0.0;
                        panel->displayPositionSec = 0.0;
                        panel->displayDurationSec = 0.0;
                    }
                    else
                    {
                        PiTapeSessionState sessionState;
                        sessionState.markUploadedNow();
                        sessionState.save();
                    }
                    panel->requestStatusPoll();
                    panel->updateButtons();
                    panel->repaint();
                }
            });
        });
    }

    void beginPlay(const juce::String& side)
    {
        if (commandInFlight.load())
            return;

        const double offsetSec = canResumeSide(side) ? remoteStatus.playback.positionSec : -1.0;
        const double knownDuration = side.equalsIgnoreCase("B") ? remoteStatus.sideB.durationSec
                                                                : remoteStatus.sideA.durationSec;
        const double knownPosition = offsetSec >= 0.0 ? offsetSec : 0.0;
        const bool resuming = offsetSec >= 0.0;

        commandInFlight = true;
        stopRequestedLocally = false;
        pendingPlaySide = side;
        pendingResumeOffsetSec = resuming ? offsetSec : -1.0;
        smoothPlaybackActive = false;
        displayDurationSec = knownDuration;
        displayPositionSec = knownPosition;
        progress01 = knownDuration > 0.0 ? knownPosition / knownDuration : 0.0;

        PiTapePlaybackState uiState;
        uiState.state = "starting";
        uiState.side = side;
        uiState.positionSec = knownPosition;
        uiState.durationSec = knownDuration;
        applyPlaybackUiState(uiState, resuming);
        updatePlayButtonLabels();
        updateButtons();
        repaint();

        requestStatusPoll();

        const auto settings = session.settings;
        const PanelPtr safe(this);

        worker.enqueue([safe, settings, side, offsetSec]() {
            PiTapeScopedTimer timer("ui/play", side);
            PiTapeControlClient client;
            const auto result = client.playSide(settings, side, offsetSec);

            juce::MessageManager::callAsync([safe, result]() {
                if (auto* panel = safe.getComponent())
                {
                    panel->commandInFlight = false;
                    if (!result.success)
                    {
                        panel->pendingPlaySide = {};
                        panel->pendingResumeOffsetSec = -1.0;
                        panel->connectionLabel.setText(trf("pi_tape.play_failed", result.error),
                                                       juce::dontSendNotification);
                    }
                    panel->requestStatusPoll();
                    panel->updateButtons();
                    panel->repaint();
                }
            });
        });
    }

    void beginStop(std::function<void()> onComplete = {})
    {
        const bool closing = static_cast<bool>(onComplete);

        commandInFlight = false;
        pendingPlaySide = {};
        pendingResumeOffsetSec = -1.0;
        smoothPlaybackActive = false;
        stopRequestedLocally = true;
        stoppedPositionSec = displayPositionSec;

        auto optimistic = remoteStatus.playback;
        optimistic.state = "stopped";
        optimistic.positionSec = displayPositionSec;
        if (optimistic.durationSec <= 0.0)
            optimistic.durationSec = displayDurationSec;
        remoteStatus.playback = optimistic;
        applyPlaybackUiState(optimistic);
        updatePlayButtonLabels();
        updateButtons();
        repaint();

        const auto settings = session.settings;

        if (closing)
        {
            shutdownForClose();
            if (onComplete)
                onComplete();

            worker.enqueue([settings]() {
                PiTapeControlClient client;
                client.stopPlayback(settings);
            });
            return;
        }

        const PanelPtr safe(this);
        worker.enqueue([safe, settings]() {
            PiTapeScopedTimer timer("ui/stop");
            PiTapeControlClient client;
            const auto result = client.stopPlayback(settings);

            juce::MessageManager::callAsync([safe, result]() {
                if (auto* panel = safe.getComponent())
                {
                    if (!result.success)
                        panel->connectionLabel.setText(trf("pi_tape.stop_failed", result.error),
                                                       juce::dontSendNotification);
                    panel->requestStatusPoll();
                    panel->updateButtons();
                }
            });
        });
    }

    bool needsStopConfirmation() const
    {
        if (pendingPlaySide.isNotEmpty() || commandInFlight.load())
            return true;
        if (remoteStatus.playback.isPlaying() || remoteStatus.playback.isStarting())
            return true;
        if (smoothPlaybackActive)
            return true;
        if (canResumeSide("A") || canResumeSide("B"))
            return true;
        return false;
    }

    void promptStopPlayback(std::function<void()> onConfirmed)
    {
        ConfirmDialogOptions options;
        options.title = tr("pi_tape.confirm.stop.title");
        options.message = tr("pi_tape.confirm.stop.message");
        options.confirmLabel = tr("pi_tape.confirm.stop.confirm");
        options.cancelLabel = tr("pi_tape.confirm.stop.cancel");

        const auto handleResult = [onConfirmed = std::move(onConfirmed)](bool confirmed) {
            if (confirmed && onConfirmed)
                onConfirmed();
        };

        if (onShowConfirm)
        {
            onShowConfirm(options, handleResult);
            return;
        }

        showConfirmDialog(this, options, handleResult);
    }

    void promptUploadCancelConfirm(std::function<void()> onConfirmed)
    {
        ConfirmDialogOptions options;
        options.title = tr("pi_tape.confirm.upload.title");
        options.message = tr("pi_tape.confirm.upload.message");
        options.confirmLabel = tr("pi_tape.confirm.upload.confirm");
        options.cancelLabel = tr("pi_tape.confirm.upload.cancel");

        const auto handleResult = [onConfirmed = std::move(onConfirmed)](bool confirmed) {
            if (confirmed && onConfirmed)
                onConfirmed();
        };

        if (onShowConfirm)
        {
            onShowConfirm(options, handleResult);
            return;
        }

        showConfirmDialog(this, options, handleResult);
    }

    void scheduleInboxCleanup()
    {
        const auto settings = session.settings;
            worker.enqueue([settings]() {
                PiTapeScopedTimer timer("ui/cleanup-on-close");
                PiTapeControlClient client;
                client.cleanupSession(settings);
                PiTapeSessionState::clearPersisted();
            });
    }

    void requestClose()
    {
        const auto finishClose = [this]() {
            scheduleInboxCleanup();
            if (onClose)
                onClose();
        };

        if (uploadInFlight.load())
        {
            promptUploadCancelConfirm(finishClose);
            return;
        }

        if (needsStopConfirmation())
        {
            promptStopPlayback([this, finishClose] { beginStop(finishClose); });
            return;
        }

        finishClose();
    }

    void buttonClicked(juce::Button* b) override
    {
        if (b == &playAButton)
        {
            beginPlay("A");
            return;
        }
        if (b == &playBButton)
        {
            beginPlay("B");
            return;
        }
        if (b == &stopButton)
        {
            beginStop();
            return;
        }
        if (b == &closeButton)
        {
            requestClose();
        }
    }
};

class PiTapeControlModal : public juce::Component
{
public:
    PiTapeControlModal(AnalysisWorker& worker, PiTapeControlSession session)
        : panel(worker, std::move(session))
    {
        panel.onClose = [this] { exitModalState(0); };
        panel.onShowConfirm = [this](const ConfirmDialogOptions& options, std::function<void(bool)> onResult) {
            attachConfirmOverlay(*this, options, std::move(onResult));
        };
        addAndMakeVisible(panel);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.55f));
    }

    void resized() override
    {
        panel.setCentrePosition(getLocalBounds().getCentre());

        for (int i = 0; i < getNumChildComponents(); ++i)
        {
            if (auto* child = getChildComponent(i); child != &panel)
                child->setBounds(getLocalBounds());
        }
    }

private:
    PiTapeControlPanel panel;
};

void showPiTapeControlDialog(juce::Component* host,
                             AnalysisWorker& worker,
                             PiTapeControlSession session)
{
    if (host == nullptr)
        return;

    auto* top = host->getTopLevelComponent();
    if (top == nullptr)
        return;

    auto* modal = new PiTapeControlModal(worker, std::move(session));
    top->addAndMakeVisible(modal);
    modal->setBounds(top->getLocalBounds());
    modal->toFront(true);
    modal->enterModalState(true,
                           juce::ModalCallbackFunction::create([modal](int) { delete modal; }),
                           true);
}

} // namespace cassette::ui
