#include <X11/Xlib.h>
#include <math.h>

#define DS_IMPL
#include "ds.h"

#define GRAPHICS_IMPL
#include "graphics.h"

#include "libwebp/src/webp/decode.h"

typedef struct {
    u8 c[3]; // RGB
    u32 packed; // 0xRRGGBB. Should be set to the colors in c.
} Color;

typedef struct {
    Color *buf;
    u32 *packed;
    int w, h;
} Image;

typedef enum {
    DIR_H,
    DIR_V,
} Dir;

void pack_color(Image *m, int i) {
    m->packed[i] = (m->buf[i].c[2]) |
                   (m->buf[i].c[1] << 8) |
                   (m->buf[i].c[0] << 16);
}

Image new_img(Image m) {
    m.buf = malloc(sizeof(m.buf[0]) * m.w * m.h);
    m.packed = malloc(sizeof(m.packed[0]) * m.w * m.h);
    return m;
}

// Bilinear interpolation
Image rescale_img(Image img, int new_w, int new_h) {
    Image ret = { .w = new_w, .h = new_h, };
    ret = new_img(ret);

    float scale_w = (float) ret.w / img.w,
          scale_h = (float) ret.h / img.h;

    for (int x = 0; x < ret.w; x++) {
        for (int y = 0; y < ret.h; y++) {
            float a = 0;

            a = x / scale_w;
            float x0 = floor(a),
                  x1 = ceil(a),
                  dx = a - x0;

            a = y / scale_h;
            float y0 = floor(a),
                  y1 = ceil(a),
                  dy = a - y0;

            int ret_i = x + y * ret.w;
            for (int i = 0; i < 3; i++) {
                float v00 = img.buf[(int) (x0 + y0 * img.w)].c[i],
                      v01 = img.buf[(int) (x0 + y1 * img.w)].c[i],
                      v10 = img.buf[(int) (x1 + y0 * img.w)].c[i],
                      v11 = img.buf[(int) (x1 + y1 * img.w)].c[i],
                      vt  = v00 * (1.0 - dx) + v10 * dx,
                      vb  = v01 * (1.0 - dx) + v11 * dx;
                ret.buf[ret_i].c[i] = vt * (1.0 - dy) + vb * dy;
            }
            pack_color(&ret, ret_i);
        }
    }

    return ret;
}

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

    img = rescale_img(
        img,
        XDisplayWidth(win.display, win.screen),
        XDisplayHeight(win.display, win.screen)
    );

    connect_img_to_win(&win, img.packed, img.w, img.h);

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
