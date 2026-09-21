#pragma once

#if defined(_WIN32)
#include <intrin.h>
#endif

#include <cassert>
namespace JSlang::DebugHelpers
{

void BuiltinTrap()
{
#if defined(_WIN32)
    __debugbreak();
#elif defined(__clang__)
    __builtin_trap();
#endif
}
} // namespace JSlang::DebugHelpers
