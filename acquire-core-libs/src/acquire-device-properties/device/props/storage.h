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
        DimensionType_Space = 0,
        DimensionType_Channel,
        DimensionType_Time,
        DimensionType_Other,
        DimensionTypeCount
    };

    struct StorageDimension
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
    };

    /// Properties for a storage driver.
    struct StorageProperties
    {
        struct String uri;
        struct String external_metadata_json;
        struct String access_key_id;
        struct String secret_access_key;

        uint32_t first_frame_id;
        struct PixelScale pixel_scale_um;

        /// Dimensions of the output array, with array extents, chunk sizes, and
        /// shard sizes. The first dimension is the fastest varying dimension.
        /// The last dimension is the append dimension.
        struct storage_properties_dimensions_s
        {
            // The dimensions of the output array.
            struct StorageDimension* data;

            // The number of dimensions in the output array.
            size_t size;
        } acquisition_dimensions;

        /// Enable multiscale storage if true.
        uint8_t enable_multiscale;
    };

    struct StoragePropertyMetadata
    {
        uint8_t chunking_is_supported;
        uint8_t sharding_is_supported;
        uint8_t multiscale_is_supported;
        uint8_t s3_is_supported;
    };

    /// Initializes StorageProperties, allocating string storage on the heap
    /// and filling out the struct fields.
    /// @returns 0 when `bytes_of_out` is not large enough, otherwise 1.
    /// @param[out] out The constructed StorageProperties object.
    /// @param[in] first_frame_id (unused; aiming for future file rollover
    /// support
    /// @param[in] uri A c-style null-terminated string. The file to create
    ///                for streaming.
    /// @param[in] bytes_of_uri Number of bytes in the `uri` buffer,
    ///                         including the terminating null.
    /// @param[in] metadata A c-style null-terminated string. Metadata string
    ///                     to save along side the created file.
    /// @param[in] bytes_of_metadata Number of bytes in the `metadata` buffer
    ///                              including the terminating null.
    /// @param[in] pixel_scale_um The pixel scale or size in microns.
    /// @param[in] dimension_count The number of dimensions in the storage
    /// array. Each of the @p dimension_count dimensions will be initialized
    /// to zero.
    int storage_properties_init(struct StorageProperties* out,
                                uint32_t first_frame_id,
                                const char* uri,
                                size_t bytes_of_uri,
                                const char* metadata,
                                size_t bytes_of_metadata,
                                struct PixelScale pixel_scale_um,
                                uint8_t dimension_count);

    /// Copies contents, reallocating string storage if necessary.
    /// @returns 1 on success, otherwise 0
    /// @param[in,out] dst Must be zero initialized or previously initialized
    ///                    via `storage_properties_init()`
    /// @param[in] src Copied to `dst`
    int storage_properties_copy(struct StorageProperties* dst,
                                const struct StorageProperties* src);

    /// @brief Set the uri string in `out`.
    /// Copies the string into storage owned by the properties struct.
    /// @returns 1 on success, otherwise 0
    /// @param[in,out] out The storage properties to change.
    /// @param[in] uri Pointer to the beginning of the uri buffer.
    /// @param[in] bytes_of_uri The number of bytes in the uri buffer.
    ///                         Should include the terminating NULL.
    int storage_properties_set_uri(struct StorageProperties* out,
                                   const char* uri,
                                   size_t bytes_of_uri);

    /// @brief Set the metadata string in `out`.
    /// Copies the string into storage owned by the properties struct.
    /// @returns 1 on success, otherwise 0
    /// @param[in,out] out The storage properties to change.
    /// @param[in] metadata pointer to the beginning of the metadata buffer.
    /// @param[in] bytes_of_metadata the number of bytes in the metadata buffer.
    ///                              Should include the terminating NULL.
    int storage_properties_set_external_metadata(struct StorageProperties* out,
                                                 const char* metadata,
                                                 size_t bytes_of_metadata);

    /// @brief Set the access key id string in `out`.
    /// Copies the string into storage owned by the properties struct.
    /// @returns 1 on success, otherwise 0
    /// @param[in,out] out The storage properties to change.
    /// @param[in] access_key_id Pointer to the beginning of the access key id
    ///                          buffer.
    /// @param[in] bytes_of_access_key_id The number of bytes in the access key
    ///                                   id.
    /// @param[in] secret_access_key Pointer to the beginning of the secret
    ///                              access key buffer.
    /// @param[in] bytes_of_secret_access_key The number of bytes in the secret
    ///                                       access key.
    int storage_properties_set_access_key_and_secret(
      struct StorageProperties* out,
      const char* access_key_id,
      size_t bytes_of_access_key_id,
      const char* secret_access_key,
      size_t bytes_of_secret_access_key);

    /// @brief Set the value of the StorageDimension struct at index `index` in
    /// `out`.
    /// @param[out] out The StorageProperties struct containing the
    /// StorageDimension array.
    /// @param[in] index The index of the dimension to set.
    /// @param[in] name The name of the dimension.
    /// @param[in] bytes_of_name The number of bytes in the name buffer.
    ///                          Should include the terminating NULL.
    /// @param[in] kind The type of dimension.
    /// @param[in] array_size_px The size of the array along this dimension.
    /// @param[in] chunk_size_px The size of a chunk along this dimension.
    /// @param[in] shard_size_chunks The number of chunks in a shard along this
    ///                              dimension.
    /// @returns 1 on success, otherwise 0
    int storage_properties_set_dimension(struct StorageProperties* out,
                                         int index,
                                         const char* name,
                                         size_t bytes_of_name,
                                         enum DimensionType kind,
                                         uint32_t array_size_px,
                                         uint32_t chunk_size_px,
                                         uint32_t shard_size_chunks);

    /// @brief Set multiscale properties for `out`.
    /// Convenience function to enable multiscale.
    /// @returns 1 on success, otherwise 0
    /// @param[in, out] out The storage properties to change.
    /// @param[in] enable A flag to enable or disable multiscale.
    int storage_properties_set_enable_multiscale(struct StorageProperties* out,
                                                 uint8_t enable);

    /// Free allocated string storage.
    void storage_properties_destroy(struct StorageProperties* self);

#ifdef __cplusplus
}
#endif

#endif // H_ACQUIRE_PROPS_STORAGE_V0
