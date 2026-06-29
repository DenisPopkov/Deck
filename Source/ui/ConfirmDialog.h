#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace cassette::ui
{

struct ConfirmDialogOptions
{
    juce::String title;
    juce::String message;
    juce::String confirmLabel { "OK" };
    juce::String cancelLabel { "Cancel" };
};

/** Styled card dialog (no system icon). confirm = primary left button, cancel = accent right. */
void showConfirmDialog(juce::Component* host,
                       const ConfirmDialogOptions& options,
                       std::function<void(bool confirmed)> onResult);

/** Overlay confirm inside an existing modal (no nested enterModalState). */
void attachConfirmOverlay(juce::Component& parent,
                          const ConfirmDialogOptions& options,
                          std::function<void(bool confirmed)> onResult);

} // namespace cassette::ui
