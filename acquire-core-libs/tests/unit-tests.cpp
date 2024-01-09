/// @file unit-tests.cpp
/// Runs all unit tests in this project.

// This is a "unit test" driver.
//
// Adding unit test functions here will run them as part of the CTest suite
// in a standardized fashion.
//
// Unit tests should be focused on testing the smallest logically isolated
// parts of the code. Practically, this means they should live close to the
// code they're testing. That is usually under the public interface
// defined by this module - if you're test uses a private interface that's a
// good sign it might be a unit test.
//
// Adding a new unit test:
// 1. Define your unit test in the same source file as what you're testing.
// 2. Add it to the declarations list below. See TEST DECLARATIONS.
// 3. Add it to the test list. See TEST LIST.
//
// Template:
//
// ```c
//      #ifndef NO_UNIT_TESTS
//      int
//      unit_test__my_descriptive_test_name()
//      {
//          // do stuff
//          return 1; // success
//      Error:
//          return 0; // failure
//      }
//      #endif // NO_UNIT_TESTS
// ```

#include "platform.h"
#include "logger.h"

#include <cstdio>
#include <stdexcept>
#include <vector>

#define L (aq_logger)
#define LOG(...) L(0, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define ERR(...) L(1, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

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

//
//      TEST DECLARATIONS
//

extern "C"
{
    // core-platform
    int unit_test__monotonic_clock_increases_monotonically();
    // device-properties
    int unit_test__storage__storage_property_string_check();
    int unit_test__storage__copy_string();
    int unit_test__storage_properties_set_chunking_props();
    int unit_test__storage_properties_default_append_dimension_is_t();
    int unit_test__storage_properties_set_sharding_props();
    int unit_test__device_state_as_string__is_defined_for_all();
    int unit_test__device_kind_as_string__is_defined_for_all();
    int unit_test__sample_type_as_string__is_defined_for_all();
    int unit_test__bytes_of_type__is_defined_for_all();
}

int
main()
{
    struct testcase
    {
        const char* name;
        int (*test)();
    };

    //
    // TEST LIST
    //

    const std::vector<testcase> tests{
#define CASE(e) { .name = #e, .test = (e) }
        CASE(unit_test__monotonic_clock_increases_monotonically),
        CASE(unit_test__storage__storage_property_string_check),
        CASE(unit_test__storage__copy_string),
        CASE(unit_test__storage_properties_set_chunking_props),
        CASE(unit_test__storage_properties_default_append_dimension_is_t),
        CASE(unit_test__storage_properties_set_sharding_props),
        CASE(unit_test__device_state_as_string__is_defined_for_all),
        CASE(unit_test__device_kind_as_string__is_defined_for_all),
        CASE(unit_test__sample_type_as_string__is_defined_for_all),
        CASE(unit_test__bytes_of_type__is_defined_for_all),
#undef CASE
    };

    bool any = false;

    for (const auto& test : tests) {
        logger_set_reporter(reporter);
        LOG("Running %s", test.name);
        if (!(test.test())) {
            ERR("unit test failed: %s", test.name);
            any = true;
        }
    }

    return any;
}
