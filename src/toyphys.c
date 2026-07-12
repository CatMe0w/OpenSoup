#include "toyphys.h"
#include "physics.h"

#define MAX_SHAPE_PTS 160

int toyphys_body_for_limb(const td_limb* l, float x, float y, float theta,
                          float scale) {
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
                                        sh->points[p].r * scale,
                                        sh->wall_repel, sh->wall_rotate };
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
    };
    for (int m = 0; m < 5; m++) {
        prm.material[m] = l->material[m];
    }
    return phys_body_add(x, y, theta, &prm, pts, npts, 0.25f);
}
