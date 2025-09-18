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

#define GRAPHICS_IMPL
#include "src/graphics.h"

#define FONT_IMPL
#include "src/font.h"

#define DS_IMPL
#include "src/ds.h"

#define NETWORKING_IMPL
#include "src/networking.h"

int main() {
    // Arena perm = new_arena(1 * GiB);

    Win win = {0};
    new_win(&win, "Test Window", 400, 200);
    make_win_bg(&win);

    while (1) {
        // FFont *font = &wins.fonts[i];

        Image screen = {
            .buf = win.buf,
            .w = win.w, .h = win.h,
            .alloc_w = win.w,
        };

        for (int j = 0; j < screen.w * screen.h; j++) {
            screen.buf[j] = (Color) { .c[COLOR_R] = 255, };
        }

        // draw_text(
        //     screen,
        //     font,
        //     s8("Ah! Like fools, men scream hatred."),
        //     screen.w * 0.3,
        //     screen.h * 0.5,
        //     255, 255, 255
        // );

        draw_to_win(&win);

        get_events_timeout(&win, 1000);

        for (int i = 0; i < win.event_queue_len; i++) {
            WinEvent event = win.event_queue[i];
            switch (event.type) {
            case EVENT_QUIT: goto end;
            default: break;
            }
        }

        win.event_queue_len = 0;
    }

end:
    // close_win(&win); // TODO
    return 0;
}
