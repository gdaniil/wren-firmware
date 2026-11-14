"""Host regression checks: python -m unittest discover -s scripts/wren."""
import unittest
import json
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import build_fonts as build


def pixels(lines):
    return [[c == '#' for c in line] for line in lines]


class StemTests(unittest.TestCase):
    def test_original_golos_9_bold_crossbar_is_not_a_stem(self):
        bm = pixels(['.##....##.', '.###..###.', '.###..###.',
                     '.########.', '.########.', '.########.',
                     '.###..###.', '.###..###.', '.###..###.'])
        self.assertEqual(build.stem(bm), 3)

    def test_original_bitter_9_bold_excludes_serifs_and_crossbar(self):
        bm = pixels(['#####.#####', '#####.#####', '.###...###.',
                     '.#########.', '.#########.', '.#########.',
                     '.###...###.', '.####.####.', '#####.#####'])
        self.assertEqual(build.stem(bm), 3)

    def test_left_stroke_remains_reference_when_snapping_differs(self):
        self.assertEqual(build.stem(pixels(['.##...###.']*9)), 2)

    def test_joined_rows_are_unmeasurable(self):
        self.assertIsNone(build.stem(pixels(['#########']*9)))

    def test_shared_size_uses_heavier_mode_target(self):
        spec = {'modes': {'small': {'stemRatio': .21},
                          'standard': {'stemRatio': .20}}, 'boldStemFactor': 1.4}
        self.assertEqual(build.stem_targets({'modes': {'small', 'standard'}}, 12, spec), (3, 4))

    def test_probe_restores_converter_hooks_on_failure(self):
        names = ('extract_kerning_fonttools', 'extract_ligatures_fonttools',
                 'extract_ligature_glyph_indices_fonttools')
        originals = {name: getattr(build.converter, name) for name in names}
        face = build.freetype.Face
        with patch.object(build, 'find_em', return_value=20), patch.object(
                build.converter, 'rasterize_font_style', side_effect=RuntimeError('probe failure')):
            with self.assertRaisesRegex(RuntimeError, 'probe failure'):
                build.probe(Path('fake.ttf'), 350, 12)
        self.assertIs(build.freetype.Face, face)
        for name, fn in originals.items():
            self.assertIs(getattr(build.converter, name), fn)


class SelectionTests(unittest.TestCase):
    def setUp(self):
        build.choose_weight.cache_clear()
        axis = SimpleNamespace(tag='wght', minimum=200, maximum=400)
        face = SimpleNamespace(get_variation_info=lambda: SimpleNamespace(axes=[axis]))
        self.face_patch = patch.object(build, 'ORIGINAL_FACE', return_value=face)
        self.face_patch.start()
        self.addCleanup(self.face_patch.stop)
        self.addCleanup(build.choose_weight.cache_clear)

    def test_lightest_exact_match_and_minimum(self):
        with patch.object(build, 'probe', side_effect=lambda p, w, x: (3 if w >= 365 else 2, 20)) as probe:
            self.assertEqual(build.choose_weight(Path('fake.ttf'), 12, 3, 350), (365, 3, 20, ''))
            self.assertEqual([c.args[1] for c in probe.call_args_list], [350, 355, 360, 365])

    def test_unattainable_selects_nearest_above_then_lightest(self):
        with patch.object(build, 'probe', side_effect=lambda p, w, x: (5 if w < 370 else 4, 20)):
            weight, achieved, em, flag = build.choose_weight(Path('fake.ttf'), 12, 3, 350)
            self.assertEqual((weight, achieved, em), (370, 4, 20))
            self.assertIn('UNATTAINABLE', flag)

    def test_cannot_silently_select_below_target(self):
        with patch.object(build, 'probe', return_value=(2, 20)):
            with self.assertRaisesRegex(RuntimeError, 'No weight reaches target from above'):
                build.choose_weight(Path('fake.ttf'), 12, 3, 350)

    def test_bold_uses_achieved_regular_stem_and_strictly_higher_weight(self):
        with patch.object(build, 'choose_weight', side_effect=[
                (400, 3, 20, 'nearest above'), (650, 4, 20, '')]) as choose:
            targets, selections = build.choose_pair(Path('fake.ttf'), 11, (2, 3), 400)
        self.assertEqual(targets, (2, 4))
        self.assertEqual(choose.call_args_list[1].args, (Path('fake.ttf'), 11, 4, 405))
        self.assertGreater(selections[1][0], selections[0][0])
        self.assertGreater(selections[1][1], selections[0][1])


class ExportedWeightsTests(unittest.TestCase):
    def test_every_exported_size_has_distinct_heavier_bold(self):
        weights = json.loads((build.ROOT/'scripts/wren/weights.json').read_text())
        rows, _ = build.table()
        self.assertEqual(set(weights), set(build.FAMILIES))
        for family, sizes in weights.items():
            self.assertEqual(set(sizes), set(map(str, rows)))
            for xh, entry in sizes.items():
                with self.subTest(family=family, xh=xh):
                    self.assertGreater(entry['boldStemPx'], entry['stemPx'])
                    self.assertGreater(entry['bold'], entry['regular'])


if __name__ == '__main__':
    unittest.main()
