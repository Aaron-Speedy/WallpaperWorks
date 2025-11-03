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

@interface MyDrawingView : NSView {
    CGContextRef bitmap_ctx;
    unsigned char *buf;
    size_t w, h;
}

- (void) setup_bitmap_ctx;
- (void) draw_buf;
- (void) cleanup_bitmap_ctx;

@end

@implementation MyDrawingView

- (void) setup_bitmap_ctx {
    width = (size_t) [self bounds].size.width;
    height = (size_t) [self bounds].size.height;

    buf = calloc(height * width * 4, sizeof(unsigned char));

    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    bitmap_ctx = CGBitmapContextCreate(
        buf,
        width, height,
        8, 4 * width,
        color_space,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big
    );
    CGColorSpaceRelease(color_space);
}

- (void) cleanup_bitmap_ctx {
    if (bitmap_ctx) {
        CGContextRelease(bitmap_ctx);
        bitmap_ctx = 0;
    }
    free(buf);
    buf = 0;
}

- (void) dealloc {
    [self cleanup_bitmap_ctx];
    [super dealloc];
}

- (void) draw_buf {
    if (!buf) return;

    for (size_t x = 0; x < width; x++) {
        for (size_t y = 0; y < height; y++) {
            size_t i = (x + y * width) * 4;

            unsigned char r = (unsigned char) (255 * sin(x * 0.05 + y * 0.05) + 128);
            unsigned char g = (unsigned char) (x % 256);
            unsigned char b = (unsigned char) (y % 256);

            buf[i + 0] = r;
            buf[i + 1] = g;
            buf[i + 2] = b;
            buf[i + 3] = 255;
        }
    }
}

- (void) drawRect : (NSRect) dirtyRect {
    [self draw_buf];

    CGImageRef image = CGBitmapContextCreateImage(bitmap_ctx);
    CGContextRef screen_ctx = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    if (image) { // TODO: handle errors
        CGContextDrawImage(screen_ctx, CGRectMake(0, 0, w, h), image);
        CGImageRelease(image);
    }
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
