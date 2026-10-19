@echo off
rem Madden 19 with precise GPU readbacks (BB_READBACKS=2). On Windows these can crash the game:
rem every read fault overwrites the 128 bytes below the stack pointer that PS4 code may use.
rem Use only to compare graphics with the default (relaxed) mode.
set BB_READBACKS=2
start "" "%~dp0Madden19.exe"
