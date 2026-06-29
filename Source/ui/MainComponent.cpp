#include "MainComponent.h"
#include "ConfirmDialog.h"
#include "ImportFolderPicker.h"
#include "UiTheme.h"
#include <cmath>
#include <memory>
#include "../analysis/EssentiaAnalyzer.h"
#include "../dsp/graph/AdaptiveMasteringProcessor.h"
#include "../export/WavExporter.h"
#include "../export/PreflightTones.h"
#include "../project/SideRenderer.h"
#include "../dsp/AudioConstants.h"
#include "../io/DropPayload.h"
#include "../util/AppLog.h"
#include "../util/CassetteBuildFlags.h"
#if CASSETTE_ENABLE_PI_TAPE
#include "../io/PiTapeUploader.h"
#include "../io/PiTapeControlClient.h"
#include "../io/PiTapeSessionState.h"
#include "PiTapeControlDialog.h"
#endif
#include "../util/AppLocale.h"

namespace cassette
{

MainComponent::MainComponent()
{
    using namespace ui;

    for (auto* c : { static_cast<juce::Component*>(&tapeSetupPanel),
                     static_cast<juce::Component*>(&dropHero),
                     static_cast<juce::Component*>(&wizardSteps),
                     static_cast<juce::Component*>(&readySummary),
                     static_cast<juce::Component*>(&compareWaveform),
                     static_cast<juce::Component*>(&trackListEditor),
                     static_cast<juce::Component*>(&newButton),
                     static_cast<juce::Component*>(&settingsButton),
                     static_cast<juce::Component*>(&startButton),
                     static_cast<juce::Component*>(&exportButton),
#if CASSETTE_ENABLE_PI_TAPE
                     static_cast<juce::Component*>(&sendToPiButton),
#endif
                     static_cast<juce::Component*>(&status) })
        addAndMakeVisible(c);

    addChildComponent(mixtapePanel);

    tapeSetupPanel.setMainScreenMode(true);
    tapeSetupPanel.setCompactToolbarMode(true);
    tapeSetupPanel.setMixtapeMode(false);
    tapeSetupPanel.onSetupChanged = [this] {
        const auto tape = tapeSetup().getTapeLengthSpec();
        if (mixtapeModeActive && folderScan.has_value() && folderScan->success)
        {
            const bool editorActive = !mixtapeEditor.sideA().empty() || !mixtapeEditor.sideB().empty();
            if (editorActive)
            {
                mixtapeEditor.syncCassettePlan(tape);
                if (mixtapeEditor.hasSideOverflow(tape) || !mixtapeEditor.canPrepare(tape))
                    mixtapeEditor.rebalance(tape);
            }
            trackListEditor.setTapeSpec(tape);
            trackListEditor.refresh();
        }
        refreshFolderFitLabel();
        if (mixtapeModeActive && hasProcessed)
            invalidatePreparedOutput();
        syncLayout();
    };
    tapeSetupPanel.onChangeTapeTypeRequested = [this] { promptChangeTapeType(); };

    Theme::styleNeutralButton(settingsButton);
    settingsButton.addListener(this);

    Theme::applyLabel(readySummary, Theme::metricFont(), Theme::okGreen());
    readySummary.setVisible(false);

    Theme::styleRecButton(startButton);
    Theme::styleExportButton(exportButton);
#if CASSETTE_ENABLE_PI_TAPE
    Theme::styleExportButton(sendToPiButton);
#endif
    Theme::styleNeutralButton(newButton);
    newButton.addListener(this);
    startButton.addListener(this);
    exportButton.addListener(this);
#if CASSETTE_ENABLE_PI_TAPE
    sendToPiButton.addListener(this);
#endif
    startButton.setEnabled(false);
    newButton.setEnabled(false);
    exportButton.setEnabled(false);
#if CASSETTE_ENABLE_PI_TAPE
    sendToPiButton.setEnabled(false);
    piTapeSettings = PiTapeSettings::load();
    maybeCleanupExpiredPiTapeInbox();
#endif

    Theme::applyLabel(status, Theme::bodyFont(), Theme::textSecondary());

    dropHero.onChooseFolder = [this] { pickImportFolder(); };
    compareWaveform.setShowEmptyDropZone(false);

    mixtapePanel.onFolderSelected = [this](const juce::File& folder) { scanMixFolder(folder); };

    trackListEditor.setVisible(false);
    previewDeviceManager.initialiseWithDefaultDevices(0, 2);
    trackListEditor.attachToAudioDevice(previewDeviceManager);
    trackListEditor.onLayoutChanged = [this] {
        if (mixtapeModeActive && hasProcessed)
            invalidatePreparedOutput();
        if (folderScan.has_value() && folderScan->success)
            mixtapeEditor.syncCassettePlan(tapeSetup().getTapeLengthSpec());
        refreshFolderFitLabel();
        syncLayout();
    };

    setStatus({}, Theme::textSecondary());
    refreshUiText();
    startTimerHz(12);
    setWantsKeyboardFocus(true);
    setSize(1180, 820);
    syncLayout();
}

MainComponent::~MainComponent()
{
    stopTimer();
    trackListEditor.shutdownPreviewAudio();
    previewDeviceManager.closeAudioDevice();
#if CASSETTE_ENABLE_PI_TAPE
    cleanupPiTapeInboxOnExit();
#endif
}

TapeSetupPanel& MainComponent::tapeSetup() { return tapeSetupPanel; }
const TapeSetupPanel& MainComponent::tapeSetup() const { return tapeSetupPanel; }

void MainComponent::setStatus(const juce::String& text, juce::Colour colour)
{
    status.setText(text, juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, colour);
    status.setVisible(text.isNotEmpty());
}

void MainComponent::refreshUiText()
{
    newButton.setButtonText(tr("btn.new"));
    settingsButton.setButtonText(tr("btn.settings"));
    startButton.setButtonText(tr("btn.prepare"));
    exportButton.setButtonText(tr("btn.export_wav"));
#if CASSETTE_ENABLE_PI_TAPE
    sendToPiButton.setButtonText(tr("btn.pi_tape"));
#endif
    tapeSetupPanel.refreshLocalisedText();
    dropHero.refreshLocalisedText();
    trackListEditor.refreshLocalisedText();
    wizardSteps.repaint();
    syncTransportButtonStyles();
}

void MainComponent::syncTransportButtonStyles()
{
    using ui::Theme;

    const bool newLooksActive = newButton.isEnabled() && (hasSource || hasProcessed);
    Theme::applyTransportButtonStyle(newButton,
                                     newLooksActive ? Theme::TransportButtonStyle::Black
                                                    : Theme::TransportButtonStyle::Neutral,
                                     newButton.isEnabled());
    Theme::applyTransportButtonStyle(settingsButton, Theme::TransportButtonStyle::Neutral, settingsButton.isEnabled());
    Theme::applyTransportButtonStyle(startButton, Theme::TransportButtonStyle::Rec, startButton.isEnabled());
    Theme::applyTransportButtonStyle(exportButton, Theme::TransportButtonStyle::Export, exportButton.isEnabled());
#if CASSETTE_ENABLE_PI_TAPE
    Theme::applyTransportButtonStyle(sendToPiButton, Theme::TransportButtonStyle::Export, sendToPiButton.isEnabled());
#endif

    newButton.repaint();
    settingsButton.repaint();
    startButton.repaint();
    exportButton.repaint();
#if CASSETTE_ENABLE_PI_TAPE
    sendToPiButton.repaint();
#endif
}

void MainComponent::updateWizardState()
{
    wizardSteps.setStepDone(WizardPhase::AddMusic, hasSource);
    wizardSteps.setStepDone(WizardPhase::EditTracks, hasSource);
    wizardSteps.setStepDone(WizardPhase::Preparing, hasProcessed);
    wizardSteps.setStepDone(WizardPhase::ReadyToExport, exportButton.isEnabled());

    const bool busy = isProcessing.load();
    const bool folderBusy = mixtapePanelBusy;
    const bool showTrackEditor = mixtapeModeActive && (hasSource || folderBusy) && !hasProcessed && !busy;

    if (!hasSource && !folderBusy)
        wizardSteps.setPhase(WizardPhase::AddMusic);
    else if (showTrackEditor || (folderBusy && mixtapeModeActive && !hasProcessed))
        wizardSteps.setPhase(WizardPhase::EditTracks);
    else if (!hasProcessed)
        wizardSteps.setPhase(WizardPhase::Preparing);
    else
        wizardSteps.setPhase(WizardPhase::ReadyToExport);

    dropHero.setVisible(!hasSource && !folderBusy);
    trackListEditor.setVisible(showTrackEditor);
    trackListEditor.setLoading(folderBusy && !hasSource);
    compareWaveform.setVisible(hasSource && !showTrackEditor);
    tapeSetupPanel.setMixtapeMode(mixtapeModeActive);
    tapeSetupPanel.setCompactToolbarMode(!mixtapeModeActive);

    dropHero.setInteractionEnabled(!busy);
    tapeSetupPanel.setInteractionEnabled(!busy);
    tapeSetupPanel.setTapeTypeLocked(hasProcessed && !busy);
    settingsButton.setEnabled(!busy);
    mixtapePanel.setBusy(busy);

    startButton.setVisible(!hasProcessed || busy);
    startButton.setEnabled(hasSource && !busy && !hasProcessed
                           && (!mixtapeModeActive || folderFitOk));
    trackListEditor.setInterceptsMouseClicks(showTrackEditor, showTrackEditor);
    startButton.setButtonText(tr("btn.prepare"));
    newButton.setEnabled((hasSource || hasProcessed) && !busy);
    exportButton.setEnabled(!busy && hasProcessed && loadedAudio.has_value());
#if CASSETTE_ENABLE_PI_TAPE
    sendToPiButton.setEnabled(!busy && hasProcessed && loadedAudio.has_value() && piTapeSettings.enabled
                                && piTapeSettings.isConfigured());
#endif

    syncTransportButtonStyles();
}

void MainComponent::updateReadySummary()
{
    readySummary.setVisible(false);
}

void MainComponent::updateWaveformInfo(const AudioFeatures& source, const AudioFeatures* processed)
{
    const auto dur = sourceAudio.has_value()
                         ? sourceAudio->buffer.getNumSamples() / sourceAudio->sampleRate
                         : (loadedAudio.has_value() ? loadedAudio->buffer.getNumSamples() / loadedAudio->sampleRate
                                                    : 0.0);

    WaveformCardInfo before;
    before.title = "Before";
    before.hasAudio = sourceAudio.has_value() || hasSource;
    before.subtitle = juce::String(source.integratedLUFS, 1) + " LUFS  |  "
                      + juce::String(dur / 60.0, 1) + " min";
    compareWaveform.setBeforeInfo(before);

    WaveformCardInfo after;
    after.title = "After";
    if (processed != nullptr)
    {
        after.hasAudio = true;
        after.subtitle = juce::String(processed->integratedLUFS, 1) + " LUFS  |  "
                         + juce::String(processed->truePeakDbfs, 1) + " dBTP";
    }
    compareWaveform.setAfterInfo(after);
}

MasteringOptions MainComponent::currentMasteringOptions() const
{
    auto options = tapeSetup().getMasteringOptions();
    options.enableTruePeakLimiter = processingChainOptions.enableTruePeakLimiter;
    options.enableStereoTightening = processingChainOptions.enableStereoTightening;
    return options;
}

TapeFormulation MainComponent::getSelectedProfile() const
{
    return tapeSetup().getTapeFormulation();
}

void MainComponent::paintProgressBar(juce::Graphics& g, juce::Rectangle<int> area) const
{
    if (area.isEmpty() || !isProcessing.load())
        return;

    auto r = area;
    ui::Theme::drawPanel(g, r, true);

    const float fill = static_cast<float>(juce::jlimit(0.0, 1.0, progress));
    if (fill > 0.001f)
    {
        auto fillR = r.withWidth(static_cast<int>(r.getWidth() * fill));
        g.setColour(ui::Theme::accent());
        g.fillRect(fillR);
    }

    const int pct = static_cast<int>(std::lround(fill * 100.0));
    g.setColour(fill > 0.55f ? juce::Colours::white : ui::Theme::textPrimary());
    g.setFont(ui::Theme::metricFont());
    g.drawText(juce::String(pct) + "%", r, juce::Justification::centred);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(ui::Theme::background());

    g.setColour(ui::Theme::border());
    g.drawVerticalLine(leftSidebarBounds.getRight(), 0.0f, static_cast<float>(getHeight()));
    if (kRightSidebarW > 0)
        g.drawVerticalLine(rightSidebarBounds.getX(), 0.0f, static_cast<float>(getHeight()));

    paintProgressBar(g, progressBounds);

    if (activeDropKind != DropPayloadKind::None)
    {
        g.setColour(ui::Theme::accent().withAlpha(0.08f));
        g.fillRect(getLocalBounds().reduced(1));
        g.setColour(ui::Theme::accent());
        g.drawRect(getLocalBounds().reduced(1), 2);
    }
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    leftSidebarBounds = bounds.removeFromLeft(kLeftSidebarW);
    if (kRightSidebarW > 0)
        rightSidebarBounds = bounds.removeFromRight(kRightSidebarW);
    else
        rightSidebarBounds = {};
    auto centre = bounds;

    auto sidebar = leftSidebarBounds.reduced(16, 18);
    settingsButton.setBounds(sidebar.removeFromBottom(36));

    newButton.setBounds(sidebar.removeFromTop(36));

    auto topBar = centre.removeFromTop(56).reduced(14, 12);

    if (!mixtapeModeActive)
    {
        tapeSetupPanel.setCompactToolbarMode(true);
        tapeSetupPanel.setBounds(topBar.removeFromLeft(196));
        topBar.removeFromLeft(12);
    }

    startButton.setBounds(topBar.removeFromLeft(112));
    topBar.removeFromLeft(10);
#if CASSETTE_ENABLE_PI_TAPE
    sendToPiButton.setBounds(topBar.removeFromRight(108));
    topBar.removeFromRight(8);
#endif
    exportButton.setBounds(topBar.removeFromRight(124));

    if (kRightSidebarW > 0)
    {
        auto right = rightSidebarBounds.reduced(0, 10);
        tapeSetupPanel.setBounds(right);
    }

    centre.removeFromTop(2);
    wizardSteps.setBounds(centre.removeFromTop(52).reduced(10, 0));
    centre.removeFromTop(6);

    if (mixtapeModeActive)
    {
        tapeSetupPanel.setCompactToolbarMode(false);
        const int tapePanelH = tapeSetupPanel.isCustomTapeLengthSelected() ? 168 : 136;
        tapeSetupPanel.setBounds(centre.removeFromTop(tapePanelH).reduced(12, 0));
        centre.removeFromTop(4);
    }

    if (!hasSource)
    {
        auto statusRow = centre.removeFromBottom(28).reduced(12, 4);
        progressBounds = statusRow.removeFromRight(168);
        statusRow.removeFromRight(8);
        status.setBounds(statusRow);
        dropHero.setBounds(centre.reduced(12, 8));
        return;
    }

    if (readySummary.isVisible())
    {
        readySummary.setBounds(centre.removeFromTop(22).reduced(14, 0));
        centre.removeFromTop(6);
    }

    auto statusRow = centre.removeFromBottom(28).reduced(12, 4);
    progressBounds = statusRow.removeFromRight(168);
    statusRow.removeFromRight(8);
    status.setBounds(statusRow);

    if (trackListEditor.isVisible())
    {
        trackListEditor.setBounds(centre.reduced(8, 4));
    }
    else
    {
        compareWaveform.setBounds(centre.reduced(8, 4));
    }
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() && key.getKeyCode() == 'N')
    {
        if (newButton.isEnabled())
            newButton.triggerClick();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == 'O')
    {
        pickImportFolder();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == juce::KeyPress::returnKey)
    {
        if (startButton.isEnabled())
            startButton.triggerClick();
        return true;
    }
    if (mods.isCommandDown() && key.getKeyCode() == 'E')
    {
        if (exportButton.isEnabled())
            exportButton.triggerClick();
        return true;
    }
    if (trackListEditor.isVisible() && trackListEditor.keyPressed(key))
        return true;

    if (auto* window = dynamic_cast<juce::DocumentWindow*>(getTopLevelComponent()))
    {
#if JUCE_MAC
        if (mods.isCommandDown() && mods.isCtrlDown() && key.getTextCharacter() == 'f')
        {
            window->setFullScreen(!window->isFullScreen());
            return true;
        }
#endif
        if (key.getKeyCode() == juce::KeyPress::F11Key)
        {
            window->setFullScreen(!window->isFullScreen());
            return true;
        }
    }

    return false;
}

void MainComponent::timerCallback()
{
    if (isProcessing.load())
        repaint(progressBounds);
}

void MainComponent::setProgress(double value)
{
    progress = juce::jlimit(0.0, 1.0, value);
    repaint(progressBounds);
}

void MainComponent::syncLayout()
{
    updateWizardState();
    resized();
}

void MainComponent::setUiProcessing(bool processing)
{
    isProcessing.store(processing);
    if (processing)
        progress = 0.0;
    if (!processing)
        refreshFolderFitLabel();
    syncLayout();
    repaint(progressBounds);
}

void MainComponent::resetSession()
{
    if (isProcessing.load())
        return;

    hasSource = false;
    hasProcessed = false;
    mixtapeModeActive = false;
    hasSideB = false;
    mixtapeCassetteCount = 1;
    folderScan.reset();
    mixtapeEditor.clear();
    loadedAudio.reset();
    sourceAudio.reset();
    mixtapeReferenceAudio.reset();
    sideAAudio.reset();
    sideBAudio.reset();
    lastQuality.reset();
    lastProcessedFeatures.reset();
    processingChainOptions = MasteringOptions {};
    loadedFile = juce::File();
    sideAPath = juce::File();
    sideBPath = juce::File();
    sideADurationSec = 0.0;
    sideBDurationSec = 0.0;

    preparedTapeLabel = {};
    compareWaveform.clearAll();
    readySummary.setVisible(false);
    exportButton.setEnabled(false);

    tapeSetupPanel.setMixtapeMode(false);
    tapeSetupPanel.setTapeFitSummary({}, true);
    folderFitOk = true;
    mixtapePanel.setFitReport("", true);
    mixtapePanelBusy = false;
    mixtapePanel.setBusy(false);

    setStatus({}, ui::Theme::textSecondary());
    syncLayout();
}

void MainComponent::invalidatePreparedOutput()
{
    hasProcessed = false;
    preparedTapeLabel = {};
    lastQuality.reset();
    lastProcessedFeatures.reset();

    if (sourceAudio.has_value())
    {
        loadedAudio = *sourceAudio;
        compareWaveform.setBeforeBuffer(sourceAudio->buffer, sourceAudio->sampleRate);
    }
    compareWaveform.clearAfter();
    readySummary.setVisible(false);

    setStatus(tr("status.choose_tape_then_prepare"), ui::Theme::textSecondary());
    syncLayout();
}

void MainComponent::showProcessingSettings()
{
#if CASSETTE_ENABLE_PI_TAPE
    ui::AppSettingsState state;
    state.mastering = processingChainOptions;
    state.piTape = piTapeSettings;

    ui::showAppSettingsDialog(this, state, [this](ui::AppSettingsState updated) {
        const bool processingChanged = updated.mastering.enableTruePeakLimiter != processingChainOptions.enableTruePeakLimiter
                                         || updated.mastering.enableStereoTightening
                                                != processingChainOptions.enableStereoTightening;
        processingChainOptions = updated.mastering;
        piTapeSettings = updated.piTape;
        piTapeSettings.save();
        updateWizardState();

        if (processingChanged && hasProcessed)
            invalidatePreparedOutput();
    });
#else
    ui::showAppSettingsDialog(this, processingChainOptions, [this](MasteringOptions options) {
        const bool processingChanged = options.enableTruePeakLimiter != processingChainOptions.enableTruePeakLimiter
                                       || options.enableStereoTightening != processingChainOptions.enableStereoTightening;
        processingChainOptions = options;
        if (processingChanged && hasProcessed)
            invalidatePreparedOutput();
    });
#endif
}

void MainComponent::promptChangeTapeType()
{
    if (!hasProcessed || isProcessing.load())
        return;

    ui::ConfirmDialogOptions options;
    options.title = tr("dialog.change_tape.title");
    options.message = tr("dialog.change_tape.message");
    options.confirmLabel = tr("dialog.change_tape.confirm");
    options.cancelLabel = tr("dialog.cancel");

    ui::showConfirmDialog(this, options, [this](bool confirmed) {
        if (confirmed)
            invalidatePreparedOutput();
    });
}

void MainComponent::pickImportFolder()
{
    auto chooser = std::make_shared<juce::FileChooser>(
        "Import folder",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        importFolderChooserWildcard());

    chooser->launchAsync(importFolderChooserFlags(),
                         [this, chooser](const juce::FileChooser& fc) {
                             const auto folder = folderFromImportPickerResult(fc.getResult());
                             if (folder.isDirectory())
                                 scanMixFolder(folder);
                         });
}

void MainComponent::refreshFolderFitLabel()
{
    if (!folderScan.has_value() || !folderScan->success)
    {
        folderFitOk = true;
        tapeSetupPanel.setTapeFitSummary({}, true);
        return;
    }

    const auto tape = tapeSetup().getTapeLengthSpec();
    const bool editorActive = !mixtapeEditor.sideA().empty() || !mixtapeEditor.sideB().empty();
    const auto report = editorActive ? mixtapeEditor.computeFullFit(tape)
                                     : FolderMixBuilder::analyzeFit(*folderScan, tape);

    folderFitOk = editorActive ? mixtapeEditor.canPrepare(tape) : report.fits;
    mixtapePanel.setFitReport(report.summary(), folderFitOk);
    mixtapePanel.setBuildEnabled(folderFitOk && !isProcessing.load());
    trackListEditor.setTapeSpec(tape);
}

void MainComponent::scanMixFolder(const juce::File& folder)
{
    log("UI: Import folder requested - " + folder.getFullPathName());

    folderScan.reset();
    hasProcessed = false;
    hasSource = false;
    mixtapePanelBusy = true;
    mixtapePanel.setBusy(true);
    mixtapeModeActive = true;
    tapeSetupPanel.setMixtapeMode(true);
    setStatus(trf("status.scanning", folder.getFileName()), ui::Theme::warnAmber());
    syncLayout();

    worker.enqueue([this, folder]() {
        const auto scan = FolderMixBuilder::scanFolder(folder);
        juce::MessageManager::callAsync([this, scan, folder]() {
            mixtapePanelBusy = false;
            mixtapePanel.setBusy(false);
            if (!scan.success)
            {
                log("UI: folder scan failed - " + scan.error);
                mixtapePanel.setFitReport(scan.error, false);
                setStatus(scan.error, ui::Theme::failRed());
                syncLayout();
                return;
            }
            folderScan = scan;
            mixtapePanel.setFolderScan(scan, folder);
            hasSource = true;
            log("UI: folder scan OK - " + juce::String(scan.tracks.size()) + " tracks");

            const auto fitReport = FolderMixBuilder::analyzeFit(scan, tapeSetup().getTapeLengthSpec());
            mixtapeEditor.loadFromScan(scan, fitReport);
            trackListEditor.setController(&mixtapeEditor);
            trackListEditor.setLoading(false);
            refreshFolderFitLabel();
            trackListEditor.refresh();
            setStatus({}, ui::Theme::textSecondary());
            syncLayout();
        });
    });
}

void MainComponent::loadAudioFile(const juce::File& file)
{
    loadedFile = file;
    loadedAudio.reset();
    sourceAudio.reset();
    hasProcessed = false;
    hasSource = false;
    preparedTapeLabel = {};
    lastQuality.reset();
    lastProcessedFeatures.reset();
    hasSideB = false;
    mixtapeModeActive = false;
    mixtapeReferenceAudio.reset();
    tapeSetupPanel.setMixtapeMode(false);
    compareWaveform.clearAfter();
    exportButton.setEnabled(false);
    setStatus(trf("status.loading", file.getFileName()), ui::Theme::warnAmber());

    worker.enqueue([this, file]() {
        const auto result = AudioFileLoader::loadToBufferWithDiagnostics(file);
        juce::MessageManager::callAsync([this, file, result]() {
            if (!result.audio.hasValue())
            {
                setStatus(trf("status.load_failed", result.error), ui::Theme::failRed());
                return;
            }

            loadedAudio = *result.audio;
            sourceAudio = *result.audio;
            hasSource = true;
            compareWaveform.setBeforeBuffer(sourceAudio->buffer, sourceAudio->sampleRate);
            compareWaveform.clearAfter();

            const auto features = EssentiaAnalyzer::extractFeatures(loadedAudio->buffer, loadedAudio->sampleRate);
            updateWaveformInfo(features, nullptr);

            setStatus(trf("status.added", file.getFileName()), ui::Theme::okGreen());
            syncLayout();
        });
    });
}

void MainComponent::startProcessing()
{
    if (!loadedAudio.has_value())
    {
        setStatus(tr("status.add_music_first"), ui::Theme::warnAmber());
        return;
    }
    if (isProcessing.load())
        return;

    const auto profile = tapeSetup().getCassetteProfile();
    const auto options = currentMasteringOptions();
    const auto fileName = loadedFile.getFileName();

    if (!sourceAudio.has_value())
        sourceAudio = *loadedAudio;

    auto audioCopy = *loadedAudio;
    setUiProcessing(true);
    setStatus(tr("status.preparing"), ui::Theme::warnAmber());
    wizardSteps.setPhase(WizardPhase::Preparing);

    worker.enqueue([this, audioCopy, profile, options, fileName]() mutable {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        juce::MessageManager::callAsync([this]() { setProgress(0.05); });

        juce::AudioBuffer<float> reference;
        reference.makeCopyOf(audioCopy.buffer);
        const auto features = EssentiaAnalyzer::extractFeatures(reference, audioCopy.sampleRate);
        juce::MessageManager::callAsync([this]() { setProgress(0.18); });

        const auto mastered = AdaptiveMasteringProcessor::process(
            audioCopy.buffer, profile, options, audioCopy.sampleRate);
        juce::MessageManager::callAsync([this]() { setProgress(0.82); });

        juce::MessageManager::callAsync([this]() { setProgress(0.96); });
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;

        juce::MessageManager::callAsync([this, audioCopy, mastered, fileName, ms, features, profile]() mutable {
            setProgress(1.0);
            loadedAudio = std::move(audioCopy);
            lastQuality = mastered.quality;
            hasProcessed = true;
            preparedTapeLabel = profile.displayName;

            if (sourceAudio.has_value())
                compareWaveform.setBeforeBuffer(sourceAudio->buffer, sourceAudio->sampleRate);
            compareWaveform.setAfterBuffer(loadedAudio->buffer, loadedAudio->sampleRate);

            lastProcessedFeatures = mastered.featuresAfter;
            updateWaveformInfo(features, &mastered.featuresAfter);

            juce::ignoreUnused(ms);
            finishProcessing(true, {});
            exportButton.setEnabled(true);
            updateReadySummary();
            syncLayout();
        });
    });
}

void MainComponent::finishProcessing(bool success, const juce::String& message)
{
    setUiProcessing(false);
    setStatus(message, success ? ui::Theme::okGreen() : ui::Theme::failRed());
}

void MainComponent::startFolderMixBuild()
{
    if (!folderScan.has_value() || !folderScan->success)
    {
        setStatus(tr("status.pick_folder"), ui::Theme::warnAmber());
        return;
    }

    const auto tape = tapeSetup().getTapeLengthSpec();
    if (!mixtapeEditor.canPrepare(tape))
    {
        const auto fit = mixtapeEditor.computeFit(tape);
        setStatus(fit.summary(), ui::Theme::warnAmber());
        return;
    }
    if (isProcessing.load())
        return;

    const auto profile = tapeSetup().getCassetteProfile();
    auto options = currentMasteringOptions();
    options.skipQualityCompare = true;

    mixtapeEditor.syncCassettePlan(tape);
    const auto scanCopy = mixtapeEditor.mergedFullScan();
    const auto fit = mixtapeEditor.computeFullFit(tape);
    const int cassetteCount = mixtapeEditor.getCassetteCount();
    const auto gapSec = mixtapeEditor.gapBetweenTracksSec();
    std::vector<MixtapeEditController::LayoutSnapshot> cassetteLayouts;
    cassetteLayouts.reserve(static_cast<size_t>(cassetteCount));
    for (int i = 0; i < cassetteCount; ++i)
        cassetteLayouts.push_back(mixtapeEditor.layoutForCassette(i));
    const auto projectName = mixtapePanel.currentFolder().getFileName();
    const juce::File outFolder = mixtapePanel.currentFolder();
    const auto allowedSec = tape.minutesPerSide * 60.0;

    setUiProcessing(true);
    wizardSteps.setPhase(WizardPhase::Preparing);
    setStatus(cassetteCount > 1 ? trf("status.preparing_cassettes", cassetteCount)
                                : tr("status.preparing_sides"),
              ui::Theme::warnAmber());

    log("UI: Build mixtape started - " + juce::String(scanCopy.tracks.size()) + " tracks, "
        + juce::String(cassetteCount) + " cassette(s), out=" + outFolder.getFullPathName());

    worker.enqueue([this,
                    scanCopy,
                    cassetteLayouts,
                    gapSec,
                    profile,
                    options,
                    projectName,
                    outFolder,
                    allowedSec,
                    fit,
                    cassetteCount]() {
        try
        {
            ScopedTimer buildTimer("folder-build", projectName + " (" + juce::String(scanCopy.tracks.size()) + " tracks)");
            const double sampleRate = kProjectSampleRate;
            const int totalTracks = static_cast<int>(scanCopy.tracks.size());

            const auto trackProgress = [this, totalTracks](int tracksDone, int, const juce::String& title) {
                const auto msg = "Track " + juce::String(tracksDone) + "/" + juce::String(totalTracks) + ": " + title;
                const double pct = juce::jlimit(0.0, 0.98, static_cast<double>(tracksDone) / static_cast<double>(totalTracks));
                juce::MessageManager::callAsync([this, msg, pct]() {
                    setProgress(pct);
                    status.setText(msg, juce::dontSendNotification);
                    status.setColour(juce::Label::textColourId, ui::Theme::warnAmber());
                });
            };

            juce::String workerError;
            std::shared_ptr<RenderResult> previewA;
            std::shared_ptr<RenderResult> previewB;
            juce::File previewSideAFile;
            juce::File previewSideBFile;
            CassettePlan previewPlan;

            int trackOffset = 0;
            for (int cassetteIdx = 0; cassetteIdx < cassetteCount && workerError.isEmpty(); ++cassetteIdx)
            {
                const auto& layout = cassetteLayouts[static_cast<size_t>(cassetteIdx)];
                const int sideATracks = static_cast<int>(layout.sideA.size());
                const int sideBTracks = static_cast<int>(layout.sideB.size());
                const int cassetteTrackCount = sideATracks + sideBTracks;

                auto project = FolderMixBuilder::buildProjectFromSides(layout.sideA,
                                                                       layout.sideB,
                                                                       outFolder,
                                                                       gapSec,
                                                                       projectName,
                                                                       profile,
                                                                       options,
                                                                       allowedSec);

                const juce::String cassetteLabel = cassetteCount > 1
                                                       ? projectName + " Cassette " + juce::String(cassetteIdx + 1)
                                                       : projectName;

                const auto sideAProgress = [&, start = trackOffset](int finishedOnSide, int, const juce::String& title) {
                    trackProgress(start + finishedOnSide, totalTracks, title);
                };

                const bool captureReference = previewA == nullptr;
                auto renderedA = SideRenderer::renderSide(project, false, sampleRate, false, sideAProgress, captureReference);
                if (!renderedA.success)
                {
                    workerError = renderedA.error;
                    break;
                }

                juce::File sideAFile = outFolder.getChildFile(cassetteLabel + " Side A.wav");
                if (!WavExporter::writeWav32Float(sideAFile, renderedA.buffer, renderedA.sampleRate))
                {
                    workerError = "Failed to write " + sideAFile.getFileName();
                    break;
                }

                if (previewA == nullptr)
                {
                    previewA = std::make_shared<RenderResult>(std::move(renderedA));
                    previewSideAFile = sideAFile;
                    previewPlan.sideAStartTrack = 0;
                    previewPlan.sideAEndTrack = sideATracks;
                    previewPlan.sideADurationSec = FolderMixBuilder::sideDurationSec(layout.sideA, gapSec);
                }

                if (sideBTracks > 0)
                {
                    const auto sideBProgress = [&, start = trackOffset + sideATracks](int finishedOnSide,
                                                                                      int,
                                                                                      const juce::String& title) {
                        trackProgress(start + finishedOnSide, totalTracks, title);
                    };

                    auto renderedB = SideRenderer::renderSide(project, true, sampleRate, false, sideBProgress);
                    if (!renderedB.success)
                    {
                        workerError = renderedB.error;
                        break;
                    }

                    juce::File sideBFile = outFolder.getChildFile(cassetteLabel + " Side B.wav");
                    if (!WavExporter::writeWav32Float(sideBFile, renderedB.buffer, renderedB.sampleRate))
                    {
                        workerError = "Failed to write " + sideBFile.getFileName();
                        break;
                    }

                    if (previewB == nullptr && cassetteCount == 1)
                    {
                        previewB = std::make_shared<RenderResult>(std::move(renderedB));
                        previewSideBFile = sideBFile;
                        previewPlan.hasSideB = true;
                        previewPlan.sideBStartTrack = sideATracks;
                        previewPlan.sideBEndTrack = cassetteTrackCount;
                        previewPlan.sideBDurationSec = FolderMixBuilder::sideDurationSec(layout.sideB, gapSec);
                    }
                }

                trackOffset += cassetteTrackCount;
            }

            juce::MessageManager::callAsync([this]() { setProgress(0.99); });

            juce::MessageManager::callAsync([this,
                                             previewA,
                                             previewB,
                                             scanCopy,
                                             projectName,
                                             fit,
                                             previewPlan,
                                             previewSideAFile,
                                             previewSideBFile,
                                             workerError,
                                             profile,
                                             cassetteCount]() mutable {
                if (workerError.isNotEmpty())
                {
                    finishProcessing(false, workerError);
                    return;
                }

                if (previewA == nullptr)
                {
                    finishProcessing(false, "No cassette output produced");
                    return;
                }

                mixtapeCassetteCount = cassetteCount;

                LoadedAudio sideA;
                sideA.buffer = std::move(previewA->buffer);
                sideA.sampleRate = previewA->sampleRate;
                sideAAudio = std::move(sideA);
                sideAPath = previewSideAFile;
                sideADurationSec = sideAAudio->buffer.getNumSamples() / sideAAudio->sampleRate;
                hasSideB = previewB != nullptr;

                if (previewA->referenceBuffer.getNumSamples() > 0)
                {
                    LoadedAudio reference;
                    reference.buffer = std::move(previewA->referenceBuffer);
                    reference.sampleRate = previewA->sampleRate;
                    mixtapeReferenceAudio = std::move(reference);
                }

                if (hasSideB && previewB != nullptr)
                {
                    LoadedAudio sideB;
                    sideB.buffer = std::move(previewB->buffer);
                    sideB.sampleRate = previewB->sampleRate;
                    sideBAudio = std::move(sideB);
                    sideBPath = previewSideBFile;
                    sideBDurationSec = sideBAudio->buffer.getNumSamples() / sideBAudio->sampleRate;
                }

                showMixtapeSide(false);
                hasProcessed = true;
                preparedTapeLabel = profile.displayName;
                exportButton.setEnabled(true);

                updateReadySummary();
                const juce::String doneMsg = cassetteCount > 1
                                                 ? juce::String(cassetteCount) + " cassettes saved next to your music"
                                                 : "Side A/B WAV files saved next to your music";
                setProgress(1.0);
                finishProcessing(true, doneMsg);
            });
        }
        catch (const std::exception& e)
        {
            juce::MessageManager::callAsync([this, msg = juce::String(e.what())]() {
                finishProcessing(false, "Processing failed: " + msg);
            });
        }
        catch (...)
        {
            juce::MessageManager::callAsync([this]() {
                finishProcessing(false, "Processing failed unexpectedly");
            });
        }
    });
}

void MainComponent::showMixtapeSide(bool sideB)
{
    if (!sideAAudio.has_value())
        return;

    const auto& audio = sideB && sideBAudio.has_value() ? *sideBAudio : *sideAAudio;
    loadedAudio = audio;
    loadedFile = sideB ? sideBPath : sideAPath;

    if (!sideB && mixtapeReferenceAudio.has_value())
    {
        sourceAudio = *mixtapeReferenceAudio;
        compareWaveform.setBeforeBuffer(sourceAudio->buffer, sourceAudio->sampleRate);
    }
    else
    {
        juce::AudioBuffer<float> empty;
        compareWaveform.setBeforeBuffer(empty, audio.sampleRate);
    }

    compareWaveform.setAfterBuffer(audio.buffer, audio.sampleRate);

    const double dur = audio.buffer.getNumSamples() / audio.sampleRate;

    if (sourceAudio.has_value() && sourceAudio->buffer.getNumSamples() > 0)
    {
        const auto srcExcerpt = EssentiaAnalyzer::excerpt(sourceAudio->buffer, sourceAudio->sampleRate);
        const auto procExcerpt = EssentiaAnalyzer::excerpt(audio.buffer, audio.sampleRate);
        const auto srcFeatures = EssentiaAnalyzer::extractFeaturesForMastering(srcExcerpt, sourceAudio->sampleRate);
        const auto procFeatures = EssentiaAnalyzer::extractFeaturesForMastering(procExcerpt, audio.sampleRate);
        lastProcessedFeatures = procFeatures;
        updateWaveformInfo(srcFeatures, &procFeatures);

        WaveformCardInfo before;
        before.title = "Before";
        before.hasAudio = true;
        before.subtitle = juce::String(srcFeatures.integratedLUFS, 1) + " LUFS  |  "
                          + juce::String(dur / 60.0, 1) + " min";
        compareWaveform.setBeforeInfo(before);

        WaveformCardInfo after;
        after.title = "After";
        after.hasAudio = true;
        after.subtitle = juce::String(procFeatures.integratedLUFS, 1) + " LUFS  |  "
                         + juce::String(procFeatures.truePeakDbfs, 1) + " dBTP";
        compareWaveform.setAfterInfo(after);
    }
    else
    {
        const auto procExcerpt = EssentiaAnalyzer::excerpt(audio.buffer, audio.sampleRate);
        const auto features = EssentiaAnalyzer::extractFeaturesForMastering(procExcerpt, audio.sampleRate);
        lastProcessedFeatures = features;
        updateWaveformInfo(features, &features);
    }
}

void MainComponent::updateDropHighlight(const juce::StringArray& files, bool active)
{
    activeDropKind = active ? classifyDropPayload(files) : DropPayloadKind::None;
    dropHero.setDragHighlight(active, activeDropKind);
    compareWaveform.setDragHighlight(activeDropKind != DropPayloadKind::None, activeDropKind);
    repaint();
}

bool MainComponent::isInterestedInDrop(const juce::StringArray& files) const
{
    if (isProcessing.load())
        return false;

    return isDropPayloadInterested(files);
}

#if CASSETTE_ENABLE_PI_TAPE

void MainComponent::maybeCleanupExpiredPiTapeInbox()
{
    if (!piTapeSettings.enabled || !piTapeSettings.isConfigured())
        return;

    const auto session = PiTapeSessionState::load();
    if (!session.hasActiveSession() || !session.isExpired())
        return;

    const auto settings = piTapeSettings;
    worker.enqueue([settings]() {
        PiTapeControlClient client;
        client.cleanupSession(settings);
        PiTapeSessionState::clearPersisted();
    });
}

void MainComponent::cleanupPiTapeInboxOnExit()
{
    if (!piTapeSettings.enabled || !piTapeSettings.isConfigured())
        return;

    const auto session = PiTapeSessionState::load();
    if (!session.hasActiveSession())
        return;

    PiTapeControlClient().cleanupSession(piTapeSettings);
    PiTapeSessionState::clearPersisted();
}

void MainComponent::showPiTapeControl()
{
    if (!loadedAudio.has_value())
    {
        setStatus(tr("status.prepare_first"), ui::Theme::warnAmber());
        return;
    }

    if (!piTapeSettings.enabled || !piTapeSettings.isConfigured())
    {
        setStatus(tr("status.pi_tape.configure"), ui::Theme::warnAmber());
        showProcessingSettings();
        return;
    }

    ui::PiTapeControlSession session;
    session.settings = piTapeSettings;

    if (mixtapeModeActive && sideAPath.existsAsFile())
    {
        session.queue.sideA = sideAPath;
        session.queue.hasSideB = hasSideB && sideBPath.existsAsFile();
        if (session.queue.hasSideB)
            session.queue.sideB = sideBPath;
        session.queue.projectName = folderScan.has_value() ? folderScan->folder.getFileName()
                                                           : loadedFile.getFileNameWithoutExtension();
    }
    else
    {
        session.queue.sideA = loadedFile;
        session.queue.hasSideB = false;
        session.queue.projectName = loadedFile.getFileNameWithoutExtension();
    }

    ui::showPiTapeControlDialog(this, worker, std::move(session));
}

#endif

void MainComponent::exportWav()
{
    if (!loadedAudio.has_value())
    {
        setStatus(tr("status.prepare_first"), ui::Theme::warnAmber());
        return;
    }

    auto chooser = std::make_shared<juce::FileChooser>(
        "Export WAV",
        loadedFile.getParentDirectory().exists() ? loadedFile.getParentDirectory()
                                                 : juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.wav");

    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                         [this, chooser](const juce::FileChooser& fc) {
                             const auto out = fc.getResult();
                             if (out == juce::File())
                                 return;

                             juce::AudioBuffer<float> exportBuffer;
                             exportBuffer.makeCopyOf(loadedAudio->buffer);

                             const bool preflight = tapeSetup().isPreflightEnabled();
                             if (preflight)
                             {
                                 PreflightOptions preflightOptions;
                                 preflightOptions.title = loadedFile.getFileNameWithoutExtension();
                                 preflightOptions.sideLabel = out.getFileName().containsIgnoreCase("Side B") ? "Side B"
                                                                                                           : "Side A";
                                 preflightOptions.kenwoodKX1100Calibration =
                                     tapeSetup().getRecordingDeck() == RecordingDeck::KenwoodKX1100G;
                                 PreflightTones::prependToBuffer(exportBuffer, loadedAudio->sampleRate, preflightOptions);
                             }

                             if (WavExporter::writeWav32Float(out, exportBuffer, loadedAudio->sampleRate))
                             {
                                 wizardSteps.setStepDone(WizardPhase::ReadyToExport, true);
                                 setStatus(trf("status.exported", out.getFileName()), ui::Theme::okGreen());
                                 updateWizardState();
                             }
                             else
                                 setStatus(tr("status.export_failed"), ui::Theme::failRed());
                         });
}

bool MainComponent::isInterestedInAudioFileDrag(const juce::StringArray& files)
{
    return isInterestedInDrop(files);
}

void MainComponent::handleAudioFilesDropped(const juce::StringArray& files, int, int)
{
    if (isProcessing.load())
        return;

    for (const auto& f : files)
    {
        if (AudioFileLoader::normaliseDroppedPath(f).isDirectory())
        {
            scanMixFolder(AudioFileLoader::normaliseDroppedPath(f));
            return;
        }
    }

    const auto file = AudioFileLoader::pickFirstAudioFile(files);
    if (file.existsAsFile())
        loadAudioFile(file);
    else
        setStatus(tr("status.unsupported_drop"), ui::Theme::warnAmber());
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    return isInterestedInDrop(files);
}

void MainComponent::fileDragEnter(const juce::StringArray& files, int, int)
{
    if (!isInterestedInDrop(files))
        return;
    ++windowDragDepth;
    updateDropHighlight(files, true);
}

void MainComponent::fileDragExit(const juce::StringArray&)
{
    if (windowDragDepth <= 0)
        return;
    --windowDragDepth;
    if (windowDragDepth == 0)
        updateDropHighlight({}, false);
}

void MainComponent::filesDropped(const juce::StringArray& files, int x, int y)
{
    windowDragDepth = 0;
    updateDropHighlight({}, false);
    handleAudioFilesDropped(files, x, y);
}

void MainComponent::buttonClicked(juce::Button* button)
{
    if (button == &newButton)
    {
        resetSession();
        return;
    }
    if (button == &settingsButton)
    {
        showProcessingSettings();
        return;
    }
    if (button == &startButton)
    {
        if (mixtapeModeActive && folderScan.has_value())
            startFolderMixBuild();
        else
            startProcessing();
        return;
    }
    if (button == &exportButton)
    {
        exportWav();
        return;
    }
#if CASSETTE_ENABLE_PI_TAPE
    if (button == &sendToPiButton)
    {
        showPiTapeControl();
        return;
    }
#endif
}

}
