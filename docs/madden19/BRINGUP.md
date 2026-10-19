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
4. Build 5 trace: last runtime calls before the NULL write were `sceKernelVirtualQuery` then
   `sceKernelBatchMap` x2 (Frostbite's allocator); the first-use list ends with PlayGo
   (GetLocus, language masks, GetProgress, GetToDoList). Build 6 logs the first virtual queries
   and batch maps and every failed batch map.
5. Build 6 log: the failing call was `sceKernelBatchMap` op 2 (protect), prot 0, 2 MiB at
   0x1040a00000 -> EINVAL: protection changes required fully mapped memory. Frostbite makes guard
   regions no-access over reserved / unmapped space; build 7 accepts prot 0 there (mapped pieces
   protected as before; granting access to unbacked memory still fails). tests/test_memory_guard.c.
6. Build 7: guard accepted; got through UserService, pad (controller found), VideoOut open,
   flip rate, Gnm init (~4M calls). Then `sceKernelBatchMap` op 0 FAILED (ENOMEM): Frostbite maps
   128 KiB of GPU memory (type 3, prot 0x33) at fixed 0x7f0000000, below the PS4 user range (the
   console's system-managed area). Windows only reserved 0x1000000000+; build 8 reserves the free
   1 GiB window around such a request on demand (platform_win.c extend_reservation).
7. Build 8: the 0x7f0000000 window reserved; audio port opened, splash screen hidden, ~200 GPU
   submissions. Stopped in the shader recompiler: translate.cpp SetDst "Unknown field 126"
   (EXEC_LO as a 32-bit destination). Build 9 handles 32-bit writes to EXEC_LO / EXEC_HI (the
   other half keeps its lanes).
8. Build 9: **menus work; Play Now started the game intro (close-up grass) and then crashed**
   (~30 s, 773M runtime calls; save data, trophies, Ajm audio, indirect draws). Crash: thread
   Job5 read 0xF74C (NULL + offset) at guest 0x2ef5a3b; no frame-pointer chain on job threads.
   Not network (offline NetCtl says not connected); modules not found: Engine.BuildInfo.prx and
   EAWebKit.prx (menus still worked). Build 10's crash report adds r8-r15, the code bytes around
   the fault and guest return addresses found on the stack.
