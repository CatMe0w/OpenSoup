#pragma once

#include "toyfile.h"
#include <stdbool.h>

// Extracts one container's resource VFS below `directory`.
toyfile_status toyfile_extract_resources(const toyfile* file,
                                         const char* directory,
                                         char* error, size_t error_size);

// Writes one container as manifest.json, defs/, and resources/.
toyfile_status toyfile_extract_container(const toyfile* file,
                                         const char* directory,
                                         char* error, size_t error_size);

typedef struct {
    const char* name;
    const void* data;
    size_t size;
} toyfile_input;

// Installs .toy containers into a new assets root. Input bytes are borrowed
// only for the duration of the call.
toyfile_status toyfile_install_into_assets(const toyfile_input* inputs,
                                           size_t count,
                                           const char* assets_root,
                                           bool* assets_root_created,
                                           char* error, size_t error_size);
