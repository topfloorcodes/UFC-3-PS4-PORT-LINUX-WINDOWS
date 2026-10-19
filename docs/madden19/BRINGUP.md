# Madden NFL 19 (CUSA10038) — bring-up notes

Branch `madden19`, built on the UFC 2 port (`docs/ufc2/BRINGUP.md` has the general method).

- Profile `madden19` in `scripts/game_check.py`: recognised by TITLE_ID `CUSA10038` or a
  param.sfo TITLE naming Madden NFL 19. Variant `madden19`, so the hooks keyed on UFC 3's
  executable stay off.
- Defaults: the same conservative set as UFC 2 (`run.sh`, the Windows launcher): precise
  readbacks, no write windows, offline only, no UFC 3 patches. Shader patches, if any are needed,
  go in `patches/shaders-madden19/`.
- Engine: Frostbite (Madden switched to it with Madden 18), unlike UFC 2/3 (EA Ignite). Nothing
  from the UFC ports is known to apply beyond the PS4 system layer.
- shadPS4 compatibility issue #1908: on 01.23, a crash shortly after launch on recent versions;
  on v0.15.0 the loading screen appeared, intro and menu sounds played, but nothing rendered.
  Expect graphics to be where the work is once the missing imports are in.

Log of runs: add each run's outcome here (version, how far it got, what was fixed).

## Runs

1. Build 1 (01.00): STOP `getargv` in libc startup (Frostbite reads its command line) -> added.
2. Build 2: STOP `sceKernelSetVirtualRangeName`; `--list-imports` gave 207 unresolved, 109 of them
   provided by the GPU library at run time -> the 98 others added (`src/runtime_bringup.c`).
3. Build 3: libc startup done, game entry, MainThread + workers, PlayGo; then a NULL write on
   MainThread at guest offset 0x54ac3 (stack through libc). `BB_IMPORT_TRACE=1` (default for this
   profile) now prints the last 64 runtime-answered PS4 calls in the crash report.
