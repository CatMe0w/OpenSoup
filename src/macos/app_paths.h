#ifndef OPENSOUP_APP_PATHS_H
#define OPENSOUP_APP_PATHS_H

#include <stdbool.h>
#include <stddef.h>

// Returns the fixed per-user assets root, or NULL if macOS cannot resolve
// the user's Application Support directory. The returned pointer remains
// valid for the lifetime of the process.
const char* app_assets_root(void);

typedef enum {
    APP_ASSETS_READY,
    APP_ASSETS_DIRECTORY_MISSING,
    APP_ASSETS_CORE_MISSING,
} app_assets_state;

// Checks the fixed assets root and the minimum files needed to boot.
app_assets_state app_assets_get_state(void);

// Decodes the original installer in memory and installs its .toy files.
bool app_assets_install_from_installer(const char* path,
                                       char* error, size_t error_size);

#endif
