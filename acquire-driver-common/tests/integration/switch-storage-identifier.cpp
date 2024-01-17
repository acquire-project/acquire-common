/// @file switch-storage-identifier.cpp
/// Test that we can switch storage identifiers without destroying the storage device.

#include "acquire.h"
#include "device/hal/device.manager.h"
#include "logger.h"

#include <cstdio>
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

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
#define SIZED(str) str, sizeof(str) - 1

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

const static uint32_t nframes = 32;

void
configure_camera(AcquireRuntime* runtime)
{
    CHECK(runtime);

    AcquireProperties props = {};
    OK(acquire_get_configuration(runtime, &props));

    auto* dm = acquire_device_manager(runtime);
    CHECK(dm);

    DEVOK(device_manager_select(dm,
                                DeviceKind_Camera,
                                SIZED(".*empty.*"),
                                &props.video[0].camera.identifier));

    props.video[0].camera.settings.binning = 1;
    props.video[0].camera.settings.pixel_type = SampleType_u8;
    props.video[0].camera.settings.shape = { .x = 64, .y = 48 };
    props.video[0].camera.settings.exposure_time_us = 1e4;
    props.video[0].max_frame_count = nframes;

    OK(acquire_configure(runtime, &props));
}

void
configure_storage_trash(AcquireRuntime* runtime)
{
    CHECK(runtime);

    AcquireProperties props = {};
    OK(acquire_get_configuration(runtime, &props));

    auto* dm = acquire_device_manager(runtime);
    CHECK(dm);

    props.video[0].storage = { 0 };
    DEVOK(device_manager_select(dm,
                                DeviceKind_Storage,
                                SIZED("trash"),
                                &props.video[0].storage.identifier));

    OK(acquire_configure(runtime, &props));
}

void
configure_storage_tiff(AcquireRuntime* runtime)
{
    CHECK(runtime);

    AcquireProperties props = {};
    OK(acquire_get_configuration(runtime, &props));

    auto* dm = acquire_device_manager(runtime);
    CHECK(dm);

    props.video[0].storage = { 0 };
    DEVOK(device_manager_select(dm,
                                DeviceKind_Storage,
                                SIZED("tiff"),
                                &props.video[0].storage.identifier));

    storage_properties_set_filename(&props.video[0].storage.settings,
                                    SIZED(TEST ".tif") + 1);

    OK(acquire_configure(runtime, &props));
}

void
validate_storage_tiff()
{
    const std::string file_path = TEST ".tif";
    EXPECT(
      fs::exists(file_path), "Expected file to exist: %s", file_path.c_str());

    const auto file_size = fs::file_size(file_path);
    EXPECT(file_size >= 64 * 48 * nframes,
           "Expected file to have size at least %d (has size %d): %s",
           64 * 48 * nframes,
           file_size,
           file_path.c_str());
}

void
configure_storage_raw(AcquireRuntime* runtime)
{
    CHECK(runtime);

    AcquireProperties props = {};
    OK(acquire_get_configuration(runtime, &props));

    auto* dm = acquire_device_manager(runtime);
    CHECK(dm);

    props.video[0].storage = { 0 };
    DEVOK(device_manager_select(dm,
                                DeviceKind_Storage,
                                SIZED("Raw"),
                                &props.video[0].storage.identifier));

    storage_properties_set_filename(&props.video[0].storage.settings,
                                    SIZED(TEST ".bin") + 1);

    OK(acquire_configure(runtime, &props));
}

void
validate_storage_raw()
{
    const std::string file_path = TEST ".bin";
    EXPECT(
      fs::exists(file_path), "Expected file to exist: %s", file_path.c_str());
    EXPECT(fs::is_regular_file(file_path),
           "Expected file to exist: %s",
           file_path.c_str());
    const auto file_size = fs::file_size(file_path);
    EXPECT(file_size == (sizeof(VideoFrame) + 64 * 48) * nframes,
           "Expected file to have size %d (has size %d): %s",
           64 * 48 * nframes,
           file_size,
           file_path.c_str());
}

void
acquire(AcquireRuntime* runtime)
{
    CHECK(runtime);

    OK(acquire_start(runtime));
    OK(acquire_stop(runtime));
}

int
main()
{
    auto* runtime = acquire_init(reporter);
    try {
        configure_camera(runtime);

        configure_storage_trash(runtime);
        acquire(runtime);

        configure_storage_tiff(runtime);
        acquire(runtime);
        validate_storage_tiff();
        fs::remove_all(TEST ".tif");

        configure_storage_trash(runtime);
        acquire(runtime);

        configure_storage_raw(runtime);
        acquire(runtime);
        validate_storage_raw();
        fs::remove_all(TEST ".bin");

        configure_storage_trash(runtime);
        acquire(runtime);

        configure_storage_tiff(runtime);
        acquire(runtime);
        validate_storage_tiff();
        fs::remove_all(TEST ".tif");

        configure_storage_raw(runtime);
        acquire(runtime);
        validate_storage_raw();
        fs::remove_all(TEST ".bin");

        configure_storage_trash(runtime);
        acquire(runtime);

        configure_storage_raw(runtime);
        acquire(runtime);
        validate_storage_raw();
        fs::remove_all(TEST ".bin");

        configure_storage_tiff(runtime);
        acquire(runtime);
        validate_storage_tiff();
        fs::remove_all(TEST ".tif");

    } catch (const std::exception& exc) {
        return 1;
    } catch (...) {
        return 1;
    }

    acquire_shutdown(runtime);
    return 0;
}