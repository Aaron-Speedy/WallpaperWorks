#ifndef GRAPHICS_H
#define GRAPHICS_H

typedef struct {
    Display *display;
    Window win;
    int screen, dpi_x, dpi_y, w, h;
    GC gc;
    XImage *img;
} Win;

Win get_root_win();
void close_win(Win win);
bool draw_to_win(Win win);
void connect_img_to_win(Win *win, u32 *buf, int w, int h);

#endif // GRAPHICS_H

#ifdef GRAPHICS_IMPL
#ifndef GRAPHICS_IMPL_GUARD
#define GRAPHICS_IMPL_GUARD

Win get_root_win() {
    Win win = {0};

    win.display = XOpenDisplay(NULL);
    if (win.display == NULL) err("Failed to open display.");

    win.screen = DefaultScreen(win.display);
    win.win = DefaultRootWindow(win.display);
    XSelectInput(win.display, win.win, ExposureMask | KeyPressMask);
    XMapWindow(win.display, win.win);

    win.gc = DefaultGC(win.display, win.screen);

    win.w = XDisplayWidth(win.display, win.screen);
    win.h = XDisplayHeight(win.display, win.screen);
    win.dpi_x = win.w / ((float) DisplayWidthMM(win.display, win.screen) / 25.4);
    win.dpi_y = win.h / ((float) DisplayHeightMM(win.display, win.screen) / 25.4);

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
    if (win->img != NULL) err("The image you're trying to connect is already connected to a window.");

    win->img = XCreateImage(
        win->display,
        DefaultVisual(win->display, win->screen),
        24, ZPixmap, 0, (char *) buf, w, h, 32, 0
    );

    if (win->img == NULL) err("Failed to connect image.");
}

#endif // GRAPHICS_IMPL_GUARD
#endif // GRAPHICS_IMPL
