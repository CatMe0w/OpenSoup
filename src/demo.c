// Milestone demo: whole toys assembled from their real definitions
// multi-limb bodies, polygon/circle collision shapes, spring joints,
// per-limb sprites with real anchors and zOrder. Toy picks are still
// hardcoded; the toybox menu UI comes later.
#include "demo.h"
#include "assets.h"
#include "physics.h"
#include "scene.h"
#include "toydefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LIMBS 64
#define MAX_SHAPE_PTS 160

static char* join(const char* root, const char* rel) {
    const size_t n = strlen(root) + strlen(rel) + 2;
    char* p = malloc(n);
    snprintf(p, n, "%s/%s", root, rel);
    return p;
}

// Souptoys ships transparency as "<name>_Alpha.flc" next to the color FLC
static char* alpha_variant(const char* color_path) {
    const char* dot = strrchr(color_path, '.');
    if (!dot) {
        return NULL;
    }
    const size_t base = (size_t)(dot - color_path);
    char* p = malloc(base + sizeof("_Alpha") + strlen(dot));
    memcpy(p, color_path, base);
    strcpy(p + base, "_Alpha");
    strcat(p, dot);
    FILE* f = fopen(p, "rb");
    if (!f) {
        free(p);
        return NULL;
    }
    fclose(f);
    return p;
}

// One physics body per limb: collision points from the def's colliding
// shapes, everything scaled from toy-local units into world meters.
static int make_limb_body(const td_limb* l, float wx, float wy, float scale) {
    phys_point pts[MAX_SHAPE_PTS];
    int npts = 0;
    for (int s = 0; s < l->nshapes; s++) {
        const td_shape* sh = &l->shapes[s];
        if (!sh->collides) {
            continue; // grab-only shapes don't collide
        }
        for (int p = 0; p < sh->npoints && npts < MAX_SHAPE_PTS; p++) {
            pts[npts++] = (phys_point){ sh->points[p].x * scale,
                                        sh->points[p].y * scale,
                                        sh->points[p].r * scale };
        }
    }
    const phys_params prm = {
        .mass = l->mass,
        .inertia = l->inertia * scale * scale, // def inertia is toy-local units^2
        .gravity = l->has_gravity_override ? l->gravity_override : PHYS_GRAVITY,
        .mouse_stiffness = l->mouse_stiffness,
        .mouse_dampener = l->mouse_dampener,
        .air_linear = l->air_resistance_linear,
        .air_angular = l->air_resistance_angular,
        .anchored = l->fixed_move,
        .fixed_rotate = l->fixed_rotate,
        .motor_force = { l->motor_force[0], l->motor_force[1] },
        .motor_torque = l->motor_torque,
    };
    return phys_body_add(wx + l->rest_pos[0] * scale, wy + l->rest_pos[1] * scale,
                         l->rest_orient, &prm, pts, npts, 0.25f);
}

// Assemble one toy instance at (x_px, y_world): bodies for every limb,
// spring joints between them, sprites added back-to-front by zOrder.
static bool spawn_toy(const char* assets_root, const char* class_name,
                      float x_px, float y_world, int group) {
    const toydef_t* d = toydefs_find(class_name);
    if (!d || d->nlimbs > MAX_LIMBS) {
        fprintf(stderr, "demo: no def for %s\n", class_name);
        return false;
    }
    const float S = d->base_scale;
    const float wx = x_px / PHYS_PX_PER_UNIT;

    int bodies[MAX_LIMBS];
    for (int i = 0; i < d->nlimbs; i++) {
        bodies[i] = make_limb_body(&d->limbs[i], wx, y_world, S);
    }

    for (int i = 0; i < d->njoints; i++) {
        const td_joint* j = &d->joints[i];
        if (j->limb1 < 0 || j->limb2 < 0) {
            continue;
        }
        phys_joint_add(bodies[j->limb1], j->anchor1[0] * S, j->anchor1[1] * S,
                       bodies[j->limb2], j->anchor2[0] * S, j->anchor2[1] * S,
                       j->rest_length * S, j->stiffness, j->dampener);
    }

    // draw order: limbs sorted by their sprite's zOrder, DESCENDING =
    // back-to-front - smaller zOrder is nearer the viewer (bear: legs -70
    // deepest, then arms -75, body -100, head -110 in front)
    int order[MAX_LIMBS];
    int nvis = 0;
    for (int i = 0; i < d->nlimbs; i++) {
        if (d->limbs[i].nsprites > 0 && d->limbs[i].sprites[0].image) {
            order[nvis++] = i;
        }
    }
    for (int a = 1; a < nvis; a++) {
        const int v = order[a];
        int b = a;
        while (b > 0 && d->limbs[order[b - 1]].sprites[0].z_order
                            < d->limbs[v].sprites[0].z_order) {
            order[b] = order[b - 1];
            b--;
        }
        order[b] = v;
    }

    int shown = 0;
    for (int k = 0; k < nvis; k++) {
        const td_limb* l = &d->limbs[order[k]];
        const td_sprite* sp = &l->sprites[0]; // TODO: multi-sprite limbs
        char* cpath = join(assets_root, sp->image);
        char* apath = alpha_variant(cpath);
        // leaked on purpose: scene borrows the pixel buffers for hit-testing
        as_anim* anim = calloc(1, sizeof(as_anim));
        if (as_load_flc(cpath, apath, anim)) {
            const int sprite = scene_sprite_add(anim->w, anim->h, anim->frame_count,
                                                anim->frames, anim->speed_ms,
                                                0, 0, group);
            scene_sprite_bind_body(sprite, bodies[order[k]], sp->com[0], sp->com[1]);
            shown++;
        } else {
            fprintf(stderr, "demo: FAILED %s\n", cpath);
        }
        free(cpath);
        free(apath);
    }
    printf("demo: %s: %d limbs (%d sprites), %d joints, scale %.3g\n",
           class_name, d->nlimbs, shown, d->njoints, S);
    return shown > 0;
}

int demo_load(const char* assets_root) {
    char* defs = join(assets_root, "toydefs.json");
    if (toydefs_load(defs)) {
        printf("demo: %d toy defs loaded\n", toydefs_count());
    } else {
        fprintf(stderr, "demo: toydefs.json missing at %s\n", defs);
        free(defs);
        return 0;
    }
    free(defs);

    int n = 0;
    // fixed scenery: anchored, gravity 0
    n += spawn_toy(assets_root, "PirateWind", 300, 4.0f, n);
    // bouncy ball: mass 1.5, default mouse spring
    n += spawn_toy(assets_root, "U1Bouncy", 600, 6.0f, n);
    // balloon: 7 limbs (balloon + string chain), POSITIVE gravity override
    n += spawn_toy(assets_root, "BalloonBlue", 900, 5.0f, n);
    // the bear: 6 limbs, 11 spring joints, the full-assembly milestone
    n += spawn_toy(assets_root, "U6Bluebear", 1200, 5.0f, n);
    return n;
}
