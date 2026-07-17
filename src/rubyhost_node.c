// Object model: sn_t plumbing, the Ruby Vector bridge, allocators, and the
// SoupNode / SoupNodeCollection / Toy / RCore method surface.
#include "physics.h"
#include <stdlib.h>
#include "rubyhost_internal.h"

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

int sn_p(VALUE v) {
    return TYPE(v) == T_DATA && RDATA(v)->dmark == sn_gcmark;
}

sn_t* sn_get(VALUE v) {
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

VALUE vec_new(double x, double y) {
    return rb_funcall(rb_const_get(rb_cObject, rb_intern("Vector")),
                      rb_intern("[]"), 2, rb_float_new(x), rb_float_new(y));
}

void vec_get(VALUE v, double* x, double* y) {
    ID idx = rb_intern("[]");
    *x = NUM2DBL(rb_funcall(v, idx, 1, INT2FIX(0)));
    *y = NUM2DBL(rb_funcall(v, idx, 1, INT2FIX(1)));
}

// allocators

VALUE alloc_generic(VALUE klass) {
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
    VALUE args = rb_ary_new3(2, child, c->engine);
    int state = 0;
    const VALUE result = rb_protect(sn_set_engine_protected, args, &state);
    if (state || result != Qtrue) {
        const VALUE error = state ? rb_gv_get("$!") : Qnil;
        if (sn_get(child)->kind == SN_TOY) {
            // Guaranteed native cleanup even if a Ruby engine_changed
            // callback raised before the normal reverse transition could run.
            toy_unrealize(child);
        }
        VALUE rollback = rb_ary_new3(2, child, Qnil);
        int rollback_state = 0;
        rb_protect(sn_set_engine_protected, rollback, &rollback_state);
        rb_ary_delete(c->items, child);
        sn_get(child)->parent = Qnil;
        if (state) {
            rb_exc_raise(error);
        }
        rb_raise(rb_eRuntimeError, "could not realize toy resources");
    }
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

// RCore

VALUE core_add_engine(VALUE self, VALUE e) {
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

void rbh_register_nodes(void) {
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

    c = cls_find("RCore");
    rb_define_method(c, "add_engine", core_add_engine, 1);
    rb_define_method(c, "remove_engine", core_remove_engine, 1);
    rb_define_method(c, "each_engine", core_each_engine, 0);
}
