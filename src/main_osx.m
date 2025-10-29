#import <Cocoa/Cocoa.h>
int main() {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    NSWindow *wnd =
        [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 320, 240)
                                    styleMask:NSTitledWindowMask
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    [wnd setTitle:@"Hello, Cocoa"];
    [wnd makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
    while (1) {
        NSEvent *event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                            untilDate:[NSDate distantPast]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        // handle events here
        [NSApp sendEvent:event];
    }
    [wnd close];
    return 0;
}
