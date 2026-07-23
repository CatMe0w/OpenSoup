#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    APP_ASSETS_READY,
    // The assets root is absent, is not a directory, or is empty.
    APP_ASSETS_MISSING,
} app_assets_state;

// A non-empty user-managed assets directory is ready to try. Content errors
// are reported by the application while booting the game.
app_assets_state app_assets_get_state(const char* assets_root);

typedef enum {
    APP_ASSETS_INSTALL_OK,
    APP_ASSETS_INSTALL_FAILED,
    // Installation failed after creating the assets root. The platform host
    // should remove that partial tree with its native filesystem API.
    APP_ASSETS_INSTALL_FAILED_PARTIAL,
} app_assets_install_status;

// Decodes the original installer and installs its .toy files into a missing
// or empty assets root.
app_assets_install_status app_assets_install_from_installer(
    const char* installer_path, const char* assets_root,
    char* error, size_t error_size);
