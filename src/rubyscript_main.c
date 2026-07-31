// Headless Ruby script runner.
// XXX: merge this into main app
#include "rubyhost.h"
#include "assets_layout.h"
#include "audio.h"
#include "physics.h"
#include "toydefs.h"
#include <stdio.h>
#include <stdlib.h>

static char* read_all(FILE* f) {
    size_t cap = 8192, len = 0;
    char* buf = malloc(cap);
    if (!buf) return NULL;
    for (;;) {
        if (len + 1 >= cap) {
            char* grown = realloc(buf, cap *= 2);
            if (!grown) {
                free(buf);
                return NULL;
            }
            buf = grown;
        }
        const size_t n = fread(buf + len, 1, cap - len - 1, f);
        if (n == 0) break;
        len += n;
    }
    buf[len] = '\0';
    return buf;
}

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: %s <assets-root> [script.rb]\n", argv[0]);
        return 2;
    }
    const char* assets = argv[1];

    // Toy realization touches the Sound container, so a run needs an audio
    // device; a null one keeps it silent and headless.
    if (!audio_init(true)) {
        fprintf(stderr, "rubyscript: audio init failed\n");
        return 1;
    }
    if (!toydefs_load(assets)) {
        fprintf(stderr, "rubyscript: cannot load toy defs under %s\n", assets);
        return 1;
    }
    if (!rbh_boot(assets)) {
        fprintf(stderr, "rubyscript: framework boot failed\n");
        return 1;
    }
    for (int i = 0; i < toydefs_count(); i++) {
        const toydef_t* def = toydefs_at(i);
        char dir[1200];
        container_resource_root(dir, sizeof dir, assets, def->root);
        if (!rbh_load_toy_class(def->class_name, dir)) {
            fprintf(stderr, "rubyscript: cannot load toy class %s\n",
                    def->class_name);
            return 1;
        }
    }
    if (!rbh_catalog_finalize()) {
        fprintf(stderr, "rubyscript: catalog finalize failed\n");
        return 1;
    }

    FILE* in = stdin;
    if (argc == 3 && !(in = fopen(argv[2], "rb"))) {
        perror(argv[2]);
        return 1;
    }
    char* text = read_all(in);
    if (in != stdin) fclose(in);
    if (!text) {
        fprintf(stderr, "rubyscript: out of memory reading the script\n");
        return 1;
    }

    const bool ok = rbh_eval(text, argc == 3 ? argv[2] : "stdin");
    free(text);

    // OPENSOUP_DUMP=<prefix>: write scene + replay dumps.
    const char* dump = getenv("OPENSOUP_DUMP");
    if (dump) {
        char path[1024];
        // Replay first: it captures pre-step state.
        snprintf(path, sizeof path, "%s.rb", dump);
        FILE* f = fopen(path, "w");
        if (f) {
            rbh_dump_replay(f);
            fclose(f);
        } else {
            perror(path);
        }
        snprintf(path, sizeof path, "%s.txt", dump);
        f = fopen(path, "w");
        if (f) {
            rbh_dump_scene(f);
            phys_debug_dump(f);
            phys_debug_capture_begin(f);
            rbh_eval("$default_engine.run_steps(1)", "dump step");
            phys_debug_capture_end();
            fclose(f);
        } else {
            perror(path);
        }
    }

    fflush(stdout);
    rbh_shutdown();
    audio_shutdown();
    return ok ? 0 : 1;
}
