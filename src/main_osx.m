#include <Cocoa/Cocoa.h>

int main(void) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    [NSApplication sharedApplication];
    NSUInteger win_style = NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask;
    NSRect win_rect = NSMakeRect(100, 100, 400, 400);
    NSWindow *win = [[NSWindow alloc] initWithContentRect:win_rect
                                          styleMask:win_style
                                          backing:NSBackingStoreBuffered
                                          defer:NO];
    [win autorelease];
    NSWindowController * win_controller = [[NSWindowController alloc] initWithWindow:win];
    [win_controller autorelease];
    NSTextView * text_view = [[NSTextView alloc] initWithFrame:win_rect];
    [text_view autorelease];
    [win setContentView:text_view];
    [text_view insertText:@"Hello OSX/Cocoa world!"];
    // TODO: Create app delegate to handle system events.
    // TODO: Create menus (especially Quit!)
    [win orderFrontRegardless];
    [NSApp run];
    [pool drain];
    return 0;
}
