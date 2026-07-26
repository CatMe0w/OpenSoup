#include "app_paths.h"

#include <stdio.h>

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0); // keep diagnostics visible when piped
    const char* assets_root = linux_assets_root();
    if (!assets_root) {
        fprintf(stderr, "cannot resolve the assets path\n");
        return 1;
    }

    puts(assets_root);
    return 0;
}
