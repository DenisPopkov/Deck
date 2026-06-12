#include "ProjectSidebar.h"
#include "../look/CassetteBurnerLook.h"
#include "../UiTheme.h"

namespace cassette
{

ProjectSidebar::ProjectSidebar() : projectList("Projects", this)
{
    addAndMakeVisible(newBtn);
    addAndMakeVisible(importBtn);
    addAndMakeVisible(importFolderBtn);
    addAndMakeVisible(projectList);
    newBtn.addListener(this);
    importBtn.addListener(this);
    importFolderBtn.addListener(this);
    projectList.setColour(juce::ListBox::backgroundColourId, CassetteBurnerLook::panel());
    projectList.setRowHeight(28);
    ui::Theme::styleNeutralButton(newBtn);
    ui::Theme::styleNeutralButton(importBtn);
    ui::Theme::styleNeutralButton(importFolderBtn);
}

void ProjectSidebar::setProjectNames(const juce::StringArray& names, int selectedIndex)
{
    projects = names;
    projectList.updateContent();
    projectList.selectRow(juce::jlimit(0, projects.size() - 1, selectedIndex));
}

void ProjectSidebar::paint(juce::Graphics& g)
{
    g.fillAll(CassetteBurnerLook::panel());
    g.setColour(CassetteBurnerLook::textPrimary().withAlpha(0.5f));
    g.setFont(ui::Theme::captionFont());
    g.drawText("PROJECTS", 12, 8, 120, 16, juce::Justification::left);
}

void ProjectSidebar::resized()
{
    auto r = getLocalBounds().reduced(8);
    r.removeFromTop(24);
    newBtn.setBounds(r.removeFromTop(30));
    r.removeFromTop(6);
    importBtn.setBounds(r.removeFromTop(30));
    r.removeFromTop(6);
    importFolderBtn.setBounds(r.removeFromTop(30));
    r.removeFromTop(12);
    projectList.setBounds(r);
}

void ProjectSidebar::buttonClicked(juce::Button* b)
{
    if (b == &newBtn && onNewProject)
        onNewProject();
    if (b == &importBtn && onImportAudio)
        onImportAudio();
    if (b == &importFolderBtn && onImportFolder)
        onImportFolder();
}

int ProjectSidebar::getNumRows() { return projects.size(); }

void ProjectSidebar::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (selected)
        g.fillAll(CassetteBurnerLook::accent().withAlpha(0.18f));
    g.setColour(CassetteBurnerLook::textPrimary());
    g.setFont(ui::Theme::bodyFont());
    g.drawText(projects[row], 8, 0, w - 12, h, juce::Justification::centredLeft);
}

void ProjectSidebar::selectedRowsChanged(int lastRowSelected)
{
    if (onProjectSelected && lastRowSelected >= 0)
        onProjectSelected(lastRowSelected);
}

}
