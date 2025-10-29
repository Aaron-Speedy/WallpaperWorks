#define err(...) do { \
  fprintf(stderr, "Error: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  *((int *) 0) = 0; \
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
#include "graphics.h"

#define IMAGE_IMPL
#include "image.h"

Color color(u8 r, u8 g, u8 b, u8 a) {
    return (Color) {
        .c[COLOR_R] = r,
        .c[COLOR_G] = g,
        .c[COLOR_B] = b,
        .c[COLOR_A] = a,
    };
}

typedef struct {
    Image *screen;
    int dpi;
    void *data;
} Context;

#include "main.c"

int main(void) {
    Monitors monitors = {0};
    collect_monitors(&monitors);

    Win win = {0};
    new_win(&win, "...", 500, 500);
    make_win_bg(&win, monitors.buf[0], true);
    show_win(&win);

    Context context = {0};

    {

        Image screen = { .buf = win.buf, .alloc_w = win.w, .w = win.w, .h = win.h, };
        context.screen = &screen;
        context.dpi = win.dpi_x; // assert(win.dpi_x == win.dpi_y);

        start(&context);
    }

    while (true) {
        Image screen = { .buf = win.buf, .alloc_w = win.w, .w = win.w, .h = win.h, };
        context.screen = &screen;
        context.dpi = win.dpi_x; // assert(win.dpi_x == win.dpi_y);

        app_loop(&context);

        struct timeval time_val;
        gettimeofday(&time_val, NULL);

        get_events_timeout(
            &win,
            1000 - (time_val.tv_usec / 1000)
        );

        for (int i = 0; i < win.event_queue_len; i++) {
            WinEvent event = win.event_queue[i];
            switch (event.type) {
            case EVENT_QUIT: goto end;
            case EVENT_SYS_TRAY: {
                int c = event.click;
                if (c == CLICK_L_UP ||
                    c == CLICK_M_UP ||
                    c == CLICK_R_UP) goto end;
            } break;
            default: break;
            }
        }

        win.event_queue_len = 0;

        draw_to_win(&win);
    }
end:
    // kill_sys_tray_icon(&win, ICON_ID);
    close_win(&win);
}
