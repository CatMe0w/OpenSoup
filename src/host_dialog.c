#include "host_dialog.h"

static host_dialog_open_file_fn g_open_file;
static void* g_open_file_user;

void host_dialog_set_open_file(host_dialog_open_file_fn fn, void* user) {
    g_open_file = fn;
    g_open_file_user = user;
}

bool host_dialog_can_open_file(void) {
    return g_open_file != NULL;
}

bool host_dialog_open_file(const char* title, const char* directory,
                           const char* kind, const char* extension,
                           char* out, size_t out_size) {
    if (!g_open_file || !out || out_size == 0) {
        return false;
    }
    out[0] = 0;
    return g_open_file(title, directory, kind, extension, out, out_size,
                       g_open_file_user);
}
