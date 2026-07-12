// Embedded Ruby 1.8.6 host, the script side of the engine.
// Class surface comes from the reverse-engineered Toybox binding surface:
// every method is first bound to a logging stub so framework load-time aliases
// (`alias :internal_render :render` etc.) always resolve; the subset the boot
// path needs is then rebound to real implementations backed by toydefs/physics.
#include "rubyhost.h"
#include "physics.h"
#include "toydefs.h"
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

enum { SN_GENERIC, SN_COLL, SN_TOY, SN_LIMB, SN_ENGINE, SN_CORE, SN_SOUP };
#define SN_NCOLLS 6

typedef struct {
    int kind;
    VALUE sid, parent, engine;
    VALUE items;            // SN_COLL
    VALUE colls[SN_NCOLLS];
    const toydef_t* def;    // SN_TOY
    int instance_id;        // SN_TOY
    int sticky;             // SN_TOY
    const td_limb* ldef;    // SN_LIMB
    double px, py, orient, shock;
    VALUE inputs;           // SN_ENGINE
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
    for (int i = 0; i < SN_NCOLLS; i++) {
        rb_gc_mark(n->colls[i]);
    }
    rb_gc_mark(n->inputs);
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
    n->license_policy = n->load_paths = Qnil;
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

// engine (re)parenting

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
    n->colls[0] = coll_new_named("SpriteContainer", v, Qnil);
    n->colls[1] = coll_new_named("ShapeContainer", v, Qnil);
    n->colls[2] = coll_new_named("LinearMotorContainer", v, Qnil);
    n->colls[3] = coll_new_named("RotationalMotorContainer", v, Qnil);
    if (l) {
        n->sid = ID2SYM(rb_intern(l->name));
        n->px = l->rest_pos[0] * base_scale;
        n->py = l->rest_pos[1] * base_scale;
        n->orient = l->rest_orient;
    }
    return v;
}

static VALUE alloc_limb(VALUE klass) {
    (void)klass;
    return alloc_limb_at(NULL, 1.0);
}

static int g_next_instance_id = 1;

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
        // TODO(step2): joints, sprites, shapes, motors, sounds
    }
    return self;
}

static VALUE alloc_engine(VALUE klass) {
    VALUE v = sn_wrap(klass, SN_ENGINE);
    sn_t* n = sn_get(v);
    n->inputs = rb_ary_new();
    n->colls[0] = coll_new_named("ToyContainer", v, v);
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

static VALUE limb_position(VALUE self) {
    sn_t* n = sn_get(self);
    return vec_new(n->px, n->py);
}

static VALUE limb_position_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    vec_get(v, &n->px, &n->py);
    return v;
}

static VALUE limb_orientation(VALUE self) {
    return rb_float_new(sn_get(self)->orient);
}

static VALUE limb_orientation_set(VALUE self, VALUE v) {
    sn_get(self)->orient = NUM2DBL(v);
    return v;
}

static VALUE limb_shock_order(VALUE self) {
    return rb_float_new(sn_get(self)->shock);
}

static VALUE limb_shock_order_set(VALUE self, VALUE v) {
    sn_get(self)->shock = NUM2DBL(v);
    return v;
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

static VALUE eng_scene_bl_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    vec_get(v, &n->scene_bl[0], &n->scene_bl[1]);
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
    vec_get(v, &n->scene_tr[0], &n->scene_tr[1]);
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
    vec_get(v, &n->canvas_tl[0], &n->canvas_tl[1]);
    rb_funcall(self, rb_intern("canvas_changed"), 0);
    return v;
}

static VALUE eng_canvas_br(VALUE self) {
    sn_t* n = sn_get(self);
    return vec_new(n->canvas_br[0], n->canvas_br[1]);
}

static VALUE eng_canvas_br_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    vec_get(v, &n->canvas_br[0], &n->canvas_br[1]);
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

static VALUE eng_scale(VALUE self) {
    return rb_float_new(sn_get(self)->scale);
}

static VALUE eng_scale_set(VALUE self, VALUE v) {
    sn_get(self)->scale = NUM2DBL(v);
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
            phys_steps(steps);
        }
        e->time += steps * e->timestep;
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

    c = cls_find("RCore");
    rb_define_method(c, "add_engine", core_add_engine, 1);
    rb_define_method(c, "remove_engine", core_remove_engine, 1);
    rb_define_method(c, "each_engine", core_each_engine, 0);

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

void rbh_shutdown(void) {
    ruby_finalize();
}
