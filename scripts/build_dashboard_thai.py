#!/usr/bin/env python3
"""Pre-shape Thai base/mark clusters into flash-only glyph plans.

Requires Pillow, freetype-py, fonttools and uharfbuzz. Normal firmware builds use
the checked-in atlas and do not download or shape fonts.
"""
import argparse
import hashlib
from itertools import product
from pathlib import Path
import urllib.request

import freetype
import uharfbuzz as hb
from fontTools.ttLib import TTFont
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "build/thai-source"
OUT = ROOT / "src/dashboard/thai"
COMMIT = "8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5"
INPUTS = {
    "NotoSansThai.ttf": (
        f"https://raw.githubusercontent.com/google/fonts/{COMMIT}/ofl/notosansthai/NotoSansThai%5Bwdth,wght%5D.ttf",
        "5a1c559bb539583c8a1fd99d1c5b9491e5e14478c9cd2bd0970d5c3096cc9ef8"),
    "OFL.txt": (
        f"https://raw.githubusercontent.com/google/fonts/{COMMIT}/ofl/notosansthai/OFL.txt",
        "2e98fd23a52d253db8612cd5942c8f2ff4111b21d2367050fdca91d8ccc374a0"),
}
MARKS = [0x0E31, *range(0x0E34, 0x0E3B), *range(0x0E47, 0x0E4F)]
SIZE = 32


def key(text):
    value = 0
    for char in text:
        value = (value << 7) | (ord(char) - 0x0E00)
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--download", action="store_true")
    args = parser.parse_args()
    SOURCE.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    for name, (url, digest) in INPUTS.items():
        path = SOURCE / name
        if args.download and not path.exists():
            path.write_bytes(urllib.request.urlopen(url, timeout=60).read())
        assert hashlib.sha256(path.read_bytes()).hexdigest() == digest, name
    font_data = (SOURCE / "NotoSansThai.ttf").read_bytes()
    font = hb.Font(hb.Face(font_data))
    font.set_variations({"wght": 400, "wdth": 100})
    font.scale = (SIZE * 64, SIZE * 64)
    face = freetype.Face(str(SOURCE / "NotoSansThai.ttf"))
    face.set_var_design_coords([400, 100])
    face.set_pixel_sizes(0, SIZE)
    cmap = TTFont(SOURCE / "NotoSansThai.ttf").getBestCmap()
    texts = {chr(cp) for cp in cmap if 0x0E00 < cp <= 0x0E5B}
    for base in range(0x0E01, 0x0E2F):
        for length in (1, 2):
            for marks in product(MARKS, repeat=length):
                texts.add(chr(base) + "".join(map(chr, marks)))
        # HarfBuzz reorders the nikhahit component of sara am below tone marks.
        for tone in [None, *range(0x0E48, 0x0E4C)]:
            texts.add(chr(base) + (chr(tone) if tone else "") + "\u0e33")

    glyphs = []
    glyph_ids = {}
    pixels = bytearray()
    plans = bytearray()
    plan_offsets = {}
    records = []
    min_y = 0
    max_y = 0
    for text in sorted(texts, key=key):
        buf = hb.Buffer()
        buf.add_str(text)
        buf.guess_segment_properties()
        hb.shape(font, buf)
        cursor = 0
        steps = []
        for info, pos in zip(buf.glyph_infos, buf.glyph_positions):
            assert info.codepoint != 0, repr(text)
            gid = info.codepoint
            if gid not in glyph_ids:
                face.load_glyph(gid, freetype.FT_LOAD_RENDER | freetype.FT_LOAD_NO_HINTING)
                bitmap = face.glyph.bitmap
                packed = bytearray()
                if bitmap.width and bitmap.rows:
                    raw = Image.frombytes("L", (bitmap.width, bitmap.rows), bytes(bitmap.buffer), "raw", "L", bitmap.pitch)
                    # Pack continuously, without byte padding at row boundaries.
                    bit = 0
                    for value in raw.getdata():
                        if bit % 8 == 0: packed.append(0)
                        if value >= 112: packed[-1] |= 0x80 >> (bit % 8)
                        bit += 1
                glyph_ids[gid] = (len(glyphs), face.glyph.bitmap_left, face.glyph.bitmap_top)
                glyphs.append((len(pixels), bitmap.width, bitmap.rows))
                pixels.extend(packed)
            index, left, top = glyph_ids[gid]
            x = round((cursor + pos.x_offset) / 64) + left
            y = -round(pos.y_offset / 64) - top
            steps.append((index, x, y))
            cursor += pos.x_advance
        shift = -min(0, min(x for _, x, _ in steps))
        advance = max(round(cursor / 64) + shift, max(x + shift + glyphs[g][1] for g, x, _ in steps))
        plan = bytearray([advance, len(steps)])
        for g, x, y in steps:
            assert g < 256 and 0 <= x + shift < 256 and -128 <= y <= 127
            plan.extend([g, x + shift, y & 255])
            min_y = min(min_y, y)
            max_y = max(max_y, y + glyphs[g][2])
        packed = bytes(plan)
        if packed not in plan_offsets:
            plan_offsets[packed] = len(plans)
            plans.extend(packed)
        records.append((key(text), plan_offsets[packed]))

    def array(name, data):
        return f"static constexpr uint8_t {name}[] = {{\n" + "\n".join(
            "  " + ",".join(f"0x{v:02x}" for v in data[i:i + 24]) + "," for i in range(0, len(data), 24)) + "\n};\n"
    output = "// Generated by scripts/build_dashboard_thai.py; do not edit.\n// Noto Sans Thai: SIL OFL 1.1; see OFL.txt.\n"
    output += "static constexpr Entry ENTRIES[] = {\n" + "\n".join(f"  {{{k}, {p}}}," for k, p in records) + "\n};\n"
    output += "static constexpr Glyph GLYPHS[] = {\n" + "\n".join(f"  {{{o}, {w}, {h}}}," for o, w, h in glyphs) + "\n};\n"
    output += array("PLANS", plans) + array("PIXELS", pixels)
    (OUT / "Atlas.inc").write_text(output, encoding="utf-8", newline="\n")
    (OUT / "OFL.txt").write_bytes((SOURCE / "OFL.txt").read_bytes())
    print(f"{len(records)} clusters, {len(glyphs)} glyphs, "
          f"{len(records)*8 + len(glyphs)*8 + len(plans) + len(pixels)} flash bytes; vertical ink {min_y}..{max_y}")


if __name__ == "__main__":
    main()
