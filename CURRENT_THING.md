# Current Task: TTF Font Support

**Goal**: Add TrueType font rendering to VibeOS so the browser (and other apps) can render text at different sizes for headings, bold, italic, etc.

## Status: COMPLETE

### What was done
- [x] Created `kernel/libc/math.h` with ARM64 hardware-accelerated math functions (sqrt, fabs, floor, ceil via FPU instructions, plus Taylor series for cos/sin/acos, and implementations for pow/exp/log)
- [x] Created `kernel/ttf.h` and `kernel/ttf.c` - wrapper around stb_truetype
- [x] TTF loads from `/fonts/Roboto/Roboto-Regular.ttf` at boot
- [x] Added TTF functions to kapi: `ttf_get_glyph`, `ttf_get_advance`, `ttf_get_kerning`, `ttf_get_metrics`, `ttf_is_ready`
- [x] Added `ttf_glyph_t` struct and TTF constants to `user/lib/vibe.h`
- [x] Added `gfx_draw_ttf_glyph()` and `gfx_draw_ttf_string()` to `user/lib/gfx.h`
- [x] Updated browser to use TTF fonts with different sizes for h1-h6 headings
- [x] Makefile copies `/fonts/` directory to disk image
- [x] Fixed bitmap stride bug for normal-style glyphs

### Font sizes in browser
- H1: 28px bold
- H2: 24px bold
- H3: 20px bold
- H4: 18px bold
- Body: 16px normal
- Bold text: adds TTF_STYLE_BOLD flag
- Italic text: adds TTF_STYLE_ITALIC flag

### Technical notes
- stb_truetype renders glyphs on demand, cached per size/style
- 4 size caches: 12, 16, 24, 32px (browser uses closest match)
- Bold is synthesized by drawing glyph twice offset by 1px
- Italic is synthesized by shearing the bitmap ~12 degrees
- Grayscale antialiasing with alpha blending in gfx_draw_ttf_glyph

### Files changed/created
- `kernel/libc/math.h` - NEW
- `kernel/ttf.h` - NEW
- `kernel/ttf.c` - NEW
- `kernel/kernel.c` - Added ttf_init() call
- `kernel/kapi.h` - Added TTF function pointers
- `kernel/kapi.c` - Wired up TTF functions
- `user/lib/vibe.h` - Added ttf_glyph_t and TTF constants
- `user/lib/gfx.h` - Added gfx_draw_ttf_glyph/gfx_draw_ttf_string
- `user/bin/browser/main.c` - Uses TTF for sized/styled text
- `Makefile` - Copies fonts/ to disk

### Next steps (future sessions)
- Improve word wrapping to account for variable-width TTF glyphs
- Add more font sizes to cache if needed
- Consider loading multiple font files (bold.ttf, italic.ttf) for better quality
