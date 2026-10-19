# Madden NFL 19 native port — Windows (bring-up)

This archive runs **Madden NFL 19** (PlayStation 4, `CUSA10038`) as a native Windows x86-64
program, on the runtime and renderer of the UFC 3 native port
(https://github.com/GNSB/UFC-3-PS4-PORT-LINUX-WINDOWS), which also runs EA SPORTS UFC 2. It
**contains no game files**: you need your own dump of the game (and its update, if you have it).

**Status: bring-up.** Madden 19 has not been played on this port yet. It runs on EA's Frostbite
engine, which none of the earlier ports used. Expect the first runs to stop early or to run with
sound and a black screen: the log names what to fix next (see *Problems*).
`docs/madden19/BRINGUP.md` in the source explains the steps.

## Requirements

- 64-bit Windows 10 (1903 or newer) or Windows 11.
- A GPU with a Vulkan 1.3 driver. 6 GB of VRAM or more is recommended.
- A CPU with AVX.
- The game: the base folder `CUSA10038` and, if you have it, its update next to it, e.g.
  `PS4\CUSA10038` and `PS4\CUSA10038-patch` (also `-PATCH`, `-update`, `_patch`). They are
  merged automatically into `CUSA10038-merged` with hard links (no extra space on NTFS).
- The `python` folder from the UFC 3 or UFC 2 Windows package, copied next to `Madden19.exe`
  (this build does not include Python).

## Playing

1. Extract the archive anywhere (not inside the game folder), and copy the `python` folder in.
2. Run **`Madden19.exe`**, pick the `CUSA10038` folder with **Browse...**, choose **Offline**,
   press **Play**.
3. Data, saves and logs live in `%LOCALAPPDATA%\Madden19Native` (separate from the UFC ports).

### Launcher settings

- **Online play** — keep **Offline**: online play is not ported; Join / Host are refused.
- **Game resolution** — 720p / 1080p. The 2K / 4K experiments are UFC 3 only and are ignored.
- **Skip the intro videos** — turn it **off** for Madden 19 for now.
- **Frame statistics** — turn it **on** while testing: the log then shows the frame rate.
- **FPS limit** — 60 is the safest while testing.
- In game: **Insert** or **L3+R3** opens the overlay menu.

## Problems

Send the newest `.log` from `%LOCALAPPDATA%\Madden19Native\logs` (the launcher's **Open logs
folder**) with a short description of what the game window showed.

- `STOP: first unsupported PS4 import: ...` — the game called a PS4 system function the port
  does not provide yet. Expected during bring-up: the name in that line is what to add next.
- Sound but a black window — a graphics problem (shadPS4 shows the same on some versions);
  the log and a screenshot help narrow it down.

If the launcher's log says a program "could not start" (Windows error 0xC0000022, access
denied): allow the file in your antivirus, extract the archive outside OneDrive (e.g.
`C:\Games\Madden19Native`), or unblock the zip (Properties > Unblock) and extract it again.

This project is not affiliated with or endorsed by Electronic Arts, Sony Interactive
Entertainment or the NFL. GPL-2.0-or-later; third-party licences are in `licenses\`.
