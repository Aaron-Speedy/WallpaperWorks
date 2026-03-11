#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CGDisplayConfiguration.h>
#include <CoreGraphics/CGWindowLevel.h>
#include <Security/Security.h>
#include <ServiceManagement/SMAppService.h>

#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>

void alert(NSString *msg) {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText: msg];
    [alert setInformativeText: @"Information"];
    [alert addButtonWithTitle: @"Ok"];
    [alert addButtonWithTitle: @"Cancel"];
    [alert runModal];
}

void alertf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    NSString *msg = [[NSString alloc]
        initWithFormat:[NSString stringWithUTF8String:fmt]
        arguments:args];

    va_end(args);

    dispatch_async(dispatch_get_main_queue(), ^{
        alert(msg);
    });
}

#define err(...) do { \
  alertf("Error: "); \
  alertf(__VA_ARGS__); \
  alertf("\n"); \
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

#define MAX_PLATFORM_MONITORS 100
#include "main.c"

@class MyDrawingView;

// TODO: this sucks
size_t wins_len = 0;
NSWindow *wins[arrlen(ctx.monitors)] = {0};
MyDrawingView *views[arrlen(ctx.monitors)] = {0};

void make_win_bg(NSWindow * win) {
    [win setStyleMask: NSWindowStyleMaskBorderless];
    [win setOpaque: NO];
    [win setBackgroundColor: [NSColor clearColor]];
    [win setHasShadow: NO];
    [win setLevel: kCGDesktopWindowLevel - 1];
    [win setCollectionBehavior: NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary];
    [win setFrame: [[win screen] frame] display: YES];
    [win setMovable: NO];
}

void reconfigure_screens(bool first_time) {
    atomic_store(&ctx.monitors_len, 0);

    for (int i = 0; i < wins_len; i++) {
        if (wins[i]) {
            [wins[i] close];
            wins[i] = nil;
        }
        if (views[i]) {
            [views[i] cleanup_bitmap_ctx];
            [views[i] removeFromSuperview];
            views[i] = nil;
        }
    }

    NSArray<NSScreen *> *screens = [NSScreen screens];
    wins_len = [screens count];

    for (int i = 0; i < wins_len; i++) {
        NSRect frame = [screens[i] frame];
        wins[i] = [[NSWindow alloc]
            initWithContentRect: frame
            styleMask: NSWindowStyleMaskBorderless
            backing: NSBackingStoreBuffered
            defer: NO
        ];
        make_win_bg(wins[i]);

        views[i] = [[MyDrawingView alloc] initWithFrame: frame];
        [wins[i] setContentView: views[i]];
        [wins[i] orderFrontRegardless];
    }

    if (first_time) start();
    else make_fonts();

    for (int i = 0; i < atomic_load(&ctx.monitors_len); i++) {
        [views[i] start_animation];
    }
}

@interface AppDelegate : NSObject <NSApplicationDelegate>
- (void) applicationDidFinishLaunching : (NSNotification *) notification;
- (void) disable_login_item;
- (void) reconfigure_monitors;
- (void) system_will_sleep : (NSNotification *) notification;
- (void) system_did_wake : (NSNotification *) notification;
- (void) skip_image : (id) sender;
@property (nonatomic, strong) NSStatusItem *status_item;
@property (nonatomic, strong) NSImage *status_on_img;
@property (nonatomic, strong) NSImage *status_off_img;
@end

@implementation AppDelegate
- (void) applicationDidFinishLaunching : (NSNotification *) notification {
    self.status_item = [[NSStatusBar systemStatusBar]
        statusItemWithLength: NSVariableStatusItemLength
    ];

    self.status_on_img = [NSImage imageNamed: @"status_bar_icon_on"];
    [self.status_on_img setSize: NSMakeSize(22, 22)];

    self.status_off_img = [NSImage imageNamed: @"status_bar_icon_off"];
    [self.status_off_img setSize: NSMakeSize(22, 22)];

    [self.status_item setImage: self.status_off_img];
    [self.status_item setAlternateImage: self.status_on_img];
    [self.status_item setHighlightMode: YES];

    self.status_item.menu = [[NSMenu alloc] initWithTitle: @""];

    NSMenuItem *quit_item = [[NSMenuItem alloc]
        initWithTitle: @"Quit"
        action: @selector(terminate:)
        keyEquivalent: @"q"
    ];
    [quit_item setTarget: NSApp];
    [self.status_item.menu addItem: quit_item];

    NSMenuItem *skip_item = [[NSMenuItem alloc]
        initWithTitle: @"Skip image"
        action: @selector(skip_image:)
        keyEquivalent: @"s"
    ];
    [skip_item setTarget: self];
    [self.status_item.menu addItem: skip_item];

    [[NSNotificationCenter defaultCenter]
        addObserver: self
        selector: @selector(reconfigure_monitors)
        name: NSApplicationDidChangeScreenParametersNotification
        object: nil
    ];

    [[NSWorkspace sharedWorkspace].notificationCenter addObserver:
        self
        selector: @selector(system_will_sleep:)
        name: NSWorkspaceWillSleepNotification
        object: nil
    ];

    [[NSWorkspace sharedWorkspace].notificationCenter addObserver:
        self
        selector: @selector(system_did_wake:)
        name: NSWorkspaceDidWakeNotification
        object: nil
    ];

    {
        SMAppService *service = [SMAppService mainAppService];
        NSError *error = 0;
        BOOL success = [service registerAndReturnError:&error];
        if (!success) NSLog(@"Login item registration failed with error: %@", error); // TODO: show an alert or something
        // if (service.status == SMAppServiceStatusNotRegistered) {
        //     NSError *error = 0;
        // } else if (service.status == SMAppServiceStatusNotFound) {
        //     NSLog(@"App service status is not found.");
        // } else if (service.status == SMAppServiceStatusEnabled) {
        //     NSLog(@"Login item is already registered.");
        // } else if (service.status == SMAppServiceStatusRequiresApproval) {
        //     NSLog(@"Login item requires approval.");
        //     [SMAppService openSystemSettingsLoginItems];
        // }
    }

    reconfigure_screens(true);
}

- (void) disable_login_item {
    SMAppService *service = [SMAppService mainAppService];
    
    // Unregister the service
    NSError *error = 0;
    BOOL success = [service unregisterAndReturnError: &error];
    
    if (!success) NSLog(@"Login item unregistration failed with error: %@", error);
}

- (void) reconfigure_monitors {
    reconfigure_screens(false);
}

- (void) system_will_sleep : (NSNotification *) notification {
    close_gate(&not_paused);
}

- (void) system_did_wake : (NSNotification *) notification {
    open_gate(&not_paused);
    [self reconfigure_monitors];
}

- (void) skip_image : (id) sender {
    atomic_store(&ctx.skip_image, true);
}

@end

@interface MyDrawingView : NSView {
    CGContextRef bitmap_ctx;
    unsigned char *buf;
    size_t w, h, monitor_index;
}

- (void) setup;
- (void) cleanup_bitmap_ctx;
- (void) draw_buf;
- (void) start_animation;

@end

@implementation MyDrawingView

- (id) initWithFrame : (NSRect) frameRect {
    self = [super initWithFrame: frameRect];
    if (self) {
        [self setup];
    }
    return self;
}

- (void) setup {
    [self cleanup_bitmap_ctx];

    w = (size_t) [self bounds].size.width;
    h = (size_t) [self bounds].size.height;

    buf = calloc(h * w * 4, sizeof(unsigned char));

    monitor_index = atomic_fetch_add(&ctx.monitors_len, 1);

    ctx.monitors[monitor_index].screen = (Image) {
        .buf = buf,
        .alloc_w = w,
        .w = w,
        .h = h,
    };

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
    ctx.monitors[monitor_index].screen.buf = 0;
}

- (void) dealloc {
    [self cleanup_bitmap_ctx];
    [super dealloc];
}

- (void) draw_buf {
    if (!buf) return;

    ctx.monitors[monitor_index].screen = (Image) {
        .buf = buf,
        .alloc_w = w,
        .w = w,
        .h = h,
    };
    app_loop(monitor_index);
}

- (void) drawRect : (NSRect) dirtyRect {
    [self draw_buf];

    CGImageRef image = CGBitmapContextCreateImage(bitmap_ctx);
    CGContextRef screen_ctx = (CGContextRef) [[NSGraphicsContext currentContext] graphicsPort];
    if (image) { // TODO: handle errors
        CGContextDrawImage(screen_ctx, CGRectMake(0, 0, w, h), image);
        CGImageRelease(image);
    }
}

- (void) start_animation {
    NSTimeInterval interval = 1;
    [NSTimer scheduledTimerWithTimeInterval:
        interval
        target: self
        selector: @selector(update_display:)
        userInfo: 0
        repeats: YES
    ];
}

- (void) update_display : (NSTimer *) timer {
    [self setNeedsDisplay:YES];
}

@end

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSApplication *app = [NSApplication sharedApplication];

    AppDelegate *app_delegate = [[AppDelegate alloc] init];
    [app_delegate autorelease];
    [app setDelegate: app_delegate];

    // TODO: Create app delegate to handle system events.

    [NSApp run];
    [pool drain];
    return 0;
}
