#include <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ServiceManagement/SMAppService.h>
#include <Foundation/Foundation.h>

#include <stdint.h>

extern void NSLog(NSString * format, ...);

Boolean SMLoginItemSetEnabled(CFStringRef identifier, Boolean enabled);

@interface AppDelegate : NSObject <NSApplicationDelegate>
- (void) applicationDidFinishLaunching : (NSNotification *) notification;
- (void) enable_login_item;
- (void) disable_login_item;
@property (nonatomic, strong) NSStatusItem *status_item;
@end

@implementation AppDelegate

- (void) applicationDidFinishLaunching : (NSNotification *) notification {
    [self enable_login_item];
}

- (void) enable_login_item {
    SMAppService *service = [SMAppService mainAppService];

    NSError *error = nil;
    BOOL success = [service registerAndReturnError:&error];
    if (!success) NSLog(@"Login item registration failed with error: %@", error); // TODO: show an alert or something
}

- (void) disable_login_item {
    SMAppService *service = [SMAppService mainAppService];
    
    NSError *error = nil;
    BOOL success = [service unregisterAndReturnError:&error];
    
    if (!success) NSLog(@"Login item unregistration failed with error: %@", error);
}

@end

int main(void) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSApplication *app = [NSApplication sharedApplication];

    AppDelegate *app_delegate = [[AppDelegate alloc] init];
    [app_delegate autorelease];
    [app setDelegate:app_delegate];

    [NSApplication sharedApplication];

    // TODO: Create app delegate to handle system events.

    @autoreleasepool {
        do {
            NSFileManager file_manager = [NSFileManager defaultManager];
            NSBundle bundle = [NSBundle mainBundle]
            NSString *cur_path = [file_manager currentDirectoryPath];
            NSString *exec_path = [bundle executablePath];
            NSString *bundle_path = [bundle bundlePath];
            NSString *replacement_path = @"";
            NSLog(@"Current Directory Path: %@", cur_path);
            NSLog(@"Bundle Path: %@", bundle_path);

            NSError *error = nil;
            if (![file_manager fileExistsAtPath:bundle_path]) break;
            if (![file_manager fileExistsAtPath:bundle_path]) break;
        } while (0);
    }

    // 1. Check for updates
    // 2. If updates exist, download them to a temporary location.
    // 3. Unzip the updated application.
    // 4. Run the updater script in the update. At the same time, close the original app.
    // 5. Clse

    [NSApp run];
    [pool drain];
    return 0;
}
