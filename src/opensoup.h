#pragma once
#include <stdbool.h>

// The platform-agnostic application: subsystem boot, per-frame
// orchestration, and the input policy (mouse capture, click detection,
// recycle-on-drop, click-through). The platform layer owns the window, GPU
// swapchain, and event pump; it translates native events into these calls
// and applies the returned decisions to its window. Coordinates are logical
// view pixels, origin top-left, y-down. Per frame: opensoup_frame first,
// then scene_frame with the platform's swapchain.

// Boot order and tolerance: audio (failure = continue silent) -> toydefs
// (missing = warn, continue) -> Ruby boot (failure = false; the platform
// shows its native alert). Ruby 1.8's conservative GC records the stack
// base at ruby_init, so call this at least as shallow as any later Ruby
// call: straight from main, before the platform's event loop starts.
bool opensoup_boot(const char* assets_root);

// After the platform has run scene_setup: report the view size to Ruby,
// install the sprite hook, and build the Toybox.
void opensoup_start(float view_w, float view_h);

void opensoup_resize(float view_w, float view_h);

typedef struct {
    bool wants_mouse; // cursor over interactive pixels; never false mid-drag
    bool quit;        // Toybox close animation finished; terminate the app
} opensoup_frame_result;

// Drive the Ruby heartbeat and the Toybox animation. (cursor_x, cursor_y)
// is the polled global pointer in view coordinates, used for the per-pixel
// click-through decision; pass cursor_valid = false if unavailable (the
// window then stays click-through unless a drag holds it).
opensoup_frame_result opensoup_frame(double dt_ms, float cursor_x,
                                     float cursor_y, bool cursor_valid);

// Left-button mouse events, already translated to view coordinates.
void opensoup_mouse_down(float x_px, float y_px);
void opensoup_mouse_drag(float x_px, float y_px);
void opensoup_mouse_up(float x_px, float y_px);
// precise=false: delta_y is in wheel detents; precise=true: logical points.
void opensoup_scroll(float x_px, float y_px, float delta_y, bool precise);

void opensoup_shutdown(void);
