#include "toyfile_fs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

typedef struct {
    char* error;
    size_t error_size;
} fs_context;

static void fs_error(fs_context* fs, const char* fmt, ...) {
    if (!fs->error || fs->error_size == 0) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(fs->error, fs->error_size, fmt, ap);
    va_end(ap);
}

static bool make_directory(const char* path, bool trusted_root,
                           fs_context* fs) {
    if (mkdir(path, 0777) == 0) {
        return true;
    }
    if (errno == EEXIST) {
        struct stat info;
        const int status = trusted_root ? stat(path, &info) : lstat(path, &info);
        if (status == 0 && S_ISDIR(info.st_mode)) {
            return true;
        }
    }
    fs_error(fs, "cannot create directory %s: %s", path, strerror(errno));
    return false;
}

static bool make_directory_tree(const char* path, fs_context* fs) {
    char* copy = strdup(path);
    if (!copy) {
        fs_error(fs, "out of memory creating %s", path);
        return false;
    }
    for (char* p = copy + 1; *p; p++) {
        if (*p != '/') {
            continue;
        }
        *p = 0;
        if (copy[0] && !make_directory(copy, true, fs)) {
            free(copy);
            return false;
        }
        *p = '/';
    }
    const bool ok = make_directory(copy, true, fs);
    free(copy);
    return ok;
}

static bool safe_resource_name(const char* name, size_t size) {
    if (!name || size == 0 || name[0] == '/' || name[size - 1] == '/') {
        return false;
    }
    size_t component = 0;
    for (size_t i = 0; i <= size; i++) {
        if (i < size && name[i] != '/') {
            if (name[i] == 0 || name[i] == '\\') {
                return false;
            }
            continue;
        }
        const size_t length = i - component;
        if (length == 0) {
            component = i + 1;
            continue; // the original corpus contains a harmless "//"
        }
        if ((length == 1 && name[component] == '.')
            || (length == 2 && name[component] == '.'
                            && name[component + 1] == '.')) {
            return false;
        }
        component = i + 1;
    }
    return true;
}

static void normalize_resource_name(const char** name, size_t* size) {
    while (*size >= 3 && memcmp(*name, "../", 3) == 0) {
        *name += 3;
        *size -= 3;
    }
}

static bool make_parent_directories(char* path, size_t root_length,
                                    fs_context* fs) {
    for (char* p = path + root_length + 1; *p; p++) {
        if (*p != '/') {
            continue;
        }
        *p = 0;
        const bool ok = make_directory(path, false, fs);
        *p = '/';
        if (!ok) {
            return false;
        }
    }
    return true;
}

static bool write_resource(const char* path, const void* data, size_t size,
                           fs_context* fs) {
    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0666);
    if (fd < 0) {
        fs_error(fs, "cannot open %s: %s", path, strerror(errno));
        return false;
    }
    const unsigned char* bytes = data;
    size_t written = 0;
    while (written < size) {
        const ssize_t n = write(fd, bytes + written, size - written);
        if (n > 0) {
            written += (size_t)n;
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else {
            const int write_error = errno;
            close(fd);
            fs_error(fs, "cannot write %s: %s", path, strerror(write_error));
            return false;
        }
    }
    if (close(fd) != 0) {
        fs_error(fs, "cannot close %s: %s", path, strerror(errno));
        return false;
    }
    return true;
}

toyfile_status toyfile_extract_resources(const toyfile* file,
                                         const char* directory,
                                         char* error, size_t error_size) {
    fs_context fs = {error, error_size};
    if (fs.error && fs.error_size) {
        error[0] = 0;
    }
    if (!file || !directory || !directory[0]) {
        fs_error(&fs, "missing container or output directory");
        return TOYFILE_INVALID_ARGUMENT;
    }
    if (!make_directory_tree(directory, &fs)) {
        return TOYFILE_IO_ERROR;
    }

    size_t root_length = strlen(directory);
    while (root_length > 1 && directory[root_length - 1] == '/') {
        root_length--;
    }
    for (size_t i = 0; i < toyfile_resource_count(file); i++) {
        const char* name = NULL;
        const void* data = NULL;
        size_t name_size = 0, data_size = 0;
        (void)toyfile_resource_at(
            file, i, &name, &name_size, NULL, NULL,
            &data, &data_size);
        normalize_resource_name(&name, &name_size);
        if (!safe_resource_name(name, name_size)) {
            fs_error(&fs, "unsafe resource path at index %zu", i);
            return TOYFILE_INVALID_FORMAT;
        }

        char* path = malloc(root_length + 1 + name_size + 1);
        if (!path) {
            fs_error(&fs, "out of memory writing resource %zu", i);
            return TOYFILE_OUT_OF_MEMORY;
        }
        memcpy(path, directory, root_length);
        path[root_length] = '/';
        memcpy(path + root_length + 1, name, name_size);
        path[root_length + 1 + name_size] = 0;
        const bool ok = make_parent_directories(path, root_length, &fs)
                     && write_resource(path, data, data_size, &fs);
        free(path);
        if (!ok) {
            return TOYFILE_IO_ERROR;
        }
    }
    return TOYFILE_OK;
}
