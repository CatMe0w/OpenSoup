#include "scene_demo.h"

#include "sokol_log.h"

typedef struct {
    float offset[2];
    float viewport[2];
    float brightness;
    float _pad[3];
} vs_params_t;

static struct {
    sg_pass_action pass_action;
    sg_pipeline pip;
    sg_bindings bind;
    float pos[2];       // triangle centre, logical pixels
    bool grabbed;
    float grab_off[2];  // grab point relative to centre
} state;

// triangle corners relative to centre, logical pixels (y down)
static const float tri[3][2] = {
    {    0.0f, -125.0f },
    {  100.0f,   75.0f },
    { -100.0f,   75.0f },
};

// D3D11 loads d3dcompiler_47.dll on demand, so plain HLSL source is enough
static const char* vs_source =
    "cbuffer params : register(b0) {\n"
    "  float2 offset;\n"
    "  float2 viewport;\n"
    "  float brightness;\n"
    "};\n"
    "struct vs_in {\n"
    "  float2 position: POSITION;\n"
    "  float4 color: COLOR;\n"
    "};\n"
    "struct vs_out {\n"
    "  float4 position: SV_Position;\n"
    "  float4 color: COLOR0;\n"
    "};\n"
    "vs_out main(vs_in inp) {\n"
    "  float2 p = inp.position + offset;\n"
    "  vs_out outp;\n"
    "  outp.position = float4(2.0 * p.x / viewport.x - 1.0,\n"
    "                         1.0 - 2.0 * p.y / viewport.y, 0.5, 1.0);\n"
    "  outp.color = float4(inp.color.rgb * brightness, inp.color.a);\n"
    "  return outp;\n"
    "}\n";

static const char* fs_source =
    "struct fs_in {\n"
    "  float4 position: SV_Position;\n"
    "  float4 color: COLOR0;\n"
    "};\n"
    "float4 main(fs_in inp): SV_Target0 {\n"
    "  return inp.color;\n"
    "}\n";

void scene_demo_setup(const sg_environment* env, float width, float height) {
    sg_setup(&(sg_desc){
        .environment = *env,
        .logger.func = slog_func,
    });

    state.pos[0] = width * 0.5f;
    state.pos[1] = height * 0.5f;

    const float vertices[] = {
        // positions            colors
        tri[0][0], tri[0][1],   1.0f, 0.2f, 0.2f, 1.0f,
        tri[1][0], tri[1][1],   0.2f, 1.0f, 0.2f, 1.0f,
        tri[2][0], tri[2][1],   0.2f, 0.2f, 1.0f, 1.0f,
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices)
    });

    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vertex_func.source = vs_source,
        .fragment_func.source = fs_source,
        .attrs = {
            [0] = { .hlsl_sem_name = "POSITION" },
            [1] = { .hlsl_sem_name = "COLOR" },
        },
        .uniform_blocks[0] = {
            .stage = SG_SHADERSTAGE_VERTEX,
            .size = sizeof(vs_params_t),
            .hlsl_register_b_n = 0,
        },
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

    // transparent scene: clear to premultiplied zero so the desktop shows
    // through everywhere the scene does not draw
    state.pass_action = (sg_pass_action){
        .colors[0] = {
            .load_action = SG_LOADACTION_CLEAR,
            .clear_value = { 0.0f, 0.0f, 0.0f, 0.0f },
        },
    };
}

void scene_demo_frame(const sg_swapchain* swapchain, float width,
                      float height) {
    const vs_params_t params = {
        .offset = { state.pos[0], state.pos[1] },
        .viewport = { width, height },
        .brightness = state.grabbed ? 1.0f : 0.75f,
    };
    sg_begin_pass(&(sg_pass){
        .action = state.pass_action,
        .swapchain = *swapchain,
    });
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    sg_apply_uniforms(0, &SG_RANGE(params));
    sg_draw(0, 3, 1);
    sg_end_pass();
    sg_commit();
}

void scene_demo_shutdown(void) {
    sg_shutdown();
}

static float edge(const float a[2], const float b[2], float px, float py) {
    return (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]);
}

bool scene_demo_hit_test(float x, float y) {
    const float px = x - state.pos[0];
    const float py = y - state.pos[1];
    const float e0 = edge(tri[0], tri[1], px, py);
    const float e1 = edge(tri[1], tri[2], px, py);
    const float e2 = edge(tri[2], tri[0], px, py);
    return (e0 <= 0 && e1 <= 0 && e2 <= 0) || (e0 >= 0 && e1 >= 0 && e2 >= 0);
}

bool scene_demo_grab_begin(float x, float y) {
    if (!scene_demo_hit_test(x, y)) {
        return false;
    }
    state.grabbed = true;
    state.grab_off[0] = x - state.pos[0];
    state.grab_off[1] = y - state.pos[1];
    return true;
}

void scene_demo_grab_move(float x, float y) {
    if (state.grabbed) {
        state.pos[0] = x - state.grab_off[0];
        state.pos[1] = y - state.grab_off[1];
    }
}

void scene_demo_grab_end(void) {
    state.grabbed = false;
}

bool scene_demo_grabbing(void) {
    return state.grabbed;
}
