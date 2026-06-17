# Сборка Deck на Linux

Инструкция для **теоретической / экспериментальной** сборки Deck на Linux.  
JUCE 8 официально поддерживает Linux; готовых бинарников на сайте пока нет — только сборка из исходников.

---

## Нужны ли изменения в коде?

**Почти нет.** Основной код кроссплатформенный (JUCE + CMake).

Уже учтено для Linux:

- шрифты UI — системные (не только SF Pro на macOS);
- полноэкранный режим — `F11` (на macOS дополнительно `Cmd+Ctrl+F`);
- GstPEAQ — через `scripts/run_peaq.sh` и переменную `CASSETTE_PEAQ_SCRIPT`;
- ONNX-модель — `models/tape_stn.onnx` или `CASSETTE_STN_MODEL`.

После сборки `Makefile` копирует `run_peaq.sh` и модель рядом с бинарником.

---

## Зависимости

### Debian / Ubuntu

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build git pkg-config \
  libasound2-dev libfreetype6-dev libfontconfig1-dev \
  libx11-dev libxrandr-dev libxcursor-dev libxinerama-dev libxext-dev libxrender-dev \
  libgl-dev libglu1-mesa-dev
```

Опционально для GstPEAQ в рантайме:

```bash
sudo apt-get install -y gstreamer1.0-plugins-bad gstreamer1.0-tools
```

### Fedora

```bash
sudo dnf install -y gcc-c++ cmake ninja-build git pkg-config \
  alsa-lib-devel freetype-devel fontconfig-devel \
  libX11-devel libXrandr-devel libXcursor-devel libXinerama-devel libXext-devel libXrender-devel \
  mesa-libGL-devel mesa-libGLU-devel
```

Список пакетов также выводит:

```bash
make deps-help
```

---

## Быстрый старт

### Готовый архив (GitHub Pages)

```bash
curl -LO https://denispopkov.github.io/Deck/downloads/Deck-Linux-latest.tar.gz
tar -xzf Deck-Linux-latest.tar.gz
cd Deck-*/
./Deck
```

### Сборка из исходников

```bash
git clone https://github.com/DenisPopkov/Deck.git
cd Deck
make
./build/Deck_artefacts/Release/Deck
```

Или через helper:

```bash
make run
```

---

## Цели Makefile

| Команда | Описание |
|---------|----------|
| `make` / `make build` | Сконфигурировать и собрать Deck |
| `make run` | Собрать и запустить |
| `make test` | Собрать и запустить `DeckTests` |
| `make package` | `dist/Deck-<версия>-Linux.tar.gz` |
| `make clean` | Очистить объектные файлы |
| `make distclean` | Удалить `build/` и `dist/Linux/` |
| `make deps-help` | Показать пакеты для Debian/Fedora |

Переменные: `BUILD_DIR`, `CONFIG`, `JOBS`, `GENERATOR`.

---

## Ручная сборка (без Makefile)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASSETTE_BUILD_TESTS=OFF
cmake --build build --target Deck --parallel
```

Бинарник:

```text
build/Deck_artefacts/Release/Deck
```

или (в зависимости от генератора):

```text
build/Deck_artefacts/Deck
```

---

## Опциональные флаги CMake

| Опция | По умолчанию | Комментарий |
|-------|----------------|-------------|
| `CASSETTE_BUILD_GSTPEAQ` | ON | Нужен GStreamer с `peaq` для полного PEAQ; иначе Basic fallback |
| `CASSETTE_BUILD_ONNX` | OFF | `ONNXRUNTIME_ROOT` → include/lib |
| `CASSETTE_BUILD_TESTS` | ON (в Makefile OFF) | `make test` включает |

Пример без GstPEAQ (если GStreamer не установлен):

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCASSETTE_BUILD_GSTPEAQ=OFF
cmake --build build --target Deck
```

---

## Частые проблемы

### Не хватает X11 / ALSA / OpenGL dev-пакетов

CMake или линковка падают с `cannot find -lX11`, `alsa`, `GL` — установите пакеты из `make deps-help`.

### GstPEAQ не находится

```bash
export CASSETTE_PEAQ_SCRIPT="$PWD/scripts/run_peaq.sh"
```

Или соберите с `-DCASSETTE_BUILD_GSTPEAQ=OFF`.

### Wayland-only окружение

JUCE на Linux по умолчанию использует X11. На чистом Wayland может понадобиться XWayland.

---

# Building Deck on Linux (English)

Experimental Linux builds are supported via **CMake + Ninja** and the root `Makefile`.  
A prebuilt tarball is published on GitHub Pages: [Deck-Linux-latest.tar.gz](https://denispopkov.github.io/Deck/downloads/Deck-Linux-latest.tar.gz).

### Quick start

### Prebuilt tarball (GitHub Pages)

```bash
curl -LO https://denispopkov.github.io/Deck/downloads/Deck-Linux-latest.tar.gz
tar -xzf Deck-Linux-latest.tar.gz
cd Deck-*/
./Deck
```

### Build from source

```bash
git clone https://github.com/DenisPopkov/Deck.git
cd Deck
make package
```

Output: `dist/Deck-<version>-Linux.tar.gz`

See the tables and troubleshooting sections above.
