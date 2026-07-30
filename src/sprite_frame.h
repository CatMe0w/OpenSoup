#pragma once
#include <math.h>

// Orientation -> rotation-phase frame, shared by the renderer and Sprite#frame.
//
// A bound multi-frame sprite is not an animation: the FLC frames are
// pre-rendered rotation phases (the original draws through GDI, which cannot
// rotate a bitmap), stepping 2pi/N per index in the visually-CCW direction,
// frame 0 at theta 0, the same sign as the physics angle.
//
// The scale is a FLOAT N/(2pi) and the product then keeps register precision,
// the same x87 PC=53 rule the physics core follows; rounding either of them
// differently moves phase boundaries by an ulp. The half-up tie is a choice,
// not a measurement.
static inline int sprite_frame_for_orientation(float orientation, int nframes) {
    if (nframes <= 1) {
        return 0;
    }
    const float scale = (float)(nframes / (2.0 * 3.14159265358979323846));
    // a long-lived spinner's angle is unbounded
    const double phase = fmod((double)orientation * scale, (double)nframes);
    const int frame = (int)floor(phase + 0.5);
    return ((frame % nframes) + nframes) % nframes;
}
