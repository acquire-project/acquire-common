/// @file can-set-with-file-uri.cpp
/// Test that we can use a file:// URI when configuring basic Storage devices.

#include "acquire.h"
#include "device/hal/device.manager.h"
#include "logger.h"

#include <cstdio>
#include <cstring>
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
configure_storage_tiff(AcquireRuntime* runtime, const std::string& filename)
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

    storage_properties_set_uri(
      &props.video[0].storage.settings, filename.c_str(), filename.size() + 1);

    OK(acquire_configure(runtime, &props));
}

void
validate_storage_tiff(AcquireRuntime* runtime)
{
    const std::string file_path = TEST ".tif";

    AcquireProperties props = {};
    OK(acquire_get_configuration(runtime, &props));

    CHECK(0 == strncmp(props.video[0].storage.settings.uri.str,
                       file_path.c_str(),
                       file_path.size() + 1));
}

void
configure_storage_side_by_side_tiff(AcquireRuntime* runtime,
                                    const std::string& filename)
{
    CHECK(runtime);

    AcquireProperties props = {};
    OK(acquire_get_configuration(runtime, &props));

    auto* dm = acquire_device_manager(runtime);
    CHECK(dm);

    props.video[0].storage = { 0 };
    DEVOK(device_manager_select(dm,
                                DeviceKind_Storage,
                                SIZED("tiff-json"),
                                &props.video[0].storage.identifier));

    storage_properties_set_uri(
      &props.video[0].storage.settings, filename.c_str(), filename.size() + 1);

    OK(acquire_configure(runtime, &props));
}

void
validate_storage_side_by_side_tiff(AcquireRuntime* runtime)
{
    const std::string file_path = TEST ".tif";

    AcquireProperties props = {};
    OK(acquire_get_configuration(runtime, &props));

    CHECK(0 == strncmp(props.video[0].storage.settings.uri.str,
                       file_path.c_str(),
                       file_path.size() + 1));
}

void
configure_storage_raw(AcquireRuntime* runtime, const std::string& filename)
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

    storage_properties_set_uri(&props.video[0].storage.settings,
                               filename.c_str(),
                               filename.size() + 1);

    OK(acquire_configure(runtime, &props));
}

void
validate_storage_raw(AcquireRuntime* runtime)
{
    const std::string file_path = TEST ".bin";

    AcquireProperties props = {};
    OK(acquire_get_configuration(runtime, &props));

    CHECK(0 == strncmp(props.video[0].storage.settings.uri.str,
                       file_path.c_str(),
                       file_path.size() + 1));
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

        configure_storage_raw(runtime, TEST ".bin");
        validate_storage_raw(runtime);

        configure_storage_raw(runtime, "file://" TEST ".bin");
        validate_storage_raw(runtime);

        configure_storage_tiff(runtime, TEST ".tif");
        validate_storage_tiff(runtime);

        configure_storage_tiff(runtime, "file://" TEST ".tif");
        validate_storage_tiff(runtime);

        configure_storage_side_by_side_tiff(runtime, TEST ".tif");
        validate_storage_side_by_side_tiff(runtime);

        configure_storage_side_by_side_tiff(runtime, "file://" TEST ".tif");
        validate_storage_side_by_side_tiff(runtime);

    } catch (const std::exception& exc) {
        acquire_shutdown(runtime);
        return 1;
    } catch (...) {
        acquire_shutdown(runtime);
        return 1;
    }

    acquire_shutdown(runtime);
    return 0;
}