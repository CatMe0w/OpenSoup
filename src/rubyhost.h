#pragma once
#include <stdbool.h>

// Embedded Ruby 1.8.6 host. Registers the engine's Ruby API surface
// (src/ruby_api.inc), then mirrors the original Toybox.exe boot:
// $:.clear -> matrix/e2mmap from resources -> $core/$default_engine/
// $ui_engine/$grid_engine/$engine globals -> eval souptoys.rb bootstrap ->
// load world.rb -> $core.core_loaded (which builds the World toy per engine).
//
// scripts_root is the resource root for Souptoys#resource_load, currently the
// extracted souptoys_core.toy directory. Requires toydefs_load() to have run
// (Toy allocation pulls limb data from the defs by class name).

bool rbh_boot(const char* scripts_root);

// Evaluate Ruby source; on exception prints class/message/backtrace tagged
// with `what` and returns false.
bool rbh_eval(const char* code, const char* what);

// Visual bridge: called once per limb sprite (back-to-front by zOrder) when a
// toy is realized into the default engine. image is the def's path relative
// to the assets root; (com_x, com_y) is objectCentreOfMass in sprite pixels,
// y-up; group is the toy instance id (sprites of one toy share it). Return
// the scene sprite id (or -1). Unset = headless, sprites skipped.
typedef int (*rbh_sprite_fn)(const char* image, int body, float com_x,
                             float com_y, int group, void* user);
// called on toy teardown for each scene sprite the toy created
typedef void (*rbh_sprite_remove_fn)(int sprite, void* user);
void rbh_set_sprite_hook(rbh_sprite_fn fn, rbh_sprite_remove_fn remove_fn,
                         void* user);

// Instantiate a toy through the Ruby framework: ToyClassResolver resolves
// class_name (loading class_dir/<name>.rb if the toy has a script), then
// .new -> move to (x_m, y_m) world meters -> $default_engine.toys << toy,
// which realizes physics bodies/joints and fires the sprite hook.
bool rbh_spawn_toy(const char* class_name, const char* class_dir,
                   double x_m, double y_m);

// Convert device-pixel view coordinates to the default engine's scene space.
// Used by non-Ruby UI (Toybox drag/drop) before handing a spawn to Ruby.
bool rbh_view_to_scene(double x_px, double y_px, double* x_m, double* y_m);

// resolve (and load, if the toy has a script) a toy class without
// instantiating it - for classes other toys reference (Goose -> GooseEgg)
bool rbh_load_toy_class(const char* class_name, const char* class_dir);

// Per-frame heartbeat: accumulates dt into fixed 0.01s steps and drives
// $default_engine.run_steps + dispatch_timers through the framework.
void rbh_frame(double dt_ms);

// Mouse dispatch. sprite = the scene sprite id the app picked (alpha
// hit-test); coordinates are view space (device px, y-down). Events run
// the framework chain: Sprite#internal_mouse_* -> bubble -> default grab.
void rbh_mouse_down(int sprite, double x_px, double y_px, int button);
void rbh_mouse_move(int sprite, double x_px, double y_px, int button,
                    bool down);
void rbh_mouse_up(int sprite, double x_px, double y_px, int button);
void rbh_mouse_click(int sprite, double x_px, double y_px, int button);

// Resolve a rendered sprite back to its owning default-engine Toy. Sticky
// toys (notably World) are never recyclable. Recycling removes the whole Toy
// through its ToyContainer, so physics, visuals, sounds, and Ruby engine
// state follow the same teardown path as a script-driven removal.
bool rbh_recycle_sprite(int sprite);

// Original Toybox "clear": restore the default scene geometry/settings,
// notify toys with on_clear hooks, then remove every non-sticky scene toy.
bool rbh_clear_scene(void);

// Report the view size in device pixels. Fires $core.screen_size_changed;
// engines with fit_to_screen re-derive canvas/scene rects, which lands in
// phys_set_world via the scene_walls_changed chain.
void rbh_screen_size(double w_px, double h_px);

void rbh_shutdown(void);
