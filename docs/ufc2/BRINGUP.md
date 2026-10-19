# UFC 2 on the UFC 3 native port — bring-up guide

**EA SPORTS UFC 2** (PS4, 2016; target dump **CUSA01968**) is the previous game on the same EA Ignite engine as UFC 3. This
branch (`ufc2`) teaches the UFC 3 port to recognise a UFC 2 dump and run it with a separate
profile. It has **not yet been run on a UFC 2 dump**: what is below is the scaffolding plus the
steps to get from "it starts" to "it plays", in the same order the UFC 3 port went through them.

> No game files are included or needed in this repository. You need your own dump of UFC 2 (and,
> ideally, its last update).

## What this branch changes

| Where | What |
|---|---|
| `scripts/game_check.py` | A second profile, `ufc2`, recognised from `param.sfo` (TITLE names UFC 2, any region, or a TITLE_ID in `UFC2_TITLES`). Variant `ufc2`, so the hooks that patch UFC 3's code by address (`BB_HEAP_HOOKS`, `BB_HEAP_SITES`, `BB_FREE_CHECK`) stay off. New flags: `--field title|version|modules`, `--info` (title, version, image hash), `--register` (records the dump's image hash in `share/ufc2_builds.json`). |
| `run.sh` | Title ID, app version and linked modules come from the profile instead of `CUSA06534` / `01.14`. A `ufc2` defaults block (below). Shader cache cleared per title. |
| `texture_cache/image.cpp` | Image barriers whose range runs past the image's mips or layers are clamped (logged once) instead of asserting. shadPS4 reaches UFC 2's (CUSA01968) menus and then crashes on that assertion (shadPS4 compatibility issue #391), and this renderer is derived from shadPS4. |
| `vk_pipeline_cache.cpp` | `BB_SHADER_PATCH_DIR` picks the shader-patch folder, so UFC 3's hash-keyed patches never load in UFC 2 (UFC 2 uses `user/shader/patch-ufc2`, sources in `patches/shaders-ufc2/`). |
| `packaging/windows/sacredfire_launcher.c` | Branded UFC 2 / CUSA01968 (window, folder prompts, `%LOCALAPPDATA%\UFC2Native`); profile-driven title/version; UFC 2 defaults; 2K/4K experiments (UFC 3 address patches) and online refused for UFC 2. Packages build as `UFC2.exe` / `UFC2Native-*.zip`. |
| `packaging/linux/ufc2.sh` | Linux entry point: `./ufc2.sh /path/to/CUSAxxxxx`, `--info`, `--register`. Own data folder `~/.local/share/UFC2Native`. |
| `tests/test_game_check.py` | Rewritten for UFC 3 + UFC 2 (the old tests still described Crash Team Racing). |

The C runtime needed no game-specific change: it already reads the title ID from `param.sfo` for
save data and AppContent.

## Defaults for the `ufc2` profile

Kept from UFC 3 because they fix engine-level behaviour, not UFC 3 code:

- `BB_READBACKS=2 BB_READBACK_LINEAR=1` — UFC 3's fight scene was black without them (the same
  as shadPS4 #4375 for UFC 4); expect UFC 2 to need them too.
- `BB_GPU_WRITE_TWINS=0` — deadlocks against precise readbacks.
- `BB_Z16S8_AS_D16=1`, `BB_FLIP_RATE=0 BB_VBLANK_HZ=480`, `BB_PREUPLOAD=1`.

Off until checked on UFC 2 (each one is an environment variable away):

- `BB_WRITE_WINDOW=0 BB_WRITE_STEP=0` — tuned against UFC 3's copy sites; try `BB_WRITE_WINDOW=1`
  once fights render, it was UFC 3's biggest FPS win.
- `BB_SKIP_MOVIES=0` — UFC 3's intro-skip clip is matched to UFC 3's movie files.
- `BB_FPS_LIMIT=60` — the frame-rate unlock (flip rate 0) assumes the simulation follows real
  frame time, as UFC 3's does. If UFC 2 runs fast, set `BB_FLIP_RATE=1 BB_VBLANK_HZ=60`
  (console 30 FPS) and look for its present-interval setter, as was done for UFC 3.
- Online — refused. UFC 2's Blaze client is an older protocol than the one
  `tools/ufc3_server.py` emulates; porting it is its own project (see `docs/ufc3/ONLINE_*.md`).

## Bring-up, step by step

1. **Dump and merge.** Base game folder plus its update side by side (`CUSAxxxxx` and
   `CUSAxxxxx-patch`); `ufc2.sh` merges them with `scripts/merge_update.py`.
2. **Identify it.** `./packaging/linux/ufc2.sh --info /path/to/CUSAxxxxx` prints the title ID,
   version and image hash. Then `--register` records it, so the game check knows this exact
   build from then on. Please add the title ID to `UFC2_TITLES` in a PR with the hash.
3. **Build** as for UFC 3: `bash build.sh`.
4. **Missing imports.** Run `BB_GAME_DIR=/path bash run.sh`. The runtime stops at the first PS4
   import it does not implement: `STOP: first unsupported PS4 import: NID#lib`. Implement it in
   the matching `src/runtime_*.c` (most will be stubs: UFC 2 calls older SDK functions and some
   network/PSN ones UFC 3 does not) and repeat. To see them all at once without running:
   `out/sacredfire out/boot-linked.bin --list-imports --import-names out/import_names.txt`.
   If `link_modules.py` finds other PRXs in UFC 2's `sce_module` worth linking natively, add them
   to `UFC2['modules']` in `game_check.py`; modules a dump lacks are skipped.
5. **Boot to menus.** Expect the UFC 3 class of problems first: kernel-event timing, GPU hangs
   (use `BB_VK_VALIDATION=1` and the NVIDIA Xid / breadcrumb reports), audio formats in AJM.
   `docs/ufc3/GROK_HANDOFF.md` §5 lists each one UFC 3 hit and its fix.
6. **Fights.** Black scene → check the readback switches are on. Wrong-looking surfaces → frame
   capture (`BB_CAPTURE_ON_FLIP=1 …`, cheat sheet in `GROK_HANDOFF.md` §7), then a GLSL patch in
   `patches/shaders-ufc2/<hash>.comp` if a shader needs one.
7. **Performance.** Try `BB_WRITE_WINDOW=1 BB_WRITE_STEP=1`, then `BB_GUEST_FPS_LIMIT`. Any
   address-keyed hook (like UFC 3's `bbport_guest_hooks.cpp`) must be found again in UFC 2's
   executable; gate it on `BB_GAME_PROFILE=ufc2` and the registered hash, never on UFC 3's.
8. **Update this file** with what you found, and turn the `ufc2` defaults into tested ones.

## What is known about CUSA01968 already

From shadPS4's compatibility tracker (issue #391), on the emulator this renderer comes from:
version 01.00 boots, reaches the menus, then crashed before a fight on an `image.cpp` barrier
assertion (handled here, see above). The log also showed a missing
`data/kettle/rendering/characters/clothing/womens_tanktop/womens_tanktop_lod7.mcd` (the game
tolerates missing LODs) and the `sceNetEpoll*` calls, which this runtime implements.

## What will not carry over

- Anything keyed on UFC 3's executable addresses or shader hashes: guest hooks, the 2K/4K patch
  experiments, `patches/shaders/*.comp`, the free-check diagnostic.
- Online play (different Blaze version).
- The GTK launcher and `UFC3.exe` branding: `ufc2.sh` is a plain command-line entry point; the
  Windows launcher still says "UFC 3" in its window text.
