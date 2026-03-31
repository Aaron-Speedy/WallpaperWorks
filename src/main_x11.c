#define err(...) do { \
  fprintf(stderr, "Error: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  *((int *) 0) = 0; \
} while (0)

#define warning(...) do { \
  fprintf(stderr, "Warning: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
} while (0)

#define print(format, ...) do { \
    fprintf(stderr, format, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
} while (0)

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>

int usleep(useconds_t usec);

#define DS_IMPL
#include "ds.h"

typedef enum {
    COLOR_B,
    COLOR_G,
    COLOR_R,
    COLOR_A,
} PlatformColorEnum;

typedef struct {
    uint8_t c[4];
} Color;

typedef struct {
    int x, y, w, h;
} PlatformMonitor;

#define MAX_PLATFORM_MONITORS 100
typedef struct {
    PlatformMonitor buf[MAX_PLATFORM_MONITORS];
    int len;
} PlatformMonitors;

typedef struct {
    enum {
        NO_EVENT = 0,
        EVENT_TIMEOUT,
        EVENT_QUIT,
        EVENT_ERR,
        EVENT_SYS_TRAY,
        EVENT_SYS_TRAY_MENU,
        NUM_WIN_EVENTS,
    } type;
    enum {
        NO_CLICK,
        CLICK_L_UP,
        CLICK_L_DOWN,
        CLICK_M_UP,
        CLICK_M_DOWN,
        CLICK_R_UP,
        CLICK_R_DOWN,
        NUM_CLICKS,
    } click;
    unsigned int menu_item_id;
} WinEvent;

#define MAX_EVENT_QUEUE_LEN 30

typedef struct {
    Window win;
    Display *display;
    int screen;
    GC gc;
    XImage *img;
    Pixmap pixmap;
    bool draw_to_img;
    bool is_root;
} PlatformWin;

typedef struct {
    Display *display;
    Window root;
    int screen;
    int root_w, root_h;
    Pixmap root_pixmap;
    Atom xrootpmap_id;
    Atom esetroot_pmap_id;
} X11State;

static X11State x11 = {0};

typedef struct Win {
    int screen, w, h;
    Color *buf;
    bool resized, is_bg;
    WinEvent event_queue[MAX_EVENT_QUEUE_LEN];
    int event_queue_len;
    PlatformWin p;
    u64 hash;
    struct {
        char **buf;
        ssize len;
    } menu_items;
} Win;

#define IMAGE_IMPL
#include "image.h"

static void init_x11(void) {
    if (x11.display) return;

    x11.display = XOpenDisplay(NULL);
    if (!x11.display) err("Failed to open display.");

    x11.screen = DefaultScreen(x11.display);
    x11.root = RootWindow(x11.display, x11.screen);
    x11.xrootpmap_id = XInternAtom(x11.display, "_XROOTPMAP_ID", False);
    x11.esetroot_pmap_id = XInternAtom(x11.display, "ESETROOT_PMAP_ID", False);
}

static void ensure_root_pixmap(void) {
    init_x11();

    XWindowAttributes attrs = {0};
    XGetWindowAttributes(x11.display, x11.root, &attrs);

    if (x11.root_pixmap && x11.root_w == attrs.width && x11.root_h == attrs.height) {
        return;
    }

    if (x11.root_pixmap) {
        XFreePixmap(x11.display, x11.root_pixmap);
        x11.root_pixmap = 0;
    }

    x11.root_w = attrs.width;
    x11.root_h = attrs.height;
    x11.root_pixmap = XCreatePixmap(
        x11.display,
        x11.root,
        x11.root_w,
        x11.root_h,
        attrs.depth
    );

    XSetWindowBackgroundPixmap(x11.display, x11.root, x11.root_pixmap);
    XChangeProperty(
        x11.display,
        x11.root,
        x11.xrootpmap_id,
        XA_PIXMAP,
        32,
        PropModeReplace,
        (unsigned char *) &x11.root_pixmap,
        1
    );
    XChangeProperty(
        x11.display,
        x11.root,
        x11.esetroot_pmap_id,
        XA_PIXMAP,
        32,
        PropModeReplace,
        (unsigned char *) &x11.root_pixmap,
        1
    );
    XClearWindow(x11.display, x11.root);
    XFlush(x11.display);
}

static void resize_win(Win *win) {
    if (!win->p.draw_to_img && win->buf) XDestroyImage(win->p.img);
    else free(win->buf);
    win->buf = 0;

    if (win->p.pixmap) {
        XFreePixmap(win->p.display, win->p.pixmap);
        win->p.pixmap = 0;
    }

    win->resized = true;
    win->buf = calloc(win->w * win->h, sizeof(*win->buf));

    if (!win->p.draw_to_img) {
        win->p.img = XCreateImage(
            win->p.display,
            DefaultVisual(win->p.display, win->p.screen),
            24, ZPixmap, 0, (char *) win->buf, win->w, win->h, 32, 0
        );
        if (!win->p.is_root) {
            win->p.pixmap = XCreatePixmap(
                win->p.display,
                win->p.win,
                win->w,
                win->h,
                DefaultDepth(win->p.display, win->p.screen)
            );
        }
    }
}

static void fill_working_area(Win *win, PlatformMonitor monitor) {
    if (!win->p.draw_to_img && !win->p.is_root) {
        XMoveResizeWindow(
            win->p.display,
            win->p.win,
            monitor.x,
            monitor.y,
            monitor.w,
            monitor.h
        );
    }
    win->w = monitor.w;
    win->h = monitor.h;
    resize_win(win);
}

static void new_win(Win *win, char *name, int w, int h) {
    *win = (Win) {0};

    init_x11();

    win->p.display = x11.display;
    win->p.screen = x11.screen;
    win->p.win = XCreateSimpleWindow(
        win->p.display,
        x11.root,
        0, 0, w, h,
        1, 0, 0
    );
    XSelectInput(win->p.display, win->p.win, ExposureMask | StructureNotifyMask);

    win->p.gc = DefaultGC(win->p.display, win->p.screen);

    win->w = w;
    win->h = h;
    resize_win(win);

    XStoreName(win->p.display, win->p.win, name);

    if (!win->p.img) err("Failed to create window.");
}

static void close_win(Win *win) {
    if (win->p.draw_to_img) return;
    if (!win->p.display) return;

    if (win->buf) {
        XDestroyImage(win->p.img);
        win->buf = 0;
        win->p.img = 0;
    }
    if (win->p.pixmap) {
        XFreePixmap(win->p.display, win->p.pixmap);
        win->p.pixmap = 0;
    }

    if (!win->p.is_root) XDestroyWindow(win->p.display, win->p.win);
    win->p = (PlatformWin) {0};
}

static void make_win_bg(Win *win, PlatformMonitor monitor, bool draw_to_root) {
    win->is_bg = true;

    if (win->p.draw_to_img) return;

    close_win(win);

    init_x11();

    win->p.display = x11.display;
    win->p.screen = x11.screen;

    if (draw_to_root) {
        ensure_root_pixmap();
        win->p.win = x11.root;
        win->p.is_root = true;
    } else {
        win->p.win = XCreateSimpleWindow(
            win->p.display,
            x11.root,
            0, 0, 500, 500,
            1, 0, 0
        );

        Atom wm_type = XInternAtom(win->p.display, "_NET_WM_WINDOW_TYPE", False);
        Atom wm_desktop = XInternAtom(win->p.display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);

        XChangeProperty(
            win->p.display,
            win->p.win,
            wm_type,
            XA_ATOM,
            32,
            PropModeReplace,
            (unsigned char *) &wm_desktop,
            1
        );

        XFlush(win->p.display);
    }

    XSelectInput(win->p.display, win->p.win, ExposureMask | StructureNotifyMask);
    win->p.gc = DefaultGC(win->p.display, win->p.screen);
    fill_working_area(win, monitor);

    if (!win->p.img) err("Failed to make the window the background.");
}

static void show_win(Win *win) {
    if (win->p.draw_to_img) return;
    if (win->p.is_root) return;
    XMapWindow(win->p.display, win->p.win);
}

static void draw_to_win(Win *win, PlatformMonitor *monitor) {
    if (win->p.draw_to_img) return;

    int dest_x = monitor && win->p.is_root ? monitor->x : 0;
    int dest_y = monitor && win->p.is_root ? monitor->y : 0;

    if (win->p.is_root) {
        ensure_root_pixmap();
        XPutImage(
            win->p.display,
            x11.root_pixmap,
            win->p.gc,
            win->p.img,
            0,
            0,
            dest_x,
            dest_y,
            win->p.img->width,
            win->p.img->height
        );
        XClearArea(
            win->p.display,
            win->p.win,
            dest_x,
            dest_y,
            win->w,
            win->h,
            False
        );
        XFlush(win->p.display);
        return;
    }

    XPutImage(
        win->p.display,
        win->p.pixmap,
        win->p.gc,
        win->p.img,
        0,
        0,
        0,
        0,
        win->p.img->width,
        win->p.img->height
    );

    XCopyArea(
        win->p.display,
        win->p.pixmap,
        win->p.win,
        win->p.gc,
        0,
        0,
        win->w,
        win->h,
        dest_x,
        dest_y
    );

    XFlush(win->p.display);
}

static void collect_monitors(PlatformMonitors *monitors) {
    *monitors = (PlatformMonitors) {0};

    init_x11();

    int major = 0;
    int minor = 0;
    if (!XineramaQueryVersion(x11.display, &major, &minor)) {
        err("Xinerama extension is not available.");
    }

    if (!XineramaIsActive(x11.display)) err("Xinerama is not active.");

    XineramaScreenInfo *screens = XineramaQueryScreens(x11.display, &monitors->len);
    if (!screens) err("No screens are connected.");

    for (int i = 0; i < monitors->len; i++) {
        monitors->buf[i] = (PlatformMonitor) {
            .x = screens[i].x_org,
            .y = screens[i].y_org,
            .w = screens[i].width,
            .h = screens[i].height,
        };
    }

    XFree(screens);
}

static bool are_monitors_equal(PlatformMonitor a, PlatformMonitor b) {
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static bool did_monitors_change(PlatformMonitors *a, PlatformMonitors *b) {
    if (a->len != b->len) return true;
    for (int i = 0; i < a->len; i++) {
        if (!are_monitors_equal(a->buf[i], b->buf[i])) return true;
    }
    return false;
}

static void setup_monitor_wins(Win *wins, PlatformMonitors *monitors, char *name);
static void refresh_monitors(Win *wins, PlatformMonitors *monitors, char *name);

static void get_events_timeout(Win *win, PlatformMonitor *monitor, int timeout_ms) {
    if (win->p.draw_to_img) usleep(1000 * timeout_ms);

    fd_set fds = {0};
    struct timeval tv = {0};
    int x11_fd = ConnectionNumber(win->p.display);

    FD_ZERO(&fds);
    FD_SET(x11_fd, &fds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int select_ret = select(x11_fd + 1, &fds, 0, 0, &tv);

    while (select_ret > 0 && XPending(win->p.display)) {
        WinEvent win_event = {0};
        XEvent event = {0};
        XNextEvent(win->p.display, &event);

        switch (event.type) {
        case Expose:
            draw_to_win(win, monitor);
            break;
        case ConfigureNotify: {
            XConfigureEvent configured = event.xconfigure;
            win->w = configured.width;
            win->h = configured.height;
            resize_win(win);
        } break;
        default:
            break;
        }

        if (win_event.type) win->event_queue[win->event_queue_len++] = win_event;
    }

    if (select_ret < 0) {
        win->event_queue[win->event_queue_len++] = (WinEvent) { .type = EVENT_ERR };
    } else {
        win->event_queue[win->event_queue_len++] = (WinEvent) { .type = EVENT_TIMEOUT };
    }
}

#include "main.c"

static void setup_monitor_wins(Win *wins, PlatformMonitors *monitors, char *name) {
    for (int i = 0; i < monitors->len; i++) {
        new_win(&wins[i], name, 500, 500);
        make_win_bg(&wins[i], monitors->buf[i], true);
        show_win(&wins[i]);
        ctx.monitors[i].screen = (Image) {
            .buf = wins[i].buf,
            .alloc_w = wins[i].w,
            .w = wins[i].w,
            .h = wins[i].h,
        };
    }
    ctx.monitors_len = monitors->len;
}

static void refresh_monitors(Win *wins, PlatformMonitors *monitors, char *name) {
    for (int i = 0; i < MAX_PLATFORM_MONITORS; i++) {
        close_win(&wins[i]);
        ctx.monitors[i].screen = (Image) {0};
        free(ctx.monitors[i].wallpaper_frame.img.buf);
        ctx.monitors[i].wallpaper_frame.img = (Image) {0};
    }

    collect_monitors(monitors);
    setup_monitor_wins(wins, monitors, name);
    make_fonts();
    my_semaphore_increment(&needs_scaling);
}

int main(int argc, char *argv[]) {
    (void) argc;
    (void) argv;

    PlatformMonitors monitors = {0};
    collect_monitors(&monitors);

    Win wins[arrlen(monitors.buf)] = {0};
    setup_monitor_wins(wins, &monitors, APP_NAME);

    start();

    time_t last_rendered_minute = -1;
    Color *last_backgrounds[MAX_PLATFORM_MONITORS] = {0};

    while (true) {
        bool should_render = false;

        time_t now = time(0) + 1;
        time_t minute = now / 60;
        if (minute != last_rendered_minute) should_render = true;

        PlatformMonitors latest_monitors = {0};
        collect_monitors(&latest_monitors);
        if (did_monitors_change(&monitors, &latest_monitors)) {
            monitors = latest_monitors;
            refresh_monitors(wins, &monitors, APP_NAME);
            for (int i = 0; i < MAX_PLATFORM_MONITORS; i++) last_backgrounds[i] = 0;
            last_rendered_minute = -1;
            should_render = true;
        }

        for (int monitor_i = 0; monitor_i < ctx.monitors_len; monitor_i++) {
            pthread_mutex_lock(&scaled_lock);
                Color *background = ctx.monitors[monitor_i].scaled_background.img.buf;
            pthread_mutex_unlock(&scaled_lock);

            if (background != last_backgrounds[monitor_i]) {
                should_render = true;
                break;
            }
        }

        if (should_render) {
        for (int monitor_i = 0; monitor_i < ctx.monitors_len; monitor_i++) {
            Monitor *monitor = &ctx.monitors[monitor_i];
            Win *win = &wins[monitor_i];

            monitor->screen = (Image) {
                .buf = win->buf,
                .alloc_w = win->w,
                .w = win->w,
                .h = win->h,
            };

            app_loop(monitor_i, last_backgrounds[monitor_i] == 0 || last_backgrounds[monitor_i] != ctx.monitors[monitor_i].scaled_background.img.buf, now);

            pthread_mutex_lock(&scaled_lock);
                last_backgrounds[monitor_i] = ctx.monitors[monitor_i].scaled_background.img.buf;
            pthread_mutex_unlock(&scaled_lock);

            draw_to_win(win, &monitors.buf[monitor_i]);
        }
            last_rendered_minute = minute;
        }

        struct timeval time_val = {0};
        gettimeofday(&time_val, NULL);
        int next_minute_ms = 60000 - (((time_val.tv_sec % 60) * 1000) + (time_val.tv_usec / 1000));
        if (next_minute_ms <= 0) next_minute_ms = 1;

        get_events_timeout(
            &wins[0],
            &monitors.buf[0],
            next_minute_ms
        );

        for (int i = 0; i < wins[0].event_queue_len; i++) {
            WinEvent event = wins[0].event_queue[i];
            if (event.type == EVENT_QUIT) goto end;
        }

        wins[0].event_queue_len = 0;
    }

end:
    for (int win_i = 0; win_i < ctx.monitors_len; win_i++) {
        close_win(&wins[win_i]);
    }
}
