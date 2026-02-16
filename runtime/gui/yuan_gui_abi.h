#pragma once

#include <stdint.h>

#if defined(_WIN32)
#if defined(YUAN_GUI_EXPORTS)
#define YUAN_GUI_API __declspec(dllexport)
#else
#define YUAN_GUI_API __declspec(dllimport)
#endif
#else
#define YUAN_GUI_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Returns 1 on success, 0 on failure.
YUAN_GUI_API uintptr_t yuan_gui_init(uintptr_t width, uintptr_t height, uintptr_t title_ptr);
YUAN_GUI_API uintptr_t yuan_gui_should_close(void);
YUAN_GUI_API uintptr_t yuan_gui_begin_frame(void);
YUAN_GUI_API uintptr_t yuan_gui_clear_rgb(uintptr_t packed_color);
YUAN_GUI_API uintptr_t yuan_gui_fill_rect(uintptr_t x,
                             uintptr_t y,
                             uintptr_t w,
                             uintptr_t h,
                             uintptr_t packed_color);
YUAN_GUI_API uintptr_t yuan_gui_draw_rect(uintptr_t x,
                             uintptr_t y,
                             uintptr_t w,
                             uintptr_t h,
                             uintptr_t packed_color);
YUAN_GUI_API uintptr_t yuan_gui_draw_line(uintptr_t x0,
                             uintptr_t y0,
                             uintptr_t x1,
                             uintptr_t y1,
                             uintptr_t packed_color);
YUAN_GUI_API uintptr_t yuan_gui_fill_circle(uintptr_t cx,
                               uintptr_t cy,
                               uintptr_t radius,
                               uintptr_t packed_color);
YUAN_GUI_API uintptr_t yuan_gui_draw_text(uintptr_t text_ptr,
                             uintptr_t x,
                             uintptr_t y,
                             uintptr_t packed_color);
YUAN_GUI_API uintptr_t yuan_gui_set_title(uintptr_t title_ptr);
YUAN_GUI_API uintptr_t yuan_gui_end_frame(void);
YUAN_GUI_API uintptr_t yuan_gui_poll_input(void);
YUAN_GUI_API uintptr_t yuan_gui_sleep_ms(uintptr_t ms);
YUAN_GUI_API uintptr_t yuan_gui_shutdown(void);

#ifdef __cplusplus
}
#endif
