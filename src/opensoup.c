// The platform-agnostic application: boots the subsystems, orchestrates the
// per-frame heartbeat, and owns the input policy the original Toybox kept in
// its Win32 window procedures.
#include "opensoup.h"
#include "assets_layout.h"
#include "audio.h"
#include "rubyhost.h"
#include "scene.h"
#include "sprite_hook.h"
#include "toybox.h"
#include "toydefs.h"
#include <math.h>
#include <stdio.h>

static const char* g_assets_root;

bool opensoup_boot(const char* assets_root) {
    g_assets_root = assets_root;
    if (!audio_init(false)) {
        fprintf(stderr, "opensoup: audio unavailable, continuing silent\n");
    }
    if (!toydefs_load(assets_root)) {
        fprintf(stderr, "no toy defs under %s\n", assets_root);
    }
    if (!rbh_boot(assets_root)) {
        return false;
    }
    printf("opensoup: Ruby framework booted\n");

    // Classes referenced by other toys' scripts (Goose -> GooseEgg,
    // SnowballCannon -> SnowballLarge) must resolve before those scripts
    // run, so preload every def's class up front. Defs without a script
    // resolve to plain Toy subclasses, same as loading them on drop.
    int preloaded = 0;
    for (int i = 0; i < toydefs_count(); i++) {
        const toydef_t* d = toydefs_at(i);
        if (!d->class_name || !d->root) {
            continue;
        }
        char dir[2048];
        container_resource_root(dir, sizeof dir, assets_root, d->root);
        if (rbh_load_toy_class(d->class_name, dir)) {
            preloaded++;
        }
    }
    printf("opensoup: Preloaded %d/%d toy classes\n", preloaded,
           toydefs_count());
    if (!rbh_catalog_finalize()) {
        fprintf(stderr, "opensoup: could not finalize Toybox catalog\n");
        return false;
    }
    printf("opensoup: Registered %d Toybox packs from CToy scripts\n",
           rbh_toypack_count());
    return true;
}

void opensoup_start(float view_w, float view_h) {
    rbh_screen_size(view_w, view_h);
    sprite_hook_install(g_assets_root);
    const bool toybox_ok = toybox_init(g_assets_root, view_w, view_h);
    printf("OpenSoup up: Toybox %s (%d icons) from %s\n",
           toybox_ok ? "ready" : "unavailable", toybox_catalog_count(),
           g_assets_root);
}

void opensoup_resize(float view_w, float view_h) {
    rbh_screen_size(view_w, view_h);
    toybox_resize(view_w, view_h);
}

// Mouse events route through Ruby: pick the sprite here (alpha test is
// scene-side), then Sprite#internal_mouse_* bubbles the event and the
// framework's default grab (limb.rb) drives engine.input_grab/move/release.
// captured = the mouse-downed sprite, our stand-in for Win32 mouse capture.
static int captured_sprite = -1;
static float down_pos[2];

void opensoup_mouse_down(float x_px, float y_px) {
    if (toybox_mouse_down(x_px, y_px)) {
        captured_sprite = -1;
        return;
    }
    const int sprite = scene_pick(x_px, y_px);
    if (sprite >= 0) {
        scene_raise(sprite);
        captured_sprite = sprite;
        down_pos[0] = x_px;
        down_pos[1] = y_px;
        rbh_mouse_down(sprite, x_px, y_px, 1);
    }
}

void opensoup_mouse_drag(float x_px, float y_px) {
    if (toybox_capturing()) {
        toybox_mouse_dragged(x_px, y_px);
    } else if (captured_sprite >= 0) {
        rbh_mouse_move(captured_sprite, x_px, y_px, 1, true);
    }
}

void opensoup_mouse_up(float x_px, float y_px) {
    if (toybox_capturing()) {
        toybox_mouse_up(x_px, y_px);
    } else if (captured_sprite >= 0) {
        const bool over_toybox = toybox_hit_test(x_px, y_px);
        rbh_mouse_up(captured_sprite, x_px, y_px, 1);
        const bool recycled = over_toybox
                           && rbh_recycle_sprite(captured_sprite);
        // barely-moved release = click (Win32 sends it on button release)
        if (!over_toybox && !recycled
            && fabsf(x_px - down_pos[0]) < 4
            && fabsf(y_px - down_pos[1]) < 4) {
            rbh_mouse_click(captured_sprite, x_px, y_px, 1);
        }
        captured_sprite = -1;
    }
}

void opensoup_scroll(float x_px, float y_px, float delta_y, bool precise) {
    if (toybox_hit_test(x_px, y_px)) {
        toybox_scroll(delta_y, precise);
    }
}

opensoup_frame_result opensoup_frame(double dt_ms, float cursor_x,
                                     float cursor_y, bool cursor_valid) {
    opensoup_frame_result r = { .wants_mouse = true, .quit = false };
    // per-pixel click-through: hit-test the polled cursor against the
    // Toybox and the scene. Never release the window mid-drag.
    if (captured_sprite < 0 && !toybox_capturing()) {
        r.wants_mouse = false;
        if (cursor_valid) {
            toybox_pointer_move(cursor_x, cursor_y);
            r.wants_mouse = toybox_hit_test(cursor_x, cursor_y)
                         || scene_hit_test(cursor_x, cursor_y);
        }
    }
    rbh_frame(dt_ms); // Ruby heartbeat: run_steps + dispatch_timers
    toybox_frame(dt_ms);
    r.quit = toybox_quit_requested();
    return r;
}

void opensoup_shutdown(void) {
    toybox_shutdown(); // removes its scene sprites, so before scene teardown
    scene_shutdown();
    rbh_shutdown();
    audio_shutdown();
}
