#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../../project/MixtapeProject.h"

namespace cassette
{

class TapeTimelineView : public juce::Component,
                           private juce::FileDragAndDropTarget
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void timelineProjectChanged() {}
        virtual void timelineSelectionChanged(int sideIndex, int clipIndex) {}
    };

    std::function<void(const juce::String&)> onStatus;

    TapeTimelineView();
    void setProject(MixtapeProject* project);
    void addListener(Listener* l) { listeners.add(l); }
    void setPlayheadSec(int sideIndex, double sec);
    void importFilesAt(const juce::StringArray& files, int x, int y);
    int getActiveSideIndex() const { return activeSideIndex; }

private:
    struct HitClip
    {
        int sideIndex = 0;
        int clipIndex = -1;
    };

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    juce::Rectangle<int> getSideBounds(int sideIndex) const;
    double xToTime(int x, const juce::Rectangle<int>& sideArea) const;
    int yToTrack(int y, const juce::Rectangle<int>& sideArea) const;
    HitClip hitTest(juce::Point<int> pos) const;
    juce::Rectangle<int> clipBounds(int sideIndex, int clipIndex) const;
    void notifyChanged();

    MixtapeProject* project = nullptr;
    int activeSideIndex = 0;
    int selectedSide = 0;
    int selectedClip = -1;
    int dragClipIndex = -1;
    double dragStartTime = 0.0;
    juce::Point<int> dragMouseOffset;
    double playheadA = 0.0;
    double playheadB = 0.0;

    juce::ListenerList<Listener> listeners;
};

}
