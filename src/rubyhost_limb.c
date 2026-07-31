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

// An Integer, not a Float, and it reaches the broadphase: negative bodies are
// left out of shock ordering. Scripts set it after realization.
static VALUE limb_shock_order(VALUE self) {
    sn_t* n = sn_get(self);
    return INT2NUM(n->body >= 0 ? phys_body_shock_order(n->body) : n->shock);
}

static VALUE limb_shock_order_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    n->shock = NUM2INT(v);
    if (n->body >= 0) phys_body_set_shock_order(n->body, n->shock);
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

// Keep script coordinates on the engine's float grid: narrow the argument to
// float, transform through the body's own 3x3, and round the result. to_local
// must use the inverse matrix: the determinant and reciprocal it is built from
// leave visible rounding. Unrealized limbs have no body and fall back to the
// same arithmetic written out.
static VALUE limb_xform(VALUE self, VALUE v, int inverse) {
    sn_t* n = sn_get(self);
    double px, py;
    vec_get(v, &px, &py);
    const float lx = (float)px, ly = (float)py;

    if (n->body >= 0) {
        float m[9];
        phys_body_transform(n->body, inverse ? NULL : m, inverse ? m : NULL);
        return vec_new((float)((double)lx * m[0] + (double)ly * m[1] + m[2]),
                       (float)((double)lx * m[3] + (double)ly * m[4] + m[5]));
    }

    double x, y, th;
    limb_pose(n, &x, &y, &th);
    const float c = (float)cos(th), sn = (float)sin(th);
    if (!inverse) {
        return vec_new((float)(x + c * lx - sn * ly),
                       (float)(y + sn * lx + c * ly));
    }
    const double dx = lx - x, dy = ly - y;
    return vec_new((float)(c * dx + sn * dy), (float)(-sn * dx + c * dy));
}

static VALUE limb_to_world(VALUE self, VALUE v) {
    return limb_xform(self, v, 0);
}

static VALUE limb_to_local(VALUE self, VALUE v) {
    return limb_xform(self, v, 1);
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
    // Def inertia is toy-local units^2; scripts expect world units. Report the
    // float the body runs on, not a double recomputation of it.
    if (n->body >= 0) {
        phys_params p;
        phys_body_get_params(n->body, &p);
        return rb_float_new(p.inertia);
    }
    return rb_float_new((float)((double)n->ldef->inertia
                                * n->lscale * n->lscale));
}

static VALUE limb_mass(VALUE self) {
    sn_t* n = sn_get(self);
    return n->ldef ? rb_float_new(n->ldef->mass) : Qnil;
}

static VALUE limb_fixed_move(VALUE self) {
    sn_t* n = sn_get(self);
    return (n->ldef && n->ldef->fixed_move) ? Qtrue : Qfalse;
}

// Live per-limb physics parameters, named as the original's Limb exposes them.
// Each pair reads phys_params, sets one field and writes it back.
#define LIMB_PARAM(name, field)                                          \
    static VALUE limb_##name(VALUE self) {                               \
        sn_t* n = sn_get(self);                                          \
        if (n->body < 0) return Qnil;                                    \
        phys_params p;                                                   \
        phys_body_get_params(n->body, &p);                               \
        return rb_float_new(p.field);                                    \
    }                                                                    \
    static VALUE limb_##name##_set(VALUE self, VALUE v) {                \
        sn_t* n = sn_get(self);                                          \
        if (n->body < 0) return v;                                       \
        phys_params p;                                                   \
        phys_body_get_params(n->body, &p);                               \
        p.field = (float)NUM2DBL(v);                                     \
        phys_body_set_params(n->body, &p);                               \
        return v;                                                        \
    }

LIMB_PARAM(mouse_stiffness, mouse_stiffness)
LIMB_PARAM(mouse_dampener, mouse_dampener)
LIMB_PARAM(air_linear, air_linear)
LIMB_PARAM(air_angular, air_angular)
LIMB_PARAM(gravity, gravity)
LIMB_PARAM(mat_velocity_response, material[0])
LIMB_PARAM(mat_stiffness, material[1])
LIMB_PARAM(mat_dampener, material[2])
LIMB_PARAM(mat_kinetic_friction, material[3])
LIMB_PARAM(mat_static_friction, material[4])
#undef LIMB_PARAM

// A Vector, so it does not fit LIMB_PARAM.
// Never nil: Limb#dup calls .dup on whatever this returns.
static VALUE limb_centre_of_resistance(VALUE self) {
    sn_t* n = sn_get(self);
    if (n->body < 0) return vec_new(0.0f, 0.0f);
    phys_params p;
    phys_body_get_params(n->body, &p);
    return vec_new(p.centre_of_resistance[0], p.centre_of_resistance[1]);
}

static VALUE limb_centre_of_resistance_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    if (n->body < 0 || NIL_P(v)) return v;
    phys_params p;
    phys_body_get_params(n->body, &p);
    double x, y;
    vec_get(v, &x, &y);
    p.centre_of_resistance[0] = (float)x;
    p.centre_of_resistance[1] = (float)y;
    phys_body_set_params(n->body, &p);
    return v;
}

// Joint

// Live joint parameters, named as the original's Joint class exposes them.

#define JOINT_PARAM(name, field, wrap, unwrap)                           \
    static VALUE joint_##name(VALUE self) {                              \
        sn_t* n = sn_get(self);                                          \
        if (n->joint < 0) return Qnil;                                   \
        phys_joint_params p;                                             \
        phys_joint_get_params(n->joint, &p);                             \
        return wrap(p.field);                                            \
    }                                                                    \
    static VALUE joint_##name##_set(VALUE self, VALUE v) {               \
        sn_t* n = sn_get(self);                                          \
        if (n->joint < 0) return v;                                      \
        phys_joint_params p;                                             \
        phys_joint_get_params(n->joint, &p);                             \
        p.field = unwrap(v);                                             \
        phys_joint_set_params(n->joint, &p);                             \
        return v;                                                        \
    }

#define AS_FLOAT(v) ((float)NUM2DBL(v))
#define AS_BOOL(v) (RTEST(v))
#define WRAP_BOOL(b) ((b) ? Qtrue : Qfalse)

JOINT_PARAM(stiffness, stiffness, rb_float_new, AS_FLOAT)
JOINT_PARAM(dampener, dampener, rb_float_new, AS_FLOAT)
JOINT_PARAM(rest_length, rest_length, rb_float_new, AS_FLOAT)
JOINT_PARAM(move1, move1, WRAP_BOOL, AS_BOOL)
JOINT_PARAM(move2, move2, WRAP_BOOL, AS_BOOL)
JOINT_PARAM(rotate1, rotate1, WRAP_BOOL, AS_BOOL)
JOINT_PARAM(rotate2, rotate2, WRAP_BOOL, AS_BOOL)
JOINT_PARAM(axis_on, axis_on, WRAP_BOOL, AS_BOOL)
#undef JOINT_PARAM

static VALUE joint_axis(VALUE self) {
    sn_t* n = sn_get(self);
    if (n->joint < 0) return Qnil;
    phys_joint_params p;
    phys_joint_get_params(n->joint, &p);
    return vec_new(p.axis[0], p.axis[1]);
}

static VALUE joint_axis_set(VALUE self, VALUE v) {
    sn_t* n = sn_get(self);
    if (n->joint < 0) return v;
    phys_joint_params p;
    phys_joint_get_params(n->joint, &p);
    double x, y;
    vec_get(v, &x, &y);
    p.axis[0] = (float)x;
    p.axis[1] = (float)y;
    phys_joint_set_params(n->joint, &p);
    return v;
}

// Anchors, each in its own limb's local frame and in the meters Limb#to_world
// takes: scripts write `j.limb1.to_world(j.point1)`. Never nil.
#define JOINT_POINT(name, field)                                              \
    static VALUE joint_##name(VALUE self) {                                   \
        sn_t* n = sn_get(self);                                               \
        if (n->joint < 0) return vec_new(0.0f, 0.0f);                         \
        phys_joint_params p;                                                  \
        phys_joint_get_params(n->joint, &p);                                  \
        return vec_new(p.field[0], p.field[1]);                               \
    }                                                                         \
    static VALUE joint_##name##_set(VALUE self, VALUE v) {                    \
        sn_t* n = sn_get(self);                                               \
        if (n->joint < 0) return v;                                           \
        phys_joint_params p;                                                  \
        phys_joint_get_params(n->joint, &p);                                  \
        double x, y;                                                          \
        vec_get(v, &x, &y);                                                   \
        p.field[0] = (float)x;                                                \
        p.field[1] = (float)y;                                                \
        phys_joint_set_params(n->joint, &p);                                  \
        return v;                                                             \
    }
JOINT_POINT(point1, point1)
JOINT_POINT(point2, point2)
#undef JOINT_POINT

static VALUE joint_limb1(VALUE self) { return sn_get(self)->ref1; }
static VALUE joint_limb2(VALUE self) { return sn_get(self)->ref2; }

// Shape

// the limb a shape belongs to (shape.parent = ShapeContainer, .parent = limb)
static VALUE shape_limb(VALUE self) {
    VALUE coll = sn_get(self)->parent;
    return NIL_P(coll) ? Qnil : sn_get(coll)->parent;
}

// The core's body-local swept vertices as [[Vector, radius], ...].
static VALUE shape_vertices(VALUE self) {
    VALUE result = rb_ary_new();
    sn_t* shape = sn_get(self);
    VALUE limb = shape_limb(self);
    if (NIL_P(limb) || shape->shape_index < 0) return result;
    float xyr[3 * PHYS_MAX_SHAPE_VERTS];
    const int n = phys_shape_vertices(sn_get(limb)->body, shape->shape_index,
                                      xyr, PHYS_MAX_SHAPE_VERTS);
    for (int i = 0; i < n; i++) {
        rb_ary_push(result, rb_ary_new3(2, vec_new(xyr[3 * i], xyr[3 * i + 1]),
                                        rb_float_new(xyr[3 * i + 2])));
    }
    return result;
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
#define LIMB_ACCESSOR(rname, cname)                                  \
    rb_define_method(c, rname, limb_##cname, 0);                     \
    rb_define_method(c, rname "=", limb_##cname##_set, 1)
    LIMB_ACCESSOR("mouse_stiffness_override", mouse_stiffness);
    LIMB_ACCESSOR("mouse_dampener_override", mouse_dampener);
    LIMB_ACCESSOR("centre_of_resistance", centre_of_resistance);
    LIMB_ACCESSOR("air_resistance_linear", air_linear);
    LIMB_ACCESSOR("air_resistance_angular", air_angular);
    LIMB_ACCESSOR("gravity_override", gravity);
    LIMB_ACCESSOR("material_velocity_response", mat_velocity_response);
    LIMB_ACCESSOR("material_stiffness", mat_stiffness);
    LIMB_ACCESSOR("material_dampener", mat_dampener);
    LIMB_ACCESSOR("material_kinetic_friction", mat_kinetic_friction);
    LIMB_ACCESSOR("material_static_friction", mat_static_friction);
#undef LIMB_ACCESSOR

    c = cls_find("Joint");
    rb_define_method(c, "limb1", joint_limb1, 0);
    rb_define_method(c, "limb2", joint_limb2, 0);
    rb_define_method(c, "axis", joint_axis, 0);
    rb_define_method(c, "axis=", joint_axis_set, 1);
    rb_define_method(c, "point1", joint_point1, 0);
    rb_define_method(c, "point1=", joint_point1_set, 1);
    rb_define_method(c, "point2", joint_point2, 0);
    rb_define_method(c, "point2=", joint_point2_set, 1);
#define JOINT_ACCESSOR(rname, cname)                                 \
    rb_define_method(c, rname, joint_##cname, 0);                    \
    rb_define_method(c, rname "=", joint_##cname##_set, 1)
    JOINT_ACCESSOR("stiffness", stiffness);
    JOINT_ACCESSOR("dampener", dampener);
    JOINT_ACCESSOR("rest_length", rest_length);
    JOINT_ACCESSOR("move1", move1);
    JOINT_ACCESSOR("move2", move2);
    JOINT_ACCESSOR("rotate1", rotate1);
    JOINT_ACCESSOR("rotate2", rotate2);
    JOINT_ACCESSOR("axis_on", axis_on);
#undef JOINT_ACCESSOR

    c = cls_find("Shape");
    rb_define_method(c, "member_of", shape_member_of, 0);
    rb_define_method(c, "member_of=", shape_member_of_set, 1);
    rb_define_method(c, "trigger_on", shape_trigger_on, 0);
    rb_define_method(c, "trigger_on=", shape_trigger_on_set, 1);
    rb_define_method(c, "triggers_overlapping", shape_triggers_overlapping, 1);
    rb_define_method(c, "vertices", shape_vertices, 0);
    rb_define_method(c, "limb", shape_limb, 0);
}
