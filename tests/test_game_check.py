from paths import ROOT
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import struct
import game_check
from test_prepare import fixture


def param_sfo(entries):
    """A minimal param.sfo with UTF-8 string entries."""
    keys = b''.join(k.encode() + b'\0' for k in entries)
    values = [v.encode() + b'\0' for v in entries.values()]
    count = len(entries)
    key_table = 20 + 16 * count
    data_table = key_table + len(keys)
    data = bytearray(struct.pack('<4sIIII', b'\0PSF', 0x101, key_table, data_table, count))
    key_off = data_off = 0
    for key, value in zip(entries, values):
        data += struct.pack('<HHIII', key_off, 0x204, len(value), len(value), data_off)
        key_off += len(key) + 1
        data_off += len(value)
    return bytes(data + keys + b''.join(values))


class GameCheckTests(unittest.TestCase):
    def game(self, title, version, eboot=None, name='EA SPORTS™ UFC® 3'):
        path = Path(self.tmp.name)
        (path / 'sce_sys').mkdir(exist_ok=True)
        (path / 'sce_sys/param.sfo').write_bytes(param_sfo({'APP_VER': version, 'TITLE': name, 'TITLE_ID': title}))
        (path / 'eboot.bin').write_bytes(fixture() if eboot is None else eboot)
        return path

    def ufc2(self, title='CUSA02588', version='01.07', eboot=None):
        return self.game(title, version, eboot, name='EA SPORTS™ UFC® 2')

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.builds = Path(self.tmp.name) / 'builds.json'
        self.patch = mock.patch.object(game_check, 'UFC2_BUILDS', self.builds)
        self.patch.start()
        os.environ.pop('BB_SKIP_GAME_CHECK', None)

    def tearDown(self):
        self.patch.stop()
        self.tmp.cleanup()

    # UFC 3 (unchanged behaviour)
    def test_ufc3_base_game_without_the_update(self):
        game = self.game('CUSA06534', '01.00')
        self.assertEqual(game_check.problem(game), ('missing_update', 'CUSA06534', '01.00'))
        self.assertIn('01.14', game_check.explain(*game_check.problem(game)))

    def test_ufc3_update_metadata_with_an_old_executable(self):
        game = self.game('CUSA06534', '01.14')
        with mock.patch.dict(game_check.OLD_IMAGES, {game_check.image_sha256(game): '01.00'}):
            self.assertEqual(game_check.problem(game)[0], 'wrong_eboot')
            self.assertIsNone(game_check.profile(game))

    def test_ufc3_supported_image_passes(self):
        game = self.game('CUSA06534', '01.14')
        table = {game_check.image_sha256(game): game_check.SUPPORTED[game_check.SUPPORTED_IMAGE]}
        with mock.patch.object(game_check, 'SUPPORTED', table):
            self.assertIsNone(game_check.problem(game))
            p = game_check.profile(game)
            self.assertEqual((p['name'], p['variant'], p['title']), ('ufc3', '5.05', 'CUSA06534'))

    def test_other_title(self):
        self.assertEqual(game_check.problem(self.game('CUSA03173', '01.09', name='Bloodborne'))[0], 'other_title')

    def test_unreadable_executable(self):
        self.assertEqual(game_check.problem(self.game('CUSA06534', '01.14', b'junk'))[0], 'unreadable')
        self.assertEqual(game_check.problem(self.ufc2(eboot=b'junk'))[0], 'unreadable')

    # UFC 2
    def test_ufc2_recognised_from_param_sfo(self):
        game = self.ufc2()
        self.assertIsNone(game_check.problem(game))
        p = game_check.profile(game)
        self.assertEqual(p['name'], 'ufc2')
        self.assertEqual((p['title'], p['version']), ('CUSA02588', '01.07'))
        # Not '5.05': the hooks keyed on UFC 3's addresses stay off (BbToggle::ReferenceBuild).
        self.assertEqual(p['variant'], 'ufc2')
        self.assertFalse(p['known'])

    def test_ufc2_any_region_and_base_game(self):
        for title, version in (('CUSA02560', '01.00'), ('CUSA03159', '01.07')):
            self.assertEqual(game_check.profile(self.ufc2(title, version))['name'], 'ufc2')

    def test_ufc2_by_title_id_list(self):
        game = self.game('CUSA09999', '01.00', name='Some localised name')
        self.assertEqual(game_check.problem(game)[0], 'other_title')
        with mock.patch.object(game_check, 'UFC2_TITLES', {'CUSA09999'}):
            self.assertEqual(game_check.profile(game)['name'], 'ufc2')

    def test_ufc3_is_not_taken_for_ufc2(self):
        self.assertFalse(game_check.is_ufc2({'TITLE': 'EA SPORTS™ UFC® 3', 'TITLE_ID': 'CUSA06534'}))
        self.assertFalse(game_check.is_ufc2({'TITLE': 'EA SPORTS UFC 4'}))
        self.assertTrue(game_check.is_ufc2({'TITLE': 'EA SPORTS UFC 2'}))

    def test_ufc2_register_marks_the_build_known(self):
        game = self.ufc2()
        game_check.register(game)
        builds = json.loads(self.builds.read_text())
        self.assertEqual(builds[game_check.image_sha256(game)]['title'], 'CUSA02588')
        self.assertTrue(game_check.profile(game)['known'])

    def test_ufc2_register_refuses_other_games(self):
        with self.assertRaises(SystemExit):
            game_check.register(self.game('CUSA06534', '01.14'))

    def test_profile_guess_with_the_check_skipped(self):
        game = self.game('CUSA06534', '01.00')
        self.assertIsNone(game_check.profile(game))
        with mock.patch.dict(os.environ, {'BB_SKIP_GAME_CHECK': '1'}):
            self.assertEqual(game_check.profile(game)['name'], 'ufc3')
            self.assertEqual(game_check.profile(self.ufc2(eboot=b'junk'))['name'], 'ufc2')

    def test_every_problem_is_explained(self):
        for kind in ('missing_update', 'wrong_eboot', 'other_title', 'unreadable'):
            self.assertIn('CUSA', game_check.explain(kind, 'CUSA06534', '01.00'))
            self.assertIn('CUSA', game_check.explain(kind, 'CUSA00000', '01.00'))
        self.assertIn('UFC 2', game_check.explain('other_title', 'CUSA00000', '01.00'))

    def test_command_line_fields(self):
        game = self.ufc2()
        env = dict(os.environ, BB_UFC2_BUILDS=str(self.builds))
        run = lambda *a: subprocess.run([sys.executable, str(ROOT / 'scripts/game_check.py'), str(game), *a],
                                        capture_output=True, text=True, env=env, check=True).stdout.strip()
        self.assertEqual(run(), 'ufc2')
        self.assertEqual(run('--variant'), 'ufc2')
        self.assertEqual(run('--field', 'title'), 'CUSA02588')
        self.assertEqual(run('--field', 'version'), '01.07')
        self.assertEqual(run('--field', 'modules'), 'libc.prx libSceFios2.prx')
        self.assertEqual(json.loads(run('--info'))['TITLE_ID'], 'CUSA02588')


if __name__ == '__main__':
    unittest.main()
