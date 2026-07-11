#include "toydefs.h"
#include "physics.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DEFS 256

static toydef_t defs[MAX_DEFS];
static int ndefs;

static float num(const cJSON* obj, const char* key, float fallback) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(v) ? (float)v->valuedouble : fallback;
}

bool toydefs_load(const char* json_path) {
    FILE* f = fopen(json_path, "rb");
    if (!f) {
        return false;
    }
    fseek(f, 0, SEEK_END);
    const long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc((size_t)len + 1);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf);
        fclose(f);
        return false;
    }
    fclose(f);
    buf[len] = 0;

    cJSON* root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        return false;
    }
    const cJSON* toys = cJSON_GetObjectItemCaseSensitive(root, "toys");
    const cJSON* toy;
    cJSON_ArrayForEach(toy, toys) {
        if (ndefs >= MAX_DEFS) {
            break;
        }
        const cJSON* limbs = cJSON_GetObjectItemCaseSensitive(toy, "limbs");
        const cJSON* limb = cJSON_GetArrayItem(limbs, 0);
        if (!limb) {
            continue;
        }
        toydef_t* d = &defs[ndefs];
        d->class_name = strdup(toy->string);
        d->mass = num(limb, "mass", 1.0f);
        d->inertia_tensor = num(limb, "inertiaTensor", 1.0f);
        const cJSON* g = cJSON_GetObjectItemCaseSensitive(limb, "gravityOverride");
        d->has_gravity_override = cJSON_IsNumber(g);
        d->gravity_override = d->has_gravity_override ? (float)g->valuedouble : 0.0f;
        d->mouse_stiffness = num(limb, "mouseStiffnessOverride", PHYS_MOUSE_STIFFNESS);
        d->mouse_dampener = num(limb, "mouseDampenerOverride", PHYS_MOUSE_DAMPENER);
        d->air_resistance_linear = num(limb, "airResistanceLinear", 0.0f);
        d->air_resistance_angular = num(limb, "airResistanceAngular", 0.0f);
        d->fixed_move = num(limb, "fixedMove", 0) != 0.0f;
        d->fixed_rotate = num(limb, "fixedRotate", 0) != 0.0f;
        const cJSON* mat = cJSON_GetObjectItemCaseSensitive(limb, "material");
        static const char* mkeys[5] = { "velocityResponse", "stiffness",
                                        "dampener", "kineticFriction", "staticFriction" };
        for (int i = 0; i < 5; i++) {
            d->material[i] = num(mat, mkeys[i], 0.0f);
        }
        ndefs++;
    }
    cJSON_Delete(root);
    return ndefs > 0;
}

const toydef_t* toydefs_find(const char* class_name) {
    for (int i = 0; i < ndefs; i++) {
        if (strcmp(defs[i].class_name, class_name) == 0) {
            return &defs[i];
        }
    }
    return NULL;
}

int toydefs_count(void) {
    return ndefs;
}
