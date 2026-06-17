@echo off
REM MinGW is NOT supported by JUCE 8. Use scripts\build_windows_msvc.ps1 instead.
REM See scripts\BUILD-WINDOWS.md
echo ERROR: MinGW build is not supported for Deck (JUCE 8 requires MSVC).
echo.
echo Use PowerShell instead:
echo   .\scripts\build_windows_msvc.ps1
echo.
exit /b 1
