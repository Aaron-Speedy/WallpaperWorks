#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CGDisplayConfiguration.h>
#include <CoreGraphics/CGWindowLevel.h>
#include <Security/Security.h>
#include <ServiceManagement/SMAppService.h>

#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>

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

#define MAX_PLATFORM_MONITORS 100
#include "main.c"

void make_win_bg(NSWindow * win) {
    [win setStyleMask:NSWindowStyleMaskBorderless];
    [win setOpaque:NO];
    [win setBackgroundColor:[NSColor clearColor]];
    [win setHasShadow:NO];
    [win setLevel:kCGDesktopWindowLevel - 1];
    [win setCollectionBehavior:NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary];
    [win setFrame:[[win screen] frame] display:YES];
    [win setMovable:NO];
}

@interface AppDelegate : NSObject <NSApplicationDelegate>
- (void) applicationDidFinishLaunching : (NSNotification *) notification;
- (void) disable_login_item;
- (void) restart_app;
@property (nonatomic, strong) NSStatusItem *status_item;
@property (nonatomic, strong) NSImage *status_on_img;
@property (nonatomic, strong) NSImage *status_off_img;
@end

@implementation AppDelegate
- (void) applicationDidFinishLaunching : (NSNotification *) notification {
    self.status_item = [[NSStatusBar systemStatusBar]
        statusItemWithLength: NSVariableStatusItemLength
    ];

    self.status_on_img = [NSImage imageNamed:@"status_bar_icon_on"];
    [self.status_on_img setSize:NSMakeSize(22, 22)];

    self.status_off_img = [NSImage imageNamed:@"status_bar_icon_off"];
    [self.status_off_img setSize:NSMakeSize(22, 22)];

    [self.status_item setImage:self.status_off_img];
    [self.status_item setAlternateImage:self.status_on_img];
    [self.status_item setHighlightMode:YES];

    self.status_item.menu = [[NSMenu alloc] initWithTitle:@""];
    NSMenuItem *quit_item = [[NSMenuItem alloc]
        initWithTitle:@"Quit"
        action:@selector(terminate:)
        keyEquivalent:@"q"
    ];
    [quit_item setTarget:NSApp];
    [self.status_item.menu addItem:quit_item];

    [[NSNotificationCenter defaultCenter]
        addObserver: self
        selector: @selector(restart_app)
        name: NSApplicationDidChangeScreenParametersNotification
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
}

- (void) disable_login_item {
    SMAppService *service = [SMAppService mainAppService];
    
    // Unregister the service
    NSError *error = 0;
    BOOL success = [service unregisterAndReturnError:&error];
    
    if (!success) NSLog(@"Login item unregistration failed with error: %@", error);
}

- (void) restart_app {
    exit(0);

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        NSString *path = [[NSBundle mainBundle] bundlePath];
        NSTask *task = [[NSTask alloc] init];
        [task setLaunchPath:@"/usr/bin/open"];
        [task setArguments:@[path]];
        [task launch];
    });
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
    self = [super initWithFrame:frameRect];
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

    monitor_index = ctx.monitors_len++;

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
    [NSTimer scheduledTimerWithTimeInterval:
        interval
        target: self
        selector: @selector(update_display)
        userInfo: 0
        repeats: YES
    ];
}

- (void) update_display : (NSTimer *) timer {
    [self setNeedsDisplay:YES];
}

@end

// void request_auth() {
//     AuthorizationRef auth_ref = 0;
//     AuthorizationStatus status = AuthorizationCreate(
//         0,
//         kAuthorizationEmptyEnvironment,
//         kAuthorizationFlagDefaults,
//         &auth_ref
//     );

//     if (status == errAuthorizationSuccess) {
//         // Create an authorization item and execute a privileged operation
//         AuthorizationItem auth_item = { kAuthorizationRightExecute, 0, 0, 0 };
//         AuthorizationRights auth_rights = { 1, &authItem };
//         AuthorizationFlags flags = kAuthorizationFlagDefaults;

//         status = AuthorizationCopyRights(authRef, &authRights, kAuthorizationEmptyEnvironment, flags, NULL);

//         if (status == errAuthorizationSuccess) {
//             NSLog(@"Permission granted.");
//         } else {
//             NSLog(@"Permission denied.");
//         }

//         AuthorizationFree(authRef, kAuthorizationFlagDefaults);
//     }
// }

void alert(NSString *msg) {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setMessageText:msg];
    [alert setInformativeText:@"Information"];
    [alert addButtonWithTitle:@"Ok"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert runModal];
}

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSApplication *app = [NSApplication sharedApplication];

    AppDelegate *app_delegate = [[AppDelegate alloc] init];
    [app_delegate autorelease];
    [app setDelegate:app_delegate];

    NSArray<NSScreen *> *screens = [NSScreen screens];

    NSWindow *wins[arrlen(ctx.monitors)] = {0};
    MyDrawingView *views[arrlen(ctx.monitors)] = {0};

    for (int i = 0; i < [screens count]; i++) {
        NSRect frame = [screens[i] frame];
        wins[i] = [[NSWindow alloc]
            initWithContentRect: frame
            styleMask:NSWindowStyleMaskBorderless
            backing:NSBackingStoreBuffered
            defer: NO
        ];
        make_win_bg(wins[i]);

        views[i] = [[MyDrawingView alloc] initWithFrame:frame];
        [wins[i] setContentView:views[i]];
        [views[i] setup];
        [wins[i] orderFrontRegardless];
    }

    start();

    for (int i = 0; i < ctx.monitors_len; i++) {
        [views[i] start_animation];
    }

    // TODO: Create app delegate to handle system events.

    [NSApp run];
    [pool drain];
    return 0;
}
