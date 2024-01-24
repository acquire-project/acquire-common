#ifndef H_ACQUIRE_PROPS_STORAGE_V0
#define H_ACQUIRE_PROPS_STORAGE_V0

#include "components.h"
#include "metadata.h"

#ifdef __cplusplus
extern "C"
{
#endif
    struct DeviceManager;
    struct VideoFrame;

    enum DimensionType {
        DimensionType_None = 0,
        DimensionType_Spatial,
        DimensionType_Channel,
        DimensionType_Time,
        DimensionTypeCount
    };

    /// Properties for a storage driver.
    struct StorageProperties
    {
        struct String filename;
        struct String external_metadata_json;
        uint32_t first_frame_id;
        struct PixelScale pixel_scale_um;

        /// Dimensions of the output array, with array extents, chunk sizes, and shard sizes.
        /// The first dimension is the fastest varying dimension. The last dimension is the append dimension.
        struct Dimension {
            struct String name;  // the name of the dimension as it appears in the metadata, e.g., "x", "y", "z", "c", "t"
            enum DimensionType kind;  // the type of dimension, e.g., spatial, channel, time
            uint32_t array_size_px;  // the expected size of the full output array along this dimension
            uint32_t chunk_size_px;  // the size of a chunk along this dimension
            uint32_t shard_size_chunks;  // the number of chunks in a shard along this dimension
        } dimensions[8];

        uint8_t append_dimension;

        /// Enable multiscale storage if true.
        uint8_t enable_multiscale;
    };

    struct StoragePropertyMetadata
    {
        struct storage_property_metadata_multiscale_s
        {
            uint8_t is_supported;
        } multiscale;
    };

    /// Initializes StorageProperties, allocating string storage on the heap
    /// and filling out the struct fields.
    /// @returns 0 when `bytes_of_out` is not large enough, otherwise 1.
    /// @param[out] out The constructed StorageProperties object.
    /// @param[in] first_frame_id (unused; aiming for future file rollover
    /// support
    /// @param[in] filename A c-style null-terminated string. The file to create
    ///                     for streaming.
    /// @param[in] bytes_of_filename Number of bytes in the `filename` buffer
    ///                              including the terminating null.
    /// @param[in] metadata A c-style null-terminated string. Metadata string
    ///                     to save along side the created file.
    /// @param[in] bytes_of_metadata Number of bytes in the `metadata` buffer
    ///                              including the terminating null.
    /// @param[in] pixel_scale_um The pixel scale or size in microns.
    int storage_properties_init(struct StorageProperties* out,
                                uint32_t first_frame_id,
                                const char* filename,
                                size_t bytes_of_filename,
                                const char* metadata,
                                size_t bytes_of_metadata,
                                struct PixelScale pixel_scale_um);

    /// Copies contents, reallocating string storage if necessary.
    /// @returns 1 on success, otherwise 0
    /// @param[in,out] dst Must be zero initialized or previously initialized
    ///                    via `storage_properties_init()`
    /// @param[in] src Copied to `dst`
    int storage_properties_copy(struct StorageProperties* dst,
                                const struct StorageProperties* src);

    /// @brief Set the filename string in `out`.
    /// Copies the string into storage owned by the properties struct.
    /// @returns 1 on success, otherwise 0
    /// @param[in,out] out The storage properties to change.
    /// @param[in] filename pointer to the beginning of the filename buffer.
    /// @param[in] bytes_of_filename the number of bytes in the filename buffer.
    ///                              Should include the terminating NULL.
    int storage_properties_set_filename(struct StorageProperties* out,
                                        const char* filename,
                                        size_t bytes_of_filename);

    /// @brief Set the metadata string in `out`.
    /// Copies the string into storage owned by the properties struct.
    /// @returns 1 on success, otherwise 0
    /// @param[in,out] out The storage properties to change.
    /// @param[in] metadata pointer to the beginning of the metadata buffer.
    /// @param[in] bytes_of_filename the number of bytes in the metadata buffer.
    ///                              Should include the terminating NULL.
    int storage_properties_set_external_metadata(struct StorageProperties* out,
                                                 const char* metadata,
                                                 size_t bytes_of_metadata);

    int storage_properties_insert_dimension(struct StorageProperties* out,
                                            uint32_t index,
                                            const char* name,
                                            enum DimensionType kind,
                                            uint32_t array_size_px,
                                            uint32_t chunk_size_px,
                                            uint32_t shard_size_chunks);

    int storage_properties_remove_dimension(struct StorageProperties* out,
                                            uint32_t index);

    int storage_properties_dimension_count(const struct StorageProperties* out);

    /// @brief Set the expected array dimensions for `out`.
    /// Convenience function to set array extents in a single call.
    /// @returns 1 on success, otherwise 0
    /// @param[in, out] out The storage properties to change.
    /// @param[in] x The width, in px, of the output array.
    /// @param[in] y The height, in px, of the output array.
    /// @param[in] z The expected number of planes per channel per time point in
    /// the output array. If z is not the append dimension, 0 indicates that
    /// this dimension should be skipped.
    /// @param[in] c The expected number of channels in the output array. If c
    /// is not the append dimension, 0 indicates that this dimension should be
    /// skipped.
    /// @param[in] t The expected number of time points in the output array. If
    /// t is not the append dimension, 0 indicates that this dimension should be
    /// skipped.
    int storage_properties_set_array_extents(struct StorageProperties* out,
                                             uint32_t x,
                                             uint32_t y,
                                             uint32_t z,
                                             uint32_t c,
                                             uint32_t t);

    /// @brief Set chunking properties for `out`.
    /// Convenience function to set chunking properties in a single call.
    /// @returns 1 on success, otherwise 0
    /// @param[in, out] out The storage properties to change.
    /// @param[in] x The width, in px, of a chunk.
    /// @param[in] y The height, in px, of a chunk.
    /// @param[in] z The number of @p chunk_width x @p chunk_height
    ///            planes per channel per time point in a single chunk.
    /// @param[in] c The number of channels in a single chunk.
    /// @param[in] t The number of time points in a single chunk.
    /// @param[in] append_dimension The dimension to append frames along.
    int storage_properties_set_chunking_props(
      struct StorageProperties* out,
      uint32_t x,
      uint32_t y,
      uint32_t z,
      uint32_t c,
      uint32_t t,
      enum AppendDimension append_dimension);

    /// @brief Set sharding properties for `out`.
    /// Convenience function to set sharding properties in a single call.
    /// @returns 1 on success, otherwise 0
    /// @param[in, out] out The storage properties to change.
    /// @param[in] x The number of chunks in a shard along the x dimension.
    /// @param[in] y The number of chunks in a shard along the y dimension.
    /// @param[in] z The number of chunks in a shard along the z dimension.
    /// @param[in] c The number of chunks in a shard along the c dimension.
    /// @param[in] t The number of chunks in a shard along the t dimension.
    int storage_properties_set_sharding_props(struct StorageProperties* out,
                                              uint32_t x,
                                              uint32_t y,
                                              uint32_t z,
                                              uint32_t c,
                                              uint32_t t);

    /// @brief Set multiscale properties for `out`.
    /// Convenience function to enable multiscale.
    /// @returns 1 on success, otherwise 0
    /// @param[in, out] out The storage properties to change.
    /// @param[in] enable A flag to enable or disable multiscale.
    int storage_properties_set_enable_multiscale(struct StorageProperties* out,
                                                 uint8_t enable);

    /// Free's allocated string storage.
    void storage_properties_destroy(struct StorageProperties* self);

#ifdef __cplusplus
}
#endif

#endif // H_ACQUIRE_PROPS_STORAGE_V0
