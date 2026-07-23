#include "app_assets.h"

#include "nsis.h"
#include "toyfile_fs.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef enum {
    DIRECTORY_UNREADABLE = -1,
    DIRECTORY_EMPTY,
    DIRECTORY_NONEMPTY,
} directory_state;

static directory_state get_directory_state(const char* path) {
    DIR* directory = opendir(path);
    if (!directory) {
        return DIRECTORY_UNREADABLE;
    }
    directory_state state = DIRECTORY_EMPTY;
    struct dirent* entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") != 0
            && strcmp(entry->d_name, "..") != 0) {
            state = DIRECTORY_NONEMPTY;
            break;
        }
    }
    closedir(directory);
    return state;
}

app_assets_state app_assets_get_state(const char* assets_root) {
    struct stat info;
    if (!assets_root || stat(assets_root, &info) != 0
        || !S_ISDIR(info.st_mode)
        || get_directory_state(assets_root) != DIRECTORY_NONEMPTY) {
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
    struct stat info;
    if (lstat(root, &info) == 0) {
        if (!S_ISDIR(info.st_mode)
            || get_directory_state(root) != DIRECTORY_EMPTY) {
            if (error && error_size) {
                snprintf(error, error_size,
                         "assets root is not an empty directory: %s", root);
            }
            return false;
        }
        if (rmdir(root) != 0) {
            if (error && error_size) {
                snprintf(error, error_size,
                         "cannot remove empty assets root %s: %s",
                         root, strerror(errno));
            }
            return false;
        }
    } else if (errno != ENOENT) {
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
