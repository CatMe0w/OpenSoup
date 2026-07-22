#import <Foundation/Foundation.h>

#include "app_paths.h"

#include <limits.h>
#include <string.h>

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
        if (![files fileExistsAtPath:root.path isDirectory:&is_directory]
            || !is_directory) {
            return APP_ASSETS_DIRECTORY_MISSING;
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

bool app_assets_create_directory(void) {
    @autoreleasepool {
        NSURL* root = assets_root_url();
        if (!root) {
            return false;
        }

        return [[NSFileManager defaultManager]
            createDirectoryAtURL:root
     withIntermediateDirectories:YES
                      attributes:nil
                           error:nil];
    }
}
