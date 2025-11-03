#include <Cocoa/Cocoa.h>
#include <CoreGraphics/CGWindowLevel.h>

void make_win_bg(NSWindow * win) {
    [win setStyleMask:NSWindowStyleMaskBorderless];

    [win setOpaque:NO];
    [win setBackgroundColor:[NSColor clearColor]];
    [win setHasShadow:NO];

    [win setLevel:kCGDesktopWindowLevel - 1];

    [win setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary];

    NSScreen *mainScreen = [NSScreen mainScreen];
    NSRect screenRect = [mainScreen frame];
    
    [win setFrame:screenRect display:YES];
    
    [win setMovable:NO];
    
    // Optional: Hide the app from the Dock and Cmd-Tab switcher (This needs to be set 
    // in the application's Info.plist using the 'LSUIElement' key set to 'YES')
}

@interface MyDrawingView : NSView
@end

@implementation MyDrawingView

- (void) drawRect : (NSRect) dirtyRect {
    NSGraphicsContext *context = [NSGraphicsContext currentContext];

    [[NSColor blueColor] setFill];

    NSRect rect = [self bounds];
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

    make_win_bg(win);

    // TODO: Create app delegate to handle system events.
    // TODO: Create menus (especially Quit!)

    [win orderFrontRegardless];
    [NSApp run];
    [pool drain];
    return 0;
}
