#include <X11/Xlib.h>

#define DS_IMPL
#include "ds.h"

#define GRAPHICS_IMPL
#include "graphics.h"

#include "libwebp/src/webp/decode.h"

int main() {
    Arena perm = new_arena(1 * GiB);
    Arena scratch = new_arena(1 * KiB);

    s8 data = read_file(&perm, scratch, s8("150.webp"));
    printf("%ld\n", data.len);

    int width, height;
    WebPGetInfo(data.buf, data.len, &width, &height);
    printf("%dx%d\n", width, height);

    u32 *img = malloc(3 * width * height);
    {
        u8 *pixels = WebPDecodeRGB(data.buf, data.len, &width, &height);
        for (int i = 0; i < width * height; i++) {
            img[i] = pixels[i * 3 + 2] | (pixels[i * 3 + 1] << 8) | (pixels[i * 3] << 16);
        }
        WebPFree(pixels);
    }

    Win win = get_root_win();

    // Enable image mode
    connect_img_to_win(&win, img, width, height);

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
