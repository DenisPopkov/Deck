Charli-XCX-SS26-Rock-Music.fixture.flac
  90 s excerpt (30–120 s) from the original FLAC in ~/Downloads.
  Used for mastering regression tests (hot/clean pop → Kenwood KX-1100G).

Regenerate:
  ffmpeg -y -ss 30 -t 90 -i ~/Downloads/Charli-XCX-SS26-Rock-Music.flac \
    -c:a flac Tests/Fixtures/Charli-XCX-SS26-Rock-Music.fixture.flac
