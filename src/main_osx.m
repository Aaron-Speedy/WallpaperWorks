#include <Cocoa/Cocoa.h>

@interface MyDrawingView : NSView
@end

@implementation MyDrawingView

- (void)drawRect:(NSRect)dirtyRect {
    NSGraphicsContext *context = [NSGraphicsContext currentContext];

    [[NSColor blueColor] setFill];

    NSRect rect = NSMakeRect(50, 50, 300, 300);
    NSRectFill(rect);
}

@end

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

    MyDrawingView *drawing_view = [[MyDrawingView alloc] initWithFrame:win_rect];
    [drawing_view autorelease];
    [win setContentView:drawing_view];

    // TODO: Create app delegate to handle system events.
    // TODO: Create menus (especially Quit!)

    [win orderFrontRegardless];
    [NSApp run];
    [pool drain];
    return 0;
}
