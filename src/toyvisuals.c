// Visual bridge from Ruby-hosted toys to the native scene.
#include "toyvisuals.h"
#include "assets.h"
#include "rubyhost.h"
#include "scene.h"
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

typedef struct cached_visual {
    char* path;
    int w, h, nframes, speed_ms;
    uint8_t** frames;
    struct cached_visual* next;
} cached_visual;

static cached_visual* visual_cache;

// Toy instances frequently reuse the same FLC. Decode it once and keep one
// stable frame-array identity so scene.c can share the corresponding GPU
// textures across every instance.
static cached_visual* visual_load(const char* path) {
    for (cached_visual* v = visual_cache; v; v = v->next) {
        if (strcmp(v->path, path) == 0) {
            return v;
        }
    }
    cached_visual* v = calloc(1, sizeof(*v));
    if (!v) {
        return NULL;
    }
    v->path = strdup(path);
    if (!v->path) {
        free(v);
        return NULL;
    }
    const char* dot = strrchr(path, '.');
    if (dot && strcmp(dot, ".flc") == 0) {
        char* apath = alpha_variant(path);
        as_anim anim = {0};
        const bool ok = as_load_flc(path, apath, &anim);
        free(apath);
        if (!ok) {
            free(v->path);
            free(v);
            return NULL;
        }
        v->w = anim.w;
        v->h = anim.h;
        v->nframes = anim.frame_count;
        v->speed_ms = anim.speed_ms;
        v->frames = anim.frames;
    } else {
        char* tpath = malloc(strlen(path) + sizeof("0000.tga"));
        if (!tpath) {
            free(v->path);
            free(v);
            return NULL;
        }
        strcpy(tpath, path);
        strcat(tpath, "0000.tga");
        as_image image = {0};
        const bool ok = as_load_tga(tpath, &image);
        free(tpath);
        if (!ok) {
            free(v->path);
            free(v);
            return NULL;
        }
        v->frames = malloc(sizeof(*v->frames));
        if (!v->frames) {
            as_image_free(&image);
            free(v->path);
            free(v);
            return NULL;
        }
        v->frames[0] = image.rgba;
        v->w = image.w;
        v->h = image.h;
        v->nframes = 1;
    }
    v->next = visual_cache;
    visual_cache = v;
    return v;
}

// Visual half of Ruby-side toy realization: load the sprite's FLC and hand
// it to the scene. group = toy instance id.
static int ruby_sprite_hook(const char* image, int body, float com_x,
                            float com_y, int group, void* user) {
    const char* assets_root = user;
    char* cpath = join(assets_root, image);
    int sprite = -1;
    cached_visual* visual = visual_load(cpath);
    if (visual) {
        sprite = scene_sprite_add(visual->w, visual->h, visual->nframes,
                                  visual->frames, visual->speed_ms,
                                  0, 0, SCENE_GROUP_TOY(group));
    }
    if (sprite >= 0) {
        scene_sprite_bind_body(sprite, body, com_x, com_y);
    } else {
        fprintf(stderr, "toyvisuals: failed to load %s\n", cpath);
    }
    free(cpath);
    return sprite;
}

static void ruby_sprite_remove_hook(int sprite, void* user) {
    (void)user;
    scene_sprite_remove(sprite);
}

void toyvisuals_init(const char* assets_root) {
    rbh_set_sprite_hook(ruby_sprite_hook, ruby_sprite_remove_hook,
                        (void*)assets_root);
}
