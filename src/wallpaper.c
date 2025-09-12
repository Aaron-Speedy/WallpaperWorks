#define HAVE_GETTIMEOFDAY
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <pthread.h>

#include <curl/curl.h>

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

#define print(format, ...) do { \
    char buf[1024]; \
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, format, __VA_ARGS__); \
    MessageBox(NULL, buf, "Debug Message", MB_OK); \
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
    pthread_mutex_lock(&lock); // initial lock

    Arena perm = new_arena(1 * GiB),
          perm_2 = new_arena(1 * KiB);

    CURL *curl = curl_easy_init(); 
    if (!curl) err("Failed to initialize libcurl.");

    curl_easy_setopt(curl, CURLOPT_CAINFO, "./curl-ca-bundle.crt");

    s8 cache_dir = get_or_make_cache_dir(&perm, s8("wallpaper"));
    int timeout_s = 60;
    bool initial = true;

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

            b = (Image) {0};
            WebPGetInfo(
                img_data.buf, img_data.len,
                &b.w, &b.h
            );
            printf("%dx%d\n", b.w, b.h);

            Image decoded = (Image) {
                .buf = (Color *) WebPDecodeRGBA(
                    img_data.buf, img_data.len,
                    &b.w, &b.h
                ),
                .w = b.w,
                .h = b.h,
                .alloc_w = b.w,
            };

            b = new_img(NULL, b);

            for (int x = 0; x < b.w; x++) {
                for (int y = 0; y < b.h; y++) {
                    Color d = *img_at(&decoded, x, y);
                    *img_at(&b, x, y) = (Color) {
                        .c[COLOR_R] = d.c[0],
                        .c[COLOR_G] = d.c[1],
                        .c[COLOR_B] = d.c[2],
                    };
                }
            }

            WebPFree(decoded.buf);

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

typedef struct {
    Monitors monitors;
    Win wins[MAX_PLATFORM_MONITORS];
    FFont time_fonts[MAX_PLATFORM_MONITORS];
    FFont date_fonts[MAX_PLATFORM_MONITORS];
    Image imgs[MAX_PLATFORM_MONITORS];
    FFontLib font_lib;
    char *font_path;
    float time_pt, date_pt;
    float time_size, date_size;
    HWND worker_w;
} Wins;

void replace_wins(Wins *wins, Monitors *m) {
    for (int i = 0; i < wins->monitors.len; i++) {
        Win *w = &wins->wins[i];
        w->is_bg = false; // to make sure the wallpaper isn't cleared
        free_font(wins->time_fonts[i]);
        free_font(wins->date_fonts[i]);
        close_win(w);
    }

    wins->monitors.len = m->len;
    for (int i = 0; i < wins->monitors.len; i++) {
        Win *w = &wins->wins[i];
        wins->monitors.buf[i] = m->buf[i];

        new_win(w, 500, 500);
        make_win_bg(w, wins->worker_w);
        move_win_to_monitor(w, m->buf[i]);
        _fill_working_area(w);

        int d = w->w < w->h ? w->w : w->h;

        load_font(
            &wins->time_fonts[i],
            wins->font_lib,
            wins->font_path,
            wins->time_pt,
            w->dpi_x, w->dpi_y
        );
        FT_Set_Pixel_Sizes(
            wins->time_fonts[i].face,
            d * wins->time_size,
            d * wins->time_size
        );
        load_font(
            &wins->date_fonts[i],
            wins->font_lib,
            wins->font_path,
            wins->date_pt,
            w->dpi_x, w->dpi_y
        );
        FT_Set_Pixel_Sizes(
            wins->date_fonts[i].face,
            d * wins->date_size,
            d * wins->date_size
        );

        show_win(*w);
    }
}

int main() {
    srand(time(0));

    Arena perm = new_arena(1 * GiB);

    Wins wins = {
        .worker_w = _make_worker_w(),
        .font_lib = init_ffont(),
        .font_path = "./font.ttf",
        .time_pt = 130,
        .date_pt = 26,
        .time_size = 0.2,
        .date_size = 0.05,
    };

    {
        Monitors m;
        collect_monitors(&m);
        replace_wins(&wins, &m);
    }

    pthread_t thread;
    if (pthread_create(&thread, NULL, background_thread, NULL)) {
        err("Failed to create background thread.");
    }

    float dt_x = 0.04, dt_y = 0.06, // from the bottom-right
          date_x = 0.0, date_y = 0.05, // from the top-left of time
          time_shadow_x = 0.002, time_shadow_y = 0.002,
          date_shadow_x = 0.002, date_shadow_y = 0.002; 

    while (1) {
        pthread_mutex_lock(&lock);
        pthread_mutex_unlock(&lock);

        Arena scratch = perm;
        time_t ftime = time(NULL) + 1;

        struct tm *lt = localtime(&ftime);

        s8 time_str = {0};
        {
            s8 a0 = u64_to_s8(&scratch, lt->tm_hour, 2);
                    s8_copy(&scratch, s8(":"));
            s8 al = u64_to_s8(&scratch, lt->tm_min, 2);
            time_str = s8_masscat(scratch, a0, al);
        }

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

        while (!background.img.buf);
        pthread_mutex_lock(&lock);
        for (int i = 0; i < wins.monitors.len; i++) {
            Win *win = &wins.wins[i];
            FFont *time_font = &wins.time_fonts[i],
                  *date_font = &wins.date_fonts[i];
            Image *bg_img = &wins.imgs[i];

            if (background.redraw) {
                Image new = rescale_img(
                    NULL,
                    background.img,
                    win->w, win->h
                );
                free(bg_img->buf);
                *bg_img = new;
            }

            Image screen = (Image) {
                 .buf = win->buf,
                 .w = win->w, .h = win->h,
                 .alloc_w = win->w,
            };
         
            place_img(screen, *bg_img, 0, 0, 0);

            Image dt_box = { .w = screen.w, .h = screen.h, };
            dt_box = new_img(&scratch, dt_box);

            Image dt_bound = get_bound_of_text(time_font, time_str);
            dt_bound = draw_text_shadow(
                 dt_box,
                 time_font,
                 time_str,
                 screen.w / 2, screen.h / 2 + dt_bound.h,
                 // screen.w - 1 - screen.w * time_x - time_bound.w,
                 // screen.h - 1 - screen.h * time_y,
                 255, 255, 255,
                 time_shadow_x * screen.w, time_shadow_y * screen.h,
                 0, 0, 0,
                 false
            );

            // Image date_bound = get_bound_of_text(date_font, date_str);
            dt_bound = combine_bound(draw_text_shadow(
                dt_box,
                date_font,
                date_str,
                dt_bound.x + screen.w * date_x,
                dt_bound.y - screen.h * date_y,
                255, 255, 255,
                date_shadow_x * screen.w, date_shadow_y * screen.h,
                0, 0, 0,
                false
            ), dt_bound);

            dt_box.w = dt_bound.w;
            dt_box.h = dt_bound.h;
            dt_box.x = dt_bound.x;
            dt_box.y = dt_bound.y;

            place_img(
                screen,
                dt_box,
                screen.w - 1 - screen.w * dt_x - dt_bound.w,
                screen.h - 1 - screen.h * dt_y - dt_bound.h,
                true
            );
        }
        if (background.redraw) background.redraw = false;
        pthread_mutex_unlock(&lock);

        struct timeval time_val;
        gettimeofday(&time_val, NULL);

        wait_event_timeout(
            &wins.wins[0],
            1000 - (time_val.tv_usec / 1000) // update exactly on the second
        );

        for (int i = 0; i < wins.monitors.len; i++) {
            Win *win = &wins.wins[i];

            while (get_next_event(win)) {
                switch (win->event.type) {
                case EVENT_QUIT: goto end;
                case EVENT_SYS_TRAY: {
                    int c = win->event.click;
                    if (c == CLICK_L_UP ||
                        c == CLICK_M_UP ||
                        c == CLICK_R_UP) {
                        goto end;
                    }
                } break;
                default: break;
                }
            }

            draw_to_win(*win);
        }

        Monitors monitors = {0};
        collect_monitors(&monitors);
        if (did_monitors_change(&wins.monitors, &monitors)) {
            replace_wins(&wins, &monitors);
            background.redraw = true;
        }
    }

end:
    // kill_sys_tray_icon(&win, ICON_ID);
    // close_win(&win);
    return 0;
}
