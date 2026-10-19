@echo off
rem Lists every PS4 function the prepared game imports and how the port provides it
rem ("unresolved" = missing). Run Madden19.exe and press Play once first (it prepares the game).
set OUT=%LOCALAPPDATA%\Madden19Native\out
if not exist "%OUT%\boot-linked.bin" (echo Run Madden19.exe and press Play once first. & pause & exit /b 1)
"%~dp0bin\sacredfire-game.exe" "%OUT%\boot-linked.bin" --list-imports --import-names "%OUT%\import_names.txt" > "%LOCALAPPDATA%\Madden19Native\logs\imports.txt" 2>&1
echo Wrote %LOCALAPPDATA%\Madden19Native\logs\imports.txt - send that file.
pause
