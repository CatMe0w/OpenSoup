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
// NSApplicationSupportDirectory resolves to exactly this for an unsandboxed
// process, and a sandbox would rewrite HOME to the container anyway - so this
// stays free of Foundation, and of Objective-C.
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
    // %LOCALAPPDATA% arrives with backslashes. Every path OpenSoup handles is
    // split on '/' alone, so normalize once, here.
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

bool app_assets_remove_partial(const char* assets_root) {
    return toyfile_remove_tree(assets_root);
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
        if (rmdir(root) != 0) {
            if (error && error_size) {
                snprintf(error, error_size,
                         "cannot remove empty assets root %s: %s",
                         root, strerror(errno));
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
