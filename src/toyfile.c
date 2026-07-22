#include "toyfile.h"

#include "cJSON.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOYFILE_MAGIC "SOUPTOYS.COM TOY FORMAT\0"
#define TOYFILE_MAGIC_SIZE 24
#define TOYFILE_V4_MD5_STATE_SIZE 96
#define TOYFILE_V4_FOOTER_SIZE (4 + TOYFILE_V4_MD5_STATE_SIZE)

typedef struct {
    const char* name;
    size_t name_size;
    const char* extension;
    size_t extension_size;
    size_t data_offset;
    size_t data_size;
} toyfile_resource;

struct toyfile {
    unsigned char* data;
    size_t size;
    size_t magic_offset;
    size_t resource_data_offset;
    toyfile_resource* resources;
    size_t resource_count;
    char* manifest_json;
    bool out_of_memory;
    char error[320];
};

typedef struct {
    const unsigned char* data;
    size_t size;
    size_t offset;
    bool ok;
    bool out_of_memory;
    char error[256];
} reader;

static bool raw_u32(const unsigned char* data, size_t size, size_t offset,
                    uint32_t* value);

static void file_error(toyfile* file, const char* fmt, ...) {
    if (!file || file->error[0]) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(file->error, sizeof file->error, fmt, ap);
    va_end(ap);
}

static void reader_error(reader* r, const char* fmt, ...) {
    if (!r || !r->ok) {
        return;
    }
    r->ok = false;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(r->error, sizeof r->error, fmt, ap);
    va_end(ap);
}

static void reader_oom(reader* r, const char* fmt, ...) {
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

static bool r_need(reader* r, size_t count) {
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

static uint8_t r_u8(reader* r) {
    if (!r_need(r, 1)) {
        return 0;
    }
    return r->data[r->offset++];
}

static uint32_t r_u32(reader* r) {
    if (!r_need(r, 4)) {
        return 0;
    }
    const unsigned char* p = r->data + r->offset;
    r->offset += 4;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static float r_f32(reader* r) {
    const uint32_t bits = r_u32(r);
    float value = 0.0f;
    if (r->ok) {
        memcpy(&value, &bits, sizeof value);
    }
    return value;
}

// Consume a fixed-width field observed to be zero-filled in every original V4
// container examined. Naming the field in the error keeps these otherwise
// discarded bytes visible in the parser, which also records the inferred
// binary structure.
static bool r_zeroes(reader* r, size_t count, const char* field) {
    if (!r_need(r, count)) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (r->data[r->offset + i] != 0) {
            reader_error(r, "%s contains non-zero byte at 0x%zx", field,
                         r->offset + i);
            return false;
        }
    }
    r->offset += count;
    return true;
}

// The original strings are length-prefixed Latin-1. The shipped corpus is
// ASCII, but converting the upper half keeps the JSON valid for other files.
static const unsigned char* r_lpstr_slice(reader* r, size_t* length_out) {
    const uint32_t length = r_u32(r);
    if (!r->ok) {
        return NULL;
    }
    if (!r_need(r, length)) {
        return NULL;
    }
    const unsigned char* result = r->data + r->offset;
    r->offset += length;
    if (length_out) {
        *length_out = length;
    }
    return result;
}

static char* r_string(reader* r) {
    size_t length = 0;
    const unsigned char* source = r_lpstr_slice(r, &length);
    if (!source) {
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

static bool r_skip_string(reader* r) {
    return r_lpstr_slice(r, NULL) != NULL;
}

// Object keys passed here must have static storage. Dynamic property names use
// cJSON_AddItemToObject directly so cJSON owns a copy of the key.
static bool json_add(cJSON* parent, const char* key, cJSON* item,
                     reader* r) {
    if (!item) {
        reader_oom(r, "out of memory creating JSON at 0x%zx", r->offset);
        return false;
    }
    if (key) {
        (void)cJSON_AddItemToObjectCS(parent, key, item);
    } else {
        (void)cJSON_AddItemToArray(parent, item);
    }
    return true;
}

static bool json_string(cJSON* object, const char* key, reader* r) {
    char* value = r_string(r);
    if (!r->ok) {
        free(value);
        return false;
    }
    cJSON* item = cJSON_CreateString(value);
    free(value);
    return json_add(object, key, item, r);
}

static bool json_number(cJSON* object, const char* key, double value,
                        reader* r) {
    return json_add(object, key, cJSON_CreateNumber(value), r);
}

static bool json_bool(cJSON* object, const char* key, bool value, reader* r) {
    return json_add(object, key, cJSON_CreateBool(value), r);
}

static bool json_f32(cJSON* object, const char* key, reader* r) {
    const float value = r_f32(r);
    return r->ok && json_number(object, key, value, r);
}

static bool json_u32(cJSON* object, const char* key, reader* r) {
    const uint32_t value = r_u32(r);
    return r->ok && json_number(object, key, value, r);
}

static bool json_i32(cJSON* object, const char* key, reader* r) {
    const int32_t value = (int32_t)r_u32(r);
    return r->ok && json_number(object, key, value, r);
}

static bool json_bool8(cJSON* object, const char* key, reader* r) {
    const bool value = r_u8(r) != 0;
    return r->ok && json_bool(object, key, value, r);
}

static bool json_optional_f32(cJSON* object, const char* key, reader* r) {
    const bool present = r_u8(r) != 0;
    if (!r->ok) {
        return false;
    }
    return present ? json_f32(object, key, r)
                   : json_add(object, key, cJSON_CreateNull(), r);
}

static cJSON* parse_vec2(reader* r) {
    const float x = r_f32(r);
    const float y = r_f32(r);
    if (!r->ok) {
        return NULL;
    }
    cJSON* array = cJSON_CreateArray();
    if (!array || !json_add(array, NULL, cJSON_CreateNumber(x), r)
               || !json_add(array, NULL, cJSON_CreateNumber(y), r)) {
        cJSON_Delete(array);
        return NULL;
    }
    return array;
}

static bool json_vec2(cJSON* object, const char* key, reader* r) {
    return json_add(object, key, parse_vec2(r), r);
}

typedef cJSON* (*parse_item_fn)(reader* r);

static cJSON* parse_counted(reader* r, const char* what,
                            parse_item_fn parse_item) {
    const uint32_t count = r_u32(r);
    if (!r->ok) {
        return NULL;
    }
    cJSON* array = cJSON_CreateArray();
    if (!array) {
        reader_oom(r, "out of memory creating %s array", what);
        return NULL;
    }
    for (uint32_t i = 0; i < count; i++) {
        if (!json_add(array, NULL, parse_item(r), r)) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

static bool json_counted(cJSON* object, const char* key, reader* r,
                         const char* what, parse_item_fn parse_item) {
    return json_add(object, key, parse_counted(r, what, parse_item), r);
}

static cJSON* parse_sprite(reader* r) {
    cJSON* sprite = cJSON_CreateObject();
    if (!sprite || !json_string(sprite, "id", r)) {
        goto fail;
    }
    const uint32_t frames = r_u32(r);
    if (!r->ok || !json_number(sprite, "numFrames", frames, r)
               || !json_string(sprite, "imageLocation", r)
               || !json_vec2(sprite, "objectCentreOfMass", r)
               || !json_i32(sprite, "zOrder", r)) {
        goto fail;
    }
    return sprite;

fail:
    cJSON_Delete(sprite);
    return NULL;
}

static cJSON* parse_sound_ref(reader* r) {
    cJSON* sound = cJSON_CreateObject();
    if (!sound || !json_string(sound, "id", r)
               || !json_string(sound, "description", r)
               || !json_string(sound, "location", r)) {
        cJSON_Delete(sound);
        return NULL;
    }
    return sound;
}

static cJSON* parse_string64(reader* r) {
    char* value = r_string(r);
    cJSON* item = value ? cJSON_CreateString(value) : NULL;
    free(value);
    return item;
}

static cJSON* parse_vertex(reader* r) {
    cJSON* vertex = cJSON_CreateObject();
    if (!vertex || !json_vec2(vertex, "position", r)
                || !json_f32(vertex, "radius", r)) {
        cJSON_Delete(vertex);
        return NULL;
    }
    return vertex;
}

static cJSON* parse_shape(reader* r) {
    cJSON* shape = cJSON_CreateObject();
    if (!shape || !json_string(shape, "id", r)
               || !json_string(shape, "description", r)
               || !json_counted(shape, "memberOf", r, "memberOf",
                                parse_string64)
               || !json_bool8(shape, "grab", r)
               || !json_bool8(shape, "grabMove", r)
               || !json_bool8(shape, "grabRotate", r)) {
        cJSON_Delete(shape);
        return NULL;
    }
    cJSON* geometry = cJSON_CreateObject();
    if (!geometry
        || !json_counted(geometry, "vertex", r, "vertex", parse_vertex)) {
        cJSON_Delete(geometry);
        cJSON_Delete(shape);
        return NULL;
    }
    if (!json_add(shape, "shape", geometry, r)) {
        cJSON_Delete(shape);
        return NULL;
    }
    return shape;
}

static cJSON* parse_magnet_producer(reader* r) {
    cJSON* item = cJSON_CreateObject();
    if (!item || !json_string(item, "description", r)
              || !json_string(item, "magnetGroup", r)
              || !json_vec2(item, "attachPoint", r)
              || !json_bool8(item, "biDirectional", r)
              || !json_bool8(item, "inverted", r)
              || !json_bool8(item, "springResponse", r)
              || !json_f32(item, "stiffness", r)
              || !json_f32(item, "dampener", r)
              || !json_f32(item, "radius", r)) {
        cJSON_Delete(item);
        return NULL;
    }
    return item;
}

static cJSON* parse_magnet_consumer(reader* r) {
    cJSON* item = cJSON_CreateObject();
    if (!item || !json_string(item, "description", r)
              || !json_string(item, "magnetGroup", r)
              || !json_vec2(item, "attachPoint", r)) {
        cJSON_Delete(item);
        return NULL;
    }
    return item;
}

static cJSON* parse_linear_motor(reader* r) {
    cJSON* item = cJSON_CreateObject();
    if (!item || !json_string(item, "id", r)
              || !json_vec2(item, "force", r)) {
        cJSON_Delete(item);
        return NULL;
    }
    return item;
}

static cJSON* parse_rotational_motor(reader* r) {
    cJSON* item = cJSON_CreateObject();
    if (!item || !json_string(item, "id", r)
              || !json_f32(item, "torque", r)) {
        cJSON_Delete(item);
        return NULL;
    }
    return item;
}

static cJSON* parse_collision_sound(reader* r) {
    cJSON* item = cJSON_CreateObject();
    if (!item || !json_add(item, "sound", parse_sound_ref(r), r)
              || !json_f32(item, "impactMinimum", r)
              || !json_f32(item, "impactMaximum", r)
              || !json_f32(item, "periodLength", r)
              || !json_u32(item, "maxSoundsPerPeriod", r)) {
        cJSON_Delete(item);
        return NULL;
    }
    return item;
}

static cJSON* parse_rotation_sound(reader* r) {
    cJSON* item = cJSON_CreateObject();
    if (!item || !json_add(item, "sound", parse_sound_ref(r), r)
              || !json_f32(item, "globalPeriodLength", r)
              || !json_u32(item, "globalMaxSoundsPerPeriod", r)
              || !json_bool8(item, "fixedPeriod", r)
              || !json_f32(item, "periodLength", r)
              || !json_f32(item, "angularSpeedStart", r)
              || !json_f32(item, "angularSpeedStop", r)
              || !json_f32(item, "angularSpeedVolumeZero", r)
              || !json_f32(item, "angularSpeedVolumeMax", r)) {
        cJSON_Delete(item);
        return NULL;
    }
    return item;
}

static cJSON* parse_limb(reader* r) {
    cJSON* limb = cJSON_CreateObject();
    if (!limb || !json_string(limb, "id", r)
              || !json_string(limb, "description", r)
              || !json_vec2(limb, "position", r)
              || !json_f32(limb, "orientation", r)
              || !json_vec2(limb, "momentum", r)
              || !json_f32(limb, "angularMomentum", r)
              || !json_f32(limb, "mass", r)
              || !json_f32(limb, "inertiaTensor", r)) {
        goto fail;
    }
    static const char* optional_names[] = {
        "gravityOverride", "mouseStiffnessOverride", "mouseDampenerOverride"
    };
    for (size_t i = 0; i < 3; i++) {
        if (!json_optional_f32(limb, optional_names[i], r)) {
            goto fail;
        }
    }
    if (!json_vec2(limb, "centreOfResistance", r)
        || !json_f32(limb, "airResistanceLinear", r)
        || !json_f32(limb, "airResistanceAngular", r)
        || !json_bool8(limb, "fixedMove", r)
        || !json_bool8(limb, "fixedRotate", r)
        || !json_bool8(limb, "defaultGrabMove", r)
        || !json_bool8(limb, "defaultGrabRotate", r)) {
        goto fail;
    }
    cJSON* material = cJSON_CreateObject();
    static const char* material_names[] = {
        "velocityResponse", "stiffness", "dampener",
        "kineticFriction", "staticFriction"
    };
    if (!material || !json_add(limb, "material", material, r)) {
        goto fail;
    }
    for (size_t i = 0; i < 5; i++) {
        if (!json_f32(material, material_names[i], r)) {
            goto fail;
        }
    }
    if (!json_string(limb, "localCollisionGroup", r)) {
        goto fail;
    }
    struct {
        const char* name;
        parse_item_fn fn;
    } collections[] = {
        {"collisionShape", parse_shape},
        {"magnetProducer", parse_magnet_producer},
        {"magnetConsumer", parse_magnet_consumer},
        {"linearMotor", parse_linear_motor},
        {"rotationalMotor", parse_rotational_motor},
        {"collisionSound", parse_collision_sound},
        {"rotationSound", parse_rotation_sound},
        {"sprite", parse_sprite},
    };
    for (size_t i = 0; i < sizeof collections / sizeof collections[0]; i++) {
        if (!json_counted(limb, collections[i].name, r,
                          collections[i].name, collections[i].fn)) {
            goto fail;
        }
    }
    return limb;

fail:
    cJSON_Delete(limb);
    return NULL;
}

static cJSON* parse_joint(reader* r) {
    cJSON* joint = cJSON_CreateObject();
    if (!joint || !json_string(joint, "id", r)
               || !json_string(joint, "description", r)) {
        cJSON_Delete(joint);
        return NULL;
    }
    static const char* keys[] = {"limb1", "limb2"};
    for (size_t i = 0; i < 2; i++) {
        cJSON* attachment = cJSON_CreateObject();
        if (!attachment || !json_add(joint, keys[i], attachment, r)
                        || !json_string(attachment, "limbID", r)
                        || !json_vec2(attachment, "attachPoint", r)) {
            cJSON_Delete(joint);
            return NULL;
        }
    }
    if (!json_f32(joint, "restLength", r)
        || !json_f32(joint, "stiffness", r)
        || !json_f32(joint, "dampener", r)) {
        cJSON_Delete(joint);
        return NULL;
    }
    return joint;
}

static cJSON* parse_rotational_joint(reader* r) {
    cJSON* joint = cJSON_CreateObject();
    if (!joint || !json_string(joint, "id", r)
               || !json_string(joint, "description", r)
               || !json_string(joint, "limbID1", r)
               || !json_f32(joint, "orientation1", r)
               || !json_string(joint, "limbID2", r)
               || !json_f32(joint, "orientation2", r)
               || !json_f32(joint, "restLength", r)
               || !json_f32(joint, "stiffness", r)
               || !json_f32(joint, "dampener", r)) {
        cJSON_Delete(joint);
        return NULL;
    }
    return joint;
}

static cJSON* parse_toy(reader* r) {
    cJSON* toy = cJSON_CreateObject();
    if (!toy || !json_string(toy, "id", r)
             || !json_string(toy, "description", r)
             || !json_vec2(toy, "basePosition", r)
             || !json_f32(toy, "baseOrientation", r)
             || !json_f32(toy, "baseScale", r)
             || !json_counted(toy, "sprite", r, "sprite", parse_sprite)
             || !json_counted(toy, "sound", r, "sound", parse_sound_ref)
             || !json_counted(toy, "limb", r, "limb", parse_limb)
             || !json_counted(toy, "joint", r, "joint", parse_joint)
             || !json_counted(toy, "rotationalJoint", r, "rotJoint",
                              parse_rotational_joint)) {
        cJSON_Delete(toy);
        return NULL;
    }
    return toy;
}

static cJSON* parse_property_value(reader* r, const char* key) {
    const uint32_t type = r_u32(r);
    cJSON* value = NULL;
    if (!r->ok) {
        return NULL;
    }
    if (type == 0) {
        const float number = r_f32(r);
        value = r->ok ? cJSON_CreateNumber(number) : NULL;
    } else if (type == 1) {
        const int32_t number = (int32_t)r_u32(r);
        value = r->ok ? cJSON_CreateNumber(number) : NULL;
    } else if (type == 2) {
        char* text = r_string(r);
        value = r->ok ? cJSON_CreateString(text) : NULL;
        free(text);
    } else {
        reader_error(r, "property type %u for %s at 0x%zx", type,
                     key ? key : "?", r->offset - 4);
    }
    if (r->ok && !value) {
        reader_oom(r, "out of memory creating property %s", key);
    }
    return value;
}

static cJSON* parse_property_groups(reader* r) {
    const uint32_t group_count = r_u32(r);
    if (!r->ok) {
        return NULL;
    }
    cJSON* properties = cJSON_CreateObject();
    cJSON* values = NULL;
    cJSON* value = NULL;
    char* group_name = NULL;
    char* key = NULL;
    if (!properties) {
        reader_oom(r, "out of memory creating properties");
        return NULL;
    }
    for (uint32_t group = 0; group < group_count; group++) {
        group_name = r_string(r);
        const uint32_t count = r_u32(r);
        values = cJSON_CreateObject();
        if (!r->ok || !group_name || !values) {
            if (r->ok) {
                reader_oom(r, "out of memory creating property group");
            }
            goto fail;
        }
        if (cJSON_GetObjectItemCaseSensitive(properties, group_name)) {
            reader_error(r, "duplicate property group %s", group_name);
            goto fail;
        }
        for (uint32_t i = 0; i < count; i++) {
            key = r_string(r);
            value = key ? parse_property_value(r, key) : NULL;
            if (!r->ok || !key || !value) {
                goto fail;
            }
            if (cJSON_GetObjectItemCaseSensitive(values, key)) {
                reader_error(r, "duplicate property %s.%s", group_name, key);
                goto fail;
            }
            if (!cJSON_AddItemToObject(values, key, value)) {
                reader_oom(r, "out of memory adding property");
                goto fail;
            }
            value = NULL;
            free(key);
            key = NULL;
        }
        if (!cJSON_AddItemToObject(properties, group_name, values)) {
            reader_oom(r, "out of memory adding property group");
            goto fail;
        }
        values = NULL;
        free(group_name);
        group_name = NULL;
    }
    return properties;

fail:
    free(key);
    cJSON_Delete(value);
    free(group_name);
    cJSON_Delete(values);
    cJSON_Delete(properties);
    return NULL;
}

static cJSON* parse_icon_action(reader* r) {
    const uint32_t type = r_u32(r);
    cJSON* action = cJSON_CreateObject();
    if (!r->ok || !action) {
        goto fail;
    }
    switch (type) {
        case 0:
            if (!json_string(action, "openToyInstance", r)
                || !json_u32(action, "globalToyInstanceLimit", r)) {
                goto fail;
            }
            break;
        case 1:
            if (!json_add(action, "destroyAllToyInstances",
                          cJSON_CreateObject(), r)) {
                goto fail;
            }
            break;
        case 2:
            if (!json_add(action, "quitToyBox", cJSON_CreateObject(), r)) {
                goto fail;
            }
            break;
        default:
            reader_error(r, "icon action type %u at 0x%zx", type,
                         r->offset - 4);
            goto fail;
    }
    return action;

fail:
    cJSON_Delete(action);
    return NULL;
}

static cJSON* parse_icon_event(reader* r) {
    const uint32_t type = r_u32(r);
    if (!r->ok) {
        return NULL;
    }
    if (type != 0) {
        reader_error(r, "icon event type %u at 0x%zx", type,
                     r->offset - 4);
        return NULL;
    }
    cJSON* event = cJSON_CreateObject();
    if (!event
        || !json_add(event, "onClick", cJSON_CreateObject(), r)
        || !json_counted(event, "action", r, "icon action",
                         parse_icon_action)) {
        cJSON_Delete(event);
        return NULL;
    }
    return event;
}

static cJSON* parse_icon(reader* r) {
    cJSON* icon = cJSON_CreateObject();
    if (!icon || !json_string(icon, "id", r)
              || !json_string(icon, "description", r)
              || !json_f32(icon, "order", r)
              || !json_add(icon, "sprite", parse_sprite(r), r)
              || !json_counted(icon, "event", r, "icon event",
                               parse_icon_event)) {
        cJSON_Delete(icon);
        return NULL;
    }
    return icon;
}

static cJSON* parse_manifest(toyfile* file) {
    reader r = {file->data, file->resource_data_offset,
                file->magic_offset + TOYFILE_MAGIC_SIZE + 4,
                true, false, {0}};
    // V4 header: lpstr versionString, zero u32 reserved, lpstr vendor,
    // followed by 36 bytes of zero padding.
    (void)r_skip_string(&r);
    const uint32_t header_reserved = r_u32(&r);
    (void)r_skip_string(&r);
    if (r.ok && header_reserved != 0) {
        reader_error(&r, "container header reserved field is non-zero");
    }
    if (!r.ok
        || !r_zeroes(&r, 36, "container header padding")) {
        file->out_of_memory = r.out_of_memory;
        file_error(file, "%s", r.error);
        return NULL;
    }

    uint32_t variant_word = 0;
    if (!raw_u32(r.data, r.size, r.offset, &variant_word)) {
        reader_error(&r, "truncated manifest variant at 0x%zx", r.offset);
    }
    cJSON* properties = NULL;
    if (r.ok && variant_word == 0) {
        // Empty property table: zero group count plus its six-byte terminator.
        if (r_zeroes(&r, 10, "empty property-table encoding")) {
            properties = cJSON_CreateObject();
        }
    } else {
        properties = parse_property_groups(&r);
        // A populated property table has the same six-byte zero terminator.
        (void)r_zeroes(&r, 6, "property-table terminator");
    }
    if (!r.ok || !properties) {
        file->out_of_memory = r.out_of_memory || (r.ok && !properties);
        file_error(file, "%s", r.error[0] ? r.error
                                           : "out of memory creating properties");
        cJSON_Delete(properties);
        return NULL;
    }

    cJSON* icons = parse_counted(&r, "icon", parse_icon);
    cJSON* toys = parse_counted(&r, "toy definition", parse_toy);
    if (r.ok && r.size - r.offset != 8) {
        reader_error(&r, "manifest leaves %zu bytes before resource data",
                     r.size - r.offset);
    }
    // The definition stream ends with an eight-byte zero terminator immediately
    // before the first resource payload.
    (void)r_zeroes(&r, 8, "definition-stream terminator");
    if (!r.ok) {
        file->out_of_memory = r.out_of_memory;
        file_error(file, "%s", r.error);
        cJSON_Delete(properties);
        cJSON_Delete(icons);
        cJSON_Delete(toys);
        return NULL;
    }
    cJSON* root = cJSON_CreateObject();
    if (!root) {
        cJSON_Delete(properties);
        cJSON_Delete(icons);
        cJSON_Delete(toys);
        file->out_of_memory = true;
        file_error(file, "out of memory assembling manifest");
        return NULL;
    }
    (void)cJSON_AddItemToObjectCS(root, "properties", properties);
    (void)cJSON_AddItemToObjectCS(root, "icons", icons);
    (void)cJSON_AddItemToObjectCS(root, "toys", toys);
    return root;
}

static bool raw_u32(const unsigned char* data, size_t size, size_t offset,
                    uint32_t* value) {
    if (offset > size || size - offset < 4) {
        return false;
    }
    const unsigned char* p = data + offset;
    *value = (uint32_t)p[0] | ((uint32_t)p[1] << 8)
           | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    return true;
}

typedef struct {
    uint8_t tag;
    size_t body_offset;
    size_t body_size;
    size_t end_offset;
} pgp_packet;

static bool parse_old_pgp_packet(toyfile* file, size_t offset,
                                 pgp_packet* packet) {
    if (offset >= file->size) {
        file_error(file, "missing OpenPGP packet at 0x%zx", offset);
        return false;
    }
    const uint8_t header = file->data[offset++];
    if ((header & 0xc0) != 0x80) {
        file_error(file, "unsupported OpenPGP packet header 0x%02x", header);
        return false;
    }
    const unsigned length_type = header & 3u;
    size_t length_bytes = 0;
    switch (length_type) {
        case 0: length_bytes = 1; break;
        case 1: length_bytes = 2; break;
        case 2: length_bytes = 4; break;
        default:
            file_error(file, "indeterminate OpenPGP packet length at 0x%zx",
                       offset - 1);
            return false;
    }
    if (length_bytes > file->size - offset) {
        file_error(file, "truncated OpenPGP packet length at 0x%zx", offset);
        return false;
    }
    uint32_t body_size = 0;
    for (size_t i = 0; i < length_bytes; i++) {
        body_size = (body_size << 8) | file->data[offset++];
    }
    if ((size_t)body_size > file->size - offset) {
        file_error(file, "truncated OpenPGP packet body at 0x%zx", offset);
        return false;
    }
    packet->tag = (header >> 2) & 0x0f;
    packet->body_offset = offset;
    packet->body_size = body_size;
    packet->end_offset = offset + body_size;
    return true;
}

static bool parse_payload_bounds(toyfile* file) {
    if (file->size >= TOYFILE_MAGIC_SIZE
        && memcmp(file->data, TOYFILE_MAGIC, TOYFILE_MAGIC_SIZE) == 0) {
        file->magic_offset = 0;
        return true;
    }

    // Signed stock files retain the old OpenPGP framing, but OpenSoup
    // intentionally ignores the obsolete DLC(?)-authentication signature body
    pgp_packet signature = {0};
    if (!parse_old_pgp_packet(file, 0, &signature)) {
        return false;
    }
    if (signature.tag != 2) {
        file_error(file, "expected OpenPGP signature packet, got tag %u",
                   signature.tag);
        return false;
    }

    pgp_packet literal = {0};
    if (!parse_old_pgp_packet(file, signature.end_offset, &literal)) {
        return false;
    }
    if (literal.tag != 11) {
        file_error(file, "expected OpenPGP literal packet, got tag %u",
                   literal.tag);
        return false;
    }
    if (literal.end_offset != file->size) {
        file_error(file, "bytes remain after OpenPGP literal packet");
        return false;
    }
    if (literal.body_size < 6) {
        file_error(file, "truncated OpenPGP literal header");
        return false;
    }
    size_t offset = literal.body_offset;
    if (file->data[offset++] != 'b') {
        file_error(file, "OpenPGP literal payload is not binary");
        return false;
    }
    const size_t filename_size = file->data[offset++];
    if (filename_size > literal.end_offset - offset
        || literal.end_offset - offset - filename_size < 4) {
        file_error(file, "truncated OpenPGP literal filename/timestamp");
        return false;
    }
    offset += filename_size + 4;
    if (literal.end_offset - offset < TOYFILE_MAGIC_SIZE
        || memcmp(file->data + offset, TOYFILE_MAGIC,
                  TOYFILE_MAGIC_SIZE) != 0) {
        file_error(file, "OpenPGP literal payload is not a Souptoys container");
        return false;
    }
    file->magic_offset = offset;
    return true;
}

static toyfile_status parse_resources(toyfile* file) {
    if (file->size < file->magic_offset
        || file->size - file->magic_offset < TOYFILE_V4_FOOTER_SIZE) {
        file_error(file, "container is too small for the V4 footer");
        return TOYFILE_INVALID_FORMAT;
    }
    const size_t footer_offset = file->size - TOYFILE_V4_FOOTER_SIZE;
    uint32_t directory_relative = 0;
    if (!raw_u32(file->data, file->size, footer_offset,
                 &directory_relative)
        || (size_t)directory_relative > footer_offset - file->magic_offset) {
        file_error(file, "invalid V4 resource-directory offset");
        return TOYFILE_INVALID_FORMAT;
    }
    const size_t directory_offset = file->magic_offset
                                  + (size_t)directory_relative;
    const size_t header_end = file->magic_offset + TOYFILE_MAGIC_SIZE + 4;
    if (directory_offset < header_end || footer_offset - directory_offset < 4) {
        file_error(file, "resource directory is outside the payload");
        return TOYFILE_INVALID_FORMAT;
    }

    reader r = {file->data, footer_offset, directory_offset,
                true, false, {0}};
    const uint32_t count = r_u32(&r);
    const size_t minimum_entry_size = 16;
    const size_t maximum_count = (footer_offset - r.offset)
                               / minimum_entry_size;
    if (!r.ok || (size_t)count > maximum_count) {
        file_error(file, "resource count %u exceeds the directory bounds", count);
        return TOYFILE_INVALID_FORMAT;
    }
    toyfile_resource* resources = calloc(count ? count : 1,
                                         sizeof(*resources));
    if (!resources) {
        file_error(file, "out of memory reading resource directory");
        return TOYFILE_OUT_OF_MEMORY;
    }
    file->resources = resources;
    file->resource_count = count;
    file->resource_data_offset = directory_offset;

    size_t total_resource_size = 0;
    for (size_t i = 0; i < count; i++) {
        toyfile_resource* resource = &resources[i];
        resource->name = (const char*)r_lpstr_slice(
            &r, &resource->name_size);
        const uint32_t data_relative = r_u32(&r);
        resource->extension = (const char*)r_lpstr_slice(
            &r, &resource->extension_size);
        const uint32_t data_size = r_u32(&r);
        if (!r.ok) {
            file_error(file, "%s", r.error);
            return TOYFILE_INVALID_FORMAT;
        }
        if ((size_t)data_relative > directory_offset - file->magic_offset) {
            file_error(file, "resource %zu starts outside the data region", i);
            return TOYFILE_INVALID_FORMAT;
        }
        const size_t data_offset = file->magic_offset
                                 + (size_t)data_relative;
        if (data_offset < header_end
            || (size_t)data_size > directory_offset - data_offset) {
            file_error(file, "invalid resource range at index %zu", i);
            return TOYFILE_INVALID_FORMAT;
        }
        resource->data_offset = data_offset;
        resource->data_size = data_size;
        if (data_offset < file->resource_data_offset) {
            file->resource_data_offset = data_offset;
        }
        if ((size_t)data_size > SIZE_MAX - total_resource_size) {
            file_error(file, "resource byte count overflows size_t");
            return TOYFILE_INVALID_FORMAT;
        }
        total_resource_size += data_size;
    }
    if (r.offset != footer_offset) {
        file_error(file, "resource directory leaves %zu unconsumed bytes",
                   footer_offset - r.offset);
        return TOYFILE_INVALID_FORMAT;
    }

    for (size_t i = 0; i < count; i++) {
        const size_t a_start = resources[i].data_offset;
        const size_t a_end = a_start + resources[i].data_size;
        for (size_t j = i + 1; j < count; j++) {
            const size_t b_start = resources[j].data_offset;
            const size_t b_end = b_start + resources[j].data_size;
            if (a_start < b_end && b_start < a_end) {
                file_error(file, "resource ranges overlap: %zu and %zu", i, j);
                return TOYFILE_INVALID_FORMAT;
            }
        }
    }
    if (count && total_resource_size
                     != directory_offset - file->resource_data_offset) {
        file_error(file, "resource data region contains an unexplained gap");
        return TOYFILE_INVALID_FORMAT;
    }
    return TOYFILE_OK;
}

static toyfile_status parse_file(toyfile* file) {
    if (!parse_payload_bounds(file)) {
        return TOYFILE_INVALID_FORMAT;
    }
    uint32_t version = 0;
    if (!raw_u32(file->data, file->size,
                 file->magic_offset + TOYFILE_MAGIC_SIZE, &version)) {
        file_error(file, "truncated container header");
        return TOYFILE_INVALID_FORMAT;
    }
    if (version != 4) {
        file_error(file, "unsupported Souptoys container version %u", version);
        return TOYFILE_INVALID_FORMAT;
    }
    toyfile_status status = parse_resources(file);
    if (status != TOYFILE_OK) {
        return status;
    }
    cJSON* manifest = parse_manifest(file);
    if (!manifest) {
        return file->out_of_memory ? TOYFILE_OUT_OF_MEMORY
                                   : TOYFILE_INVALID_FORMAT;
    }
    file->manifest_json = cJSON_PrintUnformatted(manifest);
    cJSON_Delete(manifest);
    if (!file->manifest_json) {
        file_error(file, "out of memory serializing manifest");
        return TOYFILE_OUT_OF_MEMORY;
    }
    return TOYFILE_OK;
}

toyfile_status toyfile_open_path(const char* path, toyfile** out) {
    if (!out) {
        return TOYFILE_INVALID_ARGUMENT;
    }
    *out = NULL;
    toyfile* file = calloc(1, sizeof(*file));
    if (!file) {
        return TOYFILE_OUT_OF_MEMORY;
    }
    *out = file;
    if (!path) {
        file_error(file, "missing input path");
        return TOYFILE_INVALID_ARGUMENT;
    }
    FILE* input = fopen(path, "rb");
    if (!input) {
        file_error(file, "cannot open %s", path);
        return TOYFILE_IO_ERROR;
    }
    if (fseek(input, 0, SEEK_END) != 0) {
        fclose(input);
        file_error(file, "cannot seek %s", path);
        return TOYFILE_IO_ERROR;
    }
    const long length = ftell(input);
    if (length <= 0 || fseek(input, 0, SEEK_SET) != 0) {
        fclose(input);
        file_error(file, "cannot size %s", path);
        return TOYFILE_IO_ERROR;
    }
    file->data = malloc((size_t)length);
    if (!file->data) {
        fclose(input);
        file_error(file, "out of memory reading %s", path);
        return TOYFILE_OUT_OF_MEMORY;
    }
    if (fread(file->data, 1, (size_t)length, input) != (size_t)length) {
        fclose(input);
        file_error(file, "short read from %s", path);
        return TOYFILE_IO_ERROR;
    }
    fclose(input);
    file->size = (size_t)length;
    return parse_file(file);
}

void toyfile_close(toyfile* file) {
    if (!file) {
        return;
    }
    free(file->resources);
    free(file->manifest_json);
    free(file->data);
    free(file);
}

const char* toyfile_error(const toyfile* file) {
    return file && file->error[0] ? file->error : "unknown toyfile error";
}

const char* toyfile_manifest_json(const toyfile* file) {
    return file ? file->manifest_json : NULL;
}

size_t toyfile_resource_count(const toyfile* file) {
    return file ? file->resource_count : 0;
}

toyfile_status toyfile_resource_at(const toyfile* file, size_t index,
                                   const char** name, size_t* name_size,
                                   const char** extension,
                                   size_t* extension_size,
                                   const void** data, size_t* data_size) {
    if (!file || index >= file->resource_count) {
        return TOYFILE_INVALID_ARGUMENT;
    }
    const toyfile_resource* resource = &file->resources[index];
    if (name) *name = resource->name;
    if (name_size) *name_size = resource->name_size;
    if (extension) *extension = resource->extension;
    if (extension_size) *extension_size = resource->extension_size;
    if (data) *data = file->data + resource->data_offset;
    if (data_size) *data_size = resource->data_size;
    return TOYFILE_OK;
}
