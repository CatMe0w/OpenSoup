#pragma once
#include <stdbool.h>

// Physics core v0, modeled on the reverse-engineered Toybox engine
// fixed dt=0.01, gravity=-18, momentum-space state, RK4 position integration,
// impulse contacts against 4 world walls.
// World space: meters (1 unit = 100 device pixels), origin bottom-left, y-up.
//
// v0 scope: circle bodies vs walls only. Toy-vs-toy contacts, joints/motors
// and the exact material-combine rule come with the toy-definition milestone.

#define PHYS_DT 0.01f
#define PHYS_GRAVITY -18.0f
#define PHYS_PX_PER_UNIT 100.0f

typedef struct {
    float mass;
    float inertia;          // inertiaTensor from the toy definition
    float gravity;          // world gravity or the limb's gravityOverride
    float mouse_stiffness;  // per-limb override or engine default
    float mouse_dampener;
    float air_linear;       // airResistanceLinear: F = -c*v
    float air_angular;      // airResistanceAngular: tau = -c*omega
    bool anchored;          // fixedMove: no linear motion at all
    bool fixed_rotate;      // fixedRotate: no angular motion
} phys_params;

void phys_set_world(float width, float height); // wall extents, meters
int phys_body_add(float x, float y, float radius, const phys_params* p);
void phys_steps(int n);

void phys_body_pos(int body, float* x, float* y);
float phys_body_orientation(int body); // radians, CCW positive, unbounded

// grabbing = mouse spring, exactly the original's constraint (sub_532800):
// F = stiffness * (target - pos) - dampener * vel, applied to momentum each step.
// Defaults from the Limb ctor (0x548410): stiffness=150, dampener=5,
// overridable per limb via mouse_*_override.
#define PHYS_MOUSE_STIFFNESS 150.0f
#define PHYS_MOUSE_DAMPENER 5.0f

void phys_grab(int body);
void phys_grab_move(int body, float x, float y); // updates spring target
void phys_release(int body);
