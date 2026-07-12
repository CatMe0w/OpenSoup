#pragma once

#include <stdbool.h>
#include <stdint.h>

// Original Toybox used a 44.1 kHz, 32-channel FSOUND mixer. OpenSoup keeps
// the same voice limit while miniaudio owns device output and resampling.
#define AUDIO_MAX_VOICES 32

// no_device is for deterministic headless tests. In that mode callers advance
// playback explicitly with audio_render_frames().
bool audio_init(bool no_device);
void audio_shutdown(void);

// Samples are decoded once, kept as signed 16-bit PCM, and cached by path.
// Returns a non-negative cache id, or -1 on failure.
int audio_sample_load(const char* path);

// owner groups all voices started by one Ruby Sound node. A full voice pool
// rejects the new play, matching the original fixed-channel mixer.
bool audio_play(int sample, uint32_t owner, float volume, bool looping);
void audio_stop_owner(uint32_t owner);

int audio_active_voices(void);
bool audio_sample_info(int sample, int* channels, int* sample_rate,
                       uint64_t* frames);
bool audio_render_frames(uint64_t frames);

