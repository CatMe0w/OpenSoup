#pragma once

#include "toyfile.h"

// Extracts one container's resource VFS below `directory`.
toyfile_status toyfile_extract_resources(const toyfile* file,
                                         const char* directory,
                                         char* error, size_t error_size);

// Writes one container as manifest.json, defs/, and resources/.
toyfile_status toyfile_extract_container(const toyfile* file,
                                         const char* directory,
                                         char* error, size_t error_size);

// Installs discrete .toy files into a new assets root.
toyfile_status toyfile_install_assets(const char* const* paths,
                                      size_t count,
                                      const char* assets_root,
                                      char* error, size_t error_size);
