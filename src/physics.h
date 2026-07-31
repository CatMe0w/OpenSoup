#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Souptoys-compatible 2D physics core.
// Units are meters (100 logical pixels); origin is bottom-left, y points up.
// Simulation advances in fixed 0.01 s RK4 steps.

#define PHYS_DT 0.01f
#define PHYS_GRAVITY -18.0f
#define PHYS_PX_PER_UNIT 100.0f

// Resting deadzone applied to combined vertical velocity and force.
#define PHYS_DEADZONE 0.001f

// Separating-axis scratch limit.
#define PHYS_MAX_SHAPE_VERTS 16

// Engine wall order: left, right, ceiling, floor.
#define PHYS_WALL_LEFT 0
#define PHYS_WALL_RIGHT 1
#define PHYS_WALL_CEILING 2
#define PHYS_WALL_FLOOR 3

typedef struct {
    float x, y, r; // body-local meters; r=0 for polygon vertices
} phys_point;

// Collision groups used by shipped definitions.
#define PHYS_GROUP_BOUNCERS          (UINT32_C(1) << 0)
#define PHYS_GROUP_SPINNERS          (UINT32_C(1) << 1)
#define PHYS_GROUP_BOUNCER_REPELLERS (UINT32_C(1) << 2)
#define PHYS_GROUP_SPINNER_ROTATORS  (UINT32_C(1) << 3)
#define PHYS_GROUP_EXCLUSION         (UINT32_C(1) << 4)
#define PHYS_GROUP_EXCLUSION_REPELLERS (UINT32_C(1) << 5)
#define PHYS_GROUP_LEFT_WALL         (UINT32_C(1) << 6)
#define PHYS_GROUP_RIGHT_WALL        (UINT32_C(1) << 7)
#define PHYS_GROUP_FLOOR             (UINT32_C(1) << 8)
#define PHYS_GROUP_CEILING           (UINT32_C(1) << 9)
#define PHYS_GROUP_LEFT_WALL_REPEL   (UINT32_C(1) << 10)
#define PHYS_GROUP_LEFT_WALL_ROTATE  (UINT32_C(1) << 11)
#define PHYS_GROUP_RIGHT_WALL_REPEL  (UINT32_C(1) << 12)
#define PHYS_GROUP_RIGHT_WALL_ROTATE (UINT32_C(1) << 13)
#define PHYS_GROUP_FLOOR_REPEL       (UINT32_C(1) << 14)
#define PHYS_GROUP_FLOOR_ROTATE      (UINT32_C(1) << 15)
#define PHYS_GROUP_CEILING_REPEL     (UINT32_C(1) << 16)
#define PHYS_GROUP_CEILING_ROTATE    (UINT32_C(1) << 17)
#define PHYS_GROUP_SNOWBALLS         (UINT32_C(1) << 18)

typedef struct {
    int first_point;
    int npoints;
    uint32_t groups;
} phys_shape;

typedef struct {
    float mass;
    float inertia;          // inertiaTensor in world units
    float gravity;          // world gravity or override
    float mouse_stiffness;  // override or engine default
    float mouse_dampener;
    float air_linear;       // F = -c * v
    float air_angular;      // torque = -c * omega
    // Body-local drag point; an offset adds torque.
    float centre_of_resistance[2];
    bool anchored;          // fixedMove
    bool fixed_rotate;      // fixedRotate
    float motor_force[2];   // body-local linearMotor force
    float motor_torque;     // rotationalMotor torque
    // velocity response, stiffness, dampener, kinetic/static friction
    float material[5];
    int toy_id;              // collision filtering is toy-local
    uint32_t local_group;    // same-toy collisions require equal hashes
    // Negative values are excluded from shock-ordered body traversal.
    int shock_order;
} phys_params;

void phys_set_world(float width, float height); // wall extents in meters

// Copies points and shapes while preserving shape indices. Empty geometry
// uses fallback_radius.
int phys_body_add(float x, float y, float theta, const phys_params* p,
                  const phys_point* pts, int npts,
                  const phys_shape* shapes, int nshapes,
                  float fallback_radius);
void phys_steps(int n);

// Live bodies only; reusable retired slots are excluded.
int phys_active_body_count(void);

// Final-state geometry only; response groups are ignored and walls are
// treated as infinite planes.
bool phys_shapes_overlap(int body1, int shape1, int body2, int shape2);

// Body-local swept vertices; xyr uses three floats per vertex or may be NULL.
int phys_shape_vertices(int body, int shape, float* xyr, int max_verts);

// Live body parameter access.
void phys_body_get_params(int body, phys_params* out);
void phys_body_set_params(int body, const phys_params* p);

// Live joint parameter access.
typedef struct {
    float stiffness, dampener, rest_length;
    bool move1, move2, rotate1, rotate2;
    float axis[2];           // body1-local
    bool axis_on;
    float point1[2], point2[2];  // body-local anchors
} phys_joint_params;

typedef struct {
    float orientation1, orientation2;
    float stiffness, dampener, rest_length, ratio;
    bool rotate1, rotate2;
} phys_rotjoint_params;

void phys_joint_get_params(int joint, phys_joint_params* out);
void phys_joint_set_params(int joint, const phys_joint_params* p);
void phys_rotjoint_get_params(int joint, phys_rotjoint_params* out);
void phys_rotjoint_set_params(int joint, const phys_rotjoint_params* p);

// Row-major body transforms; either output may be NULL.
void phys_body_transform(int body, float* forward, float* inverse);

void phys_body_pos(int body, float* x, float* y);
float phys_body_orientation(int body); // radians, CCW positive, unbounded

// Teleport without changing momentum.
void phys_body_set_pose(int body, float x, float y, float theta);

void phys_body_momentum(int body, float* mx, float* my, float* L);
void phys_body_set_momentum(int body, float mx, float my, float L);

// Retire a body and reuse its slot on the next phys_body_add.
void phys_body_free(int body);

// Two-body spring with body-local anchors. Force is applied at each anchor,
// including torque from the anchor offset.
int phys_joint_add(int body1, float a1x, float a1y,
                   int body2, float a2x, float a2y,
                   float rest_length, float stiffness, float dampener);

// Pair each producer with every consumer in its group while within radius;
// producer parameters define the spring.
typedef struct {
    uint32_t group;
    float attach[2];        // limb-local
    bool bidirectional;     // producer receives the reaction
    bool inverted;          // negate the force
    bool spring_response;   // raw-separation branch; see physics.c
    float stiffness, dampener, radius;
} phys_magnet;

int phys_magnet_add(int body, bool producer, const phys_magnet* m);

// Set after body creation.
void phys_body_set_shock_order(int body, int order);
int phys_body_shock_order(int body);

// Torque spring on relative orientation.
int phys_rotjoint_add(int body1, float o1, int body2, float o2,
                      float rest, float stiffness, float dampener);

// Grabs spring the clicked world point toward a world target. move and rotate
// gate force and torque and may drive otherwise fixed components while held.
#define PHYS_MOUSE_STIFFNESS 300.0f
#define PHYS_MOUSE_DAMPENER 10.0f

// ax, ay is the clicked world point.
void phys_grab(int body, float ax, float ay, bool move, bool rotate);
void phys_grab_move(int body, float x, float y); // update world target
void phys_release(int body);

// Return the live grab with its anchor mapped back to world space.
bool phys_grab_state(int body, float* anchor_x, float* anchor_y,
                     float* target_x, float* target_y,
                     bool* move, bool* rotate);

// Dump live bodies, geometry, joints and any active grab.
void phys_debug_dump(FILE* out);

// Capture the next step: pre-state, per-stage contacts/derivatives, post-state.
// The arm clears after one step; output uses %+.9e.
void phys_debug_capture_begin(FILE* out);
bool phys_debug_capture_armed(void); // waiting for a step
void phys_debug_capture_end(void);   // disarm; caller owns FILE*
