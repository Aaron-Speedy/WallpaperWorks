#define GRAPHICS_IMPL
#include "src/graphics.h"

#define DS_IMPL
#include "src/ds.h"

int main() {
    Win win = {0};
    get_bg_win(&win);

    u32 color = 0xFF;

    while (1) {
        next_event_timeout(&win, 1000);
        switch (win.event) {
            case EVENT_QUIT: goto end;
            case EVENT_TIMEOUT: color = 0xFFFF0000; break;
            // default: assert(!"Unhandled event");
        }
        for (int i = 0; i < win.w * win.h; i++) {
            win.buf[i] = color;
        }

        draw_to_win(win);
    }

    end:
    return 0;
}
