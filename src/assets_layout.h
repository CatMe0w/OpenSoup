#pragma once
#include <stdio.h>

// The extracted-assets tree convention, in one place. Each container
// (<name>_toy / <name>_playset dir under the assets root) is a self-contained
// unit holding the decoded parts of that one original container:
//   assets/<container>/manifest.json          properties + icon catalog
//   assets/<container>/defs/<classname>.json  decoded CToy records
//   assets/<container>/resources/...          resource VFS
static inline int container_resource_root(char* out, size_t cap,
                                          const char* assets_root,
                                          const char* container) {
    return snprintf(out, cap, "%s/%s/resources", assets_root, container);
}
