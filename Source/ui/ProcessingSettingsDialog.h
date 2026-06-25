#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../dsp/MasteringOptions.h"

namespace cassette::ui
{

void showAppSettingsDialog(juce::Component* host,
                           const MasteringOptions& current,
                           std::function<void(MasteringOptions)> onDone);

}
