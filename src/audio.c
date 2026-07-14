#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "stb_vorbis.c"

#define AUDIO_MAX_SAMPLES 256

typedef struct {
    char* path;
    short* pcm;
    uint64_t frames;
    int channels;
    int sample_rate;
} audio_sample_t;

typedef struct {
    bool used;
    uint32_t owner;
    ma_audio_buffer_ref buffer;
    ma_sound sound;
} audio_voice_t;

static struct {
    bool initialized;
    bool no_device;
    bool muted;
    ma_engine engine;
    audio_sample_t samples[AUDIO_MAX_SAMPLES];
    int nsamples;
    audio_voice_t voices[AUDIO_MAX_VOICES];
} g_audio;

static void voice_release(audio_voice_t* voice) {
    if (!voice->used) {
        return;
    }
    ma_sound_stop(&voice->sound);
    ma_sound_uninit(&voice->sound);
    memset(voice, 0, sizeof(*voice));
}

static void voices_reap(void) {
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        audio_voice_t* voice = &g_audio.voices[i];
        if (voice->used && ma_sound_at_end(&voice->sound)) {
            voice_release(voice);
        }
    }
}

bool audio_init(bool no_device) {
    if (g_audio.initialized) {
        return true;
    }

    ma_engine_config config = ma_engine_config_init();
    config.noDevice = no_device ? MA_TRUE : MA_FALSE;
    config.channels = 2;
    config.sampleRate = 44100;
    const ma_result result = ma_engine_init(&config, &g_audio.engine);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "[audio] miniaudio init failed: %s\n",
                ma_result_description(result));
        return false;
    }
    g_audio.initialized = true;
    g_audio.no_device = no_device;
    return true;
}

void audio_shutdown(void) {
    if (!g_audio.initialized) {
        return;
    }
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        voice_release(&g_audio.voices[i]);
    }
    for (int i = 0; i < g_audio.nsamples; i++) {
        free(g_audio.samples[i].path);
        free(g_audio.samples[i].pcm);
    }
    ma_engine_uninit(&g_audio.engine);
    memset(&g_audio, 0, sizeof(g_audio));
}

int audio_sample_load(const char* path) {
    if (!g_audio.initialized || !path || !path[0]) {
        return -1;
    }
    for (int i = 0; i < g_audio.nsamples; i++) {
        if (strcmp(g_audio.samples[i].path, path) == 0) {
            return i;
        }
    }
    if (g_audio.nsamples >= AUDIO_MAX_SAMPLES) {
        fprintf(stderr, "[audio] sample cache full: %s\n", path);
        return -1;
    }

    int channels = 0;
    int sample_rate = 0;
    short* pcm = NULL;
    const int frames = stb_vorbis_decode_filename(path, &channels,
                                                   &sample_rate, &pcm);
    if (frames <= 0 || channels <= 0 || sample_rate <= 0 || !pcm) {
        free(pcm);
        fprintf(stderr, "[audio] cannot decode Ogg: %s\n", path);
        return -1;
    }

    audio_sample_t* sample = &g_audio.samples[g_audio.nsamples];
    sample->path = strdup(path);
    if (!sample->path) {
        free(pcm);
        return -1;
    }
    sample->pcm = pcm;
    sample->frames = (uint64_t)frames;
    sample->channels = channels;
    sample->sample_rate = sample_rate;
    return g_audio.nsamples++;
}

bool audio_play(int sample_id, uint32_t owner, float volume, bool looping) {
    if (!g_audio.initialized || sample_id < 0 ||
        sample_id >= g_audio.nsamples) {
        return false;
    }
    voices_reap();

    audio_voice_t* voice = NULL;
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        if (!g_audio.voices[i].used) {
            voice = &g_audio.voices[i];
            break;
        }
    }
    if (!voice) {
        return false;
    }

    audio_sample_t* sample = &g_audio.samples[sample_id];
    ma_result result = ma_audio_buffer_ref_init(
        ma_format_s16, (ma_uint32)sample->channels, sample->pcm,
        (ma_uint64)sample->frames, &voice->buffer);
    if (result != MA_SUCCESS) {
        return false;
    }
    // miniaudio 0.11 leaves this at zero for buffer refs; the data source's
    // native rate is required for the engine's resampler.
    voice->buffer.sampleRate = (ma_uint32)sample->sample_rate;

    result = ma_sound_init_from_data_source(
        &g_audio.engine, (ma_data_source*)&voice->buffer,
        MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, &voice->sound);
    if (result != MA_SUCCESS) {
        memset(voice, 0, sizeof(*voice));
        return false;
    }
    voice->used = true;
    voice->owner = owner;
    ma_sound_set_volume(&voice->sound, volume < 0.0f ? 0.0f : volume);
    ma_sound_set_looping(&voice->sound, looping ? MA_TRUE : MA_FALSE);
    result = ma_sound_start(&voice->sound);
    if (result != MA_SUCCESS) {
        voice_release(voice);
        return false;
    }
    return true;
}

void audio_stop_owner(uint32_t owner) {
    if (!g_audio.initialized) {
        return;
    }
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        if (g_audio.voices[i].used && g_audio.voices[i].owner == owner) {
            voice_release(&g_audio.voices[i]);
        }
    }
}

bool audio_set_muted(bool muted) {
    if (!g_audio.initialized) {
        return false;
    }
    if (ma_engine_set_volume(&g_audio.engine, muted ? 0.0f : 1.0f)
        != MA_SUCCESS) {
        return false;
    }
    g_audio.muted = muted;
    return true;
}

bool audio_muted(void) {
    return g_audio.initialized && g_audio.muted;
}

int audio_active_voices(void) {
    if (!g_audio.initialized) {
        return 0;
    }
    voices_reap();
    int count = 0;
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        count += g_audio.voices[i].used ? 1 : 0;
    }
    return count;
}

bool audio_sample_info(int sample_id, int* channels, int* sample_rate,
                       uint64_t* frames) {
    if (sample_id < 0 || sample_id >= g_audio.nsamples) {
        return false;
    }
    const audio_sample_t* sample = &g_audio.samples[sample_id];
    if (channels) *channels = sample->channels;
    if (sample_rate) *sample_rate = sample->sample_rate;
    if (frames) *frames = sample->frames;
    return true;
}

bool audio_render_frames(uint64_t frames) {
    if (!g_audio.initialized || !g_audio.no_device) {
        return false;
    }
    float output[512 * 2];
    while (frames > 0) {
        const ma_uint64 chunk = frames > 512 ? 512 : (ma_uint64)frames;
        const ma_result result = ma_engine_read_pcm_frames(
            &g_audio.engine, output, chunk, NULL);
        if (result != MA_SUCCESS) {
            return false;
        }
        frames -= chunk;
    }
    voices_reap();
    return true;
}
