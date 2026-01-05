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

#include <sys/time.h>

#include "main.c"

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

#ifdef _WIN32
    if (argc == 3 && !strcmp(argv[1], "--path")) {
        chdir(argv[2]);
    } else {
        char cmd[3 * KiB] = {0};
        ssize cmd_len = 0;

        cmd[cmd_len++] = '"';
        GetModuleFileNameA(NULL, &cmd[cmd_len], arrlen(cmd) - cmd_len);
        cmd_len = strlen(cmd);
        cmd[cmd_len++] = '"';

        cmd[cmd_len++] = ' ';
        cmd[cmd_len++] = '-';
        cmd[cmd_len++] = '-';
        cmd[cmd_len++] = 'p';
        cmd[cmd_len++] = 'a';
        cmd[cmd_len++] = 't';
        cmd[cmd_len++] = 'h';
        cmd[cmd_len++] = ' ';

        cmd[cmd_len++] = '"';
        getcwd(&cmd[cmd_len], arrlen(cmd) - cmd_len);
        cmd_len = strlen(cmd);
        cmd[cmd_len++] = '"';

        HKEY key = NULL;
        RegCreateKeyA(HKEY_CURRENT_USER, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &key);
        RegSetValueExA(key, APP_NAME, 0, REG_SZ , cmd, strlen(cmd) + 1);
        RegCloseKey(key);
    }
#endif

    Monitors monitors = {0};
    collect_monitors(&monitors);

    Win win = {0};
    new_win(&win, "...", 500, 500);
    make_win_bg(&win, monitors.buf[0], true);
    show_win(&win);

    show_sys_tray_icon(&win, ICON_ID, APP_NAME);

    char *menu_items[] = { "Skip image", "Quit", };
    win.menu_items.buf = menu_items;
    win.menu_items.len = arrlen(menu_items);

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

        struct timeval time_val = {0};
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
                    c == CLICK_R_UP) show_sys_tray_menu(&win);
            } break;
            case EVENT_SYS_TRAY_MENU: {
                unsigned int id = event.menu_item_id - 1;
                if (id < win.menu_items.len) {
                    char *item = win.menu_items.buf[id];
                    if (!strcmp(item, "Quit")) goto end;
                }
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
