/// \file yuan_gui_macos.mm
/// \brief Minimal macOS GUI runtime for Yuan std.gui backend (C ABI).

#import <Cocoa/Cocoa.h>

#include "yuan_gui_abi.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <utility>
#include <vector>

struct RectCmd {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    std::uint32_t color = 0;
    bool filled = true;
};

struct LineCmd {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    std::uint32_t color = 0;
};

struct CircleCmd {
    int cx = 0;
    int cy = 0;
    int radius = 0;
    std::uint32_t color = 0;
};

struct TextCmd {
    std::string text;
    int x = 0;
    int y = 0;
    std::uint32_t color = 0;
};

static NSWindow* gWindow = nil;
static NSView* gView = nil;
static id gWindowDelegate = nil;
static bool gShouldClose = false;
static std::uint32_t gInputMask = 0;
static std::uint32_t gBgColor = 0x111827;
static std::vector<RectCmd> gRects;
static std::vector<LineCmd> gLines;
static std::vector<CircleCmd> gCircles;
static std::vector<TextCmd> gTexts;

enum InputMask : std::uint32_t {
    InputUp = 1u << 0u,
    InputRight = 1u << 1u,
    InputDown = 1u << 2u,
    InputLeft = 1u << 3u,
    InputQuit = 1u << 4u,
    InputRestart = 1u << 5u,
};

static NSColor* colorFromRGB(std::uint32_t packed) {
    const CGFloat r = static_cast<CGFloat>((packed >> 16u) & 0xffu) / 255.0;
    const CGFloat g = static_cast<CGFloat>((packed >> 8u) & 0xffu) / 255.0;
    const CGFloat b = static_cast<CGFloat>(packed & 0xffu) / 255.0;
    return [NSColor colorWithSRGBRed:r green:g blue:b alpha:1.0];
}

static CGFloat flipY(CGFloat totalHeight, int y) {
    return totalHeight - static_cast<CGFloat>(y);
}

@interface YuanSnakeWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation YuanSnakeWindowDelegate
- (BOOL)windowShouldClose:(id)sender {
    (void)sender;
    gShouldClose = true;
    return YES;
}
@end

@interface YuanSnakeView : NSView
@end

@implementation YuanSnakeView
- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)keyDown:(NSEvent*)event {
    NSString* chars = [event charactersIgnoringModifiers];
    if ([chars length] > 0) {
        unichar ch = [chars characterAtIndex:0];
        if (ch == 'w' || ch == 'W') gInputMask |= InputUp;
        if (ch == 'd' || ch == 'D') gInputMask |= InputRight;
        if (ch == 's' || ch == 'S') gInputMask |= InputDown;
        if (ch == 'a' || ch == 'A') gInputMask |= InputLeft;
        if (ch == 'r' || ch == 'R' || ch == ' ') gInputMask |= InputRestart;
        if (ch == 27) gInputMask |= InputQuit;
    }

    switch ([event keyCode]) {
        case 126: gInputMask |= InputUp; break;
        case 124: gInputMask |= InputRight; break;
        case 125: gInputMask |= InputDown; break;
        case 123: gInputMask |= InputLeft; break;
        case 36: gInputMask |= InputRestart; break;
        case 53: gInputMask |= InputQuit; break;
        default: break;
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    (void)dirtyRect;
    const NSRect bounds = [self bounds];

    [colorFromRGB(gBgColor) setFill];
    NSRectFill(bounds);

    for (const RectCmd& cmd : gRects) {
        [colorFromRGB(cmd.color) set];
        const CGFloat yBottom = flipY(bounds.size.height, cmd.y + cmd.h);
        const NSRect rect = NSMakeRect(cmd.x, yBottom, cmd.w, cmd.h);
        if (cmd.filled) {
            NSRectFill(rect);
        } else {
            [NSBezierPath strokeRect:rect];
        }
    }

    for (const LineCmd& cmd : gLines) {
        [colorFromRGB(cmd.color) setStroke];
        NSBezierPath* path = [NSBezierPath bezierPath];
        [path setLineWidth:1.0];
        [path moveToPoint:NSMakePoint(cmd.x0, flipY(bounds.size.height, cmd.y0))];
        [path lineToPoint:NSMakePoint(cmd.x1, flipY(bounds.size.height, cmd.y1))];
        [path stroke];
    }

    for (const CircleCmd& cmd : gCircles) {
        [colorFromRGB(cmd.color) setFill];
        const int d = cmd.radius * 2;
        const CGFloat yBottom = flipY(bounds.size.height, cmd.cy + cmd.radius);
        const NSRect oval = NSMakeRect(cmd.cx - cmd.radius, yBottom, d, d);
        NSBezierPath* path = [NSBezierPath bezierPathWithOvalInRect:oval];
        [path fill];
    }

    NSDictionary* baseAttrs = @{
        NSFontAttributeName : [NSFont boldSystemFontOfSize:16.0]
    };
    for (const TextCmd& cmd : gTexts) {
        NSMutableDictionary* attrs = [baseAttrs mutableCopy];
        attrs[NSForegroundColorAttributeName] = colorFromRGB(cmd.color);

        NSString* text = [NSString stringWithUTF8String:cmd.text.c_str()];
        if (!text) {
            continue;
        }
        const CGFloat y = bounds.size.height - static_cast<CGFloat>(cmd.y + 18);
        [text drawAtPoint:NSMakePoint(cmd.x, y) withAttributes:attrs];
    }
}
@end

static void pumpEvents() {
    @autoreleasepool {
        for (;;) {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                untilDate:[NSDate dateWithTimeIntervalSinceNow:0.0]
                                                   inMode:NSDefaultRunLoopMode
                                                  dequeue:YES];
            if (!event) {
                break;
            }
            [NSApp sendEvent:event];
        }
        [NSApp updateWindows];
    }
}

extern "C" std::uintptr_t yuan_gui_init(std::uintptr_t width,
                                        std::uintptr_t height,
                                        std::uintptr_t titlePtr) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        if (!gView) {
            gView = [[YuanSnakeView alloc] initWithFrame:NSMakeRect(
                0, 0, static_cast<CGFloat>(width), static_cast<CGFloat>(height))];
        }

        if (!gWindow) {
            const NSWindowStyleMask style = NSWindowStyleMaskTitled |
                                            NSWindowStyleMaskClosable |
                                            NSWindowStyleMaskMiniaturizable;
            gWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(
                                                     100, 100,
                                                     static_cast<CGFloat>(width),
                                                     static_cast<CGFloat>(height))
                                                  styleMask:style
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
            gWindowDelegate = [[YuanSnakeWindowDelegate alloc] init];
            [gWindow setDelegate:gWindowDelegate];
            [gWindow setContentView:gView];
        }

        const char* ctitle = reinterpret_cast<const char*>(titlePtr);
        NSString* title = nil;
        if (ctitle && ctitle[0] != '\0') {
            title = [NSString stringWithUTF8String:ctitle];
        }
        if (!title) {
            title = @"Yuan GUI";
        }

        [gWindow setTitle:title];
        [gWindow makeKeyAndOrderFront:nil];
        [gWindow makeFirstResponder:gView];
        [NSApp activateIgnoringOtherApps:YES];

        gShouldClose = false;
        gInputMask = 0;
        gRects.clear();
        gLines.clear();
        gCircles.clear();
        gTexts.clear();
        pumpEvents();
    }
    return 1;
}

extern "C" std::uintptr_t yuan_gui_should_close() {
    pumpEvents();
    return gShouldClose ? 1u : 0u;
}

extern "C" std::uintptr_t yuan_gui_begin_frame() {
    pumpEvents();
    gRects.clear();
    gLines.clear();
    gCircles.clear();
    gTexts.clear();
    return 1;
}

extern "C" std::uintptr_t yuan_gui_clear_rgb(std::uintptr_t packedColor) {
    gBgColor = static_cast<std::uint32_t>(packedColor & 0x00ffffffu);
    return 1;
}

extern "C" std::uintptr_t yuan_gui_fill_rect(std::uintptr_t x,
                                             std::uintptr_t y,
                                             std::uintptr_t w,
                                             std::uintptr_t h,
                                             std::uintptr_t packedColor) {
    RectCmd cmd;
    cmd.x = static_cast<int>(x);
    cmd.y = static_cast<int>(y);
    cmd.w = static_cast<int>(w);
    cmd.h = static_cast<int>(h);
    cmd.color = static_cast<std::uint32_t>(packedColor & 0x00ffffffu);
    cmd.filled = true;
    gRects.push_back(cmd);
    return 1;
}

extern "C" std::uintptr_t yuan_gui_draw_rect(std::uintptr_t x,
                                             std::uintptr_t y,
                                             std::uintptr_t w,
                                             std::uintptr_t h,
                                             std::uintptr_t packedColor) {
    RectCmd cmd;
    cmd.x = static_cast<int>(x);
    cmd.y = static_cast<int>(y);
    cmd.w = static_cast<int>(w);
    cmd.h = static_cast<int>(h);
    cmd.color = static_cast<std::uint32_t>(packedColor & 0x00ffffffu);
    cmd.filled = false;
    gRects.push_back(cmd);
    return 1;
}

extern "C" std::uintptr_t yuan_gui_draw_line(std::uintptr_t x0,
                                             std::uintptr_t y0,
                                             std::uintptr_t x1,
                                             std::uintptr_t y1,
                                             std::uintptr_t packedColor) {
    LineCmd cmd;
    cmd.x0 = static_cast<int>(x0);
    cmd.y0 = static_cast<int>(y0);
    cmd.x1 = static_cast<int>(x1);
    cmd.y1 = static_cast<int>(y1);
    cmd.color = static_cast<std::uint32_t>(packedColor & 0x00ffffffu);
    gLines.push_back(cmd);
    return 1;
}

extern "C" std::uintptr_t yuan_gui_fill_circle(std::uintptr_t cx,
                                               std::uintptr_t cy,
                                               std::uintptr_t radius,
                                               std::uintptr_t packedColor) {
    CircleCmd cmd;
    cmd.cx = static_cast<int>(cx);
    cmd.cy = static_cast<int>(cy);
    cmd.radius = static_cast<int>(radius);
    cmd.color = static_cast<std::uint32_t>(packedColor & 0x00ffffffu);
    gCircles.push_back(cmd);
    return 1;
}

extern "C" std::uintptr_t yuan_gui_draw_text(std::uintptr_t textPtr,
                                             std::uintptr_t x,
                                             std::uintptr_t y,
                                             std::uintptr_t packedColor) {
    const char* ctext = reinterpret_cast<const char*>(textPtr);
    TextCmd cmd;
    cmd.text = ctext ? ctext : "";
    cmd.x = static_cast<int>(x);
    cmd.y = static_cast<int>(y);
    cmd.color = static_cast<std::uint32_t>(packedColor & 0x00ffffffu);
    gTexts.push_back(std::move(cmd));
    return 1;
}

extern "C" std::uintptr_t yuan_gui_set_title(std::uintptr_t titlePtr) {
    @autoreleasepool {
        if (!gWindow) {
            return 0;
        }

        const char* ctitle = reinterpret_cast<const char*>(titlePtr);
        if (!ctitle || ctitle[0] == '\0') {
            return 0;
        }

        NSString* title = [NSString stringWithUTF8String:ctitle];
        if (!title) {
            return 0;
        }
        [gWindow setTitle:title];
    }
    return 1;
}

extern "C" std::uintptr_t yuan_gui_end_frame() {
    @autoreleasepool {
        if (gView) {
            [gView setNeedsDisplay:YES];
            [gView displayIfNeeded];
        }
    }
    pumpEvents();
    return 1;
}

extern "C" std::uintptr_t yuan_gui_poll_input() {
    pumpEvents();
    const std::uint32_t mask = gInputMask;
    gInputMask = 0;
    return static_cast<std::uintptr_t>(mask);
}

extern "C" std::uintptr_t yuan_gui_sleep_ms(std::uintptr_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(ms)));
    return 1;
}

extern "C" std::uintptr_t yuan_gui_shutdown() {
    @autoreleasepool {
        if (gWindow) {
            [gWindow close];
        }
        gWindow = nil;
        gView = nil;
        gWindowDelegate = nil;
    }
    gShouldClose = true;
    gRects.clear();
    gLines.clear();
    gCircles.clear();
    gTexts.clear();
    return 1;
}
