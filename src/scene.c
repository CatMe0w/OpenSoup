#include "scene.h"
#include "sokol_log.h"

typedef struct {
    float offset[2];
    float brightness;
    float _pad;
} vs_params_t;

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    float pos[2];       // triangle centre in NDC
    bool grabbed;
    float grab_off[2];  // grab point relative to centre
} state;

// triangle corners relative to centre, NDC
static const float tri[3][2] = {
    {  0.0f,  0.25f },
    {  0.2f, -0.15f },
    { -0.2f, -0.15f },
};

void scene_setup(const sg_environment* env) {
    sg_setup(&(sg_desc){
        .environment = *env,
        .logger.func = slog_func,
    });

    float vertices[] = {
        // positions          colors
        tri[0][0], tri[0][1], 1.0f, 0.2f, 0.2f, 1.0f,
        tri[1][0], tri[1][1], 0.2f, 1.0f, 0.2f, 1.0f,
        tri[2][0], tri[2][1], 0.2f, 0.2f, 1.0f, 1.0f,
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .msl_buffer_n = 0,
        },
        .vertex_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "struct params_t {\n"
            "  float2 offset;\n"
            "  float brightness;\n"
            "};\n"
            "struct vs_in {\n"
            "  float2 position [[attribute(0)]];\n"
            "  float4 color [[attribute(1)]];\n"
            "};\n"
            "struct vs_out {\n"
            "  float4 pos [[position]];\n"
            "  float4 color;\n"
            "};\n"
            "vertex vs_out _main(vs_in in [[stage_in]], constant params_t& params [[buffer(0)]]) {\n"
            "  vs_out out;\n"
            "  out.pos = float4(in.position + params.offset, 0.5, 1.0);\n"
            "  out.color = float4(in.color.rgb * params.brightness, in.color.a);\n"
            "  return out;\n"
            "}\n",
        .fragment_func.source =
            "#include <metal_stdlib>\n"
            "using namespace metal;\n"
            "fragment float4 _main(float4 color [[stage_in]]) {\n"
            "  return color;\n"
            "}\n",
    });

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            .attrs = {
                [0] = { .format = SG_VERTEXFORMAT_FLOAT2 },
                [1] = { .format = SG_VERTEXFORMAT_FLOAT4 },
            },
        },
        .shader = shd,
    });

    // transparent overlay: clear to alpha 0 so the desktop shows through
    state.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 0.0f, 0.0f },
        },
    };
}

void scene_frame(const sg_swapchain* swapchain) {
    const vs_params_t params = {
        .offset = { state.pos[0], state.pos[1] },
        .brightness = state.grabbed ? 1.0f : 0.75f,
    };
    sg_begin_pass(&(sg_pass){ .action = state.pass_action, .swapchain = *swapchain });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(0, &SG_RANGE(params));
    sg_draw(0, 3, 1);
    sg_end_pass();
    sg_commit();
}

void scene_shutdown(void) {
    sg_shutdown();
}

static float edge(const float a[2], const float b[2], float px, float py) {
    return (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]);
}

bool scene_hit_test(float x, float y) {
    const float px = x - state.pos[0];
    const float py = y - state.pos[1];
    const float e0 = edge(tri[0], tri[1], px, py);
    const float e1 = edge(tri[1], tri[2], px, py);
    const float e2 = edge(tri[2], tri[0], px, py);
    return (e0 <= 0 && e1 <= 0 && e2 <= 0) || (e0 >= 0 && e1 >= 0 && e2 >= 0);
}

bool scene_grab_begin(float x, float y) {
    if (!scene_hit_test(x, y)) {
        return false;
    }
    state.grabbed = true;
    state.grab_off[0] = x - state.pos[0];
    state.grab_off[1] = y - state.pos[1];
    return true;
}

void scene_grab_move(float x, float y) {
    if (state.grabbed) {
        state.pos[0] = x - state.grab_off[0];
        state.pos[1] = y - state.grab_off[1];
    }
}

void scene_grab_end(void) {
    state.grabbed = false;
}

bool scene_grabbing(void) {
    return state.grabbed;
}
