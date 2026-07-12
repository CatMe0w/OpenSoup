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

static float idx(const cJSON* arr, int i, float fallback) {
    const cJSON* v = cJSON_GetArrayItem(arr, i);
    return cJSON_IsNumber(v) ? (float)v->valuedouble : fallback;
}

static char* dupstr(const cJSON* obj, const char* key) {
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(v) ? strdup(v->valuestring) : NULL;
}

static void parse_limb(td_limb* l, const cJSON* limb) {
    l->name = dupstr(limb, "name");
    const cJSON* bs = cJSON_GetObjectItemCaseSensitive(limb, "bodyState");
    l->rest_pos[0] = idx(bs, 0, 0.0f);
    l->rest_pos[1] = idx(bs, 1, 0.0f);
    l->rest_orient = idx(bs, 2, 0.0f);
    l->mass = num(limb, "mass", 1.0f);
    l->inertia = num(limb, "inertiaTensor", 1.0f);
    const cJSON* g = cJSON_GetObjectItemCaseSensitive(limb, "gravityOverride");
    l->has_gravity_override = cJSON_IsNumber(g);
    l->gravity_override = l->has_gravity_override ? (float)g->valuedouble : 0.0f;
    l->mouse_stiffness = num(limb, "mouseStiffnessOverride", PHYS_MOUSE_STIFFNESS);
    l->mouse_dampener = num(limb, "mouseDampenerOverride", PHYS_MOUSE_DAMPENER);
    l->air_resistance_linear = num(limb, "airResistanceLinear", 0.0f);
    l->air_resistance_angular = num(limb, "airResistanceAngular", 0.0f);
    l->fixed_move = num(limb, "fixedMove", 0) != 0.0f;
    l->fixed_rotate = num(limb, "fixedRotate", 0) != 0.0f;
    l->default_grab_move = num(limb, "defaultGrabMove", 1) != 0.0f;
    l->default_grab_rotate = num(limb, "defaultGrabRotate", 1) != 0.0f;
    const cJSON* mat = cJSON_GetObjectItemCaseSensitive(limb, "material");
    static const char* mkeys[5] = { "velocityResponse", "stiffness",
                                    "dampener", "kineticFriction", "staticFriction" };
    for (int i = 0; i < 5; i++) {
        l->material[i] = num(mat, mkeys[i], 0.0f);
    }

    const cJSON* shapes = cJSON_GetObjectItemCaseSensitive(limb, "shapes");
    l->nshapes = cJSON_GetArraySize(shapes);
    l->shapes = calloc((size_t)l->nshapes ? (size_t)l->nshapes : 1, sizeof(td_shape));
    int si = 0;
    const cJSON* shape;
    cJSON_ArrayForEach(shape, shapes) {
        td_shape* s = &l->shapes[si++];
        const cJSON* member = cJSON_GetObjectItemCaseSensitive(shape, "memberOf");
        s->collides = cJSON_GetArraySize(member) > 0;
        static const char* walls[4] = { "left_wall", "right_wall", "floor", "ceiling" };
        const cJSON* g;
        cJSON_ArrayForEach(g, member) {
            if (!cJSON_IsString(g)) {
                continue;
            }
            for (int w = 0; w < 4; w++) {
                const size_t n = strlen(walls[w]);
                if (strncmp(g->valuestring, walls[w], n) != 0) {
                    continue;
                }
                if (strcmp(g->valuestring + n, "_repel") == 0) {
                    s->wall_repel |= (unsigned char)(1 << w);
                } else if (strcmp(g->valuestring + n, "_rotate") == 0) {
                    s->wall_rotate |= (unsigned char)(1 << w);
                }
            }
        }
        s->grab = num(shape, "grab", 0) != 0.0f;
        const cJSON* pts = cJSON_GetObjectItemCaseSensitive(shape, "points");
        s->npoints = cJSON_GetArraySize(pts);
        s->points = calloc((size_t)s->npoints ? (size_t)s->npoints : 1, sizeof(td_point));
        int pi = 0;
        const cJSON* pt;
        cJSON_ArrayForEach(pt, pts) {
            s->points[pi].x = idx(pt, 0, 0.0f);
            s->points[pi].y = idx(pt, 1, 0.0f);
            s->points[pi].r = idx(pt, 2, 0.0f);
            pi++;
        }
    }

    const cJSON* sprites = cJSON_GetObjectItemCaseSensitive(limb, "sprites");
    l->nsprites = cJSON_GetArraySize(sprites);
    l->sprites = calloc((size_t)l->nsprites ? (size_t)l->nsprites : 1, sizeof(td_sprite));
    int pi = 0;
    const cJSON* sp;
    cJSON_ArrayForEach(sp, sprites) {
        td_sprite* d = &l->sprites[pi++];
        d->image = dupstr(sp, "image");
        d->num_frames = (int)num(sp, "numFrames", 1);
        const cJSON* com = cJSON_GetObjectItemCaseSensitive(sp, "objectCentreOfMass");
        d->com[0] = idx(com, 0, 0.0f);
        d->com[1] = idx(com, 1, 0.0f);
        d->z_order = (int)num(sp, "zOrder", 0);
    }

    // motors fold to constant per-limb force/torque; the on/off scripting
    // (Ruby motor.force=) comes with the runtime milestone
    const cJSON* lm = cJSON_GetObjectItemCaseSensitive(limb, "linearMotors");
    const cJSON* m;
    cJSON_ArrayForEach(m, lm) {
        const cJSON* f = cJSON_GetObjectItemCaseSensitive(m, "force");
        l->motor_force[0] += idx(f, 0, 0.0f);
        l->motor_force[1] += idx(f, 1, 0.0f);
    }
    const cJSON* rm = cJSON_GetObjectItemCaseSensitive(limb, "rotationalMotors");
    cJSON_ArrayForEach(m, rm) {
        l->motor_torque += num(m, "torque", 0.0f);
    }
}

static int limb_index(const toydef_t* d, const char* name) {
    for (int i = 0; name && i < d->nlimbs; i++) {
        if (d->limbs[i].name && strcmp(d->limbs[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

bool toydefs_load(const char* json_path) {
    if (ndefs > 0) {
        return true; // already loaded (main boots Ruby before demo_load)
    }
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
        const int nlimbs = cJSON_GetArraySize(limbs);
        if (nlimbs < 1) {
            continue;
        }
        toydef_t* d = &defs[ndefs++];
        d->class_name = strdup(toy->string);
        d->root = dupstr(toy, "root");
        d->base_scale = num(toy, "baseScale", 1.0f);
        d->nlimbs = nlimbs;
        d->limbs = calloc((size_t)nlimbs, sizeof(td_limb));
        int li = 0;
        const cJSON* limb;
        cJSON_ArrayForEach(limb, limbs) {
            parse_limb(&d->limbs[li++], limb);
        }

        const cJSON* joints = cJSON_GetObjectItemCaseSensitive(toy, "joints");
        d->njoints = cJSON_GetArraySize(joints);
        d->joints = calloc((size_t)d->njoints ? (size_t)d->njoints : 1, sizeof(td_joint));
        int ji = 0;
        const cJSON* joint;
        cJSON_ArrayForEach(joint, joints) {
            td_joint* j = &d->joints[ji++];
            char* n1 = dupstr(joint, "limb1");
            char* n2 = dupstr(joint, "limb2");
            j->limb1 = limb_index(d, n1);
            j->limb2 = limb_index(d, n2);
            free(n1);
            free(n2);
            const cJSON* a1 = cJSON_GetObjectItemCaseSensitive(joint, "anchor1");
            const cJSON* a2 = cJSON_GetObjectItemCaseSensitive(joint, "anchor2");
            j->anchor1[0] = idx(a1, 0, 0.0f);
            j->anchor1[1] = idx(a1, 1, 0.0f);
            j->anchor2[0] = idx(a2, 0, 0.0f);
            j->anchor2[1] = idx(a2, 1, 0.0f);
            j->rest_length = num(joint, "restLength", 0.0f);
            j->stiffness = num(joint, "stiffness", 0.0f);
            j->dampener = num(joint, "dampener", 0.0f);
        }
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
