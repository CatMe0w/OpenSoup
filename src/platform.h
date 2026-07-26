#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char** names;
    size_t count;
} platform_directory_list;

typedef enum {
    PLATFORM_DIRECTORY_UNREADABLE = -1,
    PLATFORM_DIRECTORY_EMPTY,
    PLATFORM_DIRECTORY_NONEMPTY,
} platform_directory_state;

typedef enum {
    PLATFORM_PATH_ERROR = -1,
    PLATFORM_PATH_MISSING,
    PLATFORM_PATH_OTHER,
    PLATFORM_PATH_DIRECTORY,
} platform_path_kind;

typedef enum {
    PLATFORM_WRITE_OK,
    PLATFORM_WRITE_OPEN_FAILED,
    PLATFORM_WRITE_FAILED,
    PLATFORM_WRITE_CLOSE_FAILED,
} platform_write_result;

// Returns the fixed per-user assets path, or NULL if it cannot be resolved.
// The returned pointer remains valid for the lifetime of the process.
const char* platform_assets_path(void);

// Lists all directory entries, including dot entries, in locale-aware
// alphabetical order. The result must be released with
// platform_directory_list_free().
bool platform_directory_list_sorted(const char* path,
                                    platform_directory_list* result);
void platform_directory_list_free(platform_directory_list* list);

platform_directory_state platform_get_directory_state(const char* path);
platform_path_kind platform_get_path_kind(const char* path, bool follow_links);

int platform_create_directory(const char* path);
int platform_remove_empty_directory(const char* path);

// Recursively removes an absolute, non-root directory tree without following
// symbolic links encountered as entries.
bool platform_remove_tree(const char* path);

// Replaces a regular file without following a symbolic link at the final path
// component. On failure, errno describes the failed operation.
platform_write_result platform_write_file(const char* path, const void* data,
                                          size_t size);
