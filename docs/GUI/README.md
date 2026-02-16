# Yuan `std.gui` 设计说明（FFI 后端）

## 目标

`std.gui` 提供一套最小、跨平台、立即模式（immediate-mode）GUI 抽象：

- Yuan 侧只依赖 `stdlib/gui.yu`
- 平台相关逻辑在动态库中实现
- 通过 `std.ffi` 绑定统一 C ABI

## 分层

1. `stdlib/gui.yu`
   - 负责加载后端动态库
   - 查找 `yuan_gui_*` 符号
   - 暴露统一 API（窗口配置、几何绘制、输入状态、生命周期）
   - `open(window_options(...)) -> !Context`
2. 平台后端动态库
   - macOS: `build/gui/libyuan_gui_macos.dylib`
   - Linux: `build/gui/libyuan_gui_linux.so`
   - Windows: `build/gui/yuan_gui_windows.dll`

## ABI 合同

头文件：`runtime/gui/yuan_gui_abi.h`

必须导出以下符号（`extern "C"`）：

- `yuan_gui_init(width, height, title_ptr) -> uintptr_t`
- `yuan_gui_should_close() -> uintptr_t`
- `yuan_gui_begin_frame() -> uintptr_t`
- `yuan_gui_clear_rgb(color) -> uintptr_t`
- `yuan_gui_fill_rect(x, y, w, h, color) -> uintptr_t`
- `yuan_gui_draw_rect(x, y, w, h, color) -> uintptr_t`
- `yuan_gui_draw_line(x0, y0, x1, y1, color) -> uintptr_t`
- `yuan_gui_fill_circle(cx, cy, radius, color) -> uintptr_t`
- `yuan_gui_draw_text(text_ptr, x, y, color) -> uintptr_t`
- `yuan_gui_set_title(title_ptr) -> uintptr_t`
- `yuan_gui_end_frame() -> uintptr_t`
- `yuan_gui_poll_input() -> uintptr_t`
- `yuan_gui_sleep_ms(ms) -> uintptr_t`
- `yuan_gui_shutdown() -> uintptr_t`

约定：

- 成功返回 `1`，失败返回 `0`（`poll_input` 返回位掩码）
- 所有参数按 `uintptr_t` 透传
- 字符串参数为 UTF-8 且以 `\0` 结尾
- 颜色格式为 `0xRRGGBB`
- 坐标原点在左上角，单位为像素

## 输入位掩码

与 `stdlib/gui.yu` 对齐：

- `KEY_UP = 1`
- `KEY_RIGHT = 2`
- `KEY_DOWN = 4`
- `KEY_LEFT = 8`
- `KEY_QUIT = 16`
- `KEY_ACTION = 32`

`poll_input` 返回 `Input` 对象，支持：
- `input.pressed(KEY_*)`
- `input.up()/right()/down()/left()`
- `input.quit()/action()`

## 生命周期

1. `open(window_options(...))! -> err { ... }` -> 后端 `yuan_gui_init`
2. 每帧：`begin` -> `clear` -> `fill/stroke/line/fill_disk/text` -> `end`
3. 每帧轮询：`poll_input` + `should_close`
4. 退出：`close`（或 `shutdown`）

## 已实现后端

- `runtime/gui/yuan_gui_macos.mm`
  - 基于 Cocoa (`NSWindow` + `NSView`)
- `runtime/gui/yuan_gui_linux.cpp`
  - 基于 X11
- `runtime/gui/yuan_gui_windows.cpp`
  - 基于 Win32/GDI
