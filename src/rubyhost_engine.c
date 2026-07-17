// REngine: scene/canvas rects and coordinate transforms, the input grab
// triplet, stepping, and physics timers; plus the RInput surface.
#include "rubyhost.h"
#include "physics.h"
#include <math.h>
#include "rubyhost_internal.h"

static double g_screen_w = 1280.0, g_screen_h = 800.0;

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

// Coordinate spaces: view/screen = logical px y-down; canvas = logical px
// y-down with origin at the canvas rect's top-left; scene = meters y-up,
// scene_top_right stored NEGATED (framework convention). scale = logical px
// per scene unit. Backing pixels are never visible to Ruby.
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
// linear/torque components. A containing `grab` shape overrides both flags;
// the original walks every shape in definition order, so the last matching
// handle wins when handles overlap.
static int shape_contains(const td_shape* sh, double x, double y) {
    if (!sh || sh->npoints < 1) {
        return 0;
    }
    if (sh->npoints == 1) {
        const double dx = x - sh->points[0].x;
        const double dy = y - sh->points[0].y;
        const double r = sh->points[0].r;
        return dx * dx + dy * dy <= r * r;
    }

    // Rounded polygon points occasionally carry a radius. Count their caps
    // as part of the handle, then test the polygon interior itself.
    for (int i = 0; i < sh->npoints; i++) {
        const double dx = x - sh->points[i].x;
        const double dy = y - sh->points[i].y;
        const double r = sh->points[i].r;
        if (r > 0.0 && dx * dx + dy * dy <= r * r) {
            return 1;
        }
    }
    int inside = 0;
    for (int i = 0, j = sh->npoints - 1; i < sh->npoints; j = i++) {
        const double xi = sh->points[i].x, yi = sh->points[i].y;
        const double xj = sh->points[j].x, yj = sh->points[j].y;
        if (((yi > y) != (yj > y))
            && x < (xj - xi) * (y - yi) / (yj - yi) + xi) {
            inside = !inside;
        }
    }
    return inside;
}

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
        bool move = ln->ldef ? ln->ldef->default_grab_move : true;
        bool rotate = ln->ldef ? ln->ldef->default_grab_rotate : true;
        if (ln->ldef && ln->lscale > 0.0) {
            const double lx = (c * dx + sn * dy) / ln->lscale;
            const double ly = (-sn * dx + c * dy) / ln->lscale;
            for (int i = 0; i < ln->ldef->nshapes; i++) {
                const td_shape* sh = &ln->ldef->shapes[i];
                if (sh->grab && shape_contains(sh, lx, ly)) {
                    move = sh->grab_move;
                    rotate = sh->grab_rotate;
                }
            }
        }
        in->ref1 = limb;
        phys_grab(ln->body, c * dx + sn * dy, -sn * dx + c * dy,
                  move, rotate);
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

void rbh_screen_size(double w_px, double h_px) {
    g_screen_w = w_px;
    g_screen_h = h_px;
    VALUE core = rb_gv_get("$core");
    if (!NIL_P(core)) {
        fcall_protected(core, "screen_size_changed", 0, Qnil, Qnil, Qnil,
                        "screen_size_changed");
    }
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

void rbh_register_engine(void) {
    VALUE c = cls_find("REngine");
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

    c = cls_find("RInput");
    rb_define_method(c, "limb", input_limb, 0);
}
