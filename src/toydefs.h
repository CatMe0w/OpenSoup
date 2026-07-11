#pragma once
#include <stdbool.h>

// Toy definitions loaded from toydefs.json
// Interim pipeline: the original parses these natively
// (BinaryToyReader); whether we port that reader to C or keep the external
// converter is deliberately deferred.

typedef struct {
    const char* class_name;
    float mass;
    float inertia_tensor;
    bool has_gravity_override;
    float gravity_override;
    float mouse_stiffness; // resolved: per-limb override or engine default
    float mouse_dampener;
    float air_resistance_linear;
    float air_resistance_angular;
    bool fixed_move;
    bool fixed_rotate;
    // material: velocityResponse, stiffness, dampener, kineticFriction, staticFriction
    float material[5];
} toydef_t;

bool toydefs_load(const char* json_path);
const toydef_t* toydefs_find(const char* class_name);
int toydefs_count(void);
