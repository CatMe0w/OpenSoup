#pragma once

#include <stdbool.h>

// Lightweight equivalent of the original IconGrid's uniform vertical mode.
// The real catalog gave every icon a mass-1 body connected to its target by a
// k=300, c=25 spring, with 0.099 linear air resistance.  During ordinary
// scrolling every target moves by the same amount, so the neighbour springs
// remain at rest and the entire grid reduces exactly to this one state.
typedef struct {
    float position;
    float target;
    float velocity;
    double accumulator_ms;
} toybox_scroll_model;

void toybox_scroll_model_set_target(toybox_scroll_model* model, float target);
bool toybox_scroll_model_advance(toybox_scroll_model* model, double dt_ms);

