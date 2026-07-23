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

const char* macos_assets_root(void) {
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
