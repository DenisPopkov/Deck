@echo off
REM Build on Windows with Visual Studio or MinGW.
setlocal
cd /d "%~dp0\.."

if not exist Assets\AppIcon.png (
  echo Copy Assets\AppIcon.png first ^(1024x1024 PNG from your icon^).
  exit /b 1
)

cmake -B build-win -DCMAKE_BUILD_TYPE=Release -DCASSETTE_BUILD_TESTS=OFF
cmake --build build-win --config Release
echo.
echo Output:
dir /s /b build-win\CassetteAutoMaster_artefacts\Release\*.exe
pause
