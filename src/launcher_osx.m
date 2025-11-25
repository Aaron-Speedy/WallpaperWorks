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

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSString *currentPath = [fileManager currentDirectoryPath];
    NSString *executablePath = [[NSBundle mainBundle] executablePath];
    NSString *parentDirectory = [executablePath stringByDeletingLastPathComponent];
    NSLog(@"Current Directory Path: %@", currentPath);
    NSLog(@"Executable Path: %@", parentDirectory);

    [NSApp run];
    [pool drain];
    return 0;
}
