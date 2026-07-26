#pragma once
#include <stdbool.h>
#include "sokol_gfx.h"

void scene_demo_setup(const sg_environment* env, float width, float height);
void scene_demo_frame(const sg_swapchain* swapchain, float width, float height);
void scene_demo_shutdown(void);

bool scene_demo_hit_test(float x, float y);

bool scene_demo_grab_begin(float x, float y);
void scene_demo_grab_move(float x, float y);
void scene_demo_grab_end(void);
bool scene_demo_grabbing(void);
