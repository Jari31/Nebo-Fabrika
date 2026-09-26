#pragma once

#include "sol/sol.hpp"
#include "sol/state.hpp"
#include "sol/types.hpp"

namespace JSlang
{
struct LuaJITEngine
{
    sol::state Lua;

    LuaJITEngine() { Lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::ffi); }
};

} // namespace JSlang
