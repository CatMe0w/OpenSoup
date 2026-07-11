#include "physics.h"

#define MAX_BODIES 64
#define SOLVER_ITERATIONS 4

// TODO(material-combine): placeholder single material until the CMaterial
// combine rule at manifold creation is reversed (Toybox_physics.md remaining)
#define MAT_RESTITUTION 0.35f
#define MAT_FRICTION 0.15f

typedef struct {
    float px, py;   // position, meters
    float mx, my;   // momentum
    float theta;    // orientation, radians CCW, accumulated
    float L;        // angular momentum
    float radius;
    phys_params prm;
    bool grabbed;
    float tx, ty;   // mouse spring target while grabbed
} body_t;

static struct {
    float ww, wh; // wall extents
    body_t bodies[MAX_BODIES];
    int nbodies;
} P;

// classic RK4 stage time offsets, rk4_stage_coeffs @0x6D6BA0
static const float rk4_coeffs[4] = { 0.0f, 0.5f, 0.5f, 1.0f };

void phys_set_world(float width, float height) {
    P.ww = width;
    P.wh = height;
}

int phys_body_add(float x, float y, float radius, const phys_params* p) {
    if (P.nbodies >= MAX_BODIES) {
        return -1;
    }
    body_t* b = &P.bodies[P.nbodies];
    b->px = x;
    b->py = y;
    b->mx = 0;
    b->my = 0;
    b->radius = radius;
    b->prm = *p;
    b->grabbed = false;
    return P.nbodies++;
}

// impulse contact against one wall. n = inward wall normal, pen = penetration.
// Structure follows Contact_prepare/solveIter: resolve only when approaching,
// restitution on the normal, velocity-dependent friction on the tangent.
static void solve_wall(body_t* b, float nx, float ny, float pen) {
    if (pen <= 0) {
        return;
    }
    const float m = b->prm.mass;
    const float vx = b->mx / m;
    const float vy = b->my / m;
    const float vn = vx * nx + vy * ny;
    const float tx = -ny, ty = nx;
    if (vn < 0) { // approaching: normal impulse with restitution
        const float j = -(1.0f + MAT_RESTITUTION) * vn * m;
        b->mx += j * nx;
        b->my += j * ny;
    }
    // friction opposes the CONTACT POINT's tangential velocity (includes the
    // rotational term): rolling couples spin<->translation, spin decays on
    // ground contact instead of persisting forever
    const float omega = (b->prm.inertia > 0) ? b->L / b->prm.inertia : 0;
    const float vt_cp = (vx * tx + vy * ty) - omega * b->radius;
    const float jt = -MAT_FRICTION * vt_cp * m;
    b->mx += jt * tx;
    b->my += jt * ty;
    if (!b->prm.fixed_rotate) {
        b->L += -b->radius * jt;
    }
    // positional correction (stand-in for the stiffness/penetration term)
    b->px += nx * pen;
    b->py += ny * pen;
}

static void step_once(void) {
    for (int i = 0; i < P.nbodies; i++) {
        body_t* b = &P.bodies[i];
        if (b->prm.anchored) { // fixedMove: no linear motion (original +201 flag)
            continue;
        }
        // gravity as momentum impulse: dp = m * g * dt (Body_applyGravity);
        // per-body g honours gravityOverride (balloons have POSITIVE g)
        b->my += b->prm.mass * b->prm.gravity * PHYS_DT;

        // air resistance: F = -c_lin * v, tau = -c_ang * omega
        b->mx -= b->prm.air_linear * (b->mx / b->prm.mass) * PHYS_DT;
        b->my -= b->prm.air_linear * (b->my / b->prm.mass) * PHYS_DT;
        if (b->prm.inertia > 0) {
            b->L -= b->prm.air_angular * (b->L / b->prm.inertia) * PHYS_DT;
        }

        if (b->grabbed) { // mouse spring (sub_532800 form)
            const float fx = b->prm.mouse_stiffness * (b->tx - b->px)
                           - b->prm.mouse_dampener * (b->mx / b->prm.mass);
            const float fy = b->prm.mouse_stiffness * (b->ty - b->py)
                           - b->prm.mouse_dampener * (b->my / b->prm.mass);
            b->mx += fx * PHYS_DT;
            b->my += fy * PHYS_DT;
        }

        // RK4 position integration. With no position-dependent forces yet the
        // stages collapse to p += v*dt, but keep the structure of stepOnce.
        const float vx = b->mx / b->prm.mass;
        const float vy = b->my / b->prm.mass;
        float x = b->px, y = b->py;
        float acc_x = 0, acc_y = 0;
        for (int s = 0; s < 4; s++) {
            const float w = (s == 0 || s == 3) ? 1.0f / 6.0f : 1.0f / 3.0f;
            (void)rk4_coeffs[s];
            acc_x += w * vx;
            acc_y += w * vy;
        }
        b->px = x + acc_x * PHYS_DT;
        b->py = y + acc_y * PHYS_DT;

        // angular: theta += omega*dt, omega = L / inertia (momentum space)
        if (!b->prm.fixed_rotate && b->prm.inertia > 0) {
            b->theta += (b->L / b->prm.inertia) * PHYS_DT;
        }

        for (int it = 0; it < SOLVER_ITERATIONS; it++) {
            solve_wall(b, 1, 0, b->radius - b->px);            // left
            solve_wall(b, -1, 0, b->px + b->radius - P.ww);    // right
            solve_wall(b, 0, 1, b->radius - b->py);            // floor
            solve_wall(b, 0, -1, b->py + b->radius - P.wh);    // ceiling
        }
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
