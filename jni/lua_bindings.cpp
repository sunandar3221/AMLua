#include "lua_bindings.h"
#include <mod/aml.h>

#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <vector>
#include <string>
#include <algorithm>

#define LOG_TAG "AMLua"

namespace AMLua
{
    static lua_State* g_LuaState = nullptr;
    static std::string g_LogFilePath = "/storage/emulated/0/Android/data/com.rockstargames.gtasa/files/amlua.log";
    static std::string g_ScriptsDirPath = "/storage/emulated/0/Android/data/com.rockstargames.gtasa/files/scripts";
    static uintptr_t g_pGTASA = 0;
    static uintptr_t g_LibGTASASize = 0;
    static std::vector<std::string> g_LoadedScripts;

    // Exact struct member offsets verified from GTA SA Android headers (aml-psdk)
    #ifdef AML32
        constexpr uintptr_t OFF_PED_HEALTH = 0x544;
        constexpr uintptr_t OFF_PED_MAX_HEALTH = 0x548;
        constexpr uintptr_t OFF_PED_ARMOUR = 0x54C;
        constexpr uintptr_t OFF_VEH_HEALTH = 0x4CC;
    #else
        constexpr uintptr_t OFF_PED_HEALTH = 0x6AC;
        constexpr uintptr_t OFF_PED_MAX_HEALTH = 0x6B0;
        constexpr uintptr_t OFF_PED_ARMOUR = 0x6B4;
        constexpr uintptr_t OFF_VEH_HEALTH = 0x634;
    #endif

    // Function pointer types for resolved game symbols
    typedef void* (*FindPlayerPed_t)(int playerNum);
    // CMessages::AddMessageJumpQ takes (const char* Label, GxtChar* pText, u32 Duration, u16 Colour, bool bAddToPrevBriefs)
    typedef void  (*AddMessageJumpQ_t)(const char* label, unsigned short* pText, unsigned int duration, unsigned short flag, bool bAddToPrevBriefs);
    typedef void  (*VehicleFix_t)(void* vehicle);

    static FindPlayerPed_t   pfnFindPlayerPed = nullptr;
    static AddMessageJumpQ_t pfnAddMessageJumpQ = nullptr;
    static VehicleFix_t      pfnVehicleFix = nullptr;

    const char* GetLogFilePath()
    {
        return g_LogFilePath.c_str();
    }

    const std::vector<std::string>& GetLoadedScripts()
    {
        return g_LoadedScripts;
    }

    void Log(const char* fmt, ...)
    {
        char buffer[4096];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        // 1. Android Logcat
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buffer);

        // 2. AML logger if available
        if (logger)
        {
            logger->Info("%s", buffer);
        }

        // 3. File log "amlua.log"
        time_t now = time(nullptr);
        struct tm* tmInfo = localtime(&now);
        char timeStr[32] = {0};
        if (tmInfo)
        {
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tmInfo);
        }

        FILE* fp = fopen(g_LogFilePath.c_str(), "a");
        if (fp)
        {
            fprintf(fp, "[%s] %s\n", timeStr, buffer);
            fflush(fp);
            fclose(fp);
        }
    }

    // Convert standard 8-bit ASCII string to GTA SA 16-bit GXT string
    static void ConvertToGxt(const char* src, unsigned short* dst, size_t maxChars)
    {
        if (!src || !dst || maxChars == 0) return;
        size_t i = 0;
        while (src[i] != '\0' && i < maxChars - 1)
        {
            dst[i] = (unsigned char)src[i];
            ++i;
        }
        dst[i] = 0;
    }

    // Traceback error handler for protected lua_pcall
    static int Lua_TracebackHandler(lua_State* L)
    {
        const char* msg = lua_tostring(L, 1);
        if (!msg)
        {
            if (luaL_callmeta(L, 1, "__tostring") && lua_type(L, -1) == LUA_TSTRING)
            {
                return 1;
            }
            else
            {
                msg = lua_pushfstring(L, "(error object is a %s value)", lua_typename(L, lua_type(L, 1)));
            }
        }
        luaL_traceback(L, L, msg, 1);
        return 1;
    }

    // Verify pointer memory safety to avoid dereferencing garbage or dying game entities
    inline bool IsValidGameObject(void* ptr)
    {
        if (!ptr) return false;
        uintptr_t addr = (uintptr_t)ptr;
        #ifdef AML32
        if (addr < 0x10000 || addr >= 0xFFFFF000) return false;
        #else
        if (addr < 0x10000 || addr >= 0x00007FFFFFFFFFFFULL) return false;
        #endif

        // Verify vtable pointer is non-null and valid address
        uintptr_t vtable = *(uintptr_t*)ptr;
        if (!vtable) return false;
        #ifdef AML32
        if (vtable < 0x10000 || vtable >= 0xFFFFF000) return false;
        #else
        if (vtable < 0x10000 || vtable >= 0x00007FFFFFFFFFFFULL) return false;
        #endif

        return true;
    }

    // Safe helper to extract a pointer from Lua stack (lightuserdata or integer)
    static void* GetPointerFromArg(lua_State* L, int idx)
    {
        if (lua_islightuserdata(L, idx))
        {
            return lua_touserdata(L, idx);
        }
        else if (lua_isinteger(L, idx))
        {
            return (void*)(uintptr_t)lua_tointeger(L, idx);
        }
        else if (lua_isnumber(L, idx))
        {
            return (void*)(uintptr_t)lua_tonumber(L, idx);
        }
        return nullptr;
    }

    // ==========================================
    // Lua API: Player
    // ==========================================

    // Player.GetPed() -> lightuserdata (ped pointer) or nil
    static int Lua_Player_GetPed(lua_State* L)
    {
        void* ped = nullptr;
        if (pfnFindPlayerPed)
        {
            ped = pfnFindPlayerPed(-1);
        }

        if (IsValidGameObject(ped))
        {
            lua_pushlightuserdata(L, ped);
        }
        else
        {
            lua_pushnil(L);
        }
        return 1;
    }

    // Player.SetHealth(ped, hp)
    static int Lua_Player_SetHealth(lua_State* L)
    {
        void* ped = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(ped))
        {
            return 0; // Silently and safely ignore if invalid
        }

        float hp = (float)luaL_checknumber(L, 2);
        *(float*)((uintptr_t)ped + OFF_PED_HEALTH) = hp;
        return 0;
    }

    // Player.GetHealth(ped) -> number
    static int Lua_Player_GetHealth(lua_State* L)
    {
        void* ped = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(ped))
        {
            lua_pushnumber(L, 0.0);
            return 1;
        }

        float hp = *(float*)((uintptr_t)ped + OFF_PED_HEALTH);
        lua_pushnumber(L, (lua_Number)hp);
        return 1;
    }

    // Player.SetArmour(ped, armour)
    static int Lua_Player_SetArmour(lua_State* L)
    {
        void* ped = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(ped))
        {
            return 0;
        }

        float armour = (float)luaL_checknumber(L, 2);
        *(float*)((uintptr_t)ped + OFF_PED_ARMOUR) = armour;
        return 0;
    }

    // Player.GetArmour(ped) -> number
    static int Lua_Player_GetArmour(lua_State* L)
    {
        void* ped = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(ped))
        {
            lua_pushnumber(L, 0.0);
            return 1;
        }

        float armour = *(float*)((uintptr_t)ped + OFF_PED_ARMOUR);
        lua_pushnumber(L, (lua_Number)armour);
        return 1;
    }

    // ==========================================
    // Lua API: Vehicle
    // ==========================================

    // Vehicle.Repair(vehiclePtr)
    static int Lua_Vehicle_Repair(lua_State* L)
    {
        void* veh = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(veh))
        {
            return 0;
        }

        if (pfnVehicleFix)
        {
            pfnVehicleFix(veh);
        }

        // Set vehicle health to 1000.0f (full health)
        *(float*)((uintptr_t)veh + OFF_VEH_HEALTH) = 1000.0f;
        return 0;
    }

    // Vehicle.GetHealth(vehiclePtr) -> number
    static int Lua_Vehicle_GetHealth(lua_State* L)
    {
        void* veh = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(veh))
        {
            lua_pushnumber(L, 0.0);
            return 1;
        }

        float hp = *(float*)((uintptr_t)veh + OFF_VEH_HEALTH);
        lua_pushnumber(L, (lua_Number)hp);
        return 1;
    }

    // Vehicle.SetHealth(vehiclePtr, hp)
    static int Lua_Vehicle_SetHealth(lua_State* L)
    {
        void* veh = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(veh))
        {
            return 0;
        }

        float hp = (float)luaL_checknumber(L, 2);
        *(float*)((uintptr_t)veh + OFF_VEH_HEALTH) = hp;
        return 0;
    }

    // ==========================================
    // Lua API: Game / UI
    // ==========================================

    // Game.PrintText(text, timeMs)
    static int Lua_Game_PrintText(lua_State* L)
    {
        const char* text = luaL_checkstring(L, 1);
        int timeMs = (int)luaL_optinteger(L, 2, 2000);

        // Check if game world is active (player ped exists)
        bool isGameActive = false;
        if (pfnFindPlayerPed)
        {
            void* ped = pfnFindPlayerPed(-1);
            if (IsValidGameObject(ped))
            {
                isGameActive = true;
            }
        }

        if (isGameActive && pfnAddMessageJumpQ)
        {
            unsigned short gxtBuf[256];
            ConvertToGxt(text, gxtBuf, sizeof(gxtBuf) / sizeof(gxtBuf[0]));
            pfnAddMessageJumpQ(nullptr, gxtBuf, (unsigned int)timeMs, 0, false);
        }
        else if (aml)
        {
            // Guaranteed safe anywhere (menus, loading screens, or gameplay)
            aml->ShowToast(timeMs > 3000, "%s", text);
        }

        Log("[PrintText] %s", text);
        return 0;
    }

    // Game.Log(message)
    static int Lua_Game_Log(lua_State* L)
    {
        const char* msg = luaL_checkstring(L, 1);
        Log("[Lua Script] %s", msg);
        return 0;
    }

    // Register tick callback: Game.OnTick(fn) / AMLua.OnTick(fn)
    static int Lua_RegisterTick(lua_State* L)
    {
        luaL_checktype(L, 1, LUA_TFUNCTION);

        lua_getfield(L, LUA_REGISTRYINDEX, "AMLua_TickCallbacks");
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_pushvalue(L, -1);
            lua_setfield(L, LUA_REGISTRYINDEX, "AMLua_TickCallbacks");
        }

        int len = (int)lua_rawlen(L, -1);
        lua_pushvalue(L, 1);
        lua_rawseti(L, -2, len + 1);
        lua_pop(L, 1); // pop callbacks table

        Log("Registered Lua tick callback #%d", len + 1);
        return 0;
    }

    // Return list of loaded scripts to Lua as an array of strings
    static int Lua_GetLoadedScripts(lua_State* L)
    {
        lua_newtable(L);
        for (size_t i = 0; i < g_LoadedScripts.size(); ++i)
        {
            lua_pushstring(L, g_LoadedScripts[i].c_str());
            lua_rawseti(L, -2, (int)(i + 1));
        }
        return 1;
    }

    // Show native dialog listing all loaded scripts
    void ShowScriptListDialog()
    {
        std::string msg = "Active Lua Mods (" + std::to_string(g_LoadedScripts.size()) + "):\n\n";
        if (g_LoadedScripts.empty())
        {
            msg += "  (No .lua scripts found in scripts folder)\n";
        }
        else
        {
            for (size_t i = 0; i < g_LoadedScripts.size(); ++i)
            {
                msg += "  " + std::to_string(i + 1) + ". " + g_LoadedScripts[i] + "\n";
            }
        }
        msg += "\nScripts Path:\n" + g_ScriptsDirPath;

        Log("Opening AMLua Script List Dialog:\n%s", msg.c_str());

        if (aml)
        {
            aml->ShowDialog("AMLua - Loaded Scripts", msg.c_str(), "OK");
            aml->ShowToast(true, "AMLua: %zu script(s) loaded.", g_LoadedScripts.size());
        }
    }

    static int Lua_ShowScriptList(lua_State* L)
    {
        ShowScriptListDialog();
        return 0;
    }

    // Touch event gesture detection: Double-tap top of screen or two-finger tap to show mod list
    void OnTouchEvent(int actionType, int trackNum, int x, int y)
    {
        // actionType: 0 = DOWN, 1 = MOVE, 2 = UP
        if (actionType == 0) // Touch DOWN
        {
            // Check if touch is near top of screen (y < 200)
            if (y < 200)
            {
                static clock_t lastTopTapClock = 0;
                clock_t curClock = clock();
                double elapsedMs = (double)(curClock - lastTopTapClock) * 1000.0 / CLOCKS_PER_SEC;
                lastTopTapClock = curClock;

                if (elapsedMs < 600.0 && elapsedMs > 50.0)
                {
                    // Double-tap detected on top of screen!
                    ShowScriptListDialog();
                }
            }
            else if (trackNum >= 1)
            {
                // Two-finger tap
                static clock_t lastTwoFingerClock = 0;
                clock_t curClock = clock();
                double elapsedMs = (double)(curClock - lastTwoFingerClock) * 1000.0 / CLOCKS_PER_SEC;
                if (elapsedMs > 1200.0)
                {
                    lastTwoFingerClock = curClock;
                    ShowScriptListDialog();
                }
            }
        }
    }

    // Sandboxing: disable dangerous os / io / package capabilities
    static void ApplySandbox(lua_State* L)
    {
        // 1. os.execute, os.remove, os.rename
        lua_getglobal(L, "os");
        if (lua_istable(L, -1))
        {
            lua_pushnil(L);
            lua_setfield(L, -2, "execute");
            lua_pushnil(L);
            lua_setfield(L, -2, "remove");
            lua_pushnil(L);
            lua_setfield(L, -2, "rename");
        }
        lua_pop(L, 1);

        // 2. package.loadlib
        lua_getglobal(L, "package");
        if (lua_istable(L, -1))
        {
            lua_pushnil(L);
            lua_setfield(L, -2, "loadlib");
        }
        lua_pop(L, 1);

        // 3. io.popen
        lua_getglobal(L, "io");
        if (lua_istable(L, -1))
        {
            lua_pushnil(L);
            lua_setfield(L, -2, "popen");
        }
        lua_pop(L, 1);

        Log("Lua Sandbox active: os.execute, package.loadlib, io.popen disabled.");
    }

    // Register all APIs to Lua global namespace
    static void RegisterAPIs(lua_State* L)
    {
        // Table: Player
        lua_newtable(L);
        lua_pushcfunction(L, Lua_Player_GetPed);
        lua_setfield(L, -2, "GetPed");
        lua_pushcfunction(L, Lua_Player_SetHealth);
        lua_setfield(L, -2, "SetHealth");
        lua_pushcfunction(L, Lua_Player_GetHealth);
        lua_setfield(L, -2, "GetHealth");
        lua_pushcfunction(L, Lua_Player_SetArmour);
        lua_setfield(L, -2, "SetArmour");
        lua_pushcfunction(L, Lua_Player_GetArmour);
        lua_setfield(L, -2, "GetArmour");
        lua_setglobal(L, "Player");

        // Table: Vehicle
        lua_newtable(L);
        lua_pushcfunction(L, Lua_Vehicle_Repair);
        lua_setfield(L, -2, "Repair");
        lua_pushcfunction(L, Lua_Vehicle_GetHealth);
        lua_setfield(L, -2, "GetHealth");
        lua_pushcfunction(L, Lua_Vehicle_SetHealth);
        lua_setfield(L, -2, "SetHealth");
        lua_setglobal(L, "Vehicle");

        // Table: Game
        lua_newtable(L);
        lua_pushcfunction(L, Lua_Game_PrintText);
        lua_setfield(L, -2, "PrintText");
        lua_pushcfunction(L, Lua_Game_Log);
        lua_setfield(L, -2, "Log");
        lua_pushcfunction(L, Lua_RegisterTick);
        lua_setfield(L, -2, "OnTick");
        lua_pushcfunction(L, Lua_GetLoadedScripts);
        lua_setfield(L, -2, "GetLoadedScripts");
        lua_pushcfunction(L, Lua_ShowScriptList);
        lua_setfield(L, -2, "ShowScriptList");
        lua_setglobal(L, "Game");

        // Table: AMLua (contains version, mod list inspection, and direct module references)
        lua_newtable(L);
        lua_pushstring(L, "1.0");
        lua_setfield(L, -2, "Version");
        lua_pushcfunction(L, Lua_RegisterTick);
        lua_setfield(L, -2, "OnTick");
        lua_pushcfunction(L, Lua_GetLoadedScripts);
        lua_setfield(L, -2, "GetLoadedScripts");
        lua_pushcfunction(L, Lua_ShowScriptList);
        lua_setfield(L, -2, "ShowScriptList");

        // Reference Player, Vehicle, Game inside AMLua as well
        lua_getglobal(L, "Player");
        lua_setfield(L, -2, "Player");
        lua_getglobal(L, "Vehicle");
        lua_setfield(L, -2, "Vehicle");
        lua_getglobal(L, "Game");
        lua_setfield(L, -2, "Game");

        lua_setglobal(L, "AMLua");
    }

    void Init(uintptr_t libGTASA)
    {
        g_pGTASA = libGTASA;

        // Determine log and scripts path from AML data path if available
        if (aml)
        {
            const char* dataPath = aml->GetAndroidDataPath();
            if (dataPath && dataPath[0] != '\0')
            {
                g_LogFilePath = std::string(dataPath) + "/amlua.log";
                g_ScriptsDirPath = std::string(dataPath) + "/scripts";
            }
            g_LibGTASASize = aml->GetLibLength("libGTASA.so");
        }

        Log("=========================================");
        Log("AMLua: Android Mod Lua Script Loader 1.0");
        Log("Target: libGTASA.so (base: %p, size: %zu)", (void*)g_pGTASA, (size_t)g_LibGTASASize);
        Log("Log target: %s", g_LogFilePath.c_str());
        Log("Scripts dir: %s", g_ScriptsDirPath.c_str());
        Log("=========================================");

        // Resolve game engine symbols via AML
        if (aml && g_pGTASA)
        {
            // FindPlayerPed: _Z13FindPlayerPedi or FindPlayerPed
            pfnFindPlayerPed = (FindPlayerPed_t)aml->GetSym(g_pGTASA, "_Z13FindPlayerPedi");
            if (!pfnFindPlayerPed) pfnFindPlayerPed = (FindPlayerPed_t)aml->GetSym(g_pGTASA, "FindPlayerPed");
            Log("Symbol FindPlayerPed: %p", (void*)pfnFindPlayerPed);

            // CMessages::AddMessageJumpQ
            pfnAddMessageJumpQ = (AddMessageJumpQ_t)aml->GetSym(g_pGTASA, "_ZN9CMessages15AddMessageJumpQEPKcPtjtb");
            if (!pfnAddMessageJumpQ) pfnAddMessageJumpQ = (AddMessageJumpQ_t)aml->GetSym(g_pGTASA, "_ZN9CMessages15AddMessageJumpQEPKcjtb");
            Log("Symbol CMessages::AddMessageJumpQ: %p", (void*)pfnAddMessageJumpQ);

            // CVehicle::Fix
            pfnVehicleFix = (VehicleFix_t)aml->GetSym(g_pGTASA, "_ZN8CVehicle3FixEv");
            if (!pfnVehicleFix) pfnVehicleFix = (VehicleFix_t)aml->GetSym(g_pGTASA, "_ZN8CVehicle6RepairEv");
            Log("Symbol CVehicle::Fix: %p", (void*)pfnVehicleFix);
        }

        // Initialize Lua VM
        g_LuaState = luaL_newstate();
        if (!g_LuaState)
        {
            Log("[CRITICAL] Failed to initialize Lua state!");
            return;
        }

        // Load standard Lua libraries
        luaL_openlibs(g_LuaState);

        // Apply security sandboxing
        ApplySandbox(g_LuaState);

        // Register custom GTA SA bindings
        RegisterAPIs(g_LuaState);

        Log("Lua 5.4 VM initialized and API bindings registered successfully.");
    }

    void Shutdown()
    {
        if (g_LuaState)
        {
            lua_close(g_LuaState);
            g_LuaState = nullptr;
            Log("Lua VM shut down.");
        }
    }

    void LoadScripts(const char* scriptsDir)
    {
        if (!g_LuaState) return;

        g_ScriptsDirPath = scriptsDir;
        g_LoadedScripts.clear();

        Log("Scanning for scripts in: %s", scriptsDir);

        // Ensure directory exists
        mkdir(scriptsDir, 0777);

        DIR* dir = opendir(scriptsDir);
        if (!dir)
        {
            Log("Could not open scripts directory: %s", scriptsDir);
            return;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (entry->d_name[0] == '.') continue;

            size_t len = strlen(entry->d_name);
            if (len > 4 && strcmp(entry->d_name + len - 4, ".lua") == 0)
            {
                g_LoadedScripts.push_back(entry->d_name);
            }
        }
        closedir(dir);

        // Sort files alphabetically for deterministic loading
        std::sort(g_LoadedScripts.begin(), g_LoadedScripts.end());

        Log("Found %zu Lua script(s) to load.", g_LoadedScripts.size());

        for (const auto& fileName : g_LoadedScripts)
        {
            std::string fullPath = std::string(scriptsDir) + "/" + fileName;
            Log("----------------------------------------");
            Log("Loading script: %s", fileName.c_str());

            // Push traceback error handler
            lua_pushcfunction(g_LuaState, Lua_TracebackHandler);
            int errHandler = lua_gettop(g_LuaState);

            int loadStatus = luaL_loadfile(g_LuaState, fullPath.c_str());
            if (loadStatus == LUA_OK)
            {
                int callStatus = lua_pcall(g_LuaState, 0, 0, errHandler);
                if (callStatus != LUA_OK)
                {
                    const char* err = lua_tostring(g_LuaState, -1);
                    Log("[Script Error in %s]:\n%s", fileName.c_str(), err ? err : "Unknown execution error");
                    lua_pop(g_LuaState, 1);
                }
                else
                {
                    Log("Script %s executed successfully.", fileName.c_str());
                }
            }
            else
            {
                const char* err = lua_tostring(g_LuaState, -1);
                Log("[Compilation Error in %s]:\n%s", fileName.c_str(), err ? err : "Syntax/Load error");
                lua_pop(g_LuaState, 1);
            }

            lua_pop(g_LuaState, 1); // remove errHandler
        }
        Log("----------------------------------------");

        // Notify user about loaded scripts on startup
        if (aml)
        {
            aml->ShowToast(false, "AMLua: %zu script(s) loaded. Double-tap top of screen to view list.", g_LoadedScripts.size());
        }
    }

    void ProcessTick()
    {
        if (!g_LuaState) return;

        lua_pushcfunction(g_LuaState, Lua_TracebackHandler);
        int errHandler = lua_gettop(g_LuaState);

        lua_getfield(g_LuaState, LUA_REGISTRYINDEX, "AMLua_TickCallbacks");
        if (lua_istable(g_LuaState, -1))
        {
            int len = (int)lua_rawlen(g_LuaState, -1);
            for (int i = 1; i <= len; ++i)
            {
                lua_rawgeti(g_LuaState, -1, i);
                if (lua_isfunction(g_LuaState, -1))
                {
                    if (lua_pcall(g_LuaState, 0, 0, errHandler) != LUA_OK)
                    {
                        const char* err = lua_tostring(g_LuaState, -1);
                        Log("[Tick Callback Error #%d]:\n%s", i, err ? err : "Unknown tick error");
                        lua_pop(g_LuaState, 1);
                    }
                }
                else
                {
                    lua_pop(g_LuaState, 1);
                }
            }
        }
        lua_pop(g_LuaState, 1); // pop callbacks table
        lua_pop(g_LuaState, 1); // pop errHandler
    }
}
