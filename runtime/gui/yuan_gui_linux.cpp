/// \file yuan_gui_linux.cpp
/// \brief Minimal Linux GUI runtime for Yuan std.gui backend (C ABI, X11).

#include "yuan_gui_abi.h"

#include <cstdint>

#if defined(__linux__) && defined(__has_include)
#if __has_include(<X11/Xlib.h>) && __has_include(<X11/Xutil.h>) && __has_include(<X11/keysym.h>)
#define YUAN_GUI_HAS_X11 1
#endif
#endif

#ifdef YUAN_GUI_HAS_X11

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

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

static Display* gDisplay = nullptr;
static Window gWindow = 0;
static GC gGc = 0;
static Atom gWmDelete = 0;
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

static unsigned long colorToPixel(std::uint32_t packed) {
    const unsigned long r = (packed >> 16u) & 0xffu;
    const unsigned long g = (packed >> 8u) & 0xffu;
    const unsigned long b = packed & 0xffu;
    return (r << 16u) | (g << 8u) | b;
}

static void resetFrameCommands() {
    gRects.clear();
    gLines.clear();
    gCircles.clear();
    gTexts.clear();
}

static void handleKeyPress(XKeyEvent* ev) {
    if (!ev) {
        return;
    }

    KeySym key = XLookupKeysym(ev, 0);
    switch (key) {
        case XK_Up: gInputMask |= InputUp; break;
        case XK_Right: gInputMask |= InputRight; break;
        case XK_Down: gInputMask |= InputDown; break;
        case XK_Left: gInputMask |= InputLeft; break;
        case XK_Escape: gInputMask |= InputQuit; break;
        case XK_Return: gInputMask |= InputRestart; break;
        case XK_space: gInputMask |= InputRestart; break;
        case XK_w:
        case XK_W: gInputMask |= InputUp; break;
        case XK_d:
        case XK_D: gInputMask |= InputRight; break;
        case XK_s:
        case XK_S: gInputMask |= InputDown; break;
        case XK_a:
        case XK_A: gInputMask |= InputLeft; break;
        case XK_r:
        case XK_R: gInputMask |= InputRestart; break;
        default: break;
    }
}

static void pumpEvents() {
    if (!gDisplay || !gWindow) {
        return;
    }

    while (XPending(gDisplay) > 0) {
        XEvent ev;
        XNextEvent(gDisplay, &ev);
        switch (ev.type) {
            case ClientMessage:
                if (static_cast<Atom>(ev.xclient.data.l[0]) == gWmDelete) {
                    gShouldClose = true;
                }
                break;
            case DestroyNotify:
                gShouldClose = true;
                break;
            case KeyPress:
                handleKeyPress(&ev.xkey);
                break;
            default:
                break;
        }
    }
}

static void drawFrame() {
    if (!gDisplay || !gWindow || !gGc) {
        return;
    }

    XSetForeground(gDisplay, gGc, colorToPixel(gBgColor));
    XWindowAttributes attrs;
    XGetWindowAttributes(gDisplay, gWindow, &attrs);
    XFillRectangle(gDisplay, gWindow, gGc, 0, 0,
                   static_cast<unsigned int>(attrs.width),
                   static_cast<unsigned int>(attrs.height));

    for (const RectCmd& cmd : gRects) {
        XSetForeground(gDisplay, gGc, colorToPixel(cmd.color));
        const unsigned int w = cmd.w > 0 ? static_cast<unsigned int>(cmd.w) : 0u;
        const unsigned int h = cmd.h > 0 ? static_cast<unsigned int>(cmd.h) : 0u;
        if (cmd.filled) {
            XFillRectangle(gDisplay, gWindow, gGc, cmd.x, cmd.y, w, h);
        } else {
            XDrawRectangle(gDisplay, gWindow, gGc, cmd.x, cmd.y, w, h);
        }
    }

    for (const LineCmd& cmd : gLines) {
        XSetForeground(gDisplay, gGc, colorToPixel(cmd.color));
        XDrawLine(gDisplay, gWindow, gGc, cmd.x0, cmd.y0, cmd.x1, cmd.y1);
    }

    for (const CircleCmd& cmd : gCircles) {
        XSetForeground(gDisplay, gGc, colorToPixel(cmd.color));
        const int diameter = cmd.radius * 2;
        const int x = cmd.cx - cmd.radius;
        const int y = cmd.cy - cmd.radius;
        XFillArc(gDisplay, gWindow, gGc, x, y,
                 static_cast<unsigned int>(diameter),
                 static_cast<unsigned int>(diameter),
                 0, 360 * 64);
    }

    for (const TextCmd& cmd : gTexts) {
        XSetForeground(gDisplay, gGc, colorToPixel(cmd.color));
        XDrawString(gDisplay, gWindow, gGc, cmd.x, cmd.y + 14,
                    cmd.text.c_str(), static_cast<int>(cmd.text.size()));
    }

    XFlush(gDisplay);
}

extern "C" std::uintptr_t yuan_gui_init(std::uintptr_t width,
                                        std::uintptr_t height,
                                        std::uintptr_t titlePtr) {
    if (!gDisplay) {
        gDisplay = XOpenDisplay(nullptr);
        if (!gDisplay) {
            return 0;
        }
    }

    const int screen = DefaultScreen(gDisplay);
    const Window root = RootWindow(gDisplay, screen);

    if (!gWindow) {
        gWindow = XCreateSimpleWindow(gDisplay,
                                      root,
                                      100,
                                      100,
                                      static_cast<unsigned int>(width),
                                      static_cast<unsigned int>(height),
                                      1,
                                      BlackPixel(gDisplay, screen),
                                      WhitePixel(gDisplay, screen));
        if (!gWindow) {
            return 0;
        }

        XSelectInput(gDisplay, gWindow,
                     ExposureMask | KeyPressMask | StructureNotifyMask);

        gWmDelete = XInternAtom(gDisplay, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(gDisplay, gWindow, &gWmDelete, 1);

        gGc = XCreateGC(gDisplay, gWindow, 0, nullptr);
        if (!gGc) {
            return 0;
        }
    }

    const char* ctitle = reinterpret_cast<const char*>(titlePtr);
    if (ctitle && ctitle[0] != '\0') {
        XStoreName(gDisplay, gWindow, ctitle);
    } else {
        XStoreName(gDisplay, gWindow, "Yuan GUI");
    }

    XMapWindow(gDisplay, gWindow);
    XFlush(gDisplay);

    gShouldClose = false;
    gInputMask = 0;
    resetFrameCommands();
    pumpEvents();
    return 1;
}

extern "C" std::uintptr_t yuan_gui_should_close() {
    pumpEvents();
    return gShouldClose ? 1u : 0u;
}

extern "C" std::uintptr_t yuan_gui_begin_frame() {
    pumpEvents();
    resetFrameCommands();
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
    if (ctext) {
        cmd.text = ctext;
    }
    cmd.x = static_cast<int>(x);
    cmd.y = static_cast<int>(y);
    cmd.color = static_cast<std::uint32_t>(packedColor & 0x00ffffffu);
    gTexts.push_back(std::move(cmd));
    return 1;
}

extern "C" std::uintptr_t yuan_gui_set_title(std::uintptr_t titlePtr) {
    if (!gDisplay || !gWindow) {
        return 0;
    }

    const char* ctitle = reinterpret_cast<const char*>(titlePtr);
    if (!ctitle || ctitle[0] == '\0') {
        return 0;
    }

    XStoreName(gDisplay, gWindow, ctitle);
    XFlush(gDisplay);
    return 1;
}

extern "C" std::uintptr_t yuan_gui_end_frame() {
    drawFrame();
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
    if (gDisplay) {
        if (gGc) {
            XFreeGC(gDisplay, gGc);
            gGc = 0;
        }
        if (gWindow) {
            XDestroyWindow(gDisplay, gWindow);
            gWindow = 0;
        }
        XCloseDisplay(gDisplay);
        gDisplay = nullptr;
    }

    gShouldClose = true;
    resetFrameCommands();
    return 1;
}

#else

extern "C" std::uintptr_t yuan_gui_init(std::uintptr_t, std::uintptr_t, std::uintptr_t) { return 0; }
extern "C" std::uintptr_t yuan_gui_should_close() { return 1; }
extern "C" std::uintptr_t yuan_gui_begin_frame() { return 0; }
extern "C" std::uintptr_t yuan_gui_clear_rgb(std::uintptr_t) { return 0; }
extern "C" std::uintptr_t yuan_gui_fill_rect(std::uintptr_t, std::uintptr_t, std::uintptr_t,
                                             std::uintptr_t, std::uintptr_t) { return 0; }
extern "C" std::uintptr_t yuan_gui_draw_rect(std::uintptr_t, std::uintptr_t, std::uintptr_t,
                                             std::uintptr_t, std::uintptr_t) { return 0; }
extern "C" std::uintptr_t yuan_gui_draw_line(std::uintptr_t, std::uintptr_t, std::uintptr_t,
                                             std::uintptr_t, std::uintptr_t) { return 0; }
extern "C" std::uintptr_t yuan_gui_fill_circle(std::uintptr_t, std::uintptr_t, std::uintptr_t,
                                               std::uintptr_t) { return 0; }
extern "C" std::uintptr_t yuan_gui_draw_text(std::uintptr_t, std::uintptr_t, std::uintptr_t,
                                             std::uintptr_t) { return 0; }
extern "C" std::uintptr_t yuan_gui_set_title(std::uintptr_t) { return 0; }
extern "C" std::uintptr_t yuan_gui_end_frame() { return 0; }
extern "C" std::uintptr_t yuan_gui_poll_input() { return 0; }
extern "C" std::uintptr_t yuan_gui_sleep_ms(std::uintptr_t) { return 1; }
extern "C" std::uintptr_t yuan_gui_shutdown() { return 1; }

#endif
