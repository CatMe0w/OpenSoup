#pragma once
// Internals shared by the rubyhost_*.c translation units. Include this AFTER
// the system headers: it pulls in Ruby 1.8's ruby.h, whose macros and
// typedefs clash with anything included later.
#include <stdbool.h>
#include <stdint.h>
#include "toydefs.h"
#include "ruby.h"

// node model

// One tagged struct backs every engine object. colls[] carries the child
// collections (toy: toys/limbs/joints/rotational_joints/sprites/sounds;
// limb: sprites/shapes/linear_motors/rotational_motors; engine: toys).

enum {
    SN_GENERIC, SN_COLL, SN_TOY, SN_LIMB, SN_ENGINE, SN_CORE, SN_SOUP,
    SN_SOUND
};
#define SN_NCOLLS 6

typedef struct {
    int kind;
    VALUE sid, parent, engine;
    VALUE items;            // SN_COLL
    VALUE colls[SN_NCOLLS];
    VALUE ref1, ref2;       // joint: limb1/limb2
    const toydef_t* def;    // SN_TOY
    int instance_id;        // SN_TOY
    int sticky;             // SN_TOY
    int realized;           // SN_TOY: phys bodies/joints exist
    const td_limb* ldef;    // SN_LIMB
    const td_sprite* spdef; // sprite node
    const td_shape* shdef;  // shape node
    const td_sound* snddef; // sound node
    int shape_index;        // index in the owning limb/physics body
    const td_joint* jdef;   // joint node
    int body;               // SN_LIMB: phys body index, -1 = not realized
    double px, py, orient, shock;
    double mx, my, mL;      // SN_LIMB: momentum, pre-realization only
    double lscale;          // SN_LIMB: owning toy's base_scale (unit conv)
    VALUE inputs;           // SN_ENGINE
    VALUE timers;           // SN_ENGINE: [[proc, period_ticks, next_tick]...]
    VALUE overlaps;         // Shape: active [[other_shape, group], ...]
    VALUE sound_path;       // Sound: pack-relative Ogg path
    uint32_t sound_owner;   // Sound: groups its active mixer voices
    int audio_sample;       // Sound: lazily populated audio cache id
    double sound_period, sound_window_start;
    int sound_max, sound_window_count, sound_window_valid;
    double timer_tick;      // SN_ENGINE: dispatch_timers position, 1/100 s
    double scene_bl[2], scene_tr[2], canvas_tl[2], canvas_br[2];
    double scale, gravity, timestep, timescale, time;
    int paused;
    VALUE engines;          // SN_CORE
    VALUE license_policy, load_paths; // SN_SOUP
} sn_t;

// rubyhost.c: class registry, resource root, protected calls into Ruby
VALUE cls_find(const char* name);
extern char g_root[1024]; // resource root (extracted souptoys_core.toy)
bool fcall_protected(VALUE recv, const char* name, int argc,
                     VALUE a0, VALUE a1, VALUE a2, const char* what);

// rubyhost_node.c: sn_t plumbing, Vector bridge, allocators, node methods
int sn_p(VALUE v);
sn_t* sn_get(VALUE v);
VALUE alloc_generic(VALUE klass);
VALUE vec_new(double x, double y);
void vec_get(VALUE v, double* x, double* y);
VALUE core_add_engine(VALUE self, VALUE e);
void rbh_register_nodes(void);

// rubyhost_realize.c: native resource lifecycle + scene-sprite map
bool sn_set_engine(VALUE nodev, VALUE eng);
VALUE sn_set_engine_protected(VALUE args);
void toy_unrealize(VALUE toyv);
void rbh_sprite_map_free(void);

// rubyhost_limb.c: Limb/Shape methods + trigger dispatch
void dispatch_trigger_transitions(VALUE engine);
void rbh_register_limb(void);

// rubyhost_engine.c: REngine/RInput methods
void rbh_register_engine(void);

// rubyhost_sound.c: Sound methods
void rbh_register_sound(void);
