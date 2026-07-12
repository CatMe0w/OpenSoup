#include "toyphys.h"
#include "physics.h"
#include <stdint.h>
#include <string.h>

#define MAX_SHAPE_PTS 160

static uint32_t group_bit(const char* name) {
    static const struct { const char* name; uint32_t bit; } groups[] = {
        { "bouncers", PHYS_GROUP_BOUNCERS },
        { "spinners", PHYS_GROUP_SPINNERS },
        { "bouncer_repellers", PHYS_GROUP_BOUNCER_REPELLERS },
        { "spinner_rotators", PHYS_GROUP_SPINNER_ROTATORS },
        { "exclusion", PHYS_GROUP_EXCLUSION },
        { "exclusion_repellers", PHYS_GROUP_EXCLUSION_REPELLERS },
        { "left_wall", PHYS_GROUP_LEFT_WALL },
        { "right_wall", PHYS_GROUP_RIGHT_WALL },
        { "floor", PHYS_GROUP_FLOOR },
        { "ceiling", PHYS_GROUP_CEILING },
        { "left_wall_repel", PHYS_GROUP_LEFT_WALL_REPEL },
        { "left_wall_rotate", PHYS_GROUP_LEFT_WALL_ROTATE },
        { "right_wall_repel", PHYS_GROUP_RIGHT_WALL_REPEL },
        { "right_wall_rotate", PHYS_GROUP_RIGHT_WALL_ROTATE },
        { "floor_repel", PHYS_GROUP_FLOOR_REPEL },
        { "floor_rotate", PHYS_GROUP_FLOOR_ROTATE },
        { "ceiling_repel", PHYS_GROUP_CEILING_REPEL },
        { "ceiling_rotate", PHYS_GROUP_CEILING_ROTATE },
        { "snowballs", PHYS_GROUP_SNOWBALLS },
    };
    for (size_t i = 0; i < sizeof groups / sizeof groups[0]; i++) {
        if (strcmp(name, groups[i].name) == 0) {
            return groups[i].bit;
        }
    }
    return 0;
}

static uint32_t shape_groups(const td_shape* sh) {
    uint32_t bits = 0;
    for (int i = 0; i < sh->nmembers; i++) {
        bits |= group_bit(sh->members[i]);
    }
    return bits;
}

static uint32_t hash_group(const char* s) {
    uint32_t h = UINT32_C(2166136261);
    for (; s && *s; s++) {
        h = (h ^ (unsigned char)*s) * UINT32_C(16777619);
    }
    return h;
}

int toyphys_body_for_limb(const td_limb* l, float x, float y, float theta,
                          float scale, int toy_id) {
    phys_point pts[MAX_SHAPE_PTS];
    phys_shape shapes[MAX_SHAPE_PTS];
    int npts = 0;
    int nshapes = 0;
    for (int s = 0; s < l->nshapes; s++) {
        const td_shape* sh = &l->shapes[s];
        phys_shape* out = &shapes[nshapes++];
        out->first_point = npts;
        out->npoints = 0;
        out->groups = shape_groups(sh);
        for (int p = 0; p < sh->npoints && npts < MAX_SHAPE_PTS; p++) {
            pts[npts++] = (phys_point){ sh->points[p].x * scale,
                                        sh->points[p].y * scale,
                                        sh->points[p].r * scale };
            out->npoints++;
        }
    }
    phys_params prm = {
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
        .toy_id = toy_id,
        .local_group = hash_group(l->local_collision_group),
    };
    for (int m = 0; m < 5; m++) {
        prm.material[m] = l->material[m];
    }
    return phys_body_add(x, y, theta, &prm, pts, npts, shapes, nshapes, 0.25f);
}
