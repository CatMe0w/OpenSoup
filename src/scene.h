#pragma once
#include <stdbool.h>
#include "sokol_gfx.h"

// Platform-agnostic scene: everything below speaks NDC ([-1,1], y-up),
// so the platform layer only converts window coords once.

void scene_setup(const sg_environment* env);
void scene_frame(const sg_swapchain* swapchain);
void scene_shutdown(void);

// true if the point is inside interactive content (drives click-through)
bool scene_hit_test(float ndc_x, float ndc_y);

bool scene_grab_begin(float ndc_x, float ndc_y);
void scene_grab_move(float ndc_x, float ndc_y);
void scene_grab_end(void);
bool scene_grabbing(void);
