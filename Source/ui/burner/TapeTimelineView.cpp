#include "TapeTimelineView.h"
#include "../look/CassetteBurnerLook.h"
#include "../../io/AudioFileLoader.h"

namespace cassette
{

namespace
{
juce::String formatTime(double sec)
{
    const int m = static_cast<int>(sec) / 60;
    const int s = static_cast<int>(sec) % 60;
    return juce::String::formatted("%d:%02d", m, s);
}

juce::Colour clipColour(int index)
{
    static const juce::Colour cols[] = {
        juce::Colour(0xff3b82f6),
        juce::Colour(0xff22c55e),
        juce::Colour(0xff6b7280),
        juce::Colour(0xff8b5cf6),
        juce::Colour(0xfff59e0b),
    };
    return cols[index % 5];
}
}

TapeTimelineView::TapeTimelineView() { setWantsKeyboardFocus(true); }

void TapeTimelineView::setProject(MixtapeProject* p)
{
    project = p;
    repaint();
}

void TapeTimelineView::setPlayheadSec(int sideIndex, double sec)
{
    if (sideIndex == 0)
        playheadA = sec;
    else
        playheadB = sec;
    repaint();
}

juce::Rectangle<int> TapeTimelineView::getSideBounds(int sideIndex) const
{
    const int h = (getHeight() - 8) / 2;
    return { 8, 4 + sideIndex * (h + 4), getWidth() - 16, h };
}

double TapeTimelineView::xToTime(int x, const juce::Rectangle<int>& sideArea) const
{
    const int rulerW = 48;
    const auto timeline = sideArea.withTrimmedLeft(rulerW);
    if (timeline.getWidth() <= 0 || !project)
        return 0.0;
    const auto rel = juce::jlimit(0.0, 1.0, (x - timeline.getX()) / static_cast<double>(timeline.getWidth()));
    return rel * project->sideA.maxDurationSec;
}

int TapeTimelineView::yToTrack(int y, const juce::Rectangle<int>& sideArea) const
{
    const int header = 22;
    const auto tracks = sideArea.withTrimmedTop(header);
    const int lanes = 5;
    const int laneH = juce::jmax(12, tracks.getHeight() / lanes);
    const int idx = (y - tracks.getY()) / laneH;
    return juce::jlimit(1, 5, idx + 1);
}

TapeTimelineView::HitClip TapeTimelineView::hitTest(juce::Point<int> pos) const
{
    HitClip hit;
    if (!project)
        return hit;

    for (int s = 0; s < 2; ++s)
    {
        const auto& side = s == 0 ? project->sideA : project->sideB;
        for (int i = 0; i < static_cast<int>(side.clips.size()); ++i)
        {
            if (clipBounds(s, i).contains(pos))
            {
                hit.sideIndex = s;
                hit.clipIndex = i;
                return hit;
            }
        }
    }
    return hit;
}

juce::Rectangle<int> TapeTimelineView::clipBounds(int sideIndex, int clipIndex) const
{
    if (!project)
        return {};

    const auto& side = sideIndex == 0 ? project->sideA : project->sideB;
    if (clipIndex < 0 || clipIndex >= static_cast<int>(side.clips.size()))
        return {};

    const auto& clip = side.clips[static_cast<size_t>(clipIndex)];
    const auto area = getSideBounds(sideIndex);
    const int rulerW = 48;
    const int header = 22;
    const auto timeline = area.withTrimmedLeft(rulerW).withTrimmedTop(header);
    const int lanes = 5;
    const int laneH = juce::jmax(12, timeline.getHeight() / lanes);
    const double pxPerSec = timeline.getWidth() / side.maxDurationSec;

    const int x = timeline.getX() + static_cast<int>(clip.startTimeSec * pxPerSec);
    const int w = juce::jmax(8, static_cast<int>(clip.durationSec * pxPerSec));
    const int y = timeline.getY() + (clip.trackIndex - 1) * laneH + 2;
    return { x, y, w, laneH - 4 };
}

void TapeTimelineView::paint(juce::Graphics& g)
{
    g.fillAll(CassetteBurnerLook::background());

    if (!project)
    {
        g.setColour(CassetteBurnerLook::textPrimary().withAlpha(0.6f));
        g.drawText("Drop audio files onto Side A or B", getLocalBounds(), juce::Justification::centred);
        return;
    }

    for (int s = 0; s < 2; ++s)
    {
        const auto& side = s == 0 ? project->sideA : project->sideB;
        const auto area = getSideBounds(s);
        g.setColour(CassetteBurnerLook::panel());
        g.fillRoundedRectangle(area.toFloat(), 6.0f);

        g.setColour(CassetteBurnerLook::textPrimary());
        g.setFont(13.0f);
        const auto used = formatTime(side.usedDurationSec());
        const auto max = formatTime(side.maxDurationSec);
        const auto overflow = !side.fitsOnTape();
        g.drawText("SIDE " + juce::String(s == 0 ? "A" : "B") + "  " + used + " / " + max,
                   area.getX() + 8,
                   area.getY() + 4,
                   area.getWidth() - 16,
                   18,
                   juce::Justification::left);
        if (overflow)
        {
            g.setColour(juce::Colours::red.withAlpha(0.85f));
            g.drawText("OVERFLOW", area.getRight() - 80, area.getY() + 4, 72, 18, juce::Justification::right);
        }

        const int rulerW = 48;
        const int header = 22;
        const auto timeline = area.withTrimmedLeft(rulerW).withTrimmedTop(header);
        g.setColour(CassetteBurnerLook::gridLine());
        for (int min = 0; min <= 45; min += 5)
        {
            const float x = timeline.getX() + timeline.getWidth() * (min / 45.0f);
            g.drawVerticalLine(static_cast<int>(x), static_cast<float>(timeline.getY()),
                               static_cast<float>(timeline.getBottom()));
            g.setFont(10.0f);
            g.setColour(CassetteBurnerLook::textPrimary().withAlpha(0.45f));
            g.drawText(juce::String(min) + ":00", static_cast<int>(x) + 2, timeline.getY() - 16, 40, 14,
                       juce::Justification::left);
            g.setColour(CassetteBurnerLook::gridLine());
        }

        const int lanes = 5;
        const int laneH = juce::jmax(12, timeline.getHeight() / lanes);
        for (int lane = 0; lane < lanes; ++lane)
        {
            auto laneRect = timeline.withHeight(laneH).withY(timeline.getY() + lane * laneH);
            g.setColour(CassetteBurnerLook::background().brighter(0.05f));
            g.fillRect(laneRect);
            g.setColour(CassetteBurnerLook::textPrimary().withAlpha(0.35f));
            g.drawText("[" + juce::String(lane + 1) + "]", area.getX() + 6, laneRect.getY(), rulerW - 8, laneH,
                       juce::Justification::centredLeft);
        }

        for (int i = 0; i < static_cast<int>(side.clips.size()); ++i)
        {
            const auto& clip = side.clips[static_cast<size_t>(i)];
            auto cb = clipBounds(s, i);
            const bool selected = selectedSide == s && selectedClip == i;
            g.setColour(clipColour(i).withAlpha(selected ? 1.0f : 0.85f));
            g.fillRoundedRectangle(cb.toFloat(), 4.0f);
            g.setColour(juce::Colours::black.withAlpha(0.35f));
            g.drawRoundedRectangle(cb.toFloat(), 4.0f, 1.0f);
            g.setColour(juce::Colours::white);
            g.setFont(11.0f);
            g.drawText(clip.displayTitle, cb.reduced(4, 2), juce::Justification::centredLeft, true);
        }

        const double ph = s == 0 ? playheadA : playheadB;
        const float px = timeline.getX() + timeline.getWidth() * static_cast<float>(ph / side.maxDurationSec);
        g.setColour(CassetteBurnerLook::accent());
        g.drawLine(px, static_cast<float>(timeline.getY()), px, static_cast<float>(timeline.getBottom()), 2.0f);
    }
}

void TapeTimelineView::resized() { repaint(); }

bool TapeTimelineView::isInterestedInFileDrag(const juce::StringArray& files)
{
    return AudioFileLoader::isSupportedAudioFileDrop(files);
}

void TapeTimelineView::filesDropped(const juce::StringArray& files, int x, int y)
{
    if (!project)
        return;

    const auto file = AudioFileLoader::pickFirstAudioFile(files);
    if (!file.existsAsFile())
        return;

    const int sideIndex = getSideBounds(1).contains(x, y) ? 1 : 0;
    activeSideIndex = sideIndex;
    auto& side = sideIndex == 0 ? project->sideA : project->sideB;
    const auto area = getSideBounds(sideIndex);

    const auto loaded = AudioFileLoader::loadToBufferWithDiagnostics(file);
    if (!loaded.audio.hasValue())
    {
        if (onStatus)
            onStatus("Load failed: " + loaded.error);
        return;
    }

    TapeClip clip;
    clip.sourceFile = file;
    clip.displayTitle = file.getFileNameWithoutExtension();
    clip.durationSec = loaded.audio->buffer.getNumSamples() / loaded.audio->sampleRate;
    clip.startTimeSec = xToTime(x, area);
    clip.trackIndex = yToTrack(y, area);

    if (!side.validateClipPlacement(clip))
    {
        clip.startTimeSec = side.usedDurationSec() + 2.0;
        if (!side.validateClipPlacement(clip))
        {
            if (onStatus)
                onStatus("Clip does not fit on side " + juce::String(sideIndex == 0 ? "A" : "B"));
            return;
        }
    }

    side.clips.push_back(clip);
    notifyChanged();
    if (onStatus)
        onStatus("Added: " + clip.displayTitle);
}

void TapeTimelineView::mouseDown(const juce::MouseEvent& e)
{
    const auto hit = hitTest(e.getPosition());
    selectedSide = hit.sideIndex;
    selectedClip = hit.clipIndex;
    activeSideIndex = hit.sideIndex;
    dragClipIndex = hit.clipIndex;

    if (hit.clipIndex >= 0)
    {
        auto& side = hit.sideIndex == 0 ? project->sideA : project->sideB;
        auto& clip = side.clips[static_cast<size_t>(hit.clipIndex)];
        dragStartTime = clip.startTimeSec;
        dragMouseOffset = e.getPosition() - clipBounds(hit.sideIndex, hit.clipIndex).getPosition();
    }

    listeners.call([hit](Listener& l) { l.timelineSelectionChanged(hit.sideIndex, hit.clipIndex); });
    repaint();
}

void TapeTimelineView::mouseDrag(const juce::MouseEvent& e)
{
    if (!project || dragClipIndex < 0)
        return;

    auto& side = selectedSide == 0 ? project->sideA : project->sideB;
    auto& clip = side.clips[static_cast<size_t>(dragClipIndex)];
    const auto area = getSideBounds(selectedSide);
    const auto newPos = e.getPosition() - dragMouseOffset;
    double newStart = std::floor(xToTime(newPos.getX(), area) + 0.5);

    TapeClip trial = clip;
    trial.startTimeSec = juce::jmax(0.0, newStart);
    bool ok = trial.startTimeSec + trial.durationSec <= side.maxDurationSec;
    for (size_t i = 0; i < side.clips.size(); ++i)
    {
        if (static_cast<int>(i) == dragClipIndex)
            continue;
        if (trial.overlaps(side.clips[i]))
            ok = false;
    }
    if (ok)
        clip.startTimeSec = trial.startTimeSec;
    repaint();
}

void TapeTimelineView::importFilesAt(const juce::StringArray& files, int x, int y)
{
    filesDropped(files, x, y);
}

void TapeTimelineView::mouseUp(const juce::MouseEvent&)
{
    if (dragClipIndex >= 0)
        notifyChanged();
    dragClipIndex = -1;
}

void TapeTimelineView::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (!project)
        return;
    const auto hit = hitTest(e.getPosition());
    if (hit.clipIndex < 0)
        return;

    auto& side = hit.sideIndex == 0 ? project->sideA : project->sideB;
    side.clips.erase(side.clips.begin() + hit.clipIndex);
    selectedClip = -1;
    notifyChanged();
    if (onStatus)
        onStatus("Clip removed");
}

void TapeTimelineView::notifyChanged()
{
    listeners.call([](Listener& l) { l.timelineProjectChanged(); });
    repaint();
}

}
