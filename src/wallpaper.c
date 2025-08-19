#include <time.h>
#include <pthread.h>
#include <math.h>
#include <curl/curl.h>

#define DS_IMPL
#include "ds.h"

#define err(...) do { \
  fprintf(stderr, "Error: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  exit(1); \
} while (0);

#include "../third_party/libwebp/src/webp/decode.h"

#define GRAPHICS_IMPL
#include "graphics.h"

#define IMAGE_IMPL
#include "image.h"

#define FONT_IMPL
#include "font.h"

typedef struct {
    Arena *perm;
    s8 s;
} S8ArenaPair;

typedef struct {
    bool lock;
    bool redraw;
    Image img;
} Background;

Background background = {0};

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

void *background_thread() {
    int timeout_s = 60;

    Arena frame = new_arena(1 * GiB);

    CURL *curl = curl_easy_init(); 
    if (!curl) err("Failed to initialize libcurl.");

    bool initial = true;

    while (true) {
        time_t a_time = time(NULL);
        u8 *buf = NULL;
        Image img = {0};

        {
            Arena scratch = frame;

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
            buf = WebPDecodeRGB(data.buf, data.len, &img.w, &img.h);
        }

        int wait = timeout_s - (time(NULL) - a_time);
        if (wait > 0 && !initial) sleep(wait);

        while (background.lock) {}

        background.lock = true;
        {
            frame.len = 0;

            img = new_img(&frame, img);

            for (int x = 0; x < img.w; x++) {
                for (int y = 0; y < img.h; y++) {
                    int i = img_at(img, x, y);
                    img.buf[i].c[0] = buf[i * 3 + 0];
                    img.buf[i].c[1] = buf[i * 3 + 1];
                    img.buf[i].c[2] = buf[i * 3 + 2];
                    pack_color(img, x, y);
                }
            }

            WebPFree(buf);

            background.img = rescale_img(
                &frame,
                img,
                background.img.w,
                background.img.h
            );
        }
        background.lock = false;
        background.redraw = true;
        initial = false;
    }

    curl_easy_cleanup(curl);
}

int main() {
    srand(time(0));

    Arena perm = new_arena(1 * GiB);
    Win win = get_root_win();

    background.img.w = win.w;
    background.img.h = win.h;

    pthread_t thread;
    if (pthread_create(&thread, NULL, background_thread, NULL)) {
        err("Failed to create background thread.");
    }

    Image screen = new_img(&perm, (Image) { .w = win.w, .h = win.h, });
    connect_img_to_win(&win, screen.packed, screen.w, screen.h);

    FFont font = {
        .path = "./resources/Mallory/Mallory/Mallory Medium.ttf",
        .pt = 100,
    };
    load_font(&font, win.dpi_x, win.dpi_y);

    float time_x = 0.1, time_y = 0.25;
    int draw_timeout_ms = 1000, draw_init = 0;
    Image prev_bb = {0}; // previous text bounding box

    while (1) {
        WinEvent e = next_event_timeout(&win, draw_init);
        draw_init = draw_timeout_ms;

        if (!e.type) continue; // error

        printf("%d\n", background.lock);
        while (background.lock) {}
        while (!background.img.buf) {}

        background.lock = true;
        {
            Arena scratch  = perm;
            time_t cur_time = time(NULL);

            struct tm *lt = localtime(&cur_time);

            // TODO: remove the need to specify names and repeat the order
            s8 hour = u64_to_s8(&scratch, lt->tm_hour, 2),
               colon = s8_copy(&scratch, s8(":")),
               minute = u64_to_s8(&scratch, lt->tm_min, 2),
               colon2 = s8_copy(&scratch, s8(":")),
               second = u64_to_s8(&scratch, lt->tm_sec, 2),
               // space = s8_copy(&scratch, s8(" ")),
               // ampm = s8_copy(&scratch, lt->tm_hour >= 12 ? s8("PM") : s8("AM")),
               time_str = {
                .buf = hour.buf,
                .len = hour.len + colon.len +
                       minute.len + colon2.len +
                       second.len,
            };

            if (background.redraw) {
                place_img(screen, background.img, 0.0, 0.0);
                background.redraw = false;
            } else place_img(screen, prev_bb, prev_bb.x, prev_bb.y);
            prev_bb = draw_text(
                screen,
                &font,
                time_str,
                time_x, time_y,
                255, 255, 255
            );
            prev_bb.buf = background.img.buf;
            prev_bb.packed = background.img.packed;
        }
        background.lock = false;

        draw_to_win(win);
    }

    close_win(win);

    return 0;
}
