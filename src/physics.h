#pragma once
#include <stdbool.h>

// Physics core v1, modeled on the reverse-engineered Toybox engine
// fixed dt=0.01, gravity=-18, momentum-space state, RK4 position integration,
// impulse contacts against 4 world walls, spring joints (sub_532800 form).
// World space: meters (1 unit = 100 device pixels), origin bottom-left, y-up.
//
// v1 scope: bodies carry collision shapes as point sets (x,y,radius in body
// space - the .toy unified vertex encoding); every point collides with the
// walls, with torque from the contact-point offset. Toy-vs-toy narrowphase
// and the exact material-combine rule are still pending.

#define PHYS_DT 0.01f
#define PHYS_GRAVITY -18.0f
#define PHYS_PX_PER_UNIT 100.0f

typedef struct {
    float x, y, r; // body-local, meters; polygon vertex = r 0, circle = 1 point
} phys_point;

typedef struct {
    float mass;
    float inertia;          // inertiaTensor, world units (def value * scale^2)
    float gravity;          // world gravity or the limb's gravityOverride
    float mouse_stiffness;  // per-limb override or engine default
    float mouse_dampener;
    float air_linear;       // airResistanceLinear: F = -c*v
    float air_angular;      // airResistanceAngular: tau = -c*omega
    bool anchored;          // fixedMove: no linear motion at all
    bool fixed_rotate;      // fixedRotate: no angular motion
    float motor_force[2];   // constant linearMotor force, body-local
    float motor_torque;     // constant rotationalMotor torque
} phys_params;

void phys_set_world(float width, float height); // wall extents, meters

// pts are copied. npts == 0 falls back to a point at the origin with
// fallback_radius (sprite-derived), so shapeless limbs still collide.
int phys_body_add(float x, float y, float theta, const phys_params* p,
                  const phys_point* pts, int npts, float fallback_radius);
void phys_steps(int n);

void phys_body_pos(int body, float* x, float* y);
float phys_body_orientation(int body); // radians, CCW positive, unbounded

// spring joint between two bodies, anchors in body-local meters:
// F = stiffness * (|d| - restLength) * dir - dampener * relative anchor
// velocity, applied at the anchors (torque from the offset).
int phys_joint_add(int body1, float a1x, float a1y,
                   int body2, float a2x, float a2y,
                   float rest_length, float stiffness, float dampener);

// grabbing = mouse spring, exactly the original's constraint (sub_532800):
// F = stiffness * (target - pos) - dampener * vel, applied to momentum each step.
// Defaults from the Limb ctor (0x548410): stiffness=150, dampener=5,
// overridable per limb via mouse_*_override.
#define PHYS_MOUSE_STIFFNESS 150.0f
#define PHYS_MOUSE_DAMPENER 5.0f

void phys_grab(int body);
void phys_grab_move(int body, float x, float y); // updates spring target
void phys_release(int body);
