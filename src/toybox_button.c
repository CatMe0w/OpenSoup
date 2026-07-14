#include "toybox_button.h"

#include <math.h>
#include <string.h>

#define TOYBOX_BUTTON_CLICK_SLOP 1.5f

static void set_target(toybox_button_model* model, bool endpoint,
                       bool force_direction) {
    model->target = endpoint ? 1.0f : 0.0f;
    if (!force_direction && model->velocity != 0.0f) {
        return;
    }
    if (model->period_s == 0.0f) {
        model->velocity = 0.0f;
    } else {
        model->velocity = (endpoint ? 1.0f : -1.0f) / model->period_s;
    }
}

void toybox_button_init(toybox_button_model* model, float period_s,
                        bool toggle, bool on) {
    memset(model, 0, sizeof(*model));
    model->period_s = fmaxf(0.0f, period_s);
    model->toggle = toggle;
    model->on = on;
    model->target = model->progress = on ? 1.0f : 0.0f;
}

void toybox_button_down(toybox_button_model* model, float x, float y) {
    set_target(model, model->toggle ? model->on : true, true);
    model->pressed = true;
    model->down_x = x;
    model->down_y = y;
}

void toybox_button_cancel(toybox_button_model* model) {
    set_target(model, model->toggle ? model->on : false, true);
    model->pressed = false;
}

bool toybox_button_up(toybox_button_model* model, float x, float y) {
    const float distance = hypotf(x - model->down_x, y - model->down_y);
    const bool accepted = model->pressed
                       && distance < TOYBOX_BUTTON_CLICK_SLOP;
    if (accepted) {
        model->pending_on = model->toggle ? !model->on : false;
        set_target(model, model->pending_on, false);
        model->pending = true;
        model->pressed = false;
        return true;
    }
    toybox_button_cancel(model);
    return false;
}

bool toybox_button_advance(toybox_button_model* model, double dt_ms,
                           bool* desired_on) {
    if (model->velocity != 0.0f && dt_ms > 0.0) {
        model->progress += model->velocity * (float)(dt_ms / 1000.0);
        if (model->progress <= 0.0f) {
            model->progress = 0.0f;
            if (model->target == 0.0f) {
                model->velocity = 0.0f;
            } else {
                model->velocity = -model->velocity;
            }
        } else if (model->progress >= 1.0f) {
            model->progress = 1.0f;
            if (model->target == 1.0f) {
                model->velocity = 0.0f;
            } else {
                model->velocity = -model->velocity;
            }
        }
    }

    // Non-toggle commands fire after returning to frame zero. Toggle commands
    // may fire at either endpoint and are then synchronized by their backend.
    if (model->pending
        && (model->progress == 0.0f
            || (model->toggle && model->progress == 1.0f))) {
        model->pending = false;
        if (desired_on) {
            *desired_on = model->pending_on;
        }
        return true;
    }
    return false;
}

void toybox_button_sync(toybox_button_model* model, bool on) {
    model->on = on;
    set_target(model, on, true);
}

int toybox_button_frame(const toybox_button_model* model, int frame_count) {
    if (frame_count <= 1 || model->progress <= 0.0f) {
        return 0;
    }
    if (model->progress >= 1.0f) {
        return frame_count - 1;
    }
    int frame = (int)((float)frame_count * model->progress);
    if (frame >= frame_count) {
        frame = frame_count - 1;
    }
    return frame;
}

