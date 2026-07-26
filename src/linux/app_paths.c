#include "app_paths.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

const char* linux_assets_root(void) {
    static char* assets_root;
    if (assets_root) {
        return assets_root;
    }

    const char* data_home = getenv("XDG_DATA_HOME");
    if (data_home && data_home[0] == '/') {
        assets_root = append_path(data_home, "cat.me0w.opensoup/assets");
        return assets_root;
    }

    const char* home = getenv("HOME");
    if (!home || home[0] != '/') {
        return NULL;
    }
    assets_root = append_path(home,
        ".local/share/cat.me0w.opensoup/assets");
    return assets_root;
}
