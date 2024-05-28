/// @file aligned-videoframe-pointers.cpp
/// Test that VideoFrame pointers are aligned at 8 bytes.

#include "acquire.h"
#include "device/hal/device.manager.h"
#include "platform.h"
#include "logger.h"

#include <cstdio>
#include <stdexcept>

void
reporter(int is_error,
         const char* file,
         int line,
         const char* function,
         const char* msg)
{
    fprintf(is_error ? stderr : stdout,
            "%s%s(%d) - %s: %s\n",
            is_error ? "ERROR " : "",
            file,
            line,
            function,
            msg);
}

/// Helper for passing size static strings as function args.
/// For a function: `f(char*,size_t)` use `f(SIZED("hello"))`.
/// Expands to `f("hello",5)`.
#define SIZED(str) str, sizeof(str)

#define L (aq_logger)
#define LOG(...) L(0, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define ERR(...) L(1, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define EXPECT(e, ...)                                                         \
    do {                                                                       \
        if (!(e)) {                                                            \
            char buf[1 << 8] = { 0 };                                          \
            ERR(__VA_ARGS__);                                                  \
            snprintf(buf, sizeof(buf) - 1, __VA_ARGS__);                       \
            throw std::runtime_error(buf);                                     \
        }                                                                      \
    } while (0)
#define CHECK(e) EXPECT(e, "Expression evaluated as false: %s", #e)
#define DEVOK(e) CHECK(Device_Ok == (e))
#define OK(e) CHECK(AcquireStatus_Ok == (e))

void
configure(AcquireRuntime* runtime)
{
    CHECK(runtime);
    const DeviceManager* dm = acquire_device_manager(runtime);
    CHECK(dm);

    AcquireProperties props = {};
    OK(acquire_get_configuration(runtime, &props));

    // configure camera
    DEVOK(device_manager_select(dm,
                                DeviceKind_Camera,
                                SIZED("simulated.*empty.*") - 1,
                                &props.video[0].camera.identifier));

    // These settings are chosen to exercise the 8-byte alignment constraint.
    props.video[0].camera.settings.binning = 1;
    props.video[0].camera.settings.pixel_type = SampleType_u8;
    props.video[0].camera.settings.shape = {
        .x = 33,
        .y = 47,
    };

    // configure acquisition
    props.video[0].max_frame_count = 10;

    // configure storage
    DEVOK(device_manager_select(dm,
                                DeviceKind_Storage,
                                SIZED("trash") - 1,
                                &props.video[0].storage.identifier));
    storage_properties_init(
      &props.video[0].storage.settings, 0, nullptr, 0, nullptr, 0, { 0 }, 0);

    OK(acquire_configure(runtime, &props));
    storage_properties_destroy(&props.video[0].storage.settings);
}

static size_t
align_up(size_t n, size_t align)
{
    return (n + align - 1) & ~(align - 1);
}

void
acquire(AcquireRuntime* runtime)
{
    CHECK(runtime);

    AcquireProperties props = {};
    OK(acquire_get_configuration(runtime, &props));

    const auto next = [](VideoFrame* cur) -> VideoFrame* {
        return (VideoFrame*)(((uint8_t*)cur) +
                             align_up(cur->bytes_of_frame, 8));
    };

    const auto consumed_bytes = [](const VideoFrame* const cur,
                                   const VideoFrame* const end) -> size_t {
        return (uint8_t*)end - (uint8_t*)cur;
    };

    struct clock clock
    {};
    // expected time to acquire frames + 100%
    static double time_limit_ms = props.video[0].max_frame_count * 1000.0 * 3.0;
    clock_init(&clock);
    clock_shift_ms(&clock, time_limit_ms);
    OK(acquire_start(runtime));
    {
        uint64_t nframes = 0;
        while (nframes < props.video[0].max_frame_count) {
            struct clock throttle
            {};
            clock_init(&throttle);
            EXPECT(clock_cmp_now(&clock) < 0,
                   "Timeout at %f ms",
                   clock_toc_ms(&clock) + time_limit_ms);

            VideoFrame *beg, *end, *cur;
            OK(acquire_map_read(runtime, 0, &beg, &end));

            for (cur = beg; cur < end; cur = next(cur)) {
                LOG("stream %d counting frame w id %d", 0, cur->frame_id);

                const size_t unpadded_bytes =
                  bytes_of_image(&cur->shape) + sizeof(*cur);
                const size_t alignment = 8;
                const size_t padding =
                  (alignment - (unpadded_bytes & (alignment - 1))) &
                  (alignment - 1);
                const size_t nbytes_aligned = unpadded_bytes + padding;

                // check data is correct
                CHECK(bytes_of_image(&cur->shape) == 33 * 47);
                CHECK(cur->bytes_of_frame == nbytes_aligned);
                CHECK(cur->frame_id == nframes);
                CHECK(cur->shape.dims.width ==
                      props.video[0].camera.settings.shape.x);
                CHECK(cur->shape.dims.height ==
                      props.video[0].camera.settings.shape.y);

                // check pointer is aligned
                CHECK((size_t)cur % 8 == 0);
                ++nframes;
            }

            {
                const auto n = (uint32_t)consumed_bytes(beg, end);
                CHECK(n % 8 == 0);
                OK(acquire_unmap_read(runtime, 0, n));
                if (n)
                    LOG("stream %d consumed bytes %d", 0, n);
            }
            clock_sleep_ms(&throttle, 100.0f);

            LOG("stream %d nframes %d. remaining time %f s",
                0,
                nframes,
                -1e-3 * clock_toc_ms(&clock));
        }

        CHECK(nframes == props.video[0].max_frame_count);
    }

    OK(acquire_stop(runtime));
}

int
main()
{
    int retval = 1;
    AcquireRuntime* runtime = acquire_init(reporter);

    try {
        configure(runtime);
        acquire(runtime);
        retval = 0;
    } catch (const std::exception& e) {
        ERR("Exception: %s", e.what());
    }

    acquire_shutdown(runtime);
    return retval;
}
