#include "scene.h"
#include "physics.h"
#include "sokol_log.h"

#define MAX_SPRITES 64
#define MAX_FRAMES 256

typedef struct {
    float x, y; // fractional position lives here; blit position is trunc'd
    int w, h;
    int nframes;
    int frame;
    int speed_ms;
    double acc_ms;
    sg_view views[MAX_FRAMES];
    uint8_t* const* pixels; // borrowed premultiplied RGBA, for alpha hit-test
    int body;               // physics body index or -1
} sprite_t;

typedef struct {
    float pos[2];
    float size[2];
    float viewport[2];
    float _pad[2];
} vs_params_t;

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_buffer quad;
    sg_sampler smp;
    sprite_t sprites[MAX_SPRITES];
    int nsprites;
    int grabbed;        // sprite index or -1
    float grab_off[2];
    double phys_acc_ms; // fixed-step accumulator
    float view_h;       // device px, for world<->view y flip
    bool world_ready;
} state;

// world (meters, y-up, origin bottom-left) <-> view (device px, y-down)
static void body_to_sprite(sprite_t* s) {
    float wx, wy;
    phys_body_pos(s->body, &wx, &wy);
    s->x = wx * PHYS_PX_PER_UNIT - (float)s->w / 2.0f;
    s->y = state.view_h - wy * PHYS_PX_PER_UNIT - (float)s->h / 2.0f;
}

void scene_setup(const sg_environment* env) {
    sg_setup(&(sg_desc){
        .environment = *env,
        .logger.func = slog_func,
        // one image+view per animation frame for now; a texture atlas can
        // shrink this later
        .image_pool_size = 4096,
        .view_pool_size = 4096,
    });
    state.grabbed = -1;

    // unit quad, expanded to pixel rects in the vertex shader
    const float corners[] = { 0, 0, 1, 0, 0, 1, 1, 1 };
    state.quad = sg_make_buffer(&(sg_buffer_desc){ .data = SG_RANGE(corners) });

    // 1:1 texel-to-pixel is the contract; nearest filtering enforces it
    state.smp = sg_make_sampler(&(sg_sampler_desc){
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
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
            "};\n"
            "struct vs_in {\n"
            "  float2 corner [[attribute(0)]];\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 pos [[position]];\n"
            "  float2 uv;\n"
            "};\n"
            "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
            "  vs_out out;\n"
            "  float2 px = params.pos + in.corner * params.size;\n"
            "  out.pos = float4(2.0 * px.x / params.viewport.x - 1.0,\n"
            "                   1.0 - 2.0 * px.y / params.viewport.y, 0.5, 1.0);\n"
            "  out.uv = in.corner;\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "fragment float4 _main(float2 uv [[stage_in]],\n"
            "  texture2d<float> tex [[texture(0)]], sampler smp [[sampler(0)]]) {\n"
            "  return tex.sample(smp, uv);\n"
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
                     int speed_ms, float x_px, float y_px) {
    if (state.nsprites >= MAX_SPRITES || nframes < 1 || nframes > MAX_FRAMES) {
        return -1;
    }
    sprite_t* s = &state.sprites[state.nsprites];
    s->x = x_px;
    s->y = y_px;
    s->w = w;
    s->h = h;
    s->nframes = nframes;
    s->frame = 0;
    s->speed_ms = speed_ms;
    s->acc_ms = 0;
    s->pixels = frames;
    s->body = -1;
    for (int f = 0; f < nframes; f++) {
        s->views[f] = sg_make_view(&(sg_view_desc){
            .texture.image = sg_make_image(&(sg_image_desc){
                .width = w,
                .height = h,
                .pixel_format = SG_PIXELFORMAT_RGBA8,
                .data.mip_levels[0] = { .ptr = frames[f], .size = (size_t)w * h * 4 },
            }),
        });
    }
    return state.nsprites++;
}

void scene_sprite_bind_body(int sprite, int body) {
    state.sprites[sprite].body = body;
}

void scene_frame(const sg_swapchain* swapchain, double dt_ms) {
    // world extents follow the screen (fit_scene_to_canvas: screen/scale)
    state.view_h = (float)swapchain->height;
    if (!state.world_ready) {
        phys_set_world((float)swapchain->width / PHYS_PX_PER_UNIT,
                       (float)swapchain->height / PHYS_PX_PER_UNIT);
        state.world_ready = true;
    }

    // fixed-timestep accumulator, original cadence: 0.01s per step
    state.phys_acc_ms += dt_ms;
    const int nsteps = (int)(state.phys_acc_ms / (PHYS_DT * 1000.0));
    if (nsteps > 0) {
        state.phys_acc_ms -= nsteps * (PHYS_DT * 1000.0);
        phys_steps(nsteps > 25 ? 25 : nsteps); // clamp long stalls
    }
    for (int i = 0; i < state.nsprites; i++) {
        sprite_t* s = &state.sprites[i];
        if (s->body >= 0) { // grabbed sprites follow too: the spring moves them
            body_to_sprite(s);
            if (s->nframes > 1) {
                // FLC frames are pre-rendered ROTATION phases (GDI can't rotate
                // bitmaps): frame = f(orientation), not a timed animation.
                // TODO(verify): frame direction/phase vs the original renderer
                const float turns = -phys_body_orientation(s->body) / (2.0f * 3.14159265f);
                const float wrapped = turns - (float)(int)turns; // frac, sign kept
                int f = (int)((wrapped < 0 ? wrapped + 1.0f : wrapped) * (float)s->nframes);
                s->frame = (f >= s->nframes) ? 0 : f;
            }
        } else if (s->nframes > 1) { // unbound sprites: timed animation
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
        const vs_params_t params = {
            // original blit positions are integer pixels, trunc toward zero
            // (Toybox.exe converts via fldcw 0xC00 fistp / __ftol2_sse)
            .pos = { (float)(int)s->x, (float)(int)s->y },
            .size = { (float)s->w, (float)s->h },
            .viewport = { (float)swapchain->width, (float)swapchain->height },
        };
        sg_apply_bindings(&(sg_bindings){
            .vertex_buffers[0] = state.quad,
            .views[0] = s->views[s->frame],
            .samplers[0] = state.smp,
        });
        sg_apply_uniforms(0, &SG_RANGE(params));
        sg_draw(0, 4, 1);
    }
    sg_end_pass();
    sg_commit();
}

void scene_shutdown(void) {
    sg_shutdown();
}

// topmost sprite whose non-transparent pixel covers the point, or -1.
// Matches the original layered window's per-pixel hit test (alpha > 0 hits).
static int sprite_at(float x, float y) {
    for (int i = state.nsprites - 1; i >= 0; i--) {
        const sprite_t* s = &state.sprites[i];
        const int lx = (int)x - (int)s->x;
        const int ly = (int)y - (int)s->y;
        if (lx < 0 || ly < 0 || lx >= s->w || ly >= s->h) {
            continue;
        }
        if (s->pixels[s->frame][((size_t)ly * s->w + lx) * 4 + 3] > 0) {
            return i;
        }
    }
    return -1;
}

bool scene_hit_test(float x, float y) {
    return sprite_at(x, y) >= 0;
}

bool scene_grab_begin(float x, float y) {
    const int i = sprite_at(x, y);
    if (i < 0) {
        return false;
    }
    // bring to front so it draws above and hit-tests first
    sprite_t tmp = state.sprites[i];
    for (int k = i; k < state.nsprites - 1; k++) {
        state.sprites[k] = state.sprites[k + 1];
    }
    state.sprites[state.nsprites - 1] = tmp;
    state.grabbed = state.nsprites - 1;
    state.grab_off[0] = x - tmp.x;
    state.grab_off[1] = y - tmp.y;
    if (tmp.body >= 0) {
        phys_grab(tmp.body);
    }
    return true;
}

void scene_grab_move(float x, float y) {
    if (state.grabbed < 0) {
        return;
    }
    sprite_t* s = &state.sprites[state.grabbed];
    if (s->body >= 0) {
        // cursor -> desired sprite centre -> world = the spring target;
        // the body (and thus the sprite) follows via the mouse spring
        const float cx = x - state.grab_off[0] + (float)s->w / 2.0f;
        const float cy = y - state.grab_off[1] + (float)s->h / 2.0f;
        phys_grab_move(s->body, cx / PHYS_PX_PER_UNIT,
                       (state.view_h - cy) / PHYS_PX_PER_UNIT);
    } else {
        s->x = x - state.grab_off[0];
        s->y = y - state.grab_off[1];
    }
}

void scene_grab_end(void) {
    if (state.grabbed >= 0) {
        const sprite_t* s = &state.sprites[state.grabbed];
        if (s->body >= 0) {
            phys_release(s->body);
        }
    }
    state.grabbed = -1;
}

bool scene_grabbing(void) {
    return state.grabbed >= 0;
}
