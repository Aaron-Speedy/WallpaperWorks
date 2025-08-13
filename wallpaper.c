#include <X11/Xlib.h>
#include <math.h>

#define DS_IMPL
#include "ds.h"

#define GRAPHICS_IMPL
#include "graphics.h"

#include "libwebp/src/webp/decode.h"

typedef struct {
    u32 *buf; // 0xRRGGBB
    int w, h;
} Image;

typedef enum {
    DIR_H,
    DIR_V,
} Dir;

Image rescale_axis(Image img, Dir dir, int new_val) {
    Image ret = {0};
    int old_val = 0;

    if (dir == DIR_H) {
        ret.w = new_val;
        ret.h = img.h;
        old_val = img.w;
    } else {
        ret.w = img.w;
        ret.h = new_val;
        old_val = img.h;
    }

    ret.buf = malloc(sizeof(img.buf[0]) * ret.w * ret.h);

    float scale = (float) new_val / old_val;

    // Linear interpolation
    // TODO: Do average area for downscaling
    // if (scale > 1) {
        for (int x = 0; x < ret.w; x++) {
            for (int y = 0; y < ret.h; y++) {
                float x1 = x, y1 = y, x2 = x, y2 = y, d = 0;
                if (dir == DIR_H) {
                    float a = x / scale;
                    x1 = floor(a);
                    x2 = ceil(a);
                    d = a - x1;
                } else {
                    float a = y / scale;
                    y1 = floor(a);
                    y2 = ceil(a);
                    d = a - y1;
                }
                for (int i = 0; i < 3; i++) {
                    /* The fancy bit manipulation stuff gets the color channels
                     * from the u32 */
                    float v1 = (img.buf[(int) (x1 + y1 * img.w)] >> (i * 8)) & 0xFF,
                          v2 = (img.buf[(int) (x2 + y2 * img.w)] >> (i * 8)) & 0xFF;
                    ret.buf[x + y * ret.w] |= ((
                        (int) (v1 * (1.0 - d) + v2 * d)
                    ) << (i * 8));
                }
            }
        }
    // }

    return ret;
}

int main() {
    Arena perm = new_arena(1 * GiB);
    Arena scratch = new_arena(1 * KiB);

    s8 data = read_file(&perm, scratch, s8("150.webp"));
    printf("%ld\n", data.len);

    Image img = {0};
    WebPGetInfo(data.buf, data.len, &img.w, &img.h);
    printf("%dx%d\n", img.w, img.h);

    img.buf = malloc(sizeof(img.buf[0]) * img.w * img.h);
    {
        u8 *buf = WebPDecodeRGB(data.buf, data.len, &img.w, &img.h);
        for (int i = 0; i < img.w * img.h; i++) {
            img.buf[i] = buf[i * 3 + 2] | (buf[i * 3 + 1] << 8) | (buf[i * 3] << 16);
        }
        WebPFree(buf);
    }

    Win win = get_root_win();

    img = rescale_axis(
        rescale_axis(img, DIR_H, XDisplayWidth(win.display, win.screen)),
        DIR_V, XDisplayHeight(win.display, win.screen)
    );

    connect_img_to_win(&win, img.buf, img.w, img.h);

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
