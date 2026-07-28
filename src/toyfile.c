#include "toyfile.h"

#include "cJSON.h"
#include "toyfile_internal.h"
#include "toyfile_reader.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOYFILE_MAGIC "SOUPTOYS.COM TOY FORMAT\0"
#define TOYFILE_MAGIC_SIZE 24
#define TOYFILE_MD5_STATE_SIZE 96
#define TOYFILE_OLD_FOOTER_SIZE 4
#define TOYFILE_NEW_FOOTER_SIZE (4 + TOYFILE_MD5_STATE_SIZE)

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
    bool owns_data;
    size_t magic_offset;
    size_t resource_data_offset;
    uint32_t version;
    toyfile_resource* resources;
    size_t resource_count;
    char error[320];
};

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

static void manifest_error(char* error, size_t error_size,
                           const char* fmt, ...) {
    if (!error || error_size == 0) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(error, error_size, fmt, ap);
    va_end(ap);
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

static bool json_null(cJSON* object, const char* key, reader* r) {
    return json_add(object, key, cJSON_CreateNull(), r);
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
                   : json_null(object, key, r);
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

static bool skip_info(reader* r) {
    for (size_t i = 0; i < 4; i++) {
        (void)r_skip_string(r);
    }
    for (size_t i = 0; i < 4; i++) {
        (void)r_u32(r);
    }
    for (size_t i = 0; i < 4; i++) {
        (void)r_skip_string(r);
    }
    return r->ok;
}

static bool skip_optional_f32(reader* r) {
    const bool present = r_u8(r) != 0;
    if (r->ok && present) {
        (void)r_f32(r);
    }
    return r->ok;
}

static bool skip_optional_vec2(reader* r) {
    const bool present = r_u8(r) != 0;
    if (r->ok && present) {
        (void)r_f32(r);
        (void)r_f32(r);
    }
    return r->ok;
}

static bool skip_world(reader* r, uint32_t version) {
    for (size_t i = 0; i < 6; i++) {
        if (!skip_optional_f32(r)) {
            return false;
        }
    }
    if (version <= 3) {
        const bool deprecated_record = r_u8(r) != 0;
        if (!r->ok || (deprecated_record && !r_skip(r, 29))) {
            return false;
        }
        return version == 1 || skip_optional_vec2(r);
    }
    return skip_optional_vec2(r);
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

static cJSON* parse_empty_legacy_section(reader* r, const char* what) {
    const uint32_t count = r_u32(r);
    if (r->ok && count != 0) {
        reader_error(r, "legacy %s count %u is unsupported", what, count);
    }
    cJSON* array = r->ok ? cJSON_CreateArray() : NULL;
    if (r->ok && !array) {
        reader_oom(r, "out of memory creating %s array", what);
    }
    return array;
}

static cJSON* parse_manifest_body(const toyfile* file,
                                  toyfile_status* status,
                                  char* error, size_t error_size) {
    toyfile_body body = {0};
    if (!toyfile_get_body(file, &body)) {
        *status = TOYFILE_INVALID_FORMAT;
        manifest_error(error, error_size, "container has no decoded body");
        return NULL;
    }
    reader r = {body.data, body.size, 0, true, false, {0}};
    cJSON* properties = NULL;
    cJSON* icons = NULL;
    cJSON* toys = NULL;
    cJSON* manifest = NULL;

    if (!skip_info(&r)) {
        goto fail;
    }
    properties = parse_property_groups(&r);
    if (body.version == 4) {
        const uint32_t collision_count = r_u32(&r);
        if (r.ok && collision_count != 0) {
            reader_error(&r, "collision config count %u is unsupported",
                         collision_count);
        }
    }

    const bool world_present = r_u8(&r) != 0;
    if (r.ok && world_present && !skip_world(&r, body.version)) {
        goto fail;
    }
    const bool toybox_present = r_u8(&r) != 0;
    if (r.ok && toybox_present && !skip_toybox(&r, body.version)) {
        goto fail;
    }

    if (body.version == 4) {
        icons = parse_counted(&r, "icon", parse_icon);
        toys = parse_counted(&r, "toy definition", parse_toy);
    } else {
        icons = parse_empty_legacy_section(&r, "icon");
        toys = parse_empty_legacy_section(&r, "toy definition");
    }

    const uint32_t instance_count = r_u32(&r);
    if (r.ok && instance_count != 0) {
        reader_error(&r, "container carries %u playset instances",
                     instance_count);
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
    if (!r.ok || !properties || !icons || !toys) {
        goto fail;
    }
    if (world_present && cJSON_GetArraySize(icons) == 0
        && cJSON_GetArraySize(toys) == 0) {
        reader_error(&r, "container is a playset, not a toy pack");
        goto fail;
    }

    manifest = cJSON_CreateObject();
    if (!manifest) {
        reader_oom(&r, "out of memory creating toy manifest");
        goto fail;
    }
    if (!json_add(manifest, "properties", properties, &r)) {
        goto fail;
    }
    properties = NULL;
    if (!json_add(manifest, "icons", icons, &r)) {
        goto fail;
    }
    icons = NULL;
    if (!json_add(manifest, "toys", toys, &r)) {
        goto fail;
    }
    toys = NULL;
    *status = TOYFILE_OK;
    return manifest;

fail:
    *status = r.out_of_memory ? TOYFILE_OUT_OF_MEMORY
                              : TOYFILE_INVALID_FORMAT;
    manifest_error(error, error_size, "%s",
                   r.error[0] ? r.error
                              : "out of memory decoding toy manifest");
    cJSON_Delete(properties);
    cJSON_Delete(icons);
    cJSON_Delete(toys);
    cJSON_Delete(manifest);
    return NULL;
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
    const size_t footer_size = file->version < 3
        ? TOYFILE_OLD_FOOTER_SIZE : TOYFILE_NEW_FOOTER_SIZE;
    if (file->size < file->magic_offset
        || file->size - file->magic_offset < footer_size) {
        file_error(file, "container is too small for the version %u footer",
                   file->version);
        return TOYFILE_INVALID_FORMAT;
    }
    const size_t footer_offset = file->size - footer_size;
    uint32_t directory_relative = 0;
    if (!raw_u32(file->data, file->size, footer_offset,
                 &directory_relative)
        || (size_t)directory_relative > footer_offset - file->magic_offset) {
        file_error(file, "invalid resource-directory offset");
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
    if (!raw_u32(file->data, file->size,
                 file->magic_offset + TOYFILE_MAGIC_SIZE, &file->version)) {
        file_error(file, "truncated container header");
        return TOYFILE_INVALID_FORMAT;
    }
    if (file->version < 1 || file->version > 4) {
        file_error(file, "unsupported Souptoys container version %u",
                   file->version);
        return TOYFILE_INVALID_FORMAT;
    }
    return parse_resources(file);
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
    file->owns_data = true;
    if (fread(file->data, 1, (size_t)length, input) != (size_t)length) {
        fclose(input);
        file_error(file, "short read from %s", path);
        return TOYFILE_IO_ERROR;
    }
    fclose(input);
    file->size = (size_t)length;
    return parse_file(file);
}

toyfile_status toyfile_open_memory(const void* data, size_t size,
                                   toyfile** out) {
    if (!out) {
        return TOYFILE_INVALID_ARGUMENT;
    }
    *out = NULL;
    toyfile* file = calloc(1, sizeof(*file));
    if (!file) {
        return TOYFILE_OUT_OF_MEMORY;
    }
    *out = file;
    if (!data || size == 0) {
        file_error(file, "missing input data");
        return TOYFILE_INVALID_ARGUMENT;
    }
    file->data = (unsigned char*)data;
    file->size = size;
    return parse_file(file);
}

void toyfile_close(toyfile* file) {
    if (!file) {
        return;
    }
    free(file->resources);
    if (file->owns_data) {
        free(file->data);
    }
    free(file);
}

const char* toyfile_error(const toyfile* file) {
    return file && file->error[0] ? file->error : "unknown toyfile error";
}

toyfile_status toyfile_decode_manifest(const toyfile* file,
                                       struct cJSON** out,
                                       char* error, size_t error_size) {
    if (error && error_size) {
        error[0] = 0;
    }
    if (!out) {
        manifest_error(error, error_size, "missing manifest output");
        return TOYFILE_INVALID_ARGUMENT;
    }
    *out = NULL;
    if (!file) {
        manifest_error(error, error_size, "missing container");
        return TOYFILE_INVALID_ARGUMENT;
    }

    toyfile_status status = TOYFILE_INVALID_FORMAT;
    cJSON* manifest = parse_manifest_body(file, &status, error, error_size);
    if (status == TOYFILE_OK) {
        *out = manifest;
    }
    return status;
}

bool toyfile_get_body(const toyfile* file, toyfile_body* out) {
    if (!file || !out) {
        return false;
    }
    const size_t start = file->magic_offset + TOYFILE_MAGIC_SIZE + 4;
    if (start > file->resource_data_offset
        || file->resource_data_offset > file->size) {
        return false;
    }
    *out = (toyfile_body){
        file->data + start,
        file->resource_data_offset - start,
        file->version,
    };
    return true;
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
