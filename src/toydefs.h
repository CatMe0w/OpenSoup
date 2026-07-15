#pragma once
#include <stdbool.h>

// Toy definitions loaded from toydefs.json (internal, intermediate)
// Interim pipeline: the original parses these natively (BinaryToyReader);
// whether we port that reader to C or keep the external converter is
// deliberately deferred.
//
// Units: geometry (rest positions, shape points, joint anchors) is in
// TOY-LOCAL units; instantiation scales by base_scale into world meters
// (inertiaTensor scales by base_scale^2). Sprite centre-of-mass offsets use
// pixel-sized units but are limb-local y-up vectors from the body origin to
// the visual centre; FLC rotation frames are pre-rendered about that point.

typedef struct {
    float x, y, r; // unified vertex encoding: circle = 1 point with radius
} td_point;

typedef struct {
    char* sid;     // scripts address shapes via shapes.by_sid(:sid)
    bool grab;     // mouse-interaction shape
    bool grab_move;
    bool grab_rotate;
    int nmembers;
    char** members; // memberOf symbols, retained for collision + Ruby events
    int npoints;
    td_point* points; // toy-local units
} td_shape;

typedef struct {
    char* sid;   // scripts address sprites via sprites.by_sid(:sid)
    char* image; // resolved color-FLC path relative to the assets root
    int num_frames;
    float com[2]; // objectCentreOfMass: body origin -> visual centre, px, y-up
    int z_order;  // smaller = nearer the viewer; draw descending
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
    char* local_collision_group; // only filters equal groups within one toy
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
    int limb1, limb2;
    float orientation1, orientation2;
    // decoded field order [rest, stiffness, dampener] matches the linear
    // joint tail; not yet byte-verified against the binary (schema TODO)
    float rest;
    float stiffness;
    float dampener;
} td_rotjoint;

typedef struct {
    char* sid;      // scripts look sounds up via sounds.by_sid(:sid)
    char* location; // ogg path relative to <root>/defs/
} td_sound;

typedef struct {
    const char* class_name;
    const char* root; // container dir under the assets root (toy scripts live there)
    float base_scale;
    int nlimbs;
    td_limb* limbs;
    int njoints;
    td_joint* joints;
    int nrotjoints;
    td_rotjoint* rotjoints;
    int nsounds;
    td_sound* sounds; // toy-level sound emitters
} toydef_t;

// Lightweight Toybox catalog records.  These come from the icon catalog in
// each original .toy manifest; they are metadata only and never become
// physics bodies.  image is an assets-root-relative TGA sequence prefix
// (".../Ball Tennis" -> Ball Tennis0000.tga, Ball Tennis0001.tga).
typedef struct {
    const char* name;
    const char* class_name;
    const char* image;
    int num_frames;
    int instance_limit;
    float pack_order;
    float order;
} toyicon_t;

typedef struct {
    const char* id;
    const char* license;
    const char* header; // assets-root-relative TGA sequence prefix
    float order;
} toypack_t;

bool toydefs_load(const char* json_path);
const toydef_t* toydefs_find(const char* class_name);
int toydefs_count(void);
int toydefs_icon_count(void);
const toyicon_t* toydefs_icon_at(int index);
int toydefs_pack_count(void);
const toypack_t* toydefs_pack_at(int index);
