// Embedded Ruby 1.8.6 host, the script side of the engine.
// Class surface comes from the reverse-engineered Toybox binding surface:
// every method is first bound to a logging stub so framework load-time aliases
// (`alias :internal_render :render` etc.) always resolve; the subset the boot
// path needs is then rebound to real implementations backed by toydefs/physics.
#include "rubyhost.h"
#include "audio.h"
#include "physics.h"
#include "toydefs.h"
#include "toyphys.h"
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ruby.h"

void Init_ext(void); // ext/extinit.o: static stringio + syck

// registry

#define MAX_CLASSES 40
static struct { const char* cname; VALUE cls; } g_reg[MAX_CLASSES];
static int g_nreg;
static char g_root[1024]; // resource root (extracted souptoys_core.toy)

static VALUE cls_find(const char* name) {
    for (int i = 0; i < g_nreg; i++) {
        if (strcmp(g_reg[i].cname, name) == 0) {
            return g_reg[i].cls;
        }
    }
    rb_raise(rb_eRuntimeError, "rubyhost: unknown engine class %s", name);
}

// node model

// One tagged struct backs every engine object. colls[] carries the child
// collections (toy: toys/limbs/joints/rotational_joints/sprites/sounds;
// limb: sprites/shapes/linear_motors/rotational_motors; engine: toys).

enum {
    SN_GENERIC, SN_COLL, SN_TOY, SN_LIMB, SN_ENGINE, SN_CORE, SN_SOUP,
    SN_SOUND
};
#define SN_NCOLLS 6

typedef struct {
    int kind;
    VALUE sid, parent, engine;
    VALUE items;            // SN_COLL
    VALUE colls[SN_NCOLLS];
    VALUE ref1, ref2;       // joint: limb1/limb2
    const toydef_t* def;    // SN_TOY
    int instance_id;        // SN_TOY
    int sticky;             // SN_TOY
    int realized;           // SN_TOY: phys bodies/joints exist
    const td_limb* ldef;    // SN_LIMB
    const td_sprite* spdef; // sprite node
    const td_shape* shdef;  // shape node
    const td_sound* snddef; // sound node
    int shape_index;        // index in the owning limb/physics body
    const td_joint* jdef;   // joint node
    int body;               // SN_LIMB: phys body index, -1 = not realized
    double px, py, orient, shock;
    double mx, my, mL;      // SN_LIMB: momentum, pre-realization only
    double lscale;          // SN_LIMB: owning toy's base_scale (unit conv)
    VALUE inputs;           // SN_ENGINE
    VALUE timers;           // SN_ENGINE: [[proc, period_ticks, next_tick]...]
    VALUE overlaps;         // Shape: active [[other_shape, group], ...]
    VALUE sound_path;       // Sound: pack-relative Ogg path
    uint32_t sound_owner;   // Sound: groups its active mixer voices
    int audio_sample;       // Sound: lazily populated audio cache id
    double sound_period, sound_window_start;
    int sound_max, sound_window_count, sound_window_valid;
    double timer_tick;      // SN_ENGINE: dispatch_timers position, 1/100 s
    double scene_bl[2], scene_tr[2], canvas_tl[2], canvas_br[2];
    double scale, gravity, timestep, timescale, time;
    int paused;
    VALUE engines;          // SN_CORE
    VALUE license_policy, load_paths; // SN_SOUP
} sn_t;

static double g_screen_w = 1280.0, g_screen_h = 800.0;

static void sn_gcmark(void* p) {
    sn_t* n = p;
    rb_gc_mark(n->sid);
    rb_gc_mark(n->parent);
    rb_gc_mark(n->engine);
    rb_gc_mark(n->items);
    rb_gc_mark(n->ref1);
    rb_gc_mark(n->ref2);
    for (int i = 0; i < SN_NCOLLS; i++) {
        rb_gc_mark(n->colls[i]);
    }
    rb_gc_mark(n->inputs);
    rb_gc_mark(n->timers);
    rb_gc_mark(n->overlaps);
    rb_gc_mark(n->sound_path);
    rb_gc_mark(n->engines);
    rb_gc_mark(n->license_policy);
    rb_gc_mark(n->load_paths);
}

static int sn_p(VALUE v) {
    return TYPE(v) == T_DATA && RDATA(v)->dmark == sn_gcmark;
}

static sn_t* sn_get(VALUE v) {
    if (!sn_p(v)) {
        rb_raise(rb_eTypeError, "not an engine object: %s",
                 rb_obj_classname(v));
    }
    return (sn_t*)DATA_PTR(v);
}

static VALUE sn_wrap(VALUE klass, int kind) {
    sn_t* n = calloc(1, sizeof(sn_t));
    n->kind = kind;
    n->sid = n->parent = n->engine = Qnil;
    n->items = n->inputs = n->engines = Qnil;
    n->ref1 = n->ref2 = n->timers = Qnil;
    n->overlaps = Qnil;
    n->sound_path = Qnil;
    n->audio_sample = -1;
    n->license_policy = n->load_paths = Qnil;
    n->body = -1;
    n->shape_index = -1;
    for (int i = 0; i < SN_NCOLLS; i++) {
        n->colls[i] = Qnil;
    }
    n->scale = 100.0;
    n->gravity = PHYS_GRAVITY;
    n->timestep = PHYS_DT;
    n->timescale = 1.0;
    return Data_Wrap_Struct(klass, sn_gcmark, free, n);
}

// Vector

static VALUE vec_new(double x, double y) {
    return rb_funcall(rb_const_get(rb_cObject, rb_intern("Vector")),
                      rb_intern("[]"), 2, rb_float_new(x), rb_float_new(y));
}

static void vec_get(VALUE v, double* x, double* y) {
    ID idx = rb_intern("[]");
    *x = NUM2DBL(rb_funcall(v, idx, 1, INT2FIX(0)));
    *y = NUM2DBL(rb_funcall(v, idx, 1, INT2FIX(1)));
}

// realization + engine (re)parenting

static rbh_sprite_fn g_sprite_fn;
static void* g_sprite_user;

// scene sprite id -> Ruby Sprite node (nodes stay reachable via the toy
// tree; entries go stale only on toy removal, which has no teardown yet)
#define MAX_SCENE_SPRITES 1024
static VALUE g_scene_sprite[MAX_SCENE_SPRITES];

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
// by zOrder (smaller = nearer the viewer). No teardown yet: bodies outlive
// removal until phys grows a free list.
static void toy_realize(VALUE toyv) {
    sn_t* t = sn_get(toyv);
    if (t->realized || !t->def) {
        return;
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
        phys_joint_add(b1, j->anchor1[0] * S, j->anchor1[1] * S,
                       b2, j->anchor2[0] * S, j->anchor2[1] * S,
                       j->rest_length * S, j->stiffness, j->dampener);
    }

    for (int i = 0; i < t->def->nrotjoints; i++) {
        const td_rotjoint* r = &t->def->rotjoints[i];
        if (r->limb1 < 0 || r->limb2 < 0) {
            continue;
        }
        const int b1 = sn_get(rb_ary_entry(limbs, r->limb1))->body;
        const int b2 = sn_get(rb_ary_entry(limbs, r->limb2))->body;
        if (b1 >= 0 && b2 >= 0) {
            phys_rotjoint_add(b1, r->orientation1, b2, r->orientation2,
                              r->rest, r->stiffness, r->dampener);
        }
    }

    if (!g_sprite_fn) {
        return; // headless: physics only
    }
    struct { const td_sprite* sp; int body; VALUE node; } vis[128];
    int nvis = 0;
    for (long i = 0; i < RARRAY(limbs)->len && nvis < 128; i++) {
        sn_t* ln = sn_get(rb_ary_entry(limbs, i));
        VALUE sprites = sn_get(ln->colls[0])->items;
        for (long s = 0; s < RARRAY(sprites)->len && nvis < 128; s++) {
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
        if (id >= 0 && id < MAX_SCENE_SPRITES) {
            g_scene_sprite[id] = vis[i].node;
            sn_get(vis[i].node)->body = id; // sprite node: scene sprite id
        }
    }
}

// Leaving the engine tears the toy back down: phys bodies freed (joints on
// them are neutralized), scene sprites removed via the app hook. Stored
// limb state keeps the last pose so a re-add realizes in place.
static void toy_unrealize(VALUE toyv) {
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
                if (spn->body < MAX_SCENE_SPRITES) {
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
static void sn_set_engine(VALUE nodev, VALUE eng) {
    sn_t* n = sn_get(nodev);
    if (n->engine == eng) {
        return;
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
            sn_set_engine(rb_ary_entry(n->items, i), eng);
        }
    }
    for (int i = 0; i < SN_NCOLLS; i++) {
        if (!NIL_P(n->colls[i])) {
            sn_set_engine(n->colls[i], eng);
        }
    }
    if (n->kind == SN_TOY) {
        if (!NIL_P(eng) && eng == rb_gv_get("$default_engine")) {
            toy_realize(nodev);
        } else if (NIL_P(eng)) {
            toy_unrealize(nodev);
        }
    }
    rb_funcall(nodev, rb_intern("engine_changed"), 2, old, eng);
}

// stubs

static int g_stub_log_budget = 200;

static VALUE rba_stub(int argc, VALUE* argv, VALUE self) {
    (void)argc;
    (void)argv;
    if (g_stub_log_budget > 0) {
        g_stub_log_budget--;
        fprintf(stderr, "[rubyhost] stub %s#%s\n", rb_obj_classname(self),
                rb_id2name(rb_frame_last_func()));
    }
    return Qnil;
}

// allocators

static VALUE alloc_generic(VALUE klass) {
    return sn_wrap(klass, SN_GENERIC);
}

static VALUE alloc_coll(VALUE klass) {
    VALUE v = sn_wrap(klass, SN_COLL);
    sn_get(v)->items = rb_ary_new();
    return v;
}

static VALUE coll_new_named(const char* klass, VALUE parent, VALUE eng) {
    VALUE v = alloc_coll(cls_find(klass));
    sn_t* n = sn_get(v);
    n->parent = parent;
    n->engine = eng;
    return v;
}

static VALUE alloc_limb_at(const td_limb* l, double base_scale) {
    VALUE v = sn_wrap(cls_find("Limb"), SN_LIMB);
    sn_t* n = sn_get(v);
    n->ldef = l;
    n->lscale = base_scale;
    n->colls[0] = coll_new_named("SpriteContainer", v, Qnil);
    n->colls[1] = coll_new_named("ShapeContainer", v, Qnil);
    n->colls[2] = coll_new_named("LinearMotorContainer", v, Qnil);
    n->colls[3] = coll_new_named("RotationalMotorContainer", v, Qnil);
    if (l) {
        n->sid = ID2SYM(rb_intern(l->name));
        n->px = l->rest_pos[0] * base_scale;
        n->py = l->rest_pos[1] * base_scale;
        n->orient = l->rest_orient;
        for (int i = 0; i < l->nsprites; i++) {
            VALUE sp = sn_wrap(cls_find("Sprite"), SN_GENERIC);
            sn_get(sp)->spdef = &l->sprites[i];
            if (l->sprites[i].sid && l->sprites[i].sid[0]) {
                sn_get(sp)->sid = ID2SYM(rb_intern(l->sprites[i].sid));
            }
            sn_get(sp)->parent = n->colls[0];
            rb_ary_push(sn_get(n->colls[0])->items, sp);
        }
        for (int i = 0; i < l->nshapes; i++) {
            VALUE sh = sn_wrap(cls_find("Shape"), SN_GENERIC);
            sn_t* shn = sn_get(sh);
            shn->shdef = &l->shapes[i];
            shn->shape_index = i;
            shn->ref2 = rb_ary_new(); // Shape#member_of
            for (int m = 0; m < l->shapes[i].nmembers; m++) {
                rb_ary_push(shn->ref2,
                            ID2SYM(rb_intern(l->shapes[i].members[m])));
            }
            shn->overlaps = rb_ary_new();
            if (l->shapes[i].sid && l->shapes[i].sid[0]) {
                shn->sid = ID2SYM(rb_intern(l->shapes[i].sid));
            }
            shn->parent = n->colls[1];
            rb_ary_push(sn_get(n->colls[1])->items, sh);
        }
    }
    return v;
}

static VALUE alloc_limb(VALUE klass) {
    (void)klass;
    return alloc_limb_at(NULL, 1.0);
}

static int g_next_instance_id = 1;
static uint32_t g_next_sound_owner = 1;

static VALUE alloc_sound(VALUE klass) {
    VALUE v = sn_wrap(klass, SN_SOUND);
    sn_get(v)->sound_owner = g_next_sound_owner++;
    if (g_next_sound_owner == 0) {
        g_next_sound_owner = 1;
    }
    return v;
}

// Toy allocation is where the native side binds a Ruby toy class to its
// .toy definition: class name -> toydefs entry -> limbs with sids, so
// `World.new` comes back with left_wall/right_wall/floor/ceiling in place.
static VALUE alloc_toy(VALUE klass) {
    VALUE self = sn_wrap(klass, SN_TOY);
    sn_t* n = sn_get(self);
    n->instance_id = g_next_instance_id++;
    static const char* cn[SN_NCOLLS] = {
        "ToyContainer", "LimbContainer", "JointContainer",
        "RotationalJointContainer", "SpriteContainer", "SoundContainer"
    };
    for (int i = 0; i < SN_NCOLLS; i++) {
        n->colls[i] = coll_new_named(cn[i], self, Qnil);
    }
    n->def = toydefs_find(rb_class2name(klass));
    if (n->def) {
        sn_t* limbs = sn_get(n->colls[1]);
        for (int i = 0; i < n->def->nlimbs; i++) {
            VALUE lv = alloc_limb_at(&n->def->limbs[i], n->def->base_scale);
            sn_get(lv)->parent = n->colls[1];
            rb_ary_push(limbs->items, lv);
        }
        sn_t* joints = sn_get(n->colls[2]);
        for (int i = 0; i < n->def->njoints; i++) {
            const td_joint* j = &n->def->joints[i];
            VALUE jv = sn_wrap(cls_find("Joint"), SN_GENERIC);
            sn_t* jn = sn_get(jv);
            jn->jdef = j;
            jn->parent = n->colls[2];
            if (j->limb1 >= 0) {
                jn->ref1 = rb_ary_entry(limbs->items, j->limb1);
            }
            if (j->limb2 >= 0) {
                jn->ref2 = rb_ary_entry(limbs->items, j->limb2);
            }
            rb_ary_push(joints->items, jv);
        }
        sn_t* sounds = sn_get(n->colls[5]);
        for (int i = 0; i < n->def->nsounds; i++) {
            VALUE sv = alloc_sound(cls_find("Sound"));
            sn_t* sound = sn_get(sv);
            sound->snddef = &n->def->sounds[i];
            if (n->def->sounds[i].location) {
                sound->sound_path = rb_str_new2(n->def->sounds[i].location);
            }
            if (n->def->sounds[i].sid && n->def->sounds[i].sid[0]) {
                sound->sid = ID2SYM(rb_intern(n->def->sounds[i].sid));
            }
            sound->parent = n->colls[5];
            rb_ary_push(sounds->items, sv);
        }
        // TODO: rotational joints, motors (defs not consumed yet)
    }
    return self;
}

static VALUE alloc_engine(VALUE klass) {
    VALUE v = sn_wrap(klass, SN_ENGINE);
    sn_t* n = sn_get(v);
    n->inputs = rb_ary_new();
    n->timers = rb_ary_new();
    // parent stays nil: the toys container is the event-bubble ROOT
    // (events.rb route_bubble walks parents; REngine is not a SoupNode)
    n->colls[0] = coll_new_named("ToyContainer", Qnil, v);
    return v;
}

static VALUE alloc_core(VALUE klass) {
    VALUE v = sn_wrap(klass, SN_CORE);
    sn_get(v)->engines = rb_ary_new();
    return v;
}

static VALUE alloc_soup(VALUE klass) {
    return sn_wrap(klass, SN_SOUP);
}

// SoupNode

static VALUE sn_engine(VALUE self) { return sn_get(self)->engine; }
static VALUE sn_parent(VALUE self) { return sn_get(self)->parent; }
static VALUE sn_sid(VALUE self) { return sn_get(self)->sid; }

static VALUE sn_sid_set(VALUE self, VALUE sid) {
    sn_get(self)->sid = sid;
    return sid;
}

// SoupNodeCollection

static VALUE coll_add(VALUE self, VALUE child) {
    sn_t* c = sn_get(self);
    rb_ary_push(c->items, child);
    sn_get(child)->parent = self;
    sn_set_engine(child, c->engine);
    return child;
}

static VALUE coll_lshift(VALUE self, VALUE child) {
    coll_add(self, child);
    return self;
}

static VALUE coll_remove(VALUE self, VALUE child) {
    sn_t* c = sn_get(self);
    rb_ary_delete(c->items, child);
    if (sn_p(child)) {
        sn_get(child)->parent = Qnil;
        sn_set_engine(child, Qnil);
    }
    return child;
}

static VALUE coll_each(VALUE self) {
    sn_t* c = sn_get(self);
    for (long i = 0; i < RARRAY(c->items)->len; i++) {
        rb_yield(rb_ary_entry(c->items, i));
    }
    return self;
}

static VALUE coll_count(VALUE self) {
    return LONG2NUM(RARRAY(sn_get(self)->items)->len);
}

static VALUE coll_by_index(VALUE self, VALUE idx) {
    return rb_ary_entry(sn_get(self)->items, NUM2LONG(idx));
}

static VALUE coll_by_sid(VALUE self, VALUE sid) {
    sn_t* c = sn_get(self);
    for (long i = 0; i < RARRAY(c->items)->len; i++) {
        VALUE it = rb_ary_entry(c->items, i);
        if (sn_p(it) && sn_get(it)->sid == sid) {
            return it;
        }
    }
    return Qnil;
}

// Toy

static VALUE toy_coll(VALUE self, int i) { return sn_get(self)->colls[i]; }
static VALUE toy_toys(VALUE self) { return toy_coll(self, 0); }
static VALUE toy_limbs(VALUE self) { return toy_coll(self, 1); }
static VALUE toy_joints(VALUE self) { return toy_coll(self, 2); }
static VALUE toy_rjoints(VALUE self) { return toy_coll(self, 3); }
static VALUE toy_sprites(VALUE self) { return toy_coll(self, 4); }
static VALUE toy_sounds(VALUE self) { return toy_coll(self, 5); }

static VALUE toy_id(VALUE self) {
    sn_t* n = sn_get(self);
    return rb_str_new2(n->def ? n->def->class_name
                              : rb_obj_classname(self));
}

static VALUE toy_instance_id(VALUE self) {
    return INT2NUM(sn_get(self)->instance_id);
}

static VALUE toy_sticky_set(VALUE self, VALUE v) {
    sn_get(self)->sticky = RTEST(v);
    return v;
}

static VALUE toy_sticky_p(VALUE self) {
    return sn_get(self)->sticky ? Qtrue : Qfalse;
}

// Limb

static VALUE limb_sprites(VALUE self) { return sn_get(self)->colls[0]; }
static VALUE limb_shapes(VALUE self) { return sn_get(self)->colls[1]; }
static VALUE limb_lmotors(VALUE self) { return sn_get(self)->colls[2]; }
static VALUE limb_rmotors(VALUE self) { return sn_get(self)->colls[3]; }

// Once realized, the phys body owns the kinematic truth; the stored fields
// only cover the pre-realization window (construction, pre-add Toy#move).
static VALUE limb_position(VALUE self) {
    sn_t* n = sn_get(self);
    if (n->body >= 0) {
        float x, y;
        phys_body_pos(n->body, &x, &y);
        return vec_new(x, y);
    }
    return vec_new(n->px, n->py);
}

static VALUE limb_position_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    vec_get(v, &n->px, &n->py);
    if (n->body >= 0) {
        phys_body_set_pose(n->body, (float)n->px, (float)n->py,
                           phys_body_orientation(n->body));
    }
    return v;
}

static VALUE limb_orientation(VALUE self) {
    sn_t* n = sn_get(self);
    if (n->body >= 0) {
        return rb_float_new(phys_body_orientation(n->body));
    }
    return rb_float_new(n->orient);
}

static VALUE limb_orientation_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    n->orient = NUM2DBL(v);
    if (n->body >= 0) {
        float x, y;
        phys_body_pos(n->body, &x, &y);
        phys_body_set_pose(n->body, x, y, (float)n->orient);
    }
    return v;
}

static VALUE limb_shock_order(VALUE self) {
    return rb_float_new(sn_get(self)->shock);
}

static VALUE limb_shock_order_set(VALUE self, VALUE v) {
    sn_get(self)->shock = NUM2DBL(v);
    return v;
}

static void limb_pose(sn_t* n, double* x, double* y, double* th) {
    if (n->body >= 0) {
        float px, py;
        phys_body_pos(n->body, &px, &py);
        *x = px;
        *y = py;
        *th = phys_body_orientation(n->body);
    } else {
        *x = n->px;
        *y = n->py;
        *th = n->orient;
    }
}

static VALUE limb_to_world(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    double x, y, th, lx, ly;
    limb_pose(n, &x, &y, &th);
    vec_get(v, &lx, &ly);
    const double c = cos(th), sn = sin(th);
    return vec_new(x + c * lx - sn * ly, y + sn * lx + c * ly);
}

static VALUE limb_to_local(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    double x, y, th, wx, wy;
    limb_pose(n, &x, &y, &th);
    vec_get(v, &wx, &wy);
    const double c = cos(th), sn = sin(th);
    const double dx = wx - x, dy = wy - y;
    return vec_new(c * dx + sn * dy, -sn * dx + c * dy);
}

static VALUE limb_momentum(VALUE self) {
    sn_t* n = sn_get(self);
    if (n->body >= 0) {
        float mx, my, L;
        phys_body_momentum(n->body, &mx, &my, &L);
        return vec_new(mx, my);
    }
    return vec_new(n->mx, n->my);
}

static VALUE limb_momentum_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    vec_get(v, &n->mx, &n->my);
    if (n->body >= 0) {
        float mx, my, L;
        phys_body_momentum(n->body, &mx, &my, &L);
        phys_body_set_momentum(n->body, (float)n->mx, (float)n->my, L);
    }
    return v;
}

static VALUE limb_angular_momentum(VALUE self) {
    sn_t* n = sn_get(self);
    if (n->body >= 0) {
        float mx, my, L;
        phys_body_momentum(n->body, &mx, &my, &L);
        return rb_float_new(L);
    }
    return rb_float_new(n->mL);
}

static VALUE limb_angular_momentum_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    n->mL = NUM2DBL(v);
    if (n->body >= 0) {
        float mx, my, L;
        phys_body_momentum(n->body, &mx, &my, &L);
        phys_body_set_momentum(n->body, mx, my, (float)n->mL);
    }
    return v;
}

static VALUE limb_inertia_tensor(VALUE self) {
    sn_t* n = sn_get(self);
    if (!n->ldef) {
        return Qnil;
    }
    // def inertia is toy-local units^2; scripts expect world units
    return rb_float_new(n->ldef->inertia * n->lscale * n->lscale);
}

static VALUE limb_mass(VALUE self) {
    sn_t* n = sn_get(self);
    return n->ldef ? rb_float_new(n->ldef->mass) : Qnil;
}

static VALUE limb_fixed_move(VALUE self) {
    sn_t* n = sn_get(self);
    return (n->ldef && n->ldef->fixed_move) ? Qtrue : Qfalse;
}

// REngine

static VALUE eng_toys(VALUE self) { return sn_get(self)->colls[0]; }

static VALUE eng_input_each(VALUE self) {
    sn_t* n = sn_get(self);
    for (long i = 0; i < RARRAY(n->inputs)->len; i++) {
        rb_yield(rb_ary_entry(n->inputs, i));
    }
    return self;
}

static VALUE eng_input_add(VALUE self, VALUE input) {
    rb_ary_push(sn_get(self)->inputs, input);
    return input;
}

static VALUE eng_input_remove(VALUE self, VALUE input) {
    rb_ary_delete(sn_get(self)->inputs, input);
    return input;
}

static int is_default_engine(VALUE self) {
    return self == rb_gv_get("$default_engine");
}

static void eng_sync_phys_walls(sn_t* n, VALUE self) {
    if (!is_default_engine(self)) {
        return;
    }
    const double w = n->scene_bl[0] - n->scene_tr[0]; // tr is negated
    const double h = n->scene_bl[1] - n->scene_tr[1];
    if (w > 0 && h > 0) {
        phys_set_world((float)w, (float)h);
    }
}

static VALUE eng_scene_bl(VALUE self) {
    sn_t* n = sn_get(self);
    return vec_new(n->scene_bl[0], n->scene_bl[1]);
}

// All rect/scale setters are change-gated, like the original's: firing the
// framework callbacks on a no-op write would run fit_* on half-updated
// rects (scale blows up to Infinity) and can recurse.
static VALUE eng_scene_bl_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    double x, y;
    vec_get(v, &x, &y);
    if (x == n->scene_bl[0] && y == n->scene_bl[1]) {
        return v;
    }
    n->scene_bl[0] = x;
    n->scene_bl[1] = y;
    eng_sync_phys_walls(n, self);
    rb_funcall(self, rb_intern("scene_walls_changed"), 0);
    return v;
}

static VALUE eng_scene_tr(VALUE self) {
    sn_t* n = sn_get(self);
    return vec_new(n->scene_tr[0], n->scene_tr[1]);
}

static VALUE eng_scene_tr_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    double x, y;
    vec_get(v, &x, &y);
    if (x == n->scene_tr[0] && y == n->scene_tr[1]) {
        return v;
    }
    n->scene_tr[0] = x;
    n->scene_tr[1] = y;
    eng_sync_phys_walls(n, self);
    rb_funcall(self, rb_intern("scene_walls_changed"), 0);
    return v;
}

static VALUE eng_canvas_tl(VALUE self) {
    sn_t* n = sn_get(self);
    return vec_new(n->canvas_tl[0], n->canvas_tl[1]);
}

static VALUE eng_canvas_tl_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    double x, y;
    vec_get(v, &x, &y);
    if (x == n->canvas_tl[0] && y == n->canvas_tl[1]) {
        return v;
    }
    n->canvas_tl[0] = x;
    n->canvas_tl[1] = y;
    rb_funcall(self, rb_intern("canvas_changed"), 0);
    return v;
}

static VALUE eng_canvas_br(VALUE self) {
    sn_t* n = sn_get(self);
    return vec_new(n->canvas_br[0], n->canvas_br[1]);
}

static VALUE eng_canvas_br_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    double x, y;
    vec_get(v, &x, &y);
    if (x == n->canvas_br[0] && y == n->canvas_br[1]) {
        return v;
    }
    n->canvas_br[0] = x;
    n->canvas_br[1] = y;
    rb_funcall(self, rb_intern("canvas_changed"), 0);
    return v;
}

static VALUE eng_screen_tl(VALUE self) {
    (void)self;
    return vec_new(0.0, 0.0);
}

static VALUE eng_screen_br(VALUE self) {
    (void)self;
    return vec_new(g_screen_w, g_screen_h);
}

// Coordinate spaces: view/screen = device px y-down; canvas = px y-down with
// origin at the canvas rect's top-left; scene = meters y-up, scene_top_right
// stored NEGATED (framework convention). scale = px per scene unit.
static VALUE eng_view_to_canvas(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    double x, y;
    vec_get(v, &x, &y);
    return vec_new(x - n->canvas_tl[0], y - n->canvas_tl[1]);
}

static VALUE eng_canvas_to_view(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    double x, y;
    vec_get(v, &x, &y);
    return vec_new(x + n->canvas_tl[0], y + n->canvas_tl[1]);
}

static VALUE eng_canvas_to_scene(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    double x, y;
    vec_get(v, &x, &y);
    const double ch = n->canvas_br[1] - n->canvas_tl[1];
    return vec_new(n->scene_bl[0] + x / n->scale,
                   n->scene_bl[1] + (ch - y) / n->scale);
}

static VALUE eng_scene_to_canvas(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    double x, y;
    vec_get(v, &x, &y);
    const double ch = n->canvas_br[1] - n->canvas_tl[1];
    return vec_new((x - n->scene_bl[0]) * n->scale,
                   ch - (y - n->scene_bl[1]) * n->scale);
}

static VALUE eng_view_to_scene(VALUE self, VALUE v) {
    return eng_canvas_to_scene(self, eng_view_to_canvas(self, v));
}

static VALUE eng_scene_to_view(VALUE self, VALUE v) {
    return eng_canvas_to_view(self, eng_scene_to_canvas(self, v));
}

// Input grab triplet - the mouse spring, anchored at the grabbed point
// (sub_4237B0): the limb's default_grab_move/rotate flags gate the spring's
// linear/torque components. TODO: per-shape grabMove/grabRotate override of
// the limb defaults (needs a point-in-shape test on the hit shape).
static VALUE eng_input_grab(VALUE self, VALUE limb, VALUE input, VALUE pos) {
    (void)self;
    sn_t* ln = sn_get(limb);
    sn_t* in = sn_get(input);
    if (ln->body >= 0) {
        double px, py;
        vec_get(pos, &px, &py);
        float bx, by;
        phys_body_pos(ln->body, &bx, &by);
        const float th = phys_body_orientation(ln->body);
        const float c = cosf(th), sn = sinf(th);
        const float dx = (float)px - bx, dy = (float)py - by;
        in->ref1 = limb;
        phys_grab(ln->body, c * dx + sn * dy, -sn * dx + c * dy,
                  ln->ldef ? ln->ldef->default_grab_move : true,
                  ln->ldef ? ln->ldef->default_grab_rotate : true);
    }
    return Qnil;
}

static VALUE eng_input_move(VALUE self, VALUE input, VALUE pos) {
    (void)self;
    sn_t* in = sn_get(input);
    if (!NIL_P(in->ref1)) {
        sn_t* ln = sn_get(in->ref1);
        if (ln->body >= 0) {
            double px, py;
            vec_get(pos, &px, &py);
            phys_grab_move(ln->body, (float)px, (float)py);
        }
    }
    return Qnil;
}

static VALUE eng_input_release(VALUE self, VALUE limb, VALUE input,
                               VALUE pos) {
    (void)self;
    (void)limb;
    (void)pos;
    sn_t* in = sn_get(input);
    if (!NIL_P(in->ref1)) {
        sn_t* ln = sn_get(in->ref1);
        if (ln->body >= 0) {
            phys_release(ln->body);
        }
        in->ref1 = Qnil;
    }
    return Qnil;
}

static VALUE input_limb(VALUE self) {
    return sn_get(self)->ref1;
}

// the limb a shape belongs to (shape.parent = ShapeContainer, .parent = limb)
static VALUE shape_limb(VALUE self) {
    VALUE coll = sn_get(self)->parent;
    return NIL_P(coll) ? Qnil : sn_get(coll)->parent;
}

static bool value_array_has(VALUE ary, VALUE value) {
    if (TYPE(ary) != T_ARRAY) return false;
    for (long i = 0; i < RARRAY(ary)->len; i++) {
        if (RTEST(rb_equal(rb_ary_entry(ary, i), value))) return true;
    }
    return false;
}

static VALUE normalize_groups(VALUE groups) {
    if (NIL_P(groups)) return rb_ary_new();
    VALUE source = TYPE(groups) == T_ARRAY ? groups : rb_ary_new3(1, groups);
    VALUE result = rb_ary_new();
    const ID id_to_sym = rb_intern("to_sym");
    for (long i = 0; i < RARRAY(source)->len; i++) {
        VALUE group = rb_ary_entry(source, i);
        if (rb_respond_to(group, id_to_sym)) {
            group = rb_funcall(group, id_to_sym, 0);
        }
        if (!value_array_has(result, group)) rb_ary_push(result, group);
    }
    return result;
}

static VALUE shape_member_of(VALUE self) {
    sn_t* n = sn_get(self);
    if (NIL_P(n->ref2)) n->ref2 = rb_ary_new();
    return n->ref2;
}

static VALUE shape_member_of_set(VALUE self, VALUE groups) {
    sn_get(self)->ref2 = normalize_groups(groups);
    return groups;
}

static VALUE shape_trigger_on(VALUE self) {
    VALUE v = sn_get(self)->ref1;
    return NIL_P(v) ? rb_ary_new() : v;
}

static VALUE shape_trigger_on_set(VALUE self, VALUE v) {
    sn_get(self)->ref1 = normalize_groups(v);
    return v;
}

static VALUE engine_shape_snapshot(VALUE engine) {
    VALUE result = rb_ary_new();
    VALUE toys = sn_get(sn_get(engine)->colls[0])->items;
    for (long ti = 0; ti < RARRAY(toys)->len; ti++) {
        sn_t* toy = sn_get(rb_ary_entry(toys, ti));
        VALUE limbs = sn_get(toy->colls[1])->items;
        for (long li = 0; li < RARRAY(limbs)->len; li++) {
            sn_t* limb = sn_get(rb_ary_entry(limbs, li));
            if (limb->body < 0) continue;
            VALUE shapes = sn_get(limb->colls[1])->items;
            for (long si = 0; si < RARRAY(shapes)->len; si++) {
                VALUE shape = rb_ary_entry(shapes, si);
                if (sn_get(shape)->shape_index >= 0) rb_ary_push(result, shape);
            }
        }
    }
    return result;
}

static bool shape_nodes_overlap(VALUE a, VALUE b) {
    if (a == b) return false;
    sn_t* as = sn_get(a);
    sn_t* bs = sn_get(b);
    VALUE alv = shape_limb(a), blv = shape_limb(b);
    if (NIL_P(alv) || NIL_P(blv)) return false;
    sn_t* al = sn_get(alv);
    sn_t* bl = sn_get(blv);
    return phys_shapes_overlap(al->body, as->shape_index,
                               bl->body, bs->shape_index);
}

static VALUE shape_triggers_overlapping(VALUE self, VALUE groups) {
    VALUE result = rb_ary_new();
    sn_t* shape = sn_get(self);
    if (NIL_P(shape->engine)) return result;
    VALUE wanted = normalize_groups(groups);
    VALUE shapes = engine_shape_snapshot(shape->engine);
    for (long i = 0; i < RARRAY(shapes)->len; i++) {
        VALUE other = rb_ary_entry(shapes, i);
        VALUE members = shape_member_of(other);
        bool matches = false;
        for (long g = 0; g < RARRAY(wanted)->len; g++) {
            if (value_array_has(members, rb_ary_entry(wanted, g))) {
                matches = true;
                break;
            }
        }
        if (matches && shape_nodes_overlap(self, other)) rb_ary_push(result, other);
    }
    return result;
}

static bool overlap_pair_has(VALUE overlaps, VALUE other, VALUE group) {
    if (TYPE(overlaps) != T_ARRAY) return false;
    for (long i = 0; i < RARRAY(overlaps)->len; i++) {
        VALUE pair = rb_ary_entry(overlaps, i);
        if (TYPE(pair) == T_ARRAY && RARRAY(pair)->len >= 2
            && rb_ary_entry(pair, 0) == other
            && RTEST(rb_equal(rb_ary_entry(pair, 1), group))) {
            return true;
        }
    }
    return false;
}

static void transition_push(VALUE transitions, VALUE trigger,
                            VALUE other, VALUE group) {
    rb_ary_push(transitions, rb_ary_new3(3, trigger, other, group));
}

// Trigger state is sampled after every fixed physics step. Geometry is shared
// with collision narrowphase, but memberOf response and trigger_on matching are
// deliberately separate: sensor-only shapes never produce physical impulses.
static void dispatch_trigger_transitions(VALUE engine) {
    VALUE shapes = engine_shape_snapshot(engine);
    VALUE enters = rb_ary_new();
    VALUE exits = rb_ary_new();

    for (long i = 0; i < RARRAY(shapes)->len; i++) {
        VALUE trigger = rb_ary_entry(shapes, i);
        sn_t* ts = sn_get(trigger);
        VALUE watched = shape_trigger_on(trigger);
        VALUE current = rb_ary_new();
        for (long g = 0; g < RARRAY(watched)->len; g++) {
            VALUE group = rb_ary_entry(watched, g);
            for (long j = 0; j < RARRAY(shapes)->len; j++) {
                VALUE other = rb_ary_entry(shapes, j);
                if (value_array_has(shape_member_of(other), group)
                    && shape_nodes_overlap(trigger, other)) {
                    rb_ary_push(current, rb_ary_new3(2, other, group));
                }
            }
        }
        VALUE old = NIL_P(ts->overlaps) ? rb_ary_new() : ts->overlaps;
        for (long j = 0; j < RARRAY(current)->len; j++) {
            VALUE pair = rb_ary_entry(current, j);
            VALUE other = rb_ary_entry(pair, 0);
            VALUE group = rb_ary_entry(pair, 1);
            if (!overlap_pair_has(old, other, group)) {
                transition_push(enters, trigger, other, group);
            }
        }
        for (long j = 0; j < RARRAY(old)->len; j++) {
            VALUE pair = rb_ary_entry(old, j);
            VALUE other = rb_ary_entry(pair, 0);
            VALUE group = rb_ary_entry(pair, 1);
            if (!overlap_pair_has(current, other, group)) {
                transition_push(exits, trigger, other, group);
            }
        }
        ts->overlaps = current;
    }

    static ID id_enter, id_exit;
    if (!id_enter) {
        id_enter = rb_intern("internal_trigger_enter");
        id_exit = rb_intern("internal_trigger_exit");
    }
    VALUE lists[2] = { enters, exits };
    ID ids[2] = { id_enter, id_exit };
    for (int kind = 0; kind < 2; kind++) {
        for (long i = 0; i < RARRAY(lists[kind])->len; i++) {
            VALUE tr = rb_ary_entry(lists[kind], i);
            VALUE trigger = rb_ary_entry(tr, 0);
            if (sn_get(trigger)->engine != engine) continue;
            rb_funcall(trigger, ids[kind], 3, trigger,
                       rb_ary_entry(tr, 1), rb_ary_entry(tr, 2));
        }
    }
}

// Sound

static VALUE sound_toy(VALUE self) {
    VALUE sounds = sn_get(self)->parent;
    return NIL_P(sounds) ? Qnil : sn_get(sounds)->parent;
}

static bool sound_resolve_path(VALUE self, char* out, size_t cap) {
    sn_t* sound = sn_get(self);
    if (NIL_P(sound->sound_path)) {
        return false;
    }
    const char* path = StringValueCStr(sound->sound_path);
    if (path[0] == '/') {
        return (size_t)snprintf(out, cap, "%s", path) < cap;
    }

    VALUE toyv = sound_toy(self);
    if (NIL_P(toyv) || !sn_get(toyv)->def || !sn_get(toyv)->def->root) {
        return false;
    }
    char assets[sizeof g_root];
    snprintf(assets, sizeof assets, "%s", g_root);
    char* slash = strrchr(assets, '/');
    if (slash) {
        *slash = 0;
    } else {
        snprintf(assets, sizeof assets, ".");
    }

    // Def locations were relative to <pack>/defs/. The extracted layout
    // omits that synthetic defs directory, so ../sound/... maps directly to
    // <assets>/<pack>/sound/....
    while (strncmp(path, "../", 3) == 0) {
        path += 3;
    }
    return (size_t)snprintf(out, cap, "%s/%s/%s", assets,
                            sn_get(toyv)->def->root, path) < cap;
}

static VALUE sound_path(VALUE self) {
    return sn_get(self)->sound_path;
}

static VALUE sound_path_set(VALUE self, VALUE path) {
    StringValue(path);
    sn_t* sound = sn_get(self);
    audio_stop_owner(sound->sound_owner);
    sound->sound_path = path;
    sound->audio_sample = -1;
    sound->sound_window_valid = 0;
    return path;
}

static double sound_engine_time(const sn_t* sound) {
    return NIL_P(sound->engine) ? 0.0 : sn_get(sound->engine)->time;
}

static bool sound_period_allows(sn_t* sound) {
    if (sound->sound_period <= 0.0 || sound->sound_max <= 0) {
        return true;
    }
    const double now = sound_engine_time(sound);
    if (!sound->sound_window_valid || now < sound->sound_window_start ||
        now - sound->sound_window_start >= sound->sound_period) {
        sound->sound_window_valid = 1;
        sound->sound_window_start = now;
        sound->sound_window_count = 0;
    }
    return sound->sound_window_count < sound->sound_max;
}

static bool sound_start(VALUE self, VALUE volume, bool looping) {
    sn_t* sound = sn_get(self);
    if (!sound_period_allows(sound)) {
        return false;
    }
    if (sound->audio_sample < 0) {
        char path[2048];
        if (!sound_resolve_path(self, path, sizeof path)) {
            fprintf(stderr, "[audio] cannot resolve Sound path\n");
            return false;
        }
        sound->audio_sample = audio_sample_load(path);
    }
    if (sound->audio_sample < 0) {
        return false;
    }
    if (!audio_play(sound->audio_sample, sound->sound_owner,
                    (float)NUM2DBL(volume), looping)) {
        return false;
    }
    if (sound->sound_period > 0.0 && sound->sound_max > 0) {
        sound->sound_window_count++;
    }
    return true;
}

static VALUE sound_play(VALUE self, VALUE volume) {
    sound_start(self, volume, false);
    return Qnil;
}

// The argument is volume, as used by the shipped F10 script (`tune.loop(1)`),
// not a boolean looping toggle.
static VALUE sound_loop(VALUE self, VALUE volume) {
    sound_start(self, volume, true);
    return Qnil;
}

static VALUE sound_stop(VALUE self) {
    audio_stop_owner(sn_get(self)->sound_owner);
    return Qnil;
}

static VALUE sound_period_length(VALUE self) {
    return rb_float_new(sn_get(self)->sound_period);
}

static VALUE sound_period_length_set(VALUE self, VALUE period) {
    sn_t* sound = sn_get(self);
    const double value = NUM2DBL(period);
    sound->sound_period = value > 0.0 ? value : 0.0;
    sound->sound_window_valid = 0;
    return period;
}

static VALUE sound_max_per_period(VALUE self) {
    return INT2NUM(sn_get(self)->sound_max);
}

static VALUE sound_max_per_period_set(VALUE self, VALUE maximum) {
    sn_t* sound = sn_get(self);
    const int value = NUM2INT(maximum);
    sound->sound_max = value > 0 ? value : 0;
    sound->sound_window_valid = 0;
    return maximum;
}

static VALUE eng_scale(VALUE self) {
    return rb_float_new(sn_get(self)->scale);
}

static VALUE eng_scale_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    const double f = NUM2DBL(v);
    if (f == n->scale) {
        return v;
    }
    n->scale = f;
    rb_funcall(self, rb_intern("scale_changed"), 0);
    return v;
}

#define ENG_ACCESSOR(field) \
    static VALUE eng_##field(VALUE self) { \
        return rb_float_new(sn_get(self)->field); \
    } \
    static VALUE eng_##field##_set(VALUE self, VALUE v) { \
        sn_get(self)->field = NUM2DBL(v); \
        return v; \
    }
ENG_ACCESSOR(gravity)
ENG_ACCESSOR(timestep)
ENG_ACCESSOR(timescale)
#undef ENG_ACCESSOR

static VALUE eng_time(VALUE self) {
    return rb_float_new(sn_get(self)->time);
}

static VALUE eng_paused(VALUE self) {
    return sn_get(self)->paused ? Qtrue : Qfalse;
}

static VALUE eng_paused_set(VALUE self, VALUE v) {
    sn_get(self)->paused = RTEST(v);
    return v;
}

static VALUE eng_run_steps(VALUE self, VALUE n) {
    sn_t* e = sn_get(self);
    const int steps = NUM2INT(n);
    if (!e->paused && steps > 0) {
        if (is_default_engine(self)) {
            for (int i = 0; i < steps; i++) {
                phys_steps(1);
                e->time += e->timestep;
                dispatch_trigger_transitions(self);
            }
        } else {
            e->time += steps * e->timestep;
        }
    }
    return Qnil;
}

// Physics timers: proc fires every `period` ticks (1/100 s), first at
// `start` ticks from now; proc.call receives the SCHEDULED tick so the
// framework's `t % n` patterns hold even when a frame delivers several
// ticks at once.
static VALUE eng_add_phys_timer(VALUE self, VALUE proc, VALUE period,
                                VALUE start) {
    sn_t* e = sn_get(self);
    VALUE entry = rb_ary_new3(3, proc, rb_float_new(NUM2DBL(period)),
                              rb_float_new(e->timer_tick + NUM2DBL(start)));
    rb_ary_push(e->timers, entry);
    return proc;
}

static VALUE eng_remove_phys_timer(VALUE self, VALUE proc) {
    sn_t* e = sn_get(self);
    for (long i = RARRAY(e->timers)->len - 1; i >= 0; i--) {
        VALUE entry = rb_ary_entry(e->timers, i);
        if (RTEST(rb_equal(rb_ary_entry(entry, 0), proc))) {
            rb_funcall(e->timers, rb_intern("delete_at"), 1, LONG2NUM(i));
        }
    }
    return proc;
}

static VALUE eng_dispatch_timers(VALUE self, VALUE delta) {
    sn_t* e = sn_get(self);
    if (e->paused) {
        return Qnil;
    }
    e->timer_tick += NUM2DBL(delta);
    ID id_call = rb_intern("call");
    // index-walk: procs may add/remove timers from inside call
    for (long i = 0; i < RARRAY(e->timers)->len; i++) {
        VALUE entry = rb_ary_entry(e->timers, i);
        const double period = NUM2DBL(rb_ary_entry(entry, 1));
        double next = NUM2DBL(rb_ary_entry(entry, 2));
        while (next <= e->timer_tick) {
            rb_funcall(rb_ary_entry(entry, 0), id_call, 1,
                       INT2NUM((long)next));
            next += period > 0 ? period : e->timer_tick - next + 1;
            rb_ary_store(entry, 2, rb_float_new(next));
        }
    }
    return Qnil;
}

// RCore

static VALUE core_add_engine(VALUE self, VALUE e) {
    rb_ary_push(sn_get(self)->engines, e);
    return e;
}

static VALUE core_remove_engine(VALUE self, VALUE e) {
    rb_ary_delete(sn_get(self)->engines, e);
    return e;
}

static VALUE core_each_engine(VALUE self) {
    sn_t* n = sn_get(self);
    for (long i = 0; i < RARRAY(n->engines)->len; i++) {
        rb_yield(rb_ary_entry(n->engines, i));
    }
    return self;
}

// Souptoys

static bool resource_path(const char* key, char* out, size_t cap) {
    if (key[0] == '/' || strstr(key, "..")) {
        return false;
    }
    return (size_t)snprintf(out, cap, "%s/%s", g_root, key) < cap;
}

static VALUE soup_resource_exists(VALUE self, VALUE key) {
    (void)self;
    char p[1400];
    if (!resource_path(StringValueCStr(key), p, sizeof p)) {
        return Qfalse;
    }
    FILE* f = fopen(p, "rb");
    if (f) {
        fclose(f);
        return Qtrue;
    }
    return Qfalse;
}

static VALUE soup_resource_load(VALUE self, VALUE key) {
    (void)self;
    char p[1400];
    FILE* f = NULL;
    if (resource_path(StringValueCStr(key), p, sizeof p)) {
        f = fopen(p, "rb");
    }
    if (!f) {
        rb_raise(rb_eRuntimeError, "no such resource: %s",
                 StringValueCStr(key));
    }
    fseek(f, 0, SEEK_END);
    const long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    VALUE s = rb_str_new(NULL, len);
    if (fread(RSTRING(s)->ptr, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        rb_raise(rb_eRuntimeError, "short read: %s", StringValueCStr(key));
    }
    fclose(f);
    return s;
}

static VALUE soup_set_license_policy(VALUE self, VALUE v) {
    sn_get(self)->license_policy = v;
    return v;
}

static VALUE soup_get_license_policy(VALUE self) {
    return sn_get(self)->license_policy;
}

static VALUE soup_load_paths(VALUE self) {
    VALUE v = sn_get(self)->load_paths;
    return NIL_P(v) ? rb_ary_new() : v;
}

static VALUE soup_load_paths_set(VALUE self, VALUE v) {
    sn_get(self)->load_paths = v;
    return v;
}

static VALUE soup_exe_path(VALUE self) {
    (void)self;
    return rb_str_new2(g_root);
}

static VALUE soup_console_open_p(VALUE self) {
    (void)self;
    return Qtrue; // dev: keeps the framework's gated Object#puts audible
}

// registration

static VALUE lookup_super(const char* name) {
    if (strcmp(name, "Object") == 0) {
        return rb_cObject;
    }
    if (strcmp(name, "Exception") == 0) {
        return rb_eException;
    }
    return cls_find(name);
}

static void define_api(void) {
    // pass 1: classes (in .inc order, supers first) + default allocator
#define RBA_CLASS(name, super) \
    { \
        VALUE c = rb_define_class(#name, lookup_super(#super)); \
        if (strcmp(#super, "Exception") != 0) { \
            rb_define_alloc_func(c, alloc_generic); \
        } \
        g_reg[g_nreg].cname = #name; \
        g_reg[g_nreg].cls = c; \
        g_nreg++; \
    }
#define RBA_METHOD(cls, name, argc)
#define RBA_SMETHOD(cls, name, argc)
#define RBA_PMETHOD(cls, name, argc)
#include "ruby_api.inc"
#undef RBA_CLASS
#undef RBA_METHOD
#undef RBA_SMETHOD
#undef RBA_PMETHOD

    // pass 2: every API method as a variadic stub
#define RBA_CLASS(name, super)
#define RBA_METHOD(cls, name, argc) \
    rb_define_method(cls_find(#cls), name, rba_stub, -1);
#define RBA_SMETHOD(cls, name, argc) \
    rb_define_singleton_method(cls_find(#cls), name, rba_stub, -1);
#define RBA_PMETHOD(cls, name, argc) \
    rb_define_private_method(cls_find(#cls), name, rba_stub, -1);
#include "ruby_api.inc"
#undef RBA_CLASS
#undef RBA_METHOD
#undef RBA_SMETHOD
#undef RBA_PMETHOD
}

static void define_real(void) {
    VALUE c;

    rb_define_alloc_func(cls_find("SoupNodeCollection"), alloc_coll);
    rb_define_alloc_func(cls_find("Toy"), alloc_toy);
    rb_define_alloc_func(cls_find("Limb"), alloc_limb);
    rb_define_alloc_func(cls_find("REngine"), alloc_engine);
    rb_define_alloc_func(cls_find("RCore"), alloc_core);
    rb_define_alloc_func(cls_find("Souptoys"), alloc_soup);
    rb_define_alloc_func(cls_find("Sound"), alloc_sound);

    c = cls_find("SoupNode");
    rb_define_method(c, "engine", sn_engine, 0);
    rb_define_method(c, "parent", sn_parent, 0);
    rb_define_method(c, "sid", sn_sid, 0);
    rb_define_method(c, "sid=", sn_sid_set, 1);

    c = cls_find("SoupNodeCollection");
    rb_define_method(c, "<<", coll_lshift, 1);
    rb_define_method(c, "add", coll_add, 1);
    rb_define_method(c, "remove", coll_remove, 1);
    rb_define_method(c, "each", coll_each, 0);
    rb_define_method(c, "count", coll_count, 0);
    rb_define_method(c, "by_index", coll_by_index, 1);
    rb_define_method(c, "by_sid", coll_by_sid, 1);

    c = cls_find("Toy");
    rb_define_method(c, "toys", toy_toys, 0);
    rb_define_method(c, "limbs", toy_limbs, 0);
    rb_define_method(c, "joints", toy_joints, 0);
    rb_define_method(c, "rotational_joints", toy_rjoints, 0);
    rb_define_method(c, "sprites", toy_sprites, 0);
    rb_define_method(c, "sounds", toy_sounds, 0);
    rb_define_method(c, "toy_id", toy_id, 0);
    rb_define_method(c, "toy_instance_id", toy_instance_id, 0);
    rb_define_method(c, "is_sticky=", toy_sticky_set, 1);
    rb_define_method(c, "is_sticky?", toy_sticky_p, 0);

    c = cls_find("Limb");
    rb_define_method(c, "sprites", limb_sprites, 0);
    rb_define_method(c, "shapes", limb_shapes, 0);
    rb_define_method(c, "linear_motors", limb_lmotors, 0);
    rb_define_method(c, "rotational_motors", limb_rmotors, 0);
    rb_define_method(c, "position", limb_position, 0);
    rb_define_method(c, "position=", limb_position_set, 1);
    rb_define_method(c, "orientation", limb_orientation, 0);
    rb_define_method(c, "orientation=", limb_orientation_set, 1);
    rb_define_method(c, "shock_order", limb_shock_order, 0);
    rb_define_method(c, "shock_order=", limb_shock_order_set, 1);
    rb_define_method(c, "mass", limb_mass, 0);
    rb_define_method(c, "fixed_move", limb_fixed_move, 0);
    rb_define_method(c, "to_world", limb_to_world, 1);
    rb_define_method(c, "to_local", limb_to_local, 1);
    rb_define_method(c, "momentum", limb_momentum, 0);
    rb_define_method(c, "momentum=", limb_momentum_set, 1);
    rb_define_method(c, "angular_momentum", limb_angular_momentum, 0);
    rb_define_method(c, "angular_momentum=", limb_angular_momentum_set, 1);
    rb_define_method(c, "inertia_tensor", limb_inertia_tensor, 0);

    c = cls_find("REngine");
    rb_define_method(c, "toys", eng_toys, 0);
    rb_define_method(c, "input_each", eng_input_each, 0);
    rb_define_method(c, "input_add", eng_input_add, 1);
    rb_define_method(c, "input_remove", eng_input_remove, 1);
    rb_define_method(c, "scene_bottom_left", eng_scene_bl, 0);
    rb_define_method(c, "scene_bottom_left=", eng_scene_bl_set, 1);
    rb_define_method(c, "scene_top_right", eng_scene_tr, 0);
    rb_define_method(c, "scene_top_right=", eng_scene_tr_set, 1);
    rb_define_method(c, "canvas_top_left", eng_canvas_tl, 0);
    rb_define_method(c, "canvas_top_left=", eng_canvas_tl_set, 1);
    rb_define_method(c, "canvas_bottom_right", eng_canvas_br, 0);
    rb_define_method(c, "canvas_bottom_right=", eng_canvas_br_set, 1);
    rb_define_method(c, "screen_top_left", eng_screen_tl, 0);
    rb_define_method(c, "screen_bottom_right", eng_screen_br, 0);
    rb_define_method(c, "view_to_canvas", eng_view_to_canvas, 1);
    rb_define_method(c, "canvas_to_view", eng_canvas_to_view, 1);
    rb_define_method(c, "canvas_to_scene", eng_canvas_to_scene, 1);
    rb_define_method(c, "scene_to_canvas", eng_scene_to_canvas, 1);
    rb_define_method(c, "view_to_scene", eng_view_to_scene, 1);
    rb_define_method(c, "scene_to_view", eng_scene_to_view, 1);
    rb_define_method(c, "input_grab", eng_input_grab, 3);
    rb_define_method(c, "input_move", eng_input_move, 2);
    rb_define_method(c, "input_release", eng_input_release, 3);
    rb_define_method(c, "scale", eng_scale, 0);
    rb_define_method(c, "scale=", eng_scale_set, 1);
    rb_define_method(c, "gravity", eng_gravity, 0);
    rb_define_method(c, "gravity=", eng_gravity_set, 1);
    rb_define_method(c, "timestep", eng_timestep, 0);
    rb_define_method(c, "timestep=", eng_timestep_set, 1);
    rb_define_method(c, "timescale", eng_timescale, 0);
    rb_define_method(c, "timescale=", eng_timescale_set, 1);
    rb_define_method(c, "time", eng_time, 0);
    rb_define_method(c, "paused", eng_paused, 0);
    rb_define_method(c, "paused=", eng_paused_set, 1);
    rb_define_method(c, "run_steps", eng_run_steps, 1);
    rb_define_method(c, "add_phys_timer", eng_add_phys_timer, 3);
    rb_define_method(c, "remove_phys_timer", eng_remove_phys_timer, 1);
    rb_define_method(c, "dispatch_timers", eng_dispatch_timers, 1);

    c = cls_find("RCore");
    rb_define_method(c, "add_engine", core_add_engine, 1);
    rb_define_method(c, "remove_engine", core_remove_engine, 1);
    rb_define_method(c, "each_engine", core_each_engine, 0);

    c = cls_find("RInput");
    rb_define_method(c, "limb", input_limb, 0);

    c = cls_find("Shape");
    rb_define_method(c, "member_of", shape_member_of, 0);
    rb_define_method(c, "member_of=", shape_member_of_set, 1);
    rb_define_method(c, "trigger_on", shape_trigger_on, 0);
    rb_define_method(c, "trigger_on=", shape_trigger_on_set, 1);
    rb_define_method(c, "triggers_overlapping", shape_triggers_overlapping, 1);
    rb_define_method(c, "limb", shape_limb, 0);

    c = cls_find("Sound");
    rb_define_method(c, "loop", sound_loop, 1);
    rb_define_method(c, "max_sounds_per_period", sound_max_per_period, 0);
    rb_define_method(c, "max_sounds_per_period=", sound_max_per_period_set, 1);
    rb_define_method(c, "path", sound_path, 0);
    rb_define_method(c, "path=", sound_path_set, 1);
    rb_define_method(c, "period_length", sound_period_length, 0);
    rb_define_method(c, "period_length=", sound_period_length_set, 1);
    rb_define_method(c, "play", sound_play, 1);
    rb_define_method(c, "stop", sound_stop, 0);

    c = cls_find("Souptoys");
    rb_define_method(c, "resource_exists", soup_resource_exists, 1);
    rb_define_method(c, "resource_load", soup_resource_load, 1);
    rb_define_method(c, "set_license_policy", soup_set_license_policy, 1);
    rb_define_method(c, "get_license_policy", soup_get_license_policy, 0);
    rb_define_method(c, "load_paths", soup_load_paths, 0);
    rb_define_method(c, "load_paths=", soup_load_paths_set, 1);
    rb_define_method(c, "exe_path", soup_exe_path, 0);
    rb_define_method(c, "console_open?", soup_console_open_p, 0);
}

// boot

static void report_exception(const char* what) {
    VALUE err = rb_gv_get("$!");
    if (NIL_P(err)) {
        fprintf(stderr, "[rubyhost] %s failed (no $!)\n", what);
        return;
    }
    VALUE msg = rb_funcall(err, rb_intern("message"), 0);
    fprintf(stderr, "[rubyhost] %s failed: %s: %s\n", what,
            rb_obj_classname(err), StringValueCStr(msg));
    VALUE bt = rb_funcall(err, rb_intern("backtrace"), 0);
    if (TYPE(bt) == T_ARRAY) {
        const long n = RARRAY(bt)->len < 12 ? RARRAY(bt)->len : 12;
        for (long i = 0; i < n; i++) {
            VALUE line = rb_ary_entry(bt, i);
            fprintf(stderr, "    %s\n", StringValueCStr(line));
        }
    }
    ruby_errinfo = Qnil;
}

bool rbh_eval(const char* code, const char* what) {
    int state = 0;
    rb_eval_string_protect(code, &state);
    if (state == 0) {
        return true;
    }
    report_exception(what);
    return false;
}

// rb_funcall behind rb_protect: script exceptions (e.g. inside timer procs)
// must not longjmp through the C frame loop.
struct fcall {
    VALUE recv;
    ID id;
    int argc;
    VALUE argv[3];
};

static VALUE fcall_thunk(VALUE arg) {
    struct fcall* f = (struct fcall*)arg;
    return rb_funcall2(f->recv, f->id, f->argc, f->argv);
}

static bool fcall_protected(VALUE recv, const char* name, int argc,
                            VALUE a0, VALUE a1, VALUE a2, const char* what) {
    struct fcall f = { recv, rb_intern(name), argc, { a0, a1, a2 } };
    int state = 0;
    rb_protect(fcall_thunk, (VALUE)&f, &state);
    if (state == 0) {
        return true;
    }
    report_exception(what);
    return false;
}

// Per-frame heartbeat: fixed 0.01s steps with an accumulator (clamped like
// the native loop was), driven through the FRAMEWORK's run_steps wrapper so
// engine_execute and pre-timer observers behave as in the original.
void rbh_frame(double dt_ms) {
    static double acc_ms;
    acc_ms += dt_ms;
    int steps = (int)(acc_ms / (PHYS_DT * 1000.0));
    if (steps <= 0) {
        return;
    }
    acc_ms -= steps * (PHYS_DT * 1000.0);
    if (steps > 25) {
        steps = 25; // clamp long stalls
    }
    VALUE eng = rb_gv_get("$default_engine");
    if (NIL_P(eng)) {
        return;
    }
    fcall_protected(eng, "run_steps", 1, INT2FIX(steps), Qnil, Qnil,
                    "run_steps");
    fcall_protected(eng, "dispatch_timers", 1, INT2FIX(steps), Qnil, Qnil,
                    "dispatch_timers");
}

void rbh_screen_size(double w_px, double h_px) {
    g_screen_w = w_px;
    g_screen_h = h_px;
    VALUE core = rb_gv_get("$core");
    if (!NIL_P(core)) {
        fcall_protected(core, "screen_size_changed", 0, Qnil, Qnil, Qnil,
                        "screen_size_changed");
    }
}

static bool eval_resource(const char* key) {
    char p[1400];
    if (!resource_path(key, p, sizeof p)) {
        return false;
    }
    FILE* f = fopen(p, "rb");
    if (!f) {
        fprintf(stderr, "[rubyhost] missing resource %s\n", p);
        return false;
    }
    fseek(f, 0, SEEK_END);
    const long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc((size_t)len + 1);
    const bool ok = fread(buf, 1, (size_t)len, f) == (size_t)len;
    fclose(f);
    buf[len] = 0;
    const bool ran = ok && rbh_eval(buf, key);
    free(buf);
    return ran;
}

// The exact bootstrap snippet Toybox.exe evals to pull souptoys.rb out of the
// resource container (the proto version of the require hook the framework
// then installs for everything else).
static const char* BOOTSTRAP =
    "path = 'souptoys.rb';"
    "begin;"
    "   require path;"
    "rescue LoadError => load_error;"
    "   path << '.rb' if File.extname(path) == '';"
    "   if $engine.resource_exists path;"
    "       eval $engine.resource_load(path);"
    "   else;"
    "       raise load_error;"
    "   end;"
    "end;";

bool rbh_boot(const char* scripts_root) {
    snprintf(g_root, sizeof g_root, "%s", scripts_root);

    ruby_init();
    ruby_script("souptoys_embedded");
    // Ruby 1.8 traps INT/TERM and turns them into SignalExceptions, which
    // our rb_protect frames would swallow, and the app becomes unkillable.
    // The host owns process lifetime, not the interpreter.
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    Init_ext(); // static stringio + syck (yaml needs both)
    define_api();
    define_real();

    // globals, in the original's creation order
    VALUE core = rb_obj_alloc(cls_find("RCore"));
    rb_gv_set("$core", core);
    static const char* engs[] = { "$default_engine", "$ui_engine",
                                  "$grid_engine" };
    for (int i = 0; i < 3; i++) {
        VALUE e = rb_obj_alloc(cls_find("REngine"));
        rb_gv_set(engs[i], e);
        core_add_engine(core, e);
    }
    rb_gv_set("$engine", rb_obj_alloc(cls_find("Souptoys")));

    if (!rbh_eval("$:.clear();", "load path reset")) {
        return false;
    }

    // require 'matrix' has no filesystem to come from; feed e2mmap + matrix
    // from resources, exactly like the original's fallback
    int state = 0;
    rb_eval_string_protect("require 'matrix'", &state);
    if (state) {
        ruby_errinfo = Qnil;
        if (!eval_resource("e2mmap.rb") ||
            !rbh_eval("$\" << 'e2mmap.rb'", "provide e2mmap") ||
            !eval_resource("matrix.rb") ||
            !rbh_eval("$\" << 'matrix.rb'", "provide matrix")) {
            return false;
        }
    }

    if (!rbh_eval(BOOTSTRAP, "souptoys.rb bootstrap")) {
        return false;
    }

    // toy classes for the defs we instantiate natively; just World for now
    // (full toy import lands with the object-model bridge)
    if (!rbh_eval("load 'world.rb'", "world.rb")) {
        return false;
    }

    if (!rbh_eval("$core.core_loaded", "core_loaded")) {
        return false;
    }
    return true;
}

// Kernel#load only honours $LOAD_PATH (cleared at boot) for relative paths,
// so toy-class dirs must reach Ruby as ABSOLUTE paths or the resolver
// silently falls back to a script-less Toy subclass.
static const char* abs_dir(const char* dir, char* buf) {
    if (!dir) {
        return ".";
    }
    return realpath(dir, buf) ? buf : dir;
}

bool rbh_load_toy_class(const char* class_name, const char* class_dir) {
    char abs[1024], code[2048];
    snprintf(code, sizeof code,
             "ToyClassResolver.load_toy_class('%s', Pathname.new('%s'))\n",
             class_name, abs_dir(class_dir, abs));
    return rbh_eval(code, class_name);
}

bool rbh_spawn_toy(const char* class_name, const char* class_dir,
                   double x_m, double y_m) {
    char abs[1024], code[2048];
    snprintf(code, sizeof code,
             "t = ToyClassResolver.load_toy_class('%s', Pathname.new('%s')).new\n"
             "t.move(Vector[%.6f, %.6f])\n"
             "$default_engine.toys << t\n",
             class_name, abs_dir(class_dir, abs), x_m, y_m);
    return rbh_eval(code, class_name);
}

bool rbh_view_to_scene(double x_px, double y_px, double* x_m, double* y_m) {
    const VALUE eng = rb_gv_get("$default_engine");
    if (!sn_p(eng)) {
        return false;
    }
    const sn_t* n = sn_get(eng);
    if (n->scale == 0.0) {
        return false;
    }
    const double canvas_x = x_px - n->canvas_tl[0];
    const double canvas_y = y_px - n->canvas_tl[1];
    const double canvas_h = n->canvas_br[1] - n->canvas_tl[1];
    if (x_m) {
        *x_m = n->scene_bl[0] + canvas_x / n->scale;
    }
    if (y_m) {
        *y_m = n->scene_bl[1] + (canvas_h - canvas_y) / n->scale;
    }
    return true;
}

// Mouse dispatch: the app picks the hit sprite (alpha test is scene-side)
// and hands VIEW coordinates here; the Sprite's internal_mouse_* framework
// methods convert to scene space and bubble the event up the node tree
// (Limb#mouse_down runs the default grab, ToyContainer#mouse_move drives
// input_move).
static VALUE sprite_node(int sprite) {
    if (sprite < 0 || sprite >= MAX_SCENE_SPRITES) {
        return Qnil;
    }
    VALUE v = g_scene_sprite[sprite];
    return v ? v : Qnil;
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

void rbh_shutdown(void) {
    ruby_finalize();
}
