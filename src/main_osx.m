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

Boolean SMLoginItemSetEnabled(CFStringRef identifier, Boolean enabled);

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

@interface AppDelegate : NSObject <NSApplicationDelegate>
- (void) applicationDidFinishLaunching : (NSNotification *) notification;
- (NSMenu *) create_status_menu;
- (void) enable_login_item;
- (void) disable_login_item;

@property (nonatomic, strong) NSStatusItem *status_item;
@end

@implementation AppDelegate
- (void) applicationDidFinishLaunching : (NSNotification *) notification {
    NSStatusBar *status_bar = [NSStatusBar systemStatusBar];
    self.status_item = [status_bar statusItemWithLength:NSVariableStatusItemLength];
    NSStatusBarButton *button = self.status_item.button;

    if (button) {
        NSImage *icon = [NSImage imageNamed:@"favicon"];
        [icon setTemplate:NO];
        [button setImage:icon];
        // [button setTitle:@"WHATEVER ... MOM ... UGHHHHHHHHH"];
        // [button setToolTip:@"WHATEVER ... MOM ... UGHHHHHHHHH"];
    }

    self.status_item.menu = [self create_status_menu];

    [self enable_login_item];
}

- (NSMenu *) create_status_menu {
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];

    NSMenuItem *quit_item = [[NSMenuItem alloc] initWithTitle:@"Quit" 
                                                     action:@selector(terminate:) 
                                              keyEquivalent:@"q"];
    [quit_item setTarget:NSApp];
    [menu addItem:quit_item];

    return menu;
}

- (void) enable_login_item {
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

- (void) disable_login_item {
    SMAppService *service = [SMAppService mainAppService];
    
    // Unregister the service
    NSError *error = 0;
    BOOL success = [service unregisterAndReturnError:&error];
    
    if (!success) NSLog(@"Login item unregistration failed with error: %@", error);
}

@end

@interface MyDrawingView : NSView {
    CGContextRef bitmap_ctx;
    unsigned char *buf;
    size_t w, h;
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
    context = (Context) { .dpi = get_dpi([NSScreen mainScreen]).width, .screen = &screen, };
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
                                       userInfo:0 
                                        repeats:YES];
    Image screen = { .buf = buf, .alloc_w = w, .w = w, .h = h, };
    context = (Context) { .dpi = get_dpi([NSScreen mainScreen]).width, .screen = &screen, };

    start(&context);
}

- (void) update_display : (NSTimer *) timer {
    [self setNeedsDisplay:YES];
}

@end

s8 arena_printf(Arena *perm, const char *fmt, ...) {
    s8 ret = {0};
    va_list args;
    va_start(args, fmt);
        int len = vsnprintf(0, 0, fmt, args);
        char *block = new(perm, char, len);
        vsnprintf(block, len, fmt, args);
    va_end(args);
    return (s8) { .buf = block, .len = len, };
}

// void request_auth() {
//     AuthorizationRef auth_ref;
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
    [alert addButtonWithTitle:@"Cancel"];
    [alert addButtonWithTitle:@"Ok"];
    [alert runModal];
}

int main(int argc, char *argv[]) {
    Arena scratch = new_arena(10 * KiB);

    if (argc == 3 && !strcmp(argv[1], "--launch_updater")) {
        @autoreleasepool {
            alert("Yo yo yo yo!");
            NSFileManager *file_manager = [NSFileManager defaultManager];
            NSString *old_bundle = [
                NSString stringWithCString:argv[2]
                encoding:NSUTF8StringEncoding
            ];
            NSString *new_bundle = [[NSBundle mainBundle] bundlePath];

            if ([file_manager fileExistsAtPath:old_bundle]) {
                NSError *error = 0;
                if ([file_manager removeItemAtPath:old_bundle error:&error]) {
                    NSLog(@"Folder deleted successfully.");
                } else {
                    NSLog(
                        @"Could not delete folder: //@",
                        [error localizedDescription]
                    );
                }
            }

            if ([file_manager fileExistsAtPath:new_bundle]) {
                NSError *error = 0;
                [file_manager
                    copyItemAtPath:old_bundle
                    toPath:[old_bundle stringByDeletingLastPathComponent]
                    error:error
                ];
            }


            if ([file_manager fileExistsAtPath:old_bundle]) {
                NSTask *task = [[NSTask alloc] init];
                NSError *error = 0;
                [task setLaunchPath:@"/usr/bin/open"];
                [task setArguments:@[
                    @"/Users/aaron/Programming/WallpaperWorks/WallpaperWorks.app",
                    @"--args",
                    @"--todo_remove_this_flag",
                ]];
                [task launchAndReturnError:&error];
            }

        }

        exit(0);
    }

    bool needs_update = argc > 1;
    if (needs_update) {
        @autoreleasepool {
            NSTask *task = [[NSTask alloc] init];
            NSError *error = 0;
            [task setLaunchPath:@"/usr/bin/open"];
            [task setArguments:@[
                @"/Users/aaron/Programming/WallpaperWorks/WallpaperWorks.app",
                @"--args",
                @"--launch_updater",
                [[NSBundle mainBundle] bundlePath],
            ]];
            [task launchAndReturnError:&error];
            exit(0);
        }
    }

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSApplication *app = [NSApplication sharedApplication];

    AppDelegate *app_delegate = [[AppDelegate alloc] init];
    [app_delegate autorelease];
    [app setDelegate:app_delegate];

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
