#pragma once

#include "toyfile.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool present;
    float value;
} toyfile_optional_float;

typedef struct {
    bool present;
    float value[2];
} toyfile_optional_vec2;

typedef struct {
    char* version_description;
    char* file_description;
    char* author;
    char* creation_date;
} toyfile_playset_info;

typedef struct {
    toyfile_optional_float left_wall;
    toyfile_optional_float right_wall;
    toyfile_optional_float floor;
    toyfile_optional_float ceiling;
    toyfile_optional_float timestep;
    toyfile_optional_float gravity;
    toyfile_optional_vec2 drop_position;
} toyfile_playset_world;

typedef struct {
    char* limb_id;
    float position[2];
    float orientation;
    float momentum[2];
    float angular_momentum;
} toyfile_playset_limb;

typedef struct {
    int32_t toy_instance_id;
    char* toy_id;
    char* extra;
    size_t limb_count;
    toyfile_playset_limb* limbs;
} toyfile_playset_toy;

typedef struct {
    uint32_t source_version;
    toyfile_playset_info info;
    bool has_world;
    toyfile_playset_world world;
    size_t toy_count;
    toyfile_playset_toy* toys;
} toyfile_playset;

// Decodes only the playset body of an already validated original container.
// Strings and arrays in the result are owned by the result. `error` is optional.
toyfile_status toyfile_playset_decode(const toyfile* container,
                                      toyfile_playset** out,
                                      char* error, size_t error_size);
void toyfile_playset_free(toyfile_playset* playset);
