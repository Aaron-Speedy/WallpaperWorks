#include <objc/NSObjCRuntime.h>
#include <objc/objc-runtime.h>

#define msg(r, o, s) ((r(*)(id, SEL))objc_msgSend)(o, sel_getUid(s))
#define msg1(r, o, s, A, a) ((r(*)(id, SEL, A))objc_msgSend)(o, sel_getUid(s), a)
#define msg2(r, o, s, A, a, B, b) ((r(*)(id, SEL, A, B))objc_msgSend)(o, sel_getUid(s), a, b)
#define msg3(r, o, s, A, a, B, b, C, c) ((r(*)(id, SEL, A, B, C))objc_msgSend)(o, sel_getUid(s), a, b, c)
#define msg4(r, o, s, A, a, B, b, C, c, D, d) ((r(*)(id, SEL, A, B, C, D))objc_msgSend)(o, sel_getUid(s), a, b, c, d)

#define cls(x) ((id)objc_getClass(x))

extern id const NSDefaultRunLoopMode;
extern id const NSApp;
int main() {
    msg(id, cls("NSApplication"), "sharedApplication");
    msg1(void, NSApp, "setActivationPolicy:", NSInteger, 0);
    id wnd = msg4(id, msg(id, cls("NSWindow"), "alloc"), "initWithContentRect:styleMask:backing:defer:", CGRect, CGRectMake(0, 0, 320, 240), NSUInteger, 3, NSUInteger, 2, BOOL, NO);
    id title = msg1(id, cls("NSString"), "stringWithUTF8String:", const char *, "Hello, Cocoa");
    msg1(void, wnd, "setTitle:", id, title);
    msg1(void, wnd, "makeKeyAndOrderFront:", id, nil);
    msg(void, wnd, "center");
    msg1(void, NSApp, "activateIgnoringOtherApps:", BOOL, YES);
    for (;;) {
        id ev = msg4(id, NSApp, "nextEventMatchingMask:untilDate:inMode:dequeue:", NSUInteger, NSUIntegerMax, id, NULL, id, NSDefaultRunLoopMode, BOOL, YES);
        msg1(void, NSApp, "sendEvent:", id, ev);
    }
    msg(void, wnd, "close");
    return 0;
}
