#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define DS_IMPL
#include "ds.h"

void print_status_err(int argc, char *argv[]) {
    fprintf(stderr, "Status: %s [file] [array_name]\n", argv[0]);
}

s8 cstr_to_s8(char *s) {
    return (s8){ .buf = s, .len = strlen(s) };
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        print_status_err(argc, argv);
        exit(1);
    }

    s8 file_path = cstr_to_s8(argv[1]);
    s8 arr_name = cstr_to_s8(argv[2]);

    s8 file = s8_read_file(NULL, file_path);
    if (file.len < 0) {
        fprintf(stderr, "Error: Failed to read file %s\n", argv[1]);
        exit(1);
    }

    printf("const unsigned char %s[%zu] = {\n", arr_name.buf, file.len);

    char *tab = "    ";

    printf("%s", tab);
    for (size_t i = 0; i < file.len; ++i) {
        printf("0x%02x,", (unsigned char)file.buf[i]);
        if ((i + 1) % 16 == 0) printf("\n%s", tab);
        else printf(" ");
    }

    printf("\n};\n");
    printf("const int %s_len = %zu;\n", arr_name.buf, file.len);

    return 0;
}
