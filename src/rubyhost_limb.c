// Limb: kinematic state accessors bridged to the physics body. Shape:
// memberOf/trigger groups, overlap queries, and the per-step trigger
// transition dispatch.
#include "physics.h"
#include <math.h>
#include "rubyhost_internal.h"

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

// Shape

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
void dispatch_trigger_transitions(VALUE engine) {
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

void rbh_register_limb(void) {
    VALUE c = cls_find("Limb");
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

    c = cls_find("Shape");
    rb_define_method(c, "member_of", shape_member_of, 0);
    rb_define_method(c, "member_of=", shape_member_of_set, 1);
    rb_define_method(c, "trigger_on", shape_trigger_on, 0);
    rb_define_method(c, "trigger_on=", shape_trigger_on_set, 1);
    rb_define_method(c, "triggers_overlapping", shape_triggers_overlapping, 1);
    rb_define_method(c, "limb", shape_limb, 0);
}
