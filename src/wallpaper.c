#define HAVE_GETTIMEOFDAY
#include <time.h>
#include <sys/time.h>
#include <math.h>

#include <curl/curl.h>
#include <pthread.h>

#define DS_IMPL
#include "ds.h"

#define err(...) do { \
  fprintf(stderr, "Error: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  exit(1); \
} while (0);

#define warning(...) do { \
  fprintf(stderr, "Warning: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
} while (0);

#include "../third_party/libwebp/src/webp/decode.h"

#define GRAPHICS_IMPL
#include "graphics.h"

#define IMAGE_IMPL
#include "image.h"

#define FONT_IMPL
#include "font.h"

#define CACHE_IMPL
#include "cache.h"

#define NETWORKING_IMPL
#include "networking.h"

#include "recs.h"

typedef struct {
    bool redraw;
    Image img;
} Background;

Background background = {0};
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *background_thread() {
    Arena perm = new_arena(1 * GiB),
          perm_2 = new_arena(1 * KiB);

    CURL *curl = curl_easy_init(); 
    if (!curl) err("Failed to initialize libcurl.");

    curl_easy_setopt(curl, CURLOPT_CAINFO, "./curl-ca-bundle.crt");

    s8 cache_dir = get_or_make_cache_dir(&perm, s8("wallpaper"));
    int timeout_s = 60;
    bool initial = true;
    pthread_mutex_lock(&lock); // initial lock

    while (true) {
        Arena scratch = perm, e_scratch = perm_2;

        time_t a_time = time(NULL);
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
                network_mode = data.buf != NULL;

                if (network_mode) {
                    s8_write_to_file(scratch, path, data);
                } else data = s8_read_file(&scratch, e_scratch, path);

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

                img_data = s8_read_file(&scratch, e_scratch, path);

                // File was not found
                if (img_data.len <= 0) {
                    if (!network_mode) continue; // try another image

                    img_data = download(&scratch, curl, url);
                    network_mode = img_data.buf != NULL;

                    if (!network_mode) continue; // try another image;

                    s8_write_to_file(scratch, path, img_data);
                }
            }

            Image img = {0};
            WebPGetInfo(img_data.buf, img_data.len, &img.w, &img.h);
            printf("%dx%d\n", img.w, img.h);

            Image decoded = (Image) {
                .buf = (Color *) WebPDecodeRGBA(img_data.buf, img_data.len, &img.w, &img.h),
                .w = img.w,
                .h = img.h,
                .alloc_w = img.w,
            };

            img = new_img(&scratch, img);

            for (int x = 0; x < img.w; x++) {
                for (int y = 0; y < img.h; y++) {
                    Color d = *img_at(&decoded, x, y);
                    *img_at(&img, x, y) = (Color) {
                        .c[COLOR_R] = d.c[0],
                        .c[COLOR_G] = d.c[1],
                        .c[COLOR_B] = d.c[2],
                    };
                }
            }

            WebPFree(decoded.buf);

            b = rescale_img(
                NULL,
                img,
                background.img.w,
                background.img.h
            );

            break;
        }

        int wait = timeout_s - (time(NULL) - a_time);
        if (wait > 0 && !initial) sleep(wait);

        if (!initial) pthread_mutex_lock(&lock); /* if it's on the first
                                                    iteration, it's already
                                                    locked. */
            free(background.img.buf);
            background.img = b;
            background.redraw = true;
        pthread_mutex_unlock(&lock);

        initial = false;
    }

    curl_easy_cleanup(curl);
}

int main() {
    srand(time(0));

    Arena perm = new_arena(1 * GiB);
    Win win = {0};
    get_bg_win(&win);
    show_systemtray_icon(&win, ICON_ID, "Wallpaper");

    background.img.w = win.w;
    background.img.h = win.h;

    pthread_t thread;
    if (pthread_create(&thread, NULL, background_thread, NULL)) {
        err("Failed to create background thread.");
    }

    FFont time_font = {
        .path = "font.ttf",
        .pt = 100,
    };
    load_font(&time_font, win.dpi_x, win.dpi_y);

    FFont date_font = {
        .path = time_font.path,
        .pt = time_font.pt * 0.2,
    };
    load_font(&date_font, win.dpi_x, win.dpi_y);

    float time_x = 0.03, time_y = 0.05, // from the bottom-right
          date_x = 0.0, date_y = 0.05, // from the top-left of time
          time_shadow_x = 0.002, time_shadow_y = 0.002,
          date_shadow_x = 0.002, date_shadow_y = 0.002; 
    Image prev_bound = {0}; // previous altered bounding box

    while (1) {
        pthread_mutex_lock(&lock);
        pthread_mutex_unlock(&lock);

        Arena scratch  = perm;
        time_t ftime = time(NULL) + 1;

        struct tm *lt = localtime(&ftime);

        s8 time_str = {0};
        {
            s8 a0 = u64_to_s8(&scratch, lt->tm_hour, 2);
                    s8_copy(&scratch, s8(":"));
            s8 al = u64_to_s8(&scratch, lt->tm_min, 2);
            time_str = s8_masscat(scratch, a0, al);
        }

        Image time_bound = get_bound_of_text(&time_font, time_str);

        s8 date_str = {0};
        {
            s8 months[] = {
                s8("January"), s8("February"), s8("march"), s8("April"),
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

        Image date_bound = get_bound_of_text(&date_font, date_str);

        // TODO: win.resized
        pthread_mutex_lock(&lock);
            Image screen = (Image) {
                 .buf = win.buf, .w = win.w, .h = win.h, .alloc_w = win.w,
            };
             
            if (background.redraw) {
                place_img(screen, background.img, 0.0, 0.0);
                background.redraw = false;
            } else place_img(screen, prev_bound, prev_bound.x, prev_bound.y);

            time_bound = draw_text_shadow(
                 screen,
                 &time_font,
                 time_str,
                 screen.w - 1 - screen.w * time_x - time_bound.w,
                 screen.h - 1 - screen.h * time_y,
                 255, 255, 255,
                 time_shadow_x * screen.w, time_shadow_y * screen.h,
                 0, 0, 0
            );

            date_bound = draw_text_shadow(
                screen,
                &date_font,
                date_str,
                time_bound.x + screen.w * date_x,
                time_bound.y - screen.h * date_y,
                255, 255, 255,
                date_shadow_x * screen.w, date_shadow_y * screen.h,
                0, 0, 0
            );

            prev_bound = combine_bound(time_bound, date_bound);
            prev_bound.buf = background.img.buf;
        pthread_mutex_unlock(&lock);

        struct timeval time_val;
        gettimeofday(&time_val, NULL);

        next_event_timeout(
            &win,
            1000 - (time_val.tv_usec / 1000) // update exactly on the second
        );
        switch (win.event) {
            case EVENT_QUIT: goto end;
            default: break;
        }

        draw_to_win(win);
    }

    close_win(win);

end:
    return 0;
}
