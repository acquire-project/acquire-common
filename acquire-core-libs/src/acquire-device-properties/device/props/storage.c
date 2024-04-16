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

static const char*
dimension_type_as_string(enum DimensionType type)
{
    switch (type) {
        case DimensionType_Space:
            return "Spatial";
        case DimensionType_Channel:
            return "Channel";
        case DimensionType_Time:
            return "Time";
        case DimensionType_Other:
            return "Other";
        default:
            return "(unknown)";
    }
}

static int
storage_dimension_array_init(struct StorageDimension** data, size_t size)
{
    if (size == 0) {
        *data = 0;
        return 1;
    }

    CHECK(*data = malloc(size * sizeof(struct StorageDimension)));
    memset(*data, 0, size * sizeof(struct StorageDimension)); // NOLINT
    return 1;
Error:
    return 0;
}

static int
storage_properties_dimensions_init(struct StorageProperties* self, size_t size)
{
    CHECK(self);
    CHECK(size > 0);
    CHECK(self->acquisition_dimensions.data == 0);

    CHECK(
      storage_dimension_array_init(&self->acquisition_dimensions.data, size));
    CHECK(self->acquisition_dimensions.data);

    self->acquisition_dimensions.size = size;

    return 1;
Error:
    return 0;
}

static int
storage_dimension_copy(struct StorageDimension* dst,
                       const struct StorageDimension* src)
{
    CHECK(dst);
    CHECK(src);

    CHECK(copy_string(&dst->name, &src->name));
    dst->kind = src->kind;
    dst->array_size_px = src->array_size_px;
    dst->chunk_size_px = src->chunk_size_px;
    dst->shard_size_chunks = src->shard_size_chunks;

    return 1;
Error:
    return 0;
}

static void
storage_dimension_destroy(struct StorageDimension* self)
{
    CHECK(self);

    if (self->name.is_ref == 0 && self->name.str) {
        free(self->name.str);
    }

    memset(self, 0, sizeof(*self)); // NOLINT
Error:;
}

static void
storage_properties_dimensions_destroy(struct StorageProperties* self)
{
    CHECK(self);
    CHECK(self->acquisition_dimensions.data);

    // destroy each dimension
    for (int i = 0; i < self->acquisition_dimensions.size; ++i) {
        storage_dimension_destroy(&self->acquisition_dimensions.data[i]);
    }

    // destroy the array
    free(self->acquisition_dimensions.data);

    memset(
      &self->acquisition_dimensions, 0, sizeof(self->acquisition_dimensions));
Error:;
}

int
storage_properties_set_uri(struct StorageProperties* out,
                           const char* uri,
                           size_t bytes_of_uri)
{
    const struct String s = { .is_ref = 1,
                              .nbytes = bytes_of_uri,
                              .str = (char*)uri };
    return copy_string(&out->uri, &s);
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
storage_properties_set_access_key_and_secret(struct StorageProperties* out,
                                             const char* access_key_id,
                                             size_t bytes_of_access_key_id,
                                             const char* secret_access_key,
                                             size_t bytes_of_secret_access_key)
{
    const struct String s = { .is_ref = 1,
                              .nbytes = bytes_of_access_key_id,
                              .str = (char*)access_key_id };
    CHECK(copy_string(&out->access_key_id, &s));

    const struct String t = { .is_ref = 1,
                              .nbytes = bytes_of_secret_access_key,
                              .str = (char*)secret_access_key };
    return copy_string(&out->secret_access_key, &t);
Error:
    return 0;
}

int
storage_properties_set_dimension(struct StorageProperties* out,
                                 int index,
                                 const char* name,
                                 size_t bytes_of_name,
                                 enum DimensionType kind,
                                 uint32_t array_size_px,
                                 uint32_t chunk_size_px,
                                 uint32_t shard_size_chunks)
{
    CHECK(out);

    EXPECT(index < out->acquisition_dimensions.size,
           "Index %d out of range [0,%d).",
           index,
           out->acquisition_dimensions.size);

    EXPECT(name, "Dimension name cannot be null.");
    EXPECT(bytes_of_name > 0, "Bytes of name must be positive.");
    EXPECT(strlen(name) > 0, "Dimension name cannot be empty.");
    EXPECT(kind < DimensionTypeCount,
           "Invalid dimension type: %s.",
           dimension_type_as_string(kind));

    struct StorageDimension* dim = &out->acquisition_dimensions.data[index];

    memset(dim, 0, sizeof(*dim)); // NOLINT

    struct String s = { .is_ref = 1,
                        .nbytes = bytes_of_name,
                        .str = (char*)name };
    CHECK(copy_string(&dim->name, &s));

    dim->kind = kind;
    dim->array_size_px = array_size_px;
    dim->chunk_size_px = chunk_size_px;
    dim->shard_size_chunks = shard_size_chunks;

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
                        const char* uri,
                        size_t bytes_of_uri,
                        const char* metadata,
                        size_t bytes_of_metadata,
                        struct PixelScale pixel_scale_um,
                        uint8_t dimension_count)
{
    // Allocate and copy filename
    memset(out, 0, sizeof(*out)); // NOLINT
    CHECK(storage_properties_set_uri(out, uri, bytes_of_uri));

    // Set external metadata
    CHECK(storage_properties_set_external_metadata(
      out, metadata, bytes_of_metadata));

    out->first_frame_id = first_frame_id;
    out->pixel_scale_um = pixel_scale_um;

    // Initialize the dimensions array
    if (dimension_count > 0) {
        CHECK(storage_properties_dimensions_init(out, dimension_count));
    }

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
        struct String tmp_uri, tmp_meta, tmp_access_key, tmp_secret_key;
        memcpy(&tmp_uri, &dst->uri, sizeof(struct String)); // NOLINT
        memcpy(&tmp_meta,                                   // NOLINT
               &dst->external_metadata_json,
               sizeof(struct String));
        memcpy(&tmp_access_key,
               &dst->access_key_id,
               sizeof(struct String)); // NOLINT
        memcpy(&tmp_secret_key,
               &dst->secret_access_key,
               sizeof(struct String)); // NOLINT

        memcpy(dst, src, sizeof(*dst));                     // NOLINT
        memcpy(&dst->uri, &tmp_uri, sizeof(struct String)); // NOLINT
        memcpy(&dst->external_metadata_json,                // NOLINT
               &tmp_meta,
               sizeof(struct String));
        memcpy(&dst->access_key_id,
               &tmp_access_key,
               sizeof(struct String)); // NOLINT
        memcpy(&dst->secret_access_key,
               &tmp_secret_key,
               sizeof(struct String)); // NOLINT
    }

    // 2. Reallocate and copy the Strings
    CHECK(copy_string(&dst->uri, &src->uri));
    CHECK(
      copy_string(&dst->external_metadata_json, &src->external_metadata_json));
    CHECK(copy_string(&dst->access_key_id, &src->access_key_id));
    CHECK(copy_string(&dst->secret_access_key, &src->secret_access_key));

    // 3. Copy the dimensions
    if (src->acquisition_dimensions.data) {
        storage_properties_dimensions_destroy(dst);

        CHECK(storage_properties_dimensions_init(
          dst, src->acquisition_dimensions.size));
        for (size_t i = 0; i < src->acquisition_dimensions.size; ++i) {
            CHECK(storage_dimension_copy(&dst->acquisition_dimensions.data[i],
                                         &src->acquisition_dimensions.data[i]));
        }
    }

    return 1;
Error:
    return 0;
}

void
storage_properties_destroy(struct StorageProperties* self)
{
    struct String* const strings[] = { &self->uri,
                                       &self->external_metadata_json,
                                       &self->access_key_id,
                                       &self->secret_access_key };
    for (int i = 0; i < countof(strings); ++i) {
        if (strings[i]->is_ref == 0 && strings[i]->str) {
            free(strings[i]->str);
            memset(strings[i], 0, sizeof(struct String)); // NOLINT
        }
    }

    storage_properties_dimensions_destroy(self);
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
                                      pixel_scale_um,
                                      0));
        CHECK(props.uri.str[props.uri.nbytes - 1] == '\0');
        ASSERT_EQ(int, "%d", props.uri.nbytes, sizeof(filename));
        ASSERT_EQ(int, "%d", props.uri.is_ref, 0);

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
                                  pixel_scale_um,
                                  0));
        CHECK(src.uri.str[src.uri.nbytes - 1] == '\0');
        CHECK(src.uri.nbytes == sizeof(filename));
        CHECK(src.uri.is_ref == 0);
        CHECK(src.pixel_scale_um.x == 1);
        CHECK(src.pixel_scale_um.y == 2);

        CHECK(src.external_metadata_json
                .str[src.external_metadata_json.nbytes - 1] == '\0');
        CHECK(src.external_metadata_json.nbytes == sizeof(metadata));
        CHECK(src.external_metadata_json.is_ref == 0);

        CHECK(storage_properties_copy(&props, &src));
        storage_properties_destroy(&src);
        CHECK(props.uri.str[props.uri.nbytes - 1] == '\0');
        CHECK(props.uri.nbytes == sizeof(filename));
        CHECK(props.uri.is_ref == 0);

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
unit_test__storage_properties_set_access_key_and_secret()
{
    struct StorageProperties props = { 0 };
    const char access_key_id[] = "access_key_id";
    const char secret_access_key[] = "secret_access_key";

    CHECK(
      storage_properties_set_access_key_and_secret(&props,
                                                   access_key_id,
                                                   sizeof(access_key_id),
                                                   secret_access_key,
                                                   sizeof(secret_access_key)));

    CHECK(0 == strcmp(props.access_key_id.str, access_key_id));
    CHECK(props.access_key_id.nbytes == sizeof(access_key_id));
    CHECK(0 == props.access_key_id.is_ref);

    CHECK(0 == strcmp(props.secret_access_key.str, secret_access_key));
    CHECK(props.secret_access_key.nbytes == sizeof(secret_access_key));
    CHECK(0 == props.secret_access_key.is_ref);

    return 1;
Error:
    return 0;
}

int
unit_test__dimension_init()
{
    struct StorageProperties props = { 0 };
    CHECK(storage_properties_dimensions_init(&props, 1));

    struct StorageDimension* dim = &props.acquisition_dimensions.data[0];

    // can't set with a null char pointer
    CHECK(!storage_properties_set_dimension(
      &props, 0, NULL, 0, DimensionType_Space, 1, 1, 1));
    CHECK(dim->name.str == NULL);
    CHECK(dim->kind == DimensionType_Space);
    CHECK(dim->array_size_px == 0);
    CHECK(dim->chunk_size_px == 0);
    CHECK(dim->shard_size_chunks == 0);

    // can't set with 0 bytes
    CHECK(!storage_properties_set_dimension(
      &props, 0, "", 0, DimensionType_Space, 1, 1, 1));
    CHECK(dim->name.str == NULL);
    CHECK(dim->kind == DimensionType_Space);
    CHECK(dim->array_size_px == 0);
    CHECK(dim->chunk_size_px == 0);
    CHECK(dim->shard_size_chunks == 0);

    // can't set with an empty name
    CHECK(!storage_properties_set_dimension(
      &props, 0, "", 1, DimensionType_Space, 1, 1, 1));
    CHECK(dim->name.str == NULL);
    CHECK(dim->kind == DimensionType_Space);
    CHECK(dim->array_size_px == 0);
    CHECK(dim->chunk_size_px == 0);
    CHECK(dim->shard_size_chunks == 0);

    // can't set with an invalid dimension type
    CHECK(!storage_properties_set_dimension(
      &props, 0, "x", 2, DimensionTypeCount, 1, 1, 1));
    CHECK(dim->name.str == NULL);
    CHECK(dim->kind == DimensionType_Space);
    CHECK(dim->array_size_px == 0);
    CHECK(dim->chunk_size_px == 0);
    CHECK(dim->shard_size_chunks == 0);

    // can't set beyond the size of the array
    CHECK(!storage_properties_set_dimension(
      &props, 1, "x", 2, DimensionType_Space, 1, 1, 1));
    CHECK(dim->name.str == NULL);
    CHECK(dim->kind == DimensionType_Space);
    CHECK(dim->array_size_px == 0);
    CHECK(dim->chunk_size_px == 0);
    CHECK(dim->shard_size_chunks == 0);

    // set with valid values
    CHECK(storage_properties_set_dimension(
      &props, 0, "x", 2, DimensionType_Space, 1, 1, 1));
    CHECK(0 == strcmp(dim->name.str, "x"));
    CHECK(dim->kind == DimensionType_Space);
    CHECK(dim->array_size_px == 1);
    CHECK(dim->chunk_size_px == 1);
    CHECK(dim->shard_size_chunks == 1);

    return 1;
Error:
    return 0;
}

int
unit_test__storage_properties_dimensions_init()
{
    struct StorageProperties props = { 0 };
    CHECK(storage_properties_dimensions_init(&props, 5));

    CHECK(props.acquisition_dimensions.size == 5);
    CHECK(props.acquisition_dimensions.data != NULL);

    free(props.acquisition_dimensions.data);

    return 1;
Error:
    return 0;
}

int
unit_test__storage_properties_dimensions_destroy()
{
    struct StorageProperties props = { 0 };

    props.acquisition_dimensions.data =
      malloc(5 * sizeof(struct StorageDimension));
    props.acquisition_dimensions.size = 5;
    memset(props.acquisition_dimensions.data,
           0,
           5 * sizeof(struct StorageDimension));

    storage_properties_dimensions_destroy(&props);
    CHECK(props.acquisition_dimensions.size == 0);
    CHECK(props.acquisition_dimensions.data == NULL);

    return 1;
Error:
    return 0;
}

int
unit_test__dimension_type_as_string__is_defined_for_all()
{
    for (int i = 0; i < DimensionTypeCount; ++i) {
        // Check this isn't returning "(unknown)" for known values
        CHECK(dimension_type_as_string(i)[0] != '(');
    }
    return 1;
Error:
    return 0;
}
#endif
