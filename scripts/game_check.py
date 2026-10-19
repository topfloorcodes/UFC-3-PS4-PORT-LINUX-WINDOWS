#!/usr/bin/env python3
"""The game files this port runs, keyed on the executable's loaded image.

Two profiles:

* ufc3 - EA SPORTS UFC 3 CUSA06534 (EU) with the 1.14 update merged in. Checked by image hash
  (the hash is the same whatever tool dumped it; the SELF headers around it differ).
* ufc2 - EA SPORTS UFC 2 (any region) with its last update merged in. Bring-up profile: UFC 2's
  executable hashes are not known yet, so it is recognised from param.sfo (TITLE names UFC 2, or
  TITLE_ID is in UFC2_TITLES / share/ufc2_builds.json). `game_check.py GAME --register` records the
  dump's image hash in share/ufc2_builds.json so later launches check it like UFC 3's.
  Its variant is 'ufc2', so the diagnostics that hook UFC 3's code by address stay off
  (BbToggle::ReferenceBuild: only variant '5.05' turns them on).

BB_SKIP_GAME_CHECK=1 skips the check. The variant names '5.05' / '6.72+' come from SacredFire
(Crash Team Racing builds for two firmwares); for UFC 3, '5.05' means "the reference 1.14 image".
"""
import hashlib
import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
UFC2_BUILDS = Path(os.environ.get('BB_UFC2_BUILDS', ROOT / 'share/ufc2_builds.json'))

# image sha256 -> profile. `name` is what the runtime and the GPU menu read (BB_GAME_PROFILE);
# `modules` are the sce_module PRXs link_modules.py links natively (those missing from a dump are
# skipped); `patches` says whether patches.py has a patch file for it.
SUPPORTED = {
    '46c2477702ae5d3fbff3a1911f10841aa150d20941d515ffb3edec75153f055b': dict(
        name='ufc3', title='CUSA06534', version='01.14', game='EA SPORTS UFC 3', update='CUSA06534-patch',
        modules=('libc.prx', 'libSceFios2.prx'), patches=False),
}
# Images that are known NOT to be the supported update (base game executables).
OLD_IMAGES = {
    '1dc99716a77f5411834d004d6becc3c829e382a244aa0dcbd63932a7eeee0bf1': '01.00',
}
KNOWN_672 = set()  # other builds of UFC 3 1.14 (none known)
SUPPORTED_TITLE = 'CUSA06534'
SUPPORTED_VERSION = '01.14'
SUPPORTED_IMAGE = '46c2477702ae5d3fbff3a1911f10841aa150d20941d515ffb3edec75153f055b'

# UFC 2 (2016, same EA Ignite engine family as UFC 3). Title IDs vary by region; add yours here or
# let --register record it. The TITLE pattern catches any region's param.sfo.
UFC2 = dict(name='ufc2', game='EA SPORTS UFC 2', version=None, update='<title>-patch',
            modules=('libc.prx', 'libSceFios2.prx'), patches=False)
UFC2_TITLES = set()
UFC2_TITLE_RE = re.compile(r'\bUFC\W*2\b', re.IGNORECASE)


def ufc2_builds():
    """share/ufc2_builds.json: {image_sha256: {"title": ..., "version": ...}} from --register."""
    try:
        data = json.loads(UFC2_BUILDS.read_text())
        return data if isinstance(data, dict) else {}
    except (OSError, ValueError):
        return {}


def is_ufc2(info):
    title = str(info.get('TITLE', ''))
    return info.get('TITLE_ID') in UFC2_TITLES or bool(UFC2_TITLE_RE.search(title))


def ufc2_profile(info, image_hash, builds=None):
    builds = ufc2_builds() if builds is None else builds
    known = builds.get(image_hash)
    return dict(UFC2, title=info.get('TITLE_ID', '?'), version=info.get('APP_VER', '?'),
                update=f"{info.get('TITLE_ID', 'CUSAxxxxx')}-patch", variant='ufc2',
                known=known is not None)


BY_TITLE = {p['title']: p for p in SUPPORTED.values()}


def image_sha256(game):
    """The SHA-256 of eboot.bin's loaded image (its loadable segments at their addresses)."""
    from prepare import parse_self, span  # same directory
    elf, header, ph, _segments, _missing = parse_self((Path(game) / 'eboot.bin').read_bytes())
    loads = [p for p in ph if p['type'] in (1, 0x61000010)]
    image = bytearray(max(p['vaddr'] + p['memsz'] for p in loads))
    for p in loads:
        image[p['vaddr']:p['vaddr'] + p['filesz']] = span(elf, p['offset'], p['filesz'])
    return hashlib.sha256(image).hexdigest()


def _hash(game, image_hash=None):
    try:
        return image_hash or image_sha256(game)
    except (OSError, ValueError, IndexError, StopIteration, KeyError):
        return None


def profile(game, image_hash=None):
    """The profile dict for a supported game folder, else None (BB_SKIP_GAME_CHECK=1: a profile
    guessed from param.sfo when one exists, so run.sh can still branch)."""
    image_hash = _hash(game, image_hash)
    if image_hash in SUPPORTED:
        return dict(SUPPORTED[image_hash], variant='5.05')
    info = sfo_info(game)
    skip = os.environ.get('BB_SKIP_GAME_CHECK') == '1'
    if is_ufc2(info) and (image_hash or skip):
        return ufc2_profile(info, image_hash)
    if image_hash and not skip and problem(game, image_hash) is None:
        return dict(BY_TITLE[info['TITLE_ID']], variant='6.72+', known=image_hash in KNOWN_672)
    if not skip:
        return None
    found = BY_TITLE.get(info.get('TITLE_ID'))
    return dict(found, variant='?') if found else None


def sfo_info(game):
    from prepare import sfo
    try:
        return sfo((Path(game) / 'sce_sys/param.sfo').read_bytes())
    except (OSError, ValueError, IndexError):
        return {}


def problem(game, image_hash=None):
    """None for a supported game, else (kind, title, version): kind is 'missing_update' (base
    game or an older update), 'wrong_eboot' (param.sfo names the supported update, eboot.bin is
    another version), 'other_title' (another edition, region or game) or 'unreadable'."""
    if os.environ.get('BB_SKIP_GAME_CHECK') == '1':
        return None
    info = sfo_info(game)
    title, version = info.get('TITLE_ID', '?'), info.get('APP_VER', '?')
    image_hash = _hash(game, image_hash)
    if image_hash is None:
        return 'unreadable', title, version
    if image_hash in SUPPORTED:
        return None
    if is_ufc2(info):
        # UFC 2: any build runs in bring-up (run.sh warns when it is the base game, 01.00).
        return None
    if title not in BY_TITLE:
        return 'other_title', title, version
    if version != BY_TITLE[title]['version']:
        return 'missing_update', title, version
    if image_hash in OLD_IMAGES:
        return 'wrong_eboot', title, version
    return None  # another 1.14 build: accepted, see KNOWN_672


def explain(kind, title, version):
    """What is wrong and what to do, for the log (English)."""
    found = f'Found {title} version {version} (sce_sys/param.sfo).'
    p = BY_TITLE.get(title, BY_TITLE[SUPPORTED_TITLE])
    supported = ', '.join(f"{q['game']} {q['title']} {q['version']}" for q in SUPPORTED.values())
    supported += ', EA SPORTS UFC 2 (any region, bring-up profile)'
    return {
        'missing_update': f"{found} The port needs the {p['version']} update merged into the game folder: copy "
                          f"everything from the dumped {p['version']} update (e.g. {p['update']}) into the "
                          'game folder, replacing files (eboot.bin and sce_sys too).',
        'wrong_eboot': f"{found} param.sfo is from {p['version']} but eboot.bin is not: copy eboot.bin from "
                       f"the dumped {p['version']} update into the game folder, replacing the old one.",
        'other_title': f'{found} Supported: {supported}; other editions and regions have a different executable.',
        'unreadable': f'{found} eboot.bin could not be read as a decrypted PS4 executable: dump '
                      'the game and its update again.',
    }[kind]


def register(game):
    """Record this UFC 2 dump's image hash (title, version) in share/ufc2_builds.json."""
    info = sfo_info(game)
    if not is_ufc2(info):
        sys.exit(f"Not a UFC 2 folder: param.sfo TITLE={info.get('TITLE')!r} TITLE_ID={info.get('TITLE_ID')!r}")
    image_hash = _hash(game)
    if not image_hash:
        sys.exit(explain('unreadable', info.get('TITLE_ID', '?'), info.get('APP_VER', '?')))
    builds = ufc2_builds()
    builds[image_hash] = dict(title=info.get('TITLE_ID'), version=info.get('APP_VER'),
                              sdk=info.get('PUBTOOLINFO', ''), name=info.get('TITLE'))
    UFC2_BUILDS.parent.mkdir(parents=True, exist_ok=True)
    UFC2_BUILDS.write_text(json.dumps(builds, indent=2, sort_keys=True) + '\n')
    print(f"Registered UFC 2 {info.get('TITLE_ID')} {info.get('APP_VER')}: {image_hash}")


if __name__ == '__main__':
    # game_check.py GAME_DIR: prints the profile name (run.sh branches on it), or fails with the
    # reason. --variant: the build variant ('5.05', '6.72+', 'ufc2' or '?') for BB_GAME_VARIANT.
    # --field KEY: one profile field (title, version, modules: space-separated). --info: the
    # image hash and param.sfo fields. --register: record a UFC 2 dump (see register()).
    argv = sys.argv[1:]
    flags = {'--variant', '--info', '--register'}
    field = None
    if '--field' in argv:
        i = argv.index('--field')
        field = argv[i + 1] if i + 1 < len(argv) else sys.exit('--field needs a key')
        del argv[i:i + 2]
    args = [a for a in argv if a not in flags]
    game = Path(args[0]) if args else Path('.')
    if '--register' in argv:
        register(game)
        sys.exit(0)
    if '--info' in argv:
        info = sfo_info(game)
        print(json.dumps(dict(image_sha256=_hash(game), **{k: info.get(k) for k in
                              ('TITLE', 'TITLE_ID', 'APP_VER', 'VERSION', 'PUBTOOLINFO')}), indent=2))
        sys.exit(0)
    found = problem(game)
    if found:
        sys.exit('Unsupported game files: ' + explain(*found))
    p = profile(game)
    if '--variant' in argv:
        print(p.get('variant', '?') if p else '?')
    elif field:
        value = p.get(field, '') if p else ''
        print(' '.join(value) if isinstance(value, (tuple, list)) else value)
    else:
        print(p['name'] if p else 'unknown')
