#pragma once
#include <stdbool.h>
#include <stdint.h>

// Physics core v1, modeled on the reverse-engineered Toybox engine
// fixed dt=0.01, gravity=-18, momentum-space state, RK4 position integration,
// impulse contacts against world walls and other toys, spring joints
// (sub_532800 form).
// World space: meters (1 unit = 100 device pixels), origin bottom-left, y-up.
//
// Bodies retain the original shape boundaries and memberOf groups. Narrowphase
// is the original vertex-vs-edge model over the .toy swept-circle vertices.

#define PHYS_DT 0.01f
#define PHYS_GRAVITY -18.0f
#define PHYS_PX_PER_UNIT 100.0f

// wall index bits for the per-shape repel/rotate masks
// (memberOf groups left_wall_repel, floor_rotate, ... from the toy defs)
#define PHYS_WALL_LEFT 0
#define PHYS_WALL_RIGHT 1
#define PHYS_WALL_FLOOR 2
#define PHYS_WALL_CEILING 3

typedef struct {
    float x, y, r; // body-local, meters; polygon vertex = r 0, circle = 1 point
} phys_point;

// The shipped definitions use only these 19 collision groups. Keeping them as
// bits makes the original memberOf pair tests explicit and cheap.
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
    float inertia;          // inertiaTensor, world units (def value * scale^2)
    float gravity;          // world gravity or the limb's gravityOverride
    float mouse_stiffness;  // per-limb override or engine default
    float mouse_dampener;
    float air_linear;       // airResistanceLinear: F = -c*v
    float air_angular;      // airResistanceAngular: tau = -c*omega
    bool anchored;          // fixedMove: no ordinary linear physics
    bool fixed_rotate;      // fixedRotate: no ordinary angular physics
    float motor_force[2];   // constant linearMotor force, body-local
    float motor_torque;     // constant rotationalMotor torque
    // CMaterial: velocityResponse, stiffness, dampener, kineticFriction,
    // staticFriction. Contact response uses the body's OWN material only
    // (original combines self+self, then scales by the mass ratio).
    float material[5];
    int toy_id;              // collision-group filtering is toy-local
    uint32_t local_group;    // stable hash of localCollisionGroup
} phys_params;

void phys_set_world(float width, float height); // wall extents, meters

// Shapes and points are copied. Shape indices remain aligned with the source
// td_limb so Ruby Shape nodes can query overlap by (body, shape).
int phys_body_add(float x, float y, float theta, const phys_params* p,
                  const phys_point* pts, int npts,
                  const phys_shape* shapes, int nshapes,
                  float fallback_radius);
void phys_steps(int n);

// Final-state geometric overlap, independent of collision response groups.
// World wall shapes are interpreted as the corresponding infinite planes.
bool phys_shapes_overlap(int body1, int shape1, int body2, int shape2);

void phys_body_pos(int body, float* x, float* y);
float phys_body_orientation(int body); // radians, CCW positive, unbounded

// teleport (script-driven placement); momenta are left untouched
void phys_body_set_pose(int body, float x, float y, float theta);

void phys_body_momentum(int body, float* mx, float* my, float* L);
void phys_body_set_momentum(int body, float mx, float my, float L);

// retire a body: no motion, no contacts, joints on it are neutralized;
// the slot is reused by the next phys_body_add
void phys_body_free(int body);

// spring joint between two bodies, anchors in body-local meters:
// F = stiffness * (|d| - restLength) * dir - dampener * relative anchor
// velocity, applied at the anchors (torque from the offset).
int phys_joint_add(int body1, float a1x, float a1y,
                   int body2, float a2x, float a2y,
                   float rest_length, float stiffness, float dampener);

// rotational joint: torque spring on the relative orientation of two limbs
// (the goose rocks on one). rest/stiffness/dampener from the def's
// rotationalJoints entry.
int phys_rotjoint_add(int body1, float o1, int body2, float o2,
                      float rest, float stiffness, float dampener);

// grabbing = mouse spring at the GRAB ANCHOR (sub_532800 form, constructed
// per sub_4237B0): F = stiffness * (target - anchor_pos) - dampener *
// anchor_vel. move gates the linear component, rotate the torque component
// (goose body: move=0 rotate=1 - dragging only twists it about its hip).
// A requested component overrides fixedMove/fixedRotate while the handle is
// being dragged; the body becomes fixed again on release.
// Defaults from the Limb ctor (0x548410): stiffness=150, dampener=5,
// overridable per limb via mouse_*_override.
#define PHYS_MOUSE_STIFFNESS 150.0f
#define PHYS_MOUSE_DAMPENER 5.0f

void phys_grab(int body, float ax, float ay, bool move, bool rotate);
void phys_grab_move(int body, float x, float y); // updates spring target
void phys_release(int body);
