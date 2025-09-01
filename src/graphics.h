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
typedef struct {
    Window win;
    Display *display;
    int screen;
    GC gc;
    XImage *img;
} PlatformWin;
#elif _WIN32
#include <Windows.h>
typedef struct {
    HWND win;
    BITMAPINFO bitmap_info;
} PlatformWin;
#else
#error "Unsupported platform!"
#endif

typedef enum {
    EVENT_ERR = 0,
    EVENT_TIMEOUT,
    EVENT_QUIT,
    NUM_WIN_EVENTS,
} WinEvent;

typedef struct {
    int screen, dpi_x, dpi_y, w, h;
    uint32_t *buf;
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

LRESULT _main_win_cb(HWND pwin, UINT msg, WPARAM hv, LPARAM vv) {
    LRESULT ret = 0;

    Win *win = (Win *) GetWindowLongPtr(pwin, GWLP_USERDATA);

    RECT client_rect;
    GetClientRect(pwin, &client_rect);

    if (win) {
        win->p.win = pwin;
        win->w = client_rect.right - client_rect.left;
        win->h = client_rect.bottom - client_rect.top;
    }
    
    switch (msg) {
        case WM_TIMER: {
            assert(win);
            win->event = EVENT_TIMEOUT;
        } break;
        case WM_SIZE: {
            assert(win);

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
            assert(win);
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
            ret = DefWindowProc(pwin, msg, hv, vv);
        } break;
    }

    return ret;
}
#endif

void get_bg_win(Win *win) {
    assert(win);

    *win = (Win) {0};

#ifdef __linux__
    win->p.display = XOpenDisplay(NULL);
    if (win->p.display == NULL) err("Failed to open display.");

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

    if (win->p.img == NULL) err("Failed to create window.");

#elif _WIN32
    WNDCLASS win_class = {
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = _main_win_cb,
        .hInstance = GetModuleHandle(NULL),
        .lpszClassName = "wallpaperworks",
       	.hCursor = LoadCursor(NULL, IDC_ARROW),
    };
    RegisterClassA(&win_class); // TODO: Check for errors.

    win->p.win = CreateWindowExA( // TODO: Check for errors.
        0,
        win_class.lpszClassName,
        "Untited Window\n", // TODO: Add ability to change name
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        GetModuleHandle(NULL),
        0
    );

    SetWindowLongPtr(win->p.win, GWLP_USERDATA, (LONG_PTR) win);

    RECT client_rect;
    GetClientRect(win->p.win, &client_rect);
    win->w = client_rect.right - client_rect.left;
    win->h = client_rect.bottom - client_rect.top;

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

    int select_ret = select(x11_fd + 1, &fds, NULL, NULL, &tv);

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
    UINT_PTR timer_id = SetTimer(win->p.win, 1, timeout_ms, NULL);
    if (!timer_id) {
        win->event = EVENT_ERR;
        return;
    }

    MSG msg;
    if (GetMessage(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    } else win->event = EVENT_ERR;

    KillTimer(NULL, timer_id);
#endif
}

void close_win(Win win) {
#ifdef __linux__
    XCloseDisplay(win.p.display);
#elif _WIN32
    assert(!"Unimplemented");
#endif
}

#endif // GRAPHICS_IMPL_GUARD
#endif // GRAPHICS_IMPL
