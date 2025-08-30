#include <Windows.h>

#include <stdlib.h>
#include <stdint.h>

int bitmap_w, bitmap_h;
BITMAPINFO bitmap_info;
void *bitmap;

void resize_dib_section(int w, int h) {
    if (bitmap) {
        free(bitmap);
    }

    bitmap_w = w;
    bitmap_h = h;

    bitmap_info = (BITMAPINFO) {
        .bmiHeader = {
            .biSize = sizeof(bitmap_info.bmiHeader),
            .biWidth = w,
            .biHeight = -h,
            .biPlanes = 1,
            .biBitCount = 32,
        }
    };

    bitmap = calloc(w * h, 4);

    uint32_t *pixels = bitmap;
    for (int i = 0; i < w * h; i++) {
        uint8_t r = 255 * i / (w * h);
        pixels[i] |= r;
    }
}

void update_window(HDC ctx, RECT *win_rect, int x, int y, int w, int h) {
    int win_w = win_rect->right - win_rect->left,
        win_h = win_rect->bottom - win_rect->top;

    StretchDIBits(
        ctx,
        // x, y, w, h,
        // x, y, w, h,
        0, 0, bitmap_w, bitmap_h,
        0, 0, win_w, win_h,
        bitmap,
        &bitmap_info,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

LRESULT main_win_cb(HWND win, UINT msg, WPARAM hv, LPARAM vv) {
    LRESULT ret = 0;

    RECT client_rect;
    GetClientRect(win, &client_rect);
    int w = client_rect.right - client_rect.left,
        h = client_rect.bottom - client_rect.top;

    switch (msg) {
        case WM_SIZE: {
            resize_dib_section(w, h);
        } break;
        case WM_DESTROY: PostQuitMessage(0); break; // TODO: Handle this as error?
        case WM_CLOSE: PostQuitMessage(0); break;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC ctx = BeginPaint(win, &paint);
            update_window(
                ctx,
                &client_rect,
                paint.rcPaint.left,
                paint.rcPaint.top,
                paint.rcPaint.right - paint.rcPaint.left,
                paint.rcPaint.bottom - paint.rcPaint.top
            );
            EndPaint(win, &paint);
        } break;
        default: {
            ret = DefWindowProc(win, msg, hv, vv);
        } break;
    }

    return ret;
}

int main() {
    OutputDebugStringA("Yo\n");
    WNDCLASS win_class = {
        .style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = main_win_cb,
        .hInstance = GetModuleHandle(NULL),
        .lpszClassName = "wallpaperworks",
    };
    RegisterClassA(&win_class); // TODO: Check for errors.

    HWND win = CreateWindowExA(
        0,
        win_class.lpszClassName,
        "Yo!!!!!!!\n",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        0,
        0,
        GetModuleHandle(NULL),
        0
    ); // TODO: Check for errors

    MSG msg;
    while (1) {
        BOOL ret = GetMessageA(&msg, NULL, 0, 0);
        if (ret == 0) break;
        if (ret < 0) // TODO: Check for errors.

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
