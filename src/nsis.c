#include "nsis.h"

#include "LzmaDec.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    FIRST_HEADER_SIZE = 28,
    FIRST_HEADER_SIGNATURE_SIZE = 16,
    LZMA_PROPERTIES_SIZE = 5,
    NSIS_CRC_SIZE = 4,
    NSIS_BLOCK_COUNT = 8,
    NSIS_ENTRY_SIZE = 28,
    NSIS_ENTRY_BLOCK = 2,
    NSIS_STRING_BLOCK = 3,
    EW_CREATEDIR = 11,
    EW_EXTRACTFILE = 20,
};

typedef struct {
    char* text;
    size_t size;
} error_context;

typedef struct {
    const unsigned char* data;
    size_t size;
} byte_slice;

typedef enum {
    OUTPUT_OTHER,
    OUTPUT_TOYS,
    OUTPUT_PLAYSETS,
} output_kind;

static void set_error(error_context* error, const char* format, ...) {
    if (!error->text || error->size == 0) {
        return;
    }
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error->text, error->size, format, arguments);
    va_end(arguments);
}

static uint32_t read_u32(const unsigned char* data) {
    return (uint32_t)data[0]
         | (uint32_t)data[1] << 8
         | (uint32_t)data[2] << 16
         | (uint32_t)data[3] << 24;
}

static bool read_file(const char* path, unsigned char** data, size_t* size,
                      error_context* error) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        set_error(error, "cannot open installer %s", path);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        set_error(error, "cannot seek installer %s", path);
        fclose(file);
        return false;
    }
    const long end = ftell(file);
    if (end < 0 || fseek(file, 0, SEEK_SET) != 0) {
        set_error(error, "cannot measure installer %s", path);
        fclose(file);
        return false;
    }
    *size = (size_t)end;
    *data = malloc(*size ? *size : 1);
    if (!*data) {
        set_error(error, "out of memory reading installer");
        fclose(file);
        return false;
    }
    if (fread(*data, 1, *size, file) != *size) {
        set_error(error, "cannot read installer %s", path);
        free(*data);
        *data = NULL;
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

// The executable stub and the first-header fields are opaque to us.
// The signature only locates the raw LZMA properties after the 28-byte header.
static bool find_lzma_properties(const unsigned char* data, size_t size,
                                 size_t* properties_offset,
                                 error_context* error) {
    static const unsigned char signature[FIRST_HEADER_SIGNATURE_SIZE] = {
        0xef, 0xbe, 0xad, 0xde,
        'N', 'u', 'l', 'l', 's', 'o', 'f', 't', 'I', 'n', 's', 't',
    };
    const size_t framing = FIRST_HEADER_SIZE + LZMA_PROPERTIES_SIZE
                         + NSIS_CRC_SIZE;
    bool found = false;
    for (size_t offset = 0; offset <= size - framing; offset++) {
        if (memcmp(data + offset + 4, signature, sizeof signature) != 0) {
            continue;
        }
        if (found) {
            set_error(error, "installer contains multiple NSIS signatures");
            return false;
        }
        found = true;
        *properties_offset = offset + FIRST_HEADER_SIZE;
    }
    if (!found) {
        set_error(error, "installer has no complete NSIS signature");
        return false;
    }
    return true;
}

static uint32_t installer_crc32(const unsigned char* data, size_t size) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 512; i < size - NSIS_CRC_SIZE; i++) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; bit++) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static void* lzma_alloc(ISzAllocPtr allocator, size_t size) {
    (void)allocator;
    return malloc(size);
}

static void lzma_free(ISzAllocPtr allocator, void* address) {
    (void)allocator;
    free(address);
}

static bool decode_lzma(const unsigned char* properties,
                        const unsigned char* source, size_t source_size,
                        unsigned char** output, size_t* output_size,
                        error_context* error) {
    static const ISzAlloc allocator = {lzma_alloc, lzma_free};
    CLzmaDec decoder;
    LzmaDec_Construct(&decoder);
    SRes result = LzmaDec_Allocate(
        &decoder, properties, LZMA_PROPERTIES_SIZE, &allocator);
    if (result != SZ_OK) {
        set_error(error, "unsupported NSIS LZMA properties (%d)", result);
        return false;
    }
    LzmaDec_Init(&decoder);

    size_t capacity = 1024 * 1024;
    unsigned char* decoded = malloc(capacity);
    if (!decoded) {
        set_error(error, "out of memory decoding installer");
        LzmaDec_Free(&decoder, &allocator);
        return false;
    }

    size_t source_offset = 0;
    size_t decoded_size = 0;
    ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;
    while (status != LZMA_STATUS_FINISHED_WITH_MARK) {
        if (decoded_size == capacity) {
            if (capacity > SIZE_MAX / 2) {
                set_error(error, "decoded installer is too large");
                free(decoded);
                LzmaDec_Free(&decoder, &allocator);
                return false;
            }
            capacity *= 2;
            unsigned char* grown = realloc(decoded, capacity);
            if (!grown) {
                set_error(error, "out of memory decoding installer");
                free(decoded);
                LzmaDec_Free(&decoder, &allocator);
                return false;
            }
            decoded = grown;
        }

        SizeT destination_length = capacity - decoded_size;
        SizeT source_length = source_size - source_offset;
        const size_t old_source = source_offset;
        const size_t old_decoded = decoded_size;
        result = LzmaDec_DecodeToBuf(
            &decoder, decoded + decoded_size, &destination_length,
            source + source_offset, &source_length,
            LZMA_FINISH_ANY, &status);
        source_offset += source_length;
        decoded_size += destination_length;
        if (result != SZ_OK) {
            set_error(error, "cannot decode NSIS LZMA stream (%d)", result);
            free(decoded);
            LzmaDec_Free(&decoder, &allocator);
            return false;
        }
        if (source_offset == old_source && decoded_size == old_decoded) {
            set_error(error, "truncated NSIS LZMA stream");
            free(decoded);
            LzmaDec_Free(&decoder, &allocator);
            return false;
        }
    }
    LzmaDec_Free(&decoder, &allocator);
    if (source_offset != source_size) {
        set_error(error, "NSIS LZMA stream leaves %zu compressed bytes",
                  source_size - source_offset);
        free(decoded);
        return false;
    }
    *output = decoded;
    *output_size = decoded_size;
    return true;
}

static bool string_at(const unsigned char* strings, size_t strings_size,
                      uint32_t offset, byte_slice* string,
                      error_context* error) {
    if ((size_t)offset >= strings_size) {
        set_error(error, "NSIS string offset %u is out of bounds", offset);
        return false;
    }
    const unsigned char* start = strings + offset;
    const unsigned char* end = memchr(start, 0, strings_size - offset);
    if (!end) {
        set_error(error, "unterminated NSIS string at offset %u", offset);
        return false;
    }
    string->data = start;
    string->size = (size_t)(end - start);
    return true;
}

static output_kind output_directory_kind(byte_slice path) {
    static const unsigned char toys[] = {
        0xfe, 0x1a, 0x23, '\\',
        'S', 'o', 'u', 'p', 't', 'o', 'y', 's', '2', '\\',
        'T', 'o', 'y', 's',
    };
    static const unsigned char playsets[] = {
        0xfe, 0x05, 0x2e, '\\',
        'P', 'l', 'a', 'y', 's', 'e', 't', 's',
    };
    if (path.size == sizeof toys && memcmp(path.data, toys, sizeof toys) == 0) {
        return OUTPUT_TOYS;
    }
    if (path.size == sizeof playsets
        && memcmp(path.data, playsets, sizeof playsets) == 0) {
        return OUTPUT_PLAYSETS;
    }
    return OUTPUT_OTHER;
}

static unsigned char ascii_lower(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c - 'A' + 'a') : c;
}

static bool ends_with(byte_slice string, const char* suffix) {
    const size_t suffix_size = strlen(suffix);
    if (string.size < suffix_size) {
        return false;
    }
    const unsigned char* tail = string.data + string.size - suffix_size;
    for (size_t i = 0; i < suffix_size; i++) {
        if (ascii_lower(tail[i]) != (unsigned char)suffix[i]) {
            return false;
        }
    }
    return true;
}

static bool safe_filename(byte_slice name) {
    if (name.size == 0
        || (name.size == 1 && name.data[0] == '.')
        || (name.size == 2 && name.data[0] == '.' && name.data[1] == '.')) {
        return false;
    }
    for (size_t i = 0; i < name.size; i++) {
        if (name.data[i] < 0x20 || name.data[i] >= 0x7f
            || name.data[i] == '/' || name.data[i] == '\\') {
            return false;
        }
    }
    return true;
}

static bool catalog_from_decoded(const unsigned char* decoded,
                                 size_t decoded_size,
                                 unsigned types,
                                 nsis_container** output_containers,
                                 size_t* output_count,
                                 error_context* error) {
    if (decoded_size < 4) {
        set_error(error, "decoded NSIS stream has no header size");
        return false;
    }
    const size_t header_size = read_u32(decoded);
    if (header_size > decoded_size - 4) {
        set_error(error, "decoded NSIS header extends past the stream");
        return false;
    }
    const unsigned char* header = decoded + 4;
    if (header_size < 4 + NSIS_BLOCK_COUNT * 8) {
        set_error(error, "decoded NSIS header is truncated");
        return false;
    }

    uint32_t block_offsets[NSIS_BLOCK_COUNT];
    uint32_t block_counts[NSIS_BLOCK_COUNT];
    for (size_t i = 0; i < NSIS_BLOCK_COUNT; i++) {
        block_offsets[i] = read_u32(header + 4 + i * 8);
        block_counts[i] = read_u32(header + 8 + i * 8);
    }
    const size_t entries_offset = block_offsets[NSIS_ENTRY_BLOCK];
    const size_t entries_count = block_counts[NSIS_ENTRY_BLOCK];
    const size_t strings_offset = block_offsets[NSIS_STRING_BLOCK];
    // Pages, sections, language tables, and UI data describe installer
    // execution only. Container discovery needs the entry and string blocks;
    // the full compressed stream is still decoded and covered by the CRC.
    size_t strings_end = header_size;
    for (size_t i = NSIS_STRING_BLOCK + 1; i < NSIS_BLOCK_COUNT; i++) {
        if (block_offsets[i] && block_offsets[i] < strings_end) {
            strings_end = block_offsets[i];
        }
    }
    if (entries_offset > strings_offset
        || entries_count > (strings_offset - entries_offset) / NSIS_ENTRY_SIZE
        || entries_offset + entries_count * NSIS_ENTRY_SIZE != strings_offset
        || strings_offset > strings_end || strings_end > header_size) {
        set_error(error, "invalid NSIS entry or string-table bounds");
        return false;
    }

    const unsigned char* strings = header + strings_offset;
    const size_t strings_size = strings_end - strings_offset;
    const unsigned char* payloads = header + header_size;
    const size_t payloads_size = decoded_size - 4 - header_size;
    nsis_container* containers = calloc(
        entries_count ? entries_count : 1, sizeof(*containers));
    if (!containers) {
        set_error(error, "out of memory indexing NSIS containers");
        return false;
    }
    size_t container_count = 0;
    output_kind output = OUTPUT_OTHER;
    size_t toy_count = 0;
    size_t playset_count = 0;

    for (size_t i = 0; i < entries_count; i++) {
        const unsigned char* entry = header + entries_offset
                                   + i * NSIS_ENTRY_SIZE;
        const uint32_t opcode = read_u32(entry);
        uint32_t offsets[6];
        for (size_t field = 0; field < 6; field++) {
            offsets[field] = read_u32(entry + 4 + field * 4);
        }
        if (opcode == EW_CREATEDIR && offsets[1]) {
            byte_slice path;
            if (!string_at(strings, strings_size, offsets[0], &path, error)) {
                goto fail;
            }
            output = output_directory_kind(path);
            continue;
        }
        if (opcode != EW_EXTRACTFILE) {
            continue;
        }

        byte_slice name;
        if (!string_at(strings, strings_size, offsets[1], &name, error)) {
            goto fail;
        }
        unsigned type = 0;
        if (ends_with(name, ".toy")) {
            if (output != OUTPUT_TOYS) {
                set_error(error, ".toy entry is outside the Souptoys toy directory");
                goto fail;
            }
            type = NSIS_CONTAINER_TOY;
            toy_count++;
        } else if (ends_with(name, ".playset")) {
            if (output != OUTPUT_PLAYSETS) {
                set_error(error,
                          ".playset entry is outside the Souptoys playset directory");
                goto fail;
            }
            type = NSIS_CONTAINER_PLAYSET;
            playset_count++;
        } else {
            continue;
        }
        if (!safe_filename(name)) {
            set_error(error, "unsafe NSIS container filename at entry %zu", i);
            goto fail;
        }
        const size_t data_offset = offsets[2];
        if (data_offset > payloads_size || payloads_size - data_offset < 4) {
            set_error(error, "NSIS container offset is out of bounds at entry %zu", i);
            goto fail;
        }
        const size_t container_size = read_u32(payloads + data_offset);
        if (container_size > payloads_size - data_offset - 4) {
            set_error(error, "NSIS container is truncated at entry %zu", i);
            goto fail;
        }
        if (types & type) {
            containers[container_count++] = (nsis_container){
                .name = (const char*)name.data,
                .type = (nsis_container_type)type,
                .data = payloads + data_offset + 4,
                .size = container_size,
            };
        }
    }
    if ((types & NSIS_CONTAINER_TOY) && toy_count == 0) {
        set_error(error, "installer contains no Souptoys .toy files");
        goto fail;
    }
    if ((types & NSIS_CONTAINER_PLAYSET) && playset_count == 0) {
        set_error(error, "installer contains no Souptoys .playset files");
        goto fail;
    }
    *output_containers = containers;
    *output_count = container_count;
    return true;

fail:
    free(containers);
    return false;
}

bool nsis_decode_containers(const char* installer_path, unsigned types,
                            nsis_container_consumer consumer,
                            char* error_text, size_t error_size) {
    error_context error = {error_text, error_size};
    if (error.text && error.size) {
        error.text[0] = 0;
    }
    const unsigned supported = NSIS_CONTAINER_TOY | NSIS_CONTAINER_PLAYSET;
    if (!installer_path || !installer_path[0] || !consumer
        || !types || (types & ~supported)) {
        set_error(&error, "missing installer, consumer, or container type");
        return false;
    }

    unsigned char* installer = NULL;
    size_t installer_size = 0;
    if (!read_file(installer_path, &installer, &installer_size, &error)) {
        return false;
    }
    if (installer_size < 512 + NSIS_CRC_SIZE) {
        set_error(&error, "selected installer is too small");
        free(installer);
        return false;
    }

    size_t properties_offset = 0;
    if (!find_lzma_properties(installer, installer_size,
                              &properties_offset, &error)) {
        free(installer);
        return false;
    }
    const uint32_t stored_crc = read_u32(installer + installer_size - 4);
    if (installer_crc32(installer, installer_size) != stored_crc) {
        set_error(&error, "Souptoys installer failed its NSIS CRC check");
        free(installer);
        return false;
    }
    const unsigned char* properties = installer + properties_offset;
    const unsigned char* compressed = properties + LZMA_PROPERTIES_SIZE;
    const size_t compressed_size = installer_size - NSIS_CRC_SIZE
                                 - (size_t)(compressed - installer);
    unsigned char* decoded = NULL;
    size_t decoded_size = 0;
    if (!decode_lzma(properties, compressed, compressed_size,
                     &decoded, &decoded_size, &error)) {
        free(installer);
        return false;
    }
    free(installer);

    nsis_container* containers = NULL;
    size_t count = 0;
    if (!catalog_from_decoded(decoded, decoded_size, types,
                              &containers, &count, &error)) {
        free(decoded);
        return false;
    }
    const bool ok = consumer(containers, count, error_text, error_size);
    free(containers);
    free(decoded);
    return ok;
}
