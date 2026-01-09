// TODO: Optimize this mess

#ifndef FONT_H
#define FONT_H

#include <ft2build.h>
#include FT_FREETYPE_H

#include "image.h"

typedef struct {
    FT_Library lib;
} FFontLib;

typedef struct {
    FT_Face face;
} FFont;

FFontLib init_ffont();
void load_font(FFont *f, FFontLib lib, u8 *raw, int raw_len);
void free_font(FFont f);

// oy specifies the bottom of the text. Returns the bounding box.
Image draw_text_or_get_bound(Image img, FFont *f,
                             s8 s,
                             int ox, int oy,
                             u8 r, u8 g, u8 b,
                             bool draw, bool mix);
Image get_bound_of_text(FFont *f, s8 s); // See above comment
Image draw_text(Image img, FFont *f, // See above comment
                s8 s,
                int ox, int oy,
                u8 r, u8 g, u8 b,
                bool mix);

Image draw_text_or_get_bound_shadow(Image img, FFont *f,
                             s8 s,
                             int ox, int oy,
                             u8 r, u8 g, u8 b,
                             int shadow_x, int shadow_y,
                             u8 shadow_r, u8 shadow_g, u8 shadow_b,
                             bool draw, bool mix);
Image get_bound_of_text_shadow(FFont *f, s8 s, int shadow_x, int shadow_y);
Image draw_text_shadow(Image img, FFont *f,
                             s8 s,
                             int ox, int oy,
                             u8 r, u8 g, u8 b,
                             int shadow_x, int shadow_y,
                             u8 shadow_r, u8 shadow_g, u8 shadow_b,
                             bool mix);

#endif // FONT_H

#ifdef FONT_IMPL
#ifndef FONT_IMPL_GUARD
#define FONT_IMPL_GUARD

#define IMAGE_IMPL
#include "image.h"

FFontLib init_ffont() {
    FFontLib ret = {0};
    if (FT_Init_FreeType(&ret.lib)) err("Failed to initialize FreeType.");
    return ret;
}

void load_font(FFont *f, FFontLib lib, u8 *raw, int raw_len) {
    FT_Error err = 0;
    if (err = FT_New_Memory_Face(lib.lib, raw, raw_len, 0, &f->face)) {
        err("Failed to create FreeType font face. Failed with error \"%s\"",
            FT_Error_String(err));
    }
}

void free_font(FFont f) {
    if (FT_Done_Face(f.face)) err("Failed to free font");
}

Image draw_text_or_get_bound(Image img, FFont *f,
                             s8 s,
                             int ox, int oy,
                             u8 r, u8 g, u8 b,
                             bool draw, bool mix) {
    FT_GlyphSlot slot = f->face->glyph;
    int pen_x = ox, pen_y = oy;

    int max_x = 0, max_y = 0, min_x = img.w, min_y = img.h;

    for (int i = 0; i < s.len; i++) {
        // Ignore errors. TODO: Check about caching
        if (FT_Load_Char(f->face, s.buf[i], FT_LOAD_RENDER)) continue;

        for (u64 x = 0; x < slot->bitmap.width; x++) {
            for (u64 y = 0; y < slot->bitmap.rows; y++) {
                u8 alpha = slot->bitmap.buffer[x + y * slot->bitmap.width];
                if (!alpha) continue;

                int nx = x + pen_x + slot->bitmap_left,
                    ny = (y + pen_y - slot->bitmap_top);

                max_x = nx >= max_x ? nx : max_x;
                max_y = ny >= max_y ? ny : max_y;
                min_x = nx <= min_x ? nx : min_x;
                min_y = ny <= min_y ? ny : min_y;

                if (draw) {
                    Color *c1 = img_at(&img, nx, ny);
                    Color c2 = {
                        .c[COLOR_R] = r,
                        .c[COLOR_G] = g,
                        .c[COLOR_B] = b,
                        .c[COLOR_A] = alpha,
                    };
                    *c1 = mix ? mix_colors(*c1, c2) : c2;
                }
            }
        }

        pen_x += slot->advance.x >> 6;
    }

    return (Image) {
        .buf = img.buf,
        .alloc_w = img.alloc_w,
        .x = min_x,
        .y = min_y,
        .w = max_x - min_x + 1,
        .h = max_y - min_y + 1,
    };
}

Image get_bound_of_text(FFont *f, s8 s) {
    return draw_text_or_get_bound((Image) {0}, f, s, 0, 0, 0, 0, 0, 0, 0);
}

Image draw_text(Image img, FFont *f,
                s8 s,
                int ox, int oy,
                u8 r, u8 g, u8 b,
                bool mix) {
    return draw_text_or_get_bound(img, f, s, ox, oy, r, g, b, true, mix);
}

Image draw_text_or_get_bound_shadow(Image img, FFont *f,
                             s8 s,
                             int ox, int oy,
                             u8 r, u8 g, u8 b,
                             int shadow_x, int shadow_y,
                             u8 shadow_r, u8 shadow_g, u8 shadow_b,
                             bool draw, bool mix) {
    Image shadow = draw_text_or_get_bound(
        img, f, s,
        ox + shadow_x, oy + shadow_y,
        shadow_r, shadow_g, shadow_b,
        draw, mix
    );
    Image plain = draw_text_or_get_bound(
        img, f, s,
        ox, oy,
        r, g, b,
        draw, mix
    );
    return combine_bound(shadow, plain);
}

Image get_bound_of_text_shadow(FFont *f, s8 s, int shadow_x, int shadow_y) {
    return draw_text_or_get_bound_shadow(
        (Image) {0}, f, s,
        0, 0, 0, 0, 0,
        shadow_x, shadow_y, 0, 0, 0,
        false, false
    );
}

Image draw_text_shadow(Image img, FFont *f,
                             s8 s,
                             int ox, int oy,
                             u8 r, u8 g, u8 b,
                             int shadow_x, int shadow_y,
                             u8 shadow_r, u8 shadow_g, u8 shadow_b,
                             bool mix) {
    return draw_text_or_get_bound_shadow(
        img, f, s,
        ox, oy,
        r, g, b,
        shadow_x, shadow_y,
        shadow_r, shadow_g, shadow_b,
        true, mix
    );
}

#endif // FONT_IMPL_GUARD
#endif // FONT_IMPL
