#pragma once
#include <cstdint>

namespace eng {

// RGBA color, 8 bits per channel. Default is opaque black.
struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

} // namespace eng
