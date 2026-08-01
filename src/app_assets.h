#pragma once

#include <stdbool.h>
#include <stddef.h>

// The fixed per-user assets path, or NULL if it cannot be resolved. Always
// uses '/' separators, including on Windows. The returned pointer remains
// valid for the lifetime of the process.
const char* app_assets_path(void);

typedef enum {
    APP_ASSETS_READY,
    // The assets root is absent, is not a directory, or is empty.
    APP_ASSETS_MISSING,
} app_assets_state;

// A non-empty user-managed assets directory is ready to try. Content errors
// are reported by the application while booting the game.
app_assets_state app_assets_get_state(const char* assets_root);

// Decodes the original installer, installs its .toy files into a missing or
// empty assets root, and its .playset files into the playsets directory. On
// failure `error` always holds a human-readable reason.
bool app_assets_install_from_installer(
    const char* installer_path, const char* assets_root,
    char* error, size_t error_size);

#define APP_ASSETS_PLAYSETS "playsets"

// XXX: remove this. unify "assets" and "playsets" under a single "appdata" concept
bool app_assets_sibling_path(const char* assets_root, const char* name,
                             char* out, size_t out_size);
