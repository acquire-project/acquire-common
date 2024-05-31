#include "components.h"

#define countof(e) (sizeof(e) / sizeof(*(e)))

const char*
sample_type_as_string(enum SampleType type)
{
    // Note: This table needs to get updated whenever a SampleType gets
    //       added. The unit test below should crash when an entry is
    //       missing.

    // clang-format off
    const char* table[] = {
#define XXX(name) [SampleType_##name] = #name
        XXX(u8),
        XXX(u16),
        XXX(i8),
        XXX(i16),
        XXX(f32),
        XXX(u10),
        XXX(u12),
        XXX(u14),
#undef XXX
    };
    // clang-format on
    if (type >= countof(table))
        return "(unknown)";

    return table[type];
}

size_t
bytes_of_type(enum SampleType type)
{
    size_t table[SampleTypeCount]; // = { 1, 2, 1, 2, 4, 2, 2, 2 };

    // clang-format off
#define XXX(s, b) table[(s)] = (b)
        XXX(SampleType_u8, 1);
        XXX(SampleType_u16, 2);
        XXX(SampleType_i8, 1);
        XXX(SampleType_i16, 2);
        XXX(SampleType_f32, 4);
        XXX(SampleType_u10, 2);
        XXX(SampleType_u12, 2);
        XXX(SampleType_u14, 2);
#undef XXX
    // clang-format on
    if (type >= countof(table))
        return 0;

    return table[type];
}

size_t
bytes_of_image(const struct ImageShape* const shape)
{
    return shape->strides.planes * bytes_of_type(shape->type);
}

//
//  UNIT TESTS
//

#ifndef NO_UNIT_TESTS
#include "logger.h"

#define L (aq_logger)
#define LOG(...) L(0, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define ERR(...) L(1, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)
#define CHECK(e)                                                               \
    do {                                                                       \
        if (!(e)) {                                                            \
            ERR("Expression evaluated as false:\n\t%s", #e);                   \
            goto Error;                                                        \
        }                                                                      \
    } while (0)

int
unit_test__sample_type_as_string__is_defined_for_all()
{
    for (int i = 0; i < SampleTypeCount; ++i) {
        // Check this isn't returning "unknown" for known counts
        CHECK(sample_type_as_string(i)[0] != '(');
    }
    return 1;
Error:
    return 0;
}

int
unit_test__bytes_of_type__is_defined_for_all()
{
    for (int i = 0; i < SampleTypeCount; ++i) {
        // Check this isn't returning 0 for known counts
        CHECK(bytes_of_type(i) != 0);
    }
    return 1;
Error:
    return 0;
}

#endif // NO_UNIT_TESTS