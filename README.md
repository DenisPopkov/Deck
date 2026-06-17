# Deck

macOS desktop app (C++ / JUCE) for preparing digital audio before cassette recording.

## Requirements

- **macOS:** Xcode Command Line Tools, CMake 3.22+
- **Windows:** Visual Studio 2022 with the “Desktop development with C++” workload (JUCE 8 does not support MinGW)
- **Optional:** Homebrew packages for extra features (see below)

JUCE 8 is fetched automatically via CMake FetchContent.

## Build (macOS)

```bash
git clone https://github.com/DenisPopkov/Deck.git
cd Deck

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Deck
```

The app bundle is created at:

```text
build/Deck_artefacts/Release/Deck.app
```

Open it:

```bash
open build/Deck_artefacts/Release/Deck.app
```

On first launch, macOS may block an unsigned app: right-click the app → **Open**.

### Release package (macOS)

```bash
./scripts/package_release.sh
```

Output: `dist/macOS/Deck.app` and `dist/Deck-0.2.0-macOS.zip`.

## Build (Windows)

From a Windows machine with Visual Studio 2022:

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Or use the helper script:

```powershell
.\scripts\build_windows_msvc.ps1
```

The executable is under `build\Deck_artefacts\Release\`.

Pre-built Windows/macOS zips can also be downloaded from GitHub Actions (**Release builds** workflow).

## Tests

Tests are enabled by default (`CASSETTE_BUILD_TESTS=ON`):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target DeckTests
ctest --test-dir build --output-on-failure
```

Or run directly:

```bash
./build/DeckTests
```

## CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `CASSETTE_BUILD_TESTS` | ON | Mastering adequacy test suite |
| `CASSETTE_BUILD_GSTPEAQ` | ON | Runtime PEAQ/ODG via GStreamer (needs `gstreamer` at runtime) |
| `CASSETTE_BUILD_ONNX` | OFF | ONNX Runtime STN saturation model |
| `CASSETTE_BUILD_ESSENTIA` | OFF | Essentia for offline analysis |
| `CASSETTE_BUILD_RUBBERBAND` | OFF | Rubber Band wow/flutter |
| `CASSETTE_BUILD_PLUGIN` | OFF | VST3/AU plugin target |
| `CASSETTE_BUILD_BURNER` | OFF | CassetteBurner timeline app |

Example with optional dependencies (Homebrew on Apple Silicon):

```bash
brew install onnxruntime
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCASSETTE_BUILD_ONNX=ON
cmake --build build
```

### ONNX model (optional)

If you enable `CASSETTE_BUILD_ONNX`, place a model at `models/tape_stn.onnx` or export one:

```bash
pip install torch onnx onnxscript onnxruntime
python scripts/export_stn_onnx.py
```

The build copies the model into the app bundle when present (`scripts/copy_stn_onnx_if_present.sh`).

## Project layout

```text
Source/          Application and DSP code
cmake/           Core library CMake helpers
Tests/           Offline mastering tests
Tools/           CLI utilities (AnalyzeExport, RenderCompare)
scripts/         Build and packaging helpers
Assets/          App icon
```

## License

Engineering prototype. Third-party dependencies (JUCE, optional libraries) are subject to their own licenses.
