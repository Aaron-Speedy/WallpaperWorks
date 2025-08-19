#ifndef IMAGE_H
#define IMAGE_H

#include "ds.h"

typedef struct {
    u8 c[3]; // RGB
    u32 packed; // 0xRRGGBB. Should be set to the colors in c.
} Color;

typedef struct {
    Color *buf;
    u32 *packed;
    int alloc_w; /* length of rows in allocation (num pixels). This allows
                    pixels to still be accessed even when using image slices.
                    TODO: Make the semantics for allow_w better.*/
    int x, y;
    int w, h; // counting from (x, y)
} Image;

int img_at(Image m, int x, int y);
Color *img_atb(Image m, int x, int y);
void pack_color(Image m, int x, int y);
Image new_img(Arena *perm, Image m);
Image copy_img(Arena *perm, Image m);
Image rescale_img(Arena *perm, Image img, int new_w, int new_h);
void place_img(Image onto, Image img, int px, int py);

#endif // IMAGE_H

#ifdef IMAGE_IMPL
#ifndef IMAGE_IMPL_GUARD
#define IMAGE_IMPL_GUARD

#define DS_IMPL
#include "ds.h"

int img_at(Image m, int x, int y) {
    assert(x < m.w);
    assert(y < m.h);
    assert(0 <= x);
    assert(0 <= y);
    return (x + m.x) + (y + m.y) * m.alloc_w;
}

Color *img_atb(Image m, int x, int y) {
    return &m.buf[img_at(m, x, y)];
}

void pack_color(Image m, int x, int y) {
    int i = img_at(m, x, y);
    Color c = m.buf[i];
    m.packed[i] = (c.c[2]) | (c.c[1] << 8) | (c.c[0] << 16);
}

Image new_img(Arena *perm, Image m) {
    Image ret = { .w = m.w, .h = m.h, .alloc_w = m.w, };
    int s = m.w * m.w;
    ret.buf = new(perm, Color, s);
    ret.packed = new(perm, u32, s);
    return ret;
}

Image copy_img(Arena *perm, Image m) {
    Image ret = new_img(perm, m);
    place_img(ret, m, 0, 0);
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

            for (int i = 0; i < 3; i++) {
                float v00 = img_atb(img, x0, y0)->c[i],
                      v01 = img_atb(img, x0, y1)->c[i],
                      v10 = img_atb(img, x1, y0)->c[i],
                      v11 = img_atb(img, x1, y1)->c[i],
                      vt  = v00 * (1.0 - dx) + v10 * dx,
                      vb  = v01 * (1.0 - dx) + v11 * dx;
                img_atb(ret, x, y)->c[i] = vt * (1.0 - dy) + vb * dy;
            }
            pack_color(ret, x, y);
        }
    }

    return ret;
}

void place_img(Image onto, Image img, int px, int py) {
    assert(px + img.w <= onto.w);
    assert(py + img.h <= onto.h);

    for (int x = 0; x < img.w; x++) {
        for (int y = 0; y < img.h; y++) {
            int nx = px + x, ny = py + y;
            *img_atb(onto, nx, ny) = *img_atb(img, x, y);
            pack_color(onto, nx, ny);
        }
    }
}

#endif // IMAGE_IMPL_GUARD
#endif // IMAGE_IMPL
