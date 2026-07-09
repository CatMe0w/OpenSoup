#pragma once
#include <stdbool.h>
#include <stdint.h>

// Decoded, premultiplied-alpha RGBA8 images, top-left origin.
// Premultiply matches the original engine exactly (Toybox.exe 0x4FCE80):
//   f = (float)(a / 255.0); c' = (uint8)trunc((double)c * f)

typedef struct {
    int w, h;
    uint8_t* rgba;
} as_image;

typedef struct {
    int w, h;
    int frame_count;
    int speed_ms;     // per-frame delay from the FLC header
    uint8_t** frames; // frame_count buffers of w*h*4
} as_anim;

// Truevision TGA, type 2/10 (raw/RLE truecolor), 24/32bpp
bool as_load_tga(const char* path, as_image* out);

// Autodesk FLC (8-bit paletted). Souptoys ships transparency as a
// companion "<name>_Alpha.flc" whose gray level is the alpha channel;
// pass NULL for alpha_path to get an opaque animation.
bool as_load_flc(const char* color_path, const char* alpha_path, as_anim* out);

void as_image_free(as_image* img);
void as_anim_free(as_anim* anim);
