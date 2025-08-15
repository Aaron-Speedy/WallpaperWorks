#ifndef IMAGE_H
#define IMAGE_H

typedef struct {
    u8 c[3]; // RGB
    u32 packed; // 0xRRGGBB. Should be set to the colors in c.
} Color;

typedef struct {
    Color *buf;
    u32 *packed;
    int w, h;
} Image;

void pack_color(Image m, int i);
Image new_img(Image m);
Image rescale_img(Image img, int new_w, int new_h);
bool place_img(Image onto, Image img, int px, int py);

#endif // IMAGE_H

#ifdef IMAGE_IMPL
#ifndef IMAGE_IMPL_GUARD
#define IMAGE_IMPL_GUARD

void pack_color(Image m, int i) {
    m.packed[i] = (m.buf[i].c[2]) |
                  (m.buf[i].c[1] << 8) |
                  (m.buf[i].c[0] << 16);
}

Image new_img(Image m) {
    m.buf = malloc(sizeof(m.buf[0]) * m.w * m.h);
    m.packed = malloc(sizeof(m.packed[0]) * m.w * m.h);
    return m;
}

// Bilinear interpolation
Image rescale_img(Image img, int new_w, int new_h) {
    Image ret = { .w = new_w, .h = new_h, };
    ret = new_img(ret);

    float scale_w = (float) ret.w / img.w,
          scale_h = (float) ret.h / img.h;

    for (int x = 0; x < ret.w; x++) {
        for (int y = 0; y < ret.h; y++) {
            float a = 0;

            a = x / scale_w;
            float x0 = floor(a),
                  x1 = ceil(a),
                  dx = a - x0;

            a = y / scale_h;
            float y0 = floor(a),
                  y1 = ceil(a),
                  dy = a - y0;

            int ret_i = x + y * ret.w;
            for (int i = 0; i < 3; i++) {
                float v00 = img.buf[(int) (x0 + y0 * img.w)].c[i],
                      v01 = img.buf[(int) (x0 + y1 * img.w)].c[i],
                      v10 = img.buf[(int) (x1 + y0 * img.w)].c[i],
                      v11 = img.buf[(int) (x1 + y1 * img.w)].c[i],
                      vt  = v00 * (1.0 - dx) + v10 * dx,
                      vb  = v01 * (1.0 - dx) + v11 * dx;
                ret.buf[ret_i].c[i] = vt * (1.0 - dy) + vb * dy;
            }
            pack_color(ret, ret_i);
        }
    }

    return ret;
}

bool place_img(Image onto, Image img, int px, int py) {
    if (px + img.w > onto.w) return 1;
    if (py + img.h > onto.h) return 1;

    for (int x = 0; x < img.w; x++) {
        for (int y = 0; y < img.h; y++) {
            int i = (px + x) + (py + y) * onto.w;
            onto.buf[i] = img.buf[x + y * img.w];
            pack_color(onto, i);
        }
    }

    return 0;
}

#endif // IMAGE_IMPL_GUARD
#endif // IMAGE_IMPL
