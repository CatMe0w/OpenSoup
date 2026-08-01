// Platform-agnostic application: subsystem boot, per-frame heartbeat, and
// input policy.
#include "opensoup.h"
#include "app_assets.h"
#include "assets_layout.h"
#include "audio.h"
#include "host_dialog.h"
#include "physics.h"
#include "rubyhost.h"
#include "scene.h"
#include "sprite_hook.h"
#include "toybox.h"
#include "toydefs.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

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

    // Cross-referenced classes (Goose -> GooseEgg, etc.) must resolve
    // before dependent scripts run; preload all defs up front.
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

// Framework refits scale on wall changes; renderer re-reads each frame.
static void sync_view_transform(void) {
    double origin_x, origin_y, px_per_unit;
    if (rbh_view_transform(&origin_x, &origin_y, &px_per_unit)) {
        scene_set_view_transform((float)origin_x, (float)origin_y,
                                 (float)px_per_unit);
    }
}

void opensoup_start(float view_w, float view_h) {
    rbh_screen_size(view_w, view_h);
    sync_view_transform();
    sprite_hook_install(g_assets_root);
    const bool toybox_ok = toybox_init(g_assets_root, view_w, view_h);
    printf("OpenSoup up: Toybox %s (%d icons) from %s\n",
           toybox_ok ? "ready" : "unavailable", toybox_catalog_count(),
           g_assets_root);
}

void opensoup_resize(float view_w, float view_h) {
    rbh_screen_size(view_w, view_h);
    sync_view_transform();
    toybox_resize(view_w, view_h);
}

// Mouse events route through Ruby; the framework's grab drives
// engine.input_grab/move/release. captured = the mouse-downed sprite.
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
        // barely-moved release = click
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

static char loaded_playset[1024];
static bool modal_dialog_up;

static void open_playset(const char* path) {
    if (!rbh_open_playset(path)) {
        fprintf(stderr, "opensoup: cannot open playset %s\n", path);
        return;
    }
    if (path != loaded_playset) {
        snprintf(loaded_playset, sizeof loaded_playset, "%s", path);
    }
    printf("opensoup: opened playset %s\n", path);
}

static const char* installed_playsets(char* buffer, size_t cap) {
    if (!app_assets_sibling_path(g_assets_root, APP_ASSETS_PLAYSETS,
                                 buffer, cap)) {
        return NULL;
    }
    struct stat info;
    return stat(buffer, &info) == 0 && S_ISDIR(info.st_mode) ? buffer : NULL;
}

static void choose_playset(void) {
    if (!host_dialog_can_open_file()) {
        fprintf(stderr, "opensoup: this host cannot show a file dialog\n");
        return;
    }
    char directory[1152];
    char path[1024];
    if (host_dialog_open_file("Open Playset",
                              installed_playsets(directory, sizeof directory),
                              "Playset", "playset", path, sizeof path)) {
        open_playset(path);
    }
}

static void run_toybox_command(toybox_command command) {
    switch (command) {
        case TOYBOX_COMMAND_OPEN_PLAYSET:
            modal_dialog_up = true;
            choose_playset();
            modal_dialog_up = false;
            break;
        case TOYBOX_COMMAND_RESTART_PLAYSET:
            if (loaded_playset[0]) {
                open_playset(loaded_playset);
            } else {
                printf("opensoup: no playset to restart\n");
            }
            break;
        case TOYBOX_COMMAND_NONE:
            break;
    }
}

// Diagnostic dump: scene, contacts and replay, deferred to the next step.
#define DUMP_WAIT_FRAMES 300

static FILE* dump_out;
static int dump_wait;
static bool dump_pending;

static bool dump_directory(char* out, size_t cap) {
    if (!app_assets_sibling_path(g_assets_root, "diagnostics", out, cap)) {
        return false;
    }
#if defined(__MINGW32__)
    mkdir(out); // the Windows CRT takes no mode
#else
    mkdir(out, 0777);
#endif
    return true;
}

static FILE* dump_open(const char* stamp, const char* extension) {
    char dir[1024], path[1152];
    if (!dump_directory(dir, sizeof dir)) {
        return NULL;
    }
    const int n = snprintf(path, sizeof path, "%s/dump-%s.%s", dir, stamp,
                           extension);
    if (n < 0 || (size_t)n >= sizeof path) {
        return NULL;
    }
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "opensoup: cannot write %s\n", path);
        return NULL;
    }
    printf("opensoup: diagnostic dump -> %s\n", path);
    return f;
}

void opensoup_diagnostics_request(void) {
    dump_pending = true;
}

// Replay first: it captures pre-step state.
static void dump_begin(void) {
    dump_pending = false;
    if (dump_out) {
        return; // one still waiting for a step
    }
    char stamp[40];
    const time_t now = time(NULL);
    struct tm parts;
#if defined(_WIN32) || defined(__MINGW32__)
    parts = *localtime(&now);
#else
    localtime_r(&now, &parts);
#endif
    strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", &parts);
    // Two presses inside one second must not overwrite each other.
    static char previous[40];
    static int repeat;
    repeat = strcmp(stamp, previous) == 0 ? repeat + 1 : 0;
    snprintf(previous, sizeof previous, "%s", stamp);
    if (repeat) {
        snprintf(stamp + strlen(stamp), sizeof stamp - strlen(stamp), "-%d",
                 repeat);
    }

    FILE* replay = dump_open(stamp, "rb");
    if (replay) {
        rbh_dump_replay(replay);
        fclose(replay);
    }
    dump_out = dump_open(stamp, "txt");
    if (!dump_out) {
        return;
    }
    rbh_dump_scene(dump_out);
    phys_debug_dump(dump_out);
    phys_debug_capture_begin(dump_out);
    dump_wait = DUMP_WAIT_FRAMES;
}

static void dump_settle(void) {
    if (!dump_out) {
        return;
    }
    if (phys_debug_capture_armed() && --dump_wait > 0) {
        return; // no step ran in this frame; the arm stays up
    }
    if (phys_debug_capture_armed()) {
        fprintf(dump_out, "\nno step ran within %d frames "
                          "(engine paused or stopped)\n", DUMP_WAIT_FRAMES);
        phys_debug_capture_end();
    }
    fclose(dump_out);
    dump_out = NULL;
}

opensoup_frame_result opensoup_frame(double dt_ms, float cursor_x,
                                     float cursor_y, bool cursor_valid) {
    opensoup_frame_result r = { .wants_mouse = true, .quit = false };
    if (modal_dialog_up) {
        return r; // re-entered from a dialog's own event loop
    }
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
    if (dump_pending) {
        dump_begin();
    }
    rbh_frame(dt_ms); // Ruby heartbeat: run_steps + dispatch_timers
    dump_settle();
    toybox_frame(dt_ms);
    run_toybox_command(toybox_take_command());
    sync_view_transform();
    r.quit = toybox_quit_requested();
    return r;
}

void opensoup_shutdown(void) {
    toybox_shutdown(); // removes its scene sprites, so before scene teardown
    scene_shutdown();
    rbh_shutdown();
    audio_shutdown();
}
