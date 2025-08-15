#define DS_IMPL
#include "ds.h"

#include <time.h>

#define err(...) do { \
  fprintf(stderr, "Error: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  exit(1); \
} while (0);

#include <math.h>
#include <time.h>

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

    FFont font = {
        .path = "./resources/Mallory/Mallory/Mallory Medium.ttf",
        .pt = 50,
    };
    load_font(&font, win.dpi_x, win.dpi_y);

    Image wallpaper = new_img(img);

    connect_img_to_win(&win, wallpaper.packed, wallpaper.w, wallpaper.h);

    s8 time_str = s8_copy(&perm, s8("00:00:00 PM"));

    while (1) {
        WinEvent e = next_event_timeout(&win, 1000);
        if (e.type == EVENT_TIMEOUT || e.e.type == Expose) {
            time_t currentTime = time(NULL);
            struct tm *localTime = localtime(&currentTime);

            sprintf((char *) time_str.buf, "%02d:%02d:%02d",  localTime->tm_hour, localTime->tm_min, localTime->tm_sec);

            place_img(wallpaper, img, 0, 0);
            draw_text(wallpaper, font, time_str, 0.1, 0.25, 1.0, 1.0, 1.0);
            draw_to_win(win);
        }
    }

    close_win(win);

    return 0;
}
