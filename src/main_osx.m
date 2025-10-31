#import <Cocoa/Cocoa.h>

@interface CircleView : NSView
@end

@implementation CircleView

-(BOOL) isOpaque {
    return NO;
}

-(void) drawRect : (NSRect) dirtyRect {
    [super drawRect:dirtyRect];

    NSRect bounds = [self bounds];
    
    CGFloat padding = 0.0;
    CGFloat minDim = MIN(bounds.size.width, bounds.size.height) - (2 * padding);
    
    NSRect circleRect = NSMakeRect(
        bounds.origin.x + (bounds.size.width - minDim) / 2.0,
        bounds.origin.y + (bounds.size.height - minDim) / 2.0,
        minDim,
        minDim
    );

    NSBezierPath *circlePath = [NSBezierPath bezierPathWithOvalInRect:circleRect];
    
    [[NSColor systemBlueColor] setFill];
    [circlePath fill]; 
    
    [[NSColor darkGrayColor] setStroke];
    [circlePath setLineWidth:3.0];
    [circlePath stroke];
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) NSWindow *window;
@end

@implementation AppDelegate

-(void) applicationDidFinishLaunching : (NSNotification *) aNotification {
    // 1. Create the window frame and style
    NSRect frame = NSMakeRect(200, 200, 300, 300);
    NSUInteger styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;
    
    // 2. Initialize the NSWindow. NSBackingStoreBuffered ensures drawing uses a buffer.
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                             styleMask:styleMask
                                               backing:NSBackingStoreBuffered 
                                                 defer:NO];
    
    [self.window setTitle:@"Title of Window"];
    [self.window setMinSize:NSMakeSize(150, 150)];
    
    // 3. Create and configure the custom drawing view
    CircleView *circleView = [[CircleView alloc] initWithFrame:[[self.window contentView] bounds]];
    // Ensure the circle resizes when the window resizes
    [circleView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    
    // 4. Set the custom view as the window's content view
    [self.window setContentView:circleView];
    
    // 5. Display the window
    [self.window makeKeyAndOrderFront:nil];
}

-(BOOL) applicationShouldTerminateAfterLastWindowClosed : (NSApplication *) sender {
    return YES;
}

@end


// --- Main Function (The Entry Point) ---

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *application = [NSApplication sharedApplication];
        
        AppDelegate *delegate = [[AppDelegate alloc] init];
        [application setDelegate:delegate];
        
        [application run];
    }
    return 0;
}
