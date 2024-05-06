#ifndef H_ACQUIRE_DRIVER_COMPONENTS_V0
#define H_ACQUIRE_DRIVER_COMPONENTS_V0

#include <stdint.h>
#include <stddef.h>
#include "platform.h"

#define VIDEOFRAME_ALIGN 64

#ifdef __cplusplus

extern "C"
{
#endif

    struct String
    {
        char* str;

        /// Length of the `str` buffer. Should include the terminating '\0'
        /// if it exists.
        size_t nbytes;

        /// 0 when `str` is heap allocated, otherwise 1.
        /// When 1, then the string needs to live longer than the runtime; it
        /// may have static storage. The caller is responsible for deallocating
        /// any associated resources.
        ///
        /// When 0, storage may be deallocated within the runtime using the
        /// standard library's `free` function.
        uint8_t is_ref;
    };

    struct PID
    {
        float proportional, integral, derivative;
    };

    enum TriggerEdge
    {
        TriggerEdge_Rising,
        TriggerEdge_Falling,
        TriggerEdge_AnyEdge,
        TriggerEdge_LevelHigh,
        TriggerEdge_LevelLow,
        TriggerEdgeCount,
        TriggerEdge_NotApplicable,
        TriggerEdge_Unknown,
    };

    enum SignalIOKind
    {
        Signal_Input,
        Signal_Output,
    };

    struct Trigger
    {
        uint8_t enable;
        uint8_t line;
        enum SignalIOKind kind;
        enum TriggerEdge edge;
    };

    enum SignalType
    {
        Signal_Analog,
        Signal_Digital,
    };

    enum SampleType
    {
        SampleType_u8,
        SampleType_u16,
        SampleType_i8,
        SampleType_i16,
        SampleType_f32,
        SampleType_u10, // unpacked 10 bit in 2 bytes
        SampleType_u12, // unpacked 12 bit in 2 bytes
        SampleType_u14, // unpacked 14 bit in 2 bytes
        SampleTypeCount,
        SampleType_Unknown
    };

    struct SampleRateHz
    {
        uint64_t numerator, denominator;
    };

    enum Direction
    {
        Direction_Forward,
        Direction_Backward,
        Direction_Count,
        Direction_Unknown
    };

    struct VoltageRange
    {
        float mn, mx;
    };

    struct ImageShape
    {
        struct image_dims_s
        {
            uint32_t channels, width, height, planes;
        } dims;
        struct image_strides_s
        {
            int64_t channels, width, height, planes;
        } strides;
        enum SampleType type;
    };

    struct ImageInfo
    {
        struct ImageShape shape;
        uint64_t hardware_timestamp;
        uint64_t hardware_frame_id;
    };

    struct ACQUIRE_ALIGN(VIDEOFRAME_ALIGN) VideoFrame
    {
        /// The total number of bytes for this struct plus the
        /// size of the attached data buffer.
        size_t bytes_of_frame;
        struct ImageShape shape;
        uint64_t frame_id;
        uint64_t hardware_frame_id;
        struct video_frame_timestamps_s
        {
            uint64_t hardware;
            uint64_t acq_thread;
        } timestamps;
#pragma warning(suppress : 4200)
        uint8_t data[];
    };

    struct PixelScale
    {
        // Neither of these should be negative, but either can be zero if the
        // scale is not known.
        double x, y;
    };

    const char* sample_type_as_string(enum SampleType type);
    size_t bytes_of_type(enum SampleType type);
    size_t bytes_of_image(const struct ImageShape* shape);

#ifdef __cplusplus
}
#endif

#endif // H_ACQUIRE_DRIVER_COMPONENTS_V0
