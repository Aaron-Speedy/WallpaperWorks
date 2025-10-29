#define HAVE_GETTIMEOFDAY
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

int usleep(useconds_t useconds);

#include <curl/curl.h>

#define DS_IMPL
#include "ds.h"

#define err(...) do { \
  fprintf(stderr, "Error: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  *((int *) 0) = 0; \
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
#include "../build/tmp/raw_font_buf.h"
#include "../build/tmp/app_name.h"

typedef struct {
    bool redraw;
    Image img;
} Background;

Background background = {0};
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *background_thread() {
    srand(time(0));

    pthread_mutex_lock(&lock); // initial lock

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
                    network_mode = img_data.buf != NULL;

                    if (!network_mode) continue; // try another image;

                    s8_write_to_file(path, img_data);
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
                        .c[COLOR_A] = 255,
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
    u8 *raw_buf;
    int raw_len;
    float time_pt, date_pt;
    float time_size, date_size;
    s8 cmd;
    bool draw_to_img, draw_to_root;
    Arena cmd_arena;
} Wins;

#ifdef __linux__
void set_cmd_and_platform(Wins *wins) {
    wins->cmd_arena.len = 0;

    s8 desktop = get_desktop_name();

    if (s8_equals(desktop, s8("XFCE"))) {
        wins->draw_to_img = true;
        wins->cmd = s8("xfconf-query --channel xfce4-desktop --property ");

        s8 s = s8_system(
            &wins->cmd_arena,
            s8("xfconf-query --channel xfce4-desktop --list"),
            5 * KiB
        );
        s8 file = {0};

        for (int i = 0; i < s.len; i++) {
            s8 cmp1 = s8("last-image"), cmp2 = s8("image-path");
            bool b1 = (s8_equals(cmp1, (s8) slice(s, i, cmp1.len))),
                 b2 = (s8_equals(cmp2, (s8) slice(s, i, cmp2.len)));

            if (b1 || b2){
                int begin = 0, end = s.len - 1;

                for (int j = i; j >= 0; j--) {
                    if (s.buf[j] == '\n') {
                        begin = j + 1;
                        break;
                    }
                }

                for (int j = i; j < s.len; j++) {
                    if (s.buf[j] == '\n') {
                        end = j - 1;
                        break;
                    }
                }

                file = (s8) slice(s, begin, end - begin + 1);
                break;
            }
        }

        wins->cmd = s8_newcat(&wins->cmd_arena, wins->cmd, file);
        s8_modcat(&wins->cmd_arena, &wins->cmd, s8(" --set "));
        return;
    } else if (s8_equals(desktop, s8("GNOME"))) {
        wins->draw_to_root = false;
        wins->cmd = s8_copy(&wins->cmd_arena, s8("gsettings set org.gnome.desktop.background picture-uri-dark file://"));
    }  else if (s8_equals(desktop, s8(""))) wins->draw_to_root = true;
}
#endif

void replace_wins(Wins *wins, Monitors *m) {
    for (int i = 0; i < wins->monitors.len; i++) {
        Win *w = &wins->wins[i];
        w->is_bg = false; // TODO: make sure the wallpaper isn't cleared
        free_font(wins->time_fonts[i]);
        free_font(wins->date_fonts[i]);
        close_win(w);
    }

    wins->monitors.len = m->len;
    for (int i = 0; i < wins->monitors.len; i++) {
        Win *w = &wins->wins[i];
        PlatformMonitor *monitor = &m->buf[i];
        wins->monitors.buf[i] = *monitor;

        new_win(w, APP_NAME, 500, 500);
        assert(w->buf);

#ifdef __linux__
        if (wins->draw_to_img) w->p.draw_to_img = true;
#endif

        assert(w->buf && "First pos.");
        make_win_bg(w, *monitor, wins->draw_to_root);
        assert(w->buf && "Second pos.");

        move_win_to_monitor(w, *monitor);
        assert(w->buf && "Third pos.");
        _fill_working_area(w, *monitor);
        assert(w->buf && "Fourth pos.");

        int d = w->w < w->h ? w->w : w->h;

        load_font(
            &wins->time_fonts[i],
            wins->font_lib,
            wins->raw_buf,
            wins->raw_len,
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
            wins->raw_buf,
            wins->raw_len,
            wins->date_pt,
            w->dpi_x, w->dpi_y
        );
        FT_Set_Pixel_Sizes(
            wins->date_fonts[i].face,
            d * wins->date_size,
            d * wins->date_size
        );

        show_win(w);
    }
}

void show_message_box(char *text, bool err, bool block) {
#ifdef __linux__
    assert(!"Unimplemented");
    Win win = {0};
    new_win(&win, "Message Box", 300, 100);
#elif _WIN32
    unsigned int flags = (block * MB_APPLMODAL) | (err * MB_ICONERROR);
    MessageBoxA(0, text, 0, flags);
#endif
}

int main() {
    // usleep(1000000);
    // if (is_program_already_open(APP_NAME)) {
    //     show_message_box(APP_NAME" is already open!", true, true);
    //     return 1;
    // }

    Arena perm = new_arena(1 * GiB);

    float time_x = 0.04, time_y = 0.06, // from the bottom-right
          time_size = 0.18,
          time_shadow_x = 0.002, time_shadow_y = 0.002;

    float date_x = 0.04, date_y = time_y + 0.17, // from the bottom-right
          date_size = 0.04,
          date_shadow_x = 0.002, date_shadow_y = 0.002;

    Wins wins = {
        // .worker_w = _make_worker_w(),
        .font_lib = init_ffont(),
        .raw_buf = raw_font_buf,
        .raw_len = raw_font_buf_len,
        // .font_path = "./font.ttf",
        .time_pt = 130,
        .date_pt = 26,
        .time_size = time_size,
        .date_size = date_size,
        .cmd_arena = new_arena(4 * KiB),
    };

#ifdef __linux__
    set_cmd_and_platform(&wins);
#endif

    {
        Monitors m = { .len = 1, };
        collect_monitors(&m);
        replace_wins(&wins, &m);
    }

    show_sys_tray_icon(&wins.wins[0], ICON_ID, "Close " APP_NAME);

    pthread_t thread;
    if (pthread_create(&thread, NULL, background_thread, NULL)) {
        err("Failed to create background thread.");
    }

    bool xfce_hack = 0;

    while (1) {
        xfce_hack = !xfce_hack;

        while (true) {
            pthread_mutex_lock(&lock);
                bool end = background.img.buf;
            pthread_mutex_unlock(&lock);
            if (end) break;
            usleep(1/20 * 1000000);
        }

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
            };            s8 days[] = {
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

            Image time_bound = get_bound_of_text(time_font, time_str);
            time_bound = draw_text_shadow(
                screen,
                time_font,
                time_str,
                screen.w - 1 - screen.w * time_x - time_bound.w,
                screen.h - 1 - screen.h * time_y,
                255, 255, 255,
                time_shadow_x * screen.w, time_shadow_y * screen.h,
                0, 0, 0,
                true
            );
            Image date_bound = get_bound_of_text(date_font, date_str);
            date_bound = draw_text_shadow(
                screen,
                date_font,
                date_str,
                screen.w - 1 - screen.w * date_x - date_bound.w,
                screen.h - 1 - screen.h * date_y,
                255, 255, 255,
                date_shadow_x * screen.w, date_shadow_y * screen.h,
                0, 0, 0,
                true
            );
        }
        if (background.redraw) background.redraw = false;
        pthread_mutex_unlock(&lock);

        struct timeval time_val;
        gettimeofday(&time_val, NULL);

        get_events_timeout(
            &wins.wins[0],
            1000 - (time_val.tv_usec / 1000) // update exactly on the second
        );

        for (int i = 0; i < wins.monitors.len; i++) {
            Win *win = &wins.wins[i];

            for (int i = 0; i < win->event_queue_len; i++) {
                WinEvent event = win->event_queue[i];
                switch (event.type) {
                case EVENT_QUIT: goto end;
                case EVENT_SYS_TRAY: {
                    int c = event.click;
                    if (c == CLICK_L_UP ||
                        c == CLICK_M_UP ||
                        c == CLICK_R_UP) goto end;
                } break;
                default: break;
                }
            }

            win->event_queue_len = 0;

#ifdef __linux__
            if (!win->p.draw_to_img) draw_to_win(win);

            if (win->p.draw_to_img || wins.cmd.len) {
                u64 hash = 0;
                {
                    s8 s = {
                        .buf = (u8 *) win->buf,
                        .len = win->w * win->h * sizeof(*win->buf),
                    };
                    hash = s8_hash(s);
                }
                printf("%llu\n", hash);

                if (hash != win->hash) {
                    win->hash = hash;

                    new_static_arena(scratch, 3 * KiB);

                    s8 a0 = get_or_make_cache_dir(&scratch, s8(APP_NAME));
                            assert(a0.len > 0);
                            s8_copy(&scratch, s8("/"));
                            u64_to_s8(&scratch, xfce_hack, 0); // hack for XFCE
                    s8 al = s8_copy(&scratch, s8("current_wallpaper.ppm"));
                    s8 path = s8_masscat(scratch, a0, al);
                    s8_print(path);

                    Image img = { .buf = win->buf, .w = win->w, .h = win->h, };
                    write_img_to_file(path, img);
                    printf("\nDone.\n");

                    s8 cmd = s8_newcat(&scratch, wins.cmd, path);
                    s8_modcat(&scratch, &cmd, s8("\0"));
                    system(cmd.buf);
                    s8_print(cmd);
                    printf("\n");
                }
            }
#else
            draw_to_win(win);
#endif
        }


        Monitors monitors = {0};
        collect_monitors(&monitors);
        if (did_monitors_change(&wins.monitors, &monitors)) {
            replace_wins(&wins, &monitors);
            background.redraw = true;
        }
    }

end:
    kill_sys_tray_icon(&wins.wins[0], ICON_ID);
    for (int i = 0; i < wins.monitors.len; i++) {
        close_win(&wins.wins[i]);
    }
    return 0;
}
