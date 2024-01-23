/// @file default-devices.cpp
/// Test that setting DeviceKind_None for the camera or storage devices selects
/// random and trash devices, respectively, if and only if the other device is
/// not set to DeviceKind_None.

#include "acquire.h"
#include "device/hal/device.manager.h"
#include "logger.h"

#include <cstdio>
#include <cstring>
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
select_camera(AcquireRuntime* runtime)
{
    CHECK(runtime);

    auto* dm = acquire_device_manager(runtime);
    CHECK(dm);

    AcquireProperties props = {};
    OK(acquire_get_configuration(runtime, &props));
    props.video[0].camera.identifier = { 0 };
    CHECK(props.video[0].camera.identifier.kind == DeviceKind_None);
    CHECK(strlen(props.video[0].camera.identifier.name) == 0);

    DEVOK(device_manager_select(dm,
                                DeviceKind_Storage,
                                SIZED("Trash") - 1,
                                &props.video[0].storage.identifier));

    OK(acquire_configure(runtime, &props));

    CHECK(props.video[0].camera.identifier.kind == DeviceKind_Camera);
    CHECK(strcmp(props.video[0].camera.identifier.name,
                 "simulated: uniform random") == 0);
}

void
select_storage(AcquireRuntime* runtime)
{
    CHECK(runtime);

    auto* dm = acquire_device_manager(runtime);
    CHECK(dm);

    AcquireProperties props = {};
    OK(acquire_get_configuration(runtime, &props));
    props.video[0].storage.identifier = { 0 };
    CHECK(props.video[0].storage.identifier.kind == DeviceKind_None);
    CHECK(strlen(props.video[0].storage.identifier.name) == 0);

    DEVOK(device_manager_select(dm,
                                DeviceKind_Camera,
                                SIZED(".*empty.*") - 1,
                                &props.video[0].camera.identifier));

    OK(acquire_configure(runtime, &props));

    CHECK(props.video[0].storage.identifier.kind == DeviceKind_Storage);
    CHECK(strcmp(props.video[0].storage.identifier.name, "trash") == 0);
}

void
select_neither(AcquireRuntime* runtime)
{
    CHECK(runtime);

    auto* dm = acquire_device_manager(runtime);
    CHECK(dm);

    AcquireProperties props = {};
    OK(acquire_get_configuration(runtime, &props));
    props.video[0].camera.identifier = { 0 };
    CHECK(props.video[0].camera.identifier.kind == DeviceKind_None);
    CHECK(strlen(props.video[0].camera.identifier.name) == 0);

    props.video[0].storage.identifier = { 0 };
    CHECK(props.video[0].storage.identifier.kind == DeviceKind_None);
    CHECK(strlen(props.video[0].storage.identifier.name) == 0);

    OK(acquire_configure(runtime, &props));

    CHECK(props.video[0].camera.identifier.kind == DeviceKind_None);
    CHECK(strlen(props.video[0].camera.identifier.name) == 0);

    CHECK(props.video[0].storage.identifier.kind == DeviceKind_None);
    CHECK(strlen(props.video[0].storage.identifier.name) == 0);
}

int
main()
{
    auto runtime = acquire_init(reporter);

    try {
        select_camera(runtime);
        select_storage(runtime);
        select_neither(runtime);
    } catch (const std::exception& e) {
        ERR("ERROR: %s\n", e.what());
        return 1;
    }

    return 0;
}