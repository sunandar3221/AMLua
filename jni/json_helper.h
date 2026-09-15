#pragma once

extern "C" {
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

namespace AMLua
{
    namespace Json
    {
        // Register Json API into Lua VM
        void RegisterLua(lua_State* L);
    }
}
