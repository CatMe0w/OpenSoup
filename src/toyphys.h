#pragma once
#include "toydefs.h"

// Shared toy-def -> physics instantiation, used by both the native demo
// assembly and the Ruby object model (Toy realization).

// One physics body for a limb def. (x, y) is the body's WORLD position and
// theta its orientation - callers add any spawn offset to the def's rest
// position themselves. scale converts toy-local shape points and inertia
// into world meters. Returns the phys body index (-1 if full).
int toyphys_body_for_limb(const td_limb* l, float x, float y, float theta,
                          float scale, int toy_id);
