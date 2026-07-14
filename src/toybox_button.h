#pragma once

#include <stdbool.h>

// ButtonGUIComponent's animation state. The original treats all sprite
// frames as one linear 0..1 press track; toggle buttons rest at either end.
typedef struct {
    float period_s;
    float target;
    float progress;
    float velocity;
    float down_x, down_y;
    bool toggle;
    bool on;
    bool pressed;
    bool pending;
    bool pending_on;
} toybox_button_model;

void toybox_button_init(toybox_button_model* model, float period_s,
                        bool toggle, bool on);
void toybox_button_down(toybox_button_model* model, float x, float y);
void toybox_button_cancel(toybox_button_model* model);
// Returns true when this release was accepted as a click. The command itself
// remains pending until advance reaches the endpoint used by the original.
bool toybox_button_up(toybox_button_model* model, float x, float y);

// Advance the linear animation. Returns true once when a pending command may
// fire; desired_on is meaningful for toggle buttons.
bool toybox_button_advance(toybox_button_model* model, double dt_ms,
                           bool* desired_on);
// Synchronize a toggle with the command backend's real state, mirroring the
// original ToggleSyncDecorator callback.
void toybox_button_sync(toybox_button_model* model, bool on);

int toybox_button_frame(const toybox_button_model* model, int frame_count);

