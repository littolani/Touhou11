#pragma once
#include "Macros.h"
#include <cstdint>

struct RngContext
{
    uint16_t time;
    uint16_t padding;
    int32_t counter;
};
ASSERT_SIZE(RngContext, 0x8);