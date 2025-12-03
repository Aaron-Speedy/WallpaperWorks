#include <stdio.h>
#include <unistd.h>

int main(void) {
    char *args[] = { "touch", "ok", NULL };

    if (execvp(args[0], args) == -1) {
        perror("execvp failed");
    }

    return 0;
}
