// TODO: See if dpi_x == dpi_y on Linux
#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#ifdef __linux__
#include <X11/Xlib.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
typedef enum {
    COLOR_R,
    COLOR_G,
    COLOR_B,
    COLOR_A,
} PlatformColorEnum;
typedef struct {
    Window win;
    Display *display;
    int screen;
    GC gc;
    XImage *img;
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

typedef struct Win {
    int screen, dpi_x, dpi_y, w, h;
    Color *buf;
    bool resized, is_bg;
    WinEvent event;
    PlatformWin p;
} Win;

void new_win(Win *win, int w, int h);
void make_win_bg(Win *win, HWND worker_w);
void show_win(Win win);
void draw_to_win(Win win);
void collect_monitors(Monitors *m);
void wait_event_timeout(Win *win, int timeout_ms);
bool get_next_event(Win *win);
void show_sys_tray_icon(Win *win, int icon_id, char *tooltip);
void kill_sys_tray_icon(Win *win, int icon_id);
void close_win(Win *win);

#endif // GRAPHICS_H

#ifdef GRAPHICS_IMPL
#ifndef GRAPHICS_IMPL_GUARD
#define GRAPHICS_IMPL_GUARD

#ifdef _WIN32

#define SYS_TRAY_MSG (WM_USER + 1)

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

void _resize_win(Win *win) {
    if (win->buf) {
        free(win->buf);
    }

    win->p.bitmap_info = (BITMAPINFO) {
        .bmiHeader = {
            .biSize = sizeof(win->p.bitmap_info.bmiHeader),
            .biWidth = win->w,
            .biHeight = -win->h,
            .biPlanes = 1,
            .biBitCount = 32,
        },
    };

    win->buf = calloc(win->w * win->h, sizeof(*win->buf));
    win->dpi_x = win->dpi_y = GetDpiForWindow(win->p.win);
    win->resized = true;
}

void _fill_working_area(Win *win) {
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

    switch (msg) {
    case WM_TIMER: win->event.type = EVENT_TIMEOUT; break;
    case WM_SIZE: _resize_win(win); break;
    case WM_DESTROY: case WM_CLOSE: {
        win->event.type = EVENT_QUIT;
        // PostQuitMessage(0);
    } break;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        BeginPaint(pwin, &paint);
        draw_to_win(*win); // TODO: DO THIS!!!
        EndPaint(pwin, &paint);
    } break;
    case SYS_TRAY_MSG: {
        win->event.type = EVENT_SYS_TRAY;

        int c = 0;
        switch (LOWORD(vv)) {
        case WM_LBUTTONUP:   c = CLICK_L_UP; break;
        case WM_LBUTTONDOWN: c = CLICK_L_DOWN; break;
        case WM_MBUTTONUP:   c = CLICK_M_UP; break;
        case WM_MBUTTONDOWN: c = CLICK_M_DOWN; break;
        case WM_RBUTTONUP:   c = CLICK_R_UP; break;
        case WM_RBUTTONDOWN: c = CLICK_R_DOWN; break;
        }
        win->event.click = c;
    } break;
    default: {
        ret = DefWindowProc(pwin, msg, hv, vv);
    } break;
    }

    return ret;
}
#endif

void new_win(Win *win, int w, int h) {
    *win = (Win) {0};

#ifdef __linux__
    win->p.display = XOpenDisplay(0);
    if (win->p.display == 0) err("Failed to open display.");

    win->p.screen = DefaultScreen(win->p.display);
    win->p.win = DefaultRootWindow(win->p.display);
    XSelectInput(win->p.display, win->p.win, ExposureMask);
    XMapWindow(win->p.display, win->p.win);

    win->p.gc = DefaultGC(win->p.display, win->p.screen);

    win->w = XDisplayWidth(win->p.display, win->p.screen);
    win->h = XDisplayHeight(win->p.display, win->p.screen);
    win->dpi_x = win->w /
                ((float) DisplayWidthMM(win->p.display, win->p.screen) / 25.4);
    win->dpi_y = win->h /
                ((float) DisplayHeightMM(win->p.display, win->p.screen) / 25.4);

    win->buf = calloc(win->w * win->h, sizeof(*win->buf));

    win->p.img = XCreateImage(
        win->p.display,
        DefaultVisual(win->p.display, win->p.screen),
        24, ZPixmap, 0, (char *) buf, w, h, 32, 0
    );

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

void make_win_bg(Win *win, HWND worker_w) {
    win->is_bg = true;
#ifdef __linux__
    assert(!"Unimplemented")
#elif _WIN32
    SetParent(win->p.win, worker_w);

    SetWindowLongPtrA(
        win->p.win,
        GWL_STYLE,
        WS_POPUP | WS_BORDER | WS_SYSMENU
    );

    _fill_working_area(win);
#endif
}

void show_win(Win win) {
#ifdef __linux__
    assert(!"Unimplemented")
#elif _WIN32
    ShowWindow(win.p.win, SW_NORMAL);
#endif
}

void draw_to_win(Win win) {
#ifdef __linux__
    XPutImage(
        win.p.display, 
        win.p.win, win.p.gc, win.p.img,
        0, 0, 0, 0,
        win.p.img->width, win.p.img->height
    );

    XFlush(win.display);
#elif _WIN32
    HDC ctx = GetDC(win.p.win);
    StretchDIBits(
        ctx,
        0, 0, win.w, win.h,
        0, 0, win.w, win.h,
        win.buf,
        &win.p.bitmap_info,
        DIB_RGB_COLORS,
        SRCCOPY
    );
    ReleaseDC(win.p.win, ctx);
#endif
}

BOOL _collect_monitors_cb(HMONITOR h, HDC hdc, LPRECT rect, LPARAM vv) {
    Monitors *m = (Monitors *) vv;
    m->buf[m->len++] = (PlatformMonitor) {
        .rect = *rect,
    };
    return true;
}

void collect_monitors(Monitors *m) {
#ifdef __linux__
    assert(!"Unimplemented");
#elif _WIN32
    EnumDisplayMonitors(NULL, NULL, _collect_monitors_cb, (LPARAM) m);
#endif
}

void move_win_to_monitor(Win *win, PlatformMonitor m) {
#ifdef __linux__
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
    assert(!"Unimplemented");
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

void wait_event_timeout(Win *win, int timeout_ms) {
    win->event = (WinEvent) {0};

#ifdef __linux__
    fd_set fds;
    struct timeval tv;
    int x11_fd = ConnectionNumber(win->display);

    FD_ZERO(&fds);
    FD_SET(x11_fd, &fds);
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int select_ret = select(x11_fd + 1, &fds, 0, 0, &tv);

    XEvent e;
    if (select_ret > 0) {
        XNextEvent(win->display, &e);
        // win->event.type = EVENT_EVENT;
        // TODO: handle events
    } else if (select_ret < 0) {
        win->event.type = EVENT_ERR;
    } else {
        win->event.type = EVENT_TIMEOUT;
    }
#elif _WIN32
    UINT_PTR timer_id = SetTimer(win->p.win, 1, timeout_ms, 0);
    if (!timer_id) {
        win->event.type = EVENT_ERR;
        return;
    }

    if (WaitMessage()) {
    } else win->event.type = EVENT_ERR;
    
    KillTimer(0, timer_id);
#endif
}

bool get_next_event(Win *win) {
    bool ret = 0;
#ifdef __linux__
    assert(!"Unimplemented");
#elif _WIN32
    MSG msg;
    if (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        ret = true;
    }

    if (win->event.type == EVENT_TIMEOUT && win->is_bg) {
        _fill_working_area(win);
    }

    return ret;
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
    Shell_NotifyIcon(NIM_ADD, &nid); // TODO: check for errors
    #endif
}

void kill_sys_tray_icon(Win *win, int icon_id) {
    NOTIFYICONDATA nid = {
        .cbSize = sizeof(NOTIFYICONDATA),
        .hWnd = win->p.win,
        .uID = icon_id,
    };
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void close_win(Win *win) {
#ifdef __linux__
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

#endif // GRAPHICS_IMPL_GUARD
#endif // GRAPHICS_IMPL
