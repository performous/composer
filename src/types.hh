#pragma once

#ifdef WIN32

using int8_t = signed char;
using uint8_t = unsigned char;
using int16_t = short;
using uint16_t = unsigned short;
using int32_t = int;
using uint32_t = unsigned int;

#else

#include <cstdint>

#endif
