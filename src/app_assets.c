#include "app_assets.h"

#include "nsis.h"
#include "toyfile_fs.h"

#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if !defined(__APPLE__) && !defined(__linux__) && !defined(__MINGW32__)
#error Unsupported platform
#endif

static char* append_path(const char* base, const char* relative) {
    size_t base_len = strlen(base);
    while (base_len > 1 && base[base_len - 1] == '/') {
        base_len--;
    }

    const size_t relative_len = strlen(relative);
    const size_t separator_len = base[base_len - 1] == '/' ? 0 : 1;
    if (base_len > SIZE_MAX - separator_len - relative_len - 1) {
        return NULL;
    }

    char* path = malloc(base_len + separator_len + relative_len + 1);
    if (!path) {
        return NULL;
    }

    memcpy(path, base, base_len);
    size_t offset = base_len;
    if (separator_len) {
        path[offset++] = '/';
    }
    memcpy(path + offset, relative, relative_len + 1);
    return path;
}

#if defined(__APPLE__)
// Avoids Foundation; sandboxed HOME already points to the container.
const char* app_assets_path(void) {
    static char* assets_path;
    if (assets_path) {
        return assets_path;
    }

    const char* home = getenv("HOME");
    if (!home || home[0] != '/') {
        return NULL;
    }
    assets_path = append_path(home,
        "Library/Application Support/cat.me0w.opensoup/assets");
    return assets_path;
}
#elif defined(__linux__)
const char* app_assets_path(void) {
    static char* assets_path;
    if (assets_path) {
        return assets_path;
    }

    const char* data_home = getenv("XDG_DATA_HOME");
    if (data_home && data_home[0] == '/') {
        assets_path = append_path(data_home, "cat.me0w.opensoup/assets");
        return assets_path;
    }

    const char* home = getenv("HOME");
    if (!home || home[0] != '/') {
        return NULL;
    }
    assets_path = append_path(home,
        ".local/share/cat.me0w.opensoup/assets");
    return assets_path;
}
#elif defined(__MINGW32__)
const char* app_assets_path(void) {
    static char* assets_path;
    if (assets_path) {
        return assets_path;
    }

    const char* local_app_data = getenv("LOCALAPPDATA");
    if (!local_app_data || !local_app_data[0]) {
        return NULL;
    }
    // Normalize backslashes to '/'.
    char* base = strdup(local_app_data);
    if (!base) {
        return NULL;
    }
    for (char* p = base; *p; p++) {
        if (*p == '\\') {
            *p = '/';
        }
    }
    const char drive = base[0];
    const bool absolute = ((drive >= 'A' && drive <= 'Z')
                        || (drive >= 'a' && drive <= 'z'))
                       && base[1] == ':' && base[2] == '/';
    if (absolute) {
        assets_path = append_path(base, "cat.me0w.opensoup/assets");
    }
    free(base);
    return assets_path;
}
#endif

bool app_assets_sibling_path(const char* assets_root, const char* name,
                             char* out, size_t out_size) {
    if (!assets_root || !name || !out || out_size == 0) {
        return false;
    }
    size_t length = strlen(assets_root);
    while (length > 1 && assets_root[length - 1] == '/') {
        length--;
    }
    size_t parent = length;
    while (parent > 0 && assets_root[parent - 1] != '/') {
        parent--;
    }
    if (parent == 0) {
        return false; // a relative root has nothing to sit beside
    }
    parent--; // the separator itself
    // A root one level down ("/assets") leaves the filesystem root as parent
    const int n = parent
        ? snprintf(out, out_size, "%.*s/%s", (int)parent, assets_root, name)
        : snprintf(out, out_size, "/%s", name);
    return n > 0 && (size_t)n < out_size;
}

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
    const struct dirent* entry;
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
    if (!assets_root
        || toyfile_path_stat(assets_root, true) != TOYFILE_PATH_DIRECTORY
        || get_directory_state(assets_root) != DIRECTORY_NONEMPTY) {
        return APP_ASSETS_MISSING;
    }
    return APP_ASSETS_READY;
}

static bool install_assets_from_decoded_containers(
        void* context, const nsis_container* containers, size_t count,
        char* error, size_t error_size) {
    const char* root = context;
    // An existing empty root is accepted and removed by the install itself,
    // just before the staging directory is renamed into its place.
    const toyfile_path_kind kind = toyfile_path_stat(root, false);
    if (kind != TOYFILE_PATH_MISSING && kind != TOYFILE_PATH_ERROR) {
        if (kind != TOYFILE_PATH_DIRECTORY
            || get_directory_state(root) != DIRECTORY_EMPTY) {
            if (error && error_size) {
                snprintf(error, error_size,
                         "assets root is not an empty directory: %s", root);
            }
            return false;
        }
    } else if (kind == TOYFILE_PATH_ERROR) {
        if (error && error_size) {
            snprintf(error, error_size, "cannot inspect assets root %s: %s",
                     root, strerror(errno));
        }
        return false;
    }

    char playsets[2048];
    if (!app_assets_sibling_path(root, APP_ASSETS_PLAYSETS,
                                 playsets, sizeof playsets)) {
        if (error && error_size) {
            snprintf(error, error_size,
                     "cannot place the playsets directory beside %s", root);
        }
        return false;
    }

    // One allocation, .toy containers first and .playset files after them,
    // since the two groups install separately.
    toyfile_input* inputs = calloc(count ? count : 1, sizeof(*inputs));
    if (!inputs) {
        if (error && error_size) {
            snprintf(error, error_size,
                     "out of memory indexing decoded containers");
        }
        return false;
    }
    size_t toy_count = 0;
    size_t playset_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (containers[i].type != NSIS_CONTAINER_TOY
            && containers[i].type != NSIS_CONTAINER_PLAYSET) {
            if (error && error_size) {
                snprintf(error, error_size,
                         "installer returned an unknown container type");
            }
            free(inputs);
            return false;
        }
        if (containers[i].type == NSIS_CONTAINER_TOY) {
            inputs[toy_count++] = (toyfile_input){
                containers[i].name, containers[i].data, containers[i].size,
            };
        }
    }
    for (size_t i = 0; i < count; i++) {
        if (containers[i].type == NSIS_CONTAINER_PLAYSET) {
            inputs[toy_count + playset_count++] = (toyfile_input){
                containers[i].name, containers[i].data, containers[i].size,
            };
        }
    }

    toyfile_status status = toyfile_install_playsets(
        inputs + toy_count, playset_count, playsets, error, error_size);
    if (status == TOYFILE_OK) {
        status = toyfile_install_into_assets(inputs, toy_count, root,
                                             error, error_size);
    }
    free(inputs);
    return status == TOYFILE_OK;
}

bool app_assets_install_from_installer(
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
        return false;
    }

    const bool ok = nsis_decode_containers(
        installer_path, NSIS_CONTAINER_TOY | NSIS_CONTAINER_PLAYSET,
        install_assets_from_decoded_containers, (void*)assets_root,
        error, error_size);
    if (!ok && error && error_size && !error[0]) {
        snprintf(error, error_size,
                 "could not extract the selected installer");
    }
    return ok;
}
