#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const unsigned char* data;
    size_t size;
    size_t offset;
    bool ok;
    bool out_of_memory;
    char error[256];
} reader;

static inline void reader_error(reader* r, const char* fmt, ...) {
    if (!r || !r->ok) {
        return;
    }
    r->ok = false;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->error, sizeof r->error, fmt, ap);
    va_end(ap);
}

static inline void reader_oom(reader* r, const char* fmt, ...) {
    if (!r || !r->ok) {
        return;
    }
    r->ok = false;
    r->out_of_memory = true;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->error, sizeof r->error, fmt, ap);
    va_end(ap);
}

static inline bool r_need(reader* r, size_t count) {
    if (!r->ok) {
        return false;
    }
    if (r->offset > r->size || count > r->size - r->offset) {
        reader_error(r, "truncated input at 0x%zx (need %zu bytes)",
                     r->offset, count);
        return false;
    }
    return true;
}

static inline size_t r_remaining(const reader* r) {
    return r && r->offset <= r->size ? r->size - r->offset : 0;
}

static inline uint8_t r_u8(reader* r) {
    if (!r_need(r, 1)) {
        return 0;
    }
    return r->data[r->offset++];
}

static inline uint32_t r_u32(reader* r) {
    if (!r_need(r, 4)) {
        return 0;
    }
    const unsigned char* p = r->data + r->offset;
    r->offset += 4;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline float r_f32(reader* r) {
    const uint32_t bits = r_u32(r);
    float value = 0.0f;
    if (r->ok) {
        memcpy(&value, &bits, sizeof value);
    }
    return value;
}

static inline bool r_skip(reader* r, size_t count) {
    if (!r_need(r, count)) {
        return false;
    }
    r->offset += count;
    return true;
}

static inline const unsigned char* r_lpstr_slice(reader* r,
                                                 size_t* length_out) {
    const uint32_t length = r_u32(r);
    if (!r->ok || !r_need(r, length)) {
        return NULL;
    }
    const unsigned char* result = r->data + r->offset;
    r->offset += length;
    if (length_out) {
        *length_out = length;
    }
    return result;
}

static inline char* r_string(reader* r) {
    size_t length = 0;
    const unsigned char* source = r_lpstr_slice(r, &length);
    if (!source) {
        return NULL;
    }
    if (length > (SIZE_MAX - 1) / 2) {
        reader_error(r, "string at 0x%zx is too large", r->offset - length);
        return NULL;
    }
    char* result = malloc(length * 2 + 1);
    if (!result) {
        reader_oom(r, "out of memory reading string at 0x%zx", r->offset);
        return NULL;
    }
    size_t out = 0;
    for (size_t i = 0; i < length; i++) {
        const unsigned char ch = source[i];
        if (ch < 0x80) {
            result[out++] = (char)ch;
        } else {
            result[out++] = (char)(0xc0 | (ch >> 6));
            result[out++] = (char)(0x80 | (ch & 0x3f));
        }
    }
    result[out] = 0;
    return result;
}

static inline bool r_skip_string(reader* r) {
    return r_lpstr_slice(r, NULL) != NULL;
}
