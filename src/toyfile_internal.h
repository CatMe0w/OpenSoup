#pragma once

#include "toyfile.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Borrowed exact body range between the container version and the first
// resource. Body decoders must consume this slice completely.
typedef struct {
    const unsigned char* data;
    size_t size;
    uint32_t version;
} toyfile_body;

bool toyfile_get_body(const toyfile* file, toyfile_body* out);

struct cJSON;

// Returns an owned cJSON tree without mutating the container. Opening a
// toyfile itself remains a pure container operation.
toyfile_status toyfile_decode_manifest(const toyfile* file,
                                       struct cJSON** out,
                                       char* error, size_t error_size);
