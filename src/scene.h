#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sokol_gfx.h"

// Platform-agnostic scene. Coordinates are DEVICE PIXELS, origin top-left,
// y-down (the original engine's canvas space, GDI convention). The platform
// layer converts window points to device pixels once; NDC only exists inside
// the vertex shader.

void scene_setup(const sg_environment* env);
void scene_frame(const sg_swapchain* swapchain, double dt_ms);
void scene_shutdown(void);

// Register a sprite. frames are premultiplied RGBA8 (top-left origin); the
// scene keeps the pointers borrowed for alpha hit-testing - caller must keep
// them alive. speed_ms only matters when nframes > 1.
int scene_sprite_add(int w, int h, int nframes, uint8_t* const* frames,
                     int speed_ms, float x_px, float y_px);

// Attach a sprite to a physics body (sprite centre follows the body).
// Grabbing a bound sprite drives the body; releasing throws it.
void scene_sprite_bind_body(int sprite, int body);

// true if the point hits a non-transparent sprite pixel (drives click-through)
bool scene_hit_test(float x_px, float y_px);

bool scene_grab_begin(float x_px, float y_px);
void scene_grab_move(float x_px, float y_px);
void scene_grab_end(void);
bool scene_grabbing(void);
