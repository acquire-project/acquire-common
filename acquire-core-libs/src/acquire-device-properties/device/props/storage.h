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

    enum DimensionType
    {
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

        /// Dimensions of the output array, with array extents, chunk sizes, and
        /// shard sizes. The first dimension is the fastest varying dimension.
        /// The last dimension is the append dimension.
        struct storage_properties_dimension_s
        {
            // the name of the dimension as it appears in the metadata, e.g.,
            // "x", "y", "z", "c", "t"
            struct String name;

            // the type of dimension, e.g., spatial, channel, time
            enum DimensionType kind;

            // the expected size of the full output array along this dimension
            uint32_t array_size_px;

            // the size of a chunk along this dimension
            uint32_t chunk_size_px;

            // the number of chunks in a shard along this dimension
            uint32_t shard_size_chunks;
        } acquisition_dimensions[8];

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

    /// @brief Initialize the dimension struct in `out`.
    /// @param[out] out The dimension struct to initialize.
    /// @param[in] name The name of the dimension.
    /// @param[in] bytes_of_name The number of bytes in the name buffer.
    ///                          Should include the terminating NULL.
    /// @param[in] kind The type of dimension.
    /// @param[in] array_size_px The size of the array along this dimension.
    /// @param[in] chunk_size_px The size of a chunk along this dimension.
    /// @param[in] shard_size_chunks The number of chunks in a shard along this
    ///                              dimension.
    /// @returns 1 on success, otherwise 0
    int storage_properties_dimension_init(
      struct storage_properties_dimension_s* out,
      const char* name,
      size_t bytes_of_name,
      enum DimensionType kind,
      uint32_t array_size_px,
      uint32_t chunk_size_px,
      uint32_t shard_size_chunks);

    /// @brief Insert a dimension into `out` at `index`.
    /// @param[out] out The storage properties containing the dimension array.
    /// @param[in] index The index at which to insert the dimension.
    /// @param[in] name The name of the dimension.
    /// @param[in] bytes_of_name The number of bytes in the name buffer.
    ///                          Should include the terminating NULL.
    /// @param[in] kind The type of dimension.
    /// @param[in] array_size_px The size of the array along this dimension.
    /// @param[in] chunk_size_px The size of a chunk along this dimension.
    /// @param[in] shard_size_chunks The number of chunks in a shard along this
    ///                              dimension.
    /// @returns 1 on success, otherwise 0
    int storage_properties_insert_dimension(struct StorageProperties* out,
                                            uint32_t index,
                                            const char* name,
                                            size_t bytes_of_name,
                                            enum DimensionType kind,
                                            uint32_t array_size_px,
                                            uint32_t chunk_size_px,
                                            uint32_t shard_size_chunks);

    /// @brief Remove a dimension from `out` at `index`.
    /// @param[out] out The storage properties containing the dimension array.
    /// @param[in] index The index at which to remove the dimension.
    /// @returns 1 on success, otherwise 0
    int storage_properties_remove_dimension(struct StorageProperties* out,
                                            uint32_t index);

    /// @brief Get the number of dimensions in `out`.
    /// @param[in] out The storage properties containing the dimension array.
    /// @returns The number of dimensions in `out`.
    int storage_properties_dimension_count(const struct StorageProperties* out);

    /// @brief Get the dimension at `index` in `out`.
    /// @param[in] out The storage properties containing the dimension array.
    /// @param[in] index The index at which to get the dimension.
    /// @param[out] dimension The dimension at `index` in `out`.
    int storage_properties_get_dimension(
      const struct StorageProperties* out,
      uint32_t index,
      struct storage_properties_dimension_s* dimension);

    /// @brief Destroy the dimension struct in `out`.
    /// @param[out] out The dimension struct to destroy.
    void storage_properties_dimension_destroy(
      struct storage_properties_dimension_s* self);

    /// @brief Set the append dimension in `out`.
    /// @note The first two dimensions are reserved for image dimensions and
    /// are not valid append dimensions.
    /// @returns 1 on success, otherwise 0
    int storage_properties_set_append_dimension(struct StorageProperties* out,
                                                uint32_t index);

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
