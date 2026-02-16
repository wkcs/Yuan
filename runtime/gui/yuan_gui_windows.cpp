/// \file yuan_gui_windows.cpp
/// \brief Minimal Windows GUI runtime for Yuan std.gui backend (C ABI).

#include "yuan_gui_abi.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

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

static HWND gWindow = nullptr;
static bool gShouldClose = false;
static std::uint32_t gInputMask = 0;
static std::uint32_t gBgColor = 0x111827;
static std::vector<RectCmd> gRects;
static std::vector<LineCmd> gLines;
static std::vector<CircleCmd> gCircles;
static std::vector<TextCmd> gTexts;

static constexpr const wchar_t* kWindowClassName = L"YuanGuiWindowClass";

enum InputMask : std::uint32_t {
    InputUp = 1u << 0u,
    InputRight = 1u << 1u,
    InputDown = 1u << 2u,
    InputLeft = 1u << 3u,
    InputQuit = 1u << 4u,
    InputRestart = 1u << 5u,
};

static COLORREF colorRef(std::uint32_t packed) {
    const BYTE r = static_cast<BYTE>((packed >> 16u) & 0xffu);
    const BYTE g = static_cast<BYTE>((packed >> 8u) & 0xffu);
    const BYTE b = static_cast<BYTE>(packed & 0xffu);
    return RGB(r, g, b);
}

static std::wstring utf8ToWide(const char* text) {
    if (!text) {
        return std::wstring();
    }

    const int len = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (len <= 0) {
        return std::wstring();
    }

    // `len` includes the trailing NUL. Allocate full size to avoid overflow,
    // then drop the terminator for std::wstring usage.
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, out.data(), len);
    if (!out.empty() && out.back() == L'\0') {
        out.pop_back();
    }
    return out;
}

static void resetFrameCommands() {
    gRects.clear();
    gLines.clear();
    gCircles.clear();
    gTexts.clear();
}

static void render(HDC hdc) {
    RECT clientRect;
    GetClientRect(gWindow, &clientRect);

    HBRUSH bgBrush = CreateSolidBrush(colorRef(gBgColor));
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    for (const RectCmd& cmd : gRects) {
        RECT r;
        r.left = cmd.x;
        r.top = cmd.y;
        r.right = cmd.x + cmd.w;
        r.bottom = cmd.y + cmd.h;

        if (cmd.filled) {
            HBRUSH brush = CreateSolidBrush(colorRef(cmd.color));
            FillRect(hdc, &r, brush);
            DeleteObject(brush);
        } else {
            HPEN pen = CreatePen(PS_SOLID, 1, colorRef(cmd.color));
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(hdc, r.left, r.top, r.right, r.bottom);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);
        }
    }

    for (const LineCmd& cmd : gLines) {
        HPEN pen = CreatePen(PS_SOLID, 1, colorRef(cmd.color));
        HGDIOBJ oldPen = SelectObject(hdc, pen);
        MoveToEx(hdc, cmd.x0, cmd.y0, nullptr);
        LineTo(hdc, cmd.x1, cmd.y1);
        SelectObject(hdc, oldPen);
        DeleteObject(pen);
    }

    for (const CircleCmd& cmd : gCircles) {
        const int d = cmd.radius * 2;
        HBRUSH brush = CreateSolidBrush(colorRef(cmd.color));
        HGDIOBJ oldBrush = SelectObject(hdc, brush);
        HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, cmd.cx - cmd.radius, cmd.cy - cmd.radius,
                cmd.cx - cmd.radius + d, cmd.cy - cmd.radius + d);
        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(brush);
    }

    SetBkMode(hdc, TRANSPARENT);
    for (const TextCmd& cmd : gTexts) {
        SetTextColor(hdc, colorRef(cmd.color));
        std::wstring text = utf8ToWide(cmd.text.c_str());
        if (!text.empty()) {
            TextOutW(hdc, cmd.x, cmd.y, text.c_str(), static_cast<int>(text.size()));
        }
    }
}

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    (void)lParam;

    switch (msg) {
        case WM_CLOSE:
            gShouldClose = true;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            gShouldClose = true;
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            switch (wParam) {
                case VK_UP: gInputMask |= InputUp; break;
                case VK_RIGHT: gInputMask |= InputRight; break;
                case VK_DOWN: gInputMask |= InputDown; break;
                case VK_LEFT: gInputMask |= InputLeft; break;
                case VK_ESCAPE: gInputMask |= InputQuit; break;
                case VK_RETURN:
                case VK_SPACE: gInputMask |= InputRestart; break;
                case 'W': gInputMask |= InputUp; break;
                case 'D': gInputMask |= InputRight; break;
                case 'S': gInputMask |= InputDown; break;
                case 'A': gInputMask |= InputLeft; break;
                case 'R': gInputMask |= InputRestart; break;
                default: break;
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            render(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

static bool ensureWindowClass() {
    HINSTANCE hinst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hinst;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    const ATOM atom = RegisterClassExW(&wc);
    if (atom == 0) {
        const DWORD err = GetLastError();
        return err == ERROR_CLASS_ALREADY_EXISTS;
    }
    return true;
}

static void pumpEvents() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

extern "C" std::uintptr_t yuan_gui_init(std::uintptr_t width,
                                        std::uintptr_t height,
                                        std::uintptr_t titlePtr) {
    if (!ensureWindowClass()) {
        return 0;
    }

    if (!gWindow) {
        HINSTANCE hinst = GetModuleHandleW(nullptr);
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

        RECT rect;
        rect.left = 0;
        rect.top = 0;
        rect.right = static_cast<LONG>(width);
        rect.bottom = static_cast<LONG>(height);
        AdjustWindowRect(&rect, style, FALSE);

        gWindow = CreateWindowExW(
            0,
            kWindowClassName,
            L"Yuan GUI",
            style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            hinst,
            nullptr
        );
        if (!gWindow) {
            return 0;
        }
    }

    const char* ctitle = reinterpret_cast<const char*>(titlePtr);
    std::wstring title = utf8ToWide((ctitle && ctitle[0] != '\0') ? ctitle : "Yuan GUI");
    if (!title.empty()) {
        SetWindowTextW(gWindow, title.c_str());
    }

    ShowWindow(gWindow, SW_SHOW);
    UpdateWindow(gWindow);

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
    if (!gWindow) {
        return 0;
    }
    const char* ctitle = reinterpret_cast<const char*>(titlePtr);
    if (!ctitle || ctitle[0] == '\0') {
        return 0;
    }
    std::wstring title = utf8ToWide(ctitle);
    if (title.empty()) {
        return 0;
    }
    SetWindowTextW(gWindow, title.c_str());
    return 1;
}

extern "C" std::uintptr_t yuan_gui_end_frame() {
    if (!gWindow) {
        return 0;
    }
    InvalidateRect(gWindow, nullptr, FALSE);
    UpdateWindow(gWindow);
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
    if (gWindow) {
        DestroyWindow(gWindow);
        gWindow = nullptr;
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
extern "C" std::uintptr_t yuan_gui_sleep_ms(std::uintptr_t) { return 0; }
extern "C" std::uintptr_t yuan_gui_shutdown() { return 0; }

#endif
