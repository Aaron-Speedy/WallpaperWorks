#include <stdio.h>
#include <unistd.h>

int main(void) {
    @autoreleasepool {
        NSTask *task = [[NSTask alloc] init];
        [task setLaunchPath:@"/usr/bin/env"];
        [task setArguments:@[@"touch", @"AOK"]];
        [task launch];
        [task waitUntilExit];
    }

    return 0;
}
