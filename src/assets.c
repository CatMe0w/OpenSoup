#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t* read_file(const char* path, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* buf = malloc((size_t)len);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *out_len = (size_t)len;
    return buf;
}

static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t* p) { return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24)); }

// original premultiply, bit for bit: f = (float)(a/255.0); c' = trunc(c * f)
static void premultiply(uint8_t* rgba, int npixels) {
    for (int i = 0; i < npixels; i++) {
        uint8_t* p = rgba + i * 4;
        const float f = (float)((double)p[3] / 255.0);
        p[0] = (uint8_t)(int)((double)p[0] * (double)f);
        p[1] = (uint8_t)(int)((double)p[1] * (double)f);
        p[2] = (uint8_t)(int)((double)p[2] * (double)f);
    }
}

bool as_load_tga(const char* path, as_image* out) {
    size_t len;
    uint8_t* d = read_file(path, &len);
    if (!d || len < 18) {
        free(d);
        return false;
    }
    const int id_len = d[0];
    const int cmap_type = d[1];
    const int img_type = d[2];
    const int w = rd16(d + 12);
    const int h = rd16(d + 14);
    const int bpp = d[16];
    const int desc = d[17];
    const int top_origin = (desc & 0x20) != 0;
    if (cmap_type != 0 || (img_type != 2 && img_type != 10) || (bpp != 24 && bpp != 32) || w <= 0 || h <= 0) {
        free(d);
        return false;
    }
    const int bytespp = bpp / 8;
    const uint8_t* src = d + 18 + id_len;
    const uint8_t* end = d + len;
    uint8_t* pix = malloc((size_t)w * h * 4);
    if (!pix) {
        free(d);
        return false;
    }

    const int n = w * h;
    int i = 0;
    if (img_type == 2) {
        if (src + (size_t)n * bytespp > end) {
            goto fail;
        }
        for (; i < n; i++, src += bytespp) {
            uint8_t* p = pix + i * 4;
            p[0] = src[2];
            p[1] = src[1];
            p[2] = src[0];
            p[3] = (bytespp == 4) ? src[3] : 255;
        }
    } else { // RLE
        while (i < n) {
            if (src >= end) {
                goto fail;
            }
            const int hdr = *src++;
            const int count = (hdr & 0x7f) + 1;
            if (i + count > n) {
                goto fail;
            }
            if (hdr & 0x80) { // run packet
                if (src + bytespp > end) {
                    goto fail;
                }
                for (int k = 0; k < count; k++, i++) {
                    uint8_t* p = pix + i * 4;
                    p[0] = src[2];
                    p[1] = src[1];
                    p[2] = src[0];
                    p[3] = (bytespp == 4) ? src[3] : 255;
                }
                src += bytespp;
            } else { // literal packet
                if (src + (size_t)count * bytespp > end) {
                    goto fail;
                }
                for (int k = 0; k < count; k++, i++, src += bytespp) {
                    uint8_t* p = pix + i * 4;
                    p[0] = src[2];
                    p[1] = src[1];
                    p[2] = src[0];
                    p[3] = (bytespp == 4) ? src[3] : 255;
                }
            }
        }
    }
    free(d);

    if (!top_origin) { // flip rows to top-left origin
        for (int y = 0; y < h / 2; y++) {
            uint8_t tmp[4096 * 4];
            uint8_t* a = pix + (size_t)y * w * 4;
            uint8_t* b = pix + (size_t)(h - 1 - y) * w * 4;
            memcpy(tmp, a, (size_t)w * 4);
            memcpy(a, b, (size_t)w * 4);
            memcpy(b, tmp, (size_t)w * 4);
        }
    }
    premultiply(pix, n);
    out->w = w;
    out->h = h;
    out->rgba = pix;
    return true;

fail:
    free(d);
    free(pix);
    return false;
}

typedef struct {
    int w, h;
    int frames;
    int speed_ms;
    uint8_t* idx;        // w*h current indexed frame
    uint8_t pal[256][3];
} flc_ctx;

static bool flc_apply_chunk(flc_ctx* c, uint16_t type, const uint8_t* p, const uint8_t* end) {
    const int w = c->w, h = c->h;
    switch (type) {
        case 4:    // COLOR_256
        case 11: { // COLOR_64
            const int shift = (type == 11) ? 2 : 0;
            int npk = rd16(p);
            p += 2;
            int ci = 0;
            for (int k = 0; k < npk && p + 2 <= end; k++) {
                ci += *p++;
                int count = *p++;
                if (count == 0) {
                    count = 256;
                }
                for (int j = 0; j < count && ci < 256 && p + 3 <= end; j++, ci++, p += 3) {
                    c->pal[ci][0] = (uint8_t)(p[0] << shift);
                    c->pal[ci][1] = (uint8_t)(p[1] << shift);
                    c->pal[ci][2] = (uint8_t)(p[2] << shift);
                }
            }
            return true;
        }
        case 13: // BLACK
            memset(c->idx, 0, (size_t)w * h);
            return true;
        case 16: // FLI_COPY
            if (p + (size_t)w * h > end) {
                return false;
            }
            memcpy(c->idx, p, (size_t)w * h);
            return true;
        case 15: { // BYTE_RUN: full frame, run-length per line
            for (int y = 0; y < h; y++) {
                uint8_t* row = c->idx + (size_t)y * w;
                int x = 0;
                p++; // obsolete packet count, ignored: decode until width filled
                while (x < w && p < end) {
                    const int8_t cnt = (int8_t)*p++;
                    if (cnt > 0) { // replicate next byte
                        if (p >= end || x + cnt > w) {
                            return false;
                        }
                        memset(row + x, *p++, (size_t)cnt);
                        x += cnt;
                    } else if (cnt < 0) { // literal copy
                        const int m = -cnt;
                        if (p + m > end || x + m > w) {
                            return false;
                        }
                        memcpy(row + x, p, (size_t)m);
                        p += m;
                        x += m;
                    }
                }
            }
            return true;
        }
        case 12: { // FLI_LC: byte-oriented delta
            if (p + 4 > end) {
                return false;
            }
            int y = rd16(p);
            const int nlines = rd16(p + 2);
            p += 4;
            for (int l = 0; l < nlines && y < h; l++, y++) {
                if (p >= end) {
                    return false;
                }
                int npk = *p++;
                int x = 0;
                while (npk-- > 0 && p + 2 <= end) {
                    x += *p++;
                    const int8_t cnt = (int8_t)*p++;
                    if (cnt >= 0) { // literal
                        if (p + cnt > end || x + cnt > w) {
                            return false;
                        }
                        memcpy(c->idx + (size_t)y * w + x, p, (size_t)cnt);
                        p += cnt;
                        x += cnt;
                    } else { // replicate
                        const int m = -cnt;
                        if (p >= end || x + m > w) {
                            return false;
                        }
                        memset(c->idx + (size_t)y * w + x, *p++, (size_t)m);
                        x += m;
                    }
                }
            }
            return true;
        }
        case 7: { // DELTA_FLC (SS2): word-oriented delta
            if (p + 2 > end) {
                return false;
            }
            int nlines = rd16(p);
            p += 2;
            int y = 0;
            while (nlines > 0 && p + 2 <= end) {
                const uint16_t word = rd16(p);
                p += 2;
                if ((word & 0xC000) == 0xC000) { // line skip
                    y += -(int16_t)word;
                } else if ((word & 0xC000) == 0x8000) { // set last pixel
                    if (y < h) {
                        c->idx[(size_t)y * w + w - 1] = (uint8_t)(word & 0xff);
                    }
                } else { // packet count for this line
                    int npk = word;
                    int x = 0;
                    while (npk-- > 0 && p + 2 <= end && y < h) {
                        x += *p++;
                        const int8_t cnt = (int8_t)*p++;
                        uint8_t* row = c->idx + (size_t)y * w;
                        if (cnt >= 0) { // literal words
                            if (p + 2 * cnt > end || x + 2 * cnt > w) {
                                return false;
                            }
                            memcpy(row + x, p, (size_t)cnt * 2);
                            p += 2 * cnt;
                            x += 2 * cnt;
                        } else { // replicate word
                            const int m = -cnt;
                            if (p + 2 > end || x + 2 * m > w) {
                                return false;
                            }
                            for (int j = 0; j < m; j++) {
                                row[x++] = p[0];
                                row[x++] = p[1];
                            }
                            p += 2;
                        }
                    }
                    y++;
                    nlines--;
                }
            }
            return true;
        }
        case 18: // PSTAMP
        default: // unknown chunks are skippable by spec
            return true;
    }
}

// decode all frames to 8-bit index buffers; caller owns the result
static bool flc_decode(const char* path, flc_ctx* c, uint8_t*** out_idx_frames, uint8_t (**out_pal_frames)[256][3]) {
    size_t len;
    uint8_t* d = read_file(path, &len);
    if (!d || len < 128 || rd16(d + 4) != 0xAF12) {
        free(d);
        return false;
    }
    c->frames = rd16(d + 6);
    c->w = rd16(d + 8);
    c->h = rd16(d + 10);
    c->speed_ms = (int)rd32(d + 16);
    if (c->w <= 0 || c->h <= 0 || c->frames <= 0 || c->frames > 4096) {
        free(d);
        return false;
    }
    c->idx = calloc(1, (size_t)c->w * c->h);
    memset(c->pal, 0, sizeof(c->pal));

    uint8_t** idx_frames = calloc((size_t)c->frames, sizeof(uint8_t*));
    uint8_t(*pal_frames)[256][3] = calloc((size_t)c->frames, sizeof(*pal_frames));

    const uint8_t* p = d + 128;
    const uint8_t* end = d + len;
    int fi = 0;
    while (fi < c->frames && p + 6 <= end) {
        const uint32_t csize = rd32(p);
        const uint16_t ctype = rd16(p + 4);
        if (csize < 6 || p + csize > end) {
            break;
        }
        if (ctype == 0xF1FA) { // frame
            int nsub = rd16(p + 6);
            const uint8_t* sp = p + 16;
            const uint8_t* fend = p + csize;
            while (nsub-- > 0 && sp + 6 <= fend) {
                const uint32_t ssize = rd32(sp);
                const uint16_t stype = rd16(sp + 4);
                if (ssize < 6 || sp + ssize > fend) {
                    break;
                }
                // limit = frame end, not sub-chunk end: some writers (QDFL10)
                // understate sub-chunk sizes by a couple of bytes
                if (!flc_apply_chunk(c, stype, sp + 6, fend)) {
                    goto fail;
                }
                sp += ssize;
            }
            idx_frames[fi] = malloc((size_t)c->w * c->h);
            memcpy(idx_frames[fi], c->idx, (size_t)c->w * c->h);
            memcpy(pal_frames[fi], c->pal, sizeof(c->pal));
            fi++;
        }
        p += csize;
    }
    free(d);
    if (fi != c->frames) {
        goto fail_nofree;
    }
    *out_idx_frames = idx_frames;
    *out_pal_frames = pal_frames;
    return true;

fail:
    free(d);
fail_nofree:
    for (int i = 0; i < c->frames; i++) {
        free(idx_frames[i]);
    }
    free(idx_frames);
    free(pal_frames);
    free(c->idx);
    c->idx = NULL;
    return false;
}

bool as_load_flc(const char* color_path, const char* alpha_path, as_anim* out) {
    flc_ctx cc = {0}, ac = {0};
    uint8_t** cidx = NULL;
    uint8_t(*cpal)[256][3] = NULL;
    uint8_t** aidx = NULL;
    uint8_t(*apal)[256][3] = NULL;

    if (!flc_decode(color_path, &cc, &cidx, &cpal)) {
        return false;
    }
    if (alpha_path) {
        if (!flc_decode(alpha_path, &ac, &aidx, &apal) || ac.w != cc.w || ac.h != cc.h || ac.frames != cc.frames) {
            // treat a mismatched alpha companion as fatal: silent opacity would be a fidelity bug
            goto fail;
        }
    }

    const int n = cc.w * cc.h;
    uint8_t** frames = calloc((size_t)cc.frames, sizeof(uint8_t*));
    for (int f = 0; f < cc.frames; f++) {
        frames[f] = malloc((size_t)n * 4);
        for (int i = 0; i < n; i++) {
            const uint8_t ci = cidx[f][i];
            uint8_t* p = frames[f] + i * 4;
            p[0] = cpal[f][ci][0];
            p[1] = cpal[f][ci][1];
            p[2] = cpal[f][ci][2];
            p[3] = aidx ? apal[f][aidx[f][i]][0] : 255; // gray level = alpha
        }
        premultiply(frames[f], n);
    }

    out->w = cc.w;
    out->h = cc.h;
    out->frame_count = cc.frames;
    out->speed_ms = cc.speed_ms > 0 ? cc.speed_ms : 70;
    out->frames = frames;

    for (int i = 0; i < cc.frames; i++) {
        free(cidx[i]);
        if (aidx) {
            free(aidx[i]);
        }
    }
    free(cidx);
    free(cpal);
    free(aidx);
    free(apal);
    free(cc.idx);
    free(ac.idx);
    return true;

fail:
    for (int i = 0; i < cc.frames; i++) {
        free(cidx[i]);
    }
    free(cidx);
    free(cpal);
    if (aidx) {
        for (int i = 0; i < ac.frames; i++) {
            free(aidx[i]);
        }
        free(aidx);
        free(apal);
    }
    free(cc.idx);
    free(ac.idx);
    return false;
}

void as_image_free(as_image* img) {
    free(img->rgba);
    img->rgba = NULL;
}

void as_anim_free(as_anim* anim) {
    for (int i = 0; i < anim->frame_count; i++) {
        free(anim->frames[i]);
    }
    free(anim->frames);
    anim->frames = NULL;
}
