#define DS_IMPL
#include "ds.h"

#include "libwebp/src/webp/decode.h"

int main() {
    Arena perm = new_arena(1 * GiB);
    Arena scratch = new_arena(1 * KiB);

    s8 data = read_file(&perm, scratch, s8("150.webp"));
    printf("%ld\n", data.len);

    int w, h;
    WebPGetInfo(data.buf, data.len, &w, &h);
    printf("%dx%d\n", w, h);

    u8 *pixels = WebPDecodeRGB(data.buf, data.len, &w, &h);

    return 0;
}
