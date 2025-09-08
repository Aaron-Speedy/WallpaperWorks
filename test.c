#include <stdio.h>
#include <curl/curl.h>

#define err(...) do { \
    fprintf(stderr, "Error: "); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
    exit(1); \
} while (0);

#define warning(...) do { \
    fprintf(stderr, "Warning: "); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
} while (0);

#define print(format, ...) do { \
    char buf[1024]; \
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, format, __VA_ARGS__); \
    MessageBox(NULL, buf, "Debug Message", MB_OK); \
} while (0);

#define GRAPHICS_IMPL
#include "src/graphics.h"

#define FONT_IMPL
#include "src/font.h"

#define DS_IMPL
#include "src/ds.h"

#define NETWORKING_IMPL
#include "src/networking.h"

int main() {
    Arena perm = new_arena(1 * GiB);

    CURL *curl = curl_easy_init(); 
    if (!curl) err("Failed to initialize libcurl.");

    curl_easy_setopt(curl, CURLOPT_CAINFO, "./curl-ca-bundle.crt");

    s8 r = download(&perm, curl, s8("https://infotoast.org/images/num.txt"));
    print("%ld\n", r.len);

    Win win = {0};
    get_bg_win(&win);

    FFont font = {
        .path = "./resources/Mallory/Mallory Medium.ttf",
        .pt = 30,
    };
    load_font(&font, win.dpi_x, win.dpi_y);

    Color color = { .c[COLOR_B] = 255, };

    while (1) {
        Image screen = { .buf = win.buf, .w = win.w, .h = win.h, .alloc_w = win.w, };

        for (int i = 0; i < screen.w * screen.h; i++) {
            screen.buf[i] = (i % 100 >= 50) ? color : (Color) {0};
        }


        draw_text(
            screen,
            &font,
            s8("Ah! Like gold fall the leaves of the wind."),
            screen.w * 0.3,
            screen.h * 0.5,
            255, 255, 255
        );

        draw_to_win(win);

        next_event_timeout(&win, 1000);
        switch (win.event) {
            case NO_EVENT: break;
            case EVENT_QUIT: goto end;
            case EVENT_TIMEOUT: color = (Color) { .c[COLOR_R] = 255, }; break;
            default: assert(!"Unhandled event");
        }
    }

    end:
    return 0;
}
