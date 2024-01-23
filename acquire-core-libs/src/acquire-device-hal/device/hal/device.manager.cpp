#include "device.manager.h"
#include "loader.h"
#include "logger.h"

#include <exception>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <regex>

//
// static driver initializers
//

#define LOG(...) aq_logger(0, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define LOGE(...) aq_logger(1, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define EXPECT(e, ...)                                                         \
    do {                                                                       \
        if (!(e)) {                                                            \
            LOGE(__VA_ARGS__);                                                 \
            throw std::runtime_error("Check failed: " #e);                     \
        }                                                                      \
    } while (0)
#define CHECK(e) EXPECT(e, "Expression evaluated as false:\n\t%s", #e)
#define CHECK_NOTHROW(e)                                                       \
    do {                                                                       \
        if (!(e)) {                                                            \
            LOGE("Expression evaluated as false:\n\t%s", #e);                  \
        }                                                                      \
    } while (0)
// #define DEBUG(...) LOG(__VA_ARGS__)
#define DEBUG(...)

namespace {

enum class State
{
    Initialized,
    Shutdown
};

class DeviceManagerV0
{
  public:
    explicit DeviceManagerV0(void (*reporter)(int is_error,
                                              const char* file,
                                              int line,
                                              const char* function,
                                              const char* msg));

    DeviceManagerV0(DeviceManagerV0 const&) = delete;
    ~DeviceManagerV0();

    void operator=(DeviceManagerV0 const&) = delete;

    size_t count() const noexcept;
    const DeviceIdentifier* get(size_t index);
    const DeviceIdentifier* select(DeviceKind kind,
                                   const std::string& name) const;
    Driver* get_driver(const struct DeviceIdentifier*);

  private:
    void init(void (*reporter)(int is_error,
                               const char* file,
                               int line,
                               const char* function,
                               const char* msg));
    void shutdown();
    void guard_state();

    struct DeviceEnumerationResult
    {
        enum DeviceStatusCode status_;
        struct DeviceIdentifier identifier_;

        DeviceEnumerationResult(enum DeviceStatusCode status,
                                const struct DeviceIdentifier& identifier);
    };

    std::vector<DeviceEnumerationResult> identifiers_;
    std::vector<Driver*> drivers_;
    State state_;
};

DeviceManagerV0::DeviceEnumerationResult::DeviceEnumerationResult(
  DeviceStatusCode status,
  const DeviceIdentifier& identifier)
  : status_(status)
  , identifier_(identifier)
{
}

DeviceManagerV0::DeviceManagerV0(void (*reporter)(int is_error,
                                                  const char* file,
                                                  int line,
                                                  const char* function,
                                                  const char* msg))
  : state_(State::Shutdown)
{
    init(reporter);
    CHECK(state_ == State::Initialized);
}

DeviceManagerV0::~DeviceManagerV0()
{
    shutdown();
    // the post-condition check may throw, but if it does, that's a bug.
    CHECK_NOTHROW(state_ == State::Shutdown);
}

void
DeviceManagerV0::init(void (*reporter)(int is_error,
                                       const char* file,
                                       int line,
                                       const char* function,
                                       const char* msg))
{
    drivers_.push_back(driver_load("acquire-driver-common", reporter));
    drivers_.push_back(driver_load("acquire-driver-hdcam", reporter));
    drivers_.push_back(driver_load("acquire-driver-zarr", reporter));
    drivers_.push_back(driver_load("acquire-driver-egrabber", reporter));
    drivers_.push_back(driver_load("acquire-driver-spinnaker", reporter));
    drivers_.push_back(driver_load("acquire-driver-pvcam", reporter));

    // enumerate devices
    const DeviceIdentifier dflt{ 0, 0, DeviceKind_Unknown, "" };

    uint8_t driver_id = 0;
    for (const auto driver : drivers_) {
        if (driver) {
            uint32_t n = driver->device_count(driver);
            for (uint32_t i = 0; i < n; ++i) {
                auto& ident = identifiers_.emplace_back(Device_Err, dflt);
                CHECK_NOTHROW(Device_Ok == (ident.status_ = driver->describe(
                                              driver, &ident.identifier_, i)));
                // It's important to populate the driver_id after invoking
                // driver->describe().
                ident.identifier_.driver_id = driver_id;
            }
        }
        ++driver_id;
    }
    state_ = State::Initialized;
}

void
DeviceManagerV0::shutdown()
{
    if (state_ != State::Shutdown) {
        for (auto driver : drivers_) {
            if (driver) {
                CHECK_NOTHROW(Device_Ok == driver->shutdown(driver));
            }
        }
        state_ = State::Shutdown;
    }
}

void
DeviceManagerV0::guard_state()
{
    if (state_ == State::Shutdown) {
        init(0);
    }
    CHECK(state_ == State::Initialized);
}

size_t
DeviceManagerV0::count() const noexcept
{
    return identifiers_.size();
}

const struct DeviceIdentifier*
DeviceManagerV0::get(size_t index)
{
    guard_state();
    const auto& ident_result = identifiers_.at(index);
    if (ident_result.status_ == Device_Ok) {
        return &ident_result.identifier_;
    } else {
        char ident_str[80] = { 0 };
        char msg[256] = { 0 };
        device_identifier_as_debug_string(
          ident_str, sizeof(ident_str) - 1, &ident_result.identifier_);
        snprintf(msg,
                 sizeof(msg) - 1,
                 "An error was encountered enumerating %s",
                 ident_str);
        throw std::runtime_error(msg);
    }
}

Driver*
DeviceManagerV0::get_driver(const struct DeviceIdentifier* identifier)
{
    CHECK(identifier);
    return drivers_.at(identifier->driver_id);
}

const struct DeviceIdentifier*
DeviceManagerV0::select(DeviceKind kind, const std::string& name) const
{
    std::regex re(name.c_str(),
                  std::regex_constants::icase | std::regex_constants::optimize);
    for (const auto& identifier : identifiers_) {
        if (identifier.identifier_.kind == kind) {
            // regex match for name
            const auto name_match =
              name.empty() ||
              std::regex_match((const char*)identifier.identifier_.name, re);

            DEBUG("Check name (%d): %s %s %s",
                  (int)(std::char_traits<char8_t>::length(
                    (const char8_t*)identifier.identifier_.name)),
                  name.empty() ? "(empty)" : name.c_str(),
                  name_match ? "==" : "!=",
                  identifier.identifier_.name)

            if (name_match) {
                LOG("Selecting (%d,%d) for %s \"%s\"",
                    identifier.identifier_.driver_id,
                    identifier.identifier_.device_id,
                    device_kind_as_string(kind),
                    identifier.identifier_.name);
                return &identifier.identifier_;
            }
        }
    }
    return 0;
}

} // end namespace ::{anonymous}

extern "C" enum DeviceStatusCode
device_manager_init(struct DeviceManager* self,
                    void (*reporter)(int is_error,
                                     const char* file,
                                     int line,
                                     const char* function,
                                     const char* msg))
{
    try {
        CHECK(self);
        self->impl = new DeviceManagerV0(reporter);
        return Device_Ok;
    } catch (std::exception& e) {
        LOGE(e.what());
        return Device_Err;
    } catch (...) {
        LOGE("Unhandled exception");
        return Device_Err;
    }
}

extern "C" enum DeviceStatusCode
device_manager_destroy(struct DeviceManager* self_)
{
    try {
        EXPECT(self_, "Expected non-NULL pointer for `self`");
        EXPECT(self_->impl, "Expected non-NULL pointer for `self->impl`");
        auto self = (DeviceManagerV0*)self_->impl;
        delete self;
        self_->impl = 0;
        return Device_Ok;
    } catch (std::exception& e) {
        LOGE(e.what());
        return Device_Err;
    } catch (...) {
        LOGE("Unhandled exception");
        return Device_Err;
    }
}

extern "C" uint32_t
device_manager_count(const struct DeviceManager* self_)
{
    try {
        EXPECT(self_, "Expected non-NULL pointer for `self`");
        EXPECT(self_->impl, "Expected non-NULL pointer for `self->impl`");
        auto self = (DeviceManagerV0*)self_->impl;
        return uint32_t(self->count());
    } catch (std::exception& e) {
        LOGE(e.what());
        return 0;
    } catch (...) {
        LOGE("Unhandled exception");
        return Device_Err;
    }
}

extern "C" enum DeviceStatusCode
device_manager_get(struct DeviceIdentifier* out,
                   const struct DeviceManager* self_,
                   uint32_t index)
{
    try {
        EXPECT(self_, "Expected non-NULL pointer for `self`");
        EXPECT(self_->impl, "Expected non-NULL pointer for `self->impl`");
        auto self = (DeviceManagerV0*)self_->impl;
        memcpy(out, self->get(index), sizeof(*out));
        return Device_Ok;
    } catch (std::exception& e) {
        LOGE(e.what());
        return Device_Err;
    } catch (...) {
        LOGE("Unhandled exception");
        return Device_Err;
    }
}

extern "C" Driver*
device_manager_get_driver(const struct DeviceManager* self_,
                          const struct DeviceIdentifier* identifier)
{
    try {
        EXPECT(self_, "Expected non-NULL pointer for `self`");
        EXPECT(self_->impl, "Expected non-NULL pointer for `self->impl`");
        auto* self = (DeviceManagerV0*)self_->impl;
        return self->get_driver(identifier);
    } catch (std::exception& e) {
        LOGE(e.what());
        return 0;
    } catch (...) {
        LOGE("Unhandled exception");
        return 0;
    }
}

static enum DeviceStatusCode
device_manager_select_inner_(const struct DeviceManager* self_,
                             enum DeviceKind kind,
                             const char* name_,
                             size_t bytes_of_name,
                             struct DeviceIdentifier* out)
{
    try {
        EXPECT(self_, "Expected non-NULL pointer for `self`");
        EXPECT(self_->impl, "Expected non-NULL pointer for `self->impl`");
        auto* self = (DeviceManagerV0*)self_->impl;

        std::string name;
        if (name_ && bytes_of_name) {
            name.assign(name_, bytes_of_name);
            // Defensive checking of 'bytes_of_name'
            // It's easy to get wrong.
            if (*name.rbegin() == '\0') {
                LOGE("Name contains nulls (name=\"%s\", "
                     "bytes_of_name=%d, strlen(name)=%d)",
                     name_,
                     (int)bytes_of_name,
                     (int)strlen(name_));
                // Remove the NULL chars and continue.
                name.erase(std::find(name.begin(), name.end(), '\0'),
                           name.end());
            }
        }

        const auto result = self->select(kind, name);
        if (result) {
            memcpy(out, result, sizeof(*out));
            return Device_Ok;
        } else {
            LOGE("Device not found: %s %s",
                 device_kind_as_string(kind),
                 name.empty() ? "(any)" : name.c_str());
            return Device_Err;
        }
    } catch (std::exception& e) {
        LOGE(e.what());
        return Device_Err;
    } catch (...) {
        LOGE("Unhandled exception");
        return Device_Err;
    }
}

enum DeviceStatusCode
device_manager_select_first(const struct DeviceManager* self,
                            enum DeviceKind kind,
                            struct DeviceIdentifier* out)
{
    return device_manager_select_inner_(self, kind, nullptr, 0, out);
}

enum DeviceStatusCode
device_manager_select(const struct DeviceManager* self,
                      enum DeviceKind kind,
                      const char* name,
                      size_t bytes_of_name,
                      struct DeviceIdentifier* out)
{
    try {
        EXPECT((name && bytes_of_name) || bytes_of_name == 0,
               "name must not be null if bytes_of_name is non-zero");
        return device_manager_select_inner_(
          self, kind, name, bytes_of_name, out);
    } catch (const std::exception& e) {
        LOGE(e.what());
    } catch (...) {
        LOGE("Unhandled exception");
    }
    return Device_Err;
}

enum DeviceStatusCode
device_manager_select_default(const struct DeviceManager* self,
                              enum DeviceKind kind,
                              struct DeviceIdentifier* out)
{
    switch (kind) {
        case DeviceKind_Camera:
            return device_manager_select_inner_(
              self, kind, ".*random.*", sizeof(".*random.*") - 1, out);
        case DeviceKind_Storage:
            return device_manager_select_inner_(
              self, kind, "trash", sizeof("trash") - 1, out);
        default:
            LOGE("Invalid parameter `kind`. Got: %s",
                 device_kind_as_string(kind));
            return Device_Err;
    }
}