const float time_x = 0.04, time_y = 0.06, // from the bottom-right
      time_size = 0.18,
      time_shadow_x = 0.002, time_shadow_y = 0.002;

const float date_x = 0.04, date_y = time_y + 0.17, // from the bottom-right
      date_size = 0.04,
      date_shadow_x = 0.002, date_shadow_y = 0.002;

#define FONT_IMPL
#include "font.h"

#define CACHE_IMPL
#include "cache.h"

#define NETWORKING_IMPL
#include "networking.h"

#include "../third_party/libwebp/src/webp/decode.h"

#include "recs.h"
#include "../build/tmp/raw_font_buf.h"
#include "../build/tmp/app_name.h"

#include <pthread.h>

typedef struct {
    bool redraw, initial;
    Image img;
} Background;

FFontLib font_lib;
FFont time_font;
FFont date_font;
Background background = { .initial = true, };
Image scaled_background = {0};
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
u64 tick = 0;

void *background_thread() {
    srand(time(0));

    Arena perm = new_arena(1 * GiB);

    CURL *curl = curl_easy_init(); 
    if (!curl) err("Failed to initialize libcurl.");

#ifdef _WIN32
    curl_easy_setopt(curl, CURLOPT_CAINFO, "./curl-ca-bundle.crt");
#endif

    s8 cache_dir = get_or_make_cache_dir(&perm, s8(APP_NAME));
    if (cache_dir.len <= 0) {
        warning(
            "Could not get system cache directory. Disabling cache support."
        );
    }
    int timeout_s = 60;
    bool initial = true;

    while (true) {
        Arena scratch = perm;

        time_t a_time = time(0);
        Image b = {0};

        while (true) {
            s8 base = s8("https://infotoast.org/images");
            bool network_mode = true;

            u64 n = 0;
            {
                s8 f = s8("/num.txt");
                s8 url = s8_newcat(&scratch, base, f);
                s8 path = s8_newcat(&scratch, cache_dir, f);

                s8 data = download(&scratch, curl, url);
                network_mode = data.buf != 0;

                if (network_mode) {
                    s8_write_to_file(path, data);
                } else data = s8_read_file(&scratch, path);

                n = s8_to_u64(data);
            }

            s8 img_data = {0};
            {

                s8 a0 = s8_copy(&scratch, s8("/"));
                        u64_to_s8(&scratch, rand() % (n + 1), 0);
                s8 al = s8_copy(&scratch, s8(".webp"));
                s8 name = s8_masscat(scratch, a0, al);

                s8 url = s8_newcat(&scratch, base, name);
                s8 path = s8_newcat(&scratch, cache_dir, name);

                img_data = s8_read_file(&scratch, path);

                // File was not found
                if (img_data.len <= 0) {
                    if (!network_mode) continue; // try another image

                    img_data = download(&scratch, curl, url);
                    network_mode = img_data.buf != 0;

                    if (!network_mode) continue; // try another image;

                    s8_write_to_file(path, img_data);
                }
            }

            // TODO: check this
            WebPGetInfo(
                img_data.buf, img_data.len,
                &b.w, &b.h
            );
            printf("Decoding image of size %dx%d\n", b.w, b.h);

            Image decoded = {
                .alloc_w = b.w,
                .w = b.w,
                .h = b.h,
            };
            decoded.buf = (Color *) WebPDecodeRGBA(
                img_data.buf, img_data.len,
                &b.w, &b.h
            ),

            b = new_img(NULL, b);

            for (int x = 0; x < b.w; x++) {
                for (int y = 0; y < b.h; y++) {
                    Color d = *img_at(&decoded, x, y);
                    *img_at(&b, x, y) = color(d.c[0], d.c[1], d.c[2], 255);
                }
            }

            WebPFree(decoded.buf);

            break;
        }

        int wait = timeout_s - (time(NULL) - a_time);
        if (wait > 0 && !initial) sleep(wait);

        pthread_mutex_lock(&lock);
            free(background.img.buf);
            background = (Background) {
                .img = b,
                .redraw = true,
            };
        pthread_mutex_unlock(&lock);

        initial = false;
    }

    curl_easy_cleanup(curl);
}

void start(Context *ctx) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, background_thread, NULL)) {
        err("Failed to create background thread.");
    }

    // TODO: update the font sizes whenver the screen size changes

    int d = ctx->screen->w < ctx->screen->h ? ctx->screen->w : ctx->screen->h;

    font_lib = init_ffont();
    load_font(
        &time_font, font_lib,
        raw_font_buf, raw_font_buf_len,
        1,
        ctx->dpi, ctx->dpi
    );
    FT_Set_Pixel_Sizes(
        time_font.face,
        d * time_size,
        d * time_size
    );

    load_font(
        &date_font, font_lib,
        raw_font_buf, raw_font_buf_len,
        1,
        ctx->dpi, ctx->dpi
    );
    FT_Set_Pixel_Sizes(
        date_font.face,
        d * date_size,
        d * date_size
    );

    while (true) {
        bool stop = 0;
        pthread_mutex_lock(&lock);
            stop = background.img.buf != 0;
        pthread_mutex_unlock(&lock);
        if (stop) break;
    }
}

void app_loop(Context *ctx) {
    Image *screen = ctx->screen;

    new_static_arena(scratch, 500);

    s8 time_str = {0}, date_str = {0};
    {
        time_t ftime = time(NULL) + 1;
        struct tm *lt = localtime(&ftime);

        {
            s8 a0 = u64_to_s8(&scratch, lt->tm_hour, 2);
                    s8_copy(&scratch, s8(":"));
            s8 al = u64_to_s8(&scratch, lt->tm_min, 2);
            time_str = s8_masscat(scratch, a0, al);
        }

        {
            s8 months[] = {
                s8("January"), s8("February"), s8("March"), s8("April"),
                s8("May"), s8("June"), s8("July"), s8("August"),
                s8("September"), s8("October"), s8("November"), s8("December"),
            };
            s8 days[] = {
                s8("Sunday"), s8("Monday"), s8("Tuesday"), s8("Wednesday"),
                s8("Thursday"), s8("Friday"), s8("Saturday"),
            };

            s8 a0 = s8_copy(&scratch, days[lt->tm_wday]);
                    s8_copy(&scratch, s8(", "));
                    s8_copy(&scratch, months[lt->tm_mon]);
                    s8_copy(&scratch, s8(" "));
            s8 al = u64_to_s8(&scratch, lt->tm_mday, 0);
            date_str = s8_masscat(scratch, a0, al);
        }
    }

    pthread_mutex_lock(&lock);
        if (background.redraw) {
            free(scaled_background.buf);
            scaled_background = rescale_img(
                NULL,
                background.img,
                screen->w, screen->h
            );
            background.redraw = false;
        }
    pthread_mutex_unlock(&lock);

    place_img(*screen, scaled_background, 0, 0, 0);

    Image time_bound = get_bound_of_text(&time_font, time_str);
    time_bound = draw_text_shadow(
        *screen,
        &time_font,
        time_str,
        screen->w - 1 - screen->w * time_x - time_bound.w,
        screen->h - 1 - screen->h * time_y,
        255, 255, 255,
        time_shadow_x * screen->w, time_shadow_y * screen->h,
        0, 0, 0,
        true
    );
    Image date_bound = get_bound_of_text(&date_font, date_str);
    date_bound = draw_text_shadow(
        *screen,
        &date_font,
        date_str,
        screen->w - 1 - screen->w * date_x - date_bound.w,
        screen->h - 1 - screen->h * date_y,
        255, 255, 255,
        date_shadow_x * screen->w, date_shadow_y * screen->h,
        0, 0, 0,
        true
    );
}
