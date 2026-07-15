#include "scene.h"
#include "physics.h"
#include "sokol_log.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#define MAX_FRAMES 256

typedef struct texture_t {
    int w, h;
    int nframes;
    int refs;
    uint8_t* const* pixels; // identity key + borrowed alpha hit-test data
    sg_view views[MAX_FRAMES];
    sg_image imgs[MAX_FRAMES];
    struct texture_t* next;
} texture_t;

typedef struct {
    int id;     // stable handle handed to callers; draw order reorders freely
    float x, y; // logical-pixel top-left; blit position is trunc'd
    int w, h;               // source texture dimensions
    float draw_w, draw_h;   // logical-pixel destination dimensions
    int nframes;
    int frame;
    int speed_ms;
    double acc_ms;
    texture_t* texture;    // shared by every instance of the same decoded asset
    int body;               // physics body index or -1
    float ax, ay;           // body origin -> visual centre, logical px, y-up
    int group;              // toy group; grabbed together, raised together
    int layer;              // higher layers draw and hit-test above lower ones
    float alpha;
    float uv_scale[2];
    bool visible;
    bool animate;
    bool clip_enabled;
    float clip[4];          // logical x, y, w, h, top-left origin
} sprite_t;

typedef struct {
    float pos[2];
    float size[2];
    float viewport[2];
    float uv_scale[2];
    float tint[4];
} vs_params_t;

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_buffer quad;
    sg_sampler smp;
    sprite_t* sprites;
    int nsprites;
    int sprite_cap;
    int next_id;
    texture_t* textures;
} state;

static bool grow_sprites(int wanted) {
    if (wanted <= state.sprite_cap) {
        return true;
    }
    int cap = state.sprite_cap ? state.sprite_cap : 256;
    while (cap < wanted) {
        if (cap > INT_MAX / 2) {
            return false;
        }
        cap *= 2;
    }
    sprite_t* sprites = realloc(state.sprites, (size_t)cap * sizeof(*sprites));
    if (!sprites) {
        return false;
    }
    state.sprites = sprites;
    state.sprite_cap = cap;
    return true;
}

static texture_t* texture_acquire(int w, int h, int nframes,
                                  uint8_t* const* frames) {
    for (texture_t* t = state.textures; t; t = t->next) {
        if (t->w == w && t->h == h && t->nframes == nframes
            && t->pixels == frames) {
            t->refs++;
            return t;
        }
    }
    texture_t* t = calloc(1, sizeof(*t));
    if (!t) {
        return NULL;
    }
    t->w = w;
    t->h = h;
    t->nframes = nframes;
    t->refs = 1;
    t->pixels = frames;
    for (int f = 0; f < nframes; f++) {
        t->imgs[f] = sg_make_image(&(sg_image_desc){
            .width = w,
            .height = h,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .data.mip_levels[0] = {
                .ptr = frames[f], .size = (size_t)w * h * 4
            },
        });
        if (sg_query_image_state(t->imgs[f]) != SG_RESOURCESTATE_VALID) {
            goto fail;
        }
        t->views[f] = sg_make_view(&(sg_view_desc){
            .texture.image = t->imgs[f],
        });
        if (sg_query_view_state(t->views[f]) != SG_RESOURCESTATE_VALID) {
            goto fail;
        }
    }
    t->next = state.textures;
    state.textures = t;
    return t;

fail:
    for (int f = 0; f < nframes; f++) {
        if (t->views[f].id != SG_INVALID_ID) sg_destroy_view(t->views[f]);
        if (t->imgs[f].id != SG_INVALID_ID) sg_destroy_image(t->imgs[f]);
    }
    free(t);
    return NULL;
}

static void texture_release(texture_t* t) {
    if (!t || --t->refs > 0) {
        return;
    }
    texture_t** link = &state.textures;
    while (*link && *link != t) {
        link = &(*link)->next;
    }
    if (*link == t) {
        *link = t->next;
    }
    for (int f = 0; f < t->nframes; f++) {
        sg_destroy_view(t->views[f]);
        sg_destroy_image(t->imgs[f]);
    }
    free(t);
}

static sprite_t* by_id(int id) {
    for (int i = 0; i < state.nsprites; i++) {
        if (state.sprites[i].id == id) {
            return &state.sprites[i];
        }
    }
    return NULL;
}

// world (meters, y-up, origin bottom-left) -> view (logical px, y-down)
// FLC frames rotate about the CANVAS CENTRE (verified: rotated frames' art
// bboxes stay symmetric about it); objectCentreOfMass is the vector from the
// body origin to that visual centre, a material point of the limb - so the
// offset ROTATES with the body's orientation.
static void body_to_sprite(sprite_t* s, float view_h) {
    float wx, wy;
    phys_body_pos(s->body, &wx, &wy);
    const float th = phys_body_orientation(s->body);
    const float c = cosf(th), sn = sinf(th);
    const float ox = c * s->ax - sn * s->ay; // px, y-up world sense
    const float oy = sn * s->ax + c * s->ay;
    s->x = wx * PHYS_PX_PER_UNIT + ox - (float)s->w / 2.0f;
    s->y = view_h - wy * PHYS_PX_PER_UNIT - oy - (float)s->h / 2.0f;
}

void scene_setup(const sg_environment* env) {
    sg_setup(&(sg_desc){
        .environment = *env,
        .logger.func = slog_func,
        // one image+view per animation frame for now; a texture atlas can
        // shrink this later
        // Instances share textures; these pools bound distinct decoded asset
        // frames rather than the number of toys on screen.
        .image_pool_size = 16384,
        .view_pool_size = 16384,
    });
    // unit quad, expanded to pixel rects in the vertex shader
    const float corners[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
    state.quad = sg_make_buffer(&(sg_buffer_desc){ .data = SG_RANGE(corners) });

    // Smooth original 1x assets when the logical viewport is rasterized into
    // a higher-resolution backing drawable
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_REPEAT,
        .wrap_v = SG_WRAP_REPEAT,
    });

    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .msl_buffer_n = 0,
        },
        .views[0].texture = { .stage = SG_SHADERSTAGE_FRAGMENT, .msl_texture_n = 0 },
        .samplers[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .msl_sampler_n = 0 },
        .texture_sampler_pairs[0] = { .stage = SG_SHADERSTAGE_FRAGMENT, .view_slot = 0, .sampler_slot = 0 },
        .vertex_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct params_t {\n"
            "  float2 pos;\n"
            "  float2 size;\n"
            "  float2 viewport;\n"
            "  float2 uv_scale;\n"
            "  float4 tint;\n"
            "};\n"
            "struct vs_in {\n"
            "  float2 corner [[attribute(0)]];\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 pos [[position]];\n"
            "  float2 uv;\n"
            "  float4 tint;\n"
            "};\n"
            "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
            "  vs_out out;\n"
            "  float2 px = params.pos + in.corner * params.size;\n"
            "  out.pos = float4(2.0 * px.x / params.viewport.x - 1.0,\n"
            "                   1.0 - 2.0 * px.y / params.viewport.y, 0.5, 1.0);\n"
            "  out.uv = in.corner * params.uv_scale;\n"
            "  out.tint = params.tint;\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct fs_in { float2 uv; float4 tint; };\n"
            "fragment float4 _main(fs_in in [[stage_in]],\n"
            "  texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
            "  return tex.sample(smp, in.uv) * in.tint;\n"
            "}\n",
    });

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout.attrs[0] = { .format = SG_VERTEXFORMAT_FLOAT2 },
        .shader = shd,
        .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
        .colors[0].blend = { // premultiplied alpha, same as GDI AlphaBlend
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_ONE,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
            .src_factor_alpha = SG_BLENDFACTOR_ONE,
            .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        },
    });

    state.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 0.0f, 0.0f },
        },
    };
}

int scene_sprite_add(int w, int h, int nframes, uint8_t* const* frames,
                     int speed_ms, float x_px, float y_px, int group) {
    if (nframes < 1 || nframes > MAX_FRAMES || !frames
        || !grow_sprites(state.nsprites + 1)) {
        return -1;
    }
    texture_t* texture = texture_acquire(w, h, nframes, frames);
    if (!texture) {
        return -1;
    }
    sprite_t* s = &state.sprites[state.nsprites++];
    s->id = state.next_id++;
    s->x = x_px;
    s->y = y_px;
    s->w = w;
    s->h = h;
    s->draw_w = (float)w;
    s->draw_h = (float)h;
    s->nframes = nframes;
    s->frame = 0;
    s->speed_ms = speed_ms;
    s->acc_ms = 0;
    s->texture = texture;
    s->body = -1;
    s->ax = 0;
    s->ay = 0;
    s->group = group;
    s->layer = 0;
    s->alpha = 1.0f;
    s->uv_scale[0] = 1.0f;
    s->uv_scale[1] = 1.0f;
    s->visible = true;
    s->animate = true;
    s->clip_enabled = false;
    const int id = s->id;
    // New world sprites may be created after the Toybox. Keep layers sorted
    // so default layer 0 is inserted below any existing UI layer.
    int at = state.nsprites - 1;
    while (at > 0 && state.sprites[at - 1].layer > state.sprites[at].layer) {
        const sprite_t tmp = state.sprites[at - 1];
        state.sprites[at - 1] = state.sprites[at];
        state.sprites[at] = tmp;
        at--;
    }
    return id;
}

void scene_sprite_bind_body(int sprite, int body, float anchor_x, float anchor_y) {
    sprite_t* s = by_id(sprite);
    if (s) {
        s->body = body;
        s->ax = anchor_x;
        s->ay = anchor_y;
    }
}

void scene_sprite_set_position(int sprite, float x_px, float y_px) {
    sprite_t* s = by_id(sprite);
    if (s) {
        s->x = x_px;
        s->y = y_px;
    }
}

void scene_sprite_set_size(int sprite, float w_px, float h_px) {
    sprite_t* s = by_id(sprite);
    if (s && w_px > 0.0f && h_px > 0.0f) {
        s->draw_w = w_px;
        s->draw_h = h_px;
    }
}

void scene_sprite_set_frame(int sprite, int frame) {
    sprite_t* s = by_id(sprite);
    if (s && s->nframes > 0) {
        frame %= s->nframes;
        if (frame < 0) {
            frame += s->nframes;
        }
        s->frame = frame;
        s->animate = false;
    }
}

void scene_sprite_set_alpha(int sprite, float alpha) {
    sprite_t* s = by_id(sprite);
    if (s) {
        s->alpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
    }
}

void scene_sprite_set_visible(int sprite, bool visible) {
    sprite_t* s = by_id(sprite);
    if (s) {
        s->visible = visible;
    }
}

void scene_sprite_set_layer(int sprite, int layer) {
    sprite_t* s = by_id(sprite);
    if (!s || s->layer == layer) {
        return;
    }
    s->layer = layer;
    int at = (int)(s - state.sprites);
    while (at > 0 && state.sprites[at - 1].layer > state.sprites[at].layer) {
        const sprite_t tmp = state.sprites[at - 1];
        state.sprites[at - 1] = state.sprites[at];
        state.sprites[at] = tmp;
        at--;
    }
    while (at + 1 < state.nsprites &&
           state.sprites[at + 1].layer <= state.sprites[at].layer) {
        const sprite_t tmp = state.sprites[at + 1];
        state.sprites[at + 1] = state.sprites[at];
        state.sprites[at] = tmp;
        at++;
    }
}

void scene_sprite_set_uv_scale(int sprite, float u, float v) {
    sprite_t* s = by_id(sprite);
    if (s) {
        s->uv_scale[0] = u;
        s->uv_scale[1] = v;
    }
}

void scene_sprite_set_clip(int sprite, bool enabled, float x_px, float y_px,
                           float w_px, float h_px) {
    sprite_t* s = by_id(sprite);
    if (s) {
        s->clip_enabled = enabled;
        s->clip[0] = x_px;
        s->clip[1] = y_px;
        s->clip[2] = w_px;
        s->clip[3] = h_px;
    }
}

int scene_sprite_body(int sprite) {
    const sprite_t* s = by_id(sprite);
    return s ? s->body : -1;
}

void scene_sprite_remove(int sprite) {
    sprite_t* s = by_id(sprite);
    if (!s) {
        return;
    }
    texture_release(s->texture);
    const int idx = (int)(s - state.sprites);
    for (int k = idx; k < state.nsprites - 1; k++) {
        state.sprites[k] = state.sprites[k + 1];
    }
    state.nsprites--;
}

static int clamp_int(int value, int min, int max) {
    return value < min ? min : (value > max ? max : value);
}

// Sokol scissor rectangles are expressed in render-target pixels. Everything
// above this boundary remains in logical pixels, including hit-testing.
static bool apply_logical_scissor(const sg_swapchain* swapchain,
                                  float view_w, float view_h,
                                  const float clip[4]) {
    const float sx = (float)swapchain->width / view_w;
    const float sy = (float)swapchain->height / view_h;
    int x0 = (int)floorf(clip[0] * sx);
    int y0 = (int)floorf(clip[1] * sy);
    int x1 = (int)ceilf((clip[0] + clip[2]) * sx);
    int y1 = (int)ceilf((clip[1] + clip[3]) * sy);
    x0 = clamp_int(x0, 0, swapchain->width);
    y0 = clamp_int(y0, 0, swapchain->height);
    x1 = clamp_int(x1, 0, swapchain->width);
    y1 = clamp_int(y1, 0, swapchain->height);
    if (x1 <= x0 || y1 <= y0) {
        return false;
    }
    sg_apply_scissor_rect(x0, y0, x1 - x0, y1 - y0, true);
    return true;
}

void scene_frame(const sg_swapchain* swapchain, float view_w, float view_h,
                 double dt_ms) {
    if (view_w <= 0.0f || view_h <= 0.0f) {
        return;
    }
    // physics stepping and world extents now live in the Ruby heartbeat
    // (rbh_frame / rbh_screen_size); the scene only renders and hit-tests
    for (int i = 0; i < state.nsprites; i++) {
        sprite_t* s = &state.sprites[i];
        if (s->body >= 0) { // grabbed sprites follow too: the spring moves them
            body_to_sprite(s, view_h);
            if (s->nframes > 1) {
                // FLC frames are pre-rendered ROTATION phases (GDI can't rotate
                // bitmaps): frame = f(orientation), not a timed animation.
                //
                // alpha principal-axis moments: frames step 2pi/N per index in
                // the visually-CCW direction, frame 0 = theta 0 - same sign
                // as the physics angle.
                const float turns = phys_body_orientation(s->body) / (2.0f * 3.14159265f);
                const float wrapped = turns - (float)(int)turns; // frac, sign kept
                int f = (int)((wrapped < 0 ? wrapped + 1.0f : wrapped) * (float)s->nframes);
                s->frame = (f >= s->nframes) ? 0 : f;
            }
        } else if (s->animate && s->nframes > 1) { // unbound: timed animation
            s->acc_ms += dt_ms;
            while (s->acc_ms >= s->speed_ms) {
                s->acc_ms -= s->speed_ms;
                s->frame = (s->frame + 1) % s->nframes;
            }
        }
    }

    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = *swapchain });
    sg_apply_pipeline(state.pip);
    for (int i = 0; i < state.nsprites; i++) {
        const sprite_t* s = &state.sprites[i];
        if (!s->visible || s->alpha <= 0.0f) {
            continue;
        }
        if (s->clip_enabled) {
            if (!apply_logical_scissor(swapchain, view_w, view_h, s->clip)) {
                continue;
            }
        } else {
            sg_apply_scissor_rect(0, 0, swapchain->width, swapchain->height,
                                  true);
        }
        const vs_params_t params = {
            // original blit positions are integer pixels, trunc toward zero
            // (Toybox.exe converts via fldcw 0xC00 fistp / __ftol2_sse)
            .pos = { (float)(int)s->x, (float)(int)s->y },
            .size = { s->draw_w, s->draw_h },
            .viewport = { view_w, view_h },
            .uv_scale = { s->uv_scale[0], s->uv_scale[1] },
            .tint = { s->alpha, s->alpha, s->alpha, s->alpha },
        };
        sg_apply_bindings(&(sg_bindings){
            .vertex_buffers[0] = state.quad,
            .views[0] = s->texture->views[s->frame],
            .samplers[0] = state.smp,
        });
        sg_apply_uniforms(0, &SG_RANGE(params));
        sg_draw(0, 4, 1);
    }
    sg_end_pass();
    sg_commit();
}

void scene_shutdown(void) {
    while (state.nsprites > 0) {
        scene_sprite_remove(state.sprites[state.nsprites - 1].id);
    }
    free(state.sprites);
    state.sprites = NULL;
    state.sprite_cap = 0;
    sg_shutdown();
}

// topmost sprite whose non-transparent pixel covers the point, or -1.
// Matches the original layered window's per-pixel hit test (alpha > 0 hits).
static int sprite_at(float x, float y) {
    for (int i = state.nsprites - 1; i >= 0; i--) {
        const sprite_t* s = &state.sprites[i];
        if (!s->visible || s->alpha <= 0.0f) {
            continue;
        }
        if (s->clip_enabled &&
            (x < s->clip[0] || y < s->clip[1] ||
             x >= s->clip[0] + s->clip[2] ||
             y >= s->clip[1] + s->clip[3])) {
            continue;
        }
        const float dx = x - (float)(int)s->x;
        const float dy = y - (float)(int)s->y;
        if (dx < 0.0f || dy < 0.0f || dx >= s->draw_w || dy >= s->draw_h) {
            continue;
        }
        int lx = (int)(dx * (float)s->w * s->uv_scale[0] / s->draw_w);
        int ly = (int)(dy * (float)s->h * s->uv_scale[1] / s->draw_h);
        lx %= s->w;
        ly %= s->h;
        if (s->texture->pixels[s->frame]
                [((size_t)ly * s->w + lx) * 4 + 3] > 0) {
            return i;
        }
    }
    return -1;
}

bool scene_hit_test(float x, float y) {
    return sprite_at(x, y) >= 0;
}

int scene_pick(float x, float y) {
    const int i = sprite_at(x, y);
    return i >= 0 ? state.sprites[i].id : -1;
}

void scene_raise(int sprite) {
    const sprite_t* hit = by_id(sprite);
    if (!hit) {
        return;
    }
    const int group = hit->group;
    const int layer = hit->layer;
    sprite_t* reordered = malloc((size_t)state.nsprites * sizeof(*reordered));
    if (!reordered) {
        return;
    }
    int n = 0;
    for (int k = 0; k < state.nsprites; k++) {
        if (state.sprites[k].layer < layer) {
            reordered[n++] = state.sprites[k];
        }
    }
    for (int k = 0; k < state.nsprites; k++) {
        if (state.sprites[k].layer == layer && state.sprites[k].group != group) {
            reordered[n++] = state.sprites[k];
        }
    }
    for (int k = 0; k < state.nsprites; k++) {
        if (state.sprites[k].layer == layer && state.sprites[k].group == group) {
            reordered[n++] = state.sprites[k];
        }
    }
    for (int k = 0; k < state.nsprites; k++) {
        if (state.sprites[k].layer > layer) {
            reordered[n++] = state.sprites[k];
        }
    }
    for (int k = 0; k < n; k++) {
        state.sprites[k] = reordered[k];
    }
    free(reordered);
}
