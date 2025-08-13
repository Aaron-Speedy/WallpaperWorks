#include <X11/Xlib.h>

#define DS_IMPL
#include "ds.h"

#define GRAPHICS_IMPL
#include "graphics.h"

#include "libwebp/src/webp/decode.h"

typedef struct {
    u32 *buf;
    int w, h;
} Image;

int main() {
    Arena perm = new_arena(1 * GiB);
    Arena scratch = new_arena(1 * KiB);

    s8 data = read_file(&perm, scratch, s8("30.webp"));
    printf("%ld\n", data.len);

    Image img = {0};
    WebPGetInfo(data.buf, data.len, &img.w, &img.h);
    printf("%dx%d\n", img.w, img.h);

    img.buf = malloc(sizeof(u32) * img.w * img.h);
    {
        u8 *buf = WebPDecodeRGB(data.buf, data.len, &img.w, &img.h);
        for (int i = 0; i < img.w * img.h; i++) {
            img.buf[i] = buf[i * 3 + 2] | (buf[i * 3 + 1] << 8) | (buf[i * 3] << 16);
        }
        WebPFree(buf);
    }

    Win win = get_root_win();

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
