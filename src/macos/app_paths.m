#import <Foundation/Foundation.h>

#include "app_paths.h"
#include "nsis.h"
#include "toyfile_fs.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static NSURL* assets_root_url(void) {
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

const char* app_assets_root(void) {
    static char assets_root[PATH_MAX];

    @autoreleasepool {
        NSURL* root = assets_root_url();
        if (!root) {
            return NULL;
        }

        const char* path = root.fileSystemRepresentation;
        if (!path || strlcpy(assets_root, path, sizeof assets_root)
                     >= sizeof assets_root) {
            return NULL;
        }
    }

    return assets_root;
}

app_assets_state app_assets_get_state(void) {
    @autoreleasepool {
        NSURL* root = assets_root_url();
        if (!root) {
            return APP_ASSETS_DIRECTORY_MISSING;
        }

        NSFileManager* files = [NSFileManager defaultManager];
        BOOL is_directory = NO;
        if (![files fileExistsAtPath:root.path isDirectory:&is_directory]) {
            return APP_ASSETS_DIRECTORY_MISSING;
        }
        if (!is_directory) {
            return APP_ASSETS_CORE_MISSING;
        }

        NSURL* core = [[root
            URLByAppendingPathComponent:@"souptoys_core_toy" isDirectory:YES]
            URLByAppendingPathComponent:@"resources" isDirectory:YES];
        if (![files fileExistsAtPath:core.path isDirectory:&is_directory]
            || !is_directory) {
            return APP_ASSETS_CORE_MISSING;
        }
    }

    return APP_ASSETS_READY;
}

static bool install_assets_from_decoded_toys(
        const nsis_container* containers, size_t count,
        char* error, size_t error_size) {
    const char* root = app_assets_root();
    if (!root) {
        if (error && error_size) {
            snprintf(error, error_size, "cannot resolve the assets root");
        }
        return false;
    }

    struct stat info;
    if (lstat(root, &info) == 0) {
        if (error && error_size) {
            snprintf(error, error_size, "assets root already exists: %s", root);
        }
        return false;
    }
    if (errno != ENOENT) {
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
        inputs, count, root, error, error_size);
    free(inputs);
    if (status == TOYFILE_OK) {
        return true;
    }

    if (lstat(root, &info) == 0) {
        @autoreleasepool {
            NSError* rollback_error = nil;
            NSURL* url = [NSURL fileURLWithFileSystemRepresentation:root
                                                        isDirectory:YES
                                                      relativeToURL:nil];
            if (![[NSFileManager defaultManager] removeItemAtURL:url
                                                           error:&rollback_error]
                && error && error_size) {
                const char* reason = rollback_error.localizedDescription.UTF8String;
                strlcat(error, error[0] ? "; rollback failed: "
                                        : "rollback failed: ", error_size);
                strlcat(error, reason ? reason : "unknown error", error_size);
            }
        }
    }
    return false;
}

bool app_assets_install_from_installer(const char* path,
                                       char* error, size_t error_size) {
    if (error && error_size) {
        error[0] = 0;
    }
    if (!path || !path[0]) {
        if (error && error_size) {
            snprintf(error, error_size, "missing installer path");
        }
        return false;
    }

    return nsis_decode_containers(
        path, NSIS_CONTAINER_TOY, install_assets_from_decoded_toys,
        error, error_size);
}
