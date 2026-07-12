#include "physics.h"
#include <math.h>
#include <stdlib.h>

#define MAX_BODIES 64
#define MAX_JOINTS 128
#define FRICTION_ITERATIONS 4 // original: 4x Contact_solveIter per pair

// world walls are the hidden "World" toy's 4 anchored limbs
// (souptoys_core.toy def): mass=1.0, so the contact mass ratio
// massWall/massBody is simply 1/massBody.
#define WALL_MASS 1.0f

typedef struct {
    float px, py;   // position, meters
    float mx, my;   // momentum
    float theta;    // orientation, radians CCW, accumulated
    float L;        // angular momentum
    phys_point* pts; // collision points, body-local
    int npts;
    phys_params prm;
    bool grabbed;
    float tx, ty;   // mouse spring target while grabbed
} body_t;

typedef struct {
    int b1, b2;
    float a1x, a1y, a2x, a2y; // body-local anchors
    float rest, k, c;
} joint_t;

static struct {
    float ww, wh; // wall extents
    body_t bodies[MAX_BODIES];
    int nbodies;
    joint_t joints[MAX_JOINTS];
    int njoints;
} P;

// classic RK4 stage time offsets, rk4_stage_coeffs @0x6D6BA0
static const float rk4_coeffs[4] = { 0.0f, 0.5f, 0.5f, 1.0f };

void phys_set_world(float width, float height) {
    P.ww = width;
    P.wh = height;
}

int phys_body_add(float x, float y, float theta, const phys_params* p,
                  const phys_point* pts, int npts, float fallback_radius) {
    if (P.nbodies >= MAX_BODIES) {
        return -1;
    }
    body_t* b = &P.bodies[P.nbodies];
    b->px = x;
    b->py = y;
    b->mx = 0;
    b->my = 0;
    b->theta = theta;
    b->L = 0;
    if (npts > 0) {
        b->npts = npts;
        b->pts = malloc((size_t)npts * sizeof(phys_point));
        for (int i = 0; i < npts; i++) {
            b->pts[i] = pts[i];
        }
    } else {
        b->npts = 1;
        b->pts = malloc(sizeof(phys_point));
        b->pts[0] = (phys_point){ 0, 0, fallback_radius, 0xF, 0xF };
    }
    b->prm = *p;
    b->grabbed = false;
    return P.nbodies++;
}

int phys_joint_add(int body1, float a1x, float a1y,
                   int body2, float a2x, float a2y,
                   float rest_length, float stiffness, float dampener) {
    if (P.njoints >= MAX_JOINTS || body1 < 0 || body2 < 0) {
        return -1;
    }
    P.joints[P.njoints] = (joint_t){ body1, body2, a1x, a1y, a2x, a2y,
                                     rest_length, stiffness, dampener };
    return P.njoints++;
}

// RK4 stage state of one body (see the integrator section below)
typedef struct {
    float px, py, theta;
    float mx, my, L;
} bstate_t;

// one wall contact of one collision point, evaluated on an arbitrary state.
// Fills n, r (contact offset), v_cp and returns penetration (<=0 = none).
typedef struct {
    float nx, ny;   // inward wall normal
    float rx, ry;   // contact point - body origin
    float vx, vy;   // velocity of the contact point
    float pen;
} contact_t;

static const float wall_nx[4] = { 1, -1, 0, 0 };
static const float wall_ny[4] = { 0, 0, 1, -1 };

static bool contact_eval(const body_t* b, const phys_point* p, int wall,
                         float px, float py, float theta,
                         float mx, float my, float L, contact_t* out) {
    const float c = cosf(theta), s = sinf(theta);
    const float wx = px + c * p->x - s * p->y;
    const float wy = py + s * p->x + c * p->y;
    float pen;
    switch (wall) {
        case PHYS_WALL_LEFT: pen = p->r - wx; break;
        case PHYS_WALL_RIGHT: pen = wx + p->r - P.ww; break;
        case PHYS_WALL_FLOOR: pen = p->r - wy; break;
        default: pen = wy + p->r - P.wh; break;
    }
    if (pen <= 0) {
        return false;
    }
    out->nx = wall_nx[wall];
    out->ny = wall_ny[wall];
    out->rx = wx - out->nx * p->r - px;
    out->ry = wy - out->ny * p->r - py;
    const float omega = (!b->prm.fixed_rotate && b->prm.inertia > 0)
                            ? L / b->prm.inertia : 0.0f;
    out->vx = mx / b->prm.mass - omega * out->ry;
    out->vy = my / b->prm.mass + omega * out->rx;
    out->pen = pen;
    return true;
}

// normal contact response magnitude, Contact_prepare's exact form:
//   closing*C0 + pen*C1 + closing*pen*C2, C = 2*ownMaterial
// (velocityResponse damps the approach, stiffness is the penetration
// penalty spring, dampener the penetration-scaled damper). The closing-
// velocity term only acts while approaching (prepare zeroes it otherwise).
// Interpreted as FORCE (the original folds dt into precomputed body fields);
// scaled by massWall/massBody and split among the body's contacts on that
// wall. NO positional correction exists in the original - resting bodies sit
// on the penalty spring, which is what keeps them jitter-free.
static float contact_normal_force(const body_t* b, const contact_t* ct) {
    const float closing = -(ct->vx * ct->nx + ct->vy * ct->ny);
    const float c0 = 2.0f * b->prm.material[0];
    const float c1 = 2.0f * b->prm.material[1];
    const float c2 = 2.0f * b->prm.material[2];
    const float mag = (closing > 0 ? closing * c0 : 0.0f)
                    + ct->pen * c1 + closing * ct->pen * c2;
    return mag * (WALL_MASS / b->prm.mass);
}

// wall contact forces for one body at a stage state, added into the
// derivative. Force split among the body's penetrating points per wall
// (the original splits impulses by the pair's contact count).
static void derive_wall_contacts(const body_t* b, const bstate_t* s, bstate_t* d) {
    for (int w = 0; w < 4; w++) {
        int count = 0;
        contact_t ct;
        for (int i = 0; i < b->npts; i++) {
            if ((b->pts[i].repel >> w) & 1
                && contact_eval(b, &b->pts[i], w, s->px, s->py, s->theta,
                                s->mx, s->my, s->L, &ct)) {
                count++;
            }
        }
        if (!count) {
            continue;
        }
        for (int i = 0; i < b->npts; i++) {
            const phys_point* p = &b->pts[i];
            if (!((p->repel >> w) & 1)
                || !contact_eval(b, p, w, s->px, s->py, s->theta,
                                 s->mx, s->my, s->L, &ct)) {
                continue;
            }
            const float f = contact_normal_force(b, &ct) / (float)count;
            d->mx += f * ct.nx;
            d->my += f * ct.ny;
            if (((p->rotate >> w) & 1) && !b->prm.fixed_rotate) {
                d->L += (ct.rx * ct.ny - ct.ry * ct.nx) * f;
            }
        }
    }
}

// post-integration friction pass, Contact_solveIter's exact form: iterate
// 4x per contact, each iteration applies the impulse that cancels the
// contact point's TANGENTIAL velocity (effective-mass weighted), capped by
// the static-friction cone against this step's normal impulse.
static void solve_friction(body_t* b) {
    for (int it = 0; it < FRICTION_ITERATIONS; it++) {
        for (int w = 0; w < 4; w++) {
            for (int i = 0; i < b->npts; i++) {
                const phys_point* p = &b->pts[i];
                contact_t ct;
                if (!((p->repel >> w) & 1)
                    || !contact_eval(b, p, w, b->px, b->py, b->theta,
                                     b->mx, b->my, b->L, &ct)) {
                    continue;
                }
                const float tx = -ct.ny, ty = ct.nx;
                const float vt = ct.vx * tx + ct.vy * ty;
                const bool spin = ((p->rotate >> w) & 1) && !b->prm.fixed_rotate
                                  && b->prm.inertia > 0;
                const float crt = ct.rx * ty - ct.ry * tx; // r x t
                const float denom = 1.0f / b->prm.mass
                                  + (spin ? crt * crt / b->prm.inertia : 0.0f);
                float jt = -vt / denom;
                // static-friction cone vs the step's normal impulse
                const float ncap = b->prm.material[4]
                                 * contact_normal_force(b, &ct) * PHYS_DT;
                if (jt > ncap) {
                    jt = ncap;
                } else if (jt < -ncap) {
                    jt = -ncap;
                }
                b->mx += jt * tx;
                b->my += jt * ty;
                if (spin) {
                    b->L += crt * jt;
                }
            }
        }
    }
}

// RK4 over the full coupled system (positions AND momenta, spring AND
// contact forces re-evaluated at every stage). This is what the original's
// stepOnce does - with joint stiffness up to 6000 at dt=0.01 the rotational
// spring modes sit beyond explicit Euler's stability limit but inside RK4's
// (omega*dt < 2.83).
static bstate_t rk_y[MAX_BODIES];   // stage state
static bstate_t rk_k[4][MAX_BODIES]; // stage derivatives

// world position and velocity of a body-local anchor at a stage state
static void anchor_state(const bstate_t* s, const body_t* b, float ax, float ay,
                         float* px, float* py, float* vx, float* vy) {
    const float c = cosf(s->theta), sn = sinf(s->theta);
    const float rx = c * ax - sn * ay;
    const float ry = sn * ax + c * ay;
    *px = s->px + rx;
    *py = s->py + ry;
    const float omega = (!b->prm.fixed_rotate && b->prm.inertia > 0)
                            ? s->L / b->prm.inertia : 0.0f;
    *vx = s->mx / b->prm.mass - omega * ry;
    *vy = s->my / b->prm.mass + omega * rx;
}

// accumulate force at a body-local anchor into a derivative
static void force_at(bstate_t* d, const bstate_t* s, const body_t* b,
                     float ax, float ay, float fx, float fy) {
    if (b->prm.anchored) {
        return;
    }
    d->mx += fx;
    d->my += fy;
    if (!b->prm.fixed_rotate) {
        const float c = cosf(s->theta), sn = sinf(s->theta);
        const float rx = c * ax - sn * ay;
        const float ry = sn * ax + c * ay;
        d->L += rx * fy - ry * fx;
    }
}

// derivative of the whole system at stage state rk_y -> out
static void derive(bstate_t* out) {
    for (int i = 0; i < P.nbodies; i++) {
        const body_t* b = &P.bodies[i];
        const bstate_t* s = &rk_y[i];
        bstate_t* d = &out[i];
        *d = (bstate_t){ 0 };
        if (b->prm.anchored) { // fixedMove: no motion at all (original +201 flag)
            continue;
        }
        d->px = s->mx / b->prm.mass;
        d->py = s->my / b->prm.mass;
        if (!b->prm.fixed_rotate && b->prm.inertia > 0) {
            d->theta = s->L / b->prm.inertia;
        }

        // gravity (Body_applyGravity); per-body g honours gravityOverride
        // (balloons have POSITIVE g)
        d->my += b->prm.mass * b->prm.gravity;

        // air resistance: F = -c_lin * v, tau = -c_ang * omega
        d->mx -= b->prm.air_linear * (s->mx / b->prm.mass);
        d->my -= b->prm.air_linear * (s->my / b->prm.mass);
        if (b->prm.inertia > 0) {
            d->L -= b->prm.air_angular * (s->L / b->prm.inertia);
        }

        // motors: constant body-local force + torque
        // TODO(verify): whether linearMotor force rotates with the limb
        if (b->prm.motor_force[0] != 0 || b->prm.motor_force[1] != 0) {
            const float c = cosf(s->theta), sn = sinf(s->theta);
            d->mx += c * b->prm.motor_force[0] - sn * b->prm.motor_force[1];
            d->my += sn * b->prm.motor_force[0] + c * b->prm.motor_force[1];
        }
        if (!b->prm.fixed_rotate) {
            d->L += b->prm.motor_torque;
        }

        if (b->grabbed) { // mouse spring (sub_532800 form)
            d->mx += b->prm.mouse_stiffness * (b->tx - s->px)
                   - b->prm.mouse_dampener * (s->mx / b->prm.mass);
            d->my += b->prm.mouse_stiffness * (b->ty - s->py)
                   - b->prm.mouse_dampener * (s->my / b->prm.mass);
        }

        derive_wall_contacts(b, s, d);
    }

    // spring joints, same constraint form as the mouse spring (sub_532800):
    // F = k * stretch * dir - c * relative anchor velocity, applied at the
    // anchors (torque from the offset)
    for (int i = 0; i < P.njoints; i++) {
        const joint_t* j = &P.joints[i];
        const body_t* b1 = &P.bodies[j->b1];
        const body_t* b2 = &P.bodies[j->b2];
        float p1x, p1y, v1x, v1y, p2x, p2y, v2x, v2y;
        anchor_state(&rk_y[j->b1], b1, j->a1x, j->a1y, &p1x, &p1y, &v1x, &v1y);
        anchor_state(&rk_y[j->b2], b2, j->a2x, j->a2y, &p2x, &p2y, &v2x, &v2y);
        float dx = p2x - p1x, dy = p2y - p1y;
        if (j->rest != 0.0f) {
            const float dist = sqrtf(dx * dx + dy * dy);
            if (dist > 1e-6f) {
                const float scale = (dist - j->rest) / dist;
                dx *= scale;
                dy *= scale;
            }
        }
        const float fx = j->k * dx - j->c * (v1x - v2x);
        const float fy = j->k * dy - j->c * (v1y - v2y);
        force_at(&out[j->b1], &rk_y[j->b1], b1, j->a1x, j->a1y, fx, fy);
        force_at(&out[j->b2], &rk_y[j->b2], b2, j->a2x, j->a2y, -fx, -fy);
    }
}

static void step_once(void) {
    // classic RK4: k1=f(y), k2=f(y+dt/2*k1), k3=f(y+dt/2*k2), k4=f(y+dt*k3)
    for (int stage = 0; stage < 4; stage++) {
        for (int i = 0; i < P.nbodies; i++) {
            const body_t* b = &P.bodies[i];
            bstate_t y = { b->px, b->py, b->theta, b->mx, b->my, b->L };
            if (stage > 0) {
                const float h = rk4_coeffs[stage] * PHYS_DT;
                const bstate_t* k = &rk_k[stage - 1][i];
                y.px += h * k->px;
                y.py += h * k->py;
                y.theta += h * k->theta;
                y.mx += h * k->mx;
                y.my += h * k->my;
                y.L += h * k->L;
            }
            rk_y[i] = y;
        }
        derive(rk_k[stage]);
    }
    for (int i = 0; i < P.nbodies; i++) {
        body_t* b = &P.bodies[i];
        if (b->prm.anchored) {
            continue;
        }
        const bstate_t* k1 = &rk_k[0][i];
        const bstate_t* k2 = &rk_k[1][i];
        const bstate_t* k3 = &rk_k[2][i];
        const bstate_t* k4 = &rk_k[3][i];
        const float h = PHYS_DT / 6.0f;
        b->px += h * (k1->px + 2 * k2->px + 2 * k3->px + k4->px);
        b->py += h * (k1->py + 2 * k2->py + 2 * k3->py + k4->py);
        b->theta += h * (k1->theta + 2 * k2->theta + 2 * k3->theta + k4->theta);
        b->mx += h * (k1->mx + 2 * k2->mx + 2 * k3->mx + k4->mx);
        b->my += h * (k1->my + 2 * k2->my + 2 * k3->my + k4->my);
        b->L += h * (k1->L + 2 * k2->L + 2 * k3->L + k4->L);

        // friction/stick impulses on the integrated state (solveIter's role)
        solve_friction(b);
    }
}

void phys_steps(int n) {
    for (int i = 0; i < n; i++) {
        step_once();
    }
}

void phys_body_pos(int body, float* x, float* y) {
    *x = P.bodies[body].px;
    *y = P.bodies[body].py;
}

float phys_body_orientation(int body) {
    return P.bodies[body].theta;
}

void phys_body_set_pose(int body, float x, float y, float theta) {
    body_t* b = &P.bodies[body];
    b->px = x;
    b->py = y;
    b->theta = theta;
}

void phys_grab(int body) {
    body_t* b = &P.bodies[body];
    b->grabbed = true;
    b->tx = b->px;
    b->ty = b->py;
}

void phys_grab_move(int body, float x, float y) {
    P.bodies[body].tx = x;
    P.bodies[body].ty = y;
}

void phys_release(int body) {
    // the spring already built up momentum; just detach
    P.bodies[body].grabbed = false;
}
