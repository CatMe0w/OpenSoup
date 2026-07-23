#include "toyfile_fs.h"

#include "cJSON.h"

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

static char* child_path(const char* directory, const char* child,
                        fs_context* fs) {
    size_t root_size = strlen(directory);
    while (root_size > 1 && directory[root_size - 1] == '/') {
        root_size--;
    }
    const size_t child_size = strlen(child);
    char* path = malloc(root_size + 1 + child_size + 1);
    if (!path) {
        fs_error(fs, "out of memory creating path below %s", directory);
        return NULL;
    }
    memcpy(path, directory, root_size);
    path[root_size] = '/';
    memcpy(path + root_size + 1, child, child_size + 1);
    return path;
}

static bool safe_def_id(const char* id) {
    return id && id[0] && strcmp(id, ".") != 0 && strcmp(id, "..") != 0 &&
           !strchr(id, '/') && !strchr(id, '\\');
}

static char* def_filename(const char* id, fs_context* fs) {
    const size_t size = strlen(id);
    char* name = malloc(size + sizeof ".json");
    if (!name) {
        fs_error(fs, "out of memory naming CToy %s", id);
        return NULL;
    }
    for (size_t i = 0; i < size; i++) {
        const unsigned char c = (unsigned char)id[i];
        name[i] = c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : (char)c;
    }
    memcpy(name + size, ".json", sizeof ".json");
    return name;
}

static bool write_json(const char* path, const cJSON* value, fs_context* fs) {
    char* json = cJSON_Print(value);
    if (!json) {
        fs_error(fs, "out of memory serializing %s", path);
        return false;
    }
    const bool ok = write_resource(path, json, strlen(json), fs);
    free(json);
    return ok;
}

static char* container_name_from_path(const char* path, fs_context* fs,
                                      toyfile_status* status) {
    *status = TOYFILE_INVALID_ARGUMENT;
    if (!path || !path[0]) {
        fs_error(fs, "missing .toy input path");
        return NULL;
    }
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    const char* extension = strrchr(base, '.');
    if (!extension || extension == base || strcmp(extension, ".toy") != 0) {
        fs_error(fs, "input is not a .toy file: %s", path);
        return NULL;
    }
    const size_t stem_size = (size_t)(extension - base);
    char* name = malloc(stem_size + sizeof "_toy");
    if (!name) {
        fs_error(fs, "out of memory naming container %s", path);
        *status = TOYFILE_OUT_OF_MEMORY;
        return NULL;
    }
    for (size_t i = 0; i < stem_size; i++) {
        const unsigned char c = (unsigned char)base[i];
        if (c == '\\') {
            fs_error(fs, "unsafe container filename %s", base);
            free(name);
            return NULL;
        }
        name[i] = c == ' ' ? '_' :
                  c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : (char)c;
    }
    memcpy(name + stem_size, "_toy", sizeof "_toy");
    *status = TOYFILE_OK;
    return name;
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

toyfile_status toyfile_extract_container(const toyfile* file,
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

    cJSON* manifest = cJSON_Parse(toyfile_manifest_json(file));
    if (!manifest) {
        fs_error(&fs, "cannot parse decoded container manifest");
        return TOYFILE_OUT_OF_MEMORY;
    }
    cJSON* toys = cJSON_DetachItemFromObjectCaseSensitive(manifest, "toys");
    const cJSON* properties = cJSON_GetObjectItemCaseSensitive(
        manifest, "properties");
    const cJSON* icons = cJSON_GetObjectItemCaseSensitive(manifest, "icons");
    if (!cJSON_IsObject(properties) || !cJSON_IsArray(icons) ||
        !cJSON_IsArray(toys)) {
        fs_error(&fs, "decoded manifest has an invalid root");
        cJSON_Delete(toys);
        cJSON_Delete(manifest);
        return TOYFILE_INVALID_FORMAT;
    }

    const int count = cJSON_GetArraySize(toys);
    char** names = calloc((size_t)count ? (size_t)count : 1, sizeof(*names));
    if (!names) {
        fs_error(&fs, "out of memory indexing CToy definitions");
        cJSON_Delete(toys);
        cJSON_Delete(manifest);
        return TOYFILE_OUT_OF_MEMORY;
    }
    toyfile_status status = TOYFILE_OK;
    for (int i = 0; i < count; i++) {
        const cJSON* toy = cJSON_GetArrayItem(toys, i);
        const cJSON* id = cJSON_GetObjectItemCaseSensitive(toy, "id");
        if (!cJSON_IsObject(toy) || !cJSON_IsString(id) ||
            !safe_def_id(id->valuestring)) {
            fs_error(&fs, "unsafe or missing CToy id at index %d", i);
            status = TOYFILE_INVALID_FORMAT;
            break;
        }
        names[i] = def_filename(id->valuestring, &fs);
        if (!names[i]) {
            status = TOYFILE_OUT_OF_MEMORY;
            break;
        }
        for (int j = 0; j < i; j++) {
            if (strcmp(names[i], names[j]) == 0) {
                fs_error(&fs, "duplicate CToy output name %s", names[i]);
                status = TOYFILE_INVALID_FORMAT;
                break;
            }
        }
        if (status != TOYFILE_OK) {
            break;
        }
    }

    char* defs = NULL;
    char* resources = NULL;
    char* path = NULL;
    if (status == TOYFILE_OK && !make_directory_tree(directory, &fs)) {
        status = TOYFILE_IO_ERROR;
    }
    if (status == TOYFILE_OK &&
        (!(defs = child_path(directory, "defs", &fs)) ||
         !(resources = child_path(directory, "resources", &fs)))) {
        status = TOYFILE_OUT_OF_MEMORY;
    }
    if (status == TOYFILE_OK &&
        (!make_directory(defs, false, &fs) ||
         !make_directory(resources, false, &fs))) {
        status = TOYFILE_IO_ERROR;
    }
    if (status == TOYFILE_OK) {
        path = child_path(directory, "manifest.json", &fs);
        if (!path) {
            status = TOYFILE_OUT_OF_MEMORY;
        } else if (!write_json(path, manifest, &fs)) {
            status = TOYFILE_IO_ERROR;
        }
        free(path);
        path = NULL;
    }
    for (int i = 0; status == TOYFILE_OK && i < count; i++) {
        path = child_path(defs, names[i], &fs);
        if (!path) {
            status = TOYFILE_OUT_OF_MEMORY;
        } else if (!write_json(path, cJSON_GetArrayItem(toys, i), &fs)) {
            status = TOYFILE_IO_ERROR;
        }
        free(path);
        path = NULL;
    }
    if (status == TOYFILE_OK) {
        status = toyfile_extract_resources(file, resources, error, error_size);
    }

    free(path);
    free(resources);
    free(defs);
    for (int i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);
    cJSON_Delete(toys);
    cJSON_Delete(manifest);
    return status;
}

toyfile_status toyfile_install_into_assets(const toyfile_input* inputs,
                                           size_t count,
                                           const char* assets_root,
                                           bool* assets_root_created,
                                           char* error, size_t error_size) {
    fs_context fs = {error, error_size};
    if (assets_root_created) {
        *assets_root_created = false;
    }
    if (fs.error && fs.error_size) {
        error[0] = 0;
    }
    if (!inputs || count == 0 || !assets_root || assets_root[0] != '/') {
        fs_error(&fs, "missing inputs or absolute assets root");
        return TOYFILE_INVALID_ARGUMENT;
    }

    char* target = strdup(assets_root);
    if (!target) {
        fs_error(&fs, "out of memory preparing assets root");
        return TOYFILE_OUT_OF_MEMORY;
    }
    size_t target_size = strlen(target);
    while (target_size > 1 && target[target_size - 1] == '/') {
        target[--target_size] = 0;
    }
    char* separator = strrchr(target, '/');
    if (target_size <= 1 || !separator || !separator[1]) {
        fs_error(&fs, "unsafe assets root %s", assets_root);
        free(target);
        return TOYFILE_INVALID_ARGUMENT;
    }
    char** names = calloc(count, sizeof(*names));
    if (!names) {
        fs_error(&fs, "out of memory indexing containers");
        free(target);
        return TOYFILE_OUT_OF_MEMORY;
    }
    toyfile_status status = TOYFILE_OK;
    for (size_t i = 0; i < count; i++) {
        names[i] = container_name_from_path(inputs[i].name, &fs, &status);
        if (!names[i]) {
            break;
        }
        for (size_t j = 0; j < i; j++) {
            if (strcmp(names[i], names[j]) == 0) {
                fs_error(&fs, "duplicate container name %s", names[i]);
                status = TOYFILE_INVALID_ARGUMENT;
                break;
            }
        }
        if (status != TOYFILE_OK) {
            break;
        }
    }

    const size_t parent_size = separator == target
                             ? 1 : (size_t)(separator - target);
    char* parent = malloc(parent_size + 1);
    if (!parent) {
        fs_error(&fs, "out of memory preparing assets parent");
        status = TOYFILE_OUT_OF_MEMORY;
    } else {
        memcpy(parent, target, parent_size);
        parent[parent_size] = 0;
    }
    if (status == TOYFILE_OK && !make_directory_tree(parent, &fs)) {
        status = TOYFILE_IO_ERROR;
    }
    if (status == TOYFILE_OK) {
        if (mkdir(target, 0777) != 0) {
            fs_error(&fs, "cannot create assets root %s: %s", target,
                     strerror(errno));
            status = TOYFILE_IO_ERROR;
        } else if (assets_root_created) {
            *assets_root_created = true;
        }
    }

    for (size_t i = 0; status == TOYFILE_OK && i < count; i++) {
        toyfile* file = NULL;
        status = toyfile_open_memory(inputs[i].data, inputs[i].size, &file);
        if (status != TOYFILE_OK) {
            char detail[320];
            snprintf(detail, sizeof detail, "%s", toyfile_error(file));
            fs_error(&fs, "cannot import %s: %s", inputs[i].name, detail);
            toyfile_close(file);
            break;
        }
        char* output = child_path(target, names[i], &fs);
        if (!output) {
            status = TOYFILE_OUT_OF_MEMORY;
        } else {
            status = toyfile_extract_container(file, output,
                                               error, error_size);
        }
        free(output);
        toyfile_close(file);
    }
    free(parent);
    for (size_t i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);
    free(target);
    return status;
}
