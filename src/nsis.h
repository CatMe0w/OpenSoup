#pragma once

#include <stdbool.h>
#include <stddef.h>

// Container types stored by the original Souptoys NSIS 2 installer.
typedef enum {
    NSIS_CONTAINER_TOY = 1u << 0,
    NSIS_CONTAINER_PLAYSET = 1u << 1,
} nsis_container_type;

typedef struct {
    const char* name;
    nsis_container_type type;
    const void* data;
    size_t size;
} nsis_container;

// Container names and bytes are borrowed from the decoded installer and stay
// valid only during the synchronous consumer call.
typedef bool (*nsis_container_consumer)(const nsis_container* containers,
                                        size_t count,
                                        char* error, size_t error_size);

bool nsis_decode_containers(const char* installer_path, unsigned types,
                            nsis_container_consumer consumer,
                            char* error, size_t error_size);
