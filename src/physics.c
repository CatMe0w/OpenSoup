// Souptoys physics core, transliterated from Toybox.exe. Addresses below
// refer to that binary. Preserve pass order, expression grouping and f32()
// stores: the original x87 code uses PC=53 and rounds at explicit stores.
#include "physics.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Original float store (fstp).
static inline float f32(double x) { return (float)x; }

typedef struct { float x, y; } v2;

// Row-major affine matrix used by mat3_from_angle @0x440FD0:
// [ cos, sin, tx; -sin, cos, ty; 0, 0, 1 ].
typedef float mat3[9];

// Vector and matrix primitives
// Dot/cross round once; component-wise helpers round per component.

static float v2_dot(v2 a, v2 b) {          // vec2_dot @0x423600
    return f32((double)a.x * b.x + (double)a.y * b.y);
}

static float v2_cross(v2 a, v2 b) {        // vec2_cross @0x531C80
    return f32((double)a.y * b.x - (double)a.x * b.y);
}

static v2 v2_add(v2 a, v2 b) { return (v2){ f32(a.x + b.x), f32(a.y + b.y) }; }
static v2 v2_sub(v2 a, v2 b) { return (v2){ f32(a.x - b.x), f32(a.y - b.y) }; }
static v2 v2_scale(v2 a, float s) { return (v2){ f32(a.x * s), f32(a.y * s) }; }
static v2 v2_neg(v2 a) { return (v2){ f32(-a.x), f32(-a.y) }; }

// vec2_perp_eq @0x531C50.
static v2 v2_perp(v2 a) { return (v2){ f32(-a.y), a.x }; }

// vec2_normalise_eq @0x532B80; sqrt matches sqrtf_pow @0x532C00.
#define V2_NORM_EPSILON 9.999999747378752e-06f
static v2 v2_normalise(v2 a) {
    const float len2 = f32((double)a.x * a.x + (double)a.y * a.y);
    const float len = f32(sqrt(len2));
    if (len > V2_NORM_EPSILON) {
        const float inv = f32(1.0 / len);
        return (v2){ f32(a.x * inv), f32(a.y * inv) };
    }
    return a;
}

static void mat3_from_angle(mat3 m, float radians, v2 translation) {
    const float c = f32(cos(radians));   // Original CRT results match libm.
    const float s = f32(sin(radians));
    m[0] = c;  m[1] = s;       m[2] = translation.x;
    m[3] = f32(-s); m[4] = c;  m[5] = translation.y;
    m[6] = 0.0f; m[7] = 0.0f;  m[8] = 1.0f;
}

// mat3_inverse @0x441160 -> mat3_invert_into @0x441090.
static void mat3_inverse(mat3 out, const mat3 m) {
    const float det = f32((double)m[0] * m[4] - (double)m[1] * m[3]);
    const float inv = f32(1.0 / det);
    out[0] = f32(m[4] * inv);
    out[1] = f32(-m[1] * inv);
    out[3] = f32(-m[3] * inv);
    out[4] = f32(m[0] * inv);
    out[2] = f32(-((double)out[0] * m[2] + (double)out[1] * m[5]));
    out[5] = f32(-((double)out[3] * m[2] + (double)out[4] * m[5]));
    out[6] = m[6]; out[7] = m[7]; out[8] = m[8];
}

static v2 mat3_xform_point(v2 p, const mat3 m) {   // @0x423710
    return (v2){ f32((double)p.x * m[0] + (double)p.y * m[1] + m[2]),
                 f32((double)p.x * m[3] + (double)p.y * m[4] + m[5]) };
}

static v2 mat3_rotate(v2 v, const mat3 m) {        // @0x4232E0
    return (v2){ f32((double)v.x * m[0] + (double)v.y * m[1]),
                 f32((double)v.x * m[3] + (double)v.y * m[4]) };
}

// Collision shapes

typedef struct {
    v2 normal;
    float offset;   // dot(point, normal) == offset
} edge_plane;

typedef struct {
    int count;
    v2 vertex[PHYS_MAX_SHAPE_VERTS];
    float radius[PHYS_MAX_SHAPE_VERTS];
    edge_plane edge[PHYS_MAX_SHAPE_VERTS];
    float min_x, min_y, max_x, max_y;
    uint64_t member_of;      // membership masks
    uint64_t repels;         // linear response targets
    uint64_t rotates;        // angular response targets
    uint64_t collides_with;  // broadphase union
} sub_shape;

// subshape_recompute_aabb @0x4233B0; expand each vertex by its radius.
static void subshape_aabb(sub_shape* s) {
    if (s->count <= 0) return;
    s->min_x = f32(s->vertex[0].x - s->radius[0]);
    s->min_y = f32(s->vertex[0].y - s->radius[0]);
    s->max_x = f32(s->vertex[0].x + s->radius[0]);
    s->max_y = f32(s->vertex[0].y + s->radius[0]);
    for (int i = 1; i < s->count; i++) {
        const v2 v = s->vertex[i];
        const float r = s->radius[i];
        if (s->min_x > (double)v.x - r) s->min_x = f32((double)v.x - r);
        if (s->min_y > (double)v.y - r) s->min_y = f32((double)v.y - r);
        if (s->max_x < (double)v.x + r) s->max_x = f32((double)v.x + r);
        if (s->max_y < (double)v.y + r) s->max_y = f32((double)v.y + r);
    }
}

// subshape_transform @0x423520; offsets use the transformed normal.
static void subshape_transform(sub_shape* dst, const sub_shape* src,
                               const mat3 m) {
    *dst = *src;
    for (int i = 0; i < dst->count; i++) {
        dst->vertex[i] = mat3_xform_point(src->vertex[i], m);
        const v2 n = mat3_rotate(src->edge[i].normal, m);
        dst->edge[i].normal = n;
        dst->edge[i].offset =
            f32(f32((double)n.x * m[2] + (double)n.y * m[5]) + src->edge[i].offset);
    }
    subshape_aabb(dst);
}

// Shape::add @0x558BE0 rewrites the previous and closing planes on each
// append. Preserve its endpoint anchoring and rounded normalization; it
// assumes clockwise input and performs no winding correction.
static void subshape_build_edges(sub_shape* s) {
    if (s->count <= 0) return;
    s->edge[0].normal = (v2){ 0.0f, 0.0f };
    s->edge[0].offset = 0.0f;
    for (int n = 1; n < s->count; n++) {
        const v2 v = s->vertex[n];
        v2 nrm = v2_perp(v2_normalise(v2_sub(v, s->vertex[n - 1])));
        s->edge[n - 1].normal = nrm;
        s->edge[n - 1].offset = v2_dot(nrm, v);
        nrm = v2_perp(v2_normalise(v2_sub(s->vertex[0], v)));
        s->edge[n].normal = nrm;
        s->edge[n].offset = v2_dot(nrm, v);
    }
}

// Simulation data
// Each stage stores a copied core state plus its accumulated derivative.

typedef struct {
    v2 position;
    v2 momentum;
    float orientation;        // radians, unbounded
    float angular_momentum;
    bool supported;      // support rebuilt each stage
    int awake_counter;
    v2 velocity_cache;        // momentum * inv_mass
    float angular_velocity_cache;
    mat3 transform;           // built from -orientation
    mat3 inv_transform;
    float mass, inv_mass, inv_inertia;
    bool anchored, anchored2;
} core_state;

typedef struct {
    v2 velocity;              // d(position)
    v2 force;                 // d(momentum)
    float angular_velocity;   // d(orientation)
    float torque;             // d(angular_momentum)
    core_state core;
} stage_state;

// body_state_derive @0x4411A0: derive rates, then transforms.
static void core_derive(core_state* c) {
    c->velocity_cache = v2_scale(c->momentum, c->inv_mass);
    c->angular_velocity_cache = f32(c->angular_momentum * c->inv_inertia);
    mat3_from_angle(c->transform, f32(-c->orientation), c->position);
    mat3_inverse(c->inv_transform, c->transform);
}

// Joint_apply @0x532800 record.
typedef struct {
    int b1, b2;
    v2 a1, a2;                // body-local anchors
    float k, c, rest;
    bool move1, move2;
    bool rotate1, rotate2;
    v2 axis;                  // body1-local
    bool axis_on;
    bool active;
} joint_t;

// RotationalJoint_apply @0x532CD0; rest is stored but never read.
typedef struct {
    int b1, b2;
    float o1, o2;             // reference orientations
    float k, c, rest, ratio;
    bool rotate1, rotate2;
    bool active;
} rotjoint_t;

// Magnet_apply @0x532E70 record. One shared array preserves the engine's
// per-group producer/consumer order.
typedef struct {
    int body;
    uint32_t group;
    v2 attach;                // body-local
    bool producer;
    bool bidirectional, inverted, spring_response;
    float k, c, radius;
    bool active;
} magnet_t;

typedef struct {
    core_state core;
    stage_state stage[4];
    // RK4-combined derivative
    v2 k_velocity, k_force;
    float k_angular_velocity, k_torque;

    sub_shape* local;         // body-local, built once
    sub_shape* staged;        // four stage copies
    int nshapes;

    phys_params prm;
    float combined[5];        // first four pre-doubled

    bool dead;
    bool grabbed;
    v2 grab_anchor;           // local point or world offset
    v2 grab_target;
    bool grab_move, grab_rotate;
} body_t;

// Contact_* modifies only body; Constraint_create emits a mirrored pair
// for the opposing body.
typedef struct {
    v2 rel_vel;
    float response_v, response_k, response_d;
    float friction_peak;      // material seed, then max(mass * 4, value)
    float static_friction;
    stage_state* body;
    stage_state* other;       // NULL for a wall
    v2 normal;
    v2 point;
    float penetration;
    int counts;               // [linear, angular] count slot
    v2 accum;
    bool apply_linear, apply_angular;
    // Capture provenance.
    const sub_shape* shape;
    const sub_shape* other_shape;
} contact_pair;

static struct {
    float ww, wh;
    body_t* bodies;
    int nbodies, body_cap;
    joint_t* joints;
    int njoints, joint_cap;
    rotjoint_t* rotjoints;
    int nrotjoints, rotjoint_cap;
    magnet_t* magnets;
    int nmagnets, magnet_cap;
    uint32_t* groups;         // first-seen order
    int ngroups, group_cap;

    contact_pair* pairs;
    int npairs, pair_cap;
    int (*counts)[2];
    int ncounts, count_cap;
    int norder;
    int step, stage;          // diagnostics
    FILE* capture;            // armed one-step capture
    bool capturing;           // active capture flag
} P;

// Diagnostic lookup helpers

// PHYS_DEBUG_CONTACTS is a step number or "all"; unset disables it.
static bool phys_debug_step(void) {
    static int want = -2;     // -2 unread, -1 off, INT_MIN all
    if (want == -2) {
        const char* s = getenv("PHYS_DEBUG_CONTACTS");
        want = !s ? -1 : (strcmp(s, "all") == 0 ? INT_MIN : atoi(s));
    }
    return want != -1 && (want == INT_MIN || want == P.step);
}

// Return -1 for a wall, otherwise the owning body index.
static int debug_body_of(const stage_state* s) {
    if (!s) return -1;
    for (int i = 0; i < P.nbodies; i++) {
        if (s >= P.bodies[i].stage && s < P.bodies[i].stage + 4) return i;
    }
    return -2;
}

// Return the staged sub-shape index, or -1 for walls/unknown shapes.
static int debug_shape_of(const stage_state* s, const sub_shape* shape) {
    const int index = debug_body_of(s);
    if (index < 0 || !shape) return -1;
    const body_t* b = &P.bodies[index];
    for (int stage = 0; stage < 4; stage++) {
        for (int i = 0; i < b->nshapes; i++) {
            if (&b->staged[stage * b->nshapes + i] == shape) return i;
        }
    }
    return -1;
}

// Dynamic storage

static int grown_capacity(int current, int wanted, int initial) {
    int cap = current ? current : initial;
    while (cap < wanted) {
        if (cap > INT_MAX / 2) return 0;
        cap *= 2;
    }
    return cap;
}

#define DEFINE_RESERVE(name, field, cap_field, type, initial)                 \
    static bool name(int wanted) {                                            \
        if (wanted <= P.cap_field) return true;                               \
        const int cap = grown_capacity(P.cap_field, wanted, initial);         \
        if (!cap) return false;                                               \
        type* grown = realloc(P.field, (size_t)cap * sizeof(*grown));         \
        if (!grown) return false;                                             \
        P.field = grown;                                                      \
        P.cap_field = cap;                                                    \
        return true;                                                          \
    }

DEFINE_RESERVE(reserve_bodies, bodies, body_cap, body_t, 64)
DEFINE_RESERVE(reserve_joints, joints, joint_cap, joint_t, 128)
DEFINE_RESERVE(reserve_rotjoints, rotjoints, rotjoint_cap, rotjoint_t, 128)
DEFINE_RESERVE(reserve_pairs, pairs, pair_cap, contact_pair, 256)
DEFINE_RESERVE(reserve_magnets, magnets, magnet_cap, magnet_t, 64)
DEFINE_RESERVE(reserve_groups, groups, group_cap, uint32_t, 16)

static bool reserve_counts(int wanted) {
    if (wanted <= P.count_cap) return true;
    const int cap = grown_capacity(P.count_cap, wanted, 128);
    if (!cap) return false;
    int (*grown)[2] = realloc(P.counts, (size_t)cap * sizeof(*grown));
    if (!grown) return false;
    P.counts = grown;
    P.count_cap = cap;
    return true;
}

// Collision groups
// Definitions provide memberOf only; derive the inverse repels/rotates masks.

static uint64_t groups_repelled_by(uint32_t member) {
    uint64_t r = 0;
    if (member & PHYS_GROUP_BOUNCER_REPELLERS)   r |= PHYS_GROUP_BOUNCERS;
    if (member & PHYS_GROUP_EXCLUSION_REPELLERS) r |= PHYS_GROUP_EXCLUSION;
    if (member & PHYS_GROUP_LEFT_WALL)   r |= PHYS_GROUP_LEFT_WALL_REPEL;
    if (member & PHYS_GROUP_RIGHT_WALL)  r |= PHYS_GROUP_RIGHT_WALL_REPEL;
    if (member & PHYS_GROUP_FLOOR)       r |= PHYS_GROUP_FLOOR_REPEL;
    if (member & PHYS_GROUP_CEILING)     r |= PHYS_GROUP_CEILING_REPEL;
    return r;
}

static uint64_t groups_rotated_by(uint32_t member) {
    uint64_t r = 0;
    if (member & PHYS_GROUP_SPINNER_ROTATORS) r |= PHYS_GROUP_SPINNERS;
    if (member & PHYS_GROUP_LEFT_WALL)   r |= PHYS_GROUP_LEFT_WALL_ROTATE;
    if (member & PHYS_GROUP_RIGHT_WALL)  r |= PHYS_GROUP_RIGHT_WALL_ROTATE;
    if (member & PHYS_GROUP_FLOOR)       r |= PHYS_GROUP_FLOOR_ROTATE;
    if (member & PHYS_GROUP_CEILING)     r |= PHYS_GROUP_CEILING_ROTATE;
    return r;
}

static void shape_set_masks(sub_shape* s, uint32_t groups) {
    s->member_of = groups;
    s->repels = groups_repelled_by(groups);
    s->rotates = groups_rotated_by(groups);
    s->collides_with = s->repels | s->rotates;
}

// Separating-axis tests
// sat_distances @0x535340 uses dist[16 * edge + vertex]. Circle cases
// overwrite b->edge[0]; narrowphase intentionally consumes the last normal.

typedef struct {
    float dist[16 * 17];
    int deepest[17];
    int counts[17];
    unsigned char inside[16];
    int best_edge;
    int n_inside;
} sat_result;

static bool sat_distances(const sub_shape* a, sub_shape* b, sat_result* out) {
    memset(out, 0, sizeof(*out));

    if (a->count == 1 && b->count == 1) {           // circle vs circle
        const v2 n = v2_normalise(v2_sub(a->vertex[0], b->vertex[0]));
        b->edge[0].normal = n;
        b->edge[0].offset = f32(v2_dot(b->vertex[0], n) + b->radius[0]);
        out->dist[0] = f32((double)v2_dot(a->vertex[0], n)
                           - b->edge[0].offset - a->radius[0]);
        if (out->dist[0] <= 0.0f) {
            out->inside[0] = 1;
            out->n_inside = 1;
            return true;
        }
        return false;
    }

    if (b->count == 1) {                            // polygon vs circle
        for (int i = 0; i < a->count; i++) {
            const v2 v = a->vertex[i];
            const v2 prev_n = a->edge[(i + a->count - 1) % a->count].normal;
            const v2 cur_n = a->edge[i].normal;
            v2 rel = v2_sub(v, b->vertex[0]);
            if (v2_cross(rel, prev_n) < 0.0f || v2_cross(rel, cur_n) > 0.0f) {
                out->dist[i] = 1.0f;                // outside the vertex wedge
                continue;
            }
            rel = v2_normalise(rel);
            b->edge[0].normal = rel;
            b->edge[0].offset = f32(v2_dot(b->vertex[0], rel) + b->radius[0]);
            out->dist[i] = f32((double)v2_dot(v, b->edge[0].normal)
                               - b->edge[0].offset - a->radius[i]);
            out->inside[i] = out->dist[i] <= 0.0f;
            if (out->inside[i]) out->n_inside++;
            if (out->dist[i] > 0.0f) return false;
        }
        return true;
    }

    for (int j = 0; j < b->count; j++) {            // polygon vs polygon
        out->counts[j] = 0;
        for (int k = 0; k < a->count; k++) {
            const float d = f32((double)v2_dot(a->vertex[k], b->edge[j].normal)
                                - b->edge[j].offset - a->radius[k]);
            out->dist[16 * j + k] = d;
            if (!(k && out->dist[16 * j + out->deepest[j]] <= d)) {
                out->deepest[j] = k;
            }
            out->inside[k] = ((j == 0) || out->inside[k]) && d <= 0.0f;
            if (d <= 0.0f) out->counts[j]++;
        }
        if (out->dist[16 * j + out->deepest[j]] > 0.0f) return false;
    }
    for (int m = 0; m < a->count; m++) {
        if (out->inside[m]) out->n_inside++;
    }
    for (int n = 0; n < b->count; n++) {
        const int best = out->best_edge;
        if (!(n && out->dist[16 * best + out->deepest[best]]
                       >= out->dist[16 * n + out->deepest[n]])) {
            out->best_edge = n;
        }
    }
    return true;
}

// Contact generation

// Store count-slot indices because P.counts may reallocate during broadphase.
static int alloc_counts(void) {
    if (!reserve_counts(P.ncounts + 1)) return -1;
    const int slot = P.ncounts++;
    P.counts[slot][0] = P.counts[slot][1] = 0;
    return slot;
}

static v2 point_velocity(const stage_state* s, v2 point) {
    const v2 r_perp = v2_perp(v2_sub(point, s->core.position));
    return v2_add(v2_scale(r_perp, s->core.angular_velocity_cache),
                  s->core.velocity_cache);
}

// Build one receiver-side ContactPair; material belongs to self and is
// multiplied by scale.
static void push_pair(body_t* self, stage_state* self_state,
                      stage_state* other_state,
                      v2 normal, v2 point, v2 rel, float depth, float scale,
                      int counts, bool linear, bool angular,
                      const sub_shape* shape, const sub_shape* other_shape) {
    if (!reserve_pairs(P.npairs + 1)) return;

    // Mirrored body-body entries reverse the normal; one-sided hits emit one.
    if (phys_debug_step()) {
        fprintf(stderr, "  [s%d.%d] contact self=%d other=%d n=(%+.6f,%+.6f) "
                "pt=(%+.6f,%+.6f) depth=%.6f cnt=%d lin=%d ang=%d\n",
                P.step, P.stage, (int)(self - P.bodies),
                debug_body_of(other_state),
                normal.x, normal.y, point.x, point.y, depth, counts,
                linear, angular);
    }
    contact_pair* p = &P.pairs[P.npairs++];
    p->rel_vel = rel;
    p->response_v = f32(self->combined[0] * scale);
    p->response_k = f32(self->combined[1] * scale);
    p->response_d = f32(self->combined[2] * scale);
    p->friction_peak = f32(self->combined[3] * scale);
    p->static_friction = self->combined[4];
    p->body = self_state;
    p->other = other_state;
    p->normal = normal;
    p->point = point;
    p->penetration = depth;
    p->counts = counts;
    p->accum = (v2){ 0.0f, 0.0f };
    p->apply_linear = linear != 0;
    p->apply_angular = angular != 0;
    p->shape = shape;
    p->other_shape = other_shape;
    if (p->apply_linear) P.counts[counts][0]++;
    if (p->apply_angular) P.counts[counts][1]++;
}

// Constraint_create @0x534E00 emits a primary pair and its mirrored reaction.
// Contact passes modify only pair->body. The primary and mirror count slots
// retain that order for both narrowphase directions.
static void constraint_create(body_t* self, stage_state* sa,
                              body_t* other, stage_state* sb,
                              v2 normal, float radius, v2 vertex, float depth,
                              const sub_shape* self_shape,
                              const sub_shape* other_shape,
                              int counts_primary, int counts_mirror) {
    // Our action masks are the inverse naming of the binary's target masks.
    const bool lin_self  = (self_shape->member_of & other_shape->repels) != 0;
    const bool ang_self  = (self_shape->member_of & other_shape->rotates) != 0;
    const bool lin_other = (other_shape->member_of & self_shape->repels) != 0;
    const bool ang_other = (other_shape->member_of & self_shape->rotates) != 0;
    if (!lin_self && !ang_self && !lin_other && !ang_other) return;

    const v2 point_self = v2_sub(vertex, v2_scale(normal, radius));
    const v2 point_other = v2_add(point_self, v2_scale(normal, depth));

    const v2 rel = v2_sub(point_velocity(sa, point_self),
                          point_velocity(sb, point_other));

    if (lin_self || ang_self) {
        push_pair(self, sa, sb, normal, point_self, rel, depth,
                  f32((double)sb->core.mass * sa->core.inv_mass),
                  counts_primary, lin_self, ang_self,
                  self_shape, other_shape);
    }
    if (lin_other || ang_other) {
        push_pair(other, sb, sa, v2_neg(normal), point_other, v2_neg(rel),
                  depth, f32((double)sa->core.mass * sb->core.inv_mass),
                  counts_mirror, lin_other, ang_other,
                  other_shape, self_shape);
    }
}

// Narrowphase_pair @0x534750; both directions share the two count slots.
static void narrowphase_pair(body_t* ba, stage_state* sa, sub_shape* sha,
                             body_t* bb, stage_state* sb, sub_shape* shb,
                             int counts_ab, int counts_ba) {
    if (!(shb->max_x >= sha->min_x && shb->max_y >= sha->min_y
          && sha->max_x >= shb->min_x && sha->max_y >= shb->min_y)) {
        return;
    }
    if (!((shb->collides_with & sha->member_of)
          || (sha->collides_with & shb->member_of))) {
        return;
    }

    sat_result r1, r2;
    if (!sat_distances(sha, shb, &r1)) return;
    if (!sat_distances(shb, sha, &r2)) return;
    const int total = r1.n_inside + r2.n_inside;

    for (int i = 0; i < sha->count; i++) {
        const float depth = r1.dist[16 * r1.best_edge + i];
        if (depth > 0.0f) continue;
        bool accept = shb->count == 1;
        if (!accept) {
            const v2 edge = v2_sub(shb->vertex[(r1.best_edge + 1) % shb->count],
                                   shb->vertex[r1.best_edge]);
            const float c = v2_cross(v2_sub(sha->vertex[i],
                                            shb->vertex[r1.best_edge]),
                                     shb->edge[r1.best_edge].normal);
            accept = (r1.inside[i] && sha->count > 1)
                  || ((sha->count == 1 || !total
                       || r2.counts[r2.best_edge] > 1)
                      && c <= 0.0f
                      && v2_dot(edge, edge) >= (double)c * c);
        }
        if (accept) {
            constraint_create(ba, sa, bb, sb, shb->edge[r1.best_edge].normal,
                              sha->radius[i], sha->vertex[i], f32(-depth),
                              sha, shb, counts_ab, counts_ba);
        }
    }
    for (int j = 0; j < shb->count; j++) {
        const float depth = r2.dist[16 * r2.best_edge + j];
        if (depth > 0.0f) continue;
        bool accept = sha->count == 1;
        if (!accept) {
            const v2 edge = v2_sub(sha->vertex[(r2.best_edge + 1) % sha->count],
                                   sha->vertex[r2.best_edge]);
            const float c = v2_cross(v2_sub(shb->vertex[j],
                                            sha->vertex[r2.best_edge]),
                                     sha->edge[r2.best_edge].normal);
            accept = (r2.inside[j] && shb->count > 1)
                  || ((shb->count == 1 || !total
                       || r1.counts[r1.best_edge] > 1)
                      && c <= 0.0f
                      && v2_dot(edge, edge) >= (double)c * c);
        }
        if (accept) {
            constraint_create(bb, sb, ba, sa, sha->edge[r2.best_edge].normal,
                              shb->radius[j], shb->vertex[j], f32(-depth),
                              shb, sha, counts_ab, counts_ba);
        }
    }
}

// Contact_wall @0x535970 emits one receiver-side pair with no mass ratio or
// reaction body.
static void wall_contacts(body_t* b, stage_state* s, sub_shape* shape,
                          const sub_shape* wall, int counts) {
    for (int i = 0; i < shape->count; i++) {
        const v2 v = shape->vertex[i];
        const float r = shape->radius[i];
        const float depth = f32((double)wall->edge[0].offset
                                - v2_dot(v, wall->edge[0].normal) + r);
        if (depth <= 0.0f) continue;
        const bool linear = (shape->member_of & wall->repels) != 0;
        const bool angular = (shape->member_of & wall->rotates) != 0;
        if (!linear && !angular) continue;
        const v2 point = v2_sub(v, v2_scale(wall->edge[0].normal, r));
        push_pair(b, s, NULL, wall->edge[0].normal, point,
                  point_velocity(s, point), depth, 1.0f,
                  counts, linear, angular, shape, wall);
    }
}

// Contact solver

// Contact_prepare @0x5318C0 splits linear and angular response through
// independently rounded reciprocal contact counts.
static void contact_prepare(contact_pair* p, v2 ref) {
    core_state* c = &p->body->core;
    const v2 r = v2_perp(v2_sub(p->point, c->position));
    const int* counts = P.counts[p->counts];
    const float inv_lin = counts[0] ? f32(1.0 / counts[0]) : 0.0f;
    const float inv_ang = counts[1] ? f32(1.0 / counts[1]) : 0.0f;
    const float closing = -v2_dot(p->normal, p->rel_vel);

    // Preserve three independently rounded response terms.
    v2 total = (v2){ 0.0f, 0.0f };
    if (closing > 0.0f) {
        total = v2_scale(p->normal, f32(closing * p->response_v));
    }
    total = v2_add(total,
                   v2_scale(p->normal, f32(p->penetration * p->response_k)));
    // Preserve one rounding after all three factors.
    total = v2_add(total,
                   v2_scale(p->normal, f32((double)closing * p->penetration
                                           * p->response_d)));

    float angular = v2_dot(r, v2_scale(total, inv_ang));
    total = v2_scale(total, inv_lin);
    const float proj = v2_dot(total, ref);

    // Body-body grazing stabilizer for supported bodies opposing ref.
    if (c->supported && p->other && !p->other->core.anchored && proj < 0.0f) {
        const float blend = fabsf(v2_cross(p->normal, ref));
        if (blend > 0.1f) {
            angular = f32(angular * blend);
            total = v2_sub(total, v2_scale(ref, f32((1.0 - blend) * proj)));
        } else {
            angular = 0.0f;
            total = (v2){ 0.0f, 0.0f };
        }
    }

    if (p->apply_linear) {
        p->body->force = v2_add(p->body->force, total);
        p->accum = v2_add(p->accum, total);
    }
    if (p->apply_angular) {
        p->body->torque = f32(p->body->torque + angular);
    }
    if (p->other) {
        if (p->other->core.supported && proj > 0.0f) c->supported = true;
    } else if (p->normal.x == ref.x && p->normal.y == ref.y) {
        c->supported = true;
    }
}

// Contact_solveIter @0x531CE0: cancel tangential rate inside the static
// friction cone. Called four times per stage.
static void contact_solve_iter(contact_pair* p, v2 ref) {
    core_state* c = &p->body->core;
    const v2 r = v2_perp(v2_sub(p->point, c->position));
    const v2 t = v2_perp(p->normal);

    // Walls always solve; body contacts follow support/shock ordering.
    if (p->other) {
        if (!p->other->core.supported) return;
        if (!(v2_dot(v2_sub(c->position, p->other->core.position), ref) > 0.0f)) {
            return;
        }
    }

    const float rt = v2_dot(r, t);
    const float s = f32((double)rt * -p->body->torque * c->inv_inertia
                        - (double)v2_dot(p->body->force, t) * c->inv_mass);
    const v2 j = v2_scale(t, s);
    const float denom = f32((double)rt * rt * c->inv_inertia + c->inv_mass);
    const v2 jt = denom != 0.0f
                      ? (v2){ f32(j.x / denom), f32(j.y / denom) }
                      : (v2){ 0.0f, 0.0f };

    const float torque = v2_dot(jt, r);
    // Keep the cone bound in double; only the left side is rounded.
    const double cone = (double)v2_dot(p->accum, p->normal) * p->static_friction;
    const float y = v2_dot(jt, t);

    if (fabsf(y) < cone) {
        const double peak = (double)c->mass * 4.0;
        if (peak >= p->friction_peak) p->friction_peak = f32(peak);
        if (p->apply_linear) p->body->force = v2_add(p->body->force, jt);
        if (p->apply_angular) p->body->torque = f32(p->body->torque + torque);
    }
}

// Contact_finalize @0x531FC0 applies the tangential residual. Its support
// guard is intentionally the inverse of solveIter's sleeping-partner guard.
static void contact_finalize(contact_pair* p, v2 ref) {
    core_state* c = &p->body->core;
    const v2 r = v2_perp(v2_sub(p->point, c->position));

    const bool proceed =
        !p->other || !p->other->core.supported
        || v2_dot(v2_sub(c->position, p->other->core.position), ref) > 0.0f;
    if (proceed) {
        const float closing = -v2_dot(p->normal, p->rel_vel);
        const v2 tangential = v2_add(p->rel_vel,
                                     v2_scale(p->normal, closing));
        const v2 impulse = v2_scale(v2_neg(tangential), p->friction_peak);
        if (p->apply_linear) p->body->force = v2_add(p->body->force, impulse);
        if (p->apply_angular) {
            p->body->torque = f32((double)v2_dot(r, impulse) + p->body->torque);
        }
    }
    // Omitted: coincident anchored bodies are separated using an unavailable
    // object ID.
}

// Stage setup and force passes

static const float rk4_coeffs[4] = { 0.0f, 0.5f, 0.5f, 1.0f };

// World-up reference for support propagation, not the gravity vector.
static const v2 contact_ref = { 0.0f, 1.0f };

// Body_integrateStage @0x531510 -> Body_integratePosition @0x5323C0.
static void integrate_stage(body_t* b, int stage) {
    const float h = f32(PHYS_DT * rk4_coeffs[stage]);
    stage_state* cur = &b->stage[stage];
    const stage_state* prev = &b->stage[(stage + 3) % 4];

    cur->core = b->core;                       // body_state_copy @0x441230

    if (h != 0.0f) {
        cur->core.position =
            v2_add(cur->core.position, v2_scale(prev->velocity, h));
        cur->core.momentum =
            v2_add(cur->core.momentum, v2_scale(prev->force, h));
        cur->core.orientation =
            f32((double)prev->angular_velocity * h + cur->core.orientation);
        cur->core.angular_momentum =
            f32((double)prev->torque * h + cur->core.angular_momentum);
        core_derive(&cur->core);
    }

    // Static geometry starts supported; other bodies earn support in contacts.
    cur->core.supported = (cur->core.anchored && cur->core.anchored2)
                          || cur->core.awake_counter > 0;

    // Seed the derivative from rates, then clear force and torque.
    cur->velocity = cur->core.velocity_cache;
    cur->angular_velocity = cur->core.angular_velocity_cache;
    cur->force = (v2){ 0.0f, 0.0f };
    cur->torque = 0.0f;

    for (int i = 0; i < b->nshapes; i++) {
        subshape_transform(&b->staged[stage * b->nshapes + i], &b->local[i],
                           cur->core.transform);
    }
}

// Body_applyAirResistance @0x5325D0 is called from Body_applyGravity.
// Offset drag samples point velocity and adds torque; fixedRotate gates only
// that offset torque.
static void apply_air(body_t* b, stage_state* s, bool linear, bool angular) {
    const core_state* c = &s->core;
    if (angular) {
        s->torque = f32(s->torque
                        - (double)b->prm.air_angular * c->angular_velocity_cache);
    }
    if (!linear) return;

    const v2 cor = { b->prm.centre_of_resistance[0],
                     b->prm.centre_of_resistance[1] };
    if (fabsf(cor.x) < 9.999999747378752e-06f
        && fabsf(cor.y) < 9.999999747378752e-06f) {
        s->force = v2_sub(s->force, v2_scale(c->velocity_cache,
                                             b->prm.air_linear));
        return;
    }
    const v2 world = mat3_xform_point(cor, c->transform);
    const v2 r = v2_perp(v2_sub(world, c->position));
    const v2 point_vel = v2_add(v2_scale(r, c->angular_velocity_cache),
                                c->velocity_cache);
    const v2 drag = v2_scale(point_vel, b->prm.air_linear);
    s->force = v2_sub(s->force, drag);
    if (!c->anchored2) {
        s->torque = f32(s->torque - v2_dot(r, drag));
    }
}

// Body_applyGravity @0x532330 applies gravity, then air. Anchored bodies skip
// gravity but can still receive linear or angular drag.
static void apply_gravity(body_t* b, stage_state* s) {
    if (!s->core.anchored) {
        s->force.y = f32((double)b->prm.gravity * s->core.mass + s->force.y);
        apply_air(b, s, true, true);
    } else if (s->core.awake_counter == 0) {
        apply_air(b, s, true, true);
    } else if (s->core.awake_counter > 0) {
        apply_air(b, s, false, true);
    }
}

// Motors run after gravity and air.
static void apply_body_forces(body_t* b, stage_state* s) {
    const core_state* c = &s->core;

    if (b->prm.motor_force[0] != 0.0f || b->prm.motor_force[1] != 0.0f) {
        // Rotate body-local motor force into world space.
        const v2 local = { b->prm.motor_force[0], b->prm.motor_force[1] };
        s->force = v2_add(s->force, mat3_rotate(local, c->transform));
    }
    if (b->prm.motor_torque != 0.0f && !b->prm.fixed_rotate) {
        s->torque = f32(s->torque + b->prm.motor_torque);
    }
}

// Mouse_spring @0x5321F0 uses one world target. move/rotate gate both the
// damping inputs and the applied force/torque.
static void mouse_spring(stage_state* s, v2 anchor, v2 target,
                         float stiffness, float dampener,
                         bool move, bool rotate) {
    const core_state* c = &s->core;
    const v2 r_perp = v2_perp(v2_sub(anchor, c->position));

    v2 anchor_vel = { 0.0f, 0.0f };
    if (move) anchor_vel = v2_add(anchor_vel, c->velocity_cache);
    if (rotate) {
        anchor_vel = v2_add(anchor_vel,
                            v2_scale(r_perp, c->angular_velocity_cache));
    }
    const v2 force = v2_sub(v2_scale(v2_sub(target, anchor), stiffness),
                            v2_scale(anchor_vel, dampener));
    if (move) s->force = v2_add(s->force, force);
    if (rotate) s->torque = f32((double)v2_dot(r_perp, force) + s->torque);
}

// Mouse anchors rotate only when requested; grab stiffness and damping are
// scaled by body mass.
static v2 mouse_anchor(const stage_state* s, v2 local, bool rotate) {
    return rotate ? mat3_xform_point(local, s->core.transform)
                  : v2_add(local, s->core.position);
}

// Joint_apply @0x532800 computes one two-body spring and applies opposite
// forces. Anchors always rotate; there is no mass scaling.
static void apply_joints(int stage) {
    for (int i = 0; i < P.njoints; i++) {
        const joint_t* j = &P.joints[i];
        if (!j->active) continue;
        stage_state* s1 = &P.bodies[j->b1].stage[stage];
        stage_state* s2 = &P.bodies[j->b2].stage[stage];

        const v2 p1 = mat3_xform_point(j->a1, s1->core.transform);
        const v2 p2 = mat3_xform_point(j->a2, s2->core.transform);
        const v2 r1 = v2_perp(v2_sub(p1, s1->core.position));
        const v2 r2 = v2_perp(v2_sub(p2, s2->core.position));

        // Damping always includes both linear and angular anchor rates.
        const v2 vel1 = v2_add(v2_scale(r1, s1->core.angular_velocity_cache),
                               s1->core.velocity_cache);
        const v2 vel2 = v2_add(v2_scale(r2, s2->core.angular_velocity_cache),
                               s2->core.velocity_cache);
        v2 rel = v2_sub(vel2, vel1);
        v2 sep = v2_sub(p1, p2);

        // A nonzero rest length constrains spring and damping to the axis.
        if (j->rest != 0.0f) {
            const v2 dir = v2_normalise(sep);
            sep = v2_sub(sep, v2_scale(dir, j->rest));
            rel = v2_scale(dir, v2_dot(rel, dir));
        }

        v2 force = v2_sub(v2_scale(sep, j->k), v2_scale(rel, j->c));
        if (j->axis_on) {   // body1-local axis
            const v2 axis = mat3_rotate(j->axis, s1->core.transform);
            force = v2_scale(axis, v2_dot(force, axis));
        }

        if (j->move1) s1->force = v2_sub(s1->force, force);
        if (j->rotate1) s1->torque = f32(s1->torque - v2_dot(r1, force));
        if (j->move2) s2->force = v2_add(s2->force, force);
        if (j->rotate2) s2->torque = f32((double)v2_dot(r2, force) + s2->torque);
    }

    // RotationalJoint_apply @0x532CD0 applies ratio to both error and reaction.
    // rest exists in the record but is intentionally unused.
    for (int i = 0; i < P.nrotjoints; i++) {
        const rotjoint_t* rj = &P.rotjoints[i];
        if (!rj->active) continue;
        stage_state* s1 = &P.bodies[rj->b1].stage[stage];
        stage_state* s2 = &P.bodies[rj->b2].stage[stage];

        const float err = f32(((double)s2->core.orientation + rj->o2) * rj->ratio
                              - ((double)s1->core.orientation + rj->o1));
        const float rate = f32((double)s2->core.angular_velocity_cache * rj->ratio
                               - s1->core.angular_velocity_cache);
        const float tau = f32((double)err * rj->k + (double)rate * rj->c);

        if (rj->rotate1) s1->torque = f32(s1->torque + tau);
        if (rj->rotate2) s2->torque = f32(s2->torque - f32(tau * rj->ratio));
    }
}

static void apply_grabs(int stage) {
    for (int i = 0; i < P.nbodies; i++) {
        body_t* b = &P.bodies[i];
        if (b->dead || !b->grabbed) continue;
        stage_state* s = &b->stage[stage];
        const bool rotate = b->grab_rotate && !b->prm.fixed_rotate;
        const v2 anchor = mouse_anchor(s, b->grab_anchor, rotate);
        mouse_spring(s, anchor, b->grab_target,
                     f32(b->prm.mouse_stiffness * s->core.mass),
                     f32(b->prm.mouse_dampener * s->core.mass),
                     b->grab_move, rotate);
    }
}

// Magnet_apply @0x532E70 iterates groups, producers, then consumers. The
// consumer receives force; the producer reacts only when bidirectional.
// spring_response=false uses radius * unit(d) - d. Same-body and same-toy
// pairs are rejected.
static void apply_magnets(int stage) {
    for (int g = 0; g < P.ngroups; g++) {
        const uint32_t group = P.groups[g];
        for (int pi = 0; pi < P.nmagnets; pi++) {
            const magnet_t* mp = &P.magnets[pi];
            if (!mp->active || !mp->producer || mp->group != group) continue;
            body_t* bp = &P.bodies[mp->body];
            if (bp->dead) continue;
            stage_state* sp = &bp->stage[stage];

            for (int ci = 0; ci < P.nmagnets; ci++) {
                const magnet_t* mc = &P.magnets[ci];
                if (!mc->active || mc->producer || mc->group != group) continue;
                body_t* bc = &P.bodies[mc->body];
                if (bc->dead || mp->body == mc->body
                    || bp->prm.toy_id == bc->prm.toy_id) {
                    continue;
                }
                stage_state* sc = &bc->stage[stage];

                const v2 a = mat3_xform_point(mp->attach, sp->core.transform);
                const v2 b = mat3_xform_point(mc->attach, sc->core.transform);
                v2 d = v2_sub(a, b);

                // Compare rounded len2 with an unrounded radius squared.
                const float len2 = f32((double)d.x * d.x + (double)d.y * d.y);
                if (!((double)mp->radius * mp->radius > len2)) continue;
                if (!mp->spring_response) {
                    d = v2_sub(v2_scale(v2_normalise(d), mp->radius), d);
                }

                const v2 rp = v2_perp(v2_sub(a, sp->core.position));
                const v2 rc = v2_perp(v2_sub(b, sc->core.position));
                const v2 vp = v2_add(v2_scale(rp, sp->core.angular_velocity_cache),
                                     sp->core.velocity_cache);
                const v2 vc = v2_add(v2_scale(rc, sc->core.angular_velocity_cache),
                                     sc->core.velocity_cache);
                const v2 rel = v2_sub(vc, vp);

                v2 f = v2_sub(v2_scale(d, mp->k), v2_scale(rel, mp->c));
                if (mp->inverted) f = v2_scale(f, -1.0f);

                sc->force = v2_add(sc->force, f);
                sc->torque = f32((double)v2_dot(rc, f) + sc->torque);
                if (mp->bidirectional) {
                    sp->force = v2_sub(sp->force, f);
                    sp->torque = f32((double)v2_dot(rp, v2_scale(f, -1.0f))
                                     + sp->torque);
                }
            }
        }
    }
}

// Broadphase

static bool bodies_can_collide(const body_t* a, const body_t* b) {
    return !a->dead && !b->dead && a != b
        && (a->prm.toy_id != b->prm.toy_id
            || a->prm.local_group == b->prm.local_group);
}

// World walls are implicit half-planes in engine order: left, right, ceiling,
// floor. This preserves corner-contact accumulation order.
static void build_walls(sub_shape walls[4]) {
    static const uint32_t member[4] = {
        PHYS_GROUP_LEFT_WALL, PHYS_GROUP_RIGHT_WALL,
        PHYS_GROUP_CEILING, PHYS_GROUP_FLOOR
    };
    const v2 normals[4] = { { 1, 0 }, { -1, 0 }, { 0, -1 }, { 0, 1 } };
    const float offsets[4] = { 0.0f, f32(-P.ww), f32(-P.wh), 0.0f };
    for (int w = 0; w < 4; w++) {
        memset(&walls[w], 0, sizeof(walls[w]));
        walls[w].count = 2;                 // engine half-plane sentinel
        walls[w].edge[0].normal = normals[w];
        walls[w].edge[0].offset = offsets[w];
        shape_set_masks(&walls[w], member[w]);
    }
}

// Sort bodies bottom-up once per step so support propagates upward during
// Contact_prepare.
static int* shock_order(void) {
    static int* order;
    static int cap;
    if (cap < P.nbodies) {
        int* grown = realloc(order, (size_t)(P.nbodies + 16) * sizeof(*grown));
        if (!grown) return NULL;
        order = grown;
        cap = P.nbodies + 16;
    }
    // Only shock_order >= 0 enters this set; implicit walls replace the
    // negative World limbs and never reach polygon narrowphase.
    int n = 0;
    for (int i = 0; i < P.nbodies; i++) {
        if (!P.bodies[i].dead && P.bodies[i].prm.shock_order >= 0) {
            order[n++] = i;
        }
    }
    // Small, nearly sorted list.
    for (int i = 1; i < n; i++) {
        const int v = order[i];
        const float y = P.bodies[v].core.position.y;
        int j = i - 1;
        while (j >= 0 && P.bodies[order[j]].core.position.y > y) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = v;
    }
    P.norder = n;
    return order;
}

static void broadphase(int stage, sub_shape walls[4]) {
    P.npairs = 0;
    P.ncounts = 0;

    const int* order = shock_order();
    if (!order) return;

    for (int oi = 0; oi < P.norder; oi++) {
        const int i = order[oi];
        body_t* a = &P.bodies[i];
        stage_state* sa = &a->stage[stage];

        for (int w = 0; w < 4; w++) {
            int counts = -1;
            for (int si = 0; si < a->nshapes; si++) {
                sub_shape* sha = &a->staged[stage * a->nshapes + si];
                if (!(sha->member_of & walls[w].repels)
                    && !(sha->member_of & walls[w].rotates)) {
                    continue;
                }
                if (counts < 0 && (counts = alloc_counts()) < 0) break;
                wall_contacts(a, sa, sha, &walls[w], counts);
            }
        }

        for (int ok = oi + 1; ok < P.norder; ok++) {
            body_t* b = &P.bodies[order[ok]];
            if (!bodies_can_collide(a, b)) continue;
            stage_state* sb = &b->stage[stage];
            const int counts_ab = alloc_counts();
            const int counts_ba = alloc_counts();
            if (counts_ab < 0 || counts_ba < 0) continue;
            for (int si = 0; si < a->nshapes; si++) {
                for (int sj = 0; sj < b->nshapes; sj++) {
                    narrowphase_pair(a, sa, &a->staged[stage * a->nshapes + si],
                                     b, sb, &b->staged[stage * b->nshapes + sj],
                                     counts_ab, counts_ba);
                }
            }
        }
    }
}

// RK4 combination

// sub_531810 @0x531810: zero values strictly inside the deadzone.
static float deadzone(float x) {
    return (PHYS_DEADZONE <= (double)x || x <= -PHYS_DEADZONE) ? x : 0.0f;
}

// sub_531580 @0x531580: combine RK4 derivatives and integrate once.
static void combine_and_integrate(body_t* b) {
    const stage_state* k0 = &b->stage[0];
    const stage_state* k1 = &b->stage[1];
    const stage_state* k2 = &b->stage[2];
    const stage_state* k3 = &b->stage[3];
    const float sixth = 0.1666666716337204f;

    b->k_velocity = v2_scale(
        v2_add(v2_add(k0->velocity,
                      v2_scale(v2_add(k1->velocity, k2->velocity), 2.0f)),
               k3->velocity), sixth);
    b->k_force = v2_scale(
        v2_add(v2_add(k0->force, v2_scale(v2_add(k1->force, k2->force), 2.0f)),
               k3->force), sixth);
    b->k_angular_velocity =
        f32((((double)k1->angular_velocity + k2->angular_velocity) * 2.0
             + k0->angular_velocity + k3->angular_velocity) * sixth);
    b->k_torque = f32((((double)k1->torque + k2->torque) * 2.0
                       + k0->torque + k3->torque) * sixth);

    // Only vertical components use the deadzone.
    b->k_force.y = deadzone(b->k_force.y);
    b->k_velocity.y = deadzone(b->k_velocity.y);

    b->core.position = v2_add(b->core.position,
                              v2_scale(b->k_velocity, PHYS_DT));
    b->core.momentum = v2_add(b->core.momentum, v2_scale(b->k_force, PHYS_DT));
    b->core.orientation =
        f32((double)PHYS_DT * b->k_angular_velocity + b->core.orientation);
    b->core.angular_momentum =
        f32((double)PHYS_DT * b->k_torque + b->core.angular_momentum);
    core_derive(&b->core);
}

// Diagnostics

// Dump each phase's derivatives to locate the first divergence.
static void debug_phase(const char* what) {
    if (!phys_debug_step()) return;
    for (int i = 0; i < P.nbodies; i++) {
        const stage_state* s = &P.bodies[i].stage[P.stage];
        fprintf(stderr, "  [s%d.%d] %-8s b%d F=(%+.6f,%+.6f) T=%+.6f "
                "omega=%+.6f v=(%+.6f,%+.6f) sup=%d\n",
                P.step, P.stage, what, i, s->force.x, s->force.y, s->torque,
                s->core.angular_velocity_cache, s->core.velocity_cache.x,
                s->core.velocity_cache.y, s->core.supported);
    }
}

static const char* wall_name(v2 n) {
    if (n.y == 0.0f) return n.x > 0.0f ? "left" : "right";
    if (n.x == 0.0f) return n.y > 0.0f ? "floor" : "ceiling";
    return "wall";
}

// Canonical supported is omitted because support is rebuilt per stage.
static void dump_body_state(FILE* f, const char* tag, int i,
                            const core_state* c) {
    fprintf(f, "%s %d pos=(%+.9e,%+.9e) th=%+.9e p=(%+.9e,%+.9e) L=%+.9e\n",
            tag, i, c->position.x, c->position.y, c->orientation,
            c->momentum.x, c->momentum.y, c->angular_momentum);
}

void phys_debug_dump(FILE* f) {
    if (!f) return;
    fprintf(f, "world w=%+.9e h=%+.9e step=%d bodies=%d joints=%d "
               "rotjoints=%d\n",
            P.ww, P.wh, P.step, P.nbodies, P.njoints, P.nrotjoints);

    for (int i = 0; i < P.nbodies; i++) {
        const body_t* b = &P.bodies[i];
        if (b->dead) {
            fprintf(f, "body %d dead\n", i);
            continue;
        }
        const phys_params* p = &b->prm;
        fprintf(f, "body %d anchored=%d fixed_rotate=%d toy=%d "
                   "local_group=0x%08x\n",
                i, p->anchored, p->fixed_rotate, p->toy_id,
                (unsigned)p->local_group);
        fprintf(f, "  mass=%+.9e inertia=%+.9e inv_inertia=%+.9e "
                   "gravity=%+.9e\n",
                b->core.mass, p->inertia, b->core.inv_inertia, p->gravity);
        fprintf(f, "  air=(%+.9e,%+.9e) motor=(%+.9e,%+.9e,%+.9e) "
                   "mouse=(%+.9e,%+.9e)\n",
                p->air_linear, p->air_angular, p->motor_force[0],
                p->motor_force[1], p->motor_torque, p->mouse_stiffness,
                p->mouse_dampener);
        // Raw material; combined stores the first four doubled.
        fprintf(f, "  material=(%+.9e,%+.9e,%+.9e,%+.9e,%+.9e)\n",
                p->material[0], p->material[1], p->material[2], p->material[3],
                p->material[4]);
        dump_body_state(f, "  state", i, &b->core);
        if (b->grabbed) {
            fprintf(f, "  grab anchor=(%+.9e,%+.9e) target=(%+.9e,%+.9e) "
                       "move=%d rotate=%d\n",
                    b->grab_anchor.x, b->grab_anchor.y, b->grab_target.x,
                    b->grab_target.y, b->grab_move, b->grab_rotate);
        }
        for (int s = 0; s < b->nshapes; s++) {
            const sub_shape* sh = &b->local[s];
            fprintf(f, "  shape %d n=%d member=0x%llx repels=0x%llx "
                       "rotates=0x%llx\n",
                    s, sh->count, (unsigned long long)sh->member_of,
                    (unsigned long long)sh->repels,
                    (unsigned long long)sh->rotates);
            for (int v = 0; v < sh->count; v++) {
                fprintf(f, "    v %d %+.9e %+.9e r=%+.9e\n",
                        v, sh->vertex[v].x, sh->vertex[v].y, sh->radius[v]);
            }
        }
    }

    for (int i = 0; i < P.njoints; i++) {
        const joint_t* j = &P.joints[i];
        fprintf(f, "joint %d active=%d b1=%d b2=%d a1=(%+.9e,%+.9e) "
                   "a2=(%+.9e,%+.9e) k=%+.9e c=%+.9e rest=%+.9e "
                   "move=(%d,%d) rot=(%d,%d) axis=(%+.9e,%+.9e) axis_on=%d\n",
                i, j->active, j->b1, j->b2, j->a1.x, j->a1.y, j->a2.x, j->a2.y,
                j->k, j->c, j->rest, j->move1, j->move2, j->rotate1, j->rotate2,
                j->axis.x, j->axis.y, j->axis_on);
    }
    for (int i = 0; i < P.nrotjoints; i++) {
        const rotjoint_t* r = &P.rotjoints[i];
        fprintf(f, "rotjoint %d active=%d b1=%d b2=%d o1=%+.9e o2=%+.9e "
                   "k=%+.9e c=%+.9e rest=%+.9e ratio=%+.9e rot=(%d,%d)\n",
                i, r->active, r->b1, r->b2, r->o1, r->o2, r->k, r->c, r->rest,
                r->ratio, r->rotate1, r->rotate2);
    }
    fflush(f);
}

void phys_debug_capture_begin(FILE* out) { P.capture = out; }
bool phys_debug_capture_armed(void) { return P.capture != NULL; }
void phys_debug_capture_end(void) { P.capture = NULL; P.capturing = false; }

// Capture after solving, when count totals and accumulated responses are final.
static void capture_stage(int stage) {
    FILE* f = P.capture;
    fprintf(f, "stage %d pairs=%d counts=%d\n", stage, P.npairs, P.ncounts);
    for (int i = 0; i < P.npairs; i++) {
        const contact_pair* p = &P.pairs[i];
        fprintf(f, "  contact %d self=%d/%d other=", i, debug_body_of(p->body),
                debug_shape_of(p->body, p->shape));
        if (p->other) {
            fprintf(f, "%d/%d", debug_body_of(p->other),
                    debug_shape_of(p->other, p->other_shape));
        } else {
            fprintf(f, "%s", wall_name(p->normal));
        }
        fprintf(f, " n=(%+.9e,%+.9e) pt=(%+.9e,%+.9e) depth=%+.9e\n",
                p->normal.x, p->normal.y, p->point.x, p->point.y,
                p->penetration);
        fprintf(f, "    slot=%d counts=(%d,%d) apply=(%d,%d) "
                   "rel=(%+.9e,%+.9e) accum=(%+.9e,%+.9e) peak=%+.9e\n",
                p->counts, P.counts[p->counts][0], P.counts[p->counts][1],
                p->apply_linear, p->apply_angular, p->rel_vel.x, p->rel_vel.y,
                p->accum.x, p->accum.y, p->friction_peak);
    }
    for (int i = 0; i < P.nbodies; i++) {
        if (P.bodies[i].dead) continue;
        const stage_state* s = &P.bodies[i].stage[stage];
        fprintf(f, "  deriv %d F=(%+.9e,%+.9e) T=%+.9e v=(%+.9e,%+.9e) "
                   "omega=%+.9e sup=%d\n",
                i, s->force.x, s->force.y, s->torque, s->core.velocity_cache.x,
                s->core.velocity_cache.y, s->core.angular_velocity_cache,
                s->core.supported);
    }
}

// Step execution

// Physics_stepOnce @0x530AF0: four RK4 stages, then one combined integration.
static void step_once(void) {
    sub_shape walls[4];
    build_walls(walls);

    P.capturing = P.capture != NULL;
    if (P.capturing) {
        fprintf(P.capture, "\ncapture step=%d\n", P.step);
        for (int i = 0; i < P.nbodies; i++) {
            if (!P.bodies[i].dead) {
                dump_body_state(P.capture, "pre", i, &P.bodies[i].core);
            }
        }
    }

    for (int stage = 0; stage < 4; stage++) {
        P.stage = stage;
        for (int i = 0; i < P.nbodies; i++) {
            if (!P.bodies[i].dead) integrate_stage(&P.bodies[i], stage);
        }
        apply_joints(stage);
        apply_grabs(stage);
        apply_magnets(stage);
        broadphase(stage, walls);
        for (int i = 0; i < P.nbodies; i++) {
            body_t* b = &P.bodies[i];
            if (b->dead) continue;
            apply_gravity(b, &b->stage[stage]);
            apply_body_forces(b, &b->stage[stage]);
        }
        debug_phase("applied");
        for (int i = 0; i < P.npairs; i++) {
            contact_prepare(&P.pairs[i], contact_ref);
        }
        debug_phase("prepare");
        for (int it = 0; it < 4; it++) {
            for (int i = 0; i < P.npairs; i++) {
                contact_solve_iter(&P.pairs[i], contact_ref);
            }
        }
        debug_phase("solve");
        for (int i = 0; i < P.npairs; i++) {
            contact_finalize(&P.pairs[i], contact_ref);
        }
        debug_phase("finalize");
        // Attachment_apply @0x532DA0 is omitted: its container is never
        // populated in 1.6.0.8.
        if (P.capturing) capture_stage(stage);
    }

    for (int i = 0; i < P.nbodies; i++) {
        body_t* b = &P.bodies[i];
        if (b->dead) continue;
        combine_and_integrate(b);
    }

    if (P.capturing) {
        for (int i = 0; i < P.nbodies; i++) {
            if (!P.bodies[i].dead) {
                dump_body_state(P.capture, "post", i, &P.bodies[i].core);
            }
        }
        fflush(P.capture);
        P.capture = NULL;      // capture one step
        P.capturing = false;
    }
}

void phys_steps(int n) {
    for (int i = 0; i < n; i++) {
        P.step++;
        step_once();
    }
}

// Object management

void phys_set_world(float width, float height) {
    P.ww = width;
    P.wh = height;
}

static void body_free_storage(body_t* b) {
    free(b->local);
    free(b->staged);
    b->local = NULL;
    b->staged = NULL;
    b->nshapes = 0;
}

// fixedMove is implemented by response masks, joint gates and skipped gravity,
// not inv_mass = 0. Zeroing inv_mass breaks anchored-body coupling.
static void body_apply_params(body_t* b) {
    const phys_params* p = &b->prm;
    // Self/self material combine; static friction is not doubled.
    for (int m = 0; m < 4; m++) b->combined[m] = f32(p->material[m] * 2.0);
    b->combined[4] = p->material[4];
    b->core.mass = p->mass > 0.0f ? p->mass : 1.0f;
    b->core.inv_mass = f32(1.0 / b->core.mass);
    b->core.inv_inertia =
        (!p->fixed_rotate && p->inertia > 0.0f) ? f32(1.0 / p->inertia) : 0.0f;
    b->core.anchored = p->anchored;      // fixedMove
    b->core.anchored2 = p->fixed_rotate; // fixedRotate
}

int phys_body_add(float x, float y, float theta, const phys_params* p,
                  const phys_point* pts, int npts,
                  const phys_shape* shapes, int nshapes,
                  float fallback_radius) {
    // Empty collision geometry falls back to one wall-colliding circle.
    phys_shape fallback_shape = {
        0, 1,
        PHYS_GROUP_LEFT_WALL_REPEL | PHYS_GROUP_LEFT_WALL_ROTATE
            | PHYS_GROUP_RIGHT_WALL_REPEL | PHYS_GROUP_RIGHT_WALL_ROTATE
            | PHYS_GROUP_FLOOR_REPEL | PHYS_GROUP_FLOOR_ROTATE
            | PHYS_GROUP_CEILING_REPEL | PHYS_GROUP_CEILING_ROTATE
    };
    phys_point fallback_point = { 0.0f, 0.0f, fallback_radius };
    if (npts <= 0 || nshapes <= 0) {
        pts = &fallback_point;
        npts = 1;
        shapes = &fallback_shape;
        nshapes = 1;
    }

    sub_shape* local = calloc((size_t)nshapes, sizeof(*local));
    sub_shape* staged = calloc((size_t)nshapes * 4, sizeof(*staged));
    if (!local || !staged) {
        free(local);
        free(staged);
        return -1;
    }
    for (int i = 0; i < nshapes; i++) {
        sub_shape* s = &local[i];
        s->count = shapes[i].npoints < PHYS_MAX_SHAPE_VERTS
                       ? shapes[i].npoints : PHYS_MAX_SHAPE_VERTS;
        for (int v = 0; v < s->count; v++) {
            const phys_point* pt = &pts[shapes[i].first_point + v];
            s->vertex[v] = (v2){ pt->x, pt->y };
            s->radius[v] = pt->r;
        }
        subshape_build_edges(s);
        subshape_aabb(s);
        shape_set_masks(s, shapes[i].groups);
    }

    int slot = -1;
    for (int i = 0; i < P.nbodies; i++) {
        if (P.bodies[i].dead) { slot = i; break; }
    }
    if (slot < 0) {
        if (!reserve_bodies(P.nbodies + 1)) {
            free(local);
            free(staged);
            return -1;
        }
        slot = P.nbodies++;
    }

    body_t* b = &P.bodies[slot];
    memset(b, 0, sizeof(*b));
    b->local = local;
    b->staged = staged;
    b->nshapes = nshapes;
    b->prm = *p;
    body_apply_params(b);
    b->core.position = (v2){ x, y };
    b->core.orientation = theta;
    core_derive(&b->core);
    for (int s = 0; s < 4; s++) {
        b->stage[s].core = b->core;
        for (int i = 0; i < nshapes; i++) {
            subshape_transform(&b->staged[s * nshapes + i], &b->local[i],
                               b->core.transform);
        }
    }
    return slot;
}

void phys_body_get_params(int body, phys_params* out) {
    if (body < 0 || body >= P.nbodies) return;
    *out = P.bodies[body].prm;
}

// Live edits take effect on the next step as creation-time parameters would.
void phys_body_set_params(int body, const phys_params* p) {
    if (body < 0 || body >= P.nbodies) return;
    body_t* b = &P.bodies[body];
    b->prm = *p;
    body_apply_params(b);
    core_derive(&b->core);
    for (int s = 0; s < 4; s++) b->stage[s].core = b->core;
}

void phys_joint_get_params(int joint, phys_joint_params* out) {
    if (joint < 0 || joint >= P.njoints) return;
    const joint_t* j = &P.joints[joint];
    *out = (phys_joint_params){
        .stiffness = j->k, .dampener = j->c, .rest_length = j->rest,
        .move1 = j->move1, .move2 = j->move2,
        .rotate1 = j->rotate1, .rotate2 = j->rotate2,
        .axis = { j->axis.x, j->axis.y }, .axis_on = j->axis_on,
        .point1 = { j->a1.x, j->a1.y }, .point2 = { j->a2.x, j->a2.y },
    };
}

void phys_joint_set_params(int joint, const phys_joint_params* p) {
    if (joint < 0 || joint >= P.njoints) return;
    joint_t* j = &P.joints[joint];
    j->k = p->stiffness;
    j->c = p->dampener;
    j->rest = p->rest_length;
    j->move1 = p->move1;
    j->move2 = p->move2;
    j->rotate1 = p->rotate1;
    j->rotate2 = p->rotate2;
    j->axis = (v2){ p->axis[0], p->axis[1] };
    j->axis_on = p->axis_on;
    j->a1 = (v2){ p->point1[0], p->point1[1] };
    j->a2 = (v2){ p->point2[0], p->point2[1] };
}

void phys_rotjoint_get_params(int joint, phys_rotjoint_params* out) {
    if (joint < 0 || joint >= P.nrotjoints) return;
    const rotjoint_t* r = &P.rotjoints[joint];
    *out = (phys_rotjoint_params){
        .orientation1 = r->o1, .orientation2 = r->o2,
        .stiffness = r->k, .dampener = r->c, .rest_length = r->rest,
        .ratio = r->ratio, .rotate1 = r->rotate1, .rotate2 = r->rotate2,
    };
}

void phys_rotjoint_set_params(int joint, const phys_rotjoint_params* p) {
    if (joint < 0 || joint >= P.nrotjoints) return;
    rotjoint_t* r = &P.rotjoints[joint];
    r->o1 = p->orientation1;
    r->o2 = p->orientation2;
    r->k = p->stiffness;
    r->c = p->dampener;
    r->rest = p->rest_length;
    r->ratio = p->ratio;
    r->rotate1 = p->rotate1;
    r->rotate2 = p->rotate2;
}

void phys_body_free(int body) {
    body_t* b = &P.bodies[body];
    b->dead = true;
    b->grabbed = false;
    b->core.anchored = true;
    b->core.momentum = (v2){ 0.0f, 0.0f };
    b->core.angular_momentum = 0.0f;
    body_free_storage(b);
    for (int i = 0; i < P.njoints; i++) {
        joint_t* j = &P.joints[i];
        if (j->active && (j->b1 == body || j->b2 == body)) j->active = false;
    }
    for (int i = 0; i < P.nrotjoints; i++) {
        rotjoint_t* rj = &P.rotjoints[i];
        if (rj->active && (rj->b1 == body || rj->b2 == body)) rj->active = false;
    }
    for (int i = 0; i < P.nmagnets; i++) {
        if (P.magnets[i].active && P.magnets[i].body == body) {
            P.magnets[i].active = false;
        }
    }
    // Keep first-seen group order stable after the last member is removed.
}

int phys_active_body_count(void) {
    int count = 0;
    for (int i = 0; i < P.nbodies; i++) {
        if (!P.bodies[i].dead) count++;
    }
    return count;
}

void phys_body_set_shock_order(int body, int order) {
    if (body < 0 || body >= P.nbodies) return;
    P.bodies[body].prm.shock_order = order;
}

int phys_body_shock_order(int body) {
    if (body < 0 || body >= P.nbodies) return 0;
    return P.bodies[body].prm.shock_order;
}

int phys_magnet_add(int body, bool producer, const phys_magnet* m) {
    if (body < 0 || body >= P.nbodies || !m) return -1;
    int slot = -1;
    for (int i = 0; i < P.nmagnets; i++) {
        if (!P.magnets[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        if (!reserve_magnets(P.nmagnets + 1)) return -1;
        slot = P.nmagnets++;
    }
    bool known = false;
    for (int i = 0; i < P.ngroups; i++) {
        if (P.groups[i] == m->group) { known = true; break; }
    }
    if (!known) {
        if (!reserve_groups(P.ngroups + 1)) return -1;
        P.groups[P.ngroups++] = m->group;
    }
    P.magnets[slot] = (magnet_t){
        .body = body,
        .group = m->group,
        .attach = { m->attach[0], m->attach[1] },
        .producer = producer,
        .bidirectional = m->bidirectional,
        .inverted = m->inverted,
        .spring_response = m->spring_response,
        .k = m->stiffness,
        .c = m->dampener,
        .radius = m->radius,
        .active = true,
    };
    return slot;
}

int phys_joint_add(int body1, float a1x, float a1y,
                   int body2, float a2x, float a2y,
                   float rest_length, float stiffness, float dampener) {
    if (body1 < 0 || body2 < 0) return -1;
    int slot = -1;
    for (int i = 0; i < P.njoints; i++) {
        if (!P.joints[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        if (!reserve_joints(P.njoints + 1)) return -1;
        slot = P.njoints++;
    }
    // Defs omit move/rotate/axis, so derive the gates from fixedMove and
    // fixedRotate. Joint_apply has no anchored check; contacts are separate.
    P.joints[slot] = (joint_t){
        .b1 = body1, .b2 = body2,
        .a1 = { a1x, a1y }, .a2 = { a2x, a2y },
        .k = stiffness, .c = dampener, .rest = rest_length,
        .move1 = !P.bodies[body1].prm.anchored,
        .move2 = !P.bodies[body2].prm.anchored,
        .rotate1 = !P.bodies[body1].prm.fixed_rotate,
        .rotate2 = !P.bodies[body2].prm.fixed_rotate,
        .axis_on = false, .active = true,
    };
    return slot;
}

int phys_rotjoint_add(int body1, float o1, int body2, float o2,
                      float rest, float stiffness, float dampener) {
    if (body1 < 0 || body2 < 0) return -1;
    int slot = -1;
    for (int i = 0; i < P.nrotjoints; i++) {
        if (!P.rotjoints[i].active) { slot = i; break; }
    }
    if (slot < 0) {
        if (!reserve_rotjoints(P.nrotjoints + 1)) return -1;
        slot = P.nrotjoints++;
    }
    // ratio is Ruby-only; 1.0 is the solver identity.
    P.rotjoints[slot] = (rotjoint_t){
        .b1 = body1, .b2 = body2, .o1 = o1, .o2 = o2,
        .k = stiffness, .c = dampener, .rest = rest, .ratio = 1.0f,
        .rotate1 = true, .rotate2 = true, .active = true,
    };
    return slot;
}

// Body state access

void phys_body_pos(int body, float* x, float* y) {
    *x = P.bodies[body].core.position.x;
    *y = P.bodies[body].core.position.y;
}

void phys_body_transform(int body, float* forward, float* inverse) {
    if (body < 0 || body >= P.nbodies) return;
    const core_state* c = &P.bodies[body].core;
    if (forward) memcpy(forward, c->transform, sizeof(mat3));
    if (inverse) memcpy(inverse, c->inv_transform, sizeof(mat3));
}

float phys_body_orientation(int body) {
    return P.bodies[body].core.orientation;
}

void phys_body_set_pose(int body, float x, float y, float theta) {
    core_state* c = &P.bodies[body].core;
    c->position = (v2){ x, y };
    c->orientation = theta;
    core_derive(c);
}

void phys_body_momentum(int body, float* mx, float* my, float* L) {
    const core_state* c = &P.bodies[body].core;
    *mx = c->momentum.x;
    *my = c->momentum.y;
    *L = c->angular_momentum;
}

void phys_body_set_momentum(int body, float mx, float my, float L) {
    core_state* c = &P.bodies[body].core;
    c->momentum = (v2){ mx, my };
    c->angular_momentum = L;
    core_derive(c);
}

// Grabbing

// Rotating grabs store a body-local point; move-only grabs store an
// unrotated world offset. The initial target equals the clicked point.
void phys_grab(int body, float wx, float wy, bool move, bool rotate) {
    body_t* b = &P.bodies[body];
    const v2 world = { wx, wy };
    b->grabbed = true;
    b->grab_move = move;
    b->grab_rotate = rotate;
    b->grab_anchor = rotate ? mat3_xform_point(world, b->core.inv_transform)
                            : v2_sub(world, b->core.position);
    b->grab_target = world;
}

void phys_grab_move(int body, float x, float y) {
    body_t* b = &P.bodies[body];
    b->grab_target = (v2){ x, y };

    // Editor handles can move or rotate otherwise fixed components
    // kinematically while free bodies continue through the spring.
    core_state* c = &b->core;
    if (b->grab_rotate && (c->anchored || b->prm.fixed_rotate)) {
        const float r2 = f32((double)b->grab_anchor.x * b->grab_anchor.x
                             + (double)b->grab_anchor.y * b->grab_anchor.y);
        const float dx = f32(x - c->position.x), dy = f32(y - c->position.y);
        if (r2 > 1e-8f && (double)dx * dx + (double)dy * dy > 1e-8) {
            float desired = f32(atan2(dy, dx)
                                - atan2(b->grab_anchor.y, b->grab_anchor.x));
            while (desired - c->orientation > (float)M_PI) {
                desired -= 2.0f * (float)M_PI;
            }
            while (desired - c->orientation < -(float)M_PI) {
                desired += 2.0f * (float)M_PI;
            }
            c->orientation = desired;
            c->angular_momentum = 0.0f;
        }
    }
    if (b->grab_move && c->anchored) {
        const v2 offset = mat3_rotate(b->grab_anchor, c->transform);
        c->position = (v2){ f32(x - offset.x), f32(y - offset.y) };
        c->momentum = (v2){ 0.0f, 0.0f };
    }
    core_derive(c);
}

void phys_release(int body) {
    // Keep the momentum accumulated by the spring.
    P.bodies[body].grabbed = false;
}

// Return either anchor convention as the original world-space point.
bool phys_grab_state(int body, float* anchor_x, float* anchor_y,
                     float* target_x, float* target_y,
                     bool* move, bool* rotate) {
    if (body < 0 || body >= P.nbodies) return false;
    const body_t* b = &P.bodies[body];
    if (b->dead || !b->grabbed) return false;
    const bool rot = b->grab_rotate && !b->prm.fixed_rotate;
    const v2 world = rot ? mat3_xform_point(b->grab_anchor, b->core.transform)
                         : v2_add(b->grab_anchor, b->core.position);
    *anchor_x = world.x;
    *anchor_y = world.y;
    *target_x = b->grab_target.x;
    *target_y = b->grab_target.y;
    *move = b->grab_move;
    *rotate = b->grab_rotate;
    return true;
}

// Geometry queries
// Geometry only; response masks are ignored.

static int wall_from_groups(uint32_t groups) {
    if (groups & PHYS_GROUP_LEFT_WALL) return PHYS_WALL_LEFT;
    if (groups & PHYS_GROUP_RIGHT_WALL) return PHYS_WALL_RIGHT;
    if (groups & PHYS_GROUP_FLOOR) return PHYS_WALL_FLOOR;
    if (groups & PHYS_GROUP_CEILING) return PHYS_WALL_CEILING;
    return -1;
}

static bool shape_overlaps_wall(const sub_shape* s, const sub_shape* wall) {
    for (int i = 0; i < s->count; i++) {
        const float depth = f32((double)wall->edge[0].offset
                                - v2_dot(s->vertex[i], wall->edge[0].normal)
                                + s->radius[i]);
        if (depth > 0.0f) return true;
    }
    return false;
}

bool phys_shapes_overlap(int body1, int shape1, int body2, int shape2) {
    if (body1 < 0 || body1 >= P.nbodies || body2 < 0 || body2 >= P.nbodies) {
        return false;
    }
    body_t* a = &P.bodies[body1];
    body_t* b = &P.bodies[body2];
    if (shape1 < 0 || shape1 >= a->nshapes
        || shape2 < 0 || shape2 >= b->nshapes
        || !bodies_can_collide(a, b)) {
        return false;
    }
    // Transform on demand because scripted placement can precede the next
    // stage rebuild.
    sub_shape sa, sb;
    subshape_transform(&sa, &a->local[shape1], a->core.transform);
    subshape_transform(&sb, &b->local[shape2], b->core.transform);

    const int bwall = wall_from_groups((uint32_t)sb.member_of);
    const int awall = wall_from_groups((uint32_t)sa.member_of);
    if (bwall >= 0 || awall >= 0) {
        sub_shape walls[4];
        build_walls(walls);
        return bwall >= 0 ? shape_overlaps_wall(&sa, &walls[bwall])
                          : shape_overlaps_wall(&sb, &walls[awall]);
    }

    if (!(sb.max_x >= sa.min_x && sb.max_y >= sa.min_y
          && sa.max_x >= sb.min_x && sa.max_y >= sb.min_y)) {
        return false;
    }
    sat_result r;
    return sat_distances(&sa, &sb, &r) && sat_distances(&sb, &sa, &r);
}

int phys_shape_vertices(int body, int shape, float* xyr, int max_verts) {
    if (body < 0 || body >= P.nbodies) return 0;
    const body_t* b = &P.bodies[body];
    if (b->dead || shape < 0 || shape >= b->nshapes) return 0;
    const sub_shape* s = &b->local[shape];
    if (xyr) {
        for (int i = 0; i < s->count && i < max_verts; i++) {
            xyr[3 * i + 0] = s->vertex[i].x;
            xyr[3 * i + 1] = s->vertex[i].y;
            xyr[3 * i + 2] = s->radius[i];
        }
    }
    return s->count;
}
