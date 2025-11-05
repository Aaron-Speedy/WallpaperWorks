#include <Cocoa/Cocoa.h>
#include <CoreGraphics/CGWindowLevel.h>
#include <CoreGraphics/CGDisplayConfiguration.h>

#include <stdint.h>

#define err(...) do { \
  fprintf(stderr, "Error: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
  exit(1); \
} while (0);

#define warning(...) do { \
  fprintf(stderr, "Warning: "); \
  fprintf(stderr, __VA_ARGS__); \
  fprintf(stderr, "\n"); \
} while (0);

typedef struct {
    uint8_t c[4];
} Color;

typedef enum {
    COLOR_R,
    COLOR_G,
    COLOR_B,
    COLOR_A,
} PlatformColorEnum;

#define IMAGE_IMPL
#include "image.h"

Color color(u8 r, u8 g, u8 b, u8 a) {
    return (Color) {
        .c[COLOR_R] = r,
        .c[COLOR_G] = g,
        .c[COLOR_B] = b,
        .c[COLOR_A] = a,
    };
}

typedef struct {
    Image *screen;
    int dpi;
    void *data;
} Context;

#include "main.c"

CGSize get_dpi(NSScreen *screen) {
    NSDictionary *description = [screen deviceDescription];
    NSNumber *screen_num = [description objectForKey:@"NSScreenNumber"];
    CGDirectDisplayID display_id = [screen_num unsignedIntValue];

    CGSize physical_size_mm = CGDisplayScreenSize(display_id);
    
    if (physical_size_mm.width == 0 || physical_size_mm.height == 0) {
        return CGSizeMake(72.0, 72.0);
    }
    
    CGFloat backing_scale = [screen backingScaleFactor];
    NSRect screen_frame = [screen frame];
    
    CGFloat width = screen_frame.size.width * backing_scale;
    CGFloat height = screen_frame.size.height * backing_scale;

    CGFloat mm_per_inch = 25.4;
    
    CGSize dpi;
    dpi.width  = (width / physical_size_mm.width) * mm_per_inch;
    dpi.height = (height / physical_size_mm.height) * mm_per_inch;

    return dpi;
}

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
    float animation_time;
    Context context;

}

- (void) setup_bitmap_ctx;
- (void) draw_buf;
- (void) cleanup_bitmap_ctx;
- (void) start_animation;

@end

@implementation MyDrawingView

- (id) initWithFrame : (NSRect) frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        [self setup_bitmap_ctx];
    }
    return self;
}

- (void) setup_bitmap_ctx {
    [self cleanup_bitmap_ctx];

    w = (size_t) [self bounds].size.width;
    h = (size_t) [self bounds].size.height;

    buf = calloc(h * w * 4, sizeof(unsigned char));

    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    bitmap_ctx = CGBitmapContextCreate(
        buf,
        w, h,
        8, 4 * w,
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

    Image screen = { .buf = buf, .alloc_w = w, .w = w, .h = h, };
    context = (Context) { .dpi = get_dpi.width, .screen = &screen, };
    app_loop(&context);
}

- (void) drawRect : (NSRect) dirty_rect {
    [self draw_buf];

    CGImageRef image = CGBitmapContextCreateImage(bitmap_ctx);
    CGContextRef screen_ctx = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    if (image) { // TODO: handle errors
        CGContextDrawImage(screen_ctx, CGRectMake(0, 0, w, h), image);
        CGImageRelease(image);
    }
}

- (void) start_animation {
    NSTimeInterval interval = 1;
    [NSTimer scheduledTimerWithTimeInterval:interval
                                         target:self
                                       selector:@selector(update_display:) 
                                       userInfo:nil 
                                        repeats:YES];
    Image screen = { .buf = buf, .alloc_w = w, .w = w, .h = h, };
    context = (Context) { .dpi = 72, .screen = &screen, }; // TODO: get DPI
    start(&context);
}

- (void) update_display : (NSTimer *) timer {
    animation_time += 0.05; 
    [self setNeedsDisplay:YES];
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

    MyDrawingView *drawing_view = [[MyDrawingView alloc] initWithFrame:win_rect];
    [drawing_view autorelease];
    [win setContentView:drawing_view];

    make_win_bg(win);

    [drawing_view setup_bitmap_ctx];
    [drawing_view start_animation];

    // TODO: Create app delegate to handle system events.
    // TODO: Create menus (especially Quit!)

    [win orderFrontRegardless];
    [NSApp run];
    [pool drain];
    return 0;
}
