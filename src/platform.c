#if defined(__linux__) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif

#include "platform.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__APPLE__)
#import <Foundation/Foundation.h>
#elif defined(__linux__)
#include <ftw.h>
#else
#error Unsupported platform
#endif

#if defined(__linux__)
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
#endif

#if defined(__APPLE__)
static NSURL* assets_path_url(void) {
    NSArray<NSURL*>* urls = [[NSFileManager defaultManager]
        URLsForDirectory:NSApplicationSupportDirectory
               inDomains:NSUserDomainMask];
    NSURL* support = urls.firstObject;
    if (!support) {
        return nil;
    }

    return [[support URLByAppendingPathComponent:@"cat.me0w.opensoup"
                                     isDirectory:YES]
        URLByAppendingPathComponent:@"assets" isDirectory:YES];
}

const char* platform_assets_path(void) {
    static char assets_path[PATH_MAX];

    @autoreleasepool {
        NSURL* url = assets_path_url();
        if (!url) {
            return NULL;
        }

        const char* path = url.fileSystemRepresentation;
        if (!path || strlcpy(assets_path, path, sizeof assets_path)
                     >= sizeof assets_path) {
            return NULL;
        }
    }

    return assets_path;
}
#elif defined(__linux__)
const char* platform_assets_path(void) {
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
#endif

static int compare_names(const void* lhs, const void* rhs) {
    const char* const* a = lhs;
    const char* const* b = rhs;
    return strcoll(*a, *b);
}

void platform_directory_list_free(platform_directory_list* list) {
    if (!list) {
        return;
    }
    for (size_t i = 0; i < list->count; i++) {
        free(list->names[i]);
    }
    free(list->names);
    *list = (platform_directory_list){0};
}

bool platform_directory_list_sorted(const char* path,
                                    platform_directory_list* result) {
    if (!result) {
        errno = EINVAL;
        return false;
    }
    *result = (platform_directory_list){0};

    DIR* directory = opendir(path);
    if (!directory) {
        return false;
    }

    size_t capacity = 0;
    errno = 0;
    struct dirent* entry;
    while ((entry = readdir(directory))) {
        if (result->count == capacity) {
            const size_t next_capacity = capacity ? capacity * 2 : 16;
            if (next_capacity < capacity
                || next_capacity > SIZE_MAX / sizeof(*result->names)) {
                errno = ENOMEM;
                break;
            }
            char** names = realloc(
                result->names, next_capacity * sizeof(*result->names));
            if (!names) {
                errno = ENOMEM;
                break;
            }
            result->names = names;
            capacity = next_capacity;
        }
        result->names[result->count] = strdup(entry->d_name);
        if (!result->names[result->count]) {
            errno = ENOMEM;
            break;
        }
        result->count++;
        errno = 0;
    }

    int error = errno;
    if (closedir(directory) != 0 && error == 0) {
        error = errno;
    }
    if (error != 0) {
        platform_directory_list_free(result);
        errno = error;
        return false;
    }

    if (result->count > 1) {
        qsort(result->names, result->count,
              sizeof(*result->names), compare_names);
    }
    return true;
}

platform_directory_state platform_get_directory_state(const char* path) {
    DIR* directory = opendir(path);
    if (!directory) {
        return PLATFORM_DIRECTORY_UNREADABLE;
    }

    platform_directory_state state = PLATFORM_DIRECTORY_EMPTY;
    struct dirent* entry;
    while ((entry = readdir(directory))) {
        if (strcmp(entry->d_name, ".") != 0
            && strcmp(entry->d_name, "..") != 0) {
            state = PLATFORM_DIRECTORY_NONEMPTY;
            break;
        }
    }
    closedir(directory);
    return state;
}

platform_path_kind platform_get_path_kind(const char* path, bool follow_links) {
    struct stat info;
    const int status = follow_links ? stat(path, &info) : lstat(path, &info);
    if (status != 0) {
        return errno == ENOENT ? PLATFORM_PATH_MISSING : PLATFORM_PATH_ERROR;
    }
    return S_ISDIR(info.st_mode) ? PLATFORM_PATH_DIRECTORY
                                 : PLATFORM_PATH_OTHER;
}

int platform_create_directory(const char* path) {
    return mkdir(path, 0777);
}

int platform_remove_empty_directory(const char* path) {
    return rmdir(path);
}

static bool valid_remove_tree_root(const char* path) {
    if (!path || path[0] != '/') {
        return false;
    }

    bool has_component = false;
    const char* cursor = path;
    while (*cursor) {
        while (*cursor == '/') {
            cursor++;
        }
        const char* component = cursor;
        while (*cursor && *cursor != '/') {
            cursor++;
        }
        const size_t size = (size_t)(cursor - component);
        if (size == 0) {
            continue;
        }
        if ((size == 1 && component[0] == '.')
            || (size == 2 && component[0] == '.'
                          && component[1] == '.')) {
            return false;
        }
        has_component = true;
    }
    return has_component;
}

#if defined(__APPLE__)
bool platform_remove_tree(const char* path) {
    if (!valid_remove_tree_root(path)) {
        return false;
    }

    @autoreleasepool {
        NSURL* root = [NSURL fileURLWithFileSystemRepresentation:path
                                                     isDirectory:YES
                                                   relativeToURL:nil];
        return [[NSFileManager defaultManager] removeItemAtURL:root error:nil];
    }
}
#elif defined(__linux__)
static int remove_tree_entry(const char* path, const struct stat* info,
                             int type, struct FTW* state) {
    (void)info;
    (void)type;
    (void)state;
    return remove(path);
}

bool platform_remove_tree(const char* path) {
    if (!valid_remove_tree_root(path)) {
        return false;
    }
    return nftw(path, remove_tree_entry, 64,
                FTW_DEPTH | FTW_PHYS | FTW_MOUNT) == 0;
}
#endif

platform_write_result platform_write_file(const char* path, const void* data,
                                          size_t size) {
    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0666);
    if (fd < 0) {
        return PLATFORM_WRITE_OPEN_FAILED;
    }

    const unsigned char* bytes = data;
    size_t written = 0;
    while (written < size) {
        const ssize_t count = write(fd, bytes + written, size - written);
        if (count > 0) {
            written += (size_t)count;
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else {
            const int error = errno;
            close(fd);
            errno = error;
            return PLATFORM_WRITE_FAILED;
        }
    }
    if (close(fd) != 0) {
        return PLATFORM_WRITE_CLOSE_FAILED;
    }
    return PLATFORM_WRITE_OK;
}
