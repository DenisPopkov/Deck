# Сборка Deck на Windows

Инструкция для локальной сборки настольного приложения **Deck** под Windows с помощью PowerShell-скрипта `build_windows_msvc.ps1`.

---

## Что нужно установить

1. **Git for Windows** — для загрузки JUCE через CMake FetchContent.  
   https://git-scm.com/download/win

2. **CMake 3.22+** — при установке отметьте «Add CMake to the system PATH».  
   https://cmake.org/download/

3. **Visual Studio 2022** (Community достаточно) с рабочей нагрузкой:  
   **Разработка классических приложений на C++** (*Desktop development with C++*).

   > JUCE 8 **не поддерживает MinGW**. Сборка через MinGW не работает — используйте только MSVC.

4. **PowerShell 5.1+** (есть в Windows 10/11).

Проверка в PowerShell:

```powershell
cmake --version
git --version
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property displayName
```

---

## Быстрый старт

Клонируйте репозиторий и запустите скрипт из корня проекта:

```powershell
git clone https://github.com/DenisPopkov/Deck.git
cd Deck
.\scripts\build_windows_msvc.ps1
```

Первая сборка может занять **10–20 минут**: CMake скачает JUCE 8.

Готовый исполняемый файл:

```text
build\Deck_artefacts\Release\Deck.exe
```

Запуск:

```powershell
.\build\Deck_artefacts\Release\Deck.exe
```

---

## Сборка ZIP для распространения

Чтобы положить `Deck.exe` в `dist\Windows` и создать архив `dist\Deck-<версия>-Windows.zip`:

```powershell
.\scripts\build_windows_msvc.ps1 -Package
```

Архив можно загрузить на GitHub Pages (например, в `docs/assets/`) или прикрепить к релизу.

---

## Параметры скрипта

| Параметр | Описание |
|----------|----------|
| *(без параметров)* | Release-сборка в папку `build` |
| `-Package` | Скопировать EXE (+ DLL рядом) в `dist\Windows` и создать ZIP |
| `-Clean` | Удалить папку `build` перед конфигурацией |
| `-Tests` | Включить тесты (`CASSETTE_BUILD_TESTS=ON`) и запустить `ctest` |
| `-Config Debug` | Сборка в конфигурации Debug |
| `-BuildDir build-win` | Другая папка сборки |
| `-Jobs 8` | Число параллельных задач компиляции |

Примеры:

```powershell
# Чистая Release-сборка + ZIP
.\scripts\build_windows_msvc.ps1 -Clean -Package

# С тестами
.\scripts\build_windows_msvc.ps1 -Tests

# Debug в отдельной папке
.\scripts\build_windows_msvc.ps1 -BuildDir build-debug -Config Debug
```

---

## Ручная сборка (без скрипта)

Эквивалент команд из скрипта:

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_BUILD_TYPE=Release `
  -DCASSETTE_BUILD_TESTS=OFF

cmake --build build --config Release --target Deck --parallel
```

---

## Частые проблемы

### «cmake не является внутренней или внешней командой»

CMake не в PATH. Переустановите CMake с опцией добавления в PATH или добавьте вручную, например:  
`C:\Program Files\CMake\bin`.

### Visual Studio / C++ toolchain not found

Установите Visual Studio 2022 с компонентом **MSVC v143** и **Windows 10/11 SDK**.

### Ошибка при загрузке JUCE (git / FetchContent)

Убедитесь, что Git установлен и доступен в PATH. Проверьте доступ к `https://github.com`.

### SmartScreen / «Неизвестный издатель»

Локальные сборки не подписаны кодом. Нажмите **Подробнее** → **Выполнить в любом случае**, либо подпишите EXE своим сертификатом для распространения.

### Сборка в CI

Готовый workflow: `.github/workflows/release.yml` (job `windows`). Артефакт: `Deck-Windows.zip`.

---

## Опциональные зависимости CMake

По умолчанию достаточно MSVC + CMake. Дополнительные флаги (как в `README.md`):

| Опция | По умолчанию |
|-------|----------------|
| `CASSETTE_BUILD_TESTS` | ON (в скрипте OFF, если не указан `-Tests`) |
| `CASSETTE_BUILD_ONNX` | OFF |
| `CASSETTE_BUILD_ESSENTIA` | OFF |

Пример с ONNX (нужен установленный ONNX Runtime и переменная `ONNXRUNTIME_ROOT`):

```powershell
$env:ONNXRUNTIME_ROOT = "C:\path\to\onnxruntime"
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCASSETTE_BUILD_ONNX=ON
cmake --build build --config Release --target Deck
```

---

# Building Deck on Windows (English)

Use `scripts/build_windows_msvc.ps1` on a machine with **Git**, **CMake 3.22+**, and **Visual Studio 2022** (Desktop development with C++). MinGW is **not** supported.

```powershell
git clone https://github.com/DenisPopkov/Deck.git
cd Deck
.\scripts\build_windows_msvc.ps1 -Package
```

Output: `build\Deck_artefacts\Release\Deck.exe`  
Release zip: `dist\Deck-<version>-Windows.zip`

See parameters and troubleshooting in the sections above.
