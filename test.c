#define GRAPHICS_IMPL
#include "src/graphics.h"

#define DS_IMPL
#include "src/ds.h"

#include <stdlib.h>
#include <stdint.h>

int main() {
    Win win = {0};
    get_bg_win(&win);

    while (1) {
        MSG msg;
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto end;
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            for (int i = 0; i < win.w * win.h; i++) {
                win.buf[i] = 255;
            }
        }

        draw_to_win(win);
    }

    end:
    return 0;
}
