#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../dsp/MasteringOptions.h"
#include "../util/CassetteBuildFlags.h"

#if CASSETTE_ENABLE_PI_TAPE
#include "../io/PiTapeSettings.h"
#endif

namespace cassette::ui
{

#if CASSETTE_ENABLE_PI_TAPE

struct AppSettingsState
{
    MasteringOptions mastering;
    PiTapeSettings piTape;
};

void showAppSettingsDialog(juce::Component* host,
                           const AppSettingsState& current,
                           std::function<void(AppSettingsState)> onDone);

#else

void showAppSettingsDialog(juce::Component* host,
                           const MasteringOptions& current,
                           std::function<void(MasteringOptions)> onDone);

#endif

}
