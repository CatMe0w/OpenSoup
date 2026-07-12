// Demo bootstrap: hardcoded toy picks, instantiated through the Ruby
// framework (ToyClassResolver -> Toy.new -> toys <<). The C side here only
// supplies the visual half of realization: FLC loading into the scene.
// The toybox menu UI replaces the hardcoded picks later.
#include "demo.h"
#include "assets.h"
#include "rubyhost.h"
#include "scene.h"
#include "toydefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* join(const char* root, const char* rel) {
    const size_t n = strlen(root) + strlen(rel) + 2;
    char* p = malloc(n);
    snprintf(p, n, "%s/%s", root, rel);
    return p;
}

// Souptoys ships transparency as "<name>_Alpha.flc" next to the color FLC
static char* alpha_variant(const char* color_path) {
    const char* dot = strrchr(color_path, '.');
    if (!dot) {
        return NULL;
    }
    const size_t base = (size_t)(dot - color_path);
    char* p = malloc(base + sizeof("_Alpha") + strlen(dot));
    memcpy(p, color_path, base);
    strcpy(p + base, "_Alpha");
    strcat(p, dot);
    FILE* f = fopen(p, "rb");
    if (!f) {
        free(p);
        return NULL;
    }
    fclose(f);
    return p;
}

// Visual half of Ruby-side toy realization: load the sprite's FLC and hand
// it to the scene. group = toy instance id, offset to its own range.
static int ruby_sprite_hook(const char* image, int body, float com_x,
                            float com_y, int group, void* user) {
    const char* assets_root = user;
    char* cpath = join(assets_root, image);
    int sprite = -1;
    // leaked on purpose: scene borrows the pixel buffers for hit-testing
    const char* dot = strrchr(cpath, '.');
    if (dot && strcmp(dot, ".flc") == 0) {
        char* apath = alpha_variant(cpath);
        as_anim* anim = calloc(1, sizeof(as_anim));
        if (as_load_flc(cpath, apath, anim)) {
            sprite = scene_sprite_add(anim->w, anim->h, anim->frame_count,
                                      anim->frames, anim->speed_ms,
                                      0, 0, 1000 + group);
        }
        free(apath);
    } else {
        // extensionless = single-frame TGA: "<name>0000.tga"
        char* tpath = malloc(strlen(cpath) + sizeof("0000.tga"));
        strcpy(tpath, cpath);
        strcat(tpath, "0000.tga");
        as_image* img = calloc(1, sizeof(as_image));
        if (as_load_tga(tpath, img)) {
            uint8_t** frames = malloc(sizeof(uint8_t*));
            frames[0] = img->rgba;
            sprite = scene_sprite_add(img->w, img->h, 1, frames, 0,
                                      0, 0, 1000 + group);
        }
        free(tpath);
    }
    if (sprite >= 0) {
        scene_sprite_bind_body(sprite, body, com_x, com_y);
    } else {
        fprintf(stderr, "demo: FAILED %s\n", cpath);
    }
    free(cpath);
    return sprite;
}

static void ruby_sprite_remove_hook(int sprite, void* user) {
    (void)user;
    scene_sprite_remove(sprite);
}

int demo_load(const char* assets_root) {
    char* defs = join(assets_root, "toydefs.json");
    if (toydefs_load(defs)) {
        printf("demo: %d toy defs loaded\n", toydefs_count());
    } else {
        fprintf(stderr, "demo: toydefs.json missing at %s\n", defs);
        free(defs);
        return 0;
    }
    free(defs);

    // every toy goes through the Ruby framework now: ToyClassResolver ->
    // Toy.new -> move -> toys <<, realized into physics + scene on add.
    // The def's `root` names the container dir holding any toy script.
    rbh_set_sprite_hook(ruby_sprite_hook, ruby_sprite_remove_hook,
                        (void*)assets_root);
    static const struct { const char* name; float x, y; } picks[] = {
        { "PirateWind", 3.0f, 4.0f },  // fixed scenery: anchored, gravity 0
        { "U1Bouncy", 6.0f, 6.0f },    // bouncy ball
        { "BalloonBlue", 9.0f, 5.0f }, // POSITIVE gravity override, floats
        { "U6Bluebear", 12.0f, 5.0f },     // 6 limbs, 11 spring joints
        { "Goose", 15.0f, 3.0f },          // scripted: rocks, lays eggs
        { "SnowballCannon", 18.0f, 1.0f }, // scripted: click fires snowballs
    };
    // classes referenced by other toys' scripts must resolve before they run
    // (SnowballCannon#shoot does SnowballLarge.new); toy import will do this
    // wholesale later
    static const char* preload[] = { "GooseEgg", "SnowballLarge",
                                     "SnowballSmall" };
    for (size_t i = 0; i < sizeof preload / sizeof preload[0]; i++) {
        const toydef_t* d = toydefs_find(preload[i]);
        if (d) {
            char* dir = join(assets_root, d->root);
            rbh_load_toy_class(preload[i], dir);
            free(dir);
        }
    }

    int n = 0;
    for (size_t i = 0; i < sizeof picks / sizeof picks[0]; i++) {
        const toydef_t* d = toydefs_find(picks[i].name);
        char* dir = join(assets_root, d ? d->root : ".");
        if (rbh_spawn_toy(picks[i].name, dir, picks[i].x, picks[i].y)) {
            printf("demo: %s spawned via Ruby\n", picks[i].name);
            n++;
        }
        free(dir);
    }
    return n;
}
