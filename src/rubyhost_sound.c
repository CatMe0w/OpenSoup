// Sound: def-backed Ogg voices through the audio mixer, with the original
// period/max-per-period throttling.
#include "audio.h"
#include <stdio.h>
#include <string.h>
#include "rubyhost_internal.h"

static VALUE sound_toy(VALUE self) {
    VALUE sounds = sn_get(self)->parent;
    return NIL_P(sounds) ? Qnil : sn_get(sounds)->parent;
}

static bool sound_resolve_path(VALUE self, char* out, size_t cap) {
    sn_t* sound = sn_get(self);
    if (NIL_P(sound->sound_path)) {
        return false;
    }
    const char* path = StringValueCStr(sound->sound_path);
    if (path[0] == '/') {
        return (size_t)snprintf(out, cap, "%s", path) < cap;
    }

    VALUE toyv = sound_toy(self);
    if (NIL_P(toyv) || !sn_get(toyv)->def || !sn_get(toyv)->def->root) {
        return false;
    }
    char assets[sizeof g_root];
    snprintf(assets, sizeof assets, "%s", g_root);
    char* slash = strrchr(assets, '/');
    if (slash) {
        *slash = 0;
    } else {
        snprintf(assets, sizeof assets, ".");
    }

    // Def locations were relative to <pack>/defs/. The extracted layout
    // omits that synthetic defs directory, so ../sound/... maps directly to
    // <assets>/<pack>/sound/....
    while (strncmp(path, "../", 3) == 0) {
        path += 3;
    }
    return (size_t)snprintf(out, cap, "%s/%s/%s", assets,
                            sn_get(toyv)->def->root, path) < cap;
}

static VALUE sound_path(VALUE self) {
    return sn_get(self)->sound_path;
}

static VALUE sound_path_set(VALUE self, VALUE path) {
    StringValue(path);
    sn_t* sound = sn_get(self);
    audio_stop_owner(sound->sound_owner);
    sound->sound_path = path;
    sound->audio_sample = -1;
    sound->sound_window_valid = 0;
    return path;
}

static double sound_engine_time(const sn_t* sound) {
    return NIL_P(sound->engine) ? 0.0 : sn_get(sound->engine)->time;
}

static bool sound_period_allows(sn_t* sound) {
    if (sound->sound_period <= 0.0 || sound->sound_max <= 0) {
        return true;
    }
    const double now = sound_engine_time(sound);
    if (!sound->sound_window_valid || now < sound->sound_window_start ||
        now - sound->sound_window_start >= sound->sound_period) {
        sound->sound_window_valid = 1;
        sound->sound_window_start = now;
        sound->sound_window_count = 0;
    }
    return sound->sound_window_count < sound->sound_max;
}

static bool sound_start(VALUE self, VALUE volume, bool looping) {
    sn_t* sound = sn_get(self);
    if (!sound_period_allows(sound)) {
        return false;
    }
    if (sound->audio_sample < 0) {
        char path[2048];
        if (!sound_resolve_path(self, path, sizeof path)) {
            fprintf(stderr, "[audio] cannot resolve Sound path\n");
            return false;
        }
        sound->audio_sample = audio_sample_load(path);
    }
    if (sound->audio_sample < 0) {
        return false;
    }
    if (!audio_play(sound->audio_sample, sound->sound_owner,
                    (float)NUM2DBL(volume), looping)) {
        return false;
    }
    if (sound->sound_period > 0.0 && sound->sound_max > 0) {
        sound->sound_window_count++;
    }
    return true;
}

static VALUE sound_play(VALUE self, VALUE volume) {
    sound_start(self, volume, false);
    return Qnil;
}

// The argument is volume, as used by the shipped F10 script (`tune.loop(1)`),
// not a boolean looping toggle.
static VALUE sound_loop(VALUE self, VALUE volume) {
    sound_start(self, volume, true);
    return Qnil;
}

static VALUE sound_stop(VALUE self) {
    audio_stop_owner(sn_get(self)->sound_owner);
    return Qnil;
}

static VALUE sound_period_length(VALUE self) {
    return rb_float_new(sn_get(self)->sound_period);
}

static VALUE sound_period_length_set(VALUE self, VALUE period) {
    sn_t* sound = sn_get(self);
    const double value = NUM2DBL(period);
    sound->sound_period = value > 0.0 ? value : 0.0;
    sound->sound_window_valid = 0;
    return period;
}

static VALUE sound_max_per_period(VALUE self) {
    return INT2NUM(sn_get(self)->sound_max);
}

static VALUE sound_max_per_period_set(VALUE self, VALUE maximum) {
    sn_t* sound = sn_get(self);
    const int value = NUM2INT(maximum);
    sound->sound_max = value > 0 ? value : 0;
    sound->sound_window_valid = 0;
    return maximum;
}

void rbh_register_sound(void) {
    VALUE c = cls_find("Sound");
    rb_define_method(c, "loop", sound_loop, 1);
    rb_define_method(c, "max_sounds_per_period", sound_max_per_period, 0);
    rb_define_method(c, "max_sounds_per_period=", sound_max_per_period_set, 1);
    rb_define_method(c, "path", sound_path, 0);
    rb_define_method(c, "path=", sound_path_set, 1);
    rb_define_method(c, "period_length", sound_period_length, 0);
    rb_define_method(c, "period_length=", sound_period_length_set, 1);
    rb_define_method(c, "play", sound_play, 1);
    rb_define_method(c, "stop", sound_stop, 0);
}
