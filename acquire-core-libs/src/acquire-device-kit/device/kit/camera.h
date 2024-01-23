#ifndef H_ACQUIRE_KIT_CAMERA_V0
#define H_ACQUIRE_KIT_CAMERA_V0

#include "device/props/device.h"
#include "device/props/camera.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /// @brief Represents and allows control of a camera device.
    struct Camera
    {
        struct Device device;
        enum DeviceState state;

        /// @brief Attempts to set the given properties on the given camera.
        /// @details Setting properties can partially succeed depending on
        ///          other property values or the state of the camera.
        ///          If you want to be certain that a specific property was
        ///          actually used, you should call get afterwards to check
        ///          that property value.
        enum DeviceStatusCode (*set)(struct Camera*,
                                     struct CameraProperties* settings);

        /// @brief Fills out the given properties from the given camera.
        enum DeviceStatusCode (*get)(const struct Camera*,
                                     struct CameraProperties* settings);

        /// @brief Fills out the given property metadata from the given camera.
        /// @details These metadata or capabilities may depend on the camera's
        ///          current property values.
        enum DeviceStatusCode (*get_meta)(const struct Camera*,
                                          struct CameraPropertyMetadata* meta);

        /// @brief The shape of the next frame that the camera will capture.
        enum DeviceStatusCode (*get_shape)(const struct Camera*,
                                           struct ImageShape* shape);
        /// @brief Starts acquiring frames.
        enum DeviceStatusCode (*start)(struct Camera*);

        /// @brief Stops acquiring frames.
        /// @details This instructs the camera to stop and may block until it
        ///          actually has stopped acquiring frames.
        ///          The camera should also be restartable after calling this
        ///          (i.e. one call of start after one call of stop should
        ///          succeed).
        enum DeviceStatusCode (*stop)(struct Camera*);

        /// @brief Execute the software trigger.
        enum DeviceStatusCode (*execute_trigger)(struct Camera*);

        /// @brief Gets the next frame from the camera.
        enum DeviceStatusCode (*get_frame)(struct Camera*,
                                           void* im,
                                           size_t* nbytes,
                                           struct ImageInfo* info);
    };

#ifdef __cplusplus
}
#endif

#endif // H_ACQUIRE_KIT_CAMERA_V0
