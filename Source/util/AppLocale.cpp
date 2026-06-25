#include "AppLocale.h"

namespace cassette
{

namespace
{

juce::String utf8(const char* text)
{
    return text != nullptr ? juce::String::fromUTF8(text) : juce::String();
}

struct Entry
{
    const char* key;
    const char* text;
};

constexpr Entry kCatalog[] = {
    { "btn.new", "New" },
    { "btn.import_audio", "Import audio" },
    { "btn.settings", "Settings" },
    { "btn.prepare", "Prepare" },
    { "btn.export_wav", "Export WAV" },
    { "btn.import_folder", "Import folder" },
    { "btn.done", "Done" },
    { "btn.change", "Change" },
    { "btn.select_tracks", "Select tracks" },
    { "btn.remove_selected", "Remove selected" },

    { "wizard.add_music", "Add music" },
    { "wizard.edit_tracks", "Edit tracks" },
    { "wizard.prepare", "Prepare" },
    { "wizard.export", "Export" },

    { "drop.music", "Drop music here" },
    { "drop.audio", "Drop audio here" },
    { "drop.folder", "Drop folder here" },

    { "settings.title", "Settings" },
    { "settings.processing", "Processing" },

    { "processing.true_peak", "True peak limiter" },
    { "processing.stereo", "Stereo tightening" },
    { "processing.true_peak.title", "True peak limiter:" },
    { "processing.true_peak.desc", "Limits sharp digital peaks in the exported WAV." },
    { "processing.stereo.title", "Stereo tightening:" },
    { "processing.stereo.desc", "Keeps bass centered and tames wide highs." },

    { "tape.parameters", "Tape parameters" },
    { "tape.type_i", "Type I" },
    { "tape.type_ii", "Type II" },
    { "tape.type_iv", "Type IV" },
    { "tape.prepare_for_tape", "Prepare for Tape" },
    { "tape.build_sides", "Build Side A/B WAV" },
    { "tape.length", "Cassette length" },
    { "tape.custom", "Custom" },
    { "tape.tooltip.custom", "Custom length per side" },
    { "tape.tooltip.c60", "C60 — 30 min per side" },
    { "tape.tooltip.c90", "C90 — 45 min per side" },
    { "tape.tooltip.c120", "C120 — 60 min per side" },

    { "status.choose_tape_then_prepare", "Choose tape type, then Prepare" },
    { "status.add_music_first", "Add music first" },
    { "status.preparing", "Preparing for cassette..." },
    { "status.preparing_cassettes", "Preparing %d cassettes..." },
    { "status.preparing_sides", "Preparing Side A and Side B..." },
    { "status.prepare_first", "Prepare for cassette first" },
    { "status.pick_folder", "Pick a folder with tracks first" },
    { "status.export_failed", "Export failed" },
    { "status.unsupported_drop", "No supported audio in drop" },
    { "status.loading", "Loading %s..." },
    { "status.scanning", "Scanning %s..." },
    { "status.added", "Added %s" },
    { "status.exported", "Exported: %s" },
    { "status.load_failed", "Load failed: %s" },
    { "status.unsupported_format", "Unsupported format (use %s)" },

    { "track.loading", "Loading tracks..." },
    { "track.side_a", "Side A" },
    { "track.side_b", "Side B" },
    { "track.tracks", "tracks" },
    { "track.overflow", "OVERFLOW" },

    { "dialog.change_tape.title", "Change tape type?" },
    { "dialog.change_tape.message",
      "Changing the tape type clears the current Prepare result. You will need to run Prepare again." },
    { "dialog.change_tape.confirm", "Change" },
    { "dialog.cancel", "Cancel" },

    { "dialog.remove_tracks.title", "Remove tracks" },
    { "dialog.remove_tracks.message_one",
      "Remove %d selected track from the mixtape?\nFiles on disk will not be deleted." },
    { "dialog.remove_tracks.message_many",
      "Remove %d selected tracks from the mixtape?\nFiles on disk will not be deleted." },
    { "dialog.remove_tracks.confirm", "Remove" },
};

}

AppLocale& AppLocale::instance()
{
    static AppLocale locale;
    return locale;
}

juce::String AppLocale::removeTracksMessage(int count) const
{
    return count == 1 ? trf("dialog.remove_tracks.message_one", count)
                      : trf("dialog.remove_tracks.message_many", count);
}

juce::String AppLocale::tr(const juce::String& key) const
{
    for (const auto& entry : kCatalog)
    {
        if (key == entry.key)
            return utf8(entry.text);
    }

    juce::Logger::writeToLog("AppLocale missing key: " + key);
    return key;
}

}
