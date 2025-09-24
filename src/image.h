#ifndef IMAGE_H
#define IMAGE_H

#include "ds.h"
#include "graphics.h"

typedef struct {
    Color *buf;
    int alloc_w; /* length of rows in allocation (num pixels). This allows
                    pixels to still be accessed even when using image slices.
                    TODO: Make the semantics for allow_w better.*/
    int x, y;
    int w, h; // counting from (x, y)
    Color none;
} Image;

Color *img_at(Image *m, int x, int y);
Image new_img(Arena *perm, Image m);
Image copy_img(Arena *perm, Image m);
Image rescale_img(Arena *perm, Image img, int new_w, int new_h);
Image combine_bound(Image a, Image b);
Color mix_colors(Color a, Color b);
void place_img(Image onto, Image img, int px, int py, bool mix);

s8 write_img_to_file(s8 p, Image img);

#endif // IMAGE_H

#ifdef IMAGE_IMPL
#ifndef IMAGE_IMPL_GUARD
#define IMAGE_IMPL_GUARD

#include <math.h>

#define DS_IMPL
#include "ds.h"

#define GRAPHICS_IMPL
#include "graphics.h"

Color *img_at(Image *m, int x, int y) {
    // assert(x < m->w);
    // assert(y < m->h);
    // assert(0 <= x);
    // assert(0 <= y);

    int i = 0;

    if (x >= m->w || y >= m->h || 0 > x || 0 > y) {
        // warning("Accessing image out of bounds at position (%d, %d).", x, y);
        i = -1;
    } else i = (x + m->x) + (y + m->y) * m->alloc_w;

    if (i == -1) return &m->none;
    return &m->buf[i];
}

Image new_img(Arena *perm, Image m) {
    Image ret = { .w = m.w, .h = m.h, .alloc_w = m.w, };
    int s = m.w * m.h;
    ret.buf = new(perm, Color, s);
    return ret;
}

Image copy_img(Arena *perm, Image m) {
    Image ret = new_img(perm, m);
    place_img(ret, m, 0, 0, 0);
    return ret;
}

// Bilinear interpolation
Image rescale_img(Arena *perm, Image img, int new_w, int new_h) {
    assert(new_w >= 0);
    assert(new_h >= 0);

    Image ret = { .w = new_w, .h = new_h, };
    ret = new_img(perm, ret);

    float scale_w = (float) ret.w / img.w,
          scale_h = (float) ret.h / img.h;

    for (int x = 0; x < ret.w; x++) {
        for (int y = 0; y < ret.h; y++) {
            float a = 0;

            a = x / scale_w;
            float x0 = floor(a),
                  x1 = ceil(a),
                  dx = a - x0;

            if (x1 >= img.w) x1 = floor(a);

            a = y / scale_h;
            float y0 = floor(a),
                  y1 = ceil(a),
                  dy = a - y0;

            if (y1 >= img.h) y1 = floor(a);

            for (int i = 0; i < 4; i++) {
                float v00 = img_at(&img, x0, y0)->c[i],
                      v01 = img_at(&img, x0, y1)->c[i],
                      v10 = img_at(&img, x1, y0)->c[i],
                      v11 = img_at(&img, x1, y1)->c[i],
                      vt  = v00 * (1.0 - dx) + v10 * dx,
                      vb  = v01 * (1.0 - dx) + v11 * dx;
                img_at(&ret, x, y)->c[i] = vt * (1.0 - dy) + vb * dy;
            }
        }
    }

    return ret;
}

Color mix_colors(Color a, Color b) {
    float v = b.c[COLOR_A] / 255.0;
    Color ret = {
        .c[COLOR_R] = b.c[COLOR_R] * v + a.c[COLOR_R] * (1 - v),
        .c[COLOR_G] = b.c[COLOR_G] * v + a.c[COLOR_G] * (1 - v),
        .c[COLOR_B] = b.c[COLOR_B] * v + a.c[COLOR_B] * (1 - v),
        .c[COLOR_A] = 255,
    };
    return ret;
}

void place_img(Image onto, Image img, int px, int py, bool mix) {
    // assert(px + img.w <= onto.w);
    // assert(py + img.h <= onto.h);

    for (int x = 0; x < img.w; x++) {
        for (int y = 0; y < img.h; y++) {
            int nx = px + x, ny = py + y;
            Color *a = img_at(&onto, nx, ny),
                  *b = img_at(&img, x, y);
            *a = mix ? mix_colors(*a, *b) : *b;
        }
    }
}

Image combine_bound(Image a, Image b) {
    int x = (a.x < b.x) ? a.x : b.x,
        y = (a.y < b.y) ? a.y : b.y;

    assert(a.buf == b.buf);
    assert(a.alloc_w == b.alloc_w);

    // Calculate the new bottom-right corner by finding the maximum
    // of the two images' right and bottom edges.
    int right = (a.x + a.w > b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    int bottom = (a.y + a.h > b.y + b.h) ? (a.y + a.h) : (b.y + b.h);

    return (Image) {
        .buf = a.buf,
        .alloc_w = a.alloc_w,
        .x = x,
        .y = y,
        .w = right - x,
        .h = bottom - y,
    };
}

int _img_write_s8(s8 s, FILE *fp) {
    return s.len != fwrite(s.buf, 1, s.len, fp);
}

s8 write_img_to_file(s8 p, Image img) {
    s8 ret = {0};

    new_static_arena(scratch, 1 * KiB);

    FILE *fp = NULL;
    {
        s8 n = s8_newcat(&scratch, p, s8("\0"));
        fp = fopen((char *) n.buf, "w");
    }
    if (fp == NULL) return s8_errno();

    do {
        if (_img_write_s8(s8("P6\n"), fp)) goto err;
        if (_img_write_s8(u64_to_s8(&scratch, img.w, 0), fp)) goto err;
        if (_img_write_s8(s8(" "), fp)) goto err;
        if (_img_write_s8(u64_to_s8(&scratch, img.h, 0), fp)) goto err;
        if (_img_write_s8(s8("\n 255\n"), fp)) goto err;

        for (int i = 0; i < img.w * img.h; i++) {
            Color c = img.buf[i];
            u8 w[] = { c.c[COLOR_R], c.c[COLOR_G], c.c[COLOR_B], };
            s8 s = { .buf = w, .len = arrlen(w), };
            if (_img_write_s8(s, fp)) goto err;
        }
        break;
err:
        ret = s8_err(s8("fwrite"));
    } while (0);

end:
    if (fclose(fp) && !ret.len) ret = s8_errno();

    return ret;
}

#endif // IMAGE_IMPL_GUARD
#endif // IMAGE_IMPL
