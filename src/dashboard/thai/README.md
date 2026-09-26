# Thai dashboard text

The dashboard uses Noto Sans Thai for Thai project names, task titles and details.
Menus stay in English. The browser uses its native Thai fonts.

`Atlas.inc` is generated from the [Google Fonts Noto Sans Thai snapshot](https://github.com/google/fonts/tree/8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5/ofl/notosansthai),
licensed under SIL OFL 1.1 (included as `OFL.txt`). It contains 113 glyph bitmaps and
12,829 pre-shaped cluster entries occupying 238,857 bytes of read-only flash.
No shaping engine, font file, glyph cache or heap allocation is needed at runtime.

HarfBuzz shapes base letters with up to two combining marks, plus the special
sara-am/tone ordering. Runtime matching keeps leading vowels, base/mark groups
and trailing spacing vowels together across lines/pages. Precomposed sara am and
its decomposed nikhahit/sara-aa spelling are supported. Thai digits and punctuation
are included. Bold uses a one-pixel stroke expansion; all three text sizes scale
the same 32-pixel font atlas. Exceptional tall mark stacks scale to the line box.
Thai line breaking is cluster-based, without a dictionary word segmenter. This is
dashboard text support; it does not change EPUB fonts or add Thai menu translations.

Regenerate explicitly; normal firmware builds use the checked-in data:

```sh
python -m pip install Pillow==12.3.0 freetype-py==2.5.1 fonttools==4.66.0 uharfbuzz==0.56.2
python scripts/build_dashboard_thai.py --download
```

The generator verifies source SHA-256 hashes, applies Noto Sans Thai's OpenType
substitution/positioning at weight 400 and width 100, then emits glyph positions
and deduplicated one-bit artwork. Generated files must not be edited manually.
Input fonts are cached in ignored `build/thai-source/`.

Verification: run `test/dashboard/run.py`, inspect the mixed Thai/emoji previews
at all three sizes, and test `ประชุมทีม`, `ซื้อของ`, `อ่านหนังสือ`, `น้ำ`, `ปู่`,
`ญู`, `ฐุ`, `เก้า` and mixed English/Thai task text on the X3. Check selected-row
inversion, upper/lower marks, line/page boundaries and repeated refresh heap logs.
