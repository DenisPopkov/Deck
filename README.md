# Deck

[Русский](#русский) | [English](#english)

---

## Русский

**Deck** — настольное приложение (C++ / JUCE) для подготовки цифрового аудио перед записью на кассету: микстейпы, адаптивный мастеринг под тип ленты и экспорт WAV, готового к записи.

**Сайт проекта:** [denispopkov.github.io/Deck](https://denispopkov.github.io/Deck/) — описание, скриншоты, FAQ и ссылки на скачивание для MacOS, Windows и Linux.

### Возможности

- **Импорт и подготовка** — перетащите файл или папку, нажмите **Prepare**: адаптивный мастеринг и предпросмотр "до / после".
- **Настройка ленты** — Type I, II или IV; C60, C90, C120 или произвольная длина.
- **Сборка микстейпа** — распределение треков по сторонам A и B, порядок, проверка, что все помещается на ленту.
- **Экспорт** — true-peak лимитирование, EQ с учетом кассеты, опциональные тестовые тоны; на выходе WAV для записи на деку.

Поддерживаемые форматы импорта: WAV, FLAC, AIFF, OGG, MP3. Экспорт — WAV. DAW не нужен.

### Скачать

Готовые сборки — на [сайте проекта](https://denispopkov.github.io/Deck/#download) или в [релизах GitHub](https://github.com/DenisPopkov/Deck/releases/latest).
Актуальная nightly-сборка Windows также доступна:
- как artifact в GitHub Actions workflow **Windows build**;
- по прямой ссылке в GitHub Pages: [denispopkov.github.io/Deck/downloads/Deck-Windows-latest.zip](https://denispopkov.github.io/Deck/downloads/Deck-Windows-latest.zip).

При первом запуске на MacOS система может заблокировать неподписанное приложение: щелкните правой кнопкой по **Deck.app** -> **Открыть**.

### Системные требования (MacOS)

| Сборка | macOS | Процессор |
|--------|-------|-----------|
| **Deck** (основная) | 11 Big Sur и новее | Apple Silicon (M1+) |
| **Deck для Mojave** | 10.14.6 Mojave и новее | Intel (x86_64) |

На старых Intel Mac с Mojave скачайте **Deck-macOS-Mojave.zip** с [сайта](https://denispopkov.github.io/Deck/downloads/Deck-macOS-Mojave-latest.zip) или из [релизов GitHub](https://github.com/DenisPopkov/Deck/releases/latest).

### Требования для сборки

- **MacOS:** Xcode Command Line Tools, CMake 3.22+
- **Windows:** Visual Studio 2022 с рабочей нагрузкой "Разработка классических приложений на C++" (JUCE 8 не поддерживает MinGW)
- **Linux (экспериментально):** GCC/Clang, CMake 3.22+, Ninja, dev-пакеты X11/ALSA/OpenGL (см. `make deps-help`)
- **Опционально:** пакеты Homebrew для дополнительных возможностей (см. ниже)

JUCE 8 подтягивается автоматически через CMake FetchContent.

### Сборка (MacOS)

```bash
git clone https://github.com/DenisPopkov/Deck.git
cd Deck

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Deck
```

Собранное приложение:

```text
build/Deck_artefacts/Release/Deck.app
```

Запуск:

```bash
open build/Deck_artefacts/Release/Deck.app
```

#### Релизный пакет (MacOS)

```bash
./scripts/package_release.sh
```

Результат: `dist/macOS/Deck.app` и `dist/Deck-0.2.0-macOS.zip`.

#### Релиз для Mojave (Intel, macOS 10.14+)

```bash
./scripts/package_legacy_mac.sh
```

Результат: `dist/macOS-Legacy/Deck.app` и `dist/Deck-<версия>-macOS-Mojave.zip`.

### Сборка (Windows)

На машине с Visual Studio 2022 (MSVC, не MinGW):

```powershell
.\scripts\build_windows_msvc.ps1 -Package
```

Исполняемый файл: `build\Deck_artefacts\Release\Deck.exe`  
ZIP для распространения: `dist\Deck-<версия>-Windows.zip`

Для CI и локальной сборки используется **MSVC** (Visual Studio toolchain), так как JUCE 8 официально не поддерживает MinGW.

Подробная инструкция: [`scripts/BUILD-WINDOWS.md`](scripts/BUILD-WINDOWS.md).

Готовые архивы для Windows и MacOS также публикуются в GitHub Actions (workflow **Release builds** на тегах).
На каждом push/pull request в `main`/`master` Windows-архив публикуется как artifact в workflow **Windows build**.
На push в `main`/`master` тот же актуальный Windows-архив выкладывается в GitHub Pages:
`https://denispopkov.github.io/Deck/downloads/Deck-Windows-latest.zip`.

#### Локально повторить Windows-сборку (как в CI)

```powershell
git clone https://github.com/DenisPopkov/Deck.git
cd Deck
.\scripts\build_windows_msvc.ps1
```

Проверка: архив должен появиться в `dist\Deck-0.2.0-Windows.zip`.

### Сборка (Linux)

Экспериментальная сборка из исходников (готового инсталлятора пока нет):

```bash
make deps-help   # список пакетов для Debian/Fedora
make
make run
```

Архив для распространения:

```bash
make package
```

Подробная инструкция: [`scripts/BUILD-LINUX.md`](scripts/BUILD-LINUX.md).

Готовый Linux-архив на GitHub Pages: [denispopkov.github.io/Deck/downloads/Deck-Linux-latest.tar.gz](https://denispopkov.github.io/Deck/downloads/Deck-Linux-latest.tar.gz).

### Тесты

Тесты включены по умолчанию (`CASSETTE_BUILD_TESTS=ON`):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target DeckTests
ctest --test-dir build --output-on-failure
```

Полный локальный прогон (mini-album в репо или evermore, если есть):

```bash
./scripts/run_audit.sh
```

Интеграция с альбомом: `CASSETTE_FIXTURE_ALBUM=Tests/Fixtures/mini-album`. Полный prepare 14+ треков: без `CASSETTE_SKIP_FULL_PREPARE`. CI: workflow **Tests** на Linux / Windows / macOS. Ручной QA перед релизом: [Tests/MANUAL_QA.md](Tests/MANUAL_QA.md).

Или напрямую:

```bash
./build/DeckTests
```

### Опции CMake

| Опция | По умолчанию | Описание |
|--------|--------------|----------|
| `CASSETTE_BUILD_TESTS` | ON | Набор тестов адекватности мастеринга |
| `CASSETTE_BUILD_GSTPEAQ` | ON | PEAQ/ODG через GStreamer в рантайме (нужен `gstreamer`) |
| `CASSETTE_BUILD_ONNX` | OFF | ONNX Runtime — модель STN-насыщения |
| `CASSETTE_BUILD_ESSENTIA` | OFF | Essentia для офлайн-анализа |
| `CASSETTE_BUILD_RUBBERBAND` | OFF | Rubber Band — wow/flutter |
| `CASSETTE_BUILD_PLUGIN` | OFF | Цель VST3/AU |
| `CASSETTE_BUILD_BURNER` | OFF | Приложение CassetteBurner (таймлайн) |

Пример с опциональными зависимостями (Homebrew на Apple Silicon):

```bash
brew install onnxruntime
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCASSETTE_BUILD_ONNX=ON
cmake --build build
```

#### Модель ONNX (опционально)

При включении `CASSETTE_BUILD_ONNX` положите модель в `models/tape_stn.onnx` или экспортируйте:

```bash
pip install torch onnx onnxscript onnxruntime
python scripts/export_stn_onnx.py
```

При наличии модели сборка копирует ее в бандл приложения (`scripts/copy_stn_onnx_if_present.sh`).

### Структура проекта

```text
Source/          Код приложения и DSP
cmake/           CMake-хелперы ядра
Tests/           Офлайн-тесты мастеринга
Tools/           CLI-утилиты (AnalyzeExport, RenderCompare)
scripts/         Сборка и упаковка (Makefile, BUILD-LINUX.md, BUILD-WINDOWS.md)
docs/            Сайт на GitHub Pages
Assets/          Иконка приложения
```

### Лицензия

Инженерный прототип. Сторонние зависимости (JUCE и опциональные библиотеки) распространяются на своих условиях.

---

## English

**Deck** is a desktop app (C++ / JUCE) for preparing digital audio before recording to cassette: mixtape workflow, tape-aware adaptive mastering, and WAV export ready for recording.

**Project website:** [denispopkov.github.io/Deck](https://denispopkov.github.io/Deck/) — overview, screenshots, FAQ, and download links for MacOS, Windows, and Linux.

### Features

- **Import and prepare** — drag a file or folder, press **Prepare**: adaptive mastering with before/after preview.
- **Tape setup** — Type I, II, or IV; C60, C90, C120, or custom duration.
- **Mixtape building** — split tracks across Side A and B, reorder tracks, and check cassette fit.
- **Export** — true-peak limiting, cassette-aware EQ, optional preflight tones; output is WAV for deck recording.

Supported input formats: WAV, FLAC, AIFF, OGG, MP3. Export format: WAV. No DAW required.

### Download

Prebuilt binaries are available on the [project website](https://denispopkov.github.io/Deck/#download) and in [GitHub releases](https://github.com/DenisPopkov/Deck/releases/latest).
The latest Windows nightly build is also available:
- as a GitHub Actions artifact from the **Windows build** workflow;
- as a stable direct download URL on GitHub Pages: [denispopkov.github.io/Deck/downloads/Deck-Windows-latest.zip](https://denispopkov.github.io/Deck/downloads/Deck-Windows-latest.zip).

On first launch on MacOS, the system may block an unsigned app: right-click **Deck.app** -> **Open**.

### MacOS system requirements

| Build | macOS | CPU |
|-------|-------|-----|
| **Deck** (default) | 11 Big Sur or newer | Apple Silicon (M1+) |
| **Deck for Mojave** | 10.14.6 Mojave or newer | Intel (x86_64) |

On older Intel Macs running Mojave, download **Deck-macOS-Mojave.zip** from the [website](https://denispopkov.github.io/Deck/downloads/Deck-macOS-Mojave-latest.zip) or [GitHub releases](https://github.com/DenisPopkov/Deck/releases/latest).

### Build requirements

- **MacOS:** Xcode Command Line Tools, CMake 3.22+
- **Windows:** Visual Studio 2022 with the "Desktop development with C++" workload (JUCE 8 does not support MinGW)
- **Linux (experimental):** GCC/Clang, CMake 3.22+, Ninja, X11/ALSA/OpenGL dev packages (see `make deps-help`)
- **Optional:** Homebrew packages for extra features (see below)

JUCE 8 is fetched automatically via CMake FetchContent.

### Build (MacOS)

```bash
git clone https://github.com/DenisPopkov/Deck.git
cd Deck

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Deck
```

Built app:

```text
build/Deck_artefacts/Release/Deck.app
```

Run:

```bash
open build/Deck_artefacts/Release/Deck.app
```

#### Release package (MacOS)

```bash
./scripts/package_release.sh
```

Output: `dist/macOS/Deck.app` and `dist/Deck-0.2.0-macOS.zip`.

#### Mojave release package (Intel, macOS 10.14+)

```bash
./scripts/package_legacy_mac.sh
```

Output: `dist/macOS-Legacy/Deck.app` and `dist/Deck-<version>-macOS-Mojave.zip`.

### Build (Windows)

On a machine with Visual Studio 2022 (MSVC, not MinGW):

```powershell
.\scripts\build_windows_msvc.ps1 -Package
```

Executable: `build\Deck_artefacts\Release\Deck.exe`  
Release zip: `dist\Deck-<version>-Windows.zip`

CI and local automation use **MSVC** (Visual Studio toolchain), because JUCE 8 does not officially support MinGW.

Full guide: [`scripts/BUILD-WINDOWS.md`](scripts/BUILD-WINDOWS.md).

Prebuilt Windows and MacOS archives are also published in GitHub Actions (`Release builds` workflow on tags).
On every push/pull request to `main`/`master`, the Windows archive is uploaded as an artifact in the `Windows build` workflow.
On pushes to `main`/`master`, the same up-to-date Windows archive is also published to GitHub Pages at:
`https://denispopkov.github.io/Deck/downloads/Deck-Windows-latest.zip`.

#### Reproduce Windows CI build locally

```powershell
git clone https://github.com/DenisPopkov/Deck.git
cd Deck
.\scripts\build_windows_msvc.ps1
```

Verification: the archive should be created at `dist\Deck-0.2.0-Windows.zip`.

### Build (Linux)

Experimental source build (no prebuilt installer yet):

```bash
make deps-help
make
make run
```

Release tarball:

```bash
make package
```

Full guide: [`scripts/BUILD-LINUX.md`](scripts/BUILD-LINUX.md).

Prebuilt Linux tarball on GitHub Pages: [denispopkov.github.io/Deck/downloads/Deck-Linux-latest.tar.gz](https://denispopkov.github.io/Deck/downloads/Deck-Linux-latest.tar.gz).

### Tests

Tests are enabled by default (`CASSETTE_BUILD_TESTS=ON`):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target DeckTests
ctest --test-dir build --output-on-failure
```

Full local audit (repo mini-album or evermore when present):

```bash
./scripts/run_audit.sh
```

Album integration: `CASSETTE_FIXTURE_ALBUM=Tests/Fixtures/mini-album`. Full 14-track prepare: omit `CASSETTE_SKIP_FULL_PREPARE`. CI runs the **Tests** workflow on Linux / Windows / macOS. Pre-release manual QA: [Tests/MANUAL_QA.md](Tests/MANUAL_QA.md).

Or run directly:

```bash
./build/DeckTests
```

### CMake options

| Option | Default | Description |
|--------|---------|-------------|
| `CASSETTE_BUILD_TESTS` | ON | Mastering adequacy test suite |
| `CASSETTE_BUILD_GSTPEAQ` | ON | Runtime PEAQ/ODG via GStreamer (requires `gstreamer`) |
| `CASSETTE_BUILD_ONNX` | OFF | ONNX Runtime STN saturation model |
| `CASSETTE_BUILD_ESSENTIA` | OFF | Essentia for offline analysis |
| `CASSETTE_BUILD_RUBBERBAND` | OFF | Rubber Band wow/flutter |
| `CASSETTE_BUILD_PLUGIN` | OFF | VST3/AU target |
| `CASSETTE_BUILD_BURNER` | OFF | CassetteBurner timeline app |

Example with optional dependencies (Homebrew on Apple Silicon):

```bash
brew install onnxruntime
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCASSETTE_BUILD_ONNX=ON
cmake --build build
```

#### ONNX model (optional)

If `CASSETTE_BUILD_ONNX` is enabled, place a model at `models/tape_stn.onnx` or export one:

```bash
pip install torch onnx onnxscript onnxruntime
python scripts/export_stn_onnx.py
```

When present, the build copies the model into the app bundle (`scripts/copy_stn_onnx_if_present.sh`).

### Project layout

```text
Source/          Application and DSP code
cmake/           Core CMake helpers
Tests/           Offline mastering tests
Tools/           CLI utilities (AnalyzeExport, RenderCompare)
scripts/         Build and packaging (Makefile, BUILD-LINUX.md, BUILD-WINDOWS.md)
docs/            GitHub Pages site
Assets/          App icon
```

### License

Engineering prototype. Third-party dependencies (JUCE and optional libraries) are distributed under their own licenses.
