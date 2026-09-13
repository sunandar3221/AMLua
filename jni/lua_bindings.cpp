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
    static uintptr_t g_pGTASA = 0;

    // Struct member offsets for CPed and CVehicle
    #ifdef AML32
        constexpr uintptr_t OFF_PED_HEALTH = 0x540;
        constexpr uintptr_t OFF_PED_MAX_HEALTH = 0x544;
        constexpr uintptr_t OFF_PED_ARMOUR = 0x548;
        constexpr uintptr_t OFF_VEH_HEALTH = 0x4C0;
    #else
        constexpr uintptr_t OFF_PED_HEALTH = 0x764;
        constexpr uintptr_t OFF_PED_MAX_HEALTH = 0x768;
        constexpr uintptr_t OFF_PED_ARMOUR = 0x76C;
        constexpr uintptr_t OFF_VEH_HEALTH = 0x5A0;
    #endif

    // Function pointer types for resolved game symbols
    typedef void* (*FindPlayerPed_t)(int playerNum);
    typedef void  (*AddMessageJumpQ_t)(const char* text, unsigned int time, unsigned short flag, bool bPreviousBrief);
    typedef void  (*SetHelpMessage_t)(const char* text, bool quickMessage, bool permanent, bool addToBrief, unsigned int time);
    typedef void  (*VehicleFix_t)(void* vehicle);

    static FindPlayerPed_t   pfnFindPlayerPed = nullptr;
    static AddMessageJumpQ_t pfnAddMessageJumpQ = nullptr;
    static SetHelpMessage_t  pfnSetHelpMessage = nullptr;
    static VehicleFix_t      pfnVehicleFix = nullptr;

    const char* GetLogFilePath()
    {
        return g_LogFilePath.c_str();
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

        if (ped)
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
        if (!ped)
        {
            return luaL_error(L, "Player.SetHealth: invalid ped pointer");
        }

        float hp = (float)luaL_checknumber(L, 2);
        *(float*)((uintptr_t)ped + OFF_PED_HEALTH) = hp;
        return 0;
    }

    // Player.GetHealth(ped) -> number
    static int Lua_Player_GetHealth(lua_State* L)
    {
        void* ped = GetPointerFromArg(L, 1);
        if (!ped)
        {
            return luaL_error(L, "Player.GetHealth: invalid ped pointer");
        }

        float hp = *(float*)((uintptr_t)ped + OFF_PED_HEALTH);
        lua_pushnumber(L, (lua_Number)hp);
        return 1;
    }

    // Player.SetArmour(ped, armour)
    static int Lua_Player_SetArmour(lua_State* L)
    {
        void* ped = GetPointerFromArg(L, 1);
        if (!ped)
        {
            return luaL_error(L, "Player.SetArmour: invalid ped pointer");
        }

        float armour = (float)luaL_checknumber(L, 2);
        *(float*)((uintptr_t)ped + OFF_PED_ARMOUR) = armour;
        return 0;
    }

    // Player.GetArmour(ped) -> number
    static int Lua_Player_GetArmour(lua_State* L)
    {
        void* ped = GetPointerFromArg(L, 1);
        if (!ped)
        {
            return luaL_error(L, "Player.GetArmour: invalid ped pointer");
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
        if (!veh)
        {
            return luaL_error(L, "Vehicle.Repair: invalid vehicle pointer");
        }

        if (pfnVehicleFix)
        {
            pfnVehicleFix(veh);
        }

        // Set vehicle health to 1000.0f
        *(float*)((uintptr_t)veh + OFF_VEH_HEALTH) = 1000.0f;
        return 0;
    }

    // Vehicle.GetHealth(vehiclePtr) -> number
    static int Lua_Vehicle_GetHealth(lua_State* L)
    {
        void* veh = GetPointerFromArg(L, 1);
        if (!veh)
        {
            return luaL_error(L, "Vehicle.GetHealth: invalid vehicle pointer");
        }

        float hp = *(float*)((uintptr_t)veh + OFF_VEH_HEALTH);
        lua_pushnumber(L, (lua_Number)hp);
        return 1;
    }

    // Vehicle.SetHealth(vehiclePtr, hp)
    static int Lua_Vehicle_SetHealth(lua_State* L)
    {
        void* veh = GetPointerFromArg(L, 1);
        if (!veh)
        {
            return luaL_error(L, "Vehicle.SetHealth: invalid vehicle pointer");
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

        if (pfnAddMessageJumpQ)
        {
            pfnAddMessageJumpQ(text, (unsigned int)timeMs, 0, false);
        }
        else if (pfnSetHelpMessage)
        {
            pfnSetHelpMessage(text, true, false, false, (unsigned int)timeMs);
        }

        if (aml)
        {
            aml->ShowToast(timeMs > 3000, "%s", text);
        }

        Log("[In-Game Text] %s", text);
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

        Log("Registered new Lua tick callback #%d", len + 1);
        return 0;
    }

    // Sandboxing: disable dangerous os / io / package capabilities
    static void ApplySandbox(lua_State* L)
    {
        // 1. os.execute
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

        Log("Lua Sandbox applied: os.execute, package.loadlib, io.popen disabled.");
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
        lua_setglobal(L, "Game");

        // Table: AMLua (contains version and direct module references)
        lua_newtable(L);
        lua_pushstring(L, "1.0");
        lua_setfield(L, -2, "Version");
        lua_pushcfunction(L, Lua_RegisterTick);
        lua_setfield(L, -2, "OnTick");

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

        // Determine log path from AML data path if available
        if (aml)
        {
            const char* dataPath = aml->GetAndroidDataPath();
            if (dataPath && dataPath[0] != '\0')
            {
                g_LogFilePath = std::string(dataPath) + "/amlua.log";
            }
        }

        Log("=========================================");
        Log("AMLua - Android Mod Lua Script Loader 1.0");
        Log("Target Library: libGTASA.so (base: %p)", (void*)libGTASA);
        Log("Log file target: %s", g_LogFilePath.c_str());
        Log("=========================================");

        // Resolve game engine symbols via AML
        if (aml && g_pGTASA)
        {
            // FindPlayerPed
            pfnFindPlayerPed = (FindPlayerPed_t)aml->GetSym(g_pGTASA, "_Z13FindPlayerPedi");
            if (!pfnFindPlayerPed) pfnFindPlayerPed = (FindPlayerPed_t)aml->GetSym(g_pGTASA, "FindPlayerPed");
            Log("Symbol FindPlayerPed: %p", (void*)pfnFindPlayerPed);

            // CMessages::AddMessageJumpQ
            pfnAddMessageJumpQ = (AddMessageJumpQ_t)aml->GetSym(g_pGTASA, "_ZN9CMessages15AddMessageJumpQEPKcjtb");
            if (!pfnAddMessageJumpQ) pfnAddMessageJumpQ = (AddMessageJumpQ_t)aml->GetSym(g_pGTASA, "_ZN9CMessages15AddMessageJumpQEPKcttb");
            if (!pfnAddMessageJumpQ) pfnAddMessageJumpQ = (AddMessageJumpQ_t)aml->GetSym(g_pGTASA, "_ZN9CMessages15AddMessageJumpQEPKcjjb");
            if (!pfnAddMessageJumpQ) pfnAddMessageJumpQ = (AddMessageJumpQ_t)aml->GetSym(g_pGTASA, "_ZN9CMessages15AddMessageJumpQEPKcjb");
            Log("Symbol CMessages::AddMessageJumpQ: %p", (void*)pfnAddMessageJumpQ);

            // CHud::SetHelpMessage (fallback text display)
            pfnSetHelpMessage = (SetHelpMessage_t)aml->GetSym(g_pGTASA, "_ZN4CHud14SetHelpMessageEPKctbbj");
            if (!pfnSetHelpMessage) pfnSetHelpMessage = (SetHelpMessage_t)aml->GetSym(g_pGTASA, "_ZN4CHud14SetHelpMessageEPKcbbbj");
            if (!pfnSetHelpMessage) pfnSetHelpMessage = (SetHelpMessage_t)aml->GetSym(g_pGTASA, "_ZN4CHud14SetHelpMessageEPKcb");
            Log("Symbol CHud::SetHelpMessage: %p", (void*)pfnSetHelpMessage);

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

        Log("Scanning for scripts in: %s", scriptsDir);

        // Ensure directory exists
        mkdir(scriptsDir, 0777);

        DIR* dir = opendir(scriptsDir);
        if (!dir)
        {
            Log("Could not open scripts directory: %s", scriptsDir);
            return;
        }

        std::vector<std::string> scriptFiles;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (entry->d_name[0] == '.') continue;

            size_t len = strlen(entry->d_name);
            if (len > 4 && strcmp(entry->d_name + len - 4, ".lua") == 0)
            {
                scriptFiles.push_back(entry->d_name);
            }
        }
        closedir(dir);

        // Sort files alphabetically for deterministic loading
        std::sort(scriptFiles.begin(), scriptFiles.end());

        Log("Found %zu Lua script(s) to load.", scriptFiles.size());

        for (const auto& fileName : scriptFiles)
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
