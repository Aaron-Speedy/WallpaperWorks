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

typedef struct {
    Monitors monitors;
    Win wins[MAX_PLATFORM_MONITORS];
    FFont fonts[MAX_PLATFORM_MONITORS];
    FFontLib font_lib;
    char *font_path;
    int font_pt;
    HWND worker_w;
} Wins;

void replace_wins(Wins *wins, Monitors *m) {
    for (int i = 0; i < wins->monitors.len; i++) {
        Win *w = &wins->wins[i];
        w->is_bg = false; // to make sure the wallpaper isn't cleared
        free_font(wins->fonts[i]);
        close_win(w);
    }

    wins->monitors.len = m->len;
    for (int i = 0; i < wins->monitors.len; i++) {
        Win *w = &wins->wins[i];
        wins->monitors.buf[i] = m->buf[i];
        load_font(
            &wins->fonts[i],
            wins->font_lib,
            wins->font_path,
            wins->font_pt,
            w->dpi_x, w->dpi_y
        );
        new_win(w, 500, 500);
        make_win_bg(w, wins->worker_w);
        move_win_to_monitor(w, m->buf[i]);
        _fill_working_area(w);
        show_win(*w);
    }
}

int main() {
    // Arena perm = new_arena(1 * GiB);

    // Monitors m = {0};
    // collect_monitors(&m);

    // print("%d", m.len);

    Wins wins = {
        .worker_w = _make_worker_w(),
        .font_lib = init_ffont(),
        .font_path = "./font.ttf",
        .font_pt = 70,
    };

    {
        Monitors m;
        collect_monitors(&m);
        replace_wins(&wins, &m);
    }

    // show_sys_tray_icon(&wins.wins[0], ICON_ID, "Stop WallpaperWorks");

    Color colors[2] = {
        { .c[COLOR_G] = 0, },
        { .c[COLOR_R] = 0, },
    };

    while (1) {
        for (int i = 0; i < wins.monitors.len; i++) {
            Win *win = &wins.wins[i];
            FFont *font = &wins.fonts[i];

            Image screen = {
                .buf = win->buf,
                .w = win->w, .h = win->h,
                .alloc_w = win->w,
            };

            for (int j = 0; j < screen.w * screen.h; j++) {
                screen.buf[j] = colors[i];
            }

            // draw_text(
            //     screen,
            //     font,
            //     s8("Ah! Like fools, men scream hatred."),
            //     screen.w * 0.3,
            //     screen.h * 0.5,
            //     255, 255, 255
            // );

            draw_to_win(*win);

            wait_event_timeout(
                win,
                1000
            );
            while (get_next_event(win));
            switch (win->event.type) {
            case EVENT_QUIT: goto end;
            // case EVENT_TIMEOUT: color = (Color) { .c[COLOR_R] = 255, }; break;
            default: break;
            }
        }

        Monitors monitors = {0};
        collect_monitors(&monitors);
        if (did_monitors_change(&wins.monitors, &monitors)) {
            replace_wins(&wins, &monitors);
        }
    }

end:
    // close_win(&win); // TODO
    return 0;
}
