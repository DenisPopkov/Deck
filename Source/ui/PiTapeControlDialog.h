#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include "../io/PiTapeSettings.h"
#include "../io/PiTapeUploader.h"
#include "../threading/AnalysisWorker.h"

namespace cassette::ui
{

struct PiTapeControlSession
{
    PiTapeSettings settings;
    PiTapeQueueSource queue;
};

void showPiTapeControlDialog(juce::Component* host,
                             AnalysisWorker& worker,
                             PiTapeControlSession session);

} // namespace cassette::ui
