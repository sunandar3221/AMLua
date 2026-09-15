#pragma once

#include <stdint.h>
#include <chrono>
#include <vector>
#include <string>

extern "C" {
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
}

namespace AMLua
{
    namespace TouchManager
    {
        // Lifecycle
        void Init();
        void Shutdown(lua_State* L);

        // Raw touch event ingestion from AND_TouchEvent hook (thread-safe)
        void OnRawTouchEvent(int action, int trackNum, int x, int y);

        // Frame tick processing on main game thread (dispatches callbacks)
        void ProcessTick(lua_State* L, double dt);

        // Register Touch global table in Lua
        void RegisterLuaBindings(lua_State* L);

        // Screen dimensions & coordinate to zone mapping (1 to 9)
        void SetDisplaySize(int width, int height);
        void GetDisplaySize(int& width, int& height);
        int  GetZoneFromCoords(int x, int y);

        // Direct state queries (CLEO compatibility)
        bool   IsZonePressed(int zone, double minTimeMs = 0.0);
        int    GetZonePointState(int zone, double minTimeMs = 0.0);
        bool   IsSlideDetected(int fromZone, int toZone, double maxAgeMs = 600.0);
        int    GetSlideState(int fromZone, int toZone, double minTimeMs = 0.0, double maxTimeMs = 1500.0);
        int    GetActiveZone();
        double GetZoneHoldTime(int zone);

        // Mod list gesture toggle
        void SetModListGestureEnabled(bool enabled);
        bool IsModListGestureEnabled();
    }
}
