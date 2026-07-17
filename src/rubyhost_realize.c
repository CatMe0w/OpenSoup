// Realization + engine (re)parenting: toys entering/leaving the default
// engine gain/lose their physics bodies, joints, and scene sprites; the
// scene-sprite map resolves rendered sprites back to Ruby nodes for the
// sprite-addressed app entry points (mouse dispatch, recycle, clear).
#include "rubyhost.h"
#include "audio.h"
#include "physics.h"
#include "toyphys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rubyhost_internal.h"

static rbh_sprite_fn g_sprite_fn;
static void* g_sprite_user;

// scene sprite id -> Ruby Sprite node. Scene ids are stable and monotonic, so
// this side table grows with the largest issued id instead of imposing a
// second unrelated sprite ceiling.
static VALUE* g_scene_sprite;
static size_t g_scene_sprite_cap;

static bool scene_sprite_map_reserve(int id) {
    if (id < 0) return false;
    const size_t wanted = (size_t)id + 1;
    if (wanted <= g_scene_sprite_cap) return true;
    size_t cap = g_scene_sprite_cap ? g_scene_sprite_cap : 1024;
    while (cap < wanted) {
        if (cap > SIZE_MAX / 2) return false;
        cap *= 2;
    }
    VALUE* entries = realloc(g_scene_sprite, cap * sizeof(*entries));
    if (!entries) return false;
    memset(entries + g_scene_sprite_cap, 0,
           (cap - g_scene_sprite_cap) * sizeof(*entries));
    g_scene_sprite = entries;
    g_scene_sprite_cap = cap;
    return true;
}

void rbh_sprite_map_free(void) {
    free(g_scene_sprite);
    g_scene_sprite = NULL;
    g_scene_sprite_cap = 0;
}

static rbh_sprite_remove_fn g_sprite_remove_fn;

void rbh_set_sprite_hook(rbh_sprite_fn fn, rbh_sprite_remove_fn remove_fn,
                         void* user) {
    g_sprite_fn = fn;
    g_sprite_remove_fn = remove_fn;
    g_sprite_user = user;
}

// Entering the default engine makes a toy physical: one phys body per limb
// (at the limb's CURRENT position, so pre-add Toy#move works), the def's
// spring joints, and the visuals via the app's sprite hook, back-to-front
// by zOrder (smaller = nearer the viewer). Every allocation is checked; the
// caller reverses the engine transition if any prefix cannot be realized.
static bool toy_realize(VALUE toyv) {
    sn_t* t = sn_get(toyv);
    if (t->realized || !t->def) {
        return true;
    }
    t->realized = 1;
    const float S = (float)t->def->base_scale;

    VALUE limbs = sn_get(t->colls[1])->items;
    for (long i = 0; i < RARRAY(limbs)->len; i++) {
        sn_t* ln = sn_get(rb_ary_entry(limbs, i));
        if (ln->body < 0 && ln->ldef) {
            ln->body = toyphys_body_for_limb(ln->ldef, (float)ln->px,
                                             (float)ln->py,
                                             (float)ln->orient, S,
                                             t->instance_id);
            if (ln->body < 0) {
                fprintf(stderr, "rubyhost: cannot allocate body for %s/%s\n",
                        t->def->class_name, ln->ldef->name);
                return false;
            }
            if (ln->body >= 0 && (ln->mx || ln->my || ln->mL)) {
                phys_body_set_momentum(ln->body, (float)ln->mx, (float)ln->my,
                                       (float)ln->mL);
            }
        }
    }

    VALUE joints = sn_get(t->colls[2])->items;
    for (long i = 0; i < RARRAY(joints)->len; i++) {
        sn_t* jn = sn_get(rb_ary_entry(joints, i));
        const td_joint* j = jn->jdef;
        if (!j || NIL_P(jn->ref1) || NIL_P(jn->ref2)) {
            continue;
        }
        const int b1 = sn_get(jn->ref1)->body;
        const int b2 = sn_get(jn->ref2)->body;
        if (b1 < 0 || b2 < 0) {
            continue;
        }
        if (phys_joint_add(b1, j->anchor1[0] * S, j->anchor1[1] * S,
                           b2, j->anchor2[0] * S, j->anchor2[1] * S,
                           j->rest_length * S, j->stiffness,
                           j->dampener) < 0) {
            fprintf(stderr, "rubyhost: cannot allocate joint for %s\n",
                    t->def->class_name);
            return false;
        }
    }

    for (int i = 0; i < t->def->nrotjoints; i++) {
        const td_rotjoint* r = &t->def->rotjoints[i];
        if (r->limb1 < 0 || r->limb2 < 0) {
            continue;
        }
        const int b1 = sn_get(rb_ary_entry(limbs, r->limb1))->body;
        const int b2 = sn_get(rb_ary_entry(limbs, r->limb2))->body;
        if (b1 >= 0 && b2 >= 0) {
            if (phys_rotjoint_add(b1, r->orientation1, b2, r->orientation2,
                                  r->rest, r->stiffness, r->dampener) < 0) {
                fprintf(stderr,
                        "rubyhost: cannot allocate rotational joint for %s\n",
                        t->def->class_name);
                return false;
            }
        }
    }

    if (!g_sprite_fn) {
        return true; // headless: physics only
    }
    long vis_cap = 0;
    for (long i = 0; i < RARRAY(limbs)->len; i++) {
        VALUE sprites = sn_get(sn_get(rb_ary_entry(limbs, i))->colls[0])->items;
        vis_cap += RARRAY(sprites)->len;
    }
    struct visual { const td_sprite* sp; int body; VALUE node; };
    struct visual* vis = vis_cap > 0
        ? malloc((size_t)vis_cap * sizeof(*vis)) : NULL;
    if (vis_cap > 0 && !vis) {
        return false;
    }
    int nvis = 0;
    for (long i = 0; i < RARRAY(limbs)->len; i++) {
        sn_t* ln = sn_get(rb_ary_entry(limbs, i));
        VALUE sprites = sn_get(ln->colls[0])->items;
        for (long s = 0; s < RARRAY(sprites)->len; s++) {
            VALUE node = rb_ary_entry(sprites, s);
            const td_sprite* sp = sn_get(node)->spdef;
            if (sp && sp->image) {
                vis[nvis].sp = sp;
                vis[nvis].body = ln->body;
                vis[nvis].node = node;
                nvis++;
            }
        }
    }
    for (int a = 1; a < nvis; a++) { // insertion sort, zOrder descending
        const td_sprite* sp = vis[a].sp;
        const int body = vis[a].body;
        const VALUE node = vis[a].node;
        int b = a;
        // <=: on equal zOrder later def entries draw FIRST (goose legs sit
        // behind the body although both are -50)
        while (b > 0 && vis[b - 1].sp->z_order <= sp->z_order) {
            vis[b] = vis[b - 1];
            b--;
        }
        vis[b].sp = sp;
        vis[b].body = body;
        vis[b].node = node;
    }
    for (int i = 0; i < nvis; i++) {
        const int id = g_sprite_fn(vis[i].sp->image, vis[i].body,
                                   vis[i].sp->com[0], vis[i].sp->com[1],
                                   t->instance_id, g_sprite_user);
        if (id < 0) {
            free(vis);
            return false;
        }
        sn_get(vis[i].node)->body = id; // ensures rollback can remove it
        if (!scene_sprite_map_reserve(id)) {
            free(vis);
            return false;
        }
        g_scene_sprite[id] = vis[i].node;
    }
    free(vis);
    return true;
}

// Leaving the engine tears the toy back down: phys bodies freed (joints on
// them are neutralized), scene sprites removed via the app hook. Stored
// limb state keeps the last pose so a re-add realizes in place.
void toy_unrealize(VALUE toyv) {
    sn_t* t = sn_get(toyv);
    if (!t->realized) {
        return;
    }
    t->realized = 0;
    VALUE limbs = sn_get(t->colls[1])->items;
    for (long i = 0; i < RARRAY(limbs)->len; i++) {
        sn_t* ln = sn_get(rb_ary_entry(limbs, i));
        if (ln->body >= 0) {
            float x, y, mx, my, L;
            phys_body_pos(ln->body, &x, &y);
            phys_body_momentum(ln->body, &mx, &my, &L);
            ln->px = x;
            ln->py = y;
            ln->orient = phys_body_orientation(ln->body);
            ln->mx = mx;
            ln->my = my;
            ln->mL = L;
            phys_body_free(ln->body);
            ln->body = -1;
        }
        VALUE sprites = sn_get(ln->colls[0])->items;
        for (long sp = 0; sp < RARRAY(sprites)->len; sp++) {
            sn_t* spn = sn_get(rb_ary_entry(sprites, sp));
            if (spn->body >= 0) {
                if (g_sprite_remove_fn) {
                    g_sprite_remove_fn(spn->body, g_sprite_user);
                }
                if ((size_t)spn->body < g_scene_sprite_cap) {
                    g_scene_sprite[spn->body] = 0;
                }
                spn->body = -1;
            }
        }
    }
}

// Setting a node's engine recurses into children, then fires the
// SoupNode#engine_changed(old, new) callback the framework relies on to
// (de)register key/timer handlers (World hooks walls_changed here).
bool sn_set_engine(VALUE nodev, VALUE eng) {
    sn_t* n = sn_get(nodev);
    if (n->engine == eng) {
        return true;
    }
    VALUE old = n->engine;
    n->engine = eng;
    if (n->kind == SN_SOUND && NIL_P(eng)) {
        audio_stop_owner(n->sound_owner);
    }
    if (n->shdef && NIL_P(eng)) {
        n->overlaps = rb_ary_new();
    }
    if (!NIL_P(n->items)) {
        for (long i = 0; i < RARRAY(n->items)->len; i++) {
            if (!sn_set_engine(rb_ary_entry(n->items, i), eng)) goto fail;
        }
    }
    for (int i = 0; i < SN_NCOLLS; i++) {
        if (!NIL_P(n->colls[i])) {
            if (!sn_set_engine(n->colls[i], eng)) goto fail;
        }
    }
    if (n->kind == SN_TOY) {
        if (!NIL_P(eng) && eng == rb_gv_get("$default_engine")) {
            if (!toy_realize(nodev)) goto fail;
        } else if (NIL_P(eng)) {
            toy_unrealize(nodev);
        }
    }
    rb_funcall(nodev, rb_intern("engine_changed"), 2, old, eng);
    return true;

fail:
    // Some descendants may already have received engine_changed and the toy
    // may own a prefix of its bodies/sprites. Walking back through the normal
    // transition path unregisters callbacks and toy_unrealize removes every
    // successfully created resource.
    (void)sn_set_engine(nodev, old);
    return false;
}

VALUE sn_set_engine_protected(VALUE args) {
    return sn_set_engine(rb_ary_entry(args, 0), rb_ary_entry(args, 1))
        ? Qtrue : Qfalse;
}

// Mouse dispatch: the app picks the hit sprite (alpha test is scene-side)
// and hands VIEW coordinates here; the Sprite's internal_mouse_* framework
// methods convert to scene space and bubble the event up the node tree
// (Limb#mouse_down runs the default grab, ToyContainer#mouse_move drives
// input_move).
static VALUE sprite_node(int sprite) {
    if (sprite < 0 || (size_t)sprite >= g_scene_sprite_cap) {
        return Qnil;
    }
    VALUE v = g_scene_sprite[sprite];
    return v ? v : Qnil;
}

static VALUE sprite_toy(int sprite) {
    VALUE node = sprite_node(sprite);
    // Sprite -> SpriteContainer -> Limb -> LimbContainer -> Toy. Keep the
    // walk generic so nested toy definitions do not depend on exact depth.
    for (int depth = 0; depth < 16 && sn_p(node); depth++) {
        sn_t* n = sn_get(node);
        if (n->kind == SN_TOY) {
            return node;
        }
        node = n->parent;
    }
    return Qnil;
}

bool rbh_recycle_sprite(int sprite) {
    const VALUE toy = sprite_toy(sprite);
    if (NIL_P(toy)) {
        return false;
    }
    const sn_t* t = sn_get(toy);
    const VALUE parent = t->parent;
    if (t->sticky || t->engine != rb_gv_get("$default_engine")
        || !sn_p(parent) || sn_get(parent)->kind != SN_COLL) {
        return false;
    }
    const char* class_name = t->def ? t->def->class_name
                                    : rb_obj_classname(toy);
    if (!fcall_protected(parent, "remove", 1, toy, Qnil, Qnil,
                         "recycle toy")) {
        return false;
    }
    printf("toybox: recycled %s\n", class_name);
    return true;
}

bool rbh_clear_scene(void) {
    const VALUE engine = rb_gv_get("$default_engine");
    if (!sn_p(engine)) {
        return false;
    }
    const VALUE toys = sn_get(engine)->colls[0];
    if (!sn_p(toys) || sn_get(toys)->kind != SN_COLL) {
        return false;
    }

    bool ok = fcall_protected(engine, "set_scene_defaults", 0,
                              Qnil, Qnil, Qnil, "clear scene defaults");
    // on_clear may remove itself or mutate the collection, so walk a snapshot.
    const VALUE snapshot = rb_ary_dup(sn_get(toys)->items);
    const ID on_clear = rb_intern("on_clear");
    int removed = 0;
    for (long i = 0; i < RARRAY(snapshot)->len; i++) {
        const VALUE toy = rb_ary_entry(snapshot, i);
        if (!sn_p(toy) || sn_get(toy)->kind != SN_TOY) {
            continue;
        }
        if (rb_respond_to(toy, on_clear)
            && !fcall_protected(toy, "on_clear", 0, Qnil, Qnil, Qnil,
                                "toy on_clear")) {
            ok = false;
        }
        sn_t* node = sn_get(toy);
        if (!node->sticky && node->parent == toys) {
            if (fcall_protected(toys, "remove", 1, toy, Qnil, Qnil,
                                "clear toy")) {
                removed++;
            } else {
                ok = false;
            }
        }
    }
    printf("toybox: cleared %d toys\n", removed);
    return ok;
}

void rbh_mouse_down(int sprite, double x_px, double y_px, int button) {
    VALUE sp = sprite_node(sprite);
    if (!NIL_P(sp)) {
        fcall_protected(sp, "internal_mouse_down", 2, vec_new(x_px, y_px),
                        INT2FIX(button), Qnil, "mouse_down");
    }
}

void rbh_mouse_move(int sprite, double x_px, double y_px, int button,
                    bool down) {
    VALUE sp = sprite_node(sprite);
    if (!NIL_P(sp)) {
        fcall_protected(sp, "internal_mouse_move", 3, vec_new(x_px, y_px),
                        INT2FIX(button), down ? Qtrue : Qfalse, "mouse_move");
    }
}

void rbh_mouse_up(int sprite, double x_px, double y_px, int button) {
    VALUE sp = sprite_node(sprite);
    if (!NIL_P(sp)) {
        fcall_protected(sp, "internal_mouse_up", 2, vec_new(x_px, y_px),
                        INT2FIX(button), Qnil, "mouse_up");
    }
}

void rbh_mouse_click(int sprite, double x_px, double y_px, int button) {
    VALUE sp = sprite_node(sprite);
    if (!NIL_P(sp)) {
        fcall_protected(sp, "internal_mouse_click", 2, vec_new(x_px, y_px),
                        INT2FIX(button), Qnil, "mouse_click");
    }
}
