#include "app_assets.h"

#include "nsis.h"
#include "platform.h"
#include "toyfile_fs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

app_assets_state app_assets_get_state(const char* assets_root) {
    if (!assets_root
        || platform_get_path_kind(assets_root, true)
            != PLATFORM_PATH_DIRECTORY
        || platform_get_directory_state(assets_root)
            != PLATFORM_DIRECTORY_NONEMPTY) {
        return APP_ASSETS_MISSING;
    }
    return APP_ASSETS_READY;
}

typedef struct {
    const char* assets_root;
    bool assets_root_created;
} install_context;

static bool install_assets_from_decoded_toys(
        void* context, const nsis_container* containers, size_t count,
        char* error, size_t error_size) {
    install_context* install = context;
    const char* root = install->assets_root;
    const platform_path_kind kind = platform_get_path_kind(root, false);
    if (kind != PLATFORM_PATH_MISSING && kind != PLATFORM_PATH_ERROR) {
        if (kind != PLATFORM_PATH_DIRECTORY
            || platform_get_directory_state(root)
                != PLATFORM_DIRECTORY_EMPTY) {
            if (error && error_size) {
                snprintf(error, error_size,
                         "assets root is not an empty directory: %s", root);
            }
            return false;
        }
        if (platform_remove_empty_directory(root) != 0) {
            if (error && error_size) {
                snprintf(error, error_size,
                         "cannot remove empty assets root %s: %s",
                         root, strerror(errno));
            }
            return false;
        }
    } else if (kind == PLATFORM_PATH_ERROR) {
        if (error && error_size) {
            snprintf(error, error_size, "cannot inspect assets root %s: %s",
                     root, strerror(errno));
        }
        return false;
    }

    toyfile_input* inputs = calloc(count ? count : 1, sizeof(*inputs));
    if (!inputs) {
        if (error && error_size) {
            snprintf(error, error_size,
                     "out of memory indexing decoded .toy files");
        }
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (containers[i].type != NSIS_CONTAINER_TOY) {
            if (error && error_size) {
                snprintf(error, error_size,
                         "installer returned a non-.toy container");
            }
            free(inputs);
            return false;
        }
        inputs[i] = (toyfile_input){
            containers[i].name, containers[i].data, containers[i].size,
        };
    }

    const toyfile_status status = toyfile_install_into_assets(
        inputs, count, root, &install->assets_root_created,
        error, error_size);
    free(inputs);
    return status == TOYFILE_OK;
}

app_assets_install_status app_assets_install_from_installer(
        const char* installer_path, const char* assets_root,
        char* error, size_t error_size) {
    if (error && error_size) {
        error[0] = 0;
    }
    if (!installer_path || !installer_path[0]
        || !assets_root || !assets_root[0]) {
        if (error && error_size) {
            snprintf(error, error_size, "missing installer or assets root");
        }
        return APP_ASSETS_INSTALL_FAILED;
    }

    install_context context = {.assets_root = assets_root};
    const bool ok = nsis_decode_containers(
        installer_path, NSIS_CONTAINER_TOY,
        install_assets_from_decoded_toys, &context,
        error, error_size);
    if (ok) {
        return APP_ASSETS_INSTALL_OK;
    }
    return context.assets_root_created
         ? APP_ASSETS_INSTALL_FAILED_PARTIAL
         : APP_ASSETS_INSTALL_FAILED;
}
