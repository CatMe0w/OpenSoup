#pragma once
#include <stdio.h>

// The extracted-assets tree convention, in one place. Each container
// (<name>_toy / <name>_playset dir under the assets root) is a self-contained
// unit holding two parallel subtrees:
//   assets/<container>/defs/<classname>.json  decoded CToy defs (+ icon)
//   assets/<container>/resources/...          resource VFS
// Cross-container pack metadata lives at assets/packs.json.
static inline int container_resource_root(char* out, size_t cap,
                                          const char* assets_root,
                                          const char* container) {
    return snprintf(out, cap, "%s/%s/resources", assets_root, container);
}
