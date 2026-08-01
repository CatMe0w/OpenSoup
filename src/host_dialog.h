#pragma once
#include <stdbool.h>
#include <stddef.h>

// Modal file dialogs. The platform main registers its implementation before
// the event loop starts; a host that registers none simply cannot open.
// Returns false on cancel. `directory` may be NULL.
typedef bool (*host_dialog_open_file_fn)(const char* title,
                                         const char* directory,
                                         const char* kind,
                                         const char* extension,
                                         char* out, size_t out_size,
                                         void* user);

void host_dialog_set_open_file(host_dialog_open_file_fn fn, void* user);
bool host_dialog_can_open_file(void);
bool host_dialog_open_file(const char* title, const char* directory,
                           const char* kind, const char* extension,
                           char* out, size_t out_size);
