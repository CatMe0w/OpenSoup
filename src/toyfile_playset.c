#include "toyfile_playset.h"

#include "toyfile_internal.h"
#include "toyfile_reader.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int32_t toy_id;
    const char* class_name;
} legacy_toy_id;

typedef struct {
    int32_t toy_id;
    int32_t limb_id;
    const char* limb_name;
} legacy_limb_id;

#include "legacy_playset_ids.inc"

static void set_error(char* error, size_t error_size, const char* fmt, ...) {
    if (!error || error_size == 0) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error, error_size, fmt, ap);
    va_end(ap);
}

static const char* legacy_toy_name(int32_t toy_id) {
    for (size_t i = 0;
         i < sizeof LEGACY_TOY_IDS / sizeof LEGACY_TOY_IDS[0]; i++) {
        if (LEGACY_TOY_IDS[i].toy_id == toy_id) {
            return LEGACY_TOY_IDS[i].class_name;
        }
    }
    return NULL;
}

static const char* legacy_limb_name(int32_t toy_id, int32_t limb_id) {
    for (size_t i = 0;
         i < sizeof LEGACY_LIMB_IDS / sizeof LEGACY_LIMB_IDS[0]; i++) {
        const legacy_limb_id* item = &LEGACY_LIMB_IDS[i];
        if (item->toy_id == toy_id && item->limb_id == limb_id) {
            return item->limb_name;
        }
    }
    return NULL;
}

static char* duplicate_string(reader* r, const char* source) {
    const size_t size = strlen(source) + 1;
    char* result = malloc(size);
    if (!result) {
        reader_oom(r, "out of memory copying playset identifier");
        return NULL;
    }
    memcpy(result, source, size);
    return result;
}

static bool read_info(reader* r, toyfile_playset_info* info) {
    info->version_description = r_string(r);
    info->file_description = r_string(r);
    info->author = r_string(r);
    info->creation_date = r_string(r);
    if (!r->ok) {
        return false;
    }

    // PlaysetParser discards these legacy metadata slots, but they remain part
    // of every original body version.
    for (size_t i = 0; i < 4; i++) {
        (void)r_u32(r);
    }
    for (size_t i = 0; i < 4; i++) {
        (void)r_skip_string(r);
    }
    return r->ok;
}

static bool skip_properties(reader* r) {
    const uint32_t group_count = r_u32(r);
    for (uint32_t group = 0; r->ok && group < group_count; group++) {
        (void)r_skip_string(r);
        const uint32_t value_count = r_u32(r);
        for (uint32_t i = 0; r->ok && i < value_count; i++) {
            (void)r_skip_string(r);
            const uint32_t type = r_u32(r);
            switch (type) {
                case 0:
                    (void)r_f32(r);
                    break;
                case 1:
                    (void)r_u32(r);
                    break;
                case 2:
                    (void)r_skip_string(r);
                    break;
                default:
                    reader_error(r, "unsupported property type %u", type);
                    break;
            }
        }
    }
    return r->ok;
}

static bool read_optional_float(reader* r, toyfile_optional_float* value) {
    value->present = r_u8(r) != 0;
    if (r->ok && value->present) {
        value->value = r_f32(r);
    }
    return r->ok;
}

static bool read_optional_vec2(reader* r, toyfile_optional_vec2* value) {
    value->present = r_u8(r) != 0;
    if (r->ok && value->present) {
        value->value[0] = r_f32(r);
        value->value[1] = r_f32(r);
    }
    return r->ok;
}

static bool read_world(reader* r, uint32_t version,
                       toyfile_playset_world* world) {
    toyfile_optional_float* fields[] = {
        &world->left_wall,
        &world->right_wall,
        &world->floor,
        &world->ceiling,
        &world->timestep,
        &world->gravity,
    };
    for (size_t i = 0; i < sizeof fields / sizeof fields[0]; i++) {
        if (!read_optional_float(r, fields[i])) {
            return false;
        }
    }

    if (version <= 3) {
        // The seventh legacy optional is a discarded 29-byte record, not the
        // current drop-position vector.
        const bool deprecated_record = r_u8(r) != 0;
        if (!r->ok || (deprecated_record && !r_skip(r, 29))) {
            return false;
        }
        // Version 1 predates drop_position. Versions 2/3 append it as the
        // eighth optional field.
        return version == 1
            || read_optional_vec2(r, &world->drop_position);
    }
    return read_optional_vec2(r, &world->drop_position);
}

static bool skip_sprite(reader* r) {
    (void)r_skip_string(r);
    (void)r_u32(r);
    (void)r_skip_string(r);
    (void)r_f32(r);
    (void)r_f32(r);
    (void)r_u32(r);
    return r->ok;
}

static bool skip_toybox(reader* r, uint32_t version) {
    if (version <= 3) {
        reader_error(r, "legacy toybox payloads are unsupported at 0x%zx",
                     r->offset);
        return false;
    }
    (void)r_skip_string(r);
    (void)r_skip_string(r);
    (void)r_f32(r);
    (void)r_f32(r);
    (void)r_u32(r);
    return skip_sprite(r) && skip_sprite(r);
}

static bool count_fits(reader* r, uint32_t count, size_t minimum_size,
                       const char* what) {
    if (!r->ok) {
        return false;
    }
    if ((size_t)count > r_remaining(r) / minimum_size) {
        reader_error(r, "%s count %u exceeds the body bounds", what, count);
        return false;
    }
    return true;
}

static void* allocate_array(reader* r, uint32_t count, size_t item_size,
                            const char* what) {
    if (count == 0) {
        return NULL;
    }
    if ((size_t)count > SIZE_MAX / item_size) {
        reader_error(r, "%s count %u overflows size_t", what, count);
        return NULL;
    }
    void* result = calloc((size_t)count, item_size);
    if (!result) {
        reader_oom(r, "out of memory creating %s array", what);
    }
    return result;
}

static bool read_limb(reader* r, uint32_t version, int32_t legacy_toy_id,
                      toyfile_playset_limb* limb) {
    if (version <= 3) {
        const int32_t limb_id = (int32_t)r_u32(r);
        const char* name = r->ok
            ? legacy_limb_name(legacy_toy_id, limb_id) : NULL;
        if (r->ok && !name) {
            reader_error(r, "could not resolve legacy limb id (%d, %d)",
                         legacy_toy_id, limb_id);
        }
        if (r->ok) {
            limb->limb_id = duplicate_string(r, name);
        }
    } else {
        limb->limb_id = r_string(r);
    }
    if (!r->ok) {
        return false;
    }

    limb->position[0] = r_f32(r);
    limb->position[1] = r_f32(r);
    limb->orientation = r_f32(r);
    limb->momentum[0] = r_f32(r);
    limb->momentum[1] = r_f32(r);
    limb->angular_momentum = r_f32(r);
    return r->ok;
}

static bool read_toy(reader* r, uint32_t version,
                     toyfile_playset_toy* toy) {
    toy->toy_instance_id = (int32_t)r_u32(r);
    int32_t legacy_toy_id = 0;
    if (version <= 3) {
        legacy_toy_id = (int32_t)r_u32(r);
        const char* name = r->ok ? legacy_toy_name(legacy_toy_id) : NULL;
        if (r->ok && !name) {
            reader_error(r, "could not resolve legacy toy id %d",
                         legacy_toy_id);
        }
        if (r->ok) {
            toy->toy_id = duplicate_string(r, name);
        }
        if (r->ok) {
            toy->extra = duplicate_string(r, "");
        }
    } else {
        toy->toy_id = r_string(r);
        toy->extra = r_string(r);
    }
    if (!r->ok) {
        return false;
    }

    const uint32_t limb_count = r_u32(r);
    if (!count_fits(r, limb_count, 28, "limb state")) {
        return false;
    }
    toy->limb_count = limb_count;
    toy->limbs = allocate_array(r, limb_count, sizeof(*toy->limbs),
                                "limb state");
    if (!r->ok) {
        return false;
    }
    for (size_t i = 0; i < toy->limb_count; i++) {
        if (!read_limb(r, version, legacy_toy_id, &toy->limbs[i])) {
            return false;
        }
    }
    return true;
}

void toyfile_playset_free(toyfile_playset* playset) {
    if (!playset) {
        return;
    }
    free(playset->info.version_description);
    free(playset->info.file_description);
    free(playset->info.author);
    free(playset->info.creation_date);
    for (size_t i = 0; i < playset->toy_count; i++) {
        toyfile_playset_toy* toy = &playset->toys[i];
        free(toy->toy_id);
        free(toy->extra);
        for (size_t j = 0; j < toy->limb_count; j++) {
            free(toy->limbs[j].limb_id);
        }
        free(toy->limbs);
    }
    free(playset->toys);
    free(playset);
}

toyfile_status toyfile_playset_decode(const toyfile* container,
                                      toyfile_playset** out,
                                      char* error, size_t error_size) {
    if (error && error_size) {
        error[0] = 0;
    }
    if (!container || !out) {
        set_error(error, error_size, "missing container or output");
        return TOYFILE_INVALID_ARGUMENT;
    }
    *out = NULL;

    toyfile_body body = {0};
    if (!toyfile_get_body(container, &body)) {
        set_error(error, error_size, "container has no decoded body");
        return TOYFILE_INVALID_ARGUMENT;
    }
    if (toyfile_resource_count(container) != 0) {
        set_error(error, error_size, "container is not a playset");
        return TOYFILE_INVALID_FORMAT;
    }

    toyfile_playset* playset = calloc(1, sizeof(*playset));
    if (!playset) {
        set_error(error, error_size, "out of memory creating playset");
        return TOYFILE_OUT_OF_MEMORY;
    }
    playset->source_version = body.version;
    reader r = {body.data, body.size, 0, true, false, {0}};

    if (!read_info(&r, &playset->info)
        || !skip_properties(&r)) {
        goto fail;
    }
    if (body.version == 4) {
        const uint32_t collision_count = r_u32(&r);
        if (r.ok && collision_count != 0) {
            reader_error(&r, "collision config count %u is unsupported",
                         collision_count);
        }
    }

    playset->has_world = r_u8(&r) != 0;
    if (r.ok && playset->has_world
        && !read_world(&r, body.version, &playset->world)) {
        goto fail;
    }

    const bool toybox_present = r_u8(&r) != 0;
    if (r.ok && toybox_present && !skip_toybox(&r, body.version)) {
        goto fail;
    }

    const uint32_t icon_count = r_u32(&r);
    if (r.ok && icon_count != 0) {
        reader_error(&r, "container is not a playset");
    }
    const uint32_t toy_definition_count = r_u32(&r);
    if (r.ok && toy_definition_count != 0) {
        reader_error(&r, "container is not a playset");
    }

    const uint32_t toy_count = r_u32(&r);
    const size_t minimum_toy_size = body.version <= 3 ? 12 : 16;
    if (!count_fits(&r, toy_count, minimum_toy_size, "toy instance")) {
        goto fail;
    }
    playset->toy_count = toy_count;
    playset->toys = allocate_array(&r, toy_count, sizeof(*playset->toys),
                                   "toy instance");
    if (!r.ok) {
        goto fail;
    }
    for (size_t i = 0; i < playset->toy_count; i++) {
        if (!read_toy(&r, body.version, &playset->toys[i])) {
            goto fail;
        }
    }

    if (body.version > 1) {
        const uint32_t input_map_count = r_u32(&r);
        if (r.ok && input_map_count != 0) {
            reader_error(&r, "input-event map count %u is unsupported",
                         input_map_count);
        }
    }
    if (r.ok && r.offset != r.size) {
        reader_error(&r, "body leaves %zu bytes before resource data",
                     r.size - r.offset);
    }
    if (r.ok && !playset->has_world && playset->toy_count == 0) {
        reader_error(&r, "container is not a playset");
    }
    if (!r.ok) {
        goto fail;
    }

    *out = playset;
    return TOYFILE_OK;

fail:
    set_error(error, error_size, "%s",
              r.error[0] ? r.error : "could not decode playset");
    const toyfile_status status = r.out_of_memory
        ? TOYFILE_OUT_OF_MEMORY : TOYFILE_INVALID_FORMAT;
    toyfile_playset_free(playset);
    return status;
}
