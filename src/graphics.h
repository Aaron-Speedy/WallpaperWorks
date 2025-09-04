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
} ColorEnum;
typedef struct {
    Window win;
    Display *display;
    int screen;
    GC gc;
    XImage *img;
} PlatformWin;
#elif _WIN32
typedef enum {
    COLOR_B,
    COLOR_G,
    COLOR_R,
    COLOR_A,
} ColorEnum;
#include <Windows.h>
typedef struct {
    HWND win;
    BITMAPINFO bitmap_info;
    HWND worker_w, shell;
} PlatformWin;
#else
#error "Unsupported platform!"
#endif

typedef struct {
    uint8_t c[4];
} Color;

typedef enum {
    NO_EVENT = 0,
    EVENT_TIMEOUT,
    EVENT_QUIT,
    EVENT_ERR,
    NUM_WIN_EVENTS,
} WinEvent;

typedef struct {
    int screen, dpi_x, dpi_y, w, h;
    Color *buf;
    bool resized;
    WinEvent event;
    PlatformWin p;
} Win;

void get_bg_win(Win *win);
void draw_to_win(Win win);
void next_event_timeout(Win *win, int timeout_ms);
void close_win(Win win);

#endif // GRAPHICS_H

#ifdef GRAPHICS_IMPL
#ifndef GRAPHICS_IMPL_GUARD
#define GRAPHICS_IMPL_GUARD

#ifdef _WIN32

// https://www.codeproject.com/Articles/856020/Draw-Behind-Desktop-Icons-in-Windows-plus

BOOL _find_worker_w_cb(HWND top, LPARAM vv) {
    HWND p = FindWindowExA(top, 0, "SHELLDLL_DefView", 0);
    if (p) {
        PlatformWin *win = (PlatformWin *) vv;
        win->worker_w = FindWindowExA(0, top, "WorkerW", 0);
        win->shell = p;
        return false;
    }
    return true;
}

void _find_worker_w(PlatformWin *win) {
    HWND progman = FindWindowA("Progman", 0);
    if (!progman) return;

    SendMessageTimeoutA(progman, 0x052C, 0xD, 0x1, SMTO_NORMAL, 1000, 0);

    EnumWindows(_find_worker_w_cb, (LPARAM) win);

    // TODO: Other stuff for CONFIGURATIONNNNNNNNNNNNNNNNNNNNNNNNNNNN!!!!!!!!!!!!!! depending on the set up.

    // p.worker_w = FindWindowExA(progman, 0, "WorkerW", 0);

    // HWND shell = FindWindowExA(progman, 0, "SHELLDLL_DefView", 0);

    // if (shell) {
    //     assert(0);
    //     worker_w = GetWindow(shell, GW_HWNDPREV);
    //     char name[256] = {0};
    //     GetClassName(worker_w, name, sizeof(name));
    //     if (!strcmp(name, "WorkerW")) {
    //         assert(0);
    //         return false;
    //     }
    // }
}

LRESULT _main_win_cb(HWND pwin, UINT msg, WPARAM hv, LPARAM vv) {
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
        case WM_TIMER: win->event = EVENT_TIMEOUT; break;
        case WM_SIZE: {
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
            win->resized = true;
        } break;
        case WM_DESTROY: case WM_CLOSE: {
            win->event = EVENT_QUIT;
            PostQuitMessage(0);
        } break;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            BeginPaint(pwin, &paint);
            draw_to_win(*win); // TODO: DO THIS!!!
            EndPaint(pwin, &paint);
        } break;
        default: {
            return DefWindowProc(pwin, msg, hv, vv);
        } break;
    }

    return 0;
}
#endif

void get_bg_win(Win *win) {
    assert(win);

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
        .lpszClassName = "wallpaperworks",
       	.hCursor = LoadCursor(0, IDC_ARROW),
    };
    RegisterClassA(&win_class); // TODO: Check for errors.

    _find_worker_w(&win->p);

    win->p.win = CreateWindowExA( // TODO: Check for errors.
        0,
        win_class.lpszClassName,
        "Untited Window", // TODO: Add ability to change name
        WS_POPUP | WS_BORDER | WS_SYSMENU | WS_MAXIMIZE,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        0,
        0,
        GetModuleHandle(0),
        0
    );

    // SetWindowLong(win->p.win, GWL_STYLE, 0);
    SetWindowLongPtr(win->p.win, GWLP_USERDATA, (LONG_PTR) win);
    SetParent(win->p.win, win->p.worker_w);

    RECT rect;
    GetClientRect(win->p.win, &rect);
    win->w = rect.right - rect.left;
    win->h = rect.bottom - rect.top;
    win->dpi_x = win->dpi_y = GetDpiForWindow(win->p.win);
    win->buf = calloc(win->w * win->h, sizeof(*win->buf));

    ShowWindow(win->p.win, SW_NORMAL);
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
#endif
}

void next_event_timeout(Win *win, int timeout_ms) {
    win->event = 0;

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
        win->event = EVENT_ERR;
    } else {
        win->event = EVENT_TIMEOUT;
    }
#elif _WIN32
    UINT_PTR timer_id = SetTimer(win->p.win, 1, timeout_ms, 0);
    if (!timer_id) {
        win->event = EVENT_ERR;
        return;
    }

    MSG msg;
    if (GetMessage(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    } else win->event = EVENT_ERR;

    KillTimer(0, timer_id);
#endif
}

void close_win(Win win) {
#ifdef __linux__
    XCloseDisplay(win.p.display);
#elif _WIN32
    (void) win;
    assert(!"Unimplemented");
#endif
}

#endif // GRAPHICS_IMPL_GUARD
#endif // GRAPHICS_IMPL
