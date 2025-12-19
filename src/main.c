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
#include <dirent.h>

typedef struct {
    bool redraw, initial;
    Image img;
} Background;

#include "config.h"

FFontLib font_lib;
FFont time_font;
FFont date_font;
Background background = { .initial = true, };
Image scaled_background = {0};
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

s8 get_random_image(Arena *perm, CURL *curl, s8 cache_dir) {
    s8 base = s8("https://infotoast.org/images");
    bool network_mode = true;

    u64 n = 0;
    {
        s8 f = s8("/num.txt");
        s8 url = s8_newcat(perm, base, f);
        s8 path = s8_newcat(perm, cache_dir, f);

        DownloadResponse response = download(perm, curl, url);
        network_mode = response.data.len != 0 && response.code == 200;

        if (network_mode) {
            s8_write_to_file(path, response.data);
            n = s8_to_u64(response.data);
        }
    }

    s8 img_data = {0};
    if (network_mode) {
        int times_tried = 0;
try_downloading_another_one:
        times_tried += 1;
        if (times_tried >= 10) goto pick_random_downloaded_image;

        s8 a0 = s8_copy(perm, s8("/"));
                u64_to_s8(perm, rand() % (n + 1), 0);
        s8 al = s8_copy(perm, s8(".webp"));
        s8 name = s8_masscat(*perm, a0, al);

        s8_print(name);
        printf("\n");

        s8 url = s8_newcat(perm, base, name);
        s8 path = s8_newcat(perm, cache_dir, name);

        if (cache_dir.len > 0) {
            img_data = s8_read_file(perm, path);
        }

        // File was not found
        if (img_data.len <= 0) {
            if (!network_mode) goto pick_random_downloaded_image;

            DownloadResponse response = download(perm, curl, url);
            if (response.code != 200) goto try_downloading_another_one;
            img_data = response.data;
            network_mode = img_data.len != 0 && response.code == 200;

            if (!network_mode) goto pick_random_downloaded_image;

            s8_write_to_file(path, img_data);
        }
    } else { pick_random_downloaded_image: {}
        printf("You are in offline mode for now.\n");

        struct {
            struct dirent **buf;
            ssize len;
        } names = {0};

        {
            DIR *dirp = opendir(s8_newcat(perm, cache_dir, s8("\0")).buf);
            if (!dirp) return (s8) {0};

            while (true) {
                struct dirent *d = readdir(dirp);
                if (!d) break;
                if (!strcmp(d->d_name, ".") ||
                    !strcmp(d->d_name, "..") ||
                    !strcmp(d->d_name, "num.txt")) continue;
                names.buf = names.buf ? names.buf : new(perm, d, 1);
                names.buf[names.len++] = d;
            }

            if (!names.len) return (s8) {0};
        }

        while (true) {
            char *name = names.buf[rand() % names.len]->d_name;
            char *pattern = "*.webp";
            bool ok = true;

            for (char *c = name, *p = pattern;;) {
                if (!ok) break;
                if (!(*c && *p)) {
                    if (*c != *p) ok = false;
                    break;
                }

                switch (*p) {
                case '*': {
                    if (!p[1] || *c != p[1]) *c++;
                    else *p++;
                } break;
                default: {
                    if (*c == *p) { *c++; *p++; }
                    else ok = false;
                }
                }
            }

            if (ok) {
                s8 a0 = s8_copy(perm, cache_dir);
                    s8_copy(perm, s8("/"));
                s8 al = s8_copy(perm, (s8) { .buf = name, .len = strlen(name)});
                s8 path = s8_masscat(*perm, a0, al);

                img_data = s8_read_file(perm, path);
                printf("Offline image: ");
                s8_print(path);
                printf("\n");
                break;
            }
        }
    }

    return img_data;
}

void *background_thread() {
    srand(time(0));

    Arena perm = new_arena(1 * GiB);

    CURL *curl = curl_easy_init(); 
    if (!curl) err("Failed to initialize libcurl.");

#ifdef _WIN32
    curl_easy_setopt(curl, CURLOPT_CAINFO, "./curl-ca-bundle.crt");
#endif

    s8 cache_dir = get_or_make_cache_dir(&perm, s8(APP_NAME));
    bool no_cache_support = 0;
    if (cache_dir.len <= 0) {
        no_cache_support = true;
        warning(
            "Could not get system cache directory. Disabling cache support."
        );
    }
    int timeout_s = 1;
    bool initial = true;

    while (true) {
        Arena scratch = perm;

        time_t a_time = time(0);
        Image b = {0};

        s8 img_data = get_random_image(&scratch, curl, cache_dir);
        if (!img_data.buf) err("No images are saved in cache. You have to connect to the internet to run this application.");

        // TODO: check this
        WebPGetInfo(
            img_data.buf, img_data.len,
            &b.w, &b.h
        );
        printf("Decoding image of size %dx%d: \n", b.w, b.h);

        Image decoded = {
            .alloc_w = b.w,
            .w = b.w,
            .h = b.h,
        };
        decoded.buf = (Color *) WebPDecodeRGBA(
            img_data.buf, img_data.len,
            &b.w, &b.h
        ),

        b = new_img(0, b);

        for (int x = 0; x < b.w; x++) {
            for (int y = 0; y < b.h; y++) {
                Color d = *img_at(&decoded, x, y);
                *img_at(&b, x, y) = color(d.c[0], d.c[1], d.c[2], 255);
            }
        }

        WebPFree(decoded.buf);

        int wait = timeout_s - (time(0) - a_time);
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
    if (pthread_create(&thread, 0, background_thread, 0)) {
        err("Failed to create background thread.");
    }

    // TODO: update the font sizes whenever the screen resizes

    int min_dim = ctx->screen->w < ctx->screen->h ? ctx->screen->w : ctx->screen->h;

    font_lib = init_ffont();
    load_font(
        &time_font, font_lib,
        raw_font_buf, raw_font_buf_len,
        1,
        ctx->dpi, ctx->dpi
    );
    FT_Set_Pixel_Sizes(
        time_font.face,
        min_dim * time_size,
        min_dim * time_size
    );

    load_font(
        &date_font, font_lib,
        raw_font_buf, raw_font_buf_len,
        1,
        ctx->dpi, ctx->dpi
    );
    FT_Set_Pixel_Sizes(
        date_font.face,
        min_dim * date_size,
        min_dim * date_size
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
        time_t ftime = time(0) + 1; // TODO: merge this magic number with the event timeout
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
                0,
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
