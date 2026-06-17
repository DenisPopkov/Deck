# Deck

**Deck** — настольное приложение (C++ / JUCE) для подготовки цифрового аудио перед записью на кассету: микстейпы, адаптивный мастеринг под тип ленты и экспорт WAV, готового к записи.

**Сайт проекта:** [denispopkov.github.io/Deck](https://denispopkov.github.io/Deck/) — описание, скриншоты, FAQ и ссылки на скачивание для macOS и Windows.

## Возможности

- **Импорт и подготовка** — перетащите файл или папку, нажмите **Prepare**: адаптивный мастеринг и предпросмотр «до / после».
- **Настройка ленты** — Type I, II или IV; C60, C90, C120 или произвольная длина.
- **Сборка микстейпа** — распределение треков по сторонам A и B, порядок, проверка, что всё помещается на ленту.
- **Экспорт** — true-peak лимитирование, EQ с учётом кассеты, опциональные тестовые тоны; на выходе WAV для записи на деку.

Поддерживаемые форматы импорта: WAV, FLAC, AIFF, OGG. Экспорт — WAV. DAW не нужен.

## Скачать

Готовые сборки — на [сайте проекта](https://denispopkov.github.io/Deck/#download) или в [релизах GitHub](https://github.com/DenisPopkov/Deck/releases/latest).

При первом запуске на macOS система может заблокировать неподписанное приложение: щёлкните правой кнопкой по **Deck.app** → **Открыть**.

## Требования для сборки

- **macOS:** Xcode Command Line Tools, CMake 3.22+
- **Windows:** Visual Studio 2022 с рабочей нагрузкой «Разработка классических приложений на C++» (JUCE 8 не поддерживает MinGW)
- **Опционально:** пакеты Homebrew для дополнительных возможностей (см. ниже)

JUCE 8 подтягивается автоматически через CMake FetchContent.

## Сборка (macOS)

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

### Релизный пакет (macOS)

```bash
./scripts/package_release.sh
```

Результат: `dist/macOS/Deck.app` и `dist/Deck-0.2.0-macOS.zip`.

## Сборка (Windows)

На машине с Visual Studio 2022:

```powershell
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Или вспомогательный скрипт:

```powershell
.\scripts\build_windows_msvc.ps1
```

Исполняемый файл: `build\Deck_artefacts\Release\`.

Готовые архивы для Windows и macOS также публикуются в GitHub Actions (workflow **Release builds**).

## Тесты

Тесты включены по умолчанию (`CASSETTE_BUILD_TESTS=ON`):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target DeckTests
ctest --test-dir build --output-on-failure
```

Или напрямую:

```bash
./build/DeckTests
```

## Опции CMake

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

### Модель ONNX (опционально)

При включении `CASSETTE_BUILD_ONNX` положите модель в `models/tape_stn.onnx` или экспортируйте:

```bash
pip install torch onnx onnxscript onnxruntime
python scripts/export_stn_onnx.py
```

При наличии модели сборка копирует её в бандл приложения (`scripts/copy_stn_onnx_if_present.sh`).

## Структура проекта

```text
Source/          Код приложения и DSP
cmake/           CMake-хелперы ядра
Tests/           Офлайн-тесты мастеринга
Tools/           CLI-утилиты (AnalyzeExport, RenderCompare)
scripts/         Сборка и упаковка
docs/            Сайт на GitHub Pages
Assets/          Иконка приложения
```

## Лицензия

Инженерный прототип. Сторонние зависимости (JUCE и опциональные библиотеки) распространяются на своих условиях.
