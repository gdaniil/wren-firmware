#!/usr/bin/env python3
"""Build SD fonts whose filename size is the measured monochrome x-height.

Run from the repository: source .venv/bin/activate && python scripts/wren/build_fonts.py
"""
from __future__ import annotations

import contextlib
import argparse
from functools import lru_cache
import io
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

import freetype
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont
from PIL import Image

from download_fonts import CACHE, FILES, REVISION, ROOT, main as download

sys.path.insert(0, str(ROOT / 'lib/EpdFont/scripts'))
import fontconvert_sdcard as converter

OUT = ROOT / 'build/wren-fonts'
FAMILIES = {'Literata': 'literata', 'Golos': 'golostext', 'Bitter': 'bitter'}
PAIRS = ('Го', 'Ту', 'Ук', 'Ге')
ORIGINAL_FACE = freetype.Face
INTERVALS = converter.resolve_intervals('(0x0020-0x017F),(0x0400-0x04FF),(0x0301-0x0301),(0x2013-0x2014),(0x2018-0x2019),(0x201C-0x201E),(0x2026-0x2026),(0x2032-0x2033),(0x20AC-0x20AC),(0x2116-0x2116),(0x2122-0x2122)')


def table():
    """Read both device columns; keep mode membership and every conflicting row."""
    rows, conflicts = {}, []
    for line in (ROOT / 'docs/ours/READER_TYPOGRAPHY_SPEC.md').read_text().splitlines():
        cells = [c.strip() for c in line.strip('|').split('|')]
        if len(cells) != 9 or not cells[0].startswith(('small ', 'standard ', 'large ')):
            continue
        mode = cells[0].split()[0]
        for offset, device in ((1, 'X3'), (5, 'X4')):
            px = int(cells[offset].split('·')[0])
            entry = rows.setdefault(px, {'modes': set(), 'values': {f: [] for f in FAMILIES}})
            entry['modes'].add(mode)
            for i, family in enumerate(FAMILIES):
                entry['values'][family].append((tuple(map(int, cells[offset + 1 + i].split('/'))), f'{cells[0]} {device}'))
    for px, entry in rows.items():
        entry['weights'] = {}
        for family, values in entry['values'].items():
            weights = tuple(max(w[i] for w, _ in values) for i in (0, 1))
            entry['weights'][family] = weights
            if len({w for w, _ in values}) > 1:
                conflicts.append(f'| {family} | {px} | ' + '; '.join(f'{label}: {w[0]}/{w[1]}' for w, label in values) + f' | {weights[0]}/{weights[1]} |')
    assert sorted(rows) == [9,10,11,12,13,14,15,16,17,18,19,21,22,23,24,26,28,30,32,34,36,41]
    return rows, conflicts


def configure(face, weight, em):
    axes = face.get_variation_info().axes
    coords = [weight if a.tag == 'wght' else max(a.minimum, min(a.maximum, em / 259 * 72)) if a.tag == 'opsz' else a.default for a in axes]
    face.set_var_design_coords(coords)
    face.set_char_size(round(em * 64), round(em * 64), 72, 72)


def mono(face, char):
    face.load_char(char, freetype.FT_LOAD_RENDER)
    b = face.glyph.bitmap
    buf = b.buffer
    return [[buf[(y if b.pitch >= 0 else b.rows-1-y)*abs(b.pitch)+x] >= 64 for x in range(b.width)] for y in range(b.rows)]


def ink_height(bitmap):
    ys = [y for y, row in enumerate(bitmap) if any(row)]
    return max(ys)-min(ys)+1 if ys else 0


def find_em(path, weight, xh):
    face = ORIGINAL_FACE(str(path))
    # Find the first 1/64-pixel em whose thresholded x reaches the target.
    lo, hi = 64, xh * 4 * 64
    while lo < hi:
        mid = (lo + hi) // 2
        configure(face, weight, mid / 64)
        if ink_height(mono(face, 'x')) < xh:
            lo = mid + 1
        else:
            hi = mid
    configure(face, weight, lo / 64)
    if ink_height(mono(face, 'x')) != xh:
        raise RuntimeError(f'No exact x-height for {path}, weight {weight}, xh {xh}')
    first = lo
    hi = xh * 4 * 64
    while lo < hi:
        mid = (lo + hi) // 2
        configure(face, weight, mid / 64)
        if ink_height(mono(face, 'x')) <= xh:
            lo = mid + 1
        else:
            hi = mid
    return ((first + lo - 1) // 2) / 64


def instance(source, weight, em, target):
    with TTFont(source) as font:
        axes = {a.axisTag: weight if a.axisTag == 'wght' else max(a.minValue, min(a.maxValue, em / 259 * 72)) if a.axisTag == 'opsz' else a.defaultValue for a in font['fvar'].axes}
        identity = {'source': hashlib.sha256(source.read_bytes()).hexdigest(), 'axes': axes}
        metadata = target.with_suffix('.json')
        if target.exists() and metadata.exists() and json.loads(metadata.read_text()) == identity:
            return axes
        static = instantiateVariableFont(font, axes, inplace=True, optimize=False)
        static.save(target)
        metadata.write_text(json.dumps(identity))
    return axes


@contextlib.contextmanager
def pixel_raster(em, weight, static_path):
    """Adapt the converter's Face sizing and kerning ppem without editing it."""
    old_kern = converter.extract_kerning_fonttools
    class PixelFace(ORIGINAL_FACE):
        def set_char_size(self, *args, **kwargs):
            axes = self.get_variation_info().axes
            self.set_var_design_coords([weight if a.tag == 'wght' else max(a.minimum, min(a.maximum, em / 259 * 72)) if a.tag == 'opsz' else a.default for a in axes])
            super().set_char_size(round(em * 64), round(em * 64), 72, 72)
    freetype.Face = PixelFace
    converter.extract_kerning_fonttools = lambda path, cps, ignored: old_kern(static_path, cps, em)
    try:
        yield
    finally:
        freetype.Face = ORIGINAL_FACE
        converter.extract_kerning_fonttools = old_kern


def bitmap(glyph, data):
    return [[((data[(y*glyph.width+x)//4] >> (6-2*((y*glyph.width+x)%4))) & 3) >= 1 for x in range(glyph.width)] for y in range(glyph.height)]


def stem(bm):
    """Measure separated vertical strokes; joined crossbar rows are ineligible."""
    ink_rows = [row for row in bm if any(row)]
    trim = max(1, math.ceil(len(ink_rows) * .2))
    widths = []
    for row in ink_rows[trim:len(ink_rows)-trim]:
        runs = []
        start = None
        for x, ink in enumerate([*row, False]):
            if ink and start is None:
                start = x
            elif not ink and start is not None:
                runs.append(x-start)
                start = None
        if len(runs) == 2:
            widths.append(runs[0])
    return sorted(widths)[len(widths)//2] if widths else None


@lru_cache(maxsize=None)
def probe(source, weight, xh):
    """Use the converter's actual packing for x and н, omitting unrelated tables."""
    em = find_em(source, weight, xh)
    names = ('extract_kerning_fonttools', 'extract_ligatures_fonttools',
             'extract_ligature_glyph_indices_fonttools')
    originals = {name: getattr(converter, name) for name in names}
    try:
        with pixel_raster(em, weight, str(source)):
            converter.extract_kerning_fonttools = lambda *args: {}
            converter.extract_ligatures_fonttools = lambda *args: []
            converter.extract_ligature_glyph_indices_fonttools = lambda *args: {}
            with contextlib.redirect_stderr(io.StringIO()):
                data = converter.rasterize_font_style(str(source), 1, [(120,120),(1085,1085)])
        glyphs = {g.code_point: (g, packed) for g, packed in data.all_glyphs}
        assert ink_height(bitmap(*glyphs[120])) == xh
        return stem(bitmap(*glyphs[1085])), em
    finally:
        for name, fn in originals.items():
            setattr(converter, name, fn)


@lru_cache(maxsize=None)
def choose_weight(source, xh, target, minimum):
    """Scan in ascending 5-unit steps, recalibrating x-height at each weight."""
    face = ORIGINAL_FACE(str(source))
    axis = next(a for a in face.get_variation_info().axes if a.tag == 'wght')
    candidates = []
    for weight in range(max(minimum, math.ceil(axis.minimum)), math.floor(axis.maximum)+1, 5):
        achieved, em = probe(source, weight, xh)
        if achieved is None:
            continue
        candidates.append((achieved, weight, em))
        if achieved == target:
            return weight, achieved, em, ''
    above = [c for c in candidates if c[0] > target]
    if above:
        achieved, weight, em = min(above)
        return weight, achieved, em, f'UNATTAINABLE {target}px: nearest above {achieved}px'
    if not candidates:
        raise RuntimeError(f'No separated stems can be measured: {source.name}, {xh}px')
    raise RuntimeError(f'No weight reaches target from above: {source.name}, {xh}px, target {target}, maximum {max(c[0] for c in candidates)}')


def stem_targets(entry, xh, spec):
    regular = max(max(2, math.floor(spec['modes'][mode]['stemRatio']*xh+.5))
                  for mode in entry['modes'])
    return regular, math.floor(spec['boldStemFactor']*regular+.5)


def choose_pair(source, xh, targets, minimum):
    regular = choose_weight(source, xh, targets[0], minimum)
    bold_target = max(targets[1], regular[1] + 1)
    bold = choose_weight(source, xh, bold_target, max(minimum, regular[0] + 5))
    assert bold[0] > regular[0] and bold[1] > regular[1]
    return (targets[0], bold_target), (regular, bold)


def read_file(path):
    """Read emitted v4 data independently of the converter's in-memory data."""
    data = path.read_bytes()
    magic, version, flags, count = struct.unpack_from('<8sHHB', data)
    assert magic == b'CPFONT\0\0' and version == converter.CPFONT_VERSION and flags == 1
    result = {}
    for i in range(count):
        sid, ni, ng, ay, asc, desc, nl, nr, lc, rc, lig, offset = struct.unpack_from('<B3xIIBhhHHBBBI4x', data, 32+i*32)
        intervals = [struct.unpack_from('<III', data, offset+j*12) for j in range(ni)]
        pos = offset+ni*12
        glyphs = [struct.unpack_from(converter.GLYPH_STRUCT_FORMAT, data, pos+j*16) for j in range(ng)]
        pos += ng*16
        left = dict(struct.unpack_from('<HB', data, pos+j*3) for j in range(nl)); pos += nl*3
        right = dict(struct.unpack_from('<HB', data, pos+j*3) for j in range(nr)); pos += nr*3
        matrix = struct.unpack_from(f'<{lc*rc}b', data, pos); pos += lc*rc+lig*8
        gs = {}
        for start, end, index in intervals:
            for cp in range(start, end+1):
                w,h,adv,l,t,length,off = glyphs[index+cp-start]
                g = converter.GlyphProps(w,h,adv,l,t,length,off,cp)
                assert pos+off+length <= len(data)
                gs[cp] = (g, data[pos+off:pos+off+length])
        kern = {p: matrix[(left[ord(p[0])]-1)*rc+right[ord(p[1])]-1] if ord(p[0]) in left and ord(p[1]) in right else 0 for p in PAIRS}
        result[sid] = (gs, kern)
    return result


def dflt_pairs(path, em):
    with TTFont(path) as font:
        gpos = font['GPOS'].table
        cmap = font.getBestCmap()
        names = {cmap[ord(c)]: [ord(c)] for p in PAIRS for c in p}
        raw = {}
        script = next(s.Script for s in gpos.ScriptList.ScriptRecord if s.ScriptTag == 'DFLT')
        indices = set()
        for lang in [script.DefaultLangSys] + [r.LangSys for r in script.LangSysRecord]:
            if lang is None:
                continue
            for fi in list(lang.FeatureIndex) + ([lang.ReqFeatureIndex] if lang.ReqFeatureIndex != 65535 else []):
                fr = gpos.FeatureList.FeatureRecord[fi]
                if fr.FeatureTag == 'kern':
                    indices.update(fr.Feature.LookupListIndex)
        for index in indices:
            lookup = gpos.LookupList.Lookup[index]
            for sub in lookup.SubTable:
                actual = getattr(sub, 'ExtSubTable', sub)
                if getattr(sub, 'ExtensionLookupType', lookup.LookupType) == 2:
                    converter._extract_pairpos_subtable(actual, names, raw)
        return {p: converter.fp4_from_design_units(raw.get((cmap[ord(p[0])],cmap[ord(p[1])]),0), em/font['head'].unitsPerEm) for p in PAIRS}


def accents(gs, path):
    im = Image.new('1', (260, 180), 1)
    checks = []
    for word, baseline in [('мука́', 65), ('замо́к', 145)]:
        cursor = 10
        previous = None
        for char in word:
            g, packed = gs[ord(char)]
            if char == '\u0301':
                base, bx = previous
                x = bx + base.left + base.width//2 - g.width//2
                raise_by = max(0, 1-(g.top-g.height-base.top)) if g.top-g.height > 0 else 0
                y = baseline-g.top-raise_by
                checks.append(x >= bx+base.left-1 and x+g.width <= bx+base.left+base.width+1 and y+g.height < baseline-base.top)
            else:
                if previous:
                    cursor += (previous[0].advance_x+8)//16
                x, y = cursor+g.left, baseline-g.top
                previous = (g, cursor)
            for yy, row in enumerate(bitmap(g, packed)):
                for xx, ink in enumerate(row):
                    if ink:
                        im.putpixel((x+xx,y+yy),0)
    im.resize((1040,720), Image.Resampling.NEAREST).save(path)
    return all(checks)


def write_metrics(manifest, chosen):
    """Emit firmware metrics from the same raster instances and measured weights."""
    regular = {(row['family'], row['xh']): row for row in manifest if row['style'] == 0}
    lines = ['// Generated by scripts/wren/build_fonts.py, do not edit.', '#pragma once',
             '#include <cstdint>', '', 'namespace wren {',
             'struct FontMetrics {',
             '  uint8_t xHeightPx;',
             '  uint16_t emPx64, regularWeight, boldWeight;',
             '  uint8_t regularStemPx, boldStemPx;',
             '};',
             '// Family order: Literata, Golos, Bitter.',
             '// clang-format off',
             'inline constexpr FontMetrics WREN_FONT_METRICS[3][22] = {']
    for family in FAMILIES:
        lines.append('  {')
        for xh in sorted(map(int, chosen[family])):
            row = regular[family, xh]
            weight = chosen[family][str(xh)]
            assert row['axes']['wght'] == weight['regular'], (family, xh)
            values = (xh, round(row['em'] * 64), weight['regular'], weight['bold'],
                      weight['stemPx'], weight['boldStemPx'])
            lines.append('    {' + ', '.join(map(str, values)) + '},')
        lines.append('  },')
    lines += ['};', '// clang-format on', '}  // namespace wren', '']
    (ROOT/'src/wren/typography/WrenFontMetrics.generated.h').write_text('\n'.join(lines))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--weights', choices=('table', 'measured'), default='measured')
    parser.add_argument('--metrics-only', action='store_true', help='Generate firmware metrics from the last completed measured build without rebuilding fonts')
    args = parser.parse_args()
    if args.metrics_only:
        manifest = json.loads((OUT/'measurements.json').read_text())['instances']
        chosen = json.loads((ROOT/'scripts/wren/weights.json').read_text())
        write_metrics(manifest, chosen)
        return
    download()
    OUT.mkdir(parents=True, exist_ok=True)
    rows, conflicts = table()
    spec_text = (ROOT/'docs/ours/READER_TYPOGRAPHY_SPEC.md').read_text()
    spec = json.loads(spec_text.split('```json\n', 1)[1].split('```', 1)[0])
    minimums = {f: spec['minWght']['Golos Text' if f == 'Golos' else f] for f in FAMILIES}
    report = [
        '# Wren SD font build report',
        '',
        f'Command: `source .venv/bin/activate && python scripts/wren/build_fonts.py --weights {args.weights}`',
        '',
        f'Weight source: **{args.weights}**. Measured selection scans wght in ascending 5-unit steps from the spec minimum, recalibrating exact x-height and opsz for each candidate. Regular target is the maximum mode target. Bold target is max(round(1.4 × regular target), achieved regular stem + 1), and bold weight must exceed regular weight. An unattainable exact target selects the smallest achieved stem above it, then the lightest eligible weight attaining that stem, and is flagged.',
        '',
        'Measurement correction: the old fixed row bands included the crossbar at 9 px and mistook the whole joined row for a vertical stem (Golos bold: 8 px; Bitter bold: 9 px). The corrected sampler excludes the top/bottom 20% of ink rows and accepts only rows containing exactly two separate ink runs. The median of the left stroke widths gives 3 px for both original 9 px bold bitmaps. The left stroke is retained as the measurement reference because left/right strokes can snap to different pixel widths. Rows without separated stems are unmeasurable and are skipped during selection.',
        '',
        f'Google Fonts revision: `{REVISION}`.',
        '',
        'Size means thresholded 1-bit x-height. Converter 2-bit values >=1 are ink (FreeType coverage >=64), matching GlyphBitmap.h BW levels 0x0e. Em is searched on a 1/64 px grid independently per style; opsz = clamp(em / 259 × 72). X4 at 217 ppi would prefer a different optical size. Native FreeType hinting; no forced autohint. GPOS mark is deliberately omitted.',
        '',
        'Golos contains styles 0/1 only; the reader derives italic from these. Literata/Bitter contain all four styles.',
        '',
        '## Weight conflicts',
        '',
        '| Family | xh | Source rows | Selected regular/bold |',
        '|---|---:|---|---|',
        *conflicts,
        '',
        '## Coverage (fontTools cmap, each source style)',
        '',
        'Basic Latin means printable U+0020–007E; Latin-1 means U+00A0–00FF. C0/C1 controls have no printable coverage requirement. Missing U+00AD is reported, not substituted.',
        '',
    ]
    mismatch, measurements, kerning, accent_rows = [], [], [], []
    manifest = []
    comparison, flags = [], []
    chosen = {f: {} for f in FAMILIES}
    for family, folder in FAMILIES.items():
        sources = [CACHE/folder/n for n in FILES[folder] if n.endswith('.ttf')]
        for source in sources:
            with TTFont(source) as font:
                cmap = font.getBestCmap()
                required = set(range(32,127)) | set(range(160,256)) | set(map(ord,'ёіїєґў́«»„“—…№’“”‘’–€£©™'))
                missing = sorted(required-set(cmap))
                ext = sorted(set(range(0x100,0x180))-set(cmap))
                report += [
                    f'- {source.name}: spec missing: '+(', '.join(f'U+{c:04X}' for c in missing) or 'none')+f'; Latin Extended-A missing ({len(ext)}): '+(', '.join(f'U+{c:04X}' for c in ext) or 'none')+'. SHA256: `'+hashlib.sha256(source.read_bytes()).hexdigest()+'`.',
                ]
        destination = OUT/family
        destination.mkdir(exist_ok=True)
        (destination/'OFL.txt').write_bytes((CACHE/folder/'OFL.txt').read_bytes())
        for xh, entry in sorted(rows.items()):
            styles, ems = {}, {}
            table_weights = entry['weights'][family]
            targets = stem_targets(entry, xh, spec)
            targets, selections = choose_pair(sources[0], xh, targets, minimums[family])
            measured_weights = tuple(s[0] for s in selections)
            achieved = tuple(s[1] for s in selections)
            row_flags = '; '.join(f'{label}: {s[3]}' for label, s in zip(('regular','bold'),selections) if s[3])
            if row_flags:
                flags.append(f'- {family} {xh}px: {row_flags}')
            comparison.append(f'| {family} | {xh} | {table_weights[0]}/{table_weights[1]} | {measured_weights[0]}/{measured_weights[1]} | {targets[0]}/{targets[1]} | {achieved[0]}/{achieved[1]} | {row_flags or "—"} |')
            weights = measured_weights if args.weights == 'measured' else table_weights
            for sid in (range(2) if family == 'Golos' else range(4)):
                source = sources[1] if sid >= 2 else sources[0]
                weight = weights[sid%2]
                em = find_em(source, weight, xh)
                target = CACHE/f'{family}-{xh}-{sid}.ttf'
                axes = instance(source, weight, em, target)
                styles[sid], ems[sid] = str(target), em
                manifest.append({'family':family,'xh':xh,'style':sid,'em':em,'axes':axes})
            output = destination/f'Wren{family}_{xh}.cpfont'
            original = converter.rasterize_font_style
            def raster(path, size, intervals, style_id=0, **kwargs):
                source = sources[1] if style_id >= 2 else sources[0]
                with pixel_raster(ems[style_id], weights[style_id % 2], path):
                    return original(str(source), size, intervals, style_id=style_id, **kwargs)
            converter.rasterize_font_style = raster
            try:
                converter.generate_cpfont_multistyle(styles, 1, INTERVALS, str(output))
            finally:
                converter.rasterize_font_style = original
            built = read_file(output)
            assert set(built) == set(styles)
            for sid, (gs, kern) in built.items():
                assert ink_height(bitmap(*gs[ord('x')])) == xh, (family,xh,sid)
            actual_stems = tuple(stem(bitmap(*built[sid][0][ord('н')])) for sid in (0,1))
            if args.weights == 'measured':
                assert actual_stems == achieved, (family,xh,actual_stems,achieved)
                assert weights[1] > weights[0] and actual_stems[1] > actual_stems[0], (family,xh)
            chosen[family][str(xh)] = {'regular': weights[0], 'bold': weights[1],
                                       'stemPx': actual_stems[0], 'boldStemPx': actual_stems[1]}
            for sid in (0,1):
                measured = stem(bitmap(*built[sid][0][ord('н')]))
                for mode in sorted(entry['modes']):
                    target = max(2, math.floor({'small':.21,'standard':.20,'large':.23}[mode]*xh+.5))
                    if sid:
                        target = math.floor(1.4*target+.5)
                    light = choose_weight(sources[0],xh,target,minimums[family])[0]
                    row = (family,xh,mode,'bold' if sid else 'regular',weights[sid],measured,target,light)
                    measurements.append(row)
                    if measured != target:
                        mismatch.append('| '+' | '.join(map(str,row))+' |')
            if family == 'Golos':
                for sid, (_, kern) in built.items():
                    dflt = dflt_pairs(styles[sid], ems[sid])
                    assert all(kern[p] != 0 and kern[p] == dflt[p] for p in PAIRS), (xh,sid,kern,dflt)
                    kerning.append('| '+str(xh)+' | '+('bold' if sid else 'regular')+' | '+' | '.join(str(kern[p]/16) for p in PAIRS)+' |')
            if xh in (12,15,22):
                image = OUT/f'{family}-{xh}-accents.png'
                ok = accents(built[0][0],image)
                accent_rows.append(f'| {family} | {xh} | {"pass: а in мука́; о in замо́к" if ok else "CHECK IMAGE"} | [{image.name}]({image.name}) |')
            print(f'Completed {family} {xh}', flush=True)
    files = sorted(OUT.glob('*/*.cpfont'))
    total = sum(f.stat().st_size for f in files)
    assert len(files) == 66
    report += [
        '',
        '## Outputs',
        '',
        f'{len(files)} .cpfont files; {total:,} bytes ({total/1024/1024:.3f} MiB). Every emitted style was read back and its x-height checked exactly.',
        '',
        '## Table weights versus measured weights',
        '',
        'Stem columns are regular/bold pixels. Measured weights are calibrated from upright н and also used for italic/bolditalic. scripts/wren/weights.json records the weights and actual stems of the selected build mode.',
        '',
        '| Font | xh | Table wght | Measured wght | Target stem px | Achieved stem px | Flags |',
        '|---|---:|---|---|---|---|---|',
        *comparison,
        '',
        '## Selection flags',
        '',
        *(flags or ['None: every requested target was attainable on the 5-unit grid.']),
        '',
        '## Golos kerning read from emitted files',
        '',
        'Values below are pixels (signed fp4 divided by 16). Each pair is nonzero and equals the DFLT-only extraction from the instantiated source. No wrapper kerning correction was needed.',
        '',
        '| xh | Style | Го | Ту | Ук | Ге |',
        '|---:|---|---:|---:|---:|---:|',
        *kerning,
        '',
        '## Accent check',
        '',
        'PNGs use regular-style converter bitmaps read from emitted files, at 4× nearest-neighbour scale. Placement follows EpdFontData.h anchorOver/raiseAboveBase: integer bitmap centering and minimum 1 px vertical gap. The two words are on separate rows, мука́ first. Checks verify the mark lies over its intended base bitmap and above its top. Pair kerning is irrelevant to this anchor test and omitted from these isolated word previews.',
        '',
        '| Family | xh | Result | Preview |',
        '|---|---:|---|---|',
        *accent_rows,
        '',
        '## Stem mismatches',
        '',
        'Current mode-specific checks use the corrected separated-stroke sampler described above. These rows retain the original per-mode spec targets for comparison; the weight comparison table above includes the achieved-regular bold adjustment. Targets use round-half-up. Lightest weights use a 5-unit grid from the spec minimum with exact x-height recalibration. Modes sharing a size can have different targets; measured selection deliberately uses their maximum.',
        '',
        f'{len(mismatch)} mismatches among {len(measurements)} mode/style checks.',
        '',
        '| Family | xh | Mode | Style | Chosen wght | Measured px | Target px | Lightest wght |',
        '|---|---:|---|---|---:|---:|---:|---:|',
        *mismatch,
        '',
        'Hardware calibration and on-device rendering were not run (host-only task).',
        '',
        (ROOT/'scripts/wren/table-mismatches-reference.md').read_text(),
    ]
    (OUT/'REPORT.md').write_text('\n'.join(report)+'\n')
    (OUT/'measurements.json').write_text(json.dumps({'instances':manifest,'stems':measurements},indent=2)+'\n')
    (ROOT/'scripts/wren/weights.json').write_text(json.dumps(chosen,indent=2)+'\n')
    write_metrics(manifest, chosen)
    print(f'{len(files)} files, {total} bytes; report: {OUT / "REPORT.md"}')


if __name__ == '__main__':
    main()
