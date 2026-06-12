#include "CassetteBurnerComponent.h"
#include "../look/CassetteBurnerLook.h"
#include "../UiTheme.h"
#include "../../project/SideRenderer.h"
#include "../../dsp/AudioConstants.h"
#include "../../export/WavExporter.h"
#include "../../export/PreflightTones.h"

namespace cassette
{

namespace
{
juce::String formatClock(double sec, double total)
{
    auto fmt = [](double t) {
        const int m = static_cast<int>(t) / 60;
        const int s = static_cast<int>(t) % 60;
        return juce::String::formatted("%02d:%02d", m, s);
    };
    return fmt(sec) + " / " + fmt(total);
}
}

CassetteBurnerComponent::CassetteBurnerComponent()
{
    project = MixtapeProject::demoProject();
    projectFile = MixtapeProject::defaultProjectsFolder().getChildFile("Mixtape_Vol_1.cassetteproj");
    project.saveToFile(projectFile);

    addAndMakeVisible(sidebar);
    addAndMakeVisible(transportBar);
    addAndMakeVisible(timeline);
    addChildComponent(trackListEditor);
    addAndMakeVisible(params);
    addAndMakeVisible(statusBar);

    trackListEditor.onLayoutChanged = [this] { syncProjectFromEditor(); };
    trackListEditor.setTapeSpec(folderTape);

    timeline.setProject(&project);
    timeline.addListener(this);
    timeline.onStatus = [this](const juce::String& t) { statusBar.setMessage(t); };

    sidebar.setProjectNames({ "Mixtape Vol. 1", "Demo Tapes", "Live Set '23" }, 0);
    sidebar.onNewProject = [this] {
        project = MixtapeProject();
        project.name = "Untitled Mixtape";
        projectFile = MixtapeProject::defaultProjectsFolder().getChildFile("Untitled.cassetteproj");
        folderEditMode = false;
        trackListEditor.setVisible(false);
        timeline.setVisible(true);
        refreshUiFromProject();
        statusBar.setMessage("New project");
    };
    sidebar.onImportFolder = [this] {
        auto chooser = std::make_shared<juce::FileChooser>("Import folder",
                                                           juce::File(),
                                                           "*",
                                                           true);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                             [this, chooser](const juce::FileChooser& fc) {
                                 const auto f = fc.getResult();
                                 if (f.isDirectory())
                                     scanFolderForEditor(f);
                             });
    };
    sidebar.onImportAudio = [this] {
        auto chooser = std::make_shared<juce::FileChooser>("Import audio",
                                                           juce::File(),
                                                           "*.wav;*.flac;*.aiff;*.aif");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [this, chooser](const juce::FileChooser& fc) {
                                 const auto f = fc.getResult();
                                 if (f.existsAsFile())
                                 {
                                     juce::StringArray arr;
                                     arr.add(f.getFullPathName());
                                     timeline.importFilesAt(arr, timeline.getWidth() / 2, timeline.getHeight() / 4);
                                 }
                             });
    };

    transportBar.onPlay = [this] {
        if (!previewReady)
            rebuildPreviewForActiveSide();
        else
        {
            preview.setPlaying(true);
            transportBar.setPlaying(true);
        }
    };
    transportBar.onPause = [this] {
        preview.setPlaying(false);
        transportBar.setPlaying(false);
    };
    transportBar.onStop = [this] {
        preview.setPlaying(false);
        preview.setPlayheadSec(0.0);
        timeline.setPlayheadSec(previewSide, 0.0);
        transportBar.setPlaying(false);
    };

    params.onTapeTypeChanged = [this](TapeFormulation f) {
        project.profile = CassetteProfile::fromFormulation(f);
        refreshUiFromProject();
        saveProject();
    };
    params.onRecLevelChanged = [this](float db) {
        project.recLevelDb = db;
        saveProject();
    };
    params.onBiasChanged = [this](float db) {
        project.biasDb = db;
        saveProject();
    };
    params.onMaximumDigitalChanged = [this](bool on) {
        project.mastering.maximumDigital = on;
        saveProject();
    };
    params.onPreviewSettingsChanged = [this](const PerceptualPlaybackSettings& s) {
        preview.setPlaybackSettings(s);
    };
    params.onRenderSideA = [this] { renderSide(false); };
    params.onRenderSideB = [this] { renderSide(true); };
    params.onExportMix = [this] { renderSide(false); };

    deviceManager.initialiseWithDefaultDevices(0, 2);
    audioPlayer.setSource(&preview);
    preview.setMonitoringEnabled(true);
    deviceManager.addAudioCallback(this);

    refreshUiFromProject();
    startTimerHz(15);
    statusBar.setMessage("CassetteBurner ready — drop audio on timeline");
}

CassetteBurnerComponent::~CassetteBurnerComponent()
{
    deviceManager.removeAudioCallback(this);
    audioPlayer.setSource(nullptr);
    deviceManager.closeAudioDevice();
}

void CassetteBurnerComponent::paint(juce::Graphics& g)
{
    g.fillAll(CassetteBurnerLook::background());
}

void CassetteBurnerComponent::resized()
{
    auto r = getLocalBounds();
    statusBar.setBounds(r.removeFromBottom(28));
    auto left = r.removeFromLeft(200);
    sidebar.setBounds(left);
    auto right = r.removeFromRight(260);
    params.setBounds(right);
    auto top = r.removeFromTop(48);
    transportBar.setBounds(top);

    if (folderEditMode)
    {
        timeline.setVisible(false);
        trackListEditor.setVisible(true);
        trackListEditor.setBounds(r);
    }
    else
    {
        trackListEditor.setVisible(false);
        timeline.setVisible(true);
        timeline.setBounds(r);
    }
}

void CassetteBurnerComponent::scanFolderForEditor(const juce::File& folder)
{
    statusBar.setMessage("Scanning " + folder.getFileName() + "...");
    worker.enqueue([this, folder]() {
        const auto scan = FolderMixBuilder::scanFolder(folder);
        juce::MessageManager::callAsync([this, scan, folder]() {
            if (!scan.success)
            {
                statusBar.setMessage(scan.error);
                return;
            }

            const auto fit = FolderMixBuilder::analyzeFit(scan, folderTape);
            folderEditor.loadFromScan(scan, fit);
            trackListEditor.setController(&folderEditor);
            trackListEditor.setTapeSpec(folderTape);
            trackListEditor.refresh();

            project.name = folder.getFileName();
            syncProjectFromEditor();

            folderEditMode = true;
            projectFile = MixtapeProject::defaultProjectsFolder().getChildFile(project.name + ".cassetteproj");
            saveProject();
            resized();
            statusBar.setMessage(juce::String(scan.tracks.size()) + " tracks loaded — drag to reorder");
        });
    });
}

void CassetteBurnerComponent::syncProjectFromEditor()
{
    const double allowedSec = folderTape.minutesPerSide * 60.0;
    const auto profile = project.profile;
    project = folderEditor.buildPreviewProject(allowedSec);
    project.name = folderEditor.sourceFolder().getFileName();
    project.profile = profile;
    timeline.setProject(&project);
    refreshUiFromProject();
    saveProject();
}

void CassetteBurnerComponent::timelineProjectChanged()
{
    refreshUiFromProject();
    saveProject();
    previewReady = false;
}

void CassetteBurnerComponent::timelineSelectionChanged(int sideIndex, int)
{
    previewSide = sideIndex;
    transportBar.setSideLabel("SIDE " + juce::String(sideIndex == 0 ? "A" : "B"));
}

void CassetteBurnerComponent::audioDeviceIOCallbackWithContext(const float* const* in,
                                                                int numIn,
                                                                float* const* out,
                                                                int numOut,
                                                                int numSamples,
                                                                const juce::AudioIODeviceCallbackContext& ctx)
{
    juce::ignoreUnused(in, numIn, ctx);
    audioPlayer.audioDeviceIOCallbackWithContext(nullptr, 0, out, numOut, numSamples, ctx);
}

void CassetteBurnerComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    const auto sr = device->getCurrentSampleRate();
    const auto block = device->getCurrentBufferSizeSamples();
    preview.prepareToPlay(block, sr);
    juce::ignoreUnused(block, sr);
}

void CassetteBurnerComponent::audioDeviceStopped()
{
    preview.releaseResources();
}

void CassetteBurnerComponent::timerCallback()
{
    const auto ph = preview.getPlayheadSec();
    timeline.setPlayheadSec(previewSide, ph);
    const auto& side = previewSide == 0 ? project.sideA : project.sideB;
    transportBar.setTimeDisplay(formatClock(ph, side.maxDurationSec));
    transportBar.setPlaying(preview.isPlaying());

    if (preview.isPlaying())
    {
        const auto level = preview.getPlayheadSec() > 0.0 ? -12.0f : -80.0f;
        params.setVuLevels(level, level - 1.0f);
    }
}

void CassetteBurnerComponent::refreshUiFromProject()
{
    timeline.repaint();
    params.bindProject(project);
    const auto& side = previewSide == 0 ? project.sideA : project.sideB;
    transportBar.setTimeDisplay(formatClock(0.0, side.maxDurationSec));
}

void CassetteBurnerComponent::saveProject()
{
    project.saveToFile(projectFile);
}

void CassetteBurnerComponent::rebuildPreviewForActiveSide()
{
    statusBar.setMessage("Building preview...");
    const int side = previewSide;
    auto projectCopy = project;

    worker.enqueue([this, projectCopy, side]() {
        const auto result = SideRenderer::renderSide(projectCopy, side == 1, kProjectSampleRate);
        juce::MessageManager::callAsync([this, result, side]() {
            if (!result.success)
            {
                statusBar.setMessage("Preview failed: " + result.error);
                previewReady = false;
                return;
            }
            preview.setBuffer(result.buffer, result.sampleRate);
            preview.setPlaybackSettings(params.previewSettingsState());
            preview.setPlayheadSec(0.0);
            previewReady = true;
            preview.setPlaying(true);
            transportBar.setPlaying(true);
            statusBar.setMessage("Preview ready — playing");
            juce::ignoreUnused(side);
        });
    });
}

void CassetteBurnerComponent::renderSide(bool sideB)
{
    if (!(sideB ? project.sideB.fitsOnTape() : project.sideA.fitsOnTape()))
    {
        statusBar.setMessage("Cannot render: side overflows 45:00");
        return;
    }

    const auto defaultName = project.name + (sideB ? "_SideB.wav" : "_SideA.wav");
    auto chooser = std::make_shared<juce::FileChooser>("Render tape side",
                                                       juce::File::getSpecialLocation(
                                                           juce::File::userDesktopDirectory)
                                                           .getChildFile(defaultName),
                                                       "*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                         [this, chooser, sideB](const juce::FileChooser& fc) {
                             const auto out = fc.getResult();
                             if (out == juce::File())
                                 return;

                             statusBar.setMessage("Rendering...");
                             const auto projectCopy = project;
                             worker.enqueue([this, projectCopy, sideB, out]() {
                                 const auto result = SideRenderer::renderSide(projectCopy, sideB, kProjectSampleRate);
                                 juce::MessageManager::callAsync([this, result, out]() {
                                     if (!result.success)
                                     {
                                         statusBar.setMessage("Render failed: " + result.error);
                                         return;
                                     }
                                     if (WavExporter::writeWav32Float(out, result.buffer, result.sampleRate))
                                         statusBar.setMessage("Rendered: " + out.getFileName());
                                     else
                                         statusBar.setMessage("Write failed");
                                 });
                             });
                         });
}

}
