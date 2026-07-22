#pragma once

#include "toyfile.h"

// Materializes the resource VFS of one parsed container below `directory`.
// Existing regular files are replaced. The original VFS's leading "../" is
// interpreted relative to its defs directory and removed; absolute paths,
// backslashes, and any remaining dot components are rejected.
toyfile_status toyfile_extract_resources(const toyfile* file,
                                         const char* directory,
                                         char* error, size_t error_size);
