#ifndef OPENSOUP_APP_PATHS_H
#define OPENSOUP_APP_PATHS_H

// Returns the fixed per-user assets root, or NULL if macOS cannot resolve
// the user's Application Support directory. The returned pointer remains
// valid for the lifetime of the process.
const char* macos_assets_root(void);

#endif
