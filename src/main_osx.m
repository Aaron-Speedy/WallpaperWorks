#import <Cocoa/Cocoa.h>

// --- Custom NSView Subclass (The Drawing Component) ---

@interface CircleView : NSView
@end

@implementation CircleView

// Set to NO to allow the area outside the drawn circle to be transparent (optional)
- (BOOL)isOpaque {
    return NO;
}

// The method where drawing takes place onto the view's backing buffer
- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    // 1. Calculate the bounding rectangle for a centered circle
    NSRect bounds = [self bounds];
    
    // Inset to provide padding and calculate the smaller dimension for a perfect circle
    CGFloat padding = 20.0;
    CGFloat minDim = MIN(bounds.size.width, bounds.size.height) - (2 * padding);
    
    // Calculate the final, centered rectangle
    NSRect circleRect = NSMakeRect(
        bounds.origin.x + (bounds.size.width - minDim) / 2.0,
        bounds.origin.y + (bounds.size.height - minDim) / 2.0,
        minDim,
        minDim
    );

    // 2. Use NSBezierPath to create the geometric path for the circle
    NSBezierPath *circlePath = [NSBezierPath bezierPathWithOvalInRect:circleRect];
    
    // 3. Set the color and fill the path (drawing to the buffered graphics context)
    [[NSColor systemBlueColor] setFill];
    [circlePath fill]; 
    
    // 4. (Optional) Draw a border
    [[NSColor darkGrayColor] setStroke];
    [circlePath setLineWidth:3.0];
    [circlePath stroke];
}

@end

// --- Application Delegate (The Setup Component) ---

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    // 1. Create the window frame and style
    NSRect frame = NSMakeRect(200, 200, 300, 300);
    NSUInteger styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;
    
    // 2. Initialize the NSWindow. NSBackingStoreBuffered ensures drawing uses a buffer.
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                             styleMask:styleMask
                                               backing:NSBackingStoreBuffered 
                                                 defer:NO];
    
    [self.window setTitle:@"Single-File Cocoa Circle"];
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

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

@end


// --- Main Function (The Entry Point) ---

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // Create the application instance
        NSApplication *application = [NSApplication sharedApplication];
        
        // Create and set the application delegate
        AppDelegate *delegate = [[AppDelegate alloc] init];
        [application setDelegate:delegate];
        
        // Start the application run loop
        [application run];
    }
    return 0;
}
