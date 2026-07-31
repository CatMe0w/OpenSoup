// Diagnostic dumps: scene listing and replay scenario.
#include "rubyhost.h"
#include "physics.h"
#include <stdio.h>
#include "rubyhost_internal.h"

// 9 significant digits round-trip a float; 17 for a double.
#define F "%+.9e"
#define D "%+.17e"
#define FQ "%%+.9e" // F escaped for generated Ruby format strings

static VALUE items_of(VALUE coll) {
    return (!NIL_P(coll) && sn_p(coll)) ? sn_get(coll)->items : Qnil;
}

static long coll_len(VALUE items) {
    return NIL_P(items) ? 0 : RARRAY(items)->len;
}

// After realization, read state from the body, not the node.
static void limb_state(const sn_t* n, double* x, double* y, double* th,
                       double* mx, double* my, double* mL) {
    if (n->body >= 0) {
        float px, py, ix, iy, L;
        phys_body_pos(n->body, &px, &py);
        phys_body_momentum(n->body, &ix, &iy, &L);
        *x = px; *y = py; *th = phys_body_orientation(n->body);
        *mx = ix; *my = iy; *mL = L;
    } else {
        *x = n->px; *y = n->py; *th = n->orient;
        *mx = n->mx; *my = n->my; *mL = n->mL;
    }
}

static const char* toy_class(VALUE toyv) {
    return rb_class2name(CLASS_OF(toyv));
}

// ------------------------------------------------------------- scene listing

static void dump_toy(FILE* f, VALUE toyv, int depth) {
    const sn_t* t = sn_get(toyv);
    fprintf(f, "%*stoy %s instance=%d sticky=%d realized=%d base_scale=%.9g\n",
            depth * 2, "", toy_class(toyv), t->instance_id, t->sticky,
            t->realized, t->def ? t->def->base_scale : 0.0);

    const VALUE limbs = items_of(t->colls[1]);
    for (long i = 0; i < coll_len(limbs); i++) {
        const sn_t* ln = sn_get(rb_ary_entry(limbs, i));
        double x, y, th, mx, my, mL;
        limb_state(ln, &x, &y, &th, &mx, &my, &mL);
        fprintf(f, "%*slimb %ld name=%s body=%d pos=(" F "," F ") th=" F
                   " p=(" F "," F ") L=" F " shock=%d\n",
                (depth + 1) * 2, "", i,
                ln->ldef && ln->ldef->name ? ln->ldef->name : "?",
                ln->body, x, y, th, mx, my, mL, ln->shock);
    }

    const VALUE joints = items_of(t->colls[2]);
    for (long i = 0; i < coll_len(joints); i++) {
        const sn_t* jn = sn_get(rb_ary_entry(joints, i));
        fprintf(f, "%*sjoint %ld slot=%d b1=%d b2=%d\n", (depth + 1) * 2, "", i,
                jn->joint,
                NIL_P(jn->ref1) ? -1 : sn_get(jn->ref1)->body,
                NIL_P(jn->ref2) ? -1 : sn_get(jn->ref2)->body);
    }

    const VALUE children = items_of(t->colls[0]);
    for (long i = 0; i < coll_len(children); i++) {
        dump_toy(f, rb_ary_entry(children, i), depth + 1);
    }
}

void rbh_dump_scene(FILE* f) {
    if (!f) return;
    const VALUE eng = rb_gv_get("$default_engine");
    if (NIL_P(eng) || !sn_p(eng)) {
        fprintf(f, "no default engine\n");
        return;
    }
    const sn_t* e = sn_get(eng);
    fprintf(f, "engine scene_bl=(%.17g,%.17g) scene_tr=(%.17g,%.17g) "
               "scale=%.17g paused=%d time=%.17g timestep=%.17g\n",
            e->scene_bl[0], e->scene_bl[1], e->scene_tr[0], e->scene_tr[1],
            e->scale, e->paused, e->time, e->timestep);

    const VALUE toys = items_of(e->colls[0]);
    for (long i = 0; i < coll_len(toys); i++) {
        dump_toy(f, rb_ary_entry(toys, i), 0);
    }
    fflush(f);
}

// -------------------------------------------------------------- replay script

typedef struct {
    FILE* f;
    int toy;     // next $t index
    long limb;   // next $all index
    int grabbed; // the $all index holding the grab, -1 = none
} replay;

// Emit live values that differ from the def; .new only restores def defaults.
static void replay_limb_overrides(FILE* f, long i, const sn_t* ln) {
    if (ln->body < 0 || !ln->ldef) {
        return;
    }
    phys_params p;
    phys_body_get_params(ln->body, &p);
    const td_limb* d = ln->ldef;

#define OVERRIDE(rname, live, def_value)                                   \
    if ((live) != (def_value)) {                                           \
        fprintf(f, "$l[%ld].%s = " F "\n", i, rname, (double)(live));      \
    }
    OVERRIDE("gravity_override", p.gravity,
             d->has_gravity_override ? d->gravity_override : PHYS_GRAVITY)
    OVERRIDE("air_resistance_linear", p.air_linear, d->air_resistance_linear)
    OVERRIDE("air_resistance_angular", p.air_angular, d->air_resistance_angular)
    OVERRIDE("mouse_stiffness_override", p.mouse_stiffness, d->mouse_stiffness)
    OVERRIDE("mouse_dampener_override", p.mouse_dampener, d->mouse_dampener)
    static const char* const material[5] = {
        "material_velocity_response", "material_stiffness",
        "material_dampener", "material_kinetic_friction",
        "material_static_friction"
    };
    for (int m = 0; m < 5; m++) {
        OVERRIDE(material[m], p.material[m], d->material[m])
    }
#undef OVERRIDE
    if (ln->shock != 0) {
        fprintf(f, "$l[%ld].shock_order = %d\n", i, ln->shock);
    }
    // Neither engine has a setter for these two; report, cannot replay.
    const float inertia = (float)((double)d->inertia * ln->lscale * ln->lscale);
    if (p.mass != d->mass || p.inertia != inertia) {
        fprintf(f, "# limb %ld runs on mass " F " inertia " F ", the def says "
                   F " / " F " - NOT replayable\n",
                i, p.mass, p.inertia, d->mass, inertia);
    }
}

static void replay_toy(replay* r, VALUE toyv) {
    const sn_t* t = sn_get(toyv);
    const VALUE limbs = items_of(t->colls[1]);
    const long nlimbs = coll_len(limbs);

    // Sticky toys are the engine's own; recreating World duplicates the walls.
    if (t->sticky || !t->def) {
        fprintf(r->f, "# not replayed: %s (%s)\n", toy_class(toyv),
                t->sticky ? "sticky" : "no def");
    } else {
        fprintf(r->f, "# $all[%ld..%ld] = %s, live bodies", r->limb,
                r->limb + nlimbs - 1, toy_class(toyv));
        for (long i = 0; i < nlimbs; i++) {
            fprintf(r->f, " %d", sn_get(rb_ary_entry(limbs, i))->body);
        }
        fprintf(r->f, "\n");

        fprintf(r->f, "$t%d = %s.new; $e.toys << $t%d; $l = $t%d.limbs.to_a; "
                      "$all += $l\n",
                r->toy, toy_class(toyv), r->toy, r->toy);
        for (long i = 0; i < nlimbs; i++) {
            const sn_t* ln = sn_get(rb_ary_entry(limbs, i));
            double x, y, th, mx, my, mL;
            limb_state(ln, &x, &y, &th, &mx, &my, &mL);
            fprintf(r->f, "$l[%ld].position = Vector[" F ", " F "]; "
                          "$l[%ld].orientation = " F "; "
                          "$l[%ld].momentum = Vector[" F ", " F "]; "
                          "$l[%ld].angular_momentum = " F "\n",
                    i, x, y, i, th, i, mx, my, i, mL);
            replay_limb_overrides(r->f, i, ln);
            float ax, ay, tx, ty;
            bool move, rotate;
            if (ln->body >= 0
                && phys_grab_state(ln->body, &ax, &ay, &tx, &ty, &move,
                                   &rotate)) {
                r->grabbed = (int)(r->limb + i);
                fprintf(r->f, "$grab = Vector[" F ", " F "]; "
                              "$drag = Vector[" F ", " F "] "
                              "# held, move=%d rotate=%d\n",
                        ax, ay, tx, ty, move, rotate);
            }
        }
        r->toy++;
        r->limb += nlimbs; // only limbs that made it into $all count
    }

    const VALUE children = items_of(t->colls[0]);
    for (long i = 0; i < coll_len(children); i++) {
        replay_toy(r, rb_ary_entry(children, i));
    }
}

// Same six fields the acceptance scenarios print.
static void replay_probe(FILE* f, const char* tag) {
    fprintf(f, "$all.each_with_index { |l, i| puts format(\"%s %%03d " FQ " "
               FQ " " FQ " " FQ " " FQ " " FQ "\", i, l.position.x, "
               "l.position.y, l.orientation, l.momentum.x, l.momentum.y, "
               "l.angular_momentum) }\n",
            tag);
}

void rbh_dump_replay(FILE* f) {
    if (!f) return;
    const VALUE eng = rb_gv_get("$default_engine");
    if (NIL_P(eng) || !sn_p(eng)) {
        fprintf(f, "# no default engine\n");
        return;
    }
    const sn_t* e = sn_get(eng);

    fprintf(f,
        "# OpenSoup diagnostic replay.\n"
        "# Physics state is complete. Ruby-side toy state and sprite/sound state are not carried over.\n");
    fprintf(f, "$e = $default_engine\n");
    fprintf(f, "$e.scene_bottom_left = Vector[" D ", " D "]\n",
            e->scene_bl[0], e->scene_bl[1]);
    fprintf(f, "$e.scene_top_right = Vector[" D ", " D "]\n",
            e->scene_tr[0], e->scene_tr[1]);
    fprintf(f, "$e.scene_walls_changed\n");
    if (e->paused) {
        fprintf(f, "# the live engine was paused; the replay runs unpaused\n");
    }
    fprintf(f, "$all = []\n");

    replay r = { f, 0, 0, -1 };
    const VALUE toys = items_of(e->colls[0]);
    for (long i = 0; i < coll_len(toys); i++) {
        replay_toy(&r, rb_ary_entry(toys, i));
    }

    // Re-derive grab through the app's mouse path for consistent gates.
    if (r.grabbed >= 0) {
        fprintf(f, "$in = $e.input_by_id(1)\n");
        fprintf(f, "$e.input_grab($all[%d], $in, $grab)\n", r.grabbed);
        fprintf(f, "$e.input_move($in, $drag)\n");
    }

    replay_probe(f, "P000");
    static const struct { int run, total; } checkpoints[] = {
        { 1, 1 }, { 9, 10 }, { 90, 100 }, { 100, 200 }
    };
    for (size_t i = 0; i < sizeof checkpoints / sizeof checkpoints[0]; i++) {
        char tag[8];
        fprintf(f, "$e.run_steps(%d)\n", checkpoints[i].run);
        snprintf(tag, sizeof tag, "P%03d", checkpoints[i].total);
        replay_probe(f, tag);
    }
    fflush(f);
}
