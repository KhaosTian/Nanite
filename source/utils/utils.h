#pragma once

#include "pch.h"

#if defined(_DEBUG) || defined(DEBUG)
    #define Nanity_DEBUG 1
#else
    #define Nanity_DEBUG 0
#endif

#define RESTRICT __restrict

#ifdef _WIN32
    #include <windows.h>

inline bool IsDebuggerAttach() {
    BOOL bRemoteDebuggerPresent = FALSE;
    if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &bRemoteDebuggerPresent)) {
        return bRemoteDebuggerPresent;
    }
    return IsDebuggerPresent();
}
#endif

// 基本类型
namespace Nanity {
using int8  = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

using uint8  = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;
} // namespace Nanity

// 线代类型
namespace Nanity {
namespace Math = glm;
using Point2f  = Math::vec2;
using Point3f  = Math::vec3;
using Point4f  = Math::vec4;
using Vector2f = Math::vec2;
using Vector3f = Math::vec3;
using Vector4f = Math::vec4;
using Matrix3f = Math::mat3;
using Matrix4f = Math::mat4;

} // namespace Nanity

// 数学扩展
namespace Nanity {
inline static uint32 MurmurFinalize32(uint32 hash) {
    hash ^= hash >> 16;
    hash *= 0x85ebca6b;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35;
    hash ^= hash >> 16;
    return hash;
}

inline static uint32 Murmur32(std::initializer_list<uint32> init_list) {
    uint32 hash = 0;
    for (auto element: init_list) {
        element *= 0xcc9e2d51;
        element = (element << 15) | (element >> (32 - 15));
        element *= 0x1b873593;

        hash ^= element;
        hash = (hash << 13) | (hash >> (32 - 13));
        hash = hash * 5 + 0xe6546b64;
    }

    return MurmurFinalize32(hash);
}

template<class T>
inline static constexpr T DivideAndRoundUp(T Dividend, T Divisor) {
    return (Dividend + Divisor - 1) / Divisor;
}
} // namespace Nanity