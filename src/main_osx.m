#import <Foundation/Foundation.h>

#include <stdio.h>
#include <unistd.h>

int main(void) {
    @autoreleasepool {
        NSTask *task = [[NSTask alloc] init];
        [task setLaunchPath:@"/usr/bin/touch"];
        [task setArguments:@[@"AOK"]];
        [task launch];
        [task waitUntilExit];
    }

    return 0;
}
