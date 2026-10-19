@echo off
rem Bring-up run: unsupported PS4 functions return 0 and are logged (STUB lines in the log)
rem instead of stopping the game. The game may crash later; send the newest log either way.
set BB_STUB_UNRESOLVED=1
start "" "%~dp0Madden19.exe"
