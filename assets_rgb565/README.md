RAW ASSETS (FAST PATH)
=====================

icons_rgb565/*
- Little-endian RGB565, flattened over black (alpha removed).
- Use when you want max speed and don't need alpha blending.

fonts_a8/sheets/*
- 8-bit alpha masks for the font sheets.
- Pair with the JSON metadata to locate glyphs.
- Tint at draw-time with RGB565 colors.
