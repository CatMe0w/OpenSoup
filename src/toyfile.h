#pragma once

#include <stddef.h>

// Decoder for the framing and resource table of original
// "SOUPTOYS.COM TOY FORMAT" containers. Path-backed handles own the file data;
// memory-backed handles borrow it until toyfile_close().

typedef struct toyfile toyfile;

typedef enum {
    TOYFILE_OK = 0,
    TOYFILE_INVALID_ARGUMENT,
    TOYFILE_IO_ERROR,
    TOYFILE_INVALID_FORMAT,
    TOYFILE_OUT_OF_MEMORY,
} toyfile_status;

toyfile_status toyfile_open_path(const char* path, toyfile** out);

// Parses bytes owned by the caller. They must remain valid until
// toyfile_close().
toyfile_status toyfile_open_memory(const void* data, size_t size,
                                   toyfile** out);
void toyfile_close(toyfile* file);

// Human-readable parse/open error. Valid until toyfile_close().
const char* toyfile_error(const toyfile* file);

size_t toyfile_resource_count(const toyfile* file);

// Resource names and bytes point into storage owned by file. `extension` is
// the original directory-entry metadata; the resource name already carries
// the path used by the original VFS. Output arguments may be NULL.
toyfile_status toyfile_resource_at(const toyfile* file, size_t index,
                                   const char** name, size_t* name_size,
                                   const char** extension,
                                   size_t* extension_size,
                                   const void** data, size_t* data_size);
