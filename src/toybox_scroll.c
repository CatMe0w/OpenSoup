#include "toybox_scroll.h"

#include "physics.h"
#include <math.h>

#define ICON_MASS 1.0f
#define ICON_ANCHOR_STIFFNESS 300.0f
#define ICON_ANCHOR_DAMPENER 25.0f
#define ICON_AIR_RESISTANCE 0.099f

typedef struct {
    float position;
    float velocity;
} scroll_derivative;

static scroll_derivative derive(const toybox_scroll_model* model,
                                float position, float velocity) {
    return (scroll_derivative){
        .position = velocity,
        .velocity = (ICON_ANCHOR_STIFFNESS * (model->target - position)
                   - (ICON_ANCHOR_DAMPENER + ICON_AIR_RESISTANCE) * velocity)
                  / ICON_MASS,
    };
}

static void step_once(toybox_scroll_model* model) {
    const float dt = PHYS_DT;
    const scroll_derivative k1 = derive(model, model->position,
                                        model->velocity);
    const scroll_derivative k2 = derive(model,
        model->position + 0.5f * dt * k1.position,
        model->velocity + 0.5f * dt * k1.velocity);
    const scroll_derivative k3 = derive(model,
        model->position + 0.5f * dt * k2.position,
        model->velocity + 0.5f * dt * k2.velocity);
    const scroll_derivative k4 = derive(model,
        model->position + dt * k3.position,
        model->velocity + dt * k3.velocity);
    const float sixth = dt / 6.0f;
    model->position += sixth * (k1.position + 2.0f * k2.position
                              + 2.0f * k3.position + k4.position);
    model->velocity += sixth * (k1.velocity + 2.0f * k2.velocity
                              + 2.0f * k3.velocity + k4.velocity);
}

void toybox_scroll_model_set_target(toybox_scroll_model* model,
                                    float target) {
    model->target = target;
}

void toybox_scroll_model_drag(toybox_scroll_model* model,
                              float initial_target, float delta_px,
                              float travel_px, float max_target) {
    if (!model) {
        return;
    }
    max_target = fmaxf(0.0f, max_target);
    float target = initial_target;
    if (travel_px > 0.0f) {
        target += delta_px * max_target / travel_px;
    }
    toybox_scroll_model_set_target(model,
        fminf(fmaxf(target, 0.0f), max_target));
}

bool toybox_scroll_model_advance(toybox_scroll_model* model, double dt_ms) {
    if (!model || dt_ms <= 0.0) {
        return false;
    }
    const float old_position = model->position;
    model->accumulator_ms += dt_ms;
    int steps = (int)(model->accumulator_ms / (PHYS_DT * 1000.0));
    if (steps <= 0) {
        return false;
    }
    model->accumulator_ms -= steps * (PHYS_DT * 1000.0);
    if (steps > 25) {
        steps = 25;
    }
    for (int i = 0; i < steps; i++) {
        step_once(model);
    }
    return fabsf(model->position - old_position) >= 0.0001f;
}
