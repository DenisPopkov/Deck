Charli-XCX-SS26-Rock-Music.fixture.flac
  90 s excerpt (30-120 s) from Tests/Fixtures/sources/Charli-XCX-SS26-Rock-Music.flac
  Used for mastering regression tests (hot/clean pop -> Kenwood KX-1100G).

Regenerate:
  ./scripts/extract_charli_fixture.sh

Or manually:
  ffmpeg -y -ss 30 -t 90 \
    -i Tests/Fixtures/sources/Charli-XCX-SS26-Rock-Music.flac \
    -c:a flac Tests/Fixtures/Charli-XCX-SS26-Rock-Music.fixture.flac

Icon sources (Assets/):
  IconSource.png      square DECK artwork master
  AppIconSource.jpg   alternate JPEG master
  AppStoreSource.png  optional App Store-style master (if present)
  AppIcon.png         generated squircle icon for the app bundle
