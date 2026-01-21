#pragma once
#include <cstddef>
#include <cstring>

template <size_t Actual, size_t Expected>
struct CheckSize {
    static constexpr bool value = (Actual == Expected);
};

#define ASSERT_SIZE(type, expectedSize) \
    static_assert(CheckSize<sizeof(type), expectedSize>::value, \
        "Size of " #type " does not match expected size")

#define GENERATE_PADDING(size) uint8_t CONCAT(unknown_, __LINE__)[size]
#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a##b
