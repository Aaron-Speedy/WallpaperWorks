#define DS_IMPL
#include "ds.h"

#include <time.h>

#define err(...) do { \
  fprintf(stderr, "Error: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  exit(1); \
} while (0);

#include <math.h>
#include <time.h>

#include "../third_party/libwebp/src/webp/decode.h"

#define GRAPHICS_IMPL
#include "graphics.h"

#define IMAGE_IMPL
#include "image.h"

#define FONT_IMPL
#include "font.h"

#include <curl/curl.h>

typedef struct {
    Arena *perm;
    s8 s;
} S8ArenaPair;

size_t curl_get_data(char *buf, size_t item_len, size_t item_num, void *data) {
    size_t n = item_len * item_num;
    S8ArenaPair *into = (S8ArenaPair *)(data);

    char *r = new(into->perm, char, n);
    if (!into->s.buf) into->s.buf = (u8 *) r;
    memcpy(r, buf, n);
    into->s.len += n;

    return n;
}

s8 download(Arena *perm, CURL *curl, s8 url) {
    url = s8_newcat(perm, url, s8("\0"));

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_get_data);

    S8ArenaPair data = { .perm = perm, };

    curl_easy_setopt(curl, CURLOPT_URL, (char *) url.buf);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &data);

    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) err("Failed to download resource: %s.", curl_easy_strerror(result));

    return data.s;
}

int main() {
    Arena draw_arena = new_arena(1 * GiB);
    Arena perm = new_arena(1 * GiB);

    CURL *curl = curl_easy_init(); 
    if (!curl) err("Failed to initialize libcurl.");

    Win win = get_root_win();

    Image background = {0};
    Image screen = new_img(&perm, (Image) { .w = win.w, .h = win.h, });

    connect_img_to_win(&win, screen.packed, screen.w, screen.h);

    FFont font = {
        .path = "./resources/Mallory/Mallory/Mallory Medium.ttf",
        .pt = 100,
    };
    load_font(&font, win.dpi_x, win.dpi_y);

    float time_x = 0.1, time_y = 0.25;
    int draw_timeout_ms = 1000, draw_init = 0;
    time_t new_wallpaper_timeout_s = 10, new_wallpaper_init = 0;
    time_t prev_time = time(NULL);
    Image prev_bb = {0};

    while (1) {
        WinEvent e = next_event_timeout(&win, draw_init);
        draw_init = draw_timeout_ms;

        if (!e.type) continue;

        time_t cur_time = time(NULL);
        bool redraw = false;

        // Download and set random background

        if (cur_time - prev_time >= new_wallpaper_init) {
            new_wallpaper_init = new_wallpaper_timeout_s;
            draw_arena.len = 0;
            redraw = true;

            Image img = {0};
            u8 *img_ec = 0;

            {
                Arena scratch = draw_arena;

                s8 base = s8("https://infotoast.org/images/");

                u64 n = s8_to_u64(download(
                    &scratch,
                    curl,
                    s8_newcat(&scratch, base, s8("num.txt"))
                ));

                s8 name = u64_to_s8(&scratch, rand() % (n + 1), 0);
                s8_modcat(&scratch, &name, s8(".webp"));

                s8 data = download(
                    &scratch,
                    curl,
                    s8_newcat(&scratch, base, name)
                );

                WebPGetInfo(data.buf, data.len, &img.w, &img.h);
                printf("%dx%d\n", img.w, img.h);

                img_ec = WebPDecodeRGB(data.buf, data.len, &img.w, &img.h);
            }

            img = new_img(&draw_arena, img);

            for (int x = 0; x < img.w; x++) {
                for (int y = 0; y < img.h; y++) {
                    int i = img_at(img, x, y);
                    img.buf[i].c[0] = img_ec[i * 3 + 0];
                    img.buf[i].c[1] = img_ec[i * 3 + 1];
                    img.buf[i].c[2] = img_ec[i * 3 + 2];
                    pack_color(img, x, y);
                }
            }

            WebPFree(img_ec);

            background = rescale_img(&draw_arena, img, win.w, win.h);
        }

        // Draw date and time
        {
            Arena scratch  = draw_arena;

            struct tm *lt = localtime(&cur_time);

            s8 hour = u64_to_s8(&scratch, lt->tm_hour, 2);
            s8 colon = s8_copy(&scratch, s8(":"));
            s8 minute = u64_to_s8(&scratch, lt->tm_min, 2);
            // s8 space = s8_copy(&scratch, s8(" "));
            // s8 ampm = s8_copy(&scratch, lt->tm_hour >= 12 ? s8("PM") : s8("AM"));

            s8 time_str = {
                .buf = hour.buf,
                .len = hour.len + colon.len + minute.len
                       // + space.len + ampm.len,
            };

            if (redraw) place_img(screen, background, 0.0, 0.0);
            else place_img(screen, prev_bb, prev_bb.x, prev_bb.y);
            prev_bb = draw_text(
                screen,
                &font,
                time_str,
                time_x, time_y,
                1.0, 1.0, 1.0
            );
            prev_bb.buf = background.buf;
            prev_bb.packed = background.packed;
        }

        draw_to_win(win);
    }

    close_win(win);
    curl_easy_cleanup(curl);

    return 0;
}
