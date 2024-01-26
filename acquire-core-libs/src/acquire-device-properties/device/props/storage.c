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
storage_properties_insert_dimension(struct StorageProperties* out,
                                    uint32_t index,
                                    const char* name,
                                    size_t bytes_of_name,
                                    enum DimensionType kind,
                                    uint32_t array_size_px,
                                    uint32_t chunk_size_px,
                                    uint32_t shard_size_chunks)
{
    CHECK(out);

    // can only insert at existing dimensions, or append
    CHECK(storage_properties_dimension_count(out) >= index);

    EXPECT(name, "Dimension name cannot be null.");
    EXPECT(bytes_of_name > 0, "Bytes of name must be positive.");
    EXPECT(strlen(name) > 0, "Dimension name cannot be empty.");
    EXPECT(kind > DimensionType_None && kind < DimensionTypeCount,
           "Invalid dimension type: %d.",
           kind);

    EXPECT(storage_properties_dimension_count(out) <
             countof(out->acquisition_dimensions),
           "Cannot insert dimension. Too many dimensions.");

    // shift all dimensions after index up by one
    for (int i = countof(out->acquisition_dimensions) - 1; i > index; --i) {
        out->acquisition_dimensions[i] = out->acquisition_dimensions[i - 1];
    }

    out->acquisition_dimensions[index] =
      (struct storage_properties_dimension_s){
          .kind = kind,
          .array_size_px = array_size_px,
          .chunk_size_px = chunk_size_px,
          .shard_size_chunks = shard_size_chunks,
      };

    struct String s = { .is_ref = 1,
                        .nbytes = bytes_of_name,
                        .str = (char*)name };
    if (!copy_string(&out->acquisition_dimensions[index].name, &s)) {
        out->acquisition_dimensions[index] =
          (struct storage_properties_dimension_s){ 0 };
        goto Error;
    }

    return 1;
Error:
    return 0;
}

int
storage_properties_remove_dimension(struct StorageProperties* out,
                                    uint32_t index)
{
    CHECK(out);
    CHECK(index < storage_properties_dimension_count(out));

    // shift all dimensions after index down by one
    for (int i = index; i < countof(out->acquisition_dimensions) - 1; ++i) {
        out->acquisition_dimensions[i] = out->acquisition_dimensions[i + 1];
    }
    out->acquisition_dimensions[countof(out->acquisition_dimensions) - 1] =
      (struct storage_properties_dimension_s){ 0 };

    return 1;
Error:
    return 0;
}

int
storage_properties_dimension_count(const struct StorageProperties* out)
{
    CHECK(out);
    const int count = countof(out->acquisition_dimensions);

    for (int i = 0; i < count; ++i) {
        if (out->acquisition_dimensions[i].kind == DimensionType_None) {
            return i;
        }
    }
    return count;
Error:
    return 0;
}

int
storage_properties_get_dimension(
  const struct StorageProperties* out,
  uint32_t index,
  struct storage_properties_dimension_s* dimension)
{
    CHECK(out);
    CHECK(index < storage_properties_dimension_count(out));
    CHECK(dimension);

    *dimension = out->acquisition_dimensions[index];

    return 1;
Error:
    return 0;
}

int
storage_properties_set_append_dimension(struct StorageProperties* out,
                                        uint32_t index)
{
    CHECK(out);
    EXPECT(index > 1, "Invalid append dimension: %d.", index);
    EXPECT(index < countof(out->acquisition_dimensions),
           "Invalid dimension: %d.",
           index);

    out->append_dimension = index;

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
    struct String* const strings[] = {
        &self->filename,
        &self->external_metadata_json,
        &self->acquisition_dimensions[0].name,
        &self->acquisition_dimensions[1].name,
        &self->acquisition_dimensions[2].name,
        &self->acquisition_dimensions[3].name,
        &self->acquisition_dimensions[4].name,
        &self->acquisition_dimensions[5].name,
        &self->acquisition_dimensions[6].name,
        &self->acquisition_dimensions[7].name,
    };
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
unit_test__storage_properties_dimension_count()
{
    struct StorageProperties props = { 0 };
    CHECK(storage_properties_dimension_count(&props) == 0);

    props.acquisition_dimensions[0].kind = DimensionType_Spatial;
    CHECK(storage_properties_dimension_count(&props) == 1);

    props.acquisition_dimensions[1].kind = DimensionType_Spatial;
    CHECK(storage_properties_dimension_count(&props) == 2);

    props.acquisition_dimensions[2].kind = DimensionType_Spatial;
    CHECK(storage_properties_dimension_count(&props) == 3);

    props.acquisition_dimensions[3].kind = DimensionType_Channel;
    CHECK(storage_properties_dimension_count(&props) == 4);

    props.acquisition_dimensions[4].kind = DimensionType_Channel;
    CHECK(storage_properties_dimension_count(&props) == 5);

    props.acquisition_dimensions[5].kind = DimensionType_Channel;
    CHECK(storage_properties_dimension_count(&props) == 6);

    props.acquisition_dimensions[6].kind = DimensionType_Channel;
    CHECK(storage_properties_dimension_count(&props) == 7);

    props.acquisition_dimensions[7].kind = DimensionType_Time;
    CHECK(storage_properties_dimension_count(&props) == 8);

    return 1;
Error:
    return 0;
}

int
unit_test__storage_properties_set_append_dimension()
{
    struct StorageProperties props = { 0 };

    // x and y are forbidden
    CHECK(!storage_properties_set_append_dimension(&props, 0));
    CHECK(!storage_properties_set_append_dimension(&props, 1));

    for (int i = 2; i < 8; ++i) {
        CHECK(storage_properties_set_append_dimension(&props, i));
        CHECK(props.append_dimension == i);
    }

    // dimensions higher than 7 don't exist
    CHECK(!storage_properties_set_append_dimension(&props, 8));

    return 1;

Error:
    return 0;
}

int
unit_test__storage_properties_insert_dimension()
{
    struct StorageProperties props = { 0 };

    CHECK(0 == storage_properties_dimension_count(&props));

    // can't skip an entry on insert
    CHECK(!storage_properties_insert_dimension(
      &props, 1, "x", 2, DimensionType_Spatial, 1, 1, 1));

    // can't insert with a null char pointer
    CHECK(!storage_properties_insert_dimension(
      &props, 0, NULL, 0, DimensionType_Spatial, 1, 1, 1));

    // can't insert with an empty name
    CHECK(!storage_properties_insert_dimension(
      &props, 0, "", 0, DimensionType_Spatial, 1, 1, 1));

    // insert at 0
    CHECK(storage_properties_insert_dimension(
      &props, 0, "y", 2, DimensionType_Spatial, 1, 1, 1));
    CHECK(0 == strcmp(props.acquisition_dimensions[0].name.str, "y"));
    CHECK(props.acquisition_dimensions[0].kind == DimensionType_Spatial);
    CHECK(props.acquisition_dimensions[0].array_size_px == 1);
    CHECK(props.acquisition_dimensions[0].chunk_size_px == 1);
    CHECK(props.acquisition_dimensions[0].shard_size_chunks == 1);

    CHECK(1 == storage_properties_dimension_count(&props));

    // insert at 0 again
    CHECK(storage_properties_insert_dimension(
      &props, 0, "x", 2, DimensionType_Spatial, 2, 2, 2));
    CHECK(0 == strcmp(props.acquisition_dimensions[0].name.str, "x"));
    CHECK(props.acquisition_dimensions[0].kind == DimensionType_Spatial);
    CHECK(props.acquisition_dimensions[0].array_size_px == 2);
    CHECK(props.acquisition_dimensions[0].chunk_size_px == 2);
    CHECK(props.acquisition_dimensions[0].shard_size_chunks == 2);

    // pushed the previous entry to 1
    CHECK(0 == strcmp(props.acquisition_dimensions[1].name.str, "y"));
    CHECK(props.acquisition_dimensions[1].kind == DimensionType_Spatial);
    CHECK(props.acquisition_dimensions[1].array_size_px == 1);
    CHECK(props.acquisition_dimensions[1].chunk_size_px == 1);
    CHECK(props.acquisition_dimensions[1].shard_size_chunks == 1);

    CHECK(2 == storage_properties_dimension_count(&props));

    // insert at 2
    CHECK(storage_properties_insert_dimension(
      &props, 2, "z", 2, DimensionType_Spatial, 3, 3, 3));
    CHECK(0 == strcmp(props.acquisition_dimensions[2].name.str, "z"));
    CHECK(props.acquisition_dimensions[2].kind == DimensionType_Spatial);
    CHECK(props.acquisition_dimensions[2].array_size_px == 3);
    CHECK(props.acquisition_dimensions[2].chunk_size_px == 3);
    CHECK(props.acquisition_dimensions[2].shard_size_chunks == 3);

    // check previous entries, ensure no change
    CHECK(0 == strcmp(props.acquisition_dimensions[0].name.str, "x"));
    CHECK(props.acquisition_dimensions[0].kind == DimensionType_Spatial);
    CHECK(props.acquisition_dimensions[0].array_size_px == 2);
    CHECK(props.acquisition_dimensions[0].chunk_size_px == 2);
    CHECK(props.acquisition_dimensions[0].shard_size_chunks == 2);

    CHECK(0 == strcmp(props.acquisition_dimensions[1].name.str, "y"));
    CHECK(props.acquisition_dimensions[1].kind == DimensionType_Spatial);
    CHECK(props.acquisition_dimensions[1].array_size_px == 1);
    CHECK(props.acquisition_dimensions[1].chunk_size_px == 1);
    CHECK(props.acquisition_dimensions[1].shard_size_chunks == 1);

    CHECK(3 == storage_properties_dimension_count(&props));

    // can't insert at 4 (would skip an entry)
    CHECK(!storage_properties_insert_dimension(
      &props, 4, "t", 2, DimensionType_Time, 1, 1, 1));

    return 1;
Error:
    return 0;
}

int
unit_test__storage_properties_remove_dimension()
{
    struct StorageProperties props = { 0 };
    CHECK(0 == storage_properties_dimension_count(&props));

    // can't remove at 0
    CHECK(!storage_properties_remove_dimension(&props, 0));

    // insert at 0
    CHECK(storage_properties_insert_dimension(
      &props, 0, "y", 2, DimensionType_Spatial, 1, 1, 1));
    CHECK(0 == strcmp(props.acquisition_dimensions[0].name.str, "y"));
    CHECK(props.acquisition_dimensions[0].kind == DimensionType_Spatial);
    CHECK(props.acquisition_dimensions[0].array_size_px == 1);
    CHECK(props.acquisition_dimensions[0].chunk_size_px == 1);
    CHECK(props.acquisition_dimensions[0].shard_size_chunks == 1);

    CHECK(1 == storage_properties_dimension_count(&props));

    // remove at 0
    CHECK(storage_properties_remove_dimension(&props, 0));

    CHECK(0 == storage_properties_dimension_count(&props));
    CHECK(NULL == props.acquisition_dimensions[0].name.str);
    CHECK(props.acquisition_dimensions[0].kind == DimensionType_None);
    CHECK(props.acquisition_dimensions[0].array_size_px == 0);
    CHECK(props.acquisition_dimensions[0].chunk_size_px == 0);
    CHECK(props.acquisition_dimensions[0].shard_size_chunks == 0);

    return 1;
Error:
    return 0;
}

int
unit_test__storage_properties_get_dimension()
{
    struct StorageProperties props = { 0 };
    CHECK(0 == storage_properties_dimension_count(&props));

    // trying to get an out of bounds dimension will fail
    struct storage_properties_dimension_s dim = { 0 };
    CHECK(!storage_properties_get_dimension(&props, 0, &dim));

    // check before we alter it
    CHECK(NULL == dim.name.str);
    CHECK(dim.kind == DimensionType_None);
    CHECK(dim.array_size_px == 0);
    CHECK(dim.chunk_size_px == 0);
    CHECK(dim.shard_size_chunks == 0);

    // insert at 0
    CHECK(storage_properties_insert_dimension(
      &props, 0, "x", 2, DimensionType_Spatial, 1, 1, 1));

    // get at 0
    CHECK(storage_properties_get_dimension(&props, 0, &dim));
    CHECK(0 == strcmp(dim.name.str, "x"));
    CHECK(dim.kind == DimensionType_Spatial);
    CHECK(dim.array_size_px == 1);
    CHECK(dim.chunk_size_px == 1);
    CHECK(dim.shard_size_chunks == 1);

    // trying to get an oob dimension will still fail
    CHECK(!storage_properties_get_dimension(&props, 1, &dim));

    // should be unaltered
    CHECK(storage_properties_get_dimension(&props, 0, &dim));
    CHECK(0 == strcmp(dim.name.str, "x"));
    CHECK(dim.kind == DimensionType_Spatial);
    CHECK(dim.array_size_px == 1);
    CHECK(dim.chunk_size_px == 1);
    CHECK(dim.shard_size_chunks == 1);

    return 1;

Error:
    return 0;
}
#endif
