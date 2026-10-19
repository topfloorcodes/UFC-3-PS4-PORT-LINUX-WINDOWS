# UFC 2 native port — Windows (bring-up)

This archive runs **EA SPORTS UFC 2** (PlayStation 4, `CUSA01968`) as a native Windows x86-64
program, on the runtime and renderer of the UFC 3 native port
(https://github.com/GNSB/UFC-3-PS4-PORT-LINUX-WINDOWS). It **contains no game files**: you need
your own dump of the game (and its update, if you have it).

**Status: bring-up.** UFC 2 has not been played on this port yet. Expect the first runs to stop
early: the log then names what to fix next (see *Problems*). `docs/ufc2/BRINGUP.md` in the source
explains the steps.

## Requirements

- 64-bit Windows 10 (1903 or newer) or Windows 11.
- A GPU with a Vulkan 1.3 driver. 6 GB of VRAM or more is recommended.
- A CPU with AVX.
- The game: the base folder `CUSA01968` and, if you have it, its update next to it, e.g.
  `PS4\CUSA01968` and `PS4\CUSA01968-patch` (also `-PATCH`, `-update`, `_patch`). They are
  merged automatically into `CUSA01968-merged` with hard links (no extra space on NTFS).

## Playing

1. Extract the archive anywhere (not inside the game folder).
2. Run **`UFC2.exe`**, pick the `CUSA01968` folder with **Browse...**, choose **Offline**, press
   **Play**.
3. The first start prepares the game for a few seconds. Data, saves and logs live in
   `%LOCALAPPDATA%\UFC2Native` (separate from a UFC 3 install).

### Launcher settings

- **Player name** — the name the game shows (16 characters, ASCII).
- **Game language** — the PS4 system language the game reads.
- **Online play** — keep **Offline**: online play is not ported to UFC 2 (its online service uses
  another server version than the UFC 3 port's), and Join / Host are refused for UFC 2.
- **Controls...** — keyboard keys and gamepad buttons for every PS4 input.
- **Game resolution** — 720p / 1080p. The 2K / 4K experiments patch UFC 3's executable and are
  ignored for UFC 2.
- **Display resolution**, **Upscaling**, **Sharpening** — window size and scaling.
- **PS4 Pro mode** — the game's Pro rendering path, if it has one.
- **Skip the intro videos** — turn it **off** for UFC 2 for now (the replacement clip is made
  for UFC 3's videos).
- **Present mode** and **FPS limit** — as you like; 60 is the safest limit while testing.
- In game: **Insert** or **L3+R3** opens the overlay menu.

## Problems

Send the newest `.log` from `%LOCALAPPDATA%\UFC2Native\logs` (the launcher's **Open logs
folder**) with a short description of where it stopped; checking **Frame statistics** first
makes the log more useful.

- `STOP: first unsupported PS4 import: ...` — the game called a PS4 system function the port
  does not provide yet. This is expected during bring-up: the name in that line is what to
  implement next.
- `Barrier range past the image ... clamped` — a warning, not an error: the renderer met the
  case that crashes shadPS4 on UFC 2 after the menus, and handled it.
- For UFC 2 the faster memory path (write windows) is off. To try it: `set BB_WRITE_WINDOW=1`
  and `set BB_WRITE_STEP=1` in a `.bat` before starting `UFC2.exe`.

If the launcher's log says a program "could not start" (Windows error 0xC0000022, access
denied): allow the file in your antivirus (Windows Security > Protection history), extract the
archive outside OneDrive (e.g. `C:\Games\UFC2Native`), or unblock the zip (Properties >
Unblock) and extract it again.

This project is not affiliated with or endorsed by Electronic Arts, Sony Interactive
Entertainment or the UFC. GPL-2.0-or-later; third-party licences are in `licenses\`.
