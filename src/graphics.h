// TODO: Fix the flickering on Linux (maybe by using pixmap?)
// TODO: See if dpi_x == dpi_y on Linux
// TODO: make err(...) do something different depending on the function 
#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>
#include <X11/Xatom.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
typedef enum {
    COLOR_B,
    COLOR_G,
    COLOR_R,
    COLOR_A,
} PlatformColorEnum;
typedef struct {
    int x, y, w, h;
} PlatformMonitor;
typedef struct {
    Window win;
    Display *display;
    int screen;
    GC gc;
    XImage *img;
    bool draw_to_img;
} PlatformWin;
#elif _WIN32
#include <Windows.h>
typedef enum {
    COLOR_B,
    COLOR_G,
    COLOR_R,
    COLOR_A,
} PlatformColorEnum;
typedef struct {
    RECT rect;
} PlatformMonitor;
typedef struct {
    HWND win;
    BITMAPINFO bitmap_info;
} PlatformWin;
#else
#error "Unsupported platform!"
#endif

typedef struct {
    uint8_t c[4];
} Color;

#define MAX_PLATFORM_MONITORS 100
typedef struct {
    PlatformMonitor buf[MAX_PLATFORM_MONITORS];
    int len;
} Monitors;

typedef struct {
    enum {
        NO_EVENT = 0,
        EVENT_TIMEOUT,
        EVENT_QUIT,
        EVENT_ERR,
        EVENT_SYS_TRAY,
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
} WinEvent;

#define MAX_EVENT_QUEUE_LEN 30
typedef struct Win {
    int screen, dpi_x, dpi_y, w, h;
    Color *buf;
    bool resized, is_bg;
    WinEvent event_queue[MAX_EVENT_QUEUE_LEN];
    int event_queue_len;
    PlatformWin p;
} Win;

void new_win(Win *win, char *name, int w, int h);
void make_win_bg(Win *win, PlatformMonitor monitor);
void show_win(Win *win);
void draw_to_win(Win *win);
void collect_monitors(Monitors *m);
void get_events_timeout(Win *win, int timeout_ms);
void show_sys_tray_icon(Win *win, int icon_id, char *tooltip);
void kill_sys_tray_icon(Win *win, int icon_id);
void close_win(Win *win);

bool is_program_already_open(char *id);
s8 get_desktop_name();

#endif // GRAPHICS_H

#ifdef GRAPHICS_IMPL
#ifndef GRAPHICS_IMPL_GUARD
#define GRAPHICS_IMPL_GUARD

void _resize_win(Win *win) {
#ifdef __linux__
    if (!win->p.draw_to_img && win->buf) XDestroyImage(win->p.img); // NOTE: XDestroyImage frees win->buf (X11...)
    else free(win->buf);
    win->buf = 0;
#endif
    win->resized = true;
    win->buf = calloc(win->w * win->h, sizeof(*win->buf));
#ifdef __linux__
    win->dpi_x = win->w /
                ((float) DisplayWidthMM(win->p.display, win->p.screen) / 25.4);
    win->dpi_y = win->h /
                ((float) DisplayHeightMM(win->p.display, win->p.screen) / 25.4);
    if (!win->p.draw_to_img) {
        win->p.img = XCreateImage(
            win->p.display,
            DefaultVisual(win->p.display, win->p.screen),
            24, ZPixmap, 0, (char *) win->buf, win->w, win->h, 32, 0
        );
    }
#elif _WIN32
    if (win->buf) {
        free(win->buf);
        win->buf = 0;
    }
    win->dpi_x = win->dpi_y = GetDpiForWindow(win->p.win);
    win->p.bitmap_info = (BITMAPINFO) {
        .bmiHeader = {
            .biSize = sizeof(win->p.bitmap_info.bmiHeader),
            .biWidth = win->w,
            .biHeight = -win->h,
            .biPlanes = 1,
            .biBitCount = 32,
        },
    };
#endif
}

void _fill_working_area(Win *win, PlatformMonitor m) {
#ifdef __linux__
    if (!win->p.draw_to_img) XMoveResizeWindow(win->p.display, win->p.win, m.x, m.y, m.w, m.h);
    win->w = m.w;
    win->h = m.h;
    _resize_win(win);
#elif _WIN32
    RECT old;
    GetClientRect(win->p.win, &old);

    MONITORINFO info = { .cbSize = sizeof(MONITORINFO), };

    RECT old_abs;
    GetWindowRect(win->p.win, &old_abs);
    POINT point = { old_abs.left, old_abs.top, };
    HMONITOR monitor = MonitorFromPoint(point, 0);
    assert(monitor);

    bool yes = GetMonitorInfoA(monitor, &info);
    assert(yes);

    RECT work = info.rcWork;

    int w = work.right - work.left,
        h = work.bottom - work.top;

    bool resized = work.left != old.left ||
                   work.right != old.right ||
                   work.bottom != old.bottom ||
                   work.top != old.top;

    if (resized) {
        win->dpi_x = win->dpi_y = GetDpiForWindow(win->p.win);
        SetWindowPos(
            win->p.win,
            0,
            work.left, work.top,
            w, h,
            SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER
        );
    }
#endif
}

#ifdef _WIN32

#define SYS_TRAY_MSG (WM_USER + 1)

BOOL _collect_monitors_cb(HMONITOR h, HDC hdc, LPRECT rect, LPARAM vv) {
    Monitors *m = (Monitors *) vv;
    m->buf[m->len++] = (PlatformMonitor) {
        .rect = *rect,
    };
    return true;
}

// https://www.codeproject.com/Articles/856020/Draw-Behind-Desktop-Icons-in-Windows-plus

BOOL _make_worker_w_cb(HWND top, LPARAM vv) {
    HWND p = FindWindowExA(top, 0, "SHELLDLL_DefView", 0);
    if (p) {
        *((HWND *) vv) = FindWindowExA(0, top, "WorkerW", 0);
        return false;
    }
    return true;
}

HWND _make_worker_w() {
    HWND progman = FindWindowA("Progman", 0);
    if (!progman) return 0;

    SendMessageTimeoutA(progman, 0x052C, 0xD, 0x1, SMTO_NORMAL, 1000, 0);

    HWND ret = 0;
    
    EnumWindows(_make_worker_w_cb, (LPARAM) &ret);

    // TODO: Other stuff for CONFIGURATIONNNNNNNNNNNNNNNNNNNNNNNNNNNN!!!!!!!!!!!!!! depending on the set up.

    return ret;
}

LRESULT _main_win_cb(HWND pwin, UINT msg, WPARAM hv, LPARAM vv) {
    LRESULT ret = 0;

    Win *win = (Win *) GetWindowLongPtr(pwin, GWLP_USERDATA);

    // If the window isn't passed, do the default window procedure.
    if (!win) {
        return DefWindowProc(pwin, msg, hv, vv);
    }

    RECT client_rect;
    GetClientRect(pwin, &client_rect);

    win->p.win = pwin;
    win->w = client_rect.right - client_rect.left;
    win->h = client_rect.bottom - client_rect.top;
    win->dpi_x = win->dpi_y = GetDpiForWindow(pwin);

    WinEvent win_event = {0};

    switch (msg) {
    case WM_TIMER: {
        win_event.type = EVENT_TIMEOUT;
        if (win->is_bg) _fill_working_area(win, NULL);
    } break;
    case WM_SIZE: _resize_win(win); break;
    case WM_DESTROY: case WM_CLOSE: {
        win_event.type = EVENT_QUIT;
        // PostQuitMessage(0);
    } break;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        BeginPaint(pwin, &paint);
        draw_to_win(win); // TODO: DO THIS!!!
        EndPaint(pwin, &paint);
    } break;
    case SYS_TRAY_MSG: {
        win_event.type = EVENT_SYS_TRAY;

        int c = 0;
        switch (LOWORD(vv)) {
        case WM_LBUTTONUP:   c = CLICK_L_UP; break;
        case WM_LBUTTONDOWN: c = CLICK_L_DOWN; break;
        case WM_MBUTTONUP:   c = CLICK_M_UP; break;
        case WM_MBUTTONDOWN: c = CLICK_M_DOWN; break;
        case WM_RBUTTONUP:   c = CLICK_R_UP; break;
        case WM_RBUTTONDOWN: c = CLICK_R_DOWN; break;
        }

        win_event.click = c;
    } break;
    default: {
        ret = DefWindowProc(pwin, msg, hv, vv);
    } break;
    }

    if (win_event.type) win->event_queue[win->event_queue_len++] = win_event;
    assert(win->event_queue_len <= MAX_EVENT_QUEUE_LEN);

    return ret;
}
#endif

void new_win(Win *win, char *name, int w, int h) {
    *win = (Win) {0};

#ifdef __linux__
    win->p.display = XOpenDisplay(0);
    if (!win->p.display) err("Failed to open display.");

    win->p.screen = DefaultScreen(win->p.display);
    win->p.win = XCreateSimpleWindow(
        win->p.display,
        DefaultRootWindow(win->p.display),
        0, 0, w, h,
        1, 0, 0
    ); // TODO: check for errors
    XSelectInput(win->p.display, win->p.win, ExposureMask | StructureNotifyMask);

    win->p.gc = DefaultGC(win->p.display, win->p.screen);

    win->w = w;
    win->h = h;
    _resize_win(win);

     // TODO: make this transfer when make_win_bg is called
    XStoreName(win->p.display, win->p.win, name);
    // win->name = name;

    if (win->p.img == 0) err("Failed to create window.");

#elif _WIN32
    WNDCLASS win_class = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = _main_win_cb,
        .hInstance = GetModuleHandle(0),
        .lpszClassName = name,
       	.hCursor = LoadCursor(0, IDC_ARROW),
    };
    RegisterClassA(&win_class); // TODO: Check for errors.

    RECT work = {0};
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);

    win->p.win = CreateWindowExA( // TODO: Check for errors.
        0,
        win_class.lpszClassName,
        "Untited Window", // TODO: Add ability to change name
        WS_OVERLAPPEDWINDOW,
        work.left, work.top, w, h,
        0,
        0,
        GetModuleHandle(0),
        0
    );

    SetWindowTextA(win->p.win, name);

    SetWindowLongPtr(win->p.win, GWLP_USERDATA, (LONG_PTR) win);

    win->dpi_x = win->dpi_y = GetDpiForWindow(win->p.win);
    win->buf = calloc(w * h, sizeof(*win->buf));
#endif
}
void set_desktop_window_type(Display *display, Window window) {
    // Get the atoms for the property name and the property value.
    // XInternAtom queries the X server for the unique ID (Atom) of a string.
    Atom wm_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom wm_desktop = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DESKTOP", False);

    // XChangeProperty is used to change a property on a window.
    // The arguments are:
    // 1. display: The Display.
    // 2. window: The Window ID.
    // 3. property: The Atom for the property name (_NET_WM_WINDOW_TYPE).
    // 4. type: The Atom for the data type of the property (XA_ATOM for an atom).
    // 5. format: The size of the data elements in bits (32 for atoms).
    // 6. mode: How to set the property. PropModeReplace replaces the existing value.
    // 7. data: A pointer to the data to set. In this case, a pointer to our atom.
    // 8. nelements: The number of items in the data array (1 for a single atom).
    XChangeProperty(
        display,
        window,
        wm_type,
        XA_ATOM,
        32,
        PropModeReplace,
        (unsigned char *)&wm_desktop,
        1
    );

    // Flush the output buffer to make sure the request is sent to the server immediately.
    XFlush(display);
}

// TODO: worker_w
void make_win_bg(Win *win, PlatformMonitor monitor) {
    win->is_bg = true;

#ifdef __linux__
    if (win->p.draw_to_img) return;

    close_win(win);

    win->p.display = XOpenDisplay(0);
    if (!win->p.display) err("Failed to open display.");

    win->p.screen = DefaultScreen(win->p.display);

    if (get_desktop_name().len) {
        win->p.win = XCreateSimpleWindow(
            win->p.display,
            DefaultRootWindow(win->p.display),
            0, 0, 500, 500,
            1, 0, 0
        ); // TODO: check for errors
        set_desktop_window_type(win->p.display, win->p.win);
    } else win->p.win = DefaultRootWindow(win->p.display);

    XSelectInput(win->p.display, win->p.win, ExposureMask | StructureNotifyMask);

    win->p.gc = DefaultGC(win->p.display, win->p.screen);

    // _resize_win(win);
    _fill_working_area(win, monitor);

    if (!win->p.img) err("Failed to make the window the background.");

#elif _WIN32
    SetParent(win->p.win, worker_w);

    SetWindowLongPtrA(
        win->p.win,
        GWL_STYLE,
        WS_POPUP | WS_SYSMENU
    );

    _fill_working_area(win, monitor);
#endif
}

void show_win(Win *win) {
#ifdef __linux__
    if (win->p.draw_to_img) return;

    XMapWindow(win->p.display, win->p.win);
#elif _WIN32
    ShowWindow(win->p.win, SW_NORMAL);
#endif
}

void draw_to_win(Win *win) {
#ifdef __linux__
    if (win->p.draw_to_img) return;

    XPutImage(
        win->p.display, 
        win->p.win, win->p.gc, win->p.img,
        0, 0, 0, 0,
        win->p.img->width, win->p.img->height
    );

    XFlush(win->p.display);
#elif _WIN32
    HDC ctx = GetDC(win->p.win);
    StretchDIBits(
        ctx,
        0, 0, win->w, win->h,
        0, 0, win->w, win->h,
        win->buf,
        &win->p.bitmap_info,
        DIB_RGB_COLORS,
        SRCCOPY
    );
    ReleaseDC(win->p.win, ctx);
#endif
}

void collect_monitors(Monitors *m) {
    *m = (Monitors) {0};
#ifdef __linux__
    Display *display = XOpenDisplay(NULL);
    if (!display) err("Can't open display.");

    int major, minor;
    if (!XineramaQueryVersion(display, &major, &minor)) {
        err("Xinerama extension is not available.");
    }

    if (!XineramaIsActive(display)) err("Xinerama is not active.");

    XineramaScreenInfo *screens = XineramaQueryScreens(display, &m->len);
    if (!screens) err("No screens are connected.");

    for (int i = 0; i < m->len; i++) {
        m->buf[i] = (PlatformMonitor) {
            .x = screens[i].x_org,
            .y = screens[i].y_org,
            .w = screens[i].width,
            .h = screens[i].height,
        };
    }
    XFree(screens);

    XCloseDisplay(display);
#elif _WIN32
    EnumDisplayMonitors(NULL, NULL, _collect_monitors_cb, (LPARAM) m);
#endif
}

void move_win_to_monitor(Win *win, PlatformMonitor m) {
#ifdef __linux__
    if (win->p.draw_to_img) return;

    assert(!"Unimplemented");
#elif _WIN32
    SetWindowPos(
        win->p.win,
        0,
        m.rect.left, m.rect.top,
        win->w, win->h,
        SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER
    );
#endif
}

bool are_monitors_equal(PlatformMonitor a, PlatformMonitor b) {
#ifdef __linux__
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
#elif _WIN32
    // print("%ld, %ld, %ld, %ld", a.rect.left, a.rect.top, a.rect.right, a.rect.bottom);
    // print("%ld, %ld, %ld, %ld", b.rect.left, b.rect.top, b.rect.right, b.rect.bottom);
    return EqualRect(&a.rect, &b.rect);
#endif
}

bool did_monitors_change(Monitors *a, Monitors *b) {
    if (a->len != b->len) return true;
    for (int i = 0; i < a->len; i++) {
        if (!are_monitors_equal(a->buf[i], b->buf[i])) return true;
    }
    return false;
}

void get_events_timeout(Win *win, int timeout_ms) { // TODO: find a way to get events for all windows
#ifdef __linux__
    if (win->p.draw_to_img) return;

    fd_set fds;
    struct timeval tv;
    int x11_fd = ConnectionNumber(win->p.display);

    FD_ZERO(&fds);
    FD_SET(x11_fd, &fds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int select_ret = select(x11_fd + 1, &fds, 0, 0, &tv);

    while (select_ret > 0 && XPending(win->p.display)) {
        WinEvent win_event = {0};
        XEvent e;
        XNextEvent(win->p.display, &e);

        switch (e.type) {
        // TODO: Handle window closing
        case Expose: draw_to_win(win); break;
        case ConfigureNotify: {
            XConfigureEvent c = e.xconfigure;
            // if (c.width != win->w || c.height != win->h) { // TODO: see about this
                win->w = c.width; win->h = c.height;
                printf("OLD: %d %d %d %d\n", win->w, win->h, win->p.img->width, win->p.img->height);
                _resize_win(win);
                printf("NEW: %d %d %d %d\n", win->w, win->h, win->p.img->width, win->p.img->height);
            // }
        } break;
        default: break;
        }

        if (win_event.type) win->event_queue[win->event_queue_len++] = win_event;
    }
    if (select_ret <= 0) {
        WinEvent win_event = { .type = EVENT_ERR, };
        win->event_queue[win->event_queue_len++] = win_event;
    } else {
        WinEvent win_event = { .type = EVENT_TIMEOUT, };
        win->event_queue[win->event_queue_len++] = win_event;
    }
#elif _WIN32
    UINT_PTR timer_id = SetTimer(0, 1, timeout_ms, 0);

    WaitMessage();

    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    KillTimer(0, timer_id);
#endif
}

// From rc file
void show_sys_tray_icon(Win *win, int icon_id, char *tooltip) {
    #ifdef __linux__
    (void) win;
    assert(!"Unimplemented");
    #elif _WIN32
    NOTIFYICONDATA nid = {
        .cbSize = sizeof(NOTIFYICONDATA),
        .hWnd = win->p.win,
        .uID = icon_id,
        .uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP,
        .uCallbackMessage = SYS_TRAY_MSG,
    };
    nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(icon_id));
    lstrcpy(nid.szTip, tooltip);
    if (!Shell_NotifyIcon(NIM_ADD, &nid)) warning("Could not add shell icon.");
    #endif
}

void kill_sys_tray_icon(Win *win, int icon_id) {
#ifdef __linux__
    assert(!"Unimplemented");
#elif _WIN32
    NOTIFYICONDATA nid = {
        .cbSize = sizeof(NOTIFYICONDATA),
        .hWnd = win->p.win,
        .uID = icon_id,
    };
    Shell_NotifyIcon(NIM_DELETE, &nid);
#endif
}

void close_win(Win *win) {
    free(win->buf);
    win->buf = 0;

#ifdef __linux__
    if (win->p.draw_to_img) return;

    XDestroyWindow(win->p.display, win->p.win);
    XCloseDisplay(win->p.display);
#elif _WIN32
    if (win->is_bg) {
        SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, 0, SPIF_UPDATEINIFILE);
    }
    DestroyWindow(win->p.win);
#endif
    free(win->buf);
    *win = (Win) {0};
}

bool is_program_already_open(char *id) {
#ifdef __linux__
    assert(!"Unimplemented");
#elif _WIN32
    char path[1024] = { 'L', 'o', 'c', 'a', 'l', '\\', '$', };
    int path_len = strlen(path);

    int id_len = strlen(id);
    for (int i = 0; i < id_len; i++) {
        path[path_len++] = id[i];
    }

    path[path_len++] = '$';

    CreateMutexA(0, 0, path);

    return (GetLastError() == ERROR_ALREADY_EXISTS);
#endif
}

s8 get_desktop_name() {
    s8 ret = { .buf = getenv("XDG_CURRENT_DESKTOP"), };
    if (ret.buf) ret.len = strlen(ret.buf);
    return ret;
}

#endif // GRAPHICS_IMPL_GUARD
#endif // GRAPHICS_IMPL
