// TODO: Optimize this mess
#ifndef FONT_H
#define FONT_H

#include <ft2build.h>
#include FT_FREETYPE_H

#include "image.h"

typedef struct {
    char *path; // TODO: Bake font into executable.
    float pt;
    FT_Library lib;
    FT_Face face;
} FFont;

void load_font(FFont *f, int dpi_x, int dpi_y);
// 0 <= (r, g, b) <= 1. Returns the bounding box.
Image draw_text(Image img, FFont *f,
               s8 s,
               float o_x, float o_y,
               u8 r, u8 g, u8 b);

#endif // FONT_H

#ifdef FONT_IMPL
#ifndef FONT_IMPL_GUARD
#define FONT_IMPL_GUARD

void load_font(FFont *f, int dpi_x, int dpi_y) {
    if (FT_Init_FreeType(&f->lib)) err("Failed to initialize FreeType.");
    if (FT_New_Face(f->lib, f->path, 0, &f->face)) err("Failed to create FreeType font face.");
    if (FT_Set_Char_Size(f->face, 0, f->pt * 64.0, dpi_x, dpi_y)) {
        err("Failed to set character size on font.");
    }
}

Image draw_text(Image img, FFont *f,
               s8 s,
               float o_x, float o_y,
               u8 r, u8 g, u8 b) {
    FT_GlyphSlot slot = f->face->glyph;
    int pen_x = o_x * img.w, pen_y = o_y * img.h;

    int max_x = 0, max_y = 0, min_x = img.w, min_y = img.h;

    for (int i = 0; i < s.len; i++) {
        // Ignore errors
        if (FT_Load_Char(f->face, s.buf[i], FT_LOAD_RENDER)) continue;

        for (u32 x = 0; x < slot->bitmap.width; x++) {
            for (u32 y = 0; y < slot->bitmap.rows; y++) {
                float v = slot->bitmap.buffer[x + y * slot->bitmap.width] / 255.0;
                if (v == 0.0) continue;

                int nx = x + pen_x + slot->bitmap_left,
                    ny = (y + pen_y - slot->bitmap_top);

                max_x = nx >= max_x ? nx : max_x;
                max_y = ny >= max_y ? ny : max_y;
                min_x = nx <= min_x ? nx : min_x;
                min_y = ny <= min_y ? ny : min_y;

                Color *c = img_atb(img, nx, ny);

                c->c[0] = r * v + c->c[0] * (1 - v);
                c->c[1] = g * v + c->c[1] * (1 - v);
                c->c[2] = b * v + c->c[2] * (1 - v);
                pack_color(img, nx, ny);
            }
        }

        pen_x += slot->advance.x >> 6;
    }

    return (Image) {
        .buf = img.buf,
        .packed = img.packed,
        .x = min_x,
        .y = min_y,
        .w = max_x - min_x + 1,
        .h = max_y - min_y + 1,
        .alloc_w = img.alloc_w,
    };
}

#endif // FONT_IMPL_GUARD
#endif // FONT_IMPL
