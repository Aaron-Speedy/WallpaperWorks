#define DS_IMPL
#include "ds.h"

// TODO: Put this into a common library
#define err(...) do { \
  fprintf(stderr, "Error: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  exit(1); \
} while (0);

#include <math.h>

#include <X11/Xlib.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "third_party/libwebp/src/webp/decode.h"

#define GRAPHICS_IMPL
#include "graphics.h"

#define IMAGE_IMPL
#include "image.h"

int main() {
    Arena perm = new_arena(1 * GiB);
    Arena scratch = new_arena(1 * KiB);

    s8 data = read_file(&perm, scratch, s8("./308.webp"));
    printf("%ld\n", data.len);

    Image img = {0};
    WebPGetInfo(data.buf, data.len, &img.w, &img.h);
    printf("%dx%d\n", img.w, img.h);

    img = new_img(img);
    {
        u8 *buf = WebPDecodeRGB(data.buf, data.len, &img.w, &img.h);
        for (int i = 0; i < img.w * img.h; i++) {
            img.buf[i].c[0] = buf[i * 3 + 0];
            img.buf[i].c[1] = buf[i * 3 + 1];
            img.buf[i].c[2] = buf[i * 3 + 2];
            pack_color(&img, i);
        }
        WebPFree(buf);
    }

    Win win = get_root_win();

    img = rescale_img(img, win.w, win.h);

    s8 text = s8("10:10 PM");
    s8 font_path = s8("./resources/Mallory/Mallory/Mallory Medium.ttf");

    connect_img_to_win(&win, img.packed, img.w, img.h);

    FT_Library lib;
    FT_Face face;
    if (FT_Init_FreeType(&lib)) err("Failed to initialize FreeType.");
    if (FT_New_Face(lib, font_path.buf, 0, &face)) err("Failed to create FreeType font face.");
    if (FT_Set_Char_Size(
        face,
        0, 60*64 /* 1/64 pt*/,
        win.dpi_x, win.dpi_y)) {
        err("Failed to set character size on font.");
    }

    FT_GlyphSlot slot = face->glyph;
    int pen_x = 100, pen_y = 100;

    for (int i = 0; i < text.len; i++) {
        // Ignore errors
        if (FT_Load_Char(face, text.buf[i], FT_LOAD_RENDER)) continue;

        for (int x = 0; x < slot->bitmap.width; x++) {
            for (int y = 0; y < slot->bitmap.rows; y++) {
                u8 v = slot->bitmap.buffer[x + y * slot->bitmap.width];
                if (!v) continue;
                int i = (x + pen_x + slot->bitmap_left) + (y + pen_y - slot->bitmap_top) * img.w;
                img.buf[i].c[0] = v;
                img.buf[i].c[1] = v;
                img.buf[i].c[2] = v;
                pack_color(&img, i);
            }
        }

        pen_x += slot->advance.x >> 6;
    }

    while (1) {
        XEvent e;
        XNextEvent(win.display, &e);
        if (e.type == Expose) {
            draw_to_win(win);
        }
    }

    close_win(win);

    return 0;
}
