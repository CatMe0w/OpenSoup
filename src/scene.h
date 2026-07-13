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

// Register a sprite; returns a STABLE sprite id (draw order may be
// reordered internally, ids never change). frames are premultiplied RGBA8
// (top-left origin); the scene keeps the pointers borrowed for alpha
// hit-testing - caller must keep them alive. speed_ms only matters when
// nframes > 1. group: sprites of one toy share a group; raising any of
// them raises the whole group (intra-group order = insertion order, i.e.
// zOrder if the caller adds sprites back-to-front).
int scene_sprite_add(int w, int h, int nframes, uint8_t* const* frames,
                     int speed_ms, float x_px, float y_px, int group);

// Attach a sprite to a physics body. (anchor_x, anchor_y) is the vector
// from the body origin to the sprite's visual centre (objectCentreOfMass
// from the toy definition), pixels, y-up. The FLC rotation frames are
// pre-rendered about the canvas centre, and the centre is a material point
// of the limb, so the offset rotates with the body's orientation.
void scene_sprite_bind_body(int sprite, int body, float anchor_x, float anchor_y);

// Lightweight unbound-sprite controls used by the Toybox UI.  They only
// change render metadata; no physics body or native platform window exists.
void scene_sprite_set_position(int sprite, float x_px, float y_px);
void scene_sprite_set_size(int sprite, float w_px, float h_px);
void scene_sprite_set_frame(int sprite, int frame);
void scene_sprite_set_alpha(int sprite, float alpha);
void scene_sprite_set_visible(int sprite, bool visible);
void scene_sprite_set_layer(int sprite, int layer);
void scene_sprite_set_uv_scale(int sprite, float u, float v);
void scene_sprite_set_clip(int sprite, bool enabled, float x_px, float y_px,
                           float w_px, float h_px);

// true if the point hits a non-transparent sprite pixel (drives click-through)
bool scene_hit_test(float x_px, float y_px);

// topmost sprite id whose opaque pixel covers the point, or -1
int scene_pick(float x_px, float y_px);

// physics body bound to a sprite, or -1
int scene_sprite_body(int sprite);

// destroy a sprite and its GPU resources (id becomes invalid)
void scene_sprite_remove(int sprite);

// raise the sprite's whole toy group to the front (stable within the group)
void scene_raise(int sprite);
