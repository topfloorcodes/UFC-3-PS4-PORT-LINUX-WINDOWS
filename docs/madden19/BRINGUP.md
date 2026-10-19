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
