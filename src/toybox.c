#include "toybox.h"

#include "assets.h"
#include "assets_layout.h"
#include "audio.h"
#include "rubyhost.h"
#include "scene.h"
#include "toydefs.h"
#include "toybox_button.h"
#include "toybox_scroll.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TB_MAX_ASSET_FRAMES 6
#define TB_MAX_SHELL 24
#define TB_MAX_PACKS 16
#define TB_MAX_ICONS 192

#define TB_W 400.0f
#define TB_H 460.0f
#define TB_PANEL_X 24.0f
#define TB_PANEL_Y 68.0f
#define TB_PANEL_W 352.0f
#define TB_PANEL_H 348.0f
#define TB_CELL_W 80.0f
#define TB_CELL_H 80.0f
#define TB_HEADER_H 80.0f
#define TB_BUTTON_PERIOD_S 0.14f

typedef struct {
    int w, h;
    int nframes;
    uint8_t* frames[TB_MAX_ASSET_FRAMES];
    int sprite;
} tb_asset;

typedef struct {
    const rbh_toypack* def;
    char header_path[1024];
    tb_asset header;
    int icon_count;
    float x, y;
} tb_pack;

typedef struct {
    const toyicon_t* def;
    const toydef_t* toy;
    int pack;
    tb_asset asset;
    float x, y;
    bool onscreen;
} tb_icon;

typedef struct {
    float x, y, w, h;
} tb_rect;

enum {
    SH_WOOD,
    SH_BLUE,
    SH_OUT_TOP,
    SH_OUT_BOTTOM,
    SH_OUT_LEFT,
    SH_OUT_RIGHT,
    SH_IN_TOP,
    SH_IN_BOTTOM,
    SH_IN_LEFT,
    SH_IN_RIGHT,
    SH_LOGO,
    SH_MIN,
    SH_CLOSE,
    SH_SCROLL_TOP,
    SH_SCROLL_MIDDLE,
    SH_SCROLL_BOTTOM,
    SH_BACKGROUND,
    SH_PLAY,
    SH_REC,
    SH_REWIND,
    SH_CLEAR,
    SH_MUTE,
    SH_HELP,
    SH_WEB,
    SH_COUNT
};

static struct {
    bool ready;
    bool visible;
    char assets_root[1024];
    float view_w, view_h;
    float x, y;
    tb_rect panel;
    tb_rect scroll_track;
    tb_rect scroll_thumb;
    float max_scroll, content_h;
    toybox_scroll_model scroll;
    tb_asset shell[TB_MAX_SHELL];
    tb_pack packs[TB_MAX_PACKS];
    int npacks;
    tb_icon icons[TB_MAX_ICONS];
    int nicons;
    int hover;
    int pressed;
    tb_rect buttons[SH_COUNT];
    toybox_button_model button_models[SH_COUNT];
    int button_pressed;
    bool quit_requested;
    bool moving;
    bool scrolling;
    float move_dx, move_dy;
    float scroll_drag_y, scroll_drag_target;
    int preview_sprite;
} tb;

static const int animated_buttons[] = {
    SH_CLOSE,
    SH_CLEAR, SH_PLAY, SH_REC, SH_REWIND,
    SH_BACKGROUND, SH_MUTE, SH_HELP, SH_WEB,
};

static bool inside(tb_rect r, float x, float y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static bool intersects(tb_rect a, tb_rect b) {
    return a.x < b.x + b.w && a.x + a.w > b.x &&
           a.y < b.y + b.h && a.y + a.h > b.y;
}

static void asset_reset(tb_asset* a) {
    memset(a, 0, sizeof(*a));
    a->sprite = -1;
}

static bool asset_scene_add(tb_asset* a, float x, float y, int layer) {
    if (a->nframes < 1) {
        return false;
    }
    a->sprite = scene_sprite_add(a->w, a->h, a->nframes, a->frames, 0,
                                 x, y, SCENE_GROUP_UI);
    if (a->sprite < 0) {
        return false;
    }
    scene_sprite_set_frame(a->sprite, 0);
    scene_sprite_set_layer(a->sprite, layer);
    return true;
}

static bool asset_load_sequence(tb_asset* a, const char* prefix,
                                int requested, float x, float y) {
    asset_reset(a);
    if (requested > TB_MAX_ASSET_FRAMES) {
        requested = TB_MAX_ASSET_FRAMES;
    }
    for (int i = 0; i < requested; i++) {
        char path[2048];
        snprintf(path, sizeof path, "%s/%s%04d.tga", tb.assets_root,
                 prefix, i);
        as_image img = {0};
        if (!as_load_tga(path, &img)) {
            break;
        }
        if (a->nframes == 0) {
            a->w = img.w;
            a->h = img.h;
        } else if (img.w != a->w || img.h != a->h) {
            as_image_free(&img);
            break;
        }
        a->frames[a->nframes++] = img.rgba;
    }
    return asset_scene_add(a, x, y, SCENE_LAYER_UI);
}

static bool asset_load_frame(tb_asset* a, const char* prefix, int frame,
                             float x, float y) {
    asset_reset(a);
    char path[2048];
    snprintf(path, sizeof path, "%s/%s%04d.tga", tb.assets_root, prefix,
             frame);
    as_image img = {0};
    if (!as_load_tga(path, &img)) {
        return false;
    }
    a->w = img.w;
    a->h = img.h;
    a->nframes = 1;
    a->frames[0] = img.rgba;
    return asset_scene_add(a, x, y, SCENE_LAYER_UI);
}

static bool asset_load_flc(tb_asset* a, const char* color_rel,
                           const char* alpha_rel, float x, float y) {
    asset_reset(a);
    char color[2048], alpha[2048];
    snprintf(color, sizeof color, "%s/%s", tb.assets_root, color_rel);
    const char* alpha_path = NULL;
    if (alpha_rel) {
        snprintf(alpha, sizeof alpha, "%s/%s", tb.assets_root, alpha_rel);
        alpha_path = alpha;
    }
    as_anim anim = {0};
    if (!as_load_flc(color, alpha_path, &anim)) {
        return false;
    }
    a->w = anim.w;
    a->h = anim.h;
    a->nframes = anim.frame_count > TB_MAX_ASSET_FRAMES
               ? TB_MAX_ASSET_FRAMES : anim.frame_count;
    for (int i = 0; i < a->nframes; i++) {
        a->frames[i] = anim.frames[i];
    }
    for (int i = a->nframes; i < anim.frame_count; i++) {
        free(anim.frames[i]);
    }
    free(anim.frames);
    return asset_scene_add(a, x, y, SCENE_LAYER_UI);
}

static void asset_destroy(tb_asset* a) {
    if (a->nframes > 0 && a->sprite >= 0) {
        scene_sprite_remove(a->sprite);
    }
    for (int i = 0; i < a->nframes; i++) {
        free(a->frames[i]);
    }
    asset_reset(a);
}

static void asset_rect(tb_asset* a, float x, float y, float w, float h,
                       bool tile) {
    if (a->sprite < 0) {
        return;
    }
    scene_sprite_set_position(a->sprite, x, y);
    scene_sprite_set_size(a->sprite, w, h);
    scene_sprite_set_uv_scale(a->sprite,
                              tile ? w / (float)a->w : 1.0f,
                              tile ? h / (float)a->h : 1.0f);
}

static void asset_pos(tb_asset* a, float x, float y) {
    if (a->sprite >= 0) {
        scene_sprite_set_position(a->sprite, x, y);
        scene_sprite_set_size(a->sprite, (float)a->w, (float)a->h);
        scene_sprite_set_uv_scale(a->sprite, 1.0f, 1.0f);
    }
}

static void asset_visible(tb_asset* a, bool visible) {
    if (a->sprite >= 0) {
        scene_sprite_set_visible(a->sprite, visible);
    }
}

static void button_pos(int slot, float x, float y) {
    tb_asset* asset = &tb.shell[slot];
    asset_pos(asset, x, y);
    tb.buttons[slot] = (tb_rect){x, y, (float)asset->w, (float)asset->h};
}

static int button_at(float x, float y) {
    for (size_t i = 0;
         i < sizeof animated_buttons / sizeof animated_buttons[0];
         i++) {
        const int slot = animated_buttons[i];
        if (tb.shell[slot].sprite >= 0 && inside(tb.buttons[slot], x, y)) {
            return slot;
        }
    }
    return -1;
}

static void refresh_button(int slot) {
    tb_asset* asset = &tb.shell[slot];
    if (asset->sprite < 0 || asset->nframes < 1) {
        return;
    }
    scene_sprite_set_frame(asset->sprite,
        toybox_button_frame(&tb.button_models[slot], asset->nframes));
}

static void refresh_buttons(void) {
    for (size_t i = 0;
         i < sizeof animated_buttons / sizeof animated_buttons[0];
         i++) {
        refresh_button(animated_buttons[i]);
    }
}

static const char* button_command(int slot) {
    switch (slot) {
        case SH_CLOSE: return "quitToyBox";
        case SH_PLAY: return "open";
        case SH_REC: return "save";
        case SH_CLEAR: return "clear";
        case SH_MUTE: return "mute";
        case SH_REWIND: return "restartPlayset";
        case SH_BACKGROUND: return "toggle_background";
        case SH_HELP: return "toggle_help";
        case SH_WEB: return "openWebsite";
        default: return "unknown";
    }
}

static bool activate_button(int slot, bool desired_on) {
    switch (slot) {
        case SH_CLOSE:
            tb.quit_requested = true;
            return true;
        case SH_CLEAR:
            if (!rbh_clear_scene()) {
                fprintf(stderr, "toybox: clear failed\n");
                return false;
            }
            return true;
        case SH_MUTE:
            if (audio_set_muted(desired_on)) {
                printf("toybox: mute %s\n", desired_on ? "on" : "off");
                return true;
            } else {
                fprintf(stderr, "toybox: mute failed\n");
                return false;
            }
        default:
            // Keep the command identity honest until playset I/O, Background,
            // F10 help, and platform URL opening have their real backends.
            if (tb.button_models[slot].toggle) {
                printf("toybox: command %s.%s is not implemented yet\n",
                       button_command(slot), desired_on ? "on" : "off");
            } else {
                printf("toybox: command %s is not implemented yet\n",
                       button_command(slot));
            }
            return false;
    }
}

static int pack_for_id(const char* pack_id) {
    for (int i = 0; pack_id && i < tb.npacks; i++) {
        if (strcmp(tb.packs[i].def->id, pack_id) == 0) {
            return i;
        }
    }
    return -1;
}

static void sort_icons(void) {
    // IconGridStackItem sorts greater orders first. Insertion sort preserves
    // manifest order when two entries have the same order.
    for (int i = 1; i < tb.nicons; i++) {
        tb_icon icon = tb.icons[i];
        int j = i;
        while (j > 0) {
            const tb_icon* prev = &tb.icons[j - 1];
            const bool before = icon.pack < prev->pack ||
                (icon.pack == prev->pack &&
                 icon.def->order > prev->def->order);
            if (!before) {
                break;
            }
            tb.icons[j] = tb.icons[j - 1];
            j--;
        }
        tb.icons[j] = icon;
    }
}

static void update_content_height(void) {
    float h = 8.0f;
    const int cols = 4;
    for (int p = 0; p < tb.npacks; p++) {
        const int count = tb.packs[p].icon_count;
        if (count == 0) {
            continue;
        }
        h += TB_HEADER_H;
        const int rows = (count + cols - 1) / cols;  // ceil(count / cols)
        h += (float)rows * TB_CELL_H;
        h += 8.0f;
    }
    tb.content_h = h;
    tb.max_scroll = fmaxf(0.0f, h - tb.panel.h);
    if (tb.scroll.target > tb.max_scroll) {
        toybox_scroll_model_set_target(&tb.scroll, tb.max_scroll);
    }
}

static void layout_content(void) {
    const tb_rect clip = {tb.panel.x + 3.0f, tb.panel.y + 3.0f,
                          tb.panel.w - 24.0f, tb.panel.h - 6.0f};
    // The original scrollbar lives in the wood channel immediately to the
    // right of the blue catalog, not inside the catalog's clipping rect.
    const tb_rect scroll_clip = {tb.panel.x + tb.panel.w,
                                 tb.panel.y + 3.0f,
                                 20.0f, tb.panel.h - 6.0f};
    tb.scroll_track = scroll_clip;
    tb.scroll_thumb = (tb_rect){0};
    float y = tb.panel.y + 8.0f - tb.scroll.position;
    const int cols = 4;

    for (int p = 0; p < tb.npacks; p++) {
        tb_pack* pack = &tb.packs[p];
        if (pack->icon_count == 0) {
            asset_visible(&pack->header, false);
            continue;
        }
        pack->x = clip.x + 8.0f;
        pack->y = y + (TB_HEADER_H - (float)pack->header.h) * 0.5f;
        const tb_rect hr = {pack->x, pack->y, (float)pack->header.w,
                            (float)pack->header.h};
        asset_pos(&pack->header, pack->x, pack->y);
        scene_sprite_set_clip(pack->header.sprite, true, clip.x, clip.y,
                              clip.w, clip.h);
        asset_visible(&pack->header, tb.visible && intersects(hr, clip));
        y += TB_HEADER_H;

        int n = 0;
        for (int i = 0; i < tb.nicons; i++) {
            tb_icon* icon = &tb.icons[i];
            if (icon->pack != p) {
                continue;
            }
            const int col = n % cols;
            const int row = n / cols;
            icon->x = clip.x + (float)col * TB_CELL_W +
                      (TB_CELL_W - (float)icon->asset.w) * 0.5f;
            icon->y = y + (float)row * TB_CELL_H +
                      (TB_CELL_H - (float)icon->asset.h) * 0.5f;
            const tb_rect ir = {icon->x, icon->y, (float)icon->asset.w,
                                (float)icon->asset.h};
            icon->onscreen = tb.visible && intersects(ir, clip);
            asset_pos(&icon->asset, icon->x, icon->y);
            scene_sprite_set_clip(icon->asset.sprite, true, clip.x, clip.y,
                                  clip.w, clip.h);
            asset_visible(&icon->asset, icon->onscreen);
            n++;
        }
        const int rows = (pack->icon_count + cols - 1) / cols;  // ceil(count / cols)
        y += (float)rows * TB_CELL_H;
        y += 8.0f;
    }

    tb_asset* scroll_top = &tb.shell[SH_SCROLL_TOP];
    tb_asset* scroll_middle = &tb.shell[SH_SCROLL_MIDDLE];
    tb_asset* scroll_bottom = &tb.shell[SH_SCROLL_BOTTOM];
    if (scroll_top->sprite >= 0 && scroll_middle->sprite >= 0 &&
        scroll_bottom->sprite >= 0) {
        const float ratio = tb.content_h > 0.0f
                          ? fminf(1.0f, tb.panel.h / tb.content_h) : 1.0f;
        const float cap_h = (float)(scroll_top->h + scroll_bottom->h);
        // ScrollbarGUIComponent scales only the flexible middle span; the two
        // arrow caps retain their full height. This is longer than simply
        // multiplying the whole thumb by the visible/content ratio.
        const float flexible_h = fmaxf(0.0f, scroll_clip.h - cap_h);
        const float middle_h = fmaxf(1.0f, flexible_h * ratio);
        const float thumb_h = cap_h + middle_h;
        const float t = tb.max_scroll > 0.0f
                      ? tb.scroll.target / tb.max_scroll : 0.0f;
        const float thumb_y = scroll_clip.y + (scroll_clip.h - thumb_h) * t;
        const float thumb_x = scroll_clip.x + scroll_clip.w - scroll_top->w;
        const float middle_y = thumb_y + (float)scroll_top->h;
        tb.scroll_thumb = (tb_rect){thumb_x, thumb_y,
                                    (float)scroll_top->w, thumb_h};
        asset_pos(scroll_top, thumb_x, thumb_y);
        asset_rect(scroll_middle,
                   thumb_x + ((float)scroll_top->w - scroll_middle->w) * 0.5f,
                   middle_y, (float)scroll_middle->w, middle_h, true);
        asset_pos(scroll_bottom, thumb_x,
                  thumb_y + thumb_h - (float)scroll_bottom->h);
        scene_sprite_set_clip(scroll_top->sprite, true, scroll_clip.x,
                              scroll_clip.y, scroll_clip.w, scroll_clip.h);
        scene_sprite_set_clip(scroll_middle->sprite, true, scroll_clip.x,
                              scroll_clip.y, scroll_clip.w, scroll_clip.h);
        scene_sprite_set_clip(scroll_bottom->sprite, true, scroll_clip.x,
                              scroll_clip.y, scroll_clip.w, scroll_clip.h);
        const bool visible = tb.visible && tb.max_scroll > 0.0f;
        asset_visible(scroll_top, visible);
        asset_visible(scroll_middle, visible);
        asset_visible(scroll_bottom, visible);
    } else {
        asset_visible(scroll_top, false);
        asset_visible(scroll_middle, false);
        asset_visible(scroll_bottom, false);
    }
}

static void layout_shell(void) {
    tb.panel = (tb_rect){tb.x + TB_PANEL_X, tb.y + TB_PANEL_Y,
                         TB_PANEL_W, TB_PANEL_H};

    asset_rect(&tb.shell[SH_WOOD], tb.x + 3.0f, tb.y + 3.0f,
               TB_W - 6.0f, TB_H - 6.0f, true);
    asset_rect(&tb.shell[SH_BLUE], tb.panel.x, tb.panel.y,
               tb.panel.w, tb.panel.h, true);

    asset_rect(&tb.shell[SH_OUT_TOP], tb.x, tb.y, TB_W, 3.0f, true);
    asset_rect(&tb.shell[SH_OUT_BOTTOM], tb.x, tb.y + TB_H - 3.0f,
               TB_W, 3.0f, true);
    asset_rect(&tb.shell[SH_OUT_LEFT], tb.x, tb.y, 3.0f, TB_H, true);
    asset_rect(&tb.shell[SH_OUT_RIGHT], tb.x + TB_W - 3.0f, tb.y,
               3.0f, TB_H, true);
    asset_rect(&tb.shell[SH_IN_TOP], tb.panel.x, tb.panel.y,
               tb.panel.w, 3.0f, true);
    asset_rect(&tb.shell[SH_IN_BOTTOM], tb.panel.x,
               tb.panel.y + tb.panel.h - 3.0f, tb.panel.w, 3.0f, true);
    asset_rect(&tb.shell[SH_IN_LEFT], tb.panel.x, tb.panel.y,
               3.0f, tb.panel.h, true);
    asset_rect(&tb.shell[SH_IN_RIGHT], tb.panel.x + tb.panel.w - 3.0f,
               tb.panel.y, 3.0f, tb.panel.h, true);

    asset_pos(&tb.shell[SH_LOGO], tb.x + 12.0f, tb.y + 8.0f);
    asset_pos(&tb.shell[SH_MIN], tb.x + TB_W - 56.0f, tb.y + 17.0f);
    tb.buttons[SH_MIN] = (tb_rect){tb.x + TB_W - 56.0f, tb.y + 17.0f,
                                  (float)tb.shell[SH_MIN].w,
                                  (float)tb.shell[SH_MIN].h};
    button_pos(SH_CLOSE, tb.x + TB_W - 31.0f, tb.y + 10.0f);

    const float by = tb.y + TB_H - 31.0f;
    button_pos(SH_CLEAR, tb.x + 12.0f, by);
    button_pos(SH_PLAY, tb.x + 46.0f, by);
    button_pos(SH_REC, tb.x + 82.0f, by);
    button_pos(SH_REWIND, tb.x + 119.0f, by);
    button_pos(SH_BACKGROUND, tb.x + 154.0f, by + 1.0f);
    button_pos(SH_MUTE, tb.x + 187.0f, by);
    button_pos(SH_HELP, tb.x + 219.0f, by);
    button_pos(SH_WEB, tb.x + 251.0f, by);

    for (int i = 0; i < SH_COUNT; i++) {
        if (i != SH_SCROLL_TOP && i != SH_SCROLL_MIDDLE &&
            i != SH_SCROLL_BOTTOM) {
            asset_visible(&tb.shell[i], tb.visible);
        }
    }
    refresh_buttons();
    update_content_height();
    layout_content();
}

static int icon_at(float x, float y) {
    const tb_rect clip = {tb.panel.x + 3.0f, tb.panel.y + 3.0f,
                          tb.panel.w - 24.0f, tb.panel.h - 6.0f};
    if (!inside(clip, x, y)) {
        return -1;
    }
    for (int i = tb.nicons - 1; i >= 0; i--) {
        const tb_icon* icon = &tb.icons[i];
        const tb_rect r = {icon->x, icon->y, (float)icon->asset.w,
                           (float)icon->asset.h};
        if (icon->onscreen && inside(r, x, y)) {
            return i;
        }
    }
    return -1;
}

static void set_hover(int index) {
    if (index == tb.hover) {
        return;
    }
    if (tb.hover >= 0) {
        scene_sprite_set_frame(tb.icons[tb.hover].asset.sprite, 0);
    }
    tb.hover = index;
    if (tb.hover >= 0 && tb.icons[tb.hover].asset.nframes > 1) {
        scene_sprite_set_frame(tb.icons[tb.hover].asset.sprite, 1);
    }
}

static void preview_remove(void) {
    if (tb.preview_sprite >= 0) {
        scene_sprite_remove(tb.preview_sprite);
        tb.preview_sprite = -1;
    }
}

static void preview_create(int index, float x, float y) {
    preview_remove();
    if (index < 0 || index >= tb.nicons) {
        return;
    }
    const tb_asset* a = &tb.icons[index].asset;
    tb.preview_sprite = scene_sprite_add(a->w, a->h, a->nframes, a->frames,
                                         0, x - a->w * 0.5f,
                                         y - a->h * 0.5f, SCENE_GROUP_UI_DRAG);
    if (tb.preview_sprite >= 0) {
        scene_sprite_set_frame(tb.preview_sprite, 0);
        scene_sprite_set_alpha(tb.preview_sprite, 0.85f);
        scene_sprite_set_layer(tb.preview_sprite, SCENE_LAYER_UI_DRAG);
    }
}

static void preview_move(float x, float y) {
    if (tb.preview_sprite >= 0 && tb.pressed >= 0) {
        const tb_asset* a = &tb.icons[tb.pressed].asset;
        scene_sprite_set_position(tb.preview_sprite, x - a->w * 0.5f,
                                  y - a->h * 0.5f);
    }
}

bool toybox_init(const char* assets_root, float view_w, float view_h) {
    if (tb.ready) {
        return true;
    }
    memset(&tb, 0, sizeof(tb));
    tb.hover = -1;
    tb.pressed = -1;
    tb.button_pressed = -1;
    tb.preview_sprite = -1;
    tb.visible = true;
    tb.view_w = view_w;
    tb.view_h = view_h;
    tb.x = 32.0f;
    tb.y = 32.0f;
    snprintf(tb.assets_root, sizeof tb.assets_root, "%s", assets_root);
    for (int i = 0; i < TB_MAX_SHELL; i++) {
        asset_reset(&tb.shell[i]);
    }

    const char* tex = "toybox_toy/resources/graphics/toybox/textures/";
    const char* btn = "toybox_toy/resources/graphics/toybox/buttons/";
    char rel[512], alpha[512];
    snprintf(rel, sizeof rel, "%stiling_wood.flc", tex);
    snprintf(alpha, sizeof alpha, "%stiling_wood_Alpha.flc", tex);
    if (!asset_load_flc(&tb.shell[SH_WOOD], rel, alpha, tb.x, tb.y)) {
        fprintf(stderr, "toybox: missing wood texture\n");
        toybox_shutdown();
        return false;
    }
    snprintf(rel, sizeof rel, "%sbluebackground.flc", tex);
    snprintf(alpha, sizeof alpha, "%sbluebackground_Alpha.flc", tex);
    if (!asset_load_flc(&tb.shell[SH_BLUE], rel, alpha, tb.x, tb.y)) {
        fprintf(stderr, "toybox: missing blue texture\n");
        toybox_shutdown();
        return false;
    }

#define LOAD_SHELL(slot, base) do { \
    snprintf(rel, sizeof rel, "%s%s", tex, base); \
    asset_load_sequence(&tb.shell[slot], rel, 1, tb.x, tb.y); \
} while (0)
    LOAD_SHELL(SH_OUT_TOP, "top_outer");
    LOAD_SHELL(SH_OUT_BOTTOM, "bottom_outer");
    LOAD_SHELL(SH_OUT_LEFT, "left_outer");
    LOAD_SHELL(SH_OUT_RIGHT, "right_outer");
    LOAD_SHELL(SH_IN_TOP, "top_inner");
    LOAD_SHELL(SH_IN_BOTTOM, "bottom_inner");
    LOAD_SHELL(SH_IN_LEFT, "left_inner");
    LOAD_SHELL(SH_IN_RIGHT, "right_inner");
#undef LOAD_SHELL
    snprintf(rel, sizeof rel, "%sscroll", tex);
    asset_load_frame(&tb.shell[SH_SCROLL_TOP], rel, 0, tb.x, tb.y);
    asset_load_frame(&tb.shell[SH_SCROLL_MIDDLE], rel, 1, tb.x, tb.y);
    asset_load_frame(&tb.shell[SH_SCROLL_BOTTOM], rel, 2, tb.x, tb.y);

#define LOAD_BUTTON(slot, base) do { \
    snprintf(rel, sizeof rel, "%s%s", btn, base); \
    asset_load_sequence(&tb.shell[slot], rel, 1, tb.x, tb.y); \
} while (0)
    LOAD_BUTTON(SH_LOGO, "logo");
    LOAD_BUTTON(SH_MIN, "min_button");
#undef LOAD_BUTTON
#define LOAD_BUTTON_STATES(slot, base) do { \
    snprintf(rel, sizeof rel, "%s%s", btn, base); \
    asset_load_sequence(&tb.shell[slot], rel, 6, tb.x, tb.y); \
} while (0)
    LOAD_BUTTON_STATES(SH_BACKGROUND, "background_button");
    LOAD_BUTTON_STATES(SH_CLOSE, "close_button");
    LOAD_BUTTON_STATES(SH_PLAY, "play_button");
    LOAD_BUTTON_STATES(SH_REC, "rec_button");
    LOAD_BUTTON_STATES(SH_REWIND, "rewind_button");
    LOAD_BUTTON_STATES(SH_CLEAR, "clear_button");
    LOAD_BUTTON_STATES(SH_HELP, "help_button");
    LOAD_BUTTON_STATES(SH_WEB, "web_button");
#undef LOAD_BUTTON_STATES
    asset_load_sequence(&tb.shell[SH_MUTE],
                        "toybox_toy/resources/graphics/toybox/bigger mute/"
                        "mute_in_button",
                        6, tb.x, tb.y);
    for (size_t i = 0;
         i < sizeof animated_buttons / sizeof animated_buttons[0];
         i++) {
        const int slot = animated_buttons[i];
        const bool toggle = slot == SH_BACKGROUND || slot == SH_MUTE
                         || slot == SH_HELP;
        const bool on = slot == SH_MUTE && audio_muted();
        toybox_button_init(&tb.button_models[slot], TB_BUTTON_PERIOD_S,
                           toggle, on);
    }

    for (int i = 0; i < rbh_toypack_count() && tb.npacks < TB_MAX_PACKS; i++) {
        const rbh_toypack* def = rbh_toypack_at(i);
        tb_pack* pack = &tb.packs[tb.npacks];
        memset(pack, 0, sizeof(*pack));
        if (!toydefs_pack_header(def->id, def->sprite_path,
                                 pack->header_path,
                                 sizeof pack->header_path)) {
            fprintf(stderr, "toybox: no CToy header owner for pack %s\n",
                    def->id);
            continue;
        }
        pack->def = def;
        asset_reset(&pack->header);
        asset_load_sequence(&pack->header, pack->header_path, 1, tb.x, tb.y);
        tb.npacks++;
    }

    for (int i = 0; i < toydefs_icon_count() && tb.nicons < TB_MAX_ICONS; i++) {
        // Fidelity TODO: the lightweight grid deliberately bypasses
        // IconToy#add_icon/IconGridStackItem. It therefore does not yet apply
        // license enable/alpha state, globalToyInstanceLimit, leftovers, or
        // script-provided icon rewrites; see rbh_catalog_finalize().
        const toyicon_t* def = toydefs_icon_at(i);
        const toydef_t* toy = toydefs_find(def->class_name);
        const int pack = pack_for_id(rbh_toy_pack(def->class_name));
        if (!toy || pack < 0) {
            continue;
        }
        tb_icon* icon = &tb.icons[tb.nicons];
        memset(icon, 0, sizeof(*icon));
        icon->def = def;
        icon->toy = toy;
        icon->pack = pack;
        asset_reset(&icon->asset);
        if (!asset_load_sequence(&icon->asset, def->image, def->num_frames,
                                 tb.x, tb.y)) {
            fprintf(stderr, "toybox: missing icon %s (%s)\n",
                    def->name, def->image);
            asset_destroy(&icon->asset);
            continue;
        }
        tb.packs[pack].icon_count++;
        tb.nicons++;
    }
    sort_icons();

    tb.ready = true;
    layout_shell();
    printf("toybox: %d lightweight icons across %d packs\n",
           tb.nicons, tb.npacks);
    return tb.nicons > 0;
}

void toybox_shutdown(void) {
    preview_remove();
    for (int i = 0; i < tb.nicons; i++) {
        asset_destroy(&tb.icons[i].asset);
    }
    for (int i = 0; i < tb.npacks; i++) {
        asset_destroy(&tb.packs[i].header);
    }
    for (int i = 0; i < TB_MAX_SHELL; i++) {
        asset_destroy(&tb.shell[i]);
    }
    memset(&tb, 0, sizeof(tb));
    tb.hover = -1;
    tb.pressed = -1;
    tb.button_pressed = -1;
    tb.preview_sprite = -1;
}

void toybox_resize(float view_w, float view_h) {
    tb.view_w = view_w;
    tb.view_h = view_h;
    if (!tb.ready) {
        return;
    }
    tb.x = fminf(fmaxf(tb.x, 0.0f), fmaxf(0.0f, view_w - TB_W));
    tb.y = fminf(fmaxf(tb.y, 0.0f), fmaxf(0.0f, view_h - TB_H));
    layout_shell();
}

bool toybox_hit_test(float x, float y) {
    return tb.ready && tb.visible &&
           inside((tb_rect){tb.x, tb.y, TB_W, TB_H}, x, y);
}

void toybox_pointer_move(float x, float y) {
    if (!tb.ready || tb.pressed >= 0 || tb.button_pressed >= 0 ||
        tb.moving || tb.scrolling) {
        return;
    }
    const bool over = toybox_hit_test(x, y);
    set_hover(over ? icon_at(x, y) : -1);
}

bool toybox_mouse_down(float x, float y) {
    if (!toybox_hit_test(x, y)) {
        return false;
    }
    if (tb.max_scroll > 0.0f && inside(tb.scroll_thumb, x, y)) {
        tb.scrolling = true;
        tb.scroll_drag_y = y;
        tb.scroll_drag_target = tb.scroll.target;
        set_hover(-1);
        return true;
    }
    const int button = button_at(x, y);
    if (button >= 0) {
        tb.button_pressed = button;
        set_hover(-1);
        toybox_button_down(&tb.button_models[button], x, y);
        refresh_button(button);
        return true;
    }
    // Until OpenSoup has a persistent Dock/menu-bar reopening affordance,
    // consume the visible minimise button without making the Toybox vanish.
    if (inside(tb.buttons[SH_MIN], x, y)) {
        set_hover(-1);
        return true;
    }
    tb.pressed = icon_at(x, y);
    if (tb.pressed >= 0) {
        set_hover(tb.pressed);
        preview_create(tb.pressed, x, y);
    } else if (y < tb.panel.y) {
        tb.moving = true;
        tb.move_dx = x - tb.x;
        tb.move_dy = y - tb.y;
    }
    return true;
}

void toybox_mouse_dragged(float x, float y) {
    if (!tb.ready) {
        return;
    }
    if (tb.button_pressed >= 0) {
        toybox_button_model* model = &tb.button_models[tb.button_pressed];
        if (model->pressed && button_at(x, y) != tb.button_pressed) {
            toybox_button_cancel(model);
        }
    } else if (tb.scrolling) {
        const float travel = tb.scroll_track.h - tb.scroll_thumb.h;
        toybox_scroll_model_drag(&tb.scroll, tb.scroll_drag_target,
                                 y - tb.scroll_drag_y, travel,
                                 tb.max_scroll);
        set_hover(-1);
        layout_content();
    } else if (tb.pressed >= 0) {
        preview_move(x, y);
    } else if (tb.moving) {
        tb.x = x - tb.move_dx;
        tb.y = y - tb.move_dy;
        tb.x = fminf(fmaxf(tb.x, 0.0f), fmaxf(0.0f, tb.view_w - TB_W));
        tb.y = fminf(fmaxf(tb.y, 0.0f), fmaxf(0.0f, tb.view_h - TB_H));
        layout_shell();
    }
}

void toybox_mouse_up(float x, float y) {
    if (!tb.ready) {
        return;
    }
    if (tb.button_pressed >= 0) {
        const int pressed = tb.button_pressed;
        toybox_button_up(&tb.button_models[pressed], x, y);
        tb.button_pressed = -1;
        return;
    }
    if (tb.pressed >= 0 && !toybox_hit_test(x, y)) {
        const tb_icon* icon = &tb.icons[tb.pressed];
        double wx, wy;
        if (rbh_view_to_scene(x, y, &wx, &wy)) {
            char class_dir[2048];
            container_resource_root(class_dir, sizeof class_dir,
                                    tb.assets_root, icon->toy->root);
            if (rbh_spawn_toy(icon->def->class_name, class_dir, wx, wy)) {
                printf("toybox: dropped %s at %.2f, %.2f\n",
                       icon->def->class_name, wx, wy);
            }
        }
    }
    preview_remove();
    tb.pressed = -1;
    tb.moving = false;
    tb.scrolling = false;
    const bool over = toybox_hit_test(x, y);
    set_hover(over ? icon_at(x, y) : -1);
}

void toybox_scroll(float delta_y, bool precise) {
    if (!tb.ready || tb.max_scroll <= 0.0f) {
        return;
    }
    // Win32's handler changes the normalized model by
    // wheel_delta * IconToy#scroll_increment / WHEEL_DELTA.  A non-precise
    // Cocoa wheel delta is one detent; the Ruby increment is one 0.8m/80px
    // icon row. AppKit supplies precise trackpad deltas (including momentum
    // events) directly in logical points.
    const float delta = precise ? delta_y : delta_y * TB_CELL_H;
    const float target = fminf(fmaxf(tb.scroll.target - delta, 0.0f),
                               tb.max_scroll);
    toybox_scroll_model_set_target(&tb.scroll, target);
    set_hover(-1);
    layout_content();
}

void toybox_frame(double dt_ms) {
    if (tb.ready && toybox_scroll_model_advance(&tb.scroll, dt_ms)) {
        set_hover(-1);
        layout_content();
    }
    if (!tb.ready) {
        return;
    }
    for (size_t i = 0;
         i < sizeof animated_buttons / sizeof animated_buttons[0];
         i++) {
        const int slot = animated_buttons[i];
        toybox_button_model* model = &tb.button_models[slot];
        bool desired_on = false;
        if (toybox_button_advance(model, dt_ms, &desired_on)) {
            const bool old_on = model->on;
            const bool succeeded = activate_button(slot, desired_on);
            if (model->toggle) {
                toybox_button_sync(model,
                    succeeded ? desired_on : old_on);
            }
        }
        refresh_button(slot);
    }
}

bool toybox_quit_requested(void) {
    return tb.ready && tb.quit_requested;
}

bool toybox_capturing(void) {
    return tb.ready && (tb.pressed >= 0 || tb.button_pressed >= 0 ||
                        tb.moving || tb.scrolling);
}

int toybox_catalog_count(void) {
    return tb.nicons;
}
