#pragma once
#include <stdbool.h>

// Toy definitions loaded from toydefs.json (internal, intermediate)
// Interim pipeline: the original parses these natively (BinaryToyReader);
// whether we port that reader to C or keep the external converter is
// deliberately deferred.
//
// Units: geometry (rest positions, shape points, joint anchors) is in
// TOY-LOCAL units; instantiation scales by base_scale into world meters
// (inertiaTensor scales by base_scale^2). Sprite centre-of-mass offsets are
// sprite pixels, y-down, relative to the canvas centre; the FLC rotation
// frames are pre-rendered about that canvas point.

typedef struct {
    float x, y, r; // unified vertex encoding: circle = 1 point with radius
} td_point;

typedef struct {
    bool collides; // memberOf non-empty: participates in collision
    bool grab;     // mouse-interaction shape
    int npoints;
    td_point* points; // toy-local units
} td_shape;

typedef struct {
    char* image; // resolved color-FLC path relative to the assets root
    int num_frames;
    float com[2]; // objectCentreOfMass, sprite px y-down from canvas centre
    int z_order;  // ascending = back-to-front
} td_sprite;

typedef struct {
    char* name;
    float rest_pos[2];  // bodyState pos, toy-local
    float rest_orient;  // bodyState orientation, radians CCW
    float mass;
    float inertia;      // toy-local units^2
    bool has_gravity_override;
    float gravity_override;
    float mouse_stiffness; // resolved: per-limb override or engine default
    float mouse_dampener;
    float air_resistance_linear;
    float air_resistance_angular;
    bool fixed_move;
    bool fixed_rotate;
    bool default_grab_move;
    bool default_grab_rotate;
    // material: velocityResponse, stiffness, dampener, kineticFriction, staticFriction
    float material[5];
    float motor_force[2]; // summed linearMotors, limb-local
    float motor_torque;   // summed rotationalMotors
    int nshapes;
    td_shape* shapes;
    int nsprites;
    td_sprite* sprites;
} td_limb;

typedef struct {
    int limb1, limb2;             // limb indices, -1 if the name didn't resolve
    float anchor1[2], anchor2[2]; // limb-local, toy-local units
    float rest_length;
    float stiffness;
    float dampener;
} td_joint;

typedef struct {
    const char* class_name;
    float base_scale;
    int nlimbs;
    td_limb* limbs;
    int njoints;
    td_joint* joints; // spring joints; rotationalJoints not consumed yet
} toydef_t;

bool toydefs_load(const char* json_path);
const toydef_t* toydefs_find(const char* class_name);
int toydefs_count(void);
