#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace cassette
{

class ProjectSidebar : public juce::Component,
                         private juce::ListBoxModel,
                         private juce::Button::Listener
{
public:
    std::function<void()> onNewProject;
    std::function<void()> onImportAudio;
    std::function<void(int)> onProjectSelected;

    ProjectSidebar();
    void setProjectNames(const juce::StringArray& names, int selectedIndex = 0);

private:
    void paint(juce::Graphics& g) override;
    void resized() override;
    void buttonClicked(juce::Button* b) override;

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
    void selectedRowsChanged(int lastRowSelected) override;

    juce::TextButton newBtn { "+ New Project" };
    juce::TextButton importBtn { "Import Audio" };
    juce::ListBox projectList;
    juce::StringArray projects;
};

}
