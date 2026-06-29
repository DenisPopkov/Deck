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
    { "btn.send_to_pi", "Send to Pi" },
    { "btn.pi_tape", "Pi Tape" },
    { "btn.import_folder", "Import folder" },
    { "btn.done", "Done" },
    { "btn.change", "Change" },
    { "btn.select_tracks", "Select tracks" },
    { "btn.rebalance_sides", "Rebalance sides" },
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
    { "settings.pi_tape", "Raspberry Pi tape deck" },
    { "settings.pi_tape.enable", "Enable Send to Pi" },
    { "settings.pi_tape.enable.title", "Send to Pi:" },
    { "settings.pi_tape.enable.desc", "Upload Side A/B to Raspberry Pi and control playback from Deck." },
    { "settings.pi_tape.host", "Host" },
    { "settings.pi_tape.port", "Port" },
    { "settings.pi_tape.user", "User" },
    { "settings.pi_tape.password", "Password" },
    { "settings.pi_tape.remote_dir", "Remote folder" },
    { "settings.pi_tape.control_port", "Control port" },
    { "settings.pi_tape.hint", "FTP inbox on Pi; HTTP control port for Play/Stop from Deck." },

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
    { "tape.tooltip.custom", "Custom total cassette length (split evenly across Side A and Side B)" },
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
    { "status.pi_tape.configure", "Configure Pi FTP in Settings first" },
    { "status.pi_tape.uploading", "Uploading to Raspberry Pi..." },
    { "status.pi_tape.sent", "Sent to Pi: %s" },
    { "status.pi_tape.failed", "Pi upload failed: %s" },

    { "pi_tape.dialog.title", "Raspberry Pi tape deck" },
    { "pi_tape.dialog.hint", "Press REC on the deck, then Play the side you are recording." },
    { "pi_tape.btn.upload", "Upload queue" },
    { "pi_tape.btn.play", "Play" },
    { "pi_tape.btn.resume", "Resume" },
    { "pi_tape.btn.play_a", "Play Side A" },
    { "pi_tape.btn.play_b", "Play Side B" },
    { "pi_tape.btn.resume_a", "Resume Side A" },
    { "pi_tape.btn.resume_b", "Resume Side B" },
    { "pi_tape.btn.stop", "Stop" },
    { "pi_tape.connection.online", "Pi connected" },
    { "pi_tape.connection.offline", "Pi offline: %s" },
    { "pi_tape.uploading", "Uploading Side A/B to Pi..." },
    { "pi_tape.upload.progress", "Uploading..." },
    { "pi_tape.upload.side_a", "Uploading Side A" },
    { "pi_tape.upload.side_b", "Uploading Side B" },
    { "pi_tape.upload.queue", "Sending queue" },
    { "pi_tape.upload_failed", "Upload failed: %s" },
    { "pi_tape.play_failed", "Play failed: %s" },
    { "pi_tape.stop_failed", "Stop failed: %s" },
    { "pi_tape.confirm.stop.title", "Stop tape playback?" },
    { "pi_tape.confirm.stop.message", "Side playback is still running on the Pi. Stop it now?" },
    { "pi_tape.confirm.stop.confirm", "Stop playback" },
    { "pi_tape.confirm.stop.cancel", "Keep playing" },
    { "pi_tape.confirm.upload.title", "Cancel upload?" },
    { "pi_tape.confirm.upload.message", "Files are still uploading to the Pi. Close anyway?" },
    { "pi_tape.confirm.upload.confirm", "Close" },
    { "pi_tape.confirm.upload.cancel", "Keep uploading" },
    { "pi_tape.recording.idle", "Ready — choose a side to record" },
    { "pi_tape.recording.side_a", "Recording Side A" },
    { "pi_tape.recording.side_b", "Recording Side B" },
    { "pi_tape.recording.starting_a", "Starting Side A..." },
    { "pi_tape.recording.starting_b", "Starting Side B..." },
    { "pi_tape.recording.resuming_a", "Resuming Side A..." },
    { "pi_tape.recording.resuming_b", "Resuming Side B..." },
    { "pi_tape.recording.stopped", "Playback stopped" },
    { "pi_tape.recording.finished", "Side finished" },
    { "pi_tape.status.waiting", "Waiting" },
    { "pi_tape.status.ready", "Ready on Pi" },
    { "pi_tape.status.starting", "Starting" },
    { "pi_tape.status.playing", "Playing" },
    { "pi_tape.status.stopped", "Stopped" },
    { "pi_tape.status.done", "Done" },
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
