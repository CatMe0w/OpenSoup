
#include <stdio.h>

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0); // keep diagnostics visible when piped
    const char* assets_root = app_assets_path();
    if (!assets_root) {
        fprintf(stderr, "cannot resolve the assets path\n");
        return 1;
    }

    puts(assets_root);
    return 0;
}
