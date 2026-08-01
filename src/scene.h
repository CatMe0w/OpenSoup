#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sokol_gfx.h"

// Platform-agnostic scene. View/canvas/sprite coordinates are LOGICAL PIXELS,
// origin top-left, y-down (the original engine's GDI convention). Physics is
// metres, origin bottom-left, y-up. Backing pixels are confined to the final
// logical-viewport -> swapchain mapping; NDC only exists in the vertex shader.

void scene_setup(const sg_environment* env);
void scene_frame(const sg_swapchain* swapchain, float view_w, float view_h,
                 double dt_ms);
void scene_shutdown(void);

// Scene's bottom-left in logical px, plus scale. Framework updates this on
// wall changes; defaults to PHYS_PX_PER_UNIT from the view's bottom-left.
void scene_set_view_transform(float origin_x_px, float origin_y_px,
                              float px_per_unit);

// Group-id namespace. Groups are only ever compared for equality, but toy
// instance ids grow without bound, so the partition is by sign:
//   >= 1  toy sprites, group = the toy's instance id
//   <  0  native UI clusters, fixed ids below
#define SCENE_GROUP_TOY(instance_id) (instance_id)
#define SCENE_GROUP_UI      (-1) // Toybox shell + catalog icons
#define SCENE_GROUP_UI_DRAG (-2) // drag preview: own group so hit-test and
                                 // raise treat it apart from the shell

// Layer namespace: world sprites stay on the default layer, native UI above.
#define SCENE_LAYER_WORLD 0
#define SCENE_LAYER_UI 100
#define SCENE_LAYER_UI_DRAG 110

// Returns a stable sprite id. Frames are premultiplied RGBA8 (top-left
// origin), borrowed for alpha hit-testing (keep alive). Group sprites are
// raised together; intra-group order = insertion order.
int scene_sprite_add(int w, int h, int nframes, uint8_t* const* frames,
                     int speed_ms, float x_px, float y_px, int group);

// Bind to a physics body. Anchor = body origin to visual centre (pixels,
// y-up); rotates with body orientation.
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
