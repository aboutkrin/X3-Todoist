# Dashboard monochrome emoji

`Atlas.inc` is generated artwork/data, not a hand-edited header. It contains
32×32 one-bit tiles and a sorted UTF-8 sequence lookup table. The firmware uses
`constexpr` arrays in flash and renders directly through `GfxRenderer::drawPixel`.
No emoji font, shaping engine, decompression buffer or glyph cache is loaded at
runtime. Measurement and wrapping use the same longest-sequence matching as drawing.

Source: [Noto Emoji](https://github.com/googlefonts/noto-emoji), via the
[Google Fonts snapshot](https://github.com/google/fonts/tree/8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5/ofl/notoemoji),
under SIL Open Font License 1.1 (included in `OFL.txt`).
Sequence definitions: [Unicode 17 emoji test data](https://www.unicode.org/Public/17.0.0/emoji/emoji-test.txt),
under the [Unicode License v3](https://www.unicode.org/license.txt), included in `UNICODE-LICENSE.txt`.

The atlas recognizes 5,225 sequences/presentation variants; 5,060 have artwork
in this pinned font. The remaining 165 display a crossed-box placeholder.
Monochrome skin tones and gender variants can share artwork; flags use Noto's
monochrome designs. This is not color emoji or a guarantee of the latest Unicode
artwork. Native browser emoji rendering is unchanged.

Regenerate explicitly (normal firmware builds need no downloads or Python font tools):

```sh
python -m pip install Pillow==12.3.0 freetype-py==2.5.1 uharfbuzz==0.56.2
python scripts/build_dashboard_emoji.py --download
```

The script verifies source SHA-256 hashes, rasterizes at weight 400 using FreeType,
shapes sequences using HarfBuzz, deduplicates identical tiles and writes the atlas.
Inputs, a bitmap proof and unsupported sequences are retained in `build/emoji-source/`.
Commit the generated atlas and licenses with generator changes. Verify the installed
Pillow version when reproducing exact output; the generator's resampling and threshold
affect bitmap bytes.
