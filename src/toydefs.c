#include "toydefs.h"
#include "physics.h"
#include "cJSON.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DEFS 256
#define MAX_ICONS 256
#define MAX_PACKS 32

static toydef_t defs[MAX_DEFS];
static int ndefs;
static toyicon_t icons[MAX_ICONS];
static int nicons;
static toypack_t packs[MAX_PACKS];
static int npacks;

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
    l->local_collision_group = dupstr(limb, "localCollisionGroup");
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
        s->sid = dupstr(shape, "sid");
        const cJSON* member = cJSON_GetObjectItemCaseSensitive(shape, "memberOf");
        s->nmembers = cJSON_GetArraySize(member);
        s->members = calloc((size_t)s->nmembers ? (size_t)s->nmembers : 1,
                            sizeof(char*));
        const cJSON* g;
        int gi = 0;
        cJSON_ArrayForEach(g, member) {
            if (!cJSON_IsString(g)) {
                continue;
            }
            s->members[gi++] = strdup(g->valuestring);
        }
        s->nmembers = gi;
        s->grab = num(shape, "grab", 0) != 0.0f;
        s->grab_move = num(shape, "grabMove", 0) != 0.0f;
        s->grab_rotate = num(shape, "grabRotate", 0) != 0.0f;
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
        d->sid = dupstr(sp, "sid");
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

static cJSON* parse_json_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    const long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc((size_t)len + 1);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    buf[len] = 0;
    cJSON* root = cJSON_Parse(buf);
    free(buf);
    return root;
}

static bool load_packs(const char* assets_root) {
    char path[1024];
    snprintf(path, sizeof path, "%s/packs.json", assets_root);
    cJSON* root = parse_json_file(path);
    if (!root) {
        return false;
    }
    const cJSON* pack;
    cJSON_ArrayForEach(pack, cJSON_GetObjectItemCaseSensitive(root, "packs")) {
        if (npacks >= MAX_PACKS) {
            break;
        }
        toypack_t* p = &packs[npacks++];
        p->id = dupstr(pack, "id");
        p->license = dupstr(pack, "license");
        p->header = dupstr(pack, "header");
        p->order = num(pack, "order", (float)npacks);
    }
    cJSON_Delete(root);
    return true;
}

// One <container>/defs/<classname>.json: a toy def with its (optional)
// Toybox icon catalog entry embedded.
static void load_toy_file(const char* path, const char* container) {
    cJSON* toy = parse_json_file(path);
    if (!toy) {
        fprintf(stderr, "toydefs: unreadable defs file %s\n", path);
        return;
    }
    char* class_name = dupstr(toy, "className");
    if (!class_name) {
        fprintf(stderr, "toydefs: no className in %s\n", path);
        cJSON_Delete(toy);
        return;
    }

    const cJSON* icon = cJSON_GetObjectItemCaseSensitive(toy, "icon");
    if (cJSON_IsObject(icon) && nicons < MAX_ICONS) {
        toyicon_t* i = &icons[nicons++];
        i->name = dupstr(icon, "name");
        i->class_name = strdup(class_name);
        i->image = dupstr(icon, "image");
        i->num_frames = (int)num(icon, "numFrames", 1.0f);
        i->instance_limit = (int)num(icon, "instanceLimit", 100.0f);
        i->pack_order = num(icon, "packOrder", 0.0f);
        i->order = num(icon, "order", 0.0f);
        i->catalog_index = (int)num(icon, "catalogIndex", (float)nicons);
    }

    const cJSON* limbs = cJSON_GetObjectItemCaseSensitive(toy, "limbs");
    const int nlimbs = cJSON_GetArraySize(limbs);
    if (nlimbs < 1 || ndefs >= MAX_DEFS) {
        free(class_name);
        cJSON_Delete(toy);
        return;
    }
    {
        toydef_t* d = &defs[ndefs++];
        d->class_name = class_name;
        d->root = strdup(container);
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

        const cJSON* rjs = cJSON_GetObjectItemCaseSensitive(toy, "rotationalJoints");
        d->nrotjoints = cJSON_GetArraySize(rjs);
        d->rotjoints = calloc((size_t)d->nrotjoints ? (size_t)d->nrotjoints : 1,
                              sizeof(td_rotjoint));
        int ri = 0;
        const cJSON* rj;
        cJSON_ArrayForEach(rj, rjs) {
            td_rotjoint* r = &d->rotjoints[ri++];
            char* n1 = dupstr(rj, "limb1");
            char* n2 = dupstr(rj, "limb2");
            r->limb1 = limb_index(d, n1);
            r->limb2 = limb_index(d, n2);
            free(n1);
            free(n2);
            r->orientation1 = num(rj, "orientation1", 0.0f);
            r->orientation2 = num(rj, "orientation2", 0.0f);
            const cJSON* fields = cJSON_GetObjectItemCaseSensitive(rj, "fields");
            r->rest = idx(fields, 0, 0.0f);
            r->stiffness = idx(fields, 1, 0.0f);
            r->dampener = idx(fields, 2, 0.0f);
        }

        const cJSON* sounds = cJSON_GetObjectItemCaseSensitive(toy, "sounds");
        d->nsounds = cJSON_GetArraySize(sounds);
        d->sounds = calloc((size_t)d->nsounds ? (size_t)d->nsounds : 1,
                           sizeof(td_sound));
        int si = 0;
        const cJSON* sound;
        cJSON_ArrayForEach(sound, sounds) {
            d->sounds[si].sid = dupstr(sound, "sid");
            d->sounds[si].location = dupstr(sound, "location");
            si++;
        }
    }
    cJSON_Delete(toy);
}

static bool has_suffix(const char* name, const char* suffix) {
    const size_t n = strlen(name);
    const size_t s = strlen(suffix);
    return n >= s && strcmp(name + n - s, suffix) == 0;
}

static int cmp_icon_catalog(const void* a, const void* b) {
    return ((const toyicon_t*)a)->catalog_index
         - ((const toyicon_t*)b)->catalog_index;
}

static void load_container(const char* assets_root, const char* container) {
    char dir[1024];
    snprintf(dir, sizeof dir, "%s/%s/defs", assets_root, container);
    struct dirent** entries = NULL;
    const int n = scandir(dir, &entries, NULL, alphasort);
    for (int i = 0; i < n; i++) {
        if (has_suffix(entries[i]->d_name, ".json")) {
            char path[1400];
            snprintf(path, sizeof path, "%s/%s", dir, entries[i]->d_name);
            load_toy_file(path, container);
        }
        free(entries[i]);
    }
    free(entries);
}

bool toydefs_load(const char* assets_root) {
    if (ndefs > 0) {
        return true; // already loaded by the application bootstrap
    }
    if (!load_packs(assets_root)) {
        return false;
    }
    struct dirent** entries = NULL;
    const int n = scandir(assets_root, &entries, NULL, alphasort);
    for (int i = 0; i < n; i++) {
        if (entries[i]->d_name[0] != '.') {
            load_container(assets_root, entries[i]->d_name);
        }
        free(entries[i]);
    }
    free(entries);
    qsort(icons, (size_t)nicons, sizeof icons[0], cmp_icon_catalog);
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

const toydef_t* toydefs_at(int index) {
    return index >= 0 && index < ndefs ? &defs[index] : NULL;
}

int toydefs_icon_count(void) {
    return nicons;
}

const toyicon_t* toydefs_icon_at(int index) {
    return index >= 0 && index < nicons ? &icons[index] : NULL;
}

int toydefs_pack_count(void) {
    return npacks;
}

const toypack_t* toydefs_pack_at(int index) {
    return index >= 0 && index < npacks ? &packs[index] : NULL;
}
