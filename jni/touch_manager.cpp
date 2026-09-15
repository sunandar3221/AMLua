#include "touch_manager.h"
#include "lua_bindings.h"
#include "crash_handler.h"
#include <mod/aml.h>
#include <cmath>
#include <algorithm>
#include <vector>
#include <mutex>
#include <chrono>

namespace AMLua
{
    namespace TouchManager
    {
        // ---------------------------------------------------------------------
        // Screen & Display Geometry
        // ---------------------------------------------------------------------
        static int s_ScreenWidth = 1920;
        static int s_ScreenHeight = 1080;
        static bool s_DisplaySizeQueried = false;

        static void UpdateDisplaySize()
        {
            if (aml)
            {
                int w = 0, h = 0;
                aml->GetDisplaySize(&w, &h);
                if (w > 0 && h > 0)
                {
                    s_ScreenWidth = w;
                    s_ScreenHeight = h;
                    s_DisplaySizeQueried = true;
                }
            }
        }

        void SetDisplaySize(int width, int height)
        {
            if (width > 0) s_ScreenWidth = width;
            if (height > 0) s_ScreenHeight = height;
        }

        void GetDisplaySize(int& width, int& height)
        {
            UpdateDisplaySize();
            width = s_ScreenWidth;
            height = s_ScreenHeight;
        }

        // Calculates zone number (1 to 9) using the standard Cleo 3x3 screen grid
        int GetZoneFromCoords(int x, int y)
        {
            UpdateDisplaySize();
            int w = s_ScreenWidth > 0 ? s_ScreenWidth : 1920;
            int h = s_ScreenHeight > 0 ? s_ScreenHeight : 1080;

            if (x < 0) x = 0;
            if (x >= w) x = w - 1;
            if (y < 0) y = 0;
            if (y >= h) y = h - 1;

            int col = (x * 3) / w;
            if (col < 0) col = 0;
            if (col > 2) col = 2;

            int row = (y * 3) / h;
            if (row < 0) row = 0;
            if (row > 2) row = 2;

            return row * 3 + col + 1; // 1 to 9
        }

        // ---------------------------------------------------------------------
        // Pointer & Zone State Tracking
        // ---------------------------------------------------------------------
        struct PointerState
        {
            bool active = false;
            int startX = 0;
            int startY = 0;
            int startZone = 0;
            int currentX = 0;
            int currentY = 0;
            int currentZone = 0;
            std::chrono::steady_clock::time_point startTime;
            std::chrono::steady_clock::time_point lastTime;
        };

        struct ZoneState
        {
            bool isPressed = false;
            int  activePointerCount = 0;
            std::chrono::steady_clock::time_point pressTime;
            std::chrono::steady_clock::time_point releaseTime;
            double holdDurationMs = 0.0;
        };

        struct SlideRecord
        {
            int fromZone = 0;
            int toZone = 0;
            double durationMs = 0.0;
            std::chrono::steady_clock::time_point time;
            bool consumed = false;
        };

        static constexpr int MAX_POINTERS = 10;
        static PointerState s_Pointers[MAX_POINTERS];
        static ZoneState    s_Zones[10]; // 1-indexed (1 to 9)
        static std::vector<SlideRecord> s_RecentSlides;

        // ---------------------------------------------------------------------
        // Raw Event Queue (Thread-Safe ingestion from hook)
        // ---------------------------------------------------------------------
        struct RawEvent
        {
            int action;
            int trackNum;
            int x;
            int y;
            std::chrono::steady_clock::time_point time;
        };

        static std::vector<RawEvent> s_RawEventQueue;
        static std::mutex s_EventMutex;
        static bool s_ModListGestureEnabled = true;

        void OnRawTouchEvent(int action, int trackNum, int x, int y)
        {
            if (x + 1 > s_ScreenWidth) s_ScreenWidth = x + 1;
            if (y + 1 > s_ScreenHeight) s_ScreenHeight = y + 1;

            std::lock_guard<std::mutex> lock(s_EventMutex);
            s_RawEventQueue.push_back({action, trackNum, x, y, std::chrono::steady_clock::now()});
        }

        // ---------------------------------------------------------------------
        // Registered Lua Event Listeners
        // ---------------------------------------------------------------------
        struct TouchListener
        {
            int id = 0;
            int zone = 0; // 0 = any zone
            int luaRef = LUA_NOREF;
        };

        struct HoldListener
        {
            int id = 0;
            int zone = 0;
            double targetDurationMs = 0.0;
            int luaRef = LUA_NOREF;
            bool triggeredThisPress = false;
        };

        struct SlideListener
        {
            int id = 0;
            int fromZone = 0; // 0 = any
            int toZone = 0;   // 0 = any
            int luaRef = LUA_NOREF;
        };

        struct ComboListener
        {
            int id = 0;
            std::vector<int> zones;
            int luaRef = LUA_NOREF;
            bool wasActive = false;
        };

        static int s_NextListenerId = 1;
        static std::vector<TouchListener> s_PressListeners;
        static std::vector<TouchListener> s_ReleaseListeners;
        static std::vector<TouchListener> s_DoubleTapListeners;
        static std::vector<HoldListener>  s_HoldListeners;
        static std::vector<SlideListener> s_SlideListeners;
        static std::vector<ComboListener> s_ComboListeners;

        // Traceback error handler index for safe pcall
        static int GetTracebackIndex(lua_State* L)
        {
            lua_getglobal(L, "debug");
            if (lua_istable(L, -1))
            {
                lua_getfield(L, -1, "traceback");
                lua_remove(L, -2);
                return lua_gettop(L);
            }
            lua_pop(L, 1);
            return 0;
        }

        static void SafeCallLuaRef(lua_State* L, int luaRef, int argCount, const char* context)
        {
            if (luaRef == LUA_NOREF || luaRef == LUA_REFNIL)
            {
                lua_pop(L, argCount);
                return;
            }

            int errH = GetTracebackIndex(L);
            int funcIdx = lua_gettop(L) - argCount;

            lua_rawgeti(L, LUA_REGISTRYINDEX, luaRef);
            lua_insert(L, funcIdx); // Place function right before arguments

            if (errH > 0)
            {
                lua_insert(L, funcIdx); // Place traceback before function
                errH = funcIdx;
            }

            CrashHandler::SetCurrentAction(context);
            if (lua_pcall(L, argCount, 0, errH) != LUA_OK)
            {
                const char* err = lua_tostring(L, -1);
                Log("[Touch Error - %s]: %s", context, err ? err : "Unknown error");
                CrashHandler::LogError("[Touch Error - %s]: %s", context, err ? err : "Unknown error");
                lua_pop(L, 1); // pop error
            }

            if (errH > 0)
            {
                lua_remove(L, errH); // pop traceback handler
            }

            CrashHandler::ClearContext();
        }

        // Global Lua functions: onTouchZone(zone, event, ...), onTouchSlide(from, to, duration)
        static void InvokeGlobalScriptHookZone(lua_State* L, int zone, const char* eventType, int x = 0, int y = 0, double extra = 0.0)
        {
            lua_getglobal(L, "onTouchZone");
            if (lua_isfunction(L, -1))
            {
                int errH = GetTracebackIndex(L);
                if (errH > 0) lua_insert(L, -2);

                lua_pushinteger(L, zone);
                lua_pushstring(L, eventType);
                lua_pushinteger(L, x);
                lua_pushinteger(L, y);
                lua_pushnumber(L, extra);

                CrashHandler::SetCurrentAction("Global onTouchZone");
                if (lua_pcall(L, 5, 0, errH) != LUA_OK)
                {
                    const char* err = lua_tostring(L, -1);
                    Log("[onTouchZone Error]: %s", err ? err : "Unknown error");
                    CrashHandler::LogError("[onTouchZone Error]: %s", err ? err : "Unknown error");
                    lua_pop(L, 1);
                }
                if (errH > 0) lua_pop(L, 1);
                CrashHandler::ClearContext();
            }
            else
            {
                lua_pop(L, 1);
            }
        }

        static bool InvokeGlobalScriptHookSlide(lua_State* L, int fromZone, int toZone, double durationMs)
        {
            bool handled = false;
            lua_getglobal(L, "onTouchSlide");
            if (lua_isfunction(L, -1))
            {
                int errH = GetTracebackIndex(L);
                if (errH > 0) lua_insert(L, -2);

                lua_pushinteger(L, fromZone);
                lua_pushinteger(L, toZone);
                lua_pushnumber(L, durationMs);

                CrashHandler::SetCurrentAction("Global onTouchSlide");
                if (lua_pcall(L, 3, 0, errH) != LUA_OK)
                {
                    const char* err = lua_tostring(L, -1);
                    Log("[onTouchSlide Error]: %s", err ? err : "Unknown error");
                    CrashHandler::LogError("[onTouchSlide Error]: %s", err ? err : "Unknown error");
                    lua_pop(L, 1);
                }
                else
                {
                    handled = true;
                }
                if (errH > 0) lua_pop(L, 1);
                CrashHandler::ClearContext();
            }
            else
            {
                lua_pop(L, 1);
            }
            return handled;
        }

        // ---------------------------------------------------------------------
        // Frame Tick Processing (Main Thread)
        // ---------------------------------------------------------------------
        void ProcessTick(lua_State* L, double dt)
        {
            if (!L) return;

            auto now = std::chrono::steady_clock::now();

            // 1. Drain raw touch event queue
            std::vector<RawEvent> events;
            {
                std::lock_guard<std::mutex> lock(s_EventMutex);
                events.swap(s_RawEventQueue);
            }

            for (const auto& ev : events)
            {
                int track = ev.trackNum;
                if (track < 0 || track >= MAX_POINTERS) track = 0;

                int zone = GetZoneFromCoords(ev.x, ev.y);

                if (ev.action == 0) // DOWN
                {
                    s_Pointers[track].active = true;
                    s_Pointers[track].startX = ev.x;
                    s_Pointers[track].startY = ev.y;
                    s_Pointers[track].startZone = zone;
                    s_Pointers[track].currentX = ev.x;
                    s_Pointers[track].currentY = ev.y;
                    s_Pointers[track].currentZone = zone;
                    s_Pointers[track].startTime = ev.time;
                    s_Pointers[track].lastTime = ev.time;

                    // Update zone state
                    bool wasPressed = s_Zones[zone].isPressed;
                    s_Zones[zone].isPressed = true;
                    s_Zones[zone].activePointerCount++;
                    if (!wasPressed)
                    {
                        s_Zones[zone].pressTime = ev.time;
                        s_Zones[zone].holdDurationMs = 0.0;

                        // Check for Double-Tap in this zone (< 400ms since last release)
                        double timeSinceRelease = std::chrono::duration<double, std::milli>(ev.time - s_Zones[zone].releaseTime).count();
                        if (timeSinceRelease >= 30.0 && timeSinceRelease <= 400.0)
                        {
                            // Trigger OnDoubleTap listeners
                            for (const auto& l : s_DoubleTapListeners)
                            {
                                if (l.zone == 0 || l.zone == zone)
                                {
                                    lua_pushinteger(L, zone);
                                    lua_pushinteger(L, ev.x);
                                    lua_pushinteger(L, ev.y);
                                    SafeCallLuaRef(L, l.luaRef, 3, "Touch.OnDoubleTap");
                                }
                            }
                            InvokeGlobalScriptHookZone(L, zone, "doubletap", ev.x, ev.y, timeSinceRelease);
                        }

                        // Trigger OnPress listeners
                        for (const auto& l : s_PressListeners)
                        {
                            if (l.zone == 0 || l.zone == zone)
                            {
                                lua_pushinteger(L, zone);
                                lua_pushinteger(L, ev.x);
                                lua_pushinteger(L, ev.y);
                                SafeCallLuaRef(L, l.luaRef, 3, "Touch.OnPress");
                            }
                        }

                        // Global script callback
                        InvokeGlobalScriptHookZone(L, zone, "press", ev.x, ev.y);
                    }

                    // Reset hold triggered status for hold listeners on this zone
                    for (auto& h : s_HoldListeners)
                    {
                        if (h.zone == zone)
                        {
                            h.triggeredThisPress = false;
                        }
                    }
                }
                else if (ev.action == 1) // MOVE
                {
                    if (s_Pointers[track].active)
                    {
                        int oldZone = s_Pointers[track].currentZone;
                        s_Pointers[track].currentX = ev.x;
                        s_Pointers[track].currentY = ev.y;
                        s_Pointers[track].currentZone = zone;
                        s_Pointers[track].lastTime = ev.time;

                        if (oldZone != zone)
                        {
                            if (s_Zones[oldZone].activePointerCount > 0)
                            {
                                s_Zones[oldZone].activePointerCount--;
                                if (s_Zones[oldZone].activePointerCount == 0)
                                {
                                    s_Zones[oldZone].isPressed = false;
                                    s_Zones[oldZone].releaseTime = ev.time;
                                }
                            }
                            s_Zones[zone].activePointerCount++;
                            s_Zones[zone].isPressed = true;
                        }
                    }
                }
                else if (ev.action == 2 || ev.action == 3) // UP or CANCEL
                {
                    if (s_Pointers[track].active)
                    {
                        int startZone = s_Pointers[track].startZone;
                        int endZone = zone;
                        double durationMs = std::chrono::duration<double, std::milli>(ev.time - s_Pointers[track].startTime).count();
                        int dx = ev.x - s_Pointers[track].startX;
                        int dy = ev.y - s_Pointers[track].startY;
                        double dist = std::sqrt((double)(dx * dx + dy * dy));

                        // Slide detection: must cross zones, cover distance (> 7% of screen min dimension), and be within time window
                        double minDist = 0.07 * (double)std::min(s_ScreenWidth, s_ScreenHeight);
                        if (startZone != endZone && dist >= minDist && durationMs >= 40.0 && durationMs <= 1500.0)
                        {
                            s_RecentSlides.push_back({startZone, endZone, durationMs, ev.time, false});

                            bool handled = false;
                            // Trigger registered slide listeners
                            for (const auto& l : s_SlideListeners)
                            {
                                if ((l.fromZone == 0 || l.fromZone == startZone) &&
                                    (l.toZone == 0 || l.toZone == endZone))
                                {
                                    lua_pushinteger(L, startZone);
                                    lua_pushinteger(L, endZone);
                                    lua_pushnumber(L, durationMs);
                                    SafeCallLuaRef(L, l.luaRef, 3, "Touch.OnSlide");
                                    handled = true;
                                }
                            }

                            // Trigger global onTouchSlide
                            if (InvokeGlobalScriptHookSlide(L, startZone, endZone, durationMs))
                            {
                                handled = true;
                            }

                            // Built-in Cleo-style gesture (Slide 2 to 8) to open AMLua script list
                            if (!handled && s_ModListGestureEnabled && startZone == 2 && endZone == 8)
                            {
                                ShowScriptListDialog();
                            }
                        }

                        // Release zone
                        if (s_Zones[endZone].activePointerCount > 0)
                        {
                            s_Zones[endZone].activePointerCount--;
                        }
                        if (s_Zones[endZone].activePointerCount == 0)
                        {
                            s_Zones[endZone].isPressed = false;
                            s_Zones[endZone].releaseTime = ev.time;

                            double holdMs = std::chrono::duration<double, std::milli>(ev.time - s_Zones[endZone].pressTime).count();

                            // Trigger OnRelease listeners
                            for (const auto& l : s_ReleaseListeners)
                            {
                                if (l.zone == 0 || l.zone == endZone)
                                {
                                    lua_pushinteger(L, endZone);
                                    lua_pushnumber(L, holdMs);
                                    SafeCallLuaRef(L, l.luaRef, 2, "Touch.OnRelease");
                                }
                            }

                            InvokeGlobalScriptHookZone(L, endZone, "release", ev.x, ev.y, holdMs);
                        }

                        s_Pointers[track].active = false;
                    }
                }
            }

            // 2. Update hold duration and trigger OnHold listeners
            for (int z = 1; z <= 9; ++z)
            {
                if (s_Zones[z].isPressed)
                {
                    double holdMs = std::chrono::duration<double, std::milli>(now - s_Zones[z].pressTime).count();
                    s_Zones[z].holdDurationMs = holdMs;

                    for (auto& h : s_HoldListeners)
                    {
                        if ((h.zone == 0 || h.zone == z) && !h.triggeredThisPress && holdMs >= h.targetDurationMs)
                        {
                            h.triggeredThisPress = true;
                            lua_pushinteger(L, z);
                            lua_pushnumber(L, holdMs);
                            SafeCallLuaRef(L, h.luaRef, 2, "Touch.OnHold");
                        }
                    }
                }
            }

            // 3. Update Multi-zone Combos (e.g. {4, 6})
            for (auto& c : s_ComboListeners)
            {
                bool allPressed = true;
                for (int requiredZone : c.zones)
                {
                    if (requiredZone >= 1 && requiredZone <= 9 && !s_Zones[requiredZone].isPressed)
                    {
                        allPressed = false;
                        break;
                    }
                }

                if (allPressed && !c.wasActive)
                {
                    c.wasActive = true;
                    // Push table of zones as argument
                    lua_newtable(L);
                    for (size_t i = 0; i < c.zones.size(); ++i)
                    {
                        lua_pushinteger(L, c.zones[i]);
                        lua_rawseti(L, -2, (int)(i + 1));
                    }
                    SafeCallLuaRef(L, c.luaRef, 1, "Touch.OnCombo");
                }
                else if (!allPressed)
                {
                    c.wasActive = false;
                }
            }

            // 4. Prune stale slides (> 3 seconds)
            s_RecentSlides.erase(
                std::remove_if(s_RecentSlides.begin(), s_RecentSlides.end(),
                    [&now](const SlideRecord& r) {
                        return std::chrono::duration<double>(now - r.time).count() > 3.0;
                    }),
                s_RecentSlides.end()
            );
        }

        // ---------------------------------------------------------------------
        // State Queries
        // ---------------------------------------------------------------------
        bool IsZonePressed(int zone, double minTimeMs)
        {
            if (zone < 1 || zone > 9) return false;
            if (!s_Zones[zone].isPressed) return false;
            if (minTimeMs <= 0.0) return true;
            return s_Zones[zone].holdDurationMs >= minTimeMs;
        }

        int GetZonePointState(int zone, double minTimeMs)
        {
            return IsZonePressed(zone, minTimeMs) ? 1 : 0;
        }

        bool IsSlideDetected(int fromZone, int toZone, double maxAgeMs)
        {
            auto now = std::chrono::steady_clock::now();
            for (auto& s : s_RecentSlides)
            {
                if (!s.consumed)
                {
                    if ((fromZone == 0 || s.fromZone == fromZone) &&
                        (toZone == 0 || s.toZone == toZone))
                    {
                        double age = std::chrono::duration<double, std::milli>(now - s.time).count();
                        if (age <= maxAgeMs)
                        {
                            s.consumed = true;
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        int GetSlideState(int fromZone, int toZone, double minTimeMs, double maxTimeMs)
        {
            auto now = std::chrono::steady_clock::now();
            for (auto& s : s_RecentSlides)
            {
                if (!s.consumed)
                {
                    if ((fromZone == 0 || s.fromZone == fromZone) &&
                        (toZone == 0 || s.toZone == toZone))
                    {
                        double age = std::chrono::duration<double, std::milli>(now - s.time).count();
                        if (age <= 1000.0 && s.durationMs >= minTimeMs && s.durationMs <= maxTimeMs)
                        {
                            s.consumed = true;
                            return 1;
                        }
                    }
                }
            }
            return 0;
        }

        int GetActiveZone()
        {
            for (int z = 1; z <= 9; ++z)
            {
                if (s_Zones[z].isPressed) return z;
            }
            return 0;
        }

        double GetZoneHoldTime(int zone)
        {
            if (zone < 1 || zone > 9) return 0.0;
            if (!s_Zones[zone].isPressed) return 0.0;
            return s_Zones[zone].holdDurationMs;
        }

        void SetModListGestureEnabled(bool enabled)
        {
            s_ModListGestureEnabled = enabled;
        }

        bool IsModListGestureEnabled()
        {
            return s_ModListGestureEnabled;
        }

        // ---------------------------------------------------------------------
        // Lua C API Bindings for 'Touch'
        // ---------------------------------------------------------------------

        // Touch.IsPressed(zone, [minTimeMs]) -> boolean
        static int Lua_Touch_IsPressed(lua_State* L)
        {
            int zone = (int)luaL_checkinteger(L, 1);
            double minTimeMs = lua_isnoneornil(L, 2) ? 0.0 : (double)luaL_checknumber(L, 2);
            lua_pushboolean(L, IsZonePressed(zone, minTimeMs) ? 1 : 0);
            return 1;
        }

        // Touch.GetPointState(zone, [minTimeMs]) -> 1 or 0 (Cleo 0DE0 direct equivalent)
        static int Lua_Touch_GetPointState(lua_State* L)
        {
            int zone = (int)luaL_checkinteger(L, 1);
            double minTimeMs = lua_isnoneornil(L, 2) ? 0.0 : (double)luaL_checknumber(L, 2);
            lua_pushinteger(L, GetZonePointState(zone, minTimeMs));
            return 1;
        }

        // Touch.IsSlide(fromZone, toZone, [maxAgeMs]) -> boolean
        static int Lua_Touch_IsSlide(lua_State* L)
        {
            int fromZone = (int)luaL_checkinteger(L, 1);
            int toZone = (int)luaL_checkinteger(L, 2);
            double maxAgeMs = lua_isnoneornil(L, 3) ? 600.0 : (double)luaL_checknumber(L, 3);
            lua_pushboolean(L, IsSlideDetected(fromZone, toZone, maxAgeMs) ? 1 : 0);
            return 1;
        }

        // Touch.GetSlideState(fromZone, toZone, [minTimeMs], [maxTimeMs]) -> 1 or 0 (Cleo 0DE1 equivalent)
        static int Lua_Touch_GetSlideState(lua_State* L)
        {
            int fromZone = (int)luaL_checkinteger(L, 1);
            int toZone = (int)luaL_checkinteger(L, 2);
            double minTimeMs = lua_isnoneornil(L, 3) ? 0.0 : (double)luaL_checknumber(L, 3);
            double maxTimeMs = lua_isnoneornil(L, 4) ? 1500.0 : (double)luaL_checknumber(L, 4);
            lua_pushinteger(L, GetSlideState(fromZone, toZone, minTimeMs, maxTimeMs));
            return 1;
        }

        // Touch.GetZone() -> number (1-9 or 0 if none)
        static int Lua_Touch_GetZone(lua_State* L)
        {
            lua_pushinteger(L, GetActiveZone());
            return 1;
        }

        // Touch.GetPressedZones() -> table of active zones {1, 5}
        static int Lua_Touch_GetPressedZones(lua_State* L)
        {
            lua_newtable(L);
            int idx = 1;
            for (int z = 1; z <= 9; ++z)
            {
                if (s_Zones[z].isPressed)
                {
                    lua_pushinteger(L, z);
                    lua_rawseti(L, -2, idx++);
                }
            }
            return 1;
        }

        // Touch.GetHoldTime(zone) -> number (ms)
        static int Lua_Touch_GetHoldTime(lua_State* L)
        {
            int zone = (int)luaL_checkinteger(L, 1);
            lua_pushnumber(L, GetZoneHoldTime(zone));
            return 1;
        }

        // Touch.GetPos([pointerIndex]) -> x, y, normX, normY, zone
        static int Lua_Touch_GetPos(lua_State* L)
        {
            int track = lua_isnoneornil(L, 1) ? 0 : (int)luaL_checkinteger(L, 1);
            if (track < 0 || track >= MAX_POINTERS) track = 0;

            int x = s_Pointers[track].currentX;
            int y = s_Pointers[track].currentY;
            int zone = s_Pointers[track].active ? s_Pointers[track].currentZone : 0;

            int w = s_ScreenWidth > 0 ? s_ScreenWidth : 1920;
            int h = s_ScreenHeight > 0 ? s_ScreenHeight : 1080;

            lua_pushinteger(L, x);
            lua_pushinteger(L, y);
            lua_pushnumber(L, (double)x / (double)w);
            lua_pushnumber(L, (double)y / (double)h);
            lua_pushinteger(L, zone);
            return 5;
        }

        // Touch.IsAnyPressed() -> boolean
        static int Lua_Touch_IsAnyPressed(lua_State* L)
        {
            bool any = false;
            for (int z = 1; z <= 9; ++z)
            {
                if (s_Zones[z].isPressed) { any = true; break; }
            }
            lua_pushboolean(L, any ? 1 : 0);
            return 1;
        }

        // Touch.OnPress(zone, callback) -> listenerId
        static int Lua_Touch_OnPress(lua_State* L)
        {
            int zone = (int)luaL_checkinteger(L, 1);
            luaL_checktype(L, 2, LUA_TFUNCTION);

            lua_pushvalue(L, 2);
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);

            int id = s_NextListenerId++;
            s_PressListeners.push_back({id, zone, ref});

            lua_pushinteger(L, id);
            return 1;
        }

        // Touch.OnRelease(zone, callback) -> listenerId
        static int Lua_Touch_OnRelease(lua_State* L)
        {
            int zone = (int)luaL_checkinteger(L, 1);
            luaL_checktype(L, 2, LUA_TFUNCTION);

            lua_pushvalue(L, 2);
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);

            int id = s_NextListenerId++;
            s_ReleaseListeners.push_back({id, zone, ref});

            lua_pushinteger(L, id);
            return 1;
        }

        // Touch.OnHold(zone, durationMs, callback) -> listenerId
        static int Lua_Touch_OnHold(lua_State* L)
        {
            int zone = (int)luaL_checkinteger(L, 1);
            double durationMs = (double)luaL_checknumber(L, 2);
            luaL_checktype(L, 3, LUA_TFUNCTION);

            lua_pushvalue(L, 3);
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);

            int id = s_NextListenerId++;
            s_HoldListeners.push_back({id, zone, durationMs, ref, false});

            lua_pushinteger(L, id);
            return 1;
        }

        // Touch.OnSlide(fromZone, toZone, callback) -> listenerId
        static int Lua_Touch_OnSlide(lua_State* L)
        {
            int fromZone = (int)luaL_checkinteger(L, 1);
            int toZone = (int)luaL_checkinteger(L, 2);
            luaL_checktype(L, 3, LUA_TFUNCTION);

            lua_pushvalue(L, 3);
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);

            int id = s_NextListenerId++;
            s_SlideListeners.push_back({id, fromZone, toZone, ref});

            lua_pushinteger(L, id);
            return 1;
        }

        // Touch.OnDoubleTap(zone, callback) -> listenerId
        static int Lua_Touch_OnDoubleTap(lua_State* L)
        {
            int zone = (int)luaL_checkinteger(L, 1);
            luaL_checktype(L, 2, LUA_TFUNCTION);

            lua_pushvalue(L, 2);
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);

            int id = s_NextListenerId++;
            s_DoubleTapListeners.push_back({id, zone, ref});

            lua_pushinteger(L, id);
            return 1;
        }

        // Touch.OnCombo(tableOfZones, callback) -> listenerId
        static int Lua_Touch_OnCombo(lua_State* L)
        {
            luaL_checktype(L, 1, LUA_TTABLE);
            luaL_checktype(L, 2, LUA_TFUNCTION);

            std::vector<int> zones;
            int len = (int)lua_rawlen(L, 1);
            for (int i = 1; i <= len; ++i)
            {
                lua_rawgeti(L, 1, i);
                if (lua_isinteger(L, -1))
                {
                    int z = (int)lua_tointeger(L, -1);
                    if (z >= 1 && z <= 9) zones.push_back(z);
                }
                lua_pop(L, 1);
            }

            lua_pushvalue(L, 2);
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);

            int id = s_NextListenerId++;
            s_ComboListeners.push_back({id, zones, ref, false});

            lua_pushinteger(L, id);
            return 1;
        }

        // Touch.RemoveListener(id) -> boolean
        static int Lua_Touch_RemoveListener(lua_State* L)
        {
            int id = (int)luaL_checkinteger(L, 1);
            bool removed = false;

            auto removeListener = [L, id, &removed](auto& vec) {
                for (auto it = vec.begin(); it != vec.end(); )
                {
                    if (it->id == id)
                    {
                        if (it->luaRef != LUA_NOREF && it->luaRef != LUA_REFNIL)
                        {
                            luaL_unref(L, LUA_REGISTRYINDEX, it->luaRef);
                        }
                        it = vec.erase(it);
                        removed = true;
                    }
                    else
                    {
                        ++it;
                    }
                }
            };

            removeListener(s_PressListeners);
            removeListener(s_ReleaseListeners);
            removeListener(s_DoubleTapListeners);
            removeListener(s_HoldListeners);
            removeListener(s_SlideListeners);
            removeListener(s_ComboListeners);

            lua_pushboolean(L, removed ? 1 : 0);
            return 1;
        }

        // Touch.ClearListeners()
        static int Lua_Touch_ClearListeners(lua_State* L)
        {
            auto clearVec = [L](auto& vec) {
                for (auto& item : vec)
                {
                    if (item.luaRef != LUA_NOREF && item.luaRef != LUA_REFNIL)
                    {
                        luaL_unref(L, LUA_REGISTRYINDEX, item.luaRef);
                    }
                }
                vec.clear();
            };

            clearVec(s_PressListeners);
            clearVec(s_ReleaseListeners);
            clearVec(s_DoubleTapListeners);
            clearVec(s_HoldListeners);
            clearVec(s_SlideListeners);
            clearVec(s_ComboListeners);

            return 0;
        }

        // Touch.SetModListEnabled(boolean)
        static int Lua_Touch_SetModListEnabled(lua_State* L)
        {
            bool enable = lua_toboolean(L, 1) != 0;
            SetModListGestureEnabled(enable);
            return 0;
        }

        // Touch.ShowGrid([durationMs])
        static int Lua_Touch_ShowGrid(lua_State* L)
        {
            unsigned int duration = lua_isnoneornil(L, 1) ? 5000 : (unsigned int)luaL_checkinteger(L, 1);
            DisplayHelpBox(
                "~y~AMLua Touch Screen Zones (1-9):~n~"
                "~w~[1] Atas-Kiri  [2] Atas-Tgh  [3] Atas-Kanan~n~"
                "[4] Tgh-Kiri   [5] Tengah    [6] Tgh-Kanan~n~"
                "[7] Bwh-Kiri  [8] Bwh-Tgh   [9] Bwh-Kanan~n~"
                "~g~Geser 2 ke 8: Menu Cleo",
                duration
            );
            return 0;
        }

        // Touch.GetDisplaySize() -> w, h
        static int Lua_Touch_GetDisplaySize(lua_State* L)
        {
            UpdateDisplaySize();
            lua_pushinteger(L, s_ScreenWidth);
            lua_pushinteger(L, s_ScreenHeight);
            return 2;
        }

        // ---------------------------------------------------------------------
        // Registration & Lifecycle
        // ---------------------------------------------------------------------
        void Init()
        {
            UpdateDisplaySize();
            s_RecentSlides.clear();
            for (int i = 0; i < MAX_POINTERS; ++i) s_Pointers[i] = PointerState{};
            for (int i = 0; i < 10; ++i) s_Zones[i] = ZoneState{};
        }

        void Shutdown(lua_State* L)
        {
            if (L)
            {
                Lua_Touch_ClearListeners(L);
            }
            s_RecentSlides.clear();
            {
                std::lock_guard<std::mutex> lock(s_EventMutex);
                s_RawEventQueue.clear();
            }
        }

        void RegisterLuaBindings(lua_State* L)
        {
            lua_newtable(L);

            // State query methods
            lua_pushcfunction(L, Lua_Touch_IsPressed);
            lua_setfield(L, -2, "IsPressed");

            lua_pushcfunction(L, Lua_Touch_IsPressed);
            lua_setfield(L, -2, "IsZonePressed"); // alias

            lua_pushcfunction(L, Lua_Touch_GetPointState);
            lua_setfield(L, -2, "GetPointState"); // Cleo 0DE0 match

            lua_pushcfunction(L, Lua_Touch_IsSlide);
            lua_setfield(L, -2, "IsSlide");

            lua_pushcfunction(L, Lua_Touch_IsSlide);
            lua_setfield(L, -2, "IsSwipe"); // alias

            lua_pushcfunction(L, Lua_Touch_GetSlideState);
            lua_setfield(L, -2, "GetSlideState"); // Cleo 0DE1 match

            lua_pushcfunction(L, Lua_Touch_GetZone);
            lua_setfield(L, -2, "GetZone");

            lua_pushcfunction(L, Lua_Touch_GetZone);
            lua_setfield(L, -2, "GetActiveZone"); // alias

            lua_pushcfunction(L, Lua_Touch_GetPressedZones);
            lua_setfield(L, -2, "GetPressedZones");

            lua_pushcfunction(L, Lua_Touch_GetHoldTime);
            lua_setfield(L, -2, "GetHoldTime");

            lua_pushcfunction(L, Lua_Touch_GetPos);
            lua_setfield(L, -2, "GetPos");

            lua_pushcfunction(L, Lua_Touch_IsAnyPressed);
            lua_setfield(L, -2, "IsAnyPressed");

            lua_pushcfunction(L, Lua_Touch_GetDisplaySize);
            lua_setfield(L, -2, "GetDisplaySize");

            // Event listener registration
            lua_pushcfunction(L, Lua_Touch_OnPress);
            lua_setfield(L, -2, "OnPress");

            lua_pushcfunction(L, Lua_Touch_OnPress);
            lua_setfield(L, -2, "OnZone"); // alias

            lua_pushcfunction(L, Lua_Touch_OnRelease);
            lua_setfield(L, -2, "OnRelease");

            lua_pushcfunction(L, Lua_Touch_OnHold);
            lua_setfield(L, -2, "OnHold");

            lua_pushcfunction(L, Lua_Touch_OnSlide);
            lua_setfield(L, -2, "OnSlide");

            lua_pushcfunction(L, Lua_Touch_OnSlide);
            lua_setfield(L, -2, "OnSwipe"); // alias

            lua_pushcfunction(L, Lua_Touch_OnDoubleTap);
            lua_setfield(L, -2, "OnDoubleTap");

            lua_pushcfunction(L, Lua_Touch_OnCombo);
            lua_setfield(L, -2, "OnCombo");

            lua_pushcfunction(L, Lua_Touch_RemoveListener);
            lua_setfield(L, -2, "RemoveListener");

            lua_pushcfunction(L, Lua_Touch_ClearListeners);
            lua_setfield(L, -2, "ClearListeners");

            lua_pushcfunction(L, Lua_Touch_SetModListEnabled);
            lua_setfield(L, -2, "SetModListEnabled");

            lua_pushcfunction(L, Lua_Touch_ShowGrid);
            lua_setfield(L, -2, "ShowGrid");

            // 3x3 Grid Zone Constants
            lua_pushinteger(L, 1); lua_setfield(L, -2, "ZONE_TOP_LEFT");
            lua_pushinteger(L, 2); lua_setfield(L, -2, "ZONE_TOP_CENTER");
            lua_pushinteger(L, 3); lua_setfield(L, -2, "ZONE_TOP_RIGHT");
            lua_pushinteger(L, 4); lua_setfield(L, -2, "ZONE_CENTER_LEFT");
            lua_pushinteger(L, 5); lua_setfield(L, -2, "ZONE_CENTER");
            lua_pushinteger(L, 6); lua_setfield(L, -2, "ZONE_CENTER_RIGHT");
            lua_pushinteger(L, 7); lua_setfield(L, -2, "ZONE_BOTTOM_LEFT");
            lua_pushinteger(L, 8); lua_setfield(L, -2, "ZONE_BOTTOM_CENTER");
            lua_pushinteger(L, 9); lua_setfield(L, -2, "ZONE_BOTTOM_RIGHT");

            // Register global 'Touch' table
            lua_setglobal(L, "Touch");
        }
    }
}
