#include <X11/Xlib.h>

#define DS_IMPL
#include "ds.h"

#include "libwebp/src/webp/decode.h"

typedef struct {
    Display *display;
    Window win;
    int screen;
    GC gc;
    XImage *img;
} Win;

Win open_win(int w, int h) {
    Win win = {0};

    win.display = XOpenDisplay(NULL);
    if (win.display == NULL) {
        fprintf(stderr, "Error: Failed to open display.\n");
        exit(1);
    }

    win.screen = DefaultScreen(win.display);
    win.win = XCreateSimpleWindow(win.display, RootWindow(win.display, win.screen), 0, 0, w, h, 1, 0, 0);
    XSelectInput(win.display, win.win, ExposureMask | KeyPressMask);
    XMapWindow(win.display, win.win);

    win.gc = DefaultGC(win.display, win.screen);

    return win;
}

void close_win(Win win) {
    XCloseDisplay(win.display);
}

bool draw_to_win(Win win) {
    if (win.img != NULL) {
        XPutImage(
            win.display, 
            win.win, win.gc, win.img,
            0, 0, 0, 0,
            win.img->width, win.img->height
        );
    }

    return XFlush(win.display);
}

void connect_img_to_win(Win *win, u32 *buf, int w, int h) {
    if (win->img != NULL) {
        fprintf(stderr, "Error: Image already connected to window.\n");
        exit(1);
    }

    win->img = XCreateImage(
        win->display,
        DefaultVisual(win->display, win->screen),
        24, ZPixmap, 0, (char *) buf, w, h, 32, 0
    );

    if (win->img == NULL) {
        fprintf(stderr, "Error: Could not connect image.\n");
        exit(1);
    }
}

int main() {
    Arena perm = new_arena(1 * GiB);
    Arena scratch = new_arena(1 * KiB);

    s8 data = read_file(&perm, scratch, s8("30.webp"));
    printf("%ld\n", data.len);

    int width, height;
    WebPGetInfo(data.buf, data.len, &width, &height);
    printf("%dx%d\n", width, height);

    u32 *img = malloc(3 * width * height);
    {
        u8 *pixels = WebPDecodeRGB(data.buf, data.len, &width, &height);
        for (int i = 0; i < width * height; i++) {
            img[i] = pixels[i * 3 + 2] | (pixels[i * 3 + 1] << 8) | (pixels[i * 3] << 16);
        }
    }

    Win win = open_win(width, height);

    // Enable image mode
    connect_img_to_win(&win, img, width, height);

    while (1) {
        XEvent e;
        XNextEvent(win.display, &e);
        if (e.type == Expose) {
            draw_to_win(win);
        }
    }

    close_win(win);

    return 0;
}
