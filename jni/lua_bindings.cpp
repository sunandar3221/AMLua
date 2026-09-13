#include "lua_bindings.h"
#include "crash_handler.h"
#include <mod/aml.h>

#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
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
    typedef void  (*AsciiToGxtChar_t)(const char* src, unsigned short* dst);
    // CHud::SetHelpMessage overloads
    typedef void  (*SetHelpMessage6_t)(const char* helpKey, unsigned short* gxtText, bool quickMessage, bool permanent, bool addToBrief, unsigned int duration);
    typedef void  (*SetHelpMessage5_t)(const char* helpKey, unsigned short* gxtText, bool quickMessage, bool permanent, bool addToBrief);
    typedef void  (*SetHelpMessage4_t)(unsigned short* gxtText, bool quickMessage, bool permanent, bool addToBrief);
    // CMessages::AddMessageJumpQ fallback
    typedef void  (*AddMessageJumpQ_t)(const char* key, unsigned short* gxtText, unsigned int time, unsigned short flag, bool bPreviousBrief);
    typedef void  (*VehicleFix_t)(void* vehicle);

    static FindPlayerPed_t     pfnFindPlayerPed = nullptr;
    static AsciiToGxtChar_t    pfnAsciiToGxtChar = nullptr;
    static SetHelpMessage6_t   pfnSetHelpMessage6 = nullptr;
    static SetHelpMessage5_t   pfnSetHelpMessage5 = nullptr;
    static SetHelpMessage4_t   pfnSetHelpMessage4 = nullptr;
    static AddMessageJumpQ_t   pfnAddMessageJumpQ = nullptr;
    static VehicleFix_t        pfnVehicleFix = nullptr;

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
        if (i + 1 < maxChars)
        {
            dst[i + 1] = 0;
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
        const char* traceback = lua_tostring(L, -1);
        if (traceback)
        {
            CrashHandler::LogError("[Lua Runtime Error]: %s", traceback);
        }
        return 1;
    }

    // Verify memory readability via kernel syscall check to avoid SIGSEGV
    inline bool IsValidMemory(const void* ptr, size_t size)
    {
        if (!ptr) return false;
        uintptr_t addr = (uintptr_t)ptr;

        #ifdef AML32
        if (addr < 0x10000 || addr >= 0xFFFFF000) return false;
        #else
        if (addr < 0x10000 || addr >= 0x00007FFFFFFFFFFFULL) return false;
        #endif

        if ((addr % sizeof(void*)) != 0) return false;

        // write(-1, ptr, size) asks the kernel to validate the user buffer without segfaulting
        if (write(-1, ptr, size) < 0 && errno == EFAULT)
        {
            return false;
        }
        return true;
    }

    // Verify pointer memory safety to avoid dereferencing garbage or dying game entities
    inline bool IsValidGameObject(void* ptr)
    {
        if (!IsValidMemory(ptr, sizeof(void*))) return false;

        // Verify vtable pointer is readable and valid
        uintptr_t vtable = *(uintptr_t*)ptr;
        if (!IsValidMemory((void*)vtable, sizeof(void*))) return false;

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

        void* pHealth = (void*)((uintptr_t)ped + OFF_PED_HEALTH);
        if (!IsValidMemory(pHealth, sizeof(float)))
        {
            return 0;
        }

        float hp = (float)luaL_checknumber(L, 2);
        if (hp < 0.0f) hp = 0.0f;
        if (hp > 1000.0f) hp = 1000.0f;

        *(float*)pHealth = hp;
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

        void* pHealth = (void*)((uintptr_t)ped + OFF_PED_HEALTH);
        if (!IsValidMemory(pHealth, sizeof(float)))
        {
            lua_pushnumber(L, 0.0);
            return 1;
        }

        float hp = *(float*)pHealth;
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

        void* pArmour = (void*)((uintptr_t)ped + OFF_PED_ARMOUR);
        if (!IsValidMemory(pArmour, sizeof(float)))
        {
            return 0;
        }

        float armour = (float)luaL_checknumber(L, 2);
        if (armour < 0.0f) armour = 0.0f;
        if (armour > 1000.0f) armour = 1000.0f;

        *(float*)pArmour = armour;
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

        void* pArmour = (void*)((uintptr_t)ped + OFF_PED_ARMOUR);
        if (!IsValidMemory(pArmour, sizeof(float)))
        {
            lua_pushnumber(L, 0.0);
            return 1;
        }

        float armour = *(float*)pArmour;
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

        void* pVehHealth = (void*)((uintptr_t)veh + OFF_VEH_HEALTH);
        if (IsValidMemory(pVehHealth, sizeof(float)))
        {
            *(float*)pVehHealth = 1000.0f;
        }
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

        void* pVehHealth = (void*)((uintptr_t)veh + OFF_VEH_HEALTH);
        if (!IsValidMemory(pVehHealth, sizeof(float)))
        {
            lua_pushnumber(L, 0.0);
            return 1;
        }

        float hp = *(float*)pVehHealth;
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

        void* pVehHealth = (void*)((uintptr_t)veh + OFF_VEH_HEALTH);
        if (!IsValidMemory(pVehHealth, sizeof(float)))
        {
            return 0;
        }

        float hp = (float)luaL_checknumber(L, 2);
        if (hp < 0.0f) hp = 0.0f;
        if (hp > 2000.0f) hp = 2000.0f;

        *(float*)pVehHealth = hp;
        return 0;
    }

    // ==========================================
    // Lua API: Game / UI
    // ==========================================

    // Display text in GTA SA native top-right dialog box (CHud::SetHelpMessage)
    void DisplayHelpBox(const char* text, unsigned int duration)
    {
        if (!text || text[0] == '\0') return;

        // Static circular buffer to ensure pointers passed to the engine remain valid across frames
        static unsigned short s_GxtBufs[4][512] = {{0}};
        static int s_GxtBufIdx = 0;
        s_GxtBufIdx = (s_GxtBufIdx + 1) % 4;
        unsigned short* gxtBuf = s_GxtBufs[s_GxtBufIdx];
        memset(gxtBuf, 0, sizeof(s_GxtBufs[0]));

        if (pfnAsciiToGxtChar)
        {
            pfnAsciiToGxtChar(text, gxtBuf);
        }
        else
        {
            ConvertToGxt(text, gxtBuf, 512);
        }

        unsigned int timeMs = (duration > 0) ? duration : 3000;

        if (pfnSetHelpMessage6)
        {
            // Call CHud::SetHelpMessage with safe parameters:
            // arg 1: nullptr (raw custom text, skip GXT key lookup so custom string is displayed)
            // arg 2: 16-bit GXT message pointer
            // arg 3: bQuick = true (allows calling PrintText repeatedly so new messages appear immediately)
            // arg 4: bDisplayForever = false
            // arg 5: bAddToBrief = true
            // arg 6: duration in ms (e.g. 3000)
            pfnSetHelpMessage6(nullptr, gxtBuf, true, false, true, timeMs);
        }
        else if (pfnSetHelpMessage5)
        {
            pfnSetHelpMessage5(nullptr, gxtBuf, true, false, true);
        }
        else if (pfnSetHelpMessage4)
        {
            pfnSetHelpMessage4(gxtBuf, true, false, true);
        }
        else if (pfnAddMessageJumpQ)
        {
            // Fallback: bottom subtitle if CHud::SetHelpMessage is not available
            pfnAddMessageJumpQ(nullptr, gxtBuf, timeMs, 0, false);
        }
        else
        {
            Log("[DisplayHelpBox] No HUD help or message function available");
        }

        Log("[DisplayHelpBox] %s", text);
    }

    // Game.PrintText(text, timeMs) / Game.ShowHelpMessage(text, timeMs)
    static int Lua_Game_PrintText(lua_State* L)
    {
        const char* text = luaL_checkstring(L, 1);
        int timeMs = (int)luaL_optinteger(L, 2, 3000);

        DisplayHelpBox(text, (unsigned int)timeMs);
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

    // Show native GTA SA top-right dialog box listing all loaded scripts
    void ShowScriptListDialog()
    {
        std::string text = "~y~AMLua Mods (~w~" + std::to_string(g_LoadedScripts.size()) + "~y~):";
        if (g_LoadedScripts.empty())
        {
            text += "~n~~w~No .lua scripts found";
        }
        else
        {
            for (size_t i = 0; i < g_LoadedScripts.size() && i < 8; ++i)
            {
                text += "~n~~w~" + std::to_string(i + 1) + ". " + g_LoadedScripts[i];
            }
            if (g_LoadedScripts.size() > 8)
            {
                text += "~n~~g~(+ " + std::to_string(g_LoadedScripts.size() - 8) + " more)";
            }
        }

        Log("Showing Mod List in GTA SA Dialog:\n%s", text.c_str());

        // Display in GTA SA's built-in top-right help dialog box
        DisplayHelpBox(text.c_str(), 6000);
    }

    static int Lua_ShowScriptList(lua_State* L)
    {
        ShowScriptListDialog();
        return 0;
    }

    // Game.DoFile(path) / AMLua.DoFile(path)
    // Allows calling/executing Lua scripts repeatedly from inside Lua scripts
    static int Lua_Game_DoFile(lua_State* L)
    {
        const char* path = luaL_checkstring(L, 1);
        std::string fullPath;
        if (path[0] == '/')
        {
            fullPath = path;
        }
        else
        {
            fullPath = g_ScriptsDirPath + "/" + path;
        }

        lua_pushcfunction(L, Lua_TracebackHandler);
        int errHandler = lua_gettop(L);

        int status = luaL_loadfile(L, fullPath.c_str());
        if (status != LUA_OK)
        {
            const char* err = lua_tostring(L, -1);
            Log("[DoFile Error in %s]: %s", path, err ? err : "File not found or syntax error");
            CrashHandler::LogError("[DoFile Load Error in %s]: %s", path, err ? err : "File not found or syntax error");
            lua_pop(L, 2); // pop error and errHandler
            lua_pushboolean(L, 0);
            return 1;
        }

        CrashHandler::SetCurrentScript(path);
        CrashHandler::SetCurrentAction("DoFile Execution");
        status = lua_pcall(L, 0, LUA_MULTRET, errHandler);
        CrashHandler::ClearContext();
        if (status != LUA_OK)
        {
            const char* err = lua_tostring(L, -1);
            Log("[DoFile Execution Error in %s]: %s", path, err ? err : "Runtime error");
            CrashHandler::LogError("[DoFile Execution Error in %s]: %s", path, err ? err : "Runtime error");
            lua_pop(L, 2); // pop error and errHandler
            lua_pushboolean(L, 0);
            return 1;
        }

        lua_remove(L, errHandler);
        return lua_gettop(L) - (errHandler - 1);
    }

    // Game.RunString(code) / AMLua.RunString(code)
    // Evaluates a Lua code string dynamically
    static int Lua_Game_RunString(lua_State* L)
    {
        const char* code = luaL_checkstring(L, 1);

        lua_pushcfunction(L, Lua_TracebackHandler);
        int errHandler = lua_gettop(L);

        int status = luaL_loadstring(L, code);
        if (status != LUA_OK)
        {
            const char* err = lua_tostring(L, -1);
            Log("[RunString Error]: %s", err ? err : "Syntax error");
            CrashHandler::LogError("[RunString Syntax Error]: %s", err ? err : "Syntax error");
            lua_pop(L, 2);
            lua_pushboolean(L, 0);
            return 1;
        }

        CrashHandler::SetCurrentScript("RunString");
        CrashHandler::SetCurrentAction("Dynamic Code Execution");
        status = lua_pcall(L, 0, LUA_MULTRET, errHandler);
        CrashHandler::ClearContext();
        if (status != LUA_OK)
        {
            const char* err = lua_tostring(L, -1);
            Log("[RunString Execution Error]: %s", err ? err : "Runtime error");
            CrashHandler::LogError("[RunString Execution Error]: %s", err ? err : "Runtime error");
            lua_pop(L, 2);
            lua_pushboolean(L, 0);
            return 1;
        }

        lua_remove(L, errHandler);
        return lua_gettop(L) - (errHandler - 1);
    }

    // Forward declaration of LoadScripts
    void LoadScripts(const char* scriptsDir);

    // Game.ReloadScripts() / AMLua.ReloadScripts()
    // Reloads all Lua scripts on the fly
    static int Lua_ReloadScripts(lua_State* L)
    {
        // Clear tick callbacks
        lua_pushnil(L);
        lua_setfield(L, LUA_REGISTRYINDEX, "AMLua_TickCallbacks");

        LoadScripts(g_ScriptsDirPath.c_str());

        DisplayHelpBox("~g~AMLua: Scripts Reloaded!", 3000);
        return 0;
    }

    // Touch event gesture detection:
    // 1. Double-tap in top-right area (near weapon/health HUD)
    // 2. Double-tap on top status bar
    // 3. Swipe down from top (classic CLEO swipe)
    // 4. Two-finger tap
    void OnTouchEvent(int actionType, int trackNum, int x, int y)
    {
        static int s_MaxX = 1280;
        static int s_MaxY = 720;
        if (x > s_MaxX) s_MaxX = x;
        if (y > s_MaxY) s_MaxY = y;

        static int startX = 0;
        static int startY = 0;
        static clock_t startTime = 0;
        static clock_t lastTapClock = 0;

        // actionType: 0 = DOWN, 1 = MOVE, 2 = UP
        if (actionType == 0) // DOWN
        {
            startX = x;
            startY = y;
            startTime = clock();

            clock_t curClock = clock();
            double elapsedMs = (double)(curClock - lastTapClock) * 1000.0 / CLOCKS_PER_SEC;
            lastTapClock = curClock;

            // Check if touch is near top-right corner (where weapon/health HUD is)
            bool isTopRight = (x > (int)(s_MaxX * 0.55f) && y < (int)(s_MaxY * 0.40f));
            // Or top status bar
            bool isTopBar = (y < (int)(s_MaxY * 0.25f));

            if ((isTopRight || isTopBar) && elapsedMs < 650.0 && elapsedMs > 40.0)
            {
                // Double tap detected!
                ShowScriptListDialog();
                return;
            }

            // Two-finger tap
            if (trackNum >= 1)
            {
                static clock_t lastTwoFingerClock = 0;
                double tfElapsed = (double)(curClock - lastTwoFingerClock) * 1000.0 / CLOCKS_PER_SEC;
                if (tfElapsed > 1000.0)
                {
                    lastTwoFingerClock = curClock;
                    ShowScriptListDialog();
                    return;
                }
            }
        }
        else if (actionType == 2) // UP
        {
            // Swipe down from top (start y < 30% height, swipe down > 18% height, vertical)
            clock_t curClock = clock();
            double swipeDuration = (double)(curClock - startTime) * 1000.0 / CLOCKS_PER_SEC;
            int dx = abs(x - startX);
            int dy = y - startY;
            if (startY < (int)(s_MaxY * 0.30f) && dy > (int)(s_MaxY * 0.18f) && dx < (int)(s_MaxX * 0.25f) && swipeDuration < 800.0)
            {
                ShowScriptListDialog();
                return;
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
        lua_pushcfunction(L, Lua_Game_PrintText);
        lua_setfield(L, -2, "ShowHelpMessage");
        lua_pushcfunction(L, Lua_Game_Log);
        lua_setfield(L, -2, "Log");
        lua_pushcfunction(L, Lua_RegisterTick);
        lua_setfield(L, -2, "OnTick");
        lua_pushcfunction(L, Lua_GetLoadedScripts);
        lua_setfield(L, -2, "GetLoadedScripts");
        lua_pushcfunction(L, Lua_ShowScriptList);
        lua_setfield(L, -2, "ShowScriptList");
        lua_pushcfunction(L, Lua_Game_DoFile);
        lua_setfield(L, -2, "DoFile");
        lua_pushcfunction(L, Lua_Game_RunString);
        lua_setfield(L, -2, "RunString");
        lua_pushcfunction(L, Lua_ReloadScripts);
        lua_setfield(L, -2, "ReloadScripts");
        lua_setglobal(L, "Game");

        // Table: AMLua (contains version, mod list inspection, and direct module references)
        lua_newtable(L);
        lua_pushstring(L, "1.0.3");
        lua_setfield(L, -2, "Version");
        lua_pushcfunction(L, Lua_RegisterTick);
        lua_setfield(L, -2, "OnTick");
        lua_pushcfunction(L, Lua_GetLoadedScripts);
        lua_setfield(L, -2, "GetLoadedScripts");
        lua_pushcfunction(L, Lua_ShowScriptList);
        lua_setfield(L, -2, "ShowScriptList");
        lua_pushcfunction(L, Lua_Game_PrintText);
        lua_setfield(L, -2, "PrintText");
        lua_pushcfunction(L, Lua_Game_PrintText);
        lua_setfield(L, -2, "ShowHelpMessage");
        lua_pushcfunction(L, Lua_Game_DoFile);
        lua_setfield(L, -2, "DoFile");
        lua_pushcfunction(L, Lua_Game_RunString);
        lua_setfield(L, -2, "RunString");
        lua_pushcfunction(L, Lua_ReloadScripts);
        lua_setfield(L, -2, "ReloadScripts");

        // Reference Player, Vehicle, Game inside AMLua as well
        lua_getglobal(L, "Player");
        lua_setfield(L, -2, "Player");
        lua_getglobal(L, "Vehicle");
        lua_setfield(L, -2, "Vehicle");
        lua_getglobal(L, "Game");
        lua_setfield(L, -2, "Game");

        lua_setglobal(L, "AMLua");

        // Global dofile override so scripts can call dofile("myscript.lua") directly
        lua_pushcfunction(L, Lua_Game_DoFile);
        lua_setglobal(L, "dofile");
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

            // AsciiToGxtChar: _Z14AsciiToGxtCharPKcPt
            pfnAsciiToGxtChar = (AsciiToGxtChar_t)aml->GetSym(g_pGTASA, "_Z14AsciiToGxtCharPKcPt");
            if (!pfnAsciiToGxtChar) pfnAsciiToGxtChar = (AsciiToGxtChar_t)aml->GetSym(g_pGTASA, "AsciiToGxtChar");
            Log("Symbol AsciiToGxtChar: %p", (void*)pfnAsciiToGxtChar);

            // CHud::SetHelpMessage (GTA SA native top-right dialog box)
            pfnSetHelpMessage6 = (SetHelpMessage6_t)aml->GetSym(g_pGTASA, "_ZN4CHud14SetHelpMessageEPKcPtbbbj");
            if (!pfnSetHelpMessage6)
            {
                pfnSetHelpMessage5 = (SetHelpMessage5_t)aml->GetSym(g_pGTASA, "_ZN4CHud14SetHelpMessageEPKcPtbbb");
                if (!pfnSetHelpMessage5)
                {
                    pfnSetHelpMessage4 = (SetHelpMessage4_t)aml->GetSym(g_pGTASA, "_ZN4CHud14SetHelpMessageEPtbbb");
                }
            }
            Log("Symbol CHud::SetHelpMessage: %p", (void*)(pfnSetHelpMessage6 ? (void*)pfnSetHelpMessage6 : (pfnSetHelpMessage5 ? (void*)pfnSetHelpMessage5 : (void*)pfnSetHelpMessage4)));

            // CMessages::AddMessageJumpQ (fallback)
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

        // Configure package.path so require() looks in scripts directory
        std::string luaPath = g_ScriptsDirPath + "/?.lua;" + g_ScriptsDirPath + "/?/init.lua;./?.lua";
        lua_getglobal(g_LuaState, "package");
        if (lua_istable(g_LuaState, -1))
        {
            lua_pushstring(g_LuaState, luaPath.c_str());
            lua_setfield(g_LuaState, -2, "path");
        }
        lua_pop(g_LuaState, 1);

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

        // Update package.path so require() always uses current scriptsDir
        std::string luaPath = std::string(scriptsDir) + "/?.lua;" + std::string(scriptsDir) + "/?/init.lua;./?.lua";
        lua_getglobal(g_LuaState, "package");
        if (lua_istable(g_LuaState, -1))
        {
            lua_pushstring(g_LuaState, luaPath.c_str());
            lua_setfield(g_LuaState, -2, "path");
        }
        lua_pop(g_LuaState, 1);

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

            CrashHandler::SetCurrentScript(fileName.c_str());
            CrashHandler::SetCurrentAction("Script Execution (Startup)");

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
                    CrashHandler::LogError("[Script Error in %s]:\n%s", fileName.c_str(), err ? err : "Unknown execution error");
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
                CrashHandler::LogError("[Compilation Error in %s]:\n%s", fileName.c_str(), err ? err : "Syntax/Load error");
                lua_pop(g_LuaState, 1);
            }

            lua_pop(g_LuaState, 1); // remove errHandler
            CrashHandler::ClearContext();
        }
        Log("----------------------------------------");

        Log("AMLua: %zu script(s) loaded successfully.", g_LoadedScripts.size());
    }

    static int  s_ActivePlayerFrames = 0;
    static bool s_InitialGreetingShown = false;

    void ProcessTick()
    {
        if (!g_LuaState) return;

        // Display initial greeting dialog once player is active and world is stabilized (~90 frames / 3 seconds)
        if (!s_InitialGreetingShown && pfnFindPlayerPed)
        {
            void* ped = pfnFindPlayerPed(-1);
            if (IsValidGameObject(ped))
            {
                s_ActivePlayerFrames++;
                if (s_ActivePlayerFrames >= 30)
                {
                    s_InitialGreetingShown = true;
                    std::string msg = "~y~AMLua Active!~n~~w~" + std::to_string(g_LoadedScripts.size()) + " script(s) loaded.~n~~g~Double-tap top-right for list.";
                    DisplayHelpBox(msg.c_str(), 4500);
                }
            }
            else
            {
                s_ActivePlayerFrames = 0;
            }
        }

        CrashHandler::SetCurrentAction("Processing Tick Callbacks");

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
                    char actBuf[64];
                    snprintf(actBuf, sizeof(actBuf), "Tick Callback #%d", i);
                    CrashHandler::SetCurrentAction(actBuf);

                    if (lua_pcall(g_LuaState, 0, 0, errHandler) != LUA_OK)
                    {
                        const char* err = lua_tostring(g_LuaState, -1);
                        Log("[Tick Callback Error #%d]:\n%s", i, err ? err : "Unknown tick error");
                        CrashHandler::LogError("[Tick Callback Error #%d]:\n%s", i, err ? err : "Unknown tick error");
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

        CrashHandler::ClearContext();
    }
}
