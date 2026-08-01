#pragma once
#include <stdbool.h>
#include <stdio.h>

// Embedded Ruby 1.8.6 host. Boots the engine's Ruby API and framework.
// Requires toydefs_load() first; assets_root layout per assets_layout.h.
bool rbh_boot(const char* assets_root);

// Evaluate Ruby source; prints exception detail tagged with `what` on failure.
bool rbh_eval(const char* code, const char* what);

// Sprite hook: called per limb on realization. com is objectCentreOfMass in
// sprite pixels, y-up; group = toy instance id. Return scene sprite id or -1.
typedef int (*rbh_sprite_fn)(const char* image, int body, float com_x,
                             float com_y, int group, void* user);
typedef void (*rbh_sprite_remove_fn)(int sprite, void* user);
void rbh_set_sprite_hook(rbh_sprite_fn fn, rbh_sprite_remove_fn remove_fn,
                         void* user);

// Instantiate a toy at (x_m, y_m) world meters via the Ruby framework.
bool rbh_spawn_toy(const char* class_name, const char* class_dir,
                   double x_m, double y_m);

// View pixels (y-down) -> scene metres (y-up).
bool rbh_view_to_scene(double x_px, double y_px, double* x_m, double* y_m);

// The inverse mapping, in the renderer's terms: view px of the scene's
// bottom-left corner, and logical px per scene unit. Both change whenever the
// framework refits the scene, so the renderer must re-read them per frame.
bool rbh_view_transform(double* origin_x_px, double* origin_y_px,
                        double* px_per_unit);

// Resolve a toy class without instantiating (preload for cross-references).
bool rbh_load_toy_class(const char* class_name, const char* class_dir);

// Freeze the Toybox catalog after all toy class scripts have run.
typedef struct {
    const char* id;
    const char* license;
    const char* sprite_path;
    float order;
} rbh_toypack;

bool rbh_catalog_finalize(void);
int rbh_toypack_count(void);
const rbh_toypack* rbh_toypack_at(int index);
const char* rbh_toy_pack(const char* class_name);

// Per-frame heartbeat: fixed 0.01s steps + timer dispatch.
void rbh_frame(double dt_ms);

// Mouse dispatch. sprite = scene sprite id; coordinates are view px, y-down.
void rbh_mouse_down(int sprite, double x_px, double y_px, int button);
void rbh_mouse_move(int sprite, double x_px, double y_px, int button,
                    bool down);
void rbh_mouse_up(int sprite, double x_px, double y_px, int button);
void rbh_mouse_click(int sprite, double x_px, double y_px, int button);

// Remove the toy owning this sprite. Sticky toys are not recyclable.
bool rbh_recycle_sprite(int sprite);

// Clear: restore scene defaults and remove every non-sticky toy.
bool rbh_clear_scene(void);

// Load an original .playset: replaces the non-sticky toys and the world.
bool rbh_open_playset(const char* path);

// Report the view size in logical pixels; triggers wall recalculation.
void rbh_screen_size(double w_px, double h_px);

// Diagnostic dumps (rubyhost_dump.c)
void rbh_dump_scene(FILE* out);
void rbh_dump_replay(FILE* out);

void rbh_shutdown(void);
