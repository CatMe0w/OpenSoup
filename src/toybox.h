#pragma once
#include <stdbool.h>

// Lightweight single-surface Toybox UI.  Catalog icons are ordinary GPU
// quads in the main Sokol pass: no physics bodies and no native child/popup
// windows.  The original toy is instantiated through Ruby only on drop.
bool toybox_init(const char* assets_root, float view_w, float view_h);
void toybox_shutdown(void);
void toybox_resize(float view_w, float view_h);

bool toybox_hit_test(float x_px, float y_px);
void toybox_pointer_move(float x_px, float y_px);
bool toybox_mouse_down(float x_px, float y_px);
void toybox_mouse_dragged(float x_px, float y_px);
void toybox_mouse_up(float x_px, float y_px);
// precise=false: delta_y is in wheel detents; precise=true: device pixels.
// Trackpad momentum is supplied by the platform's normal scroll event stream.
void toybox_scroll(float delta_y, bool precise);
void toybox_frame(double dt_ms);
// Set only after the close button completes the original forward/reverse
// animation and dispatches quitToyBox.
bool toybox_quit_requested(void);

bool toybox_capturing(void);
int toybox_catalog_count(void);
