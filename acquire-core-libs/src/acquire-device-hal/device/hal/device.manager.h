#ifndef H_ACQUIRE_DEVICE_MANAGER_V0
#define H_ACQUIRE_DEVICE_MANAGER_V0

#include "device/props/device.h"

#ifdef __cplusplus
extern "C"
{
#endif

    struct DeviceManager
    {
        void* impl;
    };

    enum DeviceStatusCode device_manager_init(
      struct DeviceManager* self,
      void (*reporter)(int is_error,
                       const char* file,
                       int line,
                       const char* function,
                       const char* msg));

    enum DeviceStatusCode device_manager_destroy(struct DeviceManager* self);

    uint32_t device_manager_count(const struct DeviceManager* self);

    enum DeviceStatusCode device_manager_get(struct DeviceIdentifier* out,
                                             const struct DeviceManager* self,
                                             uint32_t index);

    /// Query for the first device with a matching `kind`.
    ///
    /// @param[in] self     The deviceManager context to query.
    /// @param[in] kind     The kind of device to select.
    /// @param[out] out     The id of the first device discovered that matches
    ///                     the `kind`.
    enum DeviceStatusCode device_manager_select_first(
      const struct DeviceManager* self,
      enum DeviceKind kind,
      struct DeviceIdentifier* out);

    /// Query for a device with a matching `kind` and `name`.
    ///
    /// @param[in] self     The deviceManager context to query.
    /// @param[in] kind     The kind of device to select.
    /// @param[in] name     A regex pattern matching the name of the device.
    ///                     May be NULL or empty. Does not need to be null
    ///                     terminated. If NULL or empty, the query returns
    ///                     the first device matching `kind`.
    /// @param[in] bytes_of_name The number of bytes in the "name" string.
    ///                     Should exclude any terminating NULLs.
    /// @param[out] out     The id of the first device discovered that matches
    ///                     the `kind` and `name`.
    ///
    /// `name` is optional. If `bytes_of_name` is 0, this will behave just like
    /// `device_manager_select_first`. `name` must not be 0 if `bytes_of_name`
    /// is non-zero.
    enum DeviceStatusCode device_manager_select(
      const struct DeviceManager* self,
      enum DeviceKind kind,
      const char* name,
      size_t bytes_of_name,
      struct DeviceIdentifier* out);

    /// Query for the default device with a matching `kind`.
    ///
    /// @param self   The deviceManager context to query.
    /// @param kind   The kind of device to select.
    /// @param out    The id of the default device discovered that matches the
    ///               `kind`.
    enum DeviceStatusCode device_manager_select_default(
      const struct DeviceManager* self,
      enum DeviceKind kind,
      struct DeviceIdentifier* out);

    struct Driver* device_manager_get_driver(
      const struct DeviceManager* self,
      const struct DeviceIdentifier* identifier);

#ifdef __cplusplus
}
#endif

#endif // H_ACQUIRE_DEVICE_MANAGER_V0
