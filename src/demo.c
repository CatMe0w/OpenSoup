// Milestone demo: real Souptoys art + real toy-definition physics params.
// Hardcoded asset picks; replaced by full toy assembly once the defs tail
// (shapes/sprites) is decoded.
#include "demo.h"
#include "assets.h"
#include "physics.h"
#include "scene.h"
#include "toydefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* join(const char* root, const char* rel) {
    const size_t n = strlen(root) + strlen(rel) + 2;
    char* p = malloc(n);
    snprintf(p, n, "%s/%s", root, rel);
    return p;
}

// circle body from the toy's real definition; radius still sprite-derived
// until shape polygons are decoded
static void make_body(int sprite, const char* class_name, int w, int h,
                      float x_px, float drop_order) {
    const float r = ((float)(w < h ? w : h) / 2.0f) / PHYS_PX_PER_UNIT;
    phys_params p = {
        .mass = 3.14159f * r * r * 4.0f,
        .inertia = 3.14159f * r * r * 4.0f * r * r / 2.0f,
        .gravity = PHYS_GRAVITY,
        .mouse_stiffness = PHYS_MOUSE_STIFFNESS,
        .mouse_dampener = PHYS_MOUSE_DAMPENER,
        .air_linear = 0.1f,
        .air_angular = 0.1f,
        .anchored = false,
        .fixed_rotate = false,
    };
    const toydef_t* d = toydefs_find(class_name);
    if (d) {
        p.mass = d->mass;
        p.inertia = d->inertia_tensor;
        p.gravity = d->has_gravity_override ? d->gravity_override : PHYS_GRAVITY;
        p.mouse_stiffness = d->mouse_stiffness;
        p.mouse_dampener = d->mouse_dampener;
        p.air_linear = d->air_resistance_linear;
        p.air_angular = d->air_resistance_angular;
        p.anchored = d->fixed_move;
        p.fixed_rotate = d->fixed_rotate;
        printf("demo: %s def: mass=%.3g g=%.3g mouseK=%.3g mouseC=%.3g anchored=%d\n",
               class_name, p.mass, p.gravity, p.mouse_stiffness, p.mouse_dampener,
               p.anchored);
    } else {
        fprintf(stderr, "demo: no def for %s, using fallbacks\n", class_name);
    }
    const float y0 = p.anchored ? 4.0f : 6.0f + drop_order * 1.5f;
    const int body = phys_body_add(x_px / PHYS_PX_PER_UNIT, y0, r, &p);
    scene_sprite_bind_body(sprite, body);
}

static bool add_flc(const char* root, const char* class_name,
                    const char* rel_color, const char* rel_alpha,
                    float x, float drop_order) {
    char* cpath = join(root, rel_color);
    char* apath = rel_alpha ? join(root, rel_alpha) : NULL;
    // leaked on purpose: scene borrows the pixel buffers for hit-testing
    as_anim* anim = calloc(1, sizeof(as_anim));
    const bool ok = as_load_flc(cpath, apath, anim);
    if (ok) {
        const int sprite = scene_sprite_add(anim->w, anim->h, anim->frame_count,
                                            anim->frames, anim->speed_ms, x, 100);
        make_body(sprite, class_name, anim->w, anim->h, x, drop_order);
        printf("demo: %s (%dx%d, %d frames @ %dms)\n", rel_color, anim->w,
               anim->h, anim->frame_count, anim->speed_ms);
    } else {
        fprintf(stderr, "demo: FAILED %s\n", cpath);
    }
    free(cpath);
    free(apath);
    return ok;
}

int demo_load(const char* assets_root) {
    char* defs = join(assets_root, "toydefs.json");
    if (toydefs_load(defs)) {
        printf("demo: %d toy defs loaded\n", toydefs_count());
    } else {
        fprintf(stderr, "demo: toydefs.json missing at %s - fallback params\n", defs);
    }
    free(defs);

    int n = 0;
    // fixed scenery: anchored, gravity 0
    n += add_flc(assets_root, "PirateWind",
                 "toys_toybox_toy/graphics/U5 Pirate Wheel/wheel.flc",
                 "toys_toybox_toy/graphics/U5 Pirate Wheel/wheel_Alpha.flc", 300, 0);
    // bouncy ball: mass 1.5, default mouse spring
    n += add_flc(assets_root, "U1Bouncy",
                 "toys_data_toy/Graphics/U1 Bouncy/bouncy.flc",
                 "toys_data_toy/Graphics/U1 Bouncy/bouncy_Alpha.flc", 600, 1);
    // balloon: POSITIVE gravity override, floats up
    n += add_flc(assets_root, "BalloonBlue",
                 "toys_data_toy/Graphics/Small Balloon/Blue/Balloon.flc",
                 "toys_data_toy/Graphics/Small Balloon/Blue/Balloon_Alpha.flc", 900, 2);
    // bear body: stiff mouse spring override (k=1000, c=30) - snappy grab
    n += add_flc(assets_root, "U6Bluebear",
                 "toys_toybox_toy/graphics/U6 Bluebear/Body/Body.flc",
                 "toys_toybox_toy/graphics/U6 Bluebear/Body/Body_Alpha.flc", 1200, 3);
    return n;
}
