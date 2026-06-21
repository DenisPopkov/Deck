# Manual QA checklist

Automated `DeckTests` cover DSP, project layout, and headless import/drop logic. Run this checklist before a release on **macOS** and **Windows**.

## Setup

- [ ] Build a release candidate (`scripts/package_release.sh` on macOS, `scripts/build_windows_msvc.ps1` on Windows).
- [ ] Prepare a test folder: 3–5 audio files (WAV/FLAC/MP3) with track numbers in filenames.
- [ ] Prepare a long album folder (optional): 10+ tracks, total length between 46 and 90 minutes.

## Import folder (native picker)

| Step | macOS | Windows |
|------|-------|---------|
| Open import | Mixtape → Import folder (or equivalent) | Same |
| Picker type | Native folder chooser (`NSOpenPanel`) | Native Explorer folder picker (not single-file dialog) |
| Select folder | Choose test folder | Choose test folder |
| Result | All audio files listed, natural sort order | Same |
| Cancel | No crash, no partial state | Same |

- [ ] Folder with only audio files imports cleanly.
- [ ] Folder containing `Side A.wav` / export outputs does **not** re-import exported sides as tracks.
- [ ] Empty folder shows a clear error (no crash).

## Drag and drop

- [ ] Drop **folder** onto hero / waveform → folder import starts.
- [ ] Drop **single audio file** → single-track flow loads.
- [ ] Drop unsupported file (`.txt`) → ignored, no crash.
- [ ] Drop `file://` URL (where applicable) resolves correctly.

## Mixtape editor

- [ ] Side A / Side B split looks reasonable for a long album.
- [ ] Reorder within side, move between sides, delete track.
- [ ] Undo restores previous layout.
- [ ] **Prepare** completes for Side A and Side B (spot-check audio in exported WAV).
- [ ] Multi-cassette album (if available): second cassette layout is reachable.

## Preview / playback

- [ ] Click play on a track → mini-player appears, audio audible.
- [ ] Pause / resume works.
- [ ] Seek slider updates position; drag seek works.
- [ ] Close mini-player stops playback.

## Export

- [ ] Export WAV after Prepare.
- [ ] Preflight tones enabled → exported file is longer than music-only.
- [ ] Kenwood KX-1100 calibration (if enabled) uses alternate tone stack.

## Platform-specific

### macOS

- [ ] App opens after download (signed/ad-hoc build or `xattr -cr` documented in FAQ).
- [ ] Gatekeeper: no “damaged” dialog on packaged zip from release workflow.

### Windows

- [ ] SmartScreen / Defender does not block the signed/unsigned build unexpectedly.
- [ ] Import folder never opens “select file” when folder import was requested.

## Regression scripts (developers)

```bash
# Full local audit (uses evermore if present, else mini-album)
./scripts/run_audit.sh

# CI-equivalent smoke
CASSETTE_FIXTURE_ALBUM=Tests/Fixtures/mini-album CASSETTE_SKIP_FULL_PREPARE=1 ctest --test-dir build --output-on-failure
```
