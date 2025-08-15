#define DS_IMPL
#include "ds.h"

#define err(...) do { \
  fprintf(stderr, "Error: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  exit(1); \
} while (0);

#include <math.h>

#include "third_party/libwebp/src/webp/decode.h"

#define GRAPHICS_IMPL
#include "graphics.h"

#define IMAGE_IMPL
#include "image.h"

#define FONT_IMPL
#include "font.h"

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
            pack_color(img, i);
        }
        WebPFree(buf);
    }

    Win win = get_root_win();

    img = rescale_img(img, win.w, win.h);

    connect_img_to_win(&win, img.packed, img.w, img.h);

    FFont font = {
        .path = "./resources/Mallory/Mallory/Mallory Medium.ttf",
        .pt = 50,
    };
    load_font(&font, win.dpi_x, win.dpi_y);

    draw_text(img, font, s8("10:10 PM"), 0.1, 0.25, 1.0, 1.0, 1.0);

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
