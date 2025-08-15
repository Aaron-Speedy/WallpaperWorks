#include <stdio.h>
#define STB_TRUETYPE_IMPL
#include "third_party/stb_truetype.h"

char ttf_buffer[1<<25];

int main(int argc, char **argv) {
   stbtt_fontinfo font;
   unsigned char *bitmap;
   int w, h, ch = '9', size = 20;

   fread(ttf_buffer, 1, 1<<25, fopen(argc > 3 ? argv[3] : "/usr/share/fonts/TTF/JetBrainsMono-Bold.ttf", "rb"));

   stbtt_InitFont(&font, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer, 0));
   bitmap = stbtt_GetCodepointBitmap(&font, 0, stbtt_ScaleForPixelHeight(&font, size), ch, &w, &h, 0,0);

   for (int j = 0; j < h; j++) {
      for (int i = 0; i < w; i++)
         putchar(" .:ioVM@"[bitmap[j*w+i]>>5]);
      putchar('\n');
   }

   return 0;
}
