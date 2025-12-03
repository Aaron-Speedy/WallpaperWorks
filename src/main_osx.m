#import <Foundation/Foundation.h>

#include <stdio.h>
#include <unistd.h>

int main(void) {
    @autoreleasepool {
        NSTask *task = [[NSTask alloc] init];
        [task setLaunchPath:@"/usr/bin/defaults"];
        [task setArguments:@[@"touch", @"AOK"]];
        [task launch];
        [task waitUntilExit];
    }

    return 0;
}
