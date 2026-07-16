#include "physics.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_BODY_CAP 64
#define INITIAL_JOINT_CAP 128
#define FRICTION_ITERATIONS 4 // original: 4x Contact_solveIter per pair

// world walls are the hidden "World" toy's 4 anchored limbs
// (souptoys_core.toy def): mass=1.0, so the contact mass ratio
// massWall/massBody is simply 1/massBody.
#define WALL_MASS 1.0f

typedef struct {
    float px, py;   // position, meters
    float mx, my;   // momentum
    float theta;    // orientation, radians CCW, accumulated
    float L;        // angular momentum
    phys_point* pts; // collision points, body-local
    int npts;
    phys_shape* shapes;
    int nshapes;
    phys_params prm;
    bool grabbed;
    bool dead;      // freed slot, reusable; anchored+0 pts = fully inert
    float tx, ty;   // mouse spring target while grabbed
    float gax, gay; // grab anchor, body-local (the point that was clicked)
    bool gmove;     // grab flags (sub_4237B0): move gates the linear part
    bool grot;      // of the anchor spring, rotate gates the torque part
} body_t;

// rotational joint: orientation spring, torque on the relative angle
typedef struct {
    int b1, b2;
    float o1, o2;  // reference orientations from the def
    float rest, k, c;
    bool active;
} rotjoint_t;

typedef struct {
    int b1, b2;
    float a1x, a1y, a2x, a2y; // body-local anchors
    float rest, k, c;
    bool active;
} joint_t;

// RK4 stage state of one body (see the integrator section below).
typedef struct {
    float px, py, theta;
    float mx, my, L;
} bstate_t;

static struct {
    float ww, wh; // wall extents
    body_t* bodies;
    int nbodies, body_cap;
    joint_t* joints;
    int njoints, joint_cap;
    rotjoint_t* rotjoints;
    int nrotjoints, rotjoint_cap;
    bstate_t* rk_y;
    bstate_t* rk_k[4];
} P;

static int grown_capacity(int current, int wanted, int initial) {
    int cap = current ? current : initial;
    while (cap < wanted) {
        if (cap > INT_MAX / 2) return 0;
        cap *= 2;
    }
    return cap;
}

static bool reserve_bodies(int wanted) {
    if (wanted <= P.body_cap) return true;
    const int cap = grown_capacity(P.body_cap, wanted, INITIAL_BODY_CAP);
    if (!cap) return false;

    body_t* bodies = calloc((size_t)cap, sizeof(*bodies));
    bstate_t* y = calloc((size_t)cap, sizeof(*y));
    bstate_t* k[4] = {0};
    for (int stage = 0; stage < 4; stage++) {
        k[stage] = calloc((size_t)cap, sizeof(*k[stage]));
    }
    if (!bodies || !y || !k[0] || !k[1] || !k[2] || !k[3]) {
        free(bodies);
        free(y);
        for (int stage = 0; stage < 4; stage++) free(k[stage]);
        return false;
    }
    if (P.nbodies > 0) {
        memcpy(bodies, P.bodies, (size_t)P.nbodies * sizeof(*bodies));
        memcpy(y, P.rk_y, (size_t)P.nbodies * sizeof(*y));
        for (int stage = 0; stage < 4; stage++) {
            memcpy(k[stage], P.rk_k[stage],
                   (size_t)P.nbodies * sizeof(*k[stage]));
        }
    }
    free(P.bodies);
    free(P.rk_y);
    for (int stage = 0; stage < 4; stage++) free(P.rk_k[stage]);
    P.bodies = bodies;
    P.rk_y = y;
    for (int stage = 0; stage < 4; stage++) P.rk_k[stage] = k[stage];
    P.body_cap = cap;
    return true;
}

static bool reserve_joints(int wanted) {
    if (wanted <= P.joint_cap) return true;
    const int cap = grown_capacity(P.joint_cap, wanted, INITIAL_JOINT_CAP);
    if (!cap) return false;
    joint_t* joints = realloc(P.joints, (size_t)cap * sizeof(*joints));
    if (!joints) return false;
    P.joints = joints;
    P.joint_cap = cap;
    return true;
}

static bool reserve_rotjoints(int wanted) {
    if (wanted <= P.rotjoint_cap) return true;
    const int cap = grown_capacity(P.rotjoint_cap, wanted, INITIAL_JOINT_CAP);
    if (!cap) return false;
    rotjoint_t* joints = realloc(P.rotjoints, (size_t)cap * sizeof(*joints));
    if (!joints) return false;
    P.rotjoints = joints;
    P.rotjoint_cap = cap;
    return true;
}

// classic RK4 stage time offsets, rk4_stage_coeffs @0x6D6BA0
static const float rk4_coeffs[4] = { 0.0f, 0.5f, 0.5f, 1.0f };

void phys_set_world(float width, float height) {
    P.ww = width;
    P.wh = height;
}

int phys_body_add(float x, float y, float theta, const phys_params* p,
                  const phys_point* pts, int npts,
                  const phys_shape* shapes, int nshapes,
                  float fallback_radius) {
    phys_point* owned_pts = NULL;
    phys_shape* owned_shapes = NULL;
    int owned_npts = npts;
    int owned_nshapes = nshapes;
    if (npts > 0 || nshapes > 0) {
        if (npts > 0) {
            owned_pts = malloc((size_t)npts * sizeof(*owned_pts));
        }
        if (nshapes > 0) {
            owned_shapes = malloc((size_t)nshapes * sizeof(*owned_shapes));
        }
        if ((npts > 0 && !owned_pts) || (nshapes > 0 && !owned_shapes)) {
            free(owned_pts);
            free(owned_shapes);
            return -1;
        }
        if (npts > 0) memcpy(owned_pts, pts, (size_t)npts * sizeof(*owned_pts));
        if (nshapes > 0) {
            memcpy(owned_shapes, shapes, (size_t)nshapes * sizeof(*owned_shapes));
        }
    } else {
        owned_npts = owned_nshapes = 1;
        owned_pts = malloc(sizeof(*owned_pts));
        owned_shapes = malloc(sizeof(*owned_shapes));
        if (!owned_pts || !owned_shapes) {
            free(owned_pts);
            free(owned_shapes);
            return -1;
        }
        owned_pts[0] = (phys_point){ 0, 0, fallback_radius };
        owned_shapes[0] = (phys_shape){
            0, 1,
            PHYS_GROUP_LEFT_WALL_REPEL | PHYS_GROUP_LEFT_WALL_ROTATE
              | PHYS_GROUP_RIGHT_WALL_REPEL | PHYS_GROUP_RIGHT_WALL_ROTATE
              | PHYS_GROUP_FLOOR_REPEL | PHYS_GROUP_FLOOR_ROTATE
              | PHYS_GROUP_CEILING_REPEL | PHYS_GROUP_CEILING_ROTATE
        };
    }
    int slot = -1;
    for (int i = 0; i < P.nbodies; i++) {
        if (P.bodies[i].dead) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (!reserve_bodies(P.nbodies + 1)) {
            free(owned_pts);
            free(owned_shapes);
            return -1;
        }
        slot = P.nbodies++;
    }
    body_t* b = &P.bodies[slot];
    b->dead = false;
    b->px = x;
    b->py = y;
    b->mx = 0;
    b->my = 0;
    b->theta = theta;
    b->L = 0;
    b->npts = owned_npts;
    b->pts = owned_pts;
    b->nshapes = owned_nshapes;
    b->shapes = owned_shapes;
    b->prm = *p;
    b->grabbed = false;
    return slot;
}

int phys_joint_add(int body1, float a1x, float a1y,
                   int body2, float a2x, float a2y,
                   float rest_length, float stiffness, float dampener) {
    if (body1 < 0 || body2 < 0) return -1;
    int slot = -1;
    for (int i = 0; i < P.njoints; i++) {
        if (!P.joints[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (!reserve_joints(P.njoints + 1)) return -1;
        slot = P.njoints++;
    }
    P.joints[slot] = (joint_t){ body1, body2, a1x, a1y, a2x, a2y,
                                rest_length, stiffness, dampener, true };
    return slot;
}

int phys_rotjoint_add(int body1, float o1, int body2, float o2,
                      float rest, float stiffness, float dampener) {
    if (body1 < 0 || body2 < 0) return -1;
    int slot = -1;
    for (int i = 0; i < P.nrotjoints; i++) {
        if (!P.rotjoints[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        if (!reserve_rotjoints(P.nrotjoints + 1)) return -1;
        slot = P.nrotjoints++;
    }
    P.rotjoints[slot] = (rotjoint_t){ body1, body2, o1, o2,
                                      rest, stiffness, dampener, true };
    return slot;
}

// One single-body contact constraint. n points away from the other shape;
// (vx,vy) is this contact point's velocity relative to the other body.
typedef struct {
    float nx, ny;
    float rx, ry;   // contact point - body origin
    float vx, vy;
    float pen;
    bool linear, angular;
} contact_t;

static const float wall_nx[4] = { 1, -1, 0, 0 };
static const float wall_ny[4] = { 0, 0, 1, -1 };

static void point_world(const bstate_t* s, const phys_point* p,
                        float* x, float* y) {
    const float c = cosf(s->theta), sn = sinf(s->theta);
    *x = s->px + c * p->x - sn * p->y;
    *y = s->py + sn * p->x + c * p->y;
}

static void point_velocity(const body_t* b, const bstate_t* s,
                           float rx, float ry, float* vx, float* vy) {
    const float omega = (!b->prm.fixed_rotate && b->prm.inertia > 0)
                            ? s->L / b->prm.inertia : 0.0f;
    *vx = s->mx / b->prm.mass - omega * ry;
    *vy = s->my / b->prm.mass + omega * rx;
}

static bool wall_contact_eval(const body_t* b, const phys_point* p, int wall,
                              const bstate_t* s, contact_t* out) {
    float wx, wy;
    point_world(s, p, &wx, &wy);
    float pen;
    switch (wall) {
        case PHYS_WALL_LEFT: pen = p->r - wx; break;
        case PHYS_WALL_RIGHT: pen = wx + p->r - P.ww; break;
        case PHYS_WALL_FLOOR: pen = p->r - wy; break;
        default: pen = wy + p->r - P.wh; break;
    }
    if (pen <= 0) {
        return false;
    }
    out->nx = wall_nx[wall];
    out->ny = wall_ny[wall];
    out->ry = wy - out->ny * p->r - s->py;
    out->rx = wx - out->nx * p->r - s->px;
    point_velocity(b, s, out->rx, out->ry, &out->vx, &out->vy);
    out->pen = pen;
    out->linear = true;
    out->angular = true;
    return true;
}

static uint32_t wall_repel_bit(int wall) {
    static const uint32_t bits[4] = {
        PHYS_GROUP_LEFT_WALL_REPEL, PHYS_GROUP_RIGHT_WALL_REPEL,
        PHYS_GROUP_FLOOR_REPEL, PHYS_GROUP_CEILING_REPEL
    };
    return bits[wall];
}

static uint32_t wall_rotate_bit(int wall) {
    static const uint32_t bits[4] = {
        PHYS_GROUP_LEFT_WALL_ROTATE, PHYS_GROUP_RIGHT_WALL_ROTATE,
        PHYS_GROUP_FLOOR_ROTATE, PHYS_GROUP_CEILING_ROTATE
    };
    return bits[wall];
}

static const phys_point* shape_point(const body_t* b, const phys_shape* sh,
                                     int i) {
    return &b->pts[sh->first_point + i];
}

// Closest contact of one source vertex against one target swept-circle shape.
// Polygons are convex in the shipped definitions. For an interior vertex the
// nearest outward edge normal is used; exterior vertices use exact segment
// distance so rounded corners do not create false contacts.
static bool vertex_shape_contact(const body_t* a, const bstate_t* sa,
                                 const phys_point* vp,
                                 const body_t* b, const bstate_t* sb,
                                 const phys_shape* target, contact_t* out) {
    if (target->npoints <= 0) {
        return false;
    }
    float sx, sy;
    point_world(sa, vp, &sx, &sy);
    float qx = 0, qy = 0, qr = 0, nx = 0, ny = 0;
    bool inside = false;

    if (target->npoints == 1) {
        const phys_point* tp = shape_point(b, target, 0);
        point_world(sb, tp, &qx, &qy);
        qr = tp->r;
        float dx = sx - qx, dy = sy - qy;
        const float dist = sqrtf(dx * dx + dy * dy);
        const float pen = vp->r + qr - dist;
        if (pen <= 0) {
            return false;
        }
        if (dist > 1e-7f) {
            nx = dx / dist;
            ny = dy / dist;
        } else {
            dx = sa->px - sb->px;
            dy = sa->py - sb->py;
            const float bd = sqrtf(dx * dx + dy * dy);
            nx = bd > 1e-7f ? dx / bd : 1.0f;
            ny = bd > 1e-7f ? dy / bd : 0.0f;
        }
        out->pen = pen;
    } else {
        const int edges = target->npoints == 2 ? 1 : target->npoints;
        float area2 = 0;
        if (target->npoints > 2) {
            for (int i = 0; i < target->npoints; i++) {
                float ax, ay, bx, by;
                point_world(sb, shape_point(b, target, i), &ax, &ay);
                point_world(sb, shape_point(b, target, (i + 1) % target->npoints),
                            &bx, &by);
                area2 += ax * by - ay * bx;
            }
        }

        float best_dist2 = INFINITY;
        float best_clear = -INFINITY;
        float in_qx = 0, in_qy = 0, in_qr = 0, in_nx = 0, in_ny = 0;
        inside = target->npoints > 2;
        for (int i = 0; i < edges; i++) {
            const phys_point* p0 = shape_point(b, target, i);
            const phys_point* p1 = shape_point(b, target, (i + 1) % target->npoints);
            float ax, ay, bx, by;
            point_world(sb, p0, &ax, &ay);
            point_world(sb, p1, &bx, &by);
            const float ex = bx - ax, ey = by - ay;
            const float len2 = ex * ex + ey * ey;
            if (len2 <= 1e-12f) {
                continue;
            }
            float t = ((sx - ax) * ex + (sy - ay) * ey) / len2;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            const float cx = ax + t * ex, cy = ay + t * ey;
            const float cr = p0->r + t * (p1->r - p0->r);
            const float dx = sx - cx, dy = sy - cy;
            const float dist2 = dx * dx + dy * dy;
            if (dist2 < best_dist2) {
                best_dist2 = dist2;
                qx = cx; qy = cy; qr = cr;
            }

            if (target->npoints > 2) {
                const float inv_len = 1.0f / sqrtf(len2);
                const float onx = area2 < 0 ? -ey * inv_len : ey * inv_len;
                const float ony = area2 < 0 ? ex * inv_len : -ex * inv_len;
                const float clear = (sx - ax) * onx + (sy - ay) * ony - cr;
                if (clear > 0) {
                    inside = false;
                }
                if (clear > best_clear) {
                    best_clear = clear;
                    in_qx = cx; in_qy = cy; in_qr = cr;
                    in_nx = onx; in_ny = ony;
                }
            }
        }
        if (!isfinite(best_dist2)) {
            return false;
        }
        if (inside) {
            nx = in_nx; ny = in_ny;
            qx = in_qx; qy = in_qy; qr = in_qr;
            out->pen = vp->r - best_clear;
        } else {
            const float dist = sqrtf(best_dist2);
            out->pen = vp->r + qr - dist;
            if (out->pen <= 0) {
                return false;
            }
            if (dist > 1e-7f) {
                nx = (sx - qx) / dist;
                ny = (sy - qy) / dist;
            } else {
                const float ex = sx - sb->px, ey = sy - sb->py;
                const float d = sqrtf(ex * ex + ey * ey);
                nx = d > 1e-7f ? ex / d : 1.0f;
                ny = d > 1e-7f ? ey / d : 0.0f;
            }
        }
    }

    out->nx = nx;
    out->ny = ny;
    const float acx = sx - nx * vp->r;
    const float acy = sy - ny * vp->r;
    const float bcx = qx + nx * qr;
    const float bcy = qy + ny * qr;
    out->rx = acx - sa->px;
    out->ry = acy - sa->py;
    float avx, avy, bvx, bvy;
    point_velocity(a, sa, out->rx, out->ry, &avx, &avy);
    point_velocity(b, sb, bcx - sb->px, bcy - sb->py, &bvx, &bvy);
    out->vx = avx - bvx;
    out->vy = avy - bvy;
    return out->pen > 0;
}

// normal contact response magnitude, Contact_prepare's exact form:
//   closing*C0 + pen*C1 + closing*pen*C2, C = 2*ownMaterial
// (velocityResponse damps the approach, stiffness is the penetration
// penalty spring, dampener the penetration-scaled damper). The closing-
// velocity term only acts while approaching (prepare zeroes it otherwise).
// Interpreted as FORCE (the original folds dt into precomputed body fields);
// scaled by massWall/massBody and split among the body's contacts on that
// wall. NO positional correction exists in the original - resting bodies sit
// on the penalty spring, which is what keeps them jitter-free.
static float contact_normal_force(const body_t* b, float other_mass,
                                  const contact_t* ct) {
    const float closing = -(ct->vx * ct->nx + ct->vy * ct->ny);
    const float c0 = 2.0f * b->prm.material[0];
    const float c1 = 2.0f * b->prm.material[1];
    const float c2 = 2.0f * b->prm.material[2];
    const float mag = (closing > 0 ? closing * c0 : 0.0f)
                    + ct->pen * c1 + closing * ct->pen * c2;
    return mag * (other_mass / b->prm.mass);
}

// wall contact forces for one body at a stage state, added into the
// derivative. Force split among the body's penetrating points per wall
// (the original splits impulses by the pair's contact count).
static void derive_wall_contacts(const body_t* b, const bstate_t* s, bstate_t* d) {
    for (int w = 0; w < 4; w++) {
        int count = 0;
        contact_t ct;
        for (int si = 0; si < b->nshapes; si++) {
            const phys_shape* sh = &b->shapes[si];
            if (!(sh->groups & wall_repel_bit(w))) continue;
            for (int i = 0; i < sh->npoints; i++) {
                if (wall_contact_eval(b, shape_point(b, sh, i), w, s, &ct)) {
                    count++;
                }
            }
        }
        if (!count) {
            continue;
        }
        for (int si = 0; si < b->nshapes; si++) {
            const phys_shape* sh = &b->shapes[si];
            if (!(sh->groups & wall_repel_bit(w))) continue;
            for (int i = 0; i < sh->npoints; i++) {
                if (!wall_contact_eval(b, shape_point(b, sh, i), w, s, &ct)) {
                    continue;
                }
                const float f = contact_normal_force(b, WALL_MASS, &ct)
                              / (float)count;
                d->mx += f * ct.nx;
                d->my += f * ct.ny;
                if ((sh->groups & wall_rotate_bit(w)) && !b->prm.fixed_rotate) {
                    d->L += (ct.rx * ct.ny - ct.ry * ct.nx) * f;
                }
            }
        }
    }
}

static void pair_response(uint32_t own, uint32_t other,
                          bool* linear, bool* angular) {
    *linear = ((own & PHYS_GROUP_BOUNCERS)
               && (other & PHYS_GROUP_BOUNCER_REPELLERS))
           || ((own & PHYS_GROUP_EXCLUSION)
               && (other & PHYS_GROUP_EXCLUSION_REPELLERS));
    *angular = (own & PHYS_GROUP_SPINNERS)
            && (other & PHYS_GROUP_SPINNER_ROTATORS);
}

#define MAX_PAIR_CONTACTS 256

static int pair_contacts(const body_t* a, const bstate_t* sa,
                         const body_t* b, const bstate_t* sb,
                         contact_t* contacts, int cap) {
    int n = 0;
    for (int ai = 0; ai < a->nshapes; ai++) {
        const phys_shape* ash = &a->shapes[ai];
        for (int bi = 0; bi < b->nshapes; bi++) {
            const phys_shape* bsh = &b->shapes[bi];
            bool linear, angular;
            pair_response(ash->groups, bsh->groups, &linear, &angular);
            if (!linear && !angular) continue;
            for (int pi = 0; pi < ash->npoints && n < cap; pi++) {
                contact_t ct;
                if (vertex_shape_contact(a, sa, shape_point(a, ash, pi),
                                         b, sb, bsh, &ct)) {
                    ct.linear = linear;
                    ct.angular = angular;
                    contacts[n++] = ct;
                }
            }
        }
    }
    return n;
}

static bool bodies_can_collide(const body_t* a, const body_t* b) {
    return !a->dead && !b->dead && a != b
        && (a->prm.toy_id != b->prm.toy_id
            || a->prm.local_group == b->prm.local_group);
}

static void derive_toy_contacts(int ai, const bstate_t* sa, bstate_t* d) {
    const body_t* a = &P.bodies[ai];
    contact_t contacts[MAX_PAIR_CONTACTS];
    for (int bi = 0; bi < P.nbodies; bi++) {
        const body_t* b = &P.bodies[bi];
        if (!bodies_can_collide(a, b)) continue;
        const int n = pair_contacts(a, sa, b, &P.rk_y[bi], contacts,
                                    MAX_PAIR_CONTACTS);
        for (int i = 0; i < n; i++) {
            const contact_t* ct = &contacts[i];
            const float f = contact_normal_force(a, b->prm.mass, ct) / (float)n;
            if (ct->linear) {
                d->mx += f * ct->nx;
                d->my += f * ct->ny;
            }
            if (ct->angular && !a->prm.fixed_rotate) {
                d->L += (ct->rx * ct->ny - ct->ry * ct->nx) * f;
            }
        }
    }
}

// post-integration friction pass, Contact_solveIter's exact form: iterate
// 4x per contact, each iteration applies the impulse that cancels the
// contact point's TANGENTIAL velocity (effective-mass weighted), capped by
// the static-friction cone against this step's normal impulse.
static void solve_friction(body_t* b) {
    for (int it = 0; it < FRICTION_ITERATIONS; it++) {
        for (int w = 0; w < 4; w++) {
            for (int si = 0; si < b->nshapes; si++) {
                const phys_shape* sh = &b->shapes[si];
                if (!(sh->groups & wall_repel_bit(w))) continue;
                for (int i = 0; i < sh->npoints; i++) {
                    const phys_point* p = shape_point(b, sh, i);
                    contact_t ct;
                    const bstate_t state = { b->px, b->py, b->theta,
                                             b->mx, b->my, b->L };
                    if (!wall_contact_eval(b, p, w, &state, &ct)) continue;
                const float tx = -ct.ny, ty = ct.nx;
                const float vt = ct.vx * tx + ct.vy * ty;
                const bool spin = (sh->groups & wall_rotate_bit(w)) && !b->prm.fixed_rotate
                                  && b->prm.inertia > 0;
                const float crt = ct.rx * ty - ct.ry * tx; // r x t
                const float denom = 1.0f / b->prm.mass
                                  + (spin ? crt * crt / b->prm.inertia : 0.0f);
                float jt = -vt / denom;
                // static-friction cone vs the step's normal impulse
                const float ncap = b->prm.material[4]
                                 * contact_normal_force(b, WALL_MASS, &ct) * PHYS_DT;
                if (jt > ncap) {
                    jt = ncap;
                } else if (jt < -ncap) {
                    jt = -ncap;
                }
                b->mx += jt * tx;
                b->my += jt * ty;
                if (spin) {
                    b->L += crt * jt;
                }
                }
            }
        }

        const bstate_t sa = { b->px, b->py, b->theta, b->mx, b->my, b->L };
        contact_t contacts[MAX_PAIR_CONTACTS];
        for (int bi = 0; bi < P.nbodies; bi++) {
            body_t* other = &P.bodies[bi];
            if (!bodies_can_collide(b, other)) continue;
            const bstate_t sb = { other->px, other->py, other->theta,
                                  other->mx, other->my, other->L };
            const int n = pair_contacts(b, &sa, other, &sb, contacts,
                                        MAX_PAIR_CONTACTS);
            for (int i = 0; i < n; i++) {
                const contact_t* ct = &contacts[i];
                const float tx = -ct->ny, ty = ct->nx;
                const float vt = ct->vx * tx + ct->vy * ty;
                const bool spin = ct->angular && !b->prm.fixed_rotate
                                  && b->prm.inertia > 0;
                const float crt = ct->rx * ty - ct->ry * tx;
                const float denom = (ct->linear ? 1.0f / b->prm.mass : 0.0f)
                                  + (spin ? crt * crt / b->prm.inertia : 0.0f);
                if (denom <= 0) continue;
                float jt = -vt / denom;
                const float ncap = b->prm.material[4]
                    * contact_normal_force(b, other->prm.mass, ct) * PHYS_DT
                    / (float)n;
                if (jt > ncap) jt = ncap;
                if (jt < -ncap) jt = -ncap;
                if (ct->linear) {
                    b->mx += jt * tx;
                    b->my += jt * ty;
                }
                if (spin) b->L += crt * jt;
            }
        }
    }
}

// RK4 over the full coupled system (positions AND momenta, spring AND
// contact forces re-evaluated at every stage). This is what the original's
// stepOnce does - with joint stiffness up to 6000 at dt=0.01 the rotational
// spring modes sit beyond explicit Euler's stability limit but inside RK4's
// (omega*dt < 2.83).
// world position and velocity of a body-local anchor at a stage state
static void anchor_state(const bstate_t* s, const body_t* b, float ax, float ay,
                         float* px, float* py, float* vx, float* vy) {
    const float c = cosf(s->theta), sn = sinf(s->theta);
    const float rx = c * ax - sn * ay;
    const float ry = sn * ax + c * ay;
    *px = s->px + rx;
    *py = s->py + ry;
    const float omega = (!b->prm.fixed_rotate && b->prm.inertia > 0)
                            ? s->L / b->prm.inertia : 0.0f;
    *vx = s->mx / b->prm.mass - omega * ry;
    *vy = s->my / b->prm.mass + omega * rx;
}

// accumulate force at a body-local anchor into a derivative
static void force_at(bstate_t* d, const bstate_t* s, const body_t* b,
                     float ax, float ay, float fx, float fy) {
    if (b->prm.anchored) {
        return;
    }
    d->mx += fx;
    d->my += fy;
    if (!b->prm.fixed_rotate) {
        const float c = cosf(s->theta), sn = sinf(s->theta);
        const float rx = c * ax - sn * ay;
        const float ry = sn * ax + c * ay;
        d->L += rx * fy - ry * fx;
    }
}

// derivative of the whole system at stage state P.rk_y -> out
static void derive(bstate_t* out) {
    for (int i = 0; i < P.nbodies; i++) {
        const body_t* b = &P.bodies[i];
        const bstate_t* s = &P.rk_y[i];
        bstate_t* d = &out[i];
        *d = (bstate_t){ 0 };
        if (b->prm.anchored) { // fixedMove: no motion at all (original +201 flag)
            continue;
        }
        d->px = s->mx / b->prm.mass;
        d->py = s->my / b->prm.mass;
        if (!b->prm.fixed_rotate && b->prm.inertia > 0) {
            d->theta = s->L / b->prm.inertia;
        }

        // gravity (Body_applyGravity); per-body g honours gravityOverride
        // (balloons have POSITIVE g)
        d->my += b->prm.mass * b->prm.gravity;

        // air resistance: F = -c_lin * v, tau = -c_ang * omega
        d->mx -= b->prm.air_linear * (s->mx / b->prm.mass);
        d->my -= b->prm.air_linear * (s->my / b->prm.mass);
        if (b->prm.inertia > 0) {
            d->L -= b->prm.air_angular * (s->L / b->prm.inertia);
        }

        // motors: constant body-local force + torque
        // TODO(verify): whether linearMotor force rotates with the limb
        if (b->prm.motor_force[0] != 0 || b->prm.motor_force[1] != 0) {
            const float c = cosf(s->theta), sn = sinf(s->theta);
            d->mx += c * b->prm.motor_force[0] - sn * b->prm.motor_force[1];
            d->my += sn * b->prm.motor_force[0] + c * b->prm.motor_force[1];
        }
        if (!b->prm.fixed_rotate) {
            d->L += b->prm.motor_torque;
        }

        if (b->grabbed) { // mouse spring at the grab anchor (sub_532800 form;
                          // move/rotate flags gate the two components, so a
                          // rotate-only limb like the goose body only twists)
            float px, py, vx, vy;
            anchor_state(s, b, b->gax, b->gay, &px, &py, &vx, &vy);
            const float fx = b->prm.mouse_stiffness * (b->tx - px)
                           - b->prm.mouse_dampener * vx;
            const float fy = b->prm.mouse_stiffness * (b->ty - py)
                           - b->prm.mouse_dampener * vy;
            if (b->gmove) {
                d->mx += fx;
                d->my += fy;
            }
            if (b->grot && !b->prm.fixed_rotate) {
                const float c = cosf(s->theta), sn = sinf(s->theta);
                const float rx = c * b->gax - sn * b->gay;
                const float ry = sn * b->gax + c * b->gay;
                d->L += rx * fy - ry * fx;
            }
        }

        derive_wall_contacts(b, s, d);
        derive_toy_contacts(i, s, d);
    }

    // spring joints, same constraint form as the mouse spring (sub_532800):
    // F = k * stretch * dir - c * relative anchor velocity, applied at the
    // anchors (torque from the offset)
    for (int i = 0; i < P.njoints; i++) {
        const joint_t* j = &P.joints[i];
        if (!j->active) continue;
        const body_t* b1 = &P.bodies[j->b1];
        const body_t* b2 = &P.bodies[j->b2];
        float p1x, p1y, v1x, v1y, p2x, p2y, v2x, v2y;
        anchor_state(&P.rk_y[j->b1], b1, j->a1x, j->a1y,
                     &p1x, &p1y, &v1x, &v1y);
        anchor_state(&P.rk_y[j->b2], b2, j->a2x, j->a2y,
                     &p2x, &p2y, &v2x, &v2y);
        float dx = p2x - p1x, dy = p2y - p1y;
        if (j->rest != 0.0f) {
            const float dist = sqrtf(dx * dx + dy * dy);
            if (dist > 1e-6f) {
                const float scale = (dist - j->rest) / dist;
                dx *= scale;
                dy *= scale;
            }
        }
        const float fx = j->k * dx - j->c * (v1x - v2x);
        const float fy = j->k * dy - j->c * (v1y - v2y);
        force_at(&out[j->b1], &P.rk_y[j->b1], b1,
                 j->a1x, j->a1y, fx, fy);
        force_at(&out[j->b2], &P.rk_y[j->b2], b2,
                 j->a2x, j->a2y, -fx, -fy);
    }

    // rotational joints: torque spring on the relative orientation
    // (the goose's "Upright" is k=1 c=0.5 against its anchored legs).
    // Exact original field mapping still unverified: fields read as
    // [rest, stiffness, dampener] - see schema TODO.
    for (int i = 0; i < P.nrotjoints; i++) {
        const rotjoint_t* rj = &P.rotjoints[i];
        if (!rj->active) continue;
        const body_t* b1 = &P.bodies[rj->b1];
        const body_t* b2 = &P.bodies[rj->b2];
        const float w1 = (!b1->prm.fixed_rotate && b1->prm.inertia > 0)
                             ? P.rk_y[rj->b1].L / b1->prm.inertia : 0.0f;
        const float w2 = (!b2->prm.fixed_rotate && b2->prm.inertia > 0)
                             ? P.rk_y[rj->b2].L / b2->prm.inertia : 0.0f;
        const float rel = (P.rk_y[rj->b1].theta - rj->o1)
                        - (P.rk_y[rj->b2].theta - rj->o2);
        const float tau = -rj->k * (rel - rj->rest) - rj->c * (w1 - w2);
        if (!b1->prm.anchored && !b1->prm.fixed_rotate) {
            out[rj->b1].L += tau;
        }
        if (!b2->prm.anchored && !b2->prm.fixed_rotate) {
            out[rj->b2].L -= tau;
        }
    }
}

static void step_once(void) {
    // classic RK4: k1=f(y), k2=f(y+dt/2*k1), k3=f(y+dt/2*k2), k4=f(y+dt*k3)
    for (int stage = 0; stage < 4; stage++) {
        for (int i = 0; i < P.nbodies; i++) {
            const body_t* b = &P.bodies[i];
            bstate_t y = { b->px, b->py, b->theta, b->mx, b->my, b->L };
            if (stage > 0) {
                const float h = rk4_coeffs[stage] * PHYS_DT;
                const bstate_t* k = &P.rk_k[stage - 1][i];
                y.px += h * k->px;
                y.py += h * k->py;
                y.theta += h * k->theta;
                y.mx += h * k->mx;
                y.my += h * k->my;
                y.L += h * k->L;
            }
            P.rk_y[i] = y;
        }
        derive(P.rk_k[stage]);
    }
    for (int i = 0; i < P.nbodies; i++) {
        body_t* b = &P.bodies[i];
        if (b->prm.anchored) {
            continue;
        }
        const bstate_t* k1 = &P.rk_k[0][i];
        const bstate_t* k2 = &P.rk_k[1][i];
        const bstate_t* k3 = &P.rk_k[2][i];
        const bstate_t* k4 = &P.rk_k[3][i];
        const float h = PHYS_DT / 6.0f;
        b->px += h * (k1->px + 2 * k2->px + 2 * k3->px + k4->px);
        b->py += h * (k1->py + 2 * k2->py + 2 * k3->py + k4->py);
        b->theta += h * (k1->theta + 2 * k2->theta + 2 * k3->theta + k4->theta);
        b->mx += h * (k1->mx + 2 * k2->mx + 2 * k3->mx + k4->mx);
        b->my += h * (k1->my + 2 * k2->my + 2 * k3->my + k4->my);
        b->L += h * (k1->L + 2 * k2->L + 2 * k3->L + k4->L);

        // friction/stick impulses on the integrated state (solveIter's role)
        solve_friction(b);
    }
}

void phys_steps(int n) {
    for (int i = 0; i < n; i++) {
        step_once();
    }
}

int phys_active_body_count(void) {
    int count = 0;
    for (int i = 0; i < P.nbodies; i++) {
        if (!P.bodies[i].dead) count++;
    }
    return count;
}

static int wall_from_groups(uint32_t groups) {
    if (groups & PHYS_GROUP_LEFT_WALL) return PHYS_WALL_LEFT;
    if (groups & PHYS_GROUP_RIGHT_WALL) return PHYS_WALL_RIGHT;
    if (groups & PHYS_GROUP_FLOOR) return PHYS_WALL_FLOOR;
    if (groups & PHYS_GROUP_CEILING) return PHYS_WALL_CEILING;
    return -1;
}

static bool shape_overlaps_wall(const body_t* b, const phys_shape* sh,
                                int wall) {
    const bstate_t s = { b->px, b->py, b->theta, b->mx, b->my, b->L };
    contact_t ct;
    for (int i = 0; i < sh->npoints; i++) {
        if (wall_contact_eval(b, shape_point(b, sh, i), wall, &s, &ct)) {
            return true;
        }
    }
    return false;
}

static bool shape_penetrates(const body_t* a, const bstate_t* sa,
                             const phys_shape* ash,
                             const body_t* b, const bstate_t* sb,
                             const phys_shape* bsh) {
    contact_t ct;
    for (int i = 0; i < ash->npoints; i++) {
        if (vertex_shape_contact(a, sa, shape_point(a, ash, i),
                                 b, sb, bsh, &ct)) {
            return true;
        }
    }
    return false;
}

bool phys_shapes_overlap(int body1, int shape1, int body2, int shape2) {
    if (body1 < 0 || body1 >= P.nbodies || body2 < 0 || body2 >= P.nbodies) {
        return false;
    }
    const body_t* a = &P.bodies[body1];
    const body_t* b = &P.bodies[body2];
    if (shape1 < 0 || shape1 >= a->nshapes
        || shape2 < 0 || shape2 >= b->nshapes
        || !bodies_can_collide(a, b)) {
        return false;
    }
    const phys_shape* ash = &a->shapes[shape1];
    const phys_shape* bsh = &b->shapes[shape2];
    const int bwall = wall_from_groups(bsh->groups);
    if (bwall >= 0) {
        return shape_overlaps_wall(a, ash, bwall);
    }
    const int awall = wall_from_groups(ash->groups);
    if (awall >= 0) {
        return shape_overlaps_wall(b, bsh, awall);
    }
    const bstate_t sa = { a->px, a->py, a->theta, a->mx, a->my, a->L };
    const bstate_t sb = { b->px, b->py, b->theta, b->mx, b->my, b->L };
    return shape_penetrates(a, &sa, ash, b, &sb, bsh)
        || shape_penetrates(b, &sb, bsh, a, &sa, ash);
}

void phys_body_pos(int body, float* x, float* y) {
    *x = P.bodies[body].px;
    *y = P.bodies[body].py;
}

float phys_body_orientation(int body) {
    return P.bodies[body].theta;
}

void phys_body_set_pose(int body, float x, float y, float theta) {
    body_t* b = &P.bodies[body];
    b->px = x;
    b->py = y;
    b->theta = theta;
}

void phys_body_momentum(int body, float* mx, float* my, float* L) {
    const body_t* b = &P.bodies[body];
    *mx = b->mx;
    *my = b->my;
    *L = b->L;
}

void phys_body_set_momentum(int body, float mx, float my, float L) {
    body_t* b = &P.bodies[body];
    b->mx = mx;
    b->my = my;
    b->L = L;
}

void phys_body_free(int body) {
    body_t* b = &P.bodies[body];
    b->dead = true;
    b->grabbed = false;
    // fully inert: integrator skips anchored bodies, no points = no contacts
    b->prm.anchored = true;
    b->prm.fixed_rotate = true;
    b->mx = b->my = b->L = 0;
    free(b->pts);
    free(b->shapes);
    b->pts = NULL;
    b->shapes = NULL;
    b->npts = 0;
    b->nshapes = 0;
    for (int i = 0; i < P.njoints; i++) {
        joint_t* j = &P.joints[i];
        if (j->active && (j->b1 == body || j->b2 == body)) {
            j->active = false;
        }
    }
    for (int i = 0; i < P.nrotjoints; i++) {
        rotjoint_t* rj = &P.rotjoints[i];
        if (rj->active && (rj->b1 == body || rj->b2 == body)) {
            rj->active = false;
        }
    }
}

void phys_grab(int body, float ax, float ay, bool move, bool rotate) {
    body_t* b = &P.bodies[body];
    b->grabbed = true;
    b->gax = ax;
    b->gay = ay;
    b->gmove = move;
    b->grot = rotate;
    // initial target = the grab point's current world position
    const float c = cosf(b->theta), sn = sinf(b->theta);
    b->tx = b->px + c * ax - sn * ay;
    b->ty = b->py + sn * ax + c * ay;
}

void phys_grab_move(int body, float x, float y) {
    body_t* b = &P.bodies[body];
    b->tx = x;
    b->ty = y;

    // fixedMove/fixedRotate block ordinary integration, not the explicit
    // editor handles selected by grabMove/grabRotate. The original can move
    // an anchored centre handle and turn an anchored end handle; update only
    // those locked components kinematically while the normal spring remains
    // responsible for free bodies.
    if (b->grot && (b->prm.anchored || b->prm.fixed_rotate)) {
        const float r2 = b->gax * b->gax + b->gay * b->gay;
        const float dx = x - b->px, dy = y - b->py;
        if (r2 > 1e-8f && dx * dx + dy * dy > 1e-8f) {
            float desired = atan2f(dy, dx) - atan2f(b->gay, b->gax);
            while (desired - b->theta > (float)M_PI) desired -= 2.0f * (float)M_PI;
            while (desired - b->theta < -(float)M_PI) desired += 2.0f * (float)M_PI;
            b->theta = desired;
            b->L = 0.0f;
        }
    }
    if (b->gmove && b->prm.anchored) {
        const float c = cosf(b->theta), sn = sinf(b->theta);
        b->px = x - (c * b->gax - sn * b->gay);
        b->py = y - (sn * b->gax + c * b->gay);
        b->mx = b->my = 0.0f;
    }
}

void phys_release(int body) {
    // the spring already built up momentum; just detach
    P.bodies[body].grabbed = false;
}
