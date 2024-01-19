
#include "camera.h"
#include "logger.h"
#include "driver.h"

#define countof(e) (sizeof(e) / sizeof(*(e)))
#define containerof(P, T, F) ((T*)(((char*)(P)) - offsetof(T, F)))

#define LOG(...) aq_logger(0, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOGE(...) aq_logger(1, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define CHECK(e)                                                               \
    do {                                                                       \
        if (!(e)) {                                                            \
            LOGE("Expression evaluated as false:\n\t%s", #e);                  \
            goto Error;                                                        \
        }                                                                      \
    } while (0)
#define CHECK_NOJUMP(e)                                                        \
    do {                                                                       \
        if (!(e)) {                                                            \
            LOGE("Expression evaluated as false:\n\t%s", #e);                  \
        }                                                                      \
    } while (0)

struct Camera*
camera_open(const struct DeviceManager* system,
            const struct DeviceIdentifier* identifier)
{
    struct Camera* self = 0;

    CHECK(identifier);
    CHECK(identifier->kind == DeviceKind_Camera);

    {
        struct Device* device = 0;
        CHECK(Device_Ok ==
              driver_open_device(device_manager_get_driver(system, identifier),
                                 identifier->device_id,
                                 &device));

        self = containerof(device, struct Camera, device);
    }

    // Check the required interface functions are non-null
    CHECK(self->set != NULL);
    CHECK(self->get != NULL);
    CHECK(self->get_shape != NULL);
    CHECK(self->get_meta != NULL);
    CHECK(self->start != NULL);
    CHECK(self->stop != NULL);
    CHECK(self->execute_trigger != NULL);
    CHECK(self->get_frame != NULL);

    return self;
Error:
    return 0;
}

void
camera_close(struct Camera* self)
{
    CHECK(self);
    struct Driver* const d = self->device.driver;
    CHECK_NOJUMP(Device_Ok == d->close(d, &self->device));
Error:;
}

static uint8_t
max_u8(uint8_t a, uint8_t b)
{
    return a > b ? a : b;
}

// Validates and sets any properties.
// The set function returns the new state that the self is in.
// This may depend on how validation went.
enum DeviceStatusCode
camera_set(struct Camera* self, struct CameraProperties* settings)
{
    enum DeviceStatusCode ecode;
    // Neither can be NULL
    CHECK(self);
    CHECK(settings);
    settings->binning = max_u8(1, settings->binning);
    switch (ecode = self->set(self, settings)) {
        case Device_Ok:
            if (self->state != DeviceState_Running)
                self->state = DeviceState_Armed;
            break;
        case Device_Err:
            camera_stop(self);
            self->state = DeviceState_AwaitingConfiguration;
            break;
    }
    return ecode;
Error:
    return Device_Err;
}

enum DeviceStatusCode
camera_get(const struct Camera* self, struct CameraProperties* settings)
{
    // Neither can be NULL
    CHECK(self);
    CHECK(settings);
    return self->get(self, settings);
Error:
    return Device_Err;
}

enum DeviceStatusCode
camera_get_meta(const struct Camera* self, struct CameraPropertyMetadata* meta)
{
    // Neither can be NULL
    CHECK(self);
    CHECK(meta);
    return self->get_meta(self, meta);
Error:
    return Device_Err;
}

enum DeviceStatusCode
camera_get_image_shape(const struct Camera* self, struct ImageShape* shape)
{
    // Neither can be NULL
    CHECK(self);
    CHECK(shape);
    return self->get_shape(self, shape);
Error:
    return Device_Err;
}

enum DeviceStatusCode
camera_start(struct Camera* self)
{
    enum DeviceStatusCode ecode;
    CHECK(self);
    switch (ecode = self->start(self)) {
        case Device_Ok:
            self->state = DeviceState_Running;
            break;
        case Device_Err:
            self->state = DeviceState_AwaitingConfiguration;
            break;
    }
    return ecode;
Error:
    return Device_Err;
}

enum DeviceStatusCode
camera_stop(struct Camera* self)
{
    enum DeviceStatusCode ecode = Device_Ok;
    CHECK(self);
    if (self->state == DeviceState_Running) {
        LOG("CAMERA STOP %s", self->device.identifier.name);
        switch (ecode = self->stop(self)) {
            case Device_Ok:
                self->state = DeviceState_Armed;
                break;
            case Device_Err:
                self->state = DeviceState_AwaitingConfiguration;
                break;
        }
    }
Finalize:
    return ecode;
Error:
    ecode = Device_Err;
    goto Finalize;
}

enum DeviceStatusCode
camera_execute_trigger(struct Camera* self)
{
    CHECK(self);
    if (self->state == DeviceState_Running) {
        LOG("CAMERA EXEC SOFTWARE TRIGGER");
        return self->execute_trigger(self);
    }
    return Device_Ok;
Error:
    return Device_Err;
}

enum DeviceStatusCode
camera_get_frame(struct Camera* self,
                 void* im,
                 size_t* nbytes,
                 struct ImageInfo* info)
{
    CHECK(self);
    CHECK(self->state == DeviceState_Running);
    enum DeviceStatusCode ecode = self->get_frame(self, im, nbytes, info);
    if (ecode != Device_Ok) {
        camera_stop(self);
        self->state = DeviceState_AwaitingConfiguration;
    }
    return ecode;
Error:
    return Device_Err;
}

enum DeviceState
camera_get_state(const struct Camera* const camera)
{
    CHECK(camera);
    return camera->state;
Error:
    return DeviceState_Closed;
}
