#include "storage.h"
#include "logger.h"

#include <stdlib.h>
#include <string.h>

#define countof(e) (sizeof(e) / sizeof((e)[0]))

#define LOG(...) aq_logger(0, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOGE(...) aq_logger(1, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define EXPECT(e, ...)                                                         \
    do {                                                                       \
        if (!(e)) {                                                            \
            LOGE(__VA_ARGS__);                                                 \
            goto Error;                                                        \
        }                                                                      \
    } while (0)
#define CHECK(e) EXPECT(e, #e)

/// Copies contents, reallocating string storage if necessary.
static int
copy_string(struct String* dst, const struct String* src)
{
    const struct String empty = { .is_ref = 1, .str = "", .nbytes = 1 };

    // if src string is null/empty, make an empty string
    if (!(src && src->str && src->nbytes)) {
        src = &empty;
    }

    if (!dst->str || dst->is_ref) {
        // dst string pointer refers to caller-allocated memory.
        // Allocate a new string on the heap.
        CHECK(dst->str = malloc(src->nbytes)); // NOLINT
        dst->nbytes = src->nbytes;
        dst->is_ref = 0; // mark as owned
    }

    CHECK(dst->is_ref == 0);
    if (src->nbytes > dst->nbytes) {
        char* str = realloc(dst->str, src->nbytes);
        if (!str) {
            LOGE("Failed to allocate %llu bytes for string copy.",
                 (unsigned long long)src->nbytes);
            goto Error;
        }
        dst->str = str;
    }

    dst->nbytes = src->nbytes;

    memset(dst->str, 0, dst->nbytes);        // NOLINT
    memcpy(dst->str, src->str, src->nbytes); // NOLINT
    // strings must be null terminated
    if (dst->nbytes > 0)
        dst->str[dst->nbytes - 1] = '\0';
    return 1;
Error:
    return 0;
}

int
storage_properties_set_filename(struct StorageProperties* out,
                                const char* filename,
                                size_t bytes_of_filename)
{
    const struct String s = { .is_ref = 1,
                              .nbytes = bytes_of_filename,
                              .str = (char*)filename };
    return copy_string(&out->filename, &s);
}

int
storage_properties_set_external_metadata(struct StorageProperties* out,
                                         const char* metadata,
                                         size_t bytes_of_metadata)
{
    const struct String s = { .is_ref = 1,
                              .nbytes = bytes_of_metadata,
                              .str = (char*)metadata };
    return copy_string(&out->external_metadata_json, &s);
}

int
storage_properties_set_array_extents(struct StorageProperties* out,
                                     uint32_t x,
                                     uint32_t y,
                                     uint32_t z,
                                     uint32_t c,
                                     uint32_t t)
{
    CHECK(out);
    out->array_extents = (struct storage_properties_array_extents_s){
        x, y, z, c, t,
    };
    return 1;
Error:
    return 0;
}

int
storage_properties_set_chunking_props(struct StorageProperties* out,
                                      uint32_t x,
                                      uint32_t y,
                                      uint32_t z,
                                      uint32_t c,
                                      uint32_t t,
                                      enum AppendDimension append_dimension)
{
    CHECK(out);
    CHECK(append_dimension < AppendDimensionCount);
    out->chunk_shape = (struct storage_properties_chunk_shape_s){
        x, y, z, c, t,
    };
    out->append_dimension = append_dimension;
    return 1;
Error:
    return 0;
}

int
storage_properties_set_sharding_props(struct StorageProperties* out,
                                      uint32_t x,
                                      uint32_t y,
                                      uint32_t z,
                                      uint32_t c,
                                      uint32_t t)
{
    CHECK(out);
    out->shard_shape_chunks =
      (struct storage_properties_shard_shape_s){ x, y, z, c, t };
    return 1;
Error:
    return 0;
}

int
storage_properties_set_enable_multiscale(struct StorageProperties* out,
                                         uint8_t enable)
{
    CHECK(out);
    out->enable_multiscale = enable;
    return 1;
Error:
    return 0;
}

int
storage_properties_init(struct StorageProperties* out,
                        uint32_t first_frame_id,
                        const char* filename,
                        size_t bytes_of_filename,
                        const char* metadata,
                        size_t bytes_of_metadata,
                        struct PixelScale pixel_scale_um)
{
    // Allocate and copy filename
    memset(out, 0, sizeof(*out)); // NOLINT
    CHECK(storage_properties_set_filename(out, filename, bytes_of_filename));
    CHECK(storage_properties_set_external_metadata(
      out, metadata, bytes_of_metadata));
    out->first_frame_id = first_frame_id;
    out->pixel_scale_um = pixel_scale_um;

    return 1;
Error:
    return 0;
}

int
storage_properties_copy(struct StorageProperties* dst,
                        const struct StorageProperties* src)
{
    // 1. Copy everything except the strings
    {
        struct String tmp_fname, tmp_meta;
        memcpy(&tmp_fname, &dst->filename, sizeof(struct String)); // NOLINT
        memcpy(&tmp_meta,                                          // NOLINT
               &dst->external_metadata_json,
               sizeof(struct String));
        memcpy(dst, src, sizeof(*dst));                            // NOLINT
        memcpy(&dst->filename, &tmp_fname, sizeof(struct String)); // NOLINT
        memcpy(&dst->external_metadata_json,                       // NOLINT
               &tmp_meta,
               sizeof(struct String));
    }

    // 2. Reallocate and copy the Strings
    CHECK(copy_string(&dst->filename, &src->filename));
    CHECK(
      copy_string(&dst->external_metadata_json, &src->external_metadata_json));

    return 1;
Error:
    return 0;
}

void
storage_properties_destroy(struct StorageProperties* self)
{
    struct String* const strings[] = { &self->filename,
                                       &self->external_metadata_json };
    for (int i = 0; i < countof(strings); ++i) {
        if (strings[i]->is_ref == 0 && strings[i]->str) {
            free(strings[i]->str);
            memset(strings[i], 0, sizeof(struct String)); // NOLINT
        }
    }
}

#ifndef NO_UNIT_TESTS

/// Check that a==b
/// example: `ASSERT_EQ(int,"%d",42,meaning_of_life())`
#define ASSERT_EQ(T, fmt, a, b)                                                \
    do {                                                                       \
        T a_ = (T)(a);                                                         \
        T b_ = (T)(b);                                                         \
        EXPECT(a_ == b_, "Expected %s==%s but " fmt "!=" fmt, #a, #b, a_, b_); \
    } while (0)

int
unit_test__storage__storage_property_string_check()
{
    struct StorageProperties props;
    {
        const char filename[] = "out.tif";
        const char metadata[] = "{\"hello\":\"world\"}";
        const struct PixelScale pixel_scale_um = { 1, 2 };
        CHECK(storage_properties_init(&props,
                                      0,
                                      filename,
                                      sizeof(filename),
                                      metadata,
                                      sizeof(metadata),
                                      pixel_scale_um));
        CHECK(props.filename.str[props.filename.nbytes - 1] == '\0');
        ASSERT_EQ(int, "%d", props.filename.nbytes, sizeof(filename));
        ASSERT_EQ(int, "%d", props.filename.is_ref, 0);

        CHECK(props.external_metadata_json
                .str[props.external_metadata_json.nbytes - 1] == '\0');
        ASSERT_EQ(
          int, "%d", props.external_metadata_json.nbytes, sizeof(metadata));
        ASSERT_EQ(int, "%d", props.external_metadata_json.is_ref, 0);
        ASSERT_EQ(double, "%g", props.pixel_scale_um.x, 1);
        ASSERT_EQ(double, "%g", props.pixel_scale_um.y, 2);
    }

    {
        const char filename[] = "longer_file_name.tif";
        const char metadata[] = "{\"hello\":\"world\"}";
        const struct PixelScale pixel_scale_um = { 1, 2 };
        struct StorageProperties src;
        CHECK( // NOLINT
          storage_properties_init(&src,
                                  0,
                                  filename,
                                  sizeof(filename),
                                  metadata,
                                  sizeof(metadata),
                                  pixel_scale_um));
        CHECK(src.filename.str[src.filename.nbytes - 1] == '\0');
        CHECK(src.filename.nbytes == sizeof(filename));
        CHECK(src.filename.is_ref == 0);
        CHECK(src.pixel_scale_um.x == 1);
        CHECK(src.pixel_scale_um.y == 2);

        CHECK(src.external_metadata_json
                .str[src.external_metadata_json.nbytes - 1] == '\0');
        CHECK(src.external_metadata_json.nbytes == sizeof(metadata));
        CHECK(src.external_metadata_json.is_ref == 0);

        CHECK(storage_properties_copy(&props, &src));
        storage_properties_destroy(&src);
        CHECK(props.filename.str[props.filename.nbytes - 1] == '\0');
        CHECK(props.filename.nbytes == sizeof(filename));
        CHECK(props.filename.is_ref == 0);

        CHECK(props.external_metadata_json
                .str[props.external_metadata_json.nbytes - 1] == '\0');
        CHECK(props.external_metadata_json.nbytes == sizeof(metadata));
        CHECK(props.external_metadata_json.is_ref == 0);
        CHECK(props.pixel_scale_um.x == 1);
        CHECK(props.pixel_scale_um.y == 2);
    }
    storage_properties_destroy(&props);
    return 1;
Error:
    storage_properties_destroy(&props);
    return 0;
}

int
unit_test__storage__copy_string()
{
    const char* abcde = "abcde";
    const char* vwxyz = "vwxyz";
    const char* fghi = "fghi";
    const char* jklmno = "jklmno";

    struct String src = { .str = (char*)malloc(strlen(abcde) + 1),
                          .nbytes = strlen(abcde) + 1,
                          .is_ref = 1 };
    struct String dst = { .str = (char*)malloc(strlen(vwxyz) + 1),
                          .nbytes = strlen(vwxyz) + 1,
                          .is_ref = 0 };
    CHECK(src.str);
    CHECK(dst.str);

    // dst is_ref = 1
    // lengths equal
    memcpy(src.str, abcde, strlen(abcde) + 1);
    memcpy(dst.str, vwxyz, strlen(vwxyz) + 1);
    CHECK(copy_string(&dst, &src));
    // src should be unchanged
    CHECK(0 == strcmp(src.str, abcde));
    CHECK(strlen(abcde) + 1 == src.nbytes);
    CHECK(1 == src.is_ref);

    // dst should be identical to src, except is_ref
    CHECK(0 == strcmp(dst.str, src.str));
    CHECK(dst.nbytes == src.nbytes);
    CHECK(0 == dst.is_ref); // no matter what happens, this String is owned

    // copy longer to shorter
    memcpy(dst.str, fghi, strlen(fghi) + 1);
    dst.nbytes = strlen(fghi) + 1;
    dst.is_ref = 1;

    CHECK(copy_string(&dst, &src));
    CHECK(0 == strcmp(dst.str, src.str));
    CHECK(dst.nbytes == src.nbytes);
    CHECK(0 == dst.is_ref); // no matter what happens, this String is owned

    // copy shorter to longer
    memcpy(dst.str, jklmno, strlen(jklmno) + 1);
    dst.nbytes = strlen(jklmno) + 1;
    dst.is_ref = 1;

    CHECK(copy_string(&dst, &src));
    CHECK(0 == strcmp(dst.str, src.str));
    CHECK(dst.nbytes == src.nbytes);
    CHECK(0 == dst.is_ref); // no matter what happens, this String is owned

    // dst is_ref = 0
    // lengths equal
    memcpy(dst.str, vwxyz, strlen(vwxyz) + 1);
    dst.nbytes = strlen(vwxyz) + 1;
    dst.is_ref = 0;

    CHECK(copy_string(&dst, &src));
    // src should be unchanged
    CHECK(0 == strcmp(src.str, abcde));
    CHECK(strlen(abcde) + 1 == src.nbytes);
    CHECK(1 == src.is_ref);

    // dst should be identical to src, except is_ref
    CHECK(0 == strcmp(dst.str, src.str));
    CHECK(dst.nbytes == src.nbytes);
    CHECK(0 == dst.is_ref); // no matter what happens, this String is owned

    // copy longer to shorter
    memcpy(dst.str, fghi, strlen(fghi) + 1);
    dst.nbytes = strlen(fghi) + 1;
    dst.is_ref = 0;

    CHECK(copy_string(&dst, &src));
    CHECK(0 == strcmp(dst.str, src.str));
    CHECK(dst.nbytes == src.nbytes);
    CHECK(0 == dst.is_ref); // no matter what happens, this String is owned

    // copy shorter to longer
    free(dst.str);
    CHECK(dst.str = malloc(strlen(jklmno) + 1));
    memcpy(dst.str, jklmno, strlen(jklmno) + 1);
    dst.nbytes = strlen(jklmno) + 1;
    dst.is_ref = 0;

    CHECK(copy_string(&dst, &src));
    CHECK(0 == strcmp(dst.str, src.str));
    CHECK(dst.nbytes == src.nbytes);
    CHECK(0 == dst.is_ref); // no matter what happens, this String is owned

    free(src.str);
    free(dst.str);

    return 1;
Error:
    return 0;
}

int
unit_test__storage_properties_set_array_extents()
{
    struct StorageProperties props = { 0 };

    const uint32_t x = 4, y = 3, z = 2, c = 1, t = 0;
    CHECK(storage_properties_set_array_extents(&props, x, y, z, c, t));

    CHECK(x == props.array_extents.x);
    CHECK(y == props.array_extents.y);
    CHECK(z == props.array_extents.z);
    CHECK(c == props.array_extents.c);
    CHECK(t == props.array_extents.t);

    storage_properties_destroy(&props);

    return 1;
Error:
    return 0;
}

int
unit_test__storage_properties_set_chunking_props()
{
    struct StorageProperties props = { 0 };

    const uint32_t x = 5, y = 4, z = 3, c = 2, t = 1;
    CHECK(storage_properties_set_chunking_props(
      &props, x, y, z, c, t, AppendDimension_c));

    CHECK(x == props.chunk_shape.x);
    CHECK(y == props.chunk_shape.y);
    CHECK(z == props.chunk_shape.z);
    CHECK(c == props.chunk_shape.c);
    CHECK(t == props.chunk_shape.t);
    CHECK(props.append_dimension == AppendDimension_c);

    storage_properties_destroy(&props);

    return 1;
Error:
    return 0;
}

int
unit_test__storage_properties_default_append_dimension_is_t()
{
    struct StorageProperties props = { 0 };
    CHECK(props.append_dimension == AppendDimension_t);
    return 1;
Error:
    return 0;
}

int
unit_test__storage_properties_set_sharding_props()
{
    struct StorageProperties props = { 0 };

    const uint32_t x = 5, y = 4, z = 3, c = 2, t = 1;
    CHECK(storage_properties_set_sharding_props(&props, x, y, z, c, t));

    CHECK(x == props.shard_shape_chunks.x);
    CHECK(y == props.shard_shape_chunks.y);
    CHECK(z == props.shard_shape_chunks.z);
    CHECK(c == props.shard_shape_chunks.c);
    CHECK(t == props.shard_shape_chunks.t);

    storage_properties_destroy(&props);

    return 1;
Error:
    return 0;
}
#endif
