#include "lua_bindings.h"
#include "crash_handler.h"
#include "http_client.h"
#include "json_helper.h"
#include <mod/aml.h>

#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <cmath>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>

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
        constexpr uintptr_t OFF_PED_HEALTH     = 0x544;
        constexpr uintptr_t OFF_PED_MAX_HEALTH = 0x548;
        constexpr uintptr_t OFF_PED_ARMOUR     = 0x54C;
        constexpr uintptr_t OFF_VEH_HEALTH     = 0x4CC;
        constexpr uintptr_t OFF_PLACEMENT      = 0x4;
        constexpr uintptr_t OFF_HEADING        = 0x10;
        constexpr uintptr_t OFF_MATRIX         = 0x14;
        constexpr uintptr_t OFF_MOVE_SPEED     = 0x54;
        constexpr uintptr_t OFF_TURN_SPEED     = 0x60;
        constexpr uintptr_t OFF_DOOR_LOCK      = 0x508;
        constexpr uintptr_t OFF_MONEY          = 0xB8;
        constexpr uintptr_t OFF_DISP_MONEY     = 0xBC;
    #else
        constexpr uintptr_t OFF_PED_HEALTH     = 0x6AC;
        constexpr uintptr_t OFF_PED_MAX_HEALTH = 0x6B0;
        constexpr uintptr_t OFF_PED_ARMOUR     = 0x6B4;
        constexpr uintptr_t OFF_VEH_HEALTH     = 0x634;
        constexpr uintptr_t OFF_PLACEMENT      = 0x8;
        constexpr uintptr_t OFF_HEADING        = 0x14;
        constexpr uintptr_t OFF_MATRIX         = 0x18;
        constexpr uintptr_t OFF_MOVE_SPEED     = 0x78;
        constexpr uintptr_t OFF_TURN_SPEED     = 0x84;
        constexpr uintptr_t OFF_DOOR_LOCK      = 0x68C;
        constexpr uintptr_t OFF_MONEY          = 0xF0;
        constexpr uintptr_t OFF_DISP_MONEY     = 0xF4;
    #endif

    struct CVector
    {
        float x, y, z;
    };

    // Function pointer types for resolved game symbols
    typedef void* (*FindPlayerPed_t)(int playerNum);
    typedef void* (*FindPlayerVehicle_t)(int playerNum, bool bIncludeRemote);
    typedef void* (*FindPlayerWanted_t)(int playerNum);
    typedef void  (*AsciiToGxtChar_t)(const char* src, unsigned short* dst);
    // CHud::SetHelpMessage overloads
    typedef void  (*SetHelpMessage6_t)(const char* helpKey, unsigned short* gxtText, bool quickMessage, bool permanent, bool addToBrief, unsigned int duration);
    typedef void  (*SetHelpMessage5_t)(const char* helpKey, unsigned short* gxtText, bool quickMessage, bool permanent, bool addToBrief);
    typedef void  (*SetHelpMessage4_t)(unsigned short* gxtText, bool quickMessage, bool permanent, bool addToBrief);
    // CMessages::AddMessageJumpQ fallback
    typedef void  (*AddMessageJumpQ_t)(const char* key, unsigned short* gxtText, unsigned int time, unsigned short flag, bool bPreviousBrief);
    typedef void  (*VehicleFix_t)(void* vehicle);
    typedef void  (*VehicleBlowUp_t)(void* vehicle, void* culprit, unsigned char flags);
    typedef void  (*EntityTeleport_t)(void* entity, CVector dest, bool resetRotation);
    // SCM Script Engine & Explosion Function Signatures
    typedef int8_t (*ProcessOneCommand_t)(void* script);
    typedef int8_t (*ProcessCommands_t)(void* script, int commandID);
    typedef void   (*ScriptInit_t)(void* script);

    // Direct CExplosion signatures:
    // 1. By-value CVector (float x, float y, float z) - matches ARM32 0x5A70D0 and Itanium 7CVector mangling
    typedef void (*AddExplosionByVal_t)(void* victim, void* creator, int type, float x, float y, float z, bool makeSound, bool moveable, float camShake);
    // 2. Soundless explosion - matches ARM32 0x5A7220
    typedef void (*AddSoundlessExplosion_t)(float x, float y, float z, int type);
    // 3. By-ref const CVector&
    typedef void (*AddExplosionByRef_t)(void* victim, void* creator, int type, const CVector& pos, unsigned int time, bool makeSound, float camShake, bool bInvisible);
    typedef void (*TriggerExplosion_t)(const CVector& pos, float radius, float visibleDistance, void* victim, void* creator, bool processVehicleBombTimer, float damage);
    typedef void* (*FindPlayerInfo_t)(int playerNum);
    typedef float (*FindGroundZForCoord_t)(float x, float y);
    typedef float (*FindGroundZFor3DCoord_t)(float x, float y, float z, bool* pBool, void** ppEnt);
    typedef void  (*SetGameClock_t)(unsigned char hour, unsigned char min, unsigned char day);
    typedef void  (*ForceWeatherNow_t)(short weatherType);
    typedef void  (*ForceWeather_t)(short weatherType);
    typedef void  (*ReleaseWeather_t)();
    typedef void  (*SetWantedLevel_t)(void* wanted, int level);
    typedef void  (*SetWantedLevelNoDrop_t)(void* wanted, int level);
    typedef int   (*GetWantedLevel_t)(void* wanted);
    typedef int   (*GetPedRef_t)(void* ped);
    typedef void* (*GetPed_t)(int ref);
    typedef int   (*GetVehicleRef_t)(void* veh);
    typedef void* (*GetVehicle_t)(int ref);
    typedef void  (*GiveWeapon_t)(void* ped, int weaponType, unsigned int ammo, bool likeUnused);
    typedef void  (*SetCurrentWeapon_t)(void* ped, int weaponType);
    typedef void  (*ClearWeapons_t)(void* ped);
    typedef void  (*RemoveWeapon_t)(void* ped, int weaponType);

    static FindPlayerPed_t         pfnFindPlayerPed = nullptr;
    static FindPlayerVehicle_t     pfnFindPlayerVehicle = nullptr;
    static FindPlayerWanted_t      pfnFindPlayerWanted = nullptr;
    static FindPlayerInfo_t        pfnFindPlayerInfo = nullptr;
    static AsciiToGxtChar_t        pfnAsciiToGxtChar = nullptr;
    static SetHelpMessage6_t       pfnSetHelpMessage6 = nullptr;
    static SetHelpMessage5_t       pfnSetHelpMessage5 = nullptr;
    static SetHelpMessage4_t       pfnSetHelpMessage4 = nullptr;
    static AddMessageJumpQ_t       pfnAddMessageJumpQ = nullptr;
    static VehicleFix_t            pfnVehicleFix = nullptr;
    static VehicleBlowUp_t         pfnVehicleBlowUp = nullptr;
    static EntityTeleport_t        pfnEntityTeleport = nullptr;
    static ProcessOneCommand_t     pfnProcessOneCommand = nullptr;
    static ProcessCommands_t       pfnProcessCommands200To299 = nullptr;
    static ProcessCommands_t       pfnProcessCommands500To599 = nullptr;
    static ProcessCommands_t       pfnProcessCommands900To999 = nullptr;
    static ScriptInit_t            pfnScriptInit = nullptr;
    static AddExplosionByVal_t     pfnAddExplosionByVal = nullptr;
    static AddSoundlessExplosion_t pfnAddSoundlessExp = nullptr;
    static AddExplosionByRef_t     pfnAddExplosionByRef = nullptr;
    static TriggerExplosion_t      pfnTriggerExplosion = nullptr;
    static FindGroundZForCoord_t   pfnFindGroundZForCoord = nullptr;
    static FindGroundZFor3DCoord_t pfnFindGroundZFor3DCoord = nullptr;
    static SetGameClock_t          pfnSetGameClock = nullptr;
    static ForceWeatherNow_t       pfnForceWeatherNow = nullptr;
    static ForceWeather_t          pfnForceWeather = nullptr;
    static ReleaseWeather_t        pfnReleaseWeather = nullptr;
    static SetWantedLevel_t        pfnSetWantedLevel = nullptr;
    static SetWantedLevelNoDrop_t  pfnSetWantedLevelNoDrop = nullptr;
    static GetWantedLevel_t        pfnGetWantedLevel = nullptr;
    static GetPedRef_t             pfnGetPedRef = nullptr;
    static GetPed_t                pfnGetPed = nullptr;
    static GetVehicleRef_t         pfnGetVehicleRef = nullptr;
    static GetVehicle_t            pfnGetVehicle = nullptr;
    static GiveWeapon_t            pfnGiveWeapon = nullptr;
    static SetCurrentWeapon_t      pfnSetCurrentWeapon = nullptr;
    static ClearWeapons_t          pfnClearWeapons = nullptr;
    static RemoveWeapon_t          pfnRemoveWeapon = nullptr;

    // Direct game memory pointers
    static float*                  pTimeScale = nullptr;
    static float*                  pGameFPS = nullptr;
    static unsigned char*          pClockHours = nullptr;
    static unsigned char*          pClockMinutes = nullptr;
    static void*                   pWorldPlayers = nullptr;
    static int                     s_CachedWantedLevel = 0;

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
        if (!ptr || size == 0) return false;
        uintptr_t addr = (uintptr_t)ptr;

        #ifdef AML32
        if (addr < 0x10000 || addr >= 0xFFFFF000) return false;
        if (addr + size < addr) return false;
        #else
        if (addr < 0x10000 || addr >= 0x00007FFFFFFFFFFFULL) return false;
        if (addr + size < addr) return false;
        #endif

        if ((addr & 3) != 0) return false;

        uintptr_t pageStart = addr & ~(4096 - 1);
        uintptr_t pageEnd = (addr + size - 1) & ~(4096 - 1);
        size_t numPages = ((pageEnd - pageStart) / 4096) + 1;
        if (numPages <= 8)
        {
            unsigned char vec[8];
            if (mincore((void*)pageStart, numPages * 4096, vec) != 0)
            {
                if (errno == ENOMEM) return false;
            }
        }
        return true;
    }

    // Verify pointer memory safety to avoid dereferencing garbage or dying game entities
    inline bool IsValidGameObject(void* ptr)
    {
        if (!IsValidMemory(ptr, sizeof(void*))) return false;
        if (((uintptr_t)ptr % sizeof(void*)) != 0) return false;

        // Verify vtable pointer is readable and valid
        uintptr_t vtable = *(uintptr_t*)ptr;
        if (!IsValidMemory((void*)vtable, sizeof(void*))) return false;

        if (g_pGTASA && g_LibGTASASize)
        {
            if (vtable >= g_pGTASA && vtable < (g_pGTASA + g_LibGTASASize))
            {
                return true;
            }
        }
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

    // Safe retrieval of local player vehicle pointer without out-of-bounds crash
    static void* GetLocalPlayerVehicle()
    {
        if (!pfnFindPlayerPed) return nullptr;
        void* ped = pfnFindPlayerPed(-1);
        if (!IsValidGameObject(ped)) return nullptr;

        #ifdef AML32
        constexpr uintptr_t OFF_PED_VEH = 0x590;
        #else
        constexpr uintptr_t OFF_PED_VEH = 0x708;
        #endif

        void** ppVeh = (void**)((uintptr_t)ped + OFF_PED_VEH);
        if (IsValidMemory(ppVeh, sizeof(void*)) && IsValidGameObject(*ppVeh))
        {
            return *ppVeh;
        }

        if (pfnFindPlayerVehicle)
        {
            void* veh = pfnFindPlayerVehicle(0, false);
            if (IsValidGameObject(veh)) return veh;
        }
        return nullptr;
    }

    // Safe retrieval of CPlayerInfo pointer matching current spawned player
    static void* GetPlayerInfo()
    {
        if (!pfnFindPlayerPed) return nullptr;
        void* ped = pfnFindPlayerPed(-1);
        if (!IsValidGameObject(ped)) return nullptr;

        void* playerInfo = nullptr;
        if (pfnFindPlayerInfo)
        {
            playerInfo = pfnFindPlayerInfo(0);
        }
        if (!playerInfo && pWorldPlayers)
        {
            playerInfo = pWorldPlayers;
        }

        if (!playerInfo || !IsValidMemory(playerInfo, 0x100)) return nullptr;

        // Check direct struct match: playerInfo->m_pPed == ped
        void* pedInInfo = *(void**)playerInfo;
        if (pedInInfo == ped)
        {
            return playerInfo;
        }

        // Check if playerInfo is a pointer-to-pointer (GOT entry)
        if (IsValidMemory(pedInInfo, 0x100))
        {
            void* derefInfo = *(void**)pedInInfo;
            if (derefInfo == ped)
            {
                return (void*)pedInInfo;
            }
        }

        return nullptr;
    }

    // Safely retrieve entity position from matrix or placement
    static bool GetEntityPosition(void* entity, float& x, float& y, float& z)
    {
        if (!IsValidGameObject(entity)) return false;

        uintptr_t pMatrix = *(uintptr_t*)((uintptr_t)entity + OFF_MATRIX);
        if (IsValidMemory((void*)pMatrix, 0x40))
        {
            float* posMat = (float*)(pMatrix + 0x30);
            if (IsValidMemory(posMat, sizeof(float) * 3))
            {
                x = posMat[0];
                y = posMat[1];
                z = posMat[2];
                return true;
            }
        }

        float* posPlace = (float*)((uintptr_t)entity + OFF_PLACEMENT);
        if (IsValidMemory(posPlace, sizeof(float) * 3))
        {
            x = posPlace[0];
            y = posPlace[1];
            z = posPlace[2];
            return true;
        }

        return false;
    }

    // Safely update entity position and reset velocity for clean teleportation
    static bool SetEntityPosition(void* entity, float x, float y, float z)
    {
        if (!IsValidGameObject(entity)) return false;

        // Update placement position
        float* posPlace = (float*)((uintptr_t)entity + OFF_PLACEMENT);
        if (IsValidMemory(posPlace, sizeof(float) * 3))
        {
            posPlace[0] = x;
            posPlace[1] = y;
            posPlace[2] = z;
        }

        // Update matrix position if matrix is present
        uintptr_t pMatrix = *(uintptr_t*)((uintptr_t)entity + OFF_MATRIX);
        if (IsValidMemory((void*)pMatrix, 0x40))
        {
            float* posMat = (float*)(pMatrix + 0x30);
            if (IsValidMemory(posMat, sizeof(float) * 3))
            {
                posMat[0] = x;
                posMat[1] = y;
                posMat[2] = z;
            }
        }

        // Reset velocity so entity doesn't fling wildly with past momentum
        float* pMove = (float*)((uintptr_t)entity + OFF_MOVE_SPEED);
        if (IsValidMemory(pMove, sizeof(float) * 3))
        {
            pMove[0] = 0.0f;
            pMove[1] = 0.0f;
            pMove[2] = 0.0f;
        }

        float* pTurn = (float*)((uintptr_t)entity + OFF_TURN_SPEED);
        if (IsValidMemory(pTurn, sizeof(float) * 3))
        {
            pTurn[0] = 0.0f;
            pTurn[1] = 0.0f;
            pTurn[2] = 0.0f;
        }

        return true;
    }

    // Script runner structure matching GTA SA Android CRunningScript layout (aml-psdk)
    union ScriptParam
    {
        unsigned int uParam;
        int iParam;
        float fParam;
        void* pParam;
    };

    struct alignas(16) ScriptRunner
    {
        void*           m_pNext;
        void*           m_pPrev;
        char            m_szName[8];
        unsigned char*  m_pBaseIP;
        unsigned char*  m_pCurrentIP;
        unsigned char*  m_apStack[8];
        unsigned short  m_nSP;
        #ifndef AML32
        unsigned short  m_nPad64_1;
        unsigned int    m_nPad64_2;
        #endif
        ScriptParam     m_aLocalVars[40];
        int             m_anTimers[2];
        bool            m_bIsActive;
        bool            m_bCondResult;
        bool            m_bUseMissionCleanup;
        bool            m_bIsExternal;
        bool            m_bTextBlockOverride;
        char            m_nScriptBrainType;
        int             m_nWakeTime;
        unsigned short  m_nLogicalOp;
        bool            m_bNotFlag;
        bool            m_bWastedBustedCheck;
        bool            m_bWastedOrBusted;
        int             m_pSceneSkipIP;
        bool            m_bIsMission;
        char            m_safetyPadding[256];
    };

    static ScriptRunner s_Script;
    static bool s_ScriptInited = false;

    static void EnsureScriptInited()
    {
        if (!s_ScriptInited)
        {
            memset(&s_Script, 0, sizeof(s_Script));
            strncpy(s_Script.m_szName, "amlua", sizeof(s_Script.m_szName) - 1);
            if (pfnScriptInit)
            {
                pfnScriptInit(&s_Script);
            }
            s_ScriptInited = true;
        }
    }

    static bool RunSCM(uint16_t opcode, const char* fmt = "", ...)
    {
        if (!pfnProcessOneCommand) return false;
        EnsureScriptInited();

        alignas(16) uint8_t buffer[128] = {0};
        memcpy(&buffer[0], &opcode, 2);
        size_t offset = 2;

        va_list args;
        va_start(args, fmt);

        for (const char* p = fmt; *p; ++p)
        {
            if (*p == 'i') // 32-bit int
            {
                int val = va_arg(args, int);
                buffer[offset++] = 0x01;
                memcpy(&buffer[offset], &val, 4);
                offset += 4;
            }
            else if (*p == 'f') // 32-bit float
            {
                float val = (float)va_arg(args, double);
                buffer[offset++] = 0x06;
                memcpy(&buffer[offset], &val, 4);
                offset += 4;
            }
            else if (*p == 'v') // local var index (e.g. 0 for 0@)
            {
                uint16_t varIdx = (uint16_t)va_arg(args, int);
                buffer[offset++] = 0x03;
                memcpy(&buffer[offset], &varIdx, 2);
                offset += 2;
            }
        }
        va_end(args);

        buffer[offset++] = 0x00;

        s_Script.m_bIsActive = true;
        s_Script.m_nScriptBrainType = -1;
        s_Script.m_bWastedBustedCheck = true;
        s_Script.m_nSP = 0;
        s_Script.m_pBaseIP = buffer;
        s_Script.m_pCurrentIP = buffer;

        pfnProcessOneCommand(&s_Script);
        return true;
    }

    // Helper to create explosion via game engine with full visual effects & physics
    static bool CreateExplosionInternal(float x, float y, float z, int type, float radius, bool makeSound, float camShake)
    {
        CVector pos = { x, y, z };
        bool visualTriggered = false;

        // Method 1: SCM Opcode Execution via CRunningScript
        if (pfnProcessOneCommand || pfnProcessCommands200To299)
        {
            EnsureScriptInited();

            alignas(16) uint8_t cmdBuffer[64] = {0};
            s_Script.m_bIsActive = true;
            s_Script.m_nScriptBrainType = -1;
            s_Script.m_bWastedBustedCheck = true;
            s_Script.m_nSP = 0;

            if (!makeSound && (pfnProcessCommands500To599 || pfnProcessOneCommand))
            {
                // Opcode 0565: add_explosion_no_sound x y z type
                uint16_t opcode = 0x0565;
                memcpy(&cmdBuffer[0], &opcode, 2);
                cmdBuffer[2] = 0x06; // SCRIPTPARAM_STATIC_FLOAT
                memcpy(&cmdBuffer[3], &x, 4);
                cmdBuffer[7] = 0x06;
                memcpy(&cmdBuffer[8], &y, 4);
                cmdBuffer[12] = 0x06;
                memcpy(&cmdBuffer[13], &z, 4);
                cmdBuffer[17] = 0x01; // SCRIPTPARAM_STATIC_INT_32BITS
                memcpy(&cmdBuffer[18], &type, 4);
                cmdBuffer[22] = 0x00; // SCRIPTPARAM_END_OF_ARGUMENTS

                if (pfnProcessOneCommand)
                {
                    s_Script.m_pBaseIP = cmdBuffer;
                    s_Script.m_pCurrentIP = cmdBuffer;
                    pfnProcessOneCommand(&s_Script);
                    visualTriggered = true;
                }
                else if (pfnProcessCommands500To599)
                {
                    s_Script.m_pBaseIP = cmdBuffer;
                    s_Script.m_pCurrentIP = cmdBuffer + 2;
                    pfnProcessCommands500To599(&s_Script, 0x0565);
                    visualTriggered = true;
                }
            }
            else if (camShake > 0.0f && camShake != 1.0f && (pfnProcessCommands900To999 || pfnProcessOneCommand))
            {
                // Opcode 0948: add_explosion_variable_shake x y z type shake
                uint16_t opcode = 0x0948;
                memcpy(&cmdBuffer[0], &opcode, 2);
                cmdBuffer[2] = 0x06;
                memcpy(&cmdBuffer[3], &x, 4);
                cmdBuffer[7] = 0x06;
                memcpy(&cmdBuffer[8], &y, 4);
                cmdBuffer[12] = 0x06;
                memcpy(&cmdBuffer[13], &z, 4);
                cmdBuffer[17] = 0x01;
                memcpy(&cmdBuffer[18], &type, 4);
                cmdBuffer[22] = 0x06;
                memcpy(&cmdBuffer[23], &camShake, 4);
                cmdBuffer[27] = 0x00;

                if (pfnProcessOneCommand)
                {
                    s_Script.m_pBaseIP = cmdBuffer;
                    s_Script.m_pCurrentIP = cmdBuffer;
                    pfnProcessOneCommand(&s_Script);
                    visualTriggered = true;
                }
                else if (pfnProcessCommands900To999)
                {
                    s_Script.m_pBaseIP = cmdBuffer;
                    s_Script.m_pCurrentIP = cmdBuffer + 2;
                    pfnProcessCommands900To999(&s_Script, 0x0948);
                    visualTriggered = true;
                }
            }
            else
            {
                // Opcode 020C: add_explosion x y z type
                uint16_t opcode = 0x020C;
                memcpy(&cmdBuffer[0], &opcode, 2);
                cmdBuffer[2] = 0x06;
                memcpy(&cmdBuffer[3], &x, 4);
                cmdBuffer[7] = 0x06;
                memcpy(&cmdBuffer[8], &y, 4);
                cmdBuffer[12] = 0x06;
                memcpy(&cmdBuffer[13], &z, 4);
                cmdBuffer[17] = 0x01;
                memcpy(&cmdBuffer[18], &type, 4);
                cmdBuffer[22] = 0x00;

                if (pfnProcessOneCommand)
                {
                    s_Script.m_pBaseIP = cmdBuffer;
                    s_Script.m_pCurrentIP = cmdBuffer;
                    pfnProcessOneCommand(&s_Script);
                    visualTriggered = true;
                }
                else if (pfnProcessCommands200To299)
                {
                    s_Script.m_pBaseIP = cmdBuffer;
                    s_Script.m_pCurrentIP = cmdBuffer + 2;
                    pfnProcessCommands200To299(&s_Script, 0x020C);
                    visualTriggered = true;
                }
            }
        }

        // Method 2: Direct CExplosion::AddExplosion (By-Value CVector / x, y, z floats)
        if (!visualTriggered && pfnAddExplosionByVal)
        {
            pfnAddExplosionByVal(nullptr, nullptr, type, x, y, z, makeSound, false, camShake > 0.0f ? camShake : 1.0f);
            visualTriggered = true;
        }

        // Method 3: Direct Soundless (if makeSound is false)
        if (!visualTriggered && !makeSound && pfnAddSoundlessExp)
        {
            pfnAddSoundlessExp(x, y, z, type);
            visualTriggered = true;
        }

        // Method 4: Direct CExplosion::AddExplosion (By-Ref const CVector&)
        if (!visualTriggered && pfnAddExplosionByRef)
        {
            pfnAddExplosionByRef(nullptr, nullptr, type, pos, 0, makeSound, camShake, false);
            visualTriggered = true;
        }

        // Method 5: CWorld::TriggerExplosion (Physics impulse, vehicle push, sector damage)
        // In GTA SA, TriggerExplosion is responsible for spatial physics knockback,
        // camera shake, and entity damage in sectors. Calling this along with the visual
        // explosion ensures that both the fireball/smoke AND the damage/impulse occur!
        if (pfnTriggerExplosion)
        {
            float visibleDistance = 350.0f;
            if (visibleDistance < radius * 35.0f) visibleDistance = radius * 35.0f;
            float damage = (radius > 0.0f) ? (radius * 10.0f) : 100.0f;
            bool processVehicleBombTimer = false;
            pfnTriggerExplosion(pos, radius, visibleDistance, nullptr, nullptr, processVehicleBombTimer, damage);
            return true;
        }

        return visualTriggered;
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

    // Player.SetHealth(ped, hp) or Player.SetHealth(hp)
    static int Lua_Player_SetHealth(lua_State* L)
    {
        int top = lua_gettop(L);
        void* ped = nullptr;
        float hp = 100.0f;

        if (top >= 2)
        {
            ped = GetPointerFromArg(L, 1);
            hp = (float)luaL_checknumber(L, 2);
        }
        else if (top == 1)
        {
            if (pfnFindPlayerPed) ped = pfnFindPlayerPed(-1);
            hp = (float)luaL_checknumber(L, 1);
        }

        if (!IsValidGameObject(ped)) return 0;

        void* pHealth = (void*)((uintptr_t)ped + OFF_PED_HEALTH);
        if (!IsValidMemory(pHealth, sizeof(float))) return 0;

        if (hp < 0.0f) hp = 0.0f;
        if (hp > 1000.0f) hp = 1000.0f;

        *(float*)pHealth = hp;
        return 0;
    }

    // Player.GetHealth([ped]) -> number
    static int Lua_Player_GetHealth(lua_State* L)
    {
        void* ped = nullptr;
        if (lua_gettop(L) >= 1) ped = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(ped) && pfnFindPlayerPed) ped = pfnFindPlayerPed(-1);

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

    // Player.SetArmour(ped, armour) or Player.SetArmour(armour)
    static int Lua_Player_SetArmour(lua_State* L)
    {
        int top = lua_gettop(L);
        void* ped = nullptr;
        float armour = 100.0f;

        if (top >= 2)
        {
            ped = GetPointerFromArg(L, 1);
            armour = (float)luaL_checknumber(L, 2);
        }
        else if (top == 1)
        {
            if (pfnFindPlayerPed) ped = pfnFindPlayerPed(-1);
            armour = (float)luaL_checknumber(L, 1);
        }

        if (!IsValidGameObject(ped)) return 0;

        void* pArmour = (void*)((uintptr_t)ped + OFF_PED_ARMOUR);
        if (!IsValidMemory(pArmour, sizeof(float))) return 0;

        if (armour < 0.0f) armour = 0.0f;
        if (armour > 1000.0f) armour = 1000.0f;

        *(float*)pArmour = armour;
        return 0;
    }

    // Player.GetArmour([ped]) -> number
    static int Lua_Player_GetArmour(lua_State* L)
    {
        void* ped = nullptr;
        if (lua_gettop(L) >= 1) ped = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(ped) && pfnFindPlayerPed) ped = pfnFindPlayerPed(-1);

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

    // Player.GetPosition([ped]) -> x, y, z
    static int Lua_Player_GetPosition(lua_State* L)
    {
        void* ped = nullptr;
        if (lua_gettop(L) >= 1) ped = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(ped) && pfnFindPlayerPed) ped = pfnFindPlayerPed(-1);

        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (GetEntityPosition(ped, x, y, z))
        {
            lua_pushnumber(L, (lua_Number)x);
            lua_pushnumber(L, (lua_Number)y);
            lua_pushnumber(L, (lua_Number)z);
            return 3;
        }

        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 3;
    }

    // Player.SetPosition(x, y, z) or Player.SetPosition(ped, x, y, z) or Player.Teleport(...)
    static int Lua_Player_SetPosition(lua_State* L)
    {
        int top = lua_gettop(L);
        void* ped = nullptr;
        float x = 0.0f, y = 0.0f, z = 0.0f;

        if (top >= 4)
        {
            ped = GetPointerFromArg(L, 1);
            x = (float)luaL_checknumber(L, 2);
            y = (float)luaL_checknumber(L, 3);
            z = (float)luaL_checknumber(L, 4);
        }
        else if (top >= 3)
        {
            if (pfnFindPlayerPed) ped = pfnFindPlayerPed(-1);
            x = (float)luaL_checknumber(L, 1);
            y = (float)luaL_checknumber(L, 2);
            z = (float)luaL_checknumber(L, 3);
        }
        else
        {
            return luaL_error(L, "Player.SetPosition expects (x, y, z) or (ped, x, y, z)");
        }

        // If player is in a vehicle and local player is teleporting, move the vehicle
        void* veh = GetLocalPlayerVehicle();
        if (veh)
        {
            SetEntityPosition(veh, x, y, z);
            lua_pushboolean(L, 1);
            return 1;
        }

        bool ok = SetEntityPosition(ped, x, y, z);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }

    // Player.GetVehicle([ped]) -> lightuserdata or nil
    static int Lua_Player_GetVehicle(lua_State* L)
    {
        void* veh = GetLocalPlayerVehicle();
        if (veh)
        {
            lua_pushlightuserdata(L, veh);
        }
        else
        {
            lua_pushnil(L);
        }
        return 1;
    }

    // Player.IsInVehicle([ped]) -> boolean
    static int Lua_Player_IsInVehicle(lua_State* L)
    {
        void* veh = GetLocalPlayerVehicle();
        lua_pushboolean(L, veh != nullptr ? 1 : 0);
        return 1;
    }

    // Player.GetHeading([ped]) -> degrees (0-360)
    static int Lua_Player_GetHeading(lua_State* L)
    {
        void* ped = nullptr;
        if (lua_gettop(L) >= 1) ped = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(ped) && pfnFindPlayerPed) ped = pfnFindPlayerPed(-1);
        if (!IsValidGameObject(ped))
        {
            lua_pushnumber(L, 0.0);
            return 1;
        }

        float* pHeading = (float*)((uintptr_t)ped + OFF_HEADING);
        float rad = (IsValidMemory(pHeading, sizeof(float))) ? *pHeading : 0.0f;
        float deg = rad * 57.2957795f;
        if (deg < 0.0f) deg += 360.0f;
        lua_pushnumber(L, (lua_Number)deg);
        return 1;
    }

    // Player.SetHeading([ped], deg)
    static int Lua_Player_SetHeading(lua_State* L)
    {
        int top = lua_gettop(L);
        void* ped = nullptr;
        float deg = 0.0f;

        if (top >= 2)
        {
            ped = GetPointerFromArg(L, 1);
            deg = (float)luaL_checknumber(L, 2);
        }
        else
        {
            if (pfnFindPlayerPed) ped = pfnFindPlayerPed(-1);
            deg = (float)luaL_checknumber(L, 1);
        }

        if (!IsValidGameObject(ped)) return 0;

        float rad = deg * 0.0174532925f;
        float* pHeading = (float*)((uintptr_t)ped + OFF_HEADING);
        if (IsValidMemory(pHeading, sizeof(float)))
        {
            *pHeading = rad;
        }
        return 0;
    }

    // Player.GetWantedLevel() -> integer
    static int Lua_Player_GetWantedLevel(lua_State* L)
    {
        void* ped = pfnFindPlayerPed ? pfnFindPlayerPed(-1) : nullptr;
        if (!IsValidGameObject(ped))
        {
            lua_pushinteger(L, s_CachedWantedLevel);
            return 1;
        }

        void* wanted = nullptr;
        if (pfnFindPlayerWanted) wanted = pfnFindPlayerWanted(0);
        if (wanted && IsValidMemory(wanted, sizeof(void*)))
        {
            if (pfnGetWantedLevel)
            {
                int lvl = pfnGetWantedLevel(wanted);
                lua_pushinteger(L, lvl);
                return 1;
            }

            int* pLvl = (int*)((uintptr_t)wanted + 0x24);
            if (IsValidMemory(pLvl, sizeof(int)) && *pLvl >= 0 && *pLvl <= 6)
            {
                lua_pushinteger(L, *pLvl);
                return 1;
            }
        }
        lua_pushinteger(L, s_CachedWantedLevel);
        return 1;
    }

    // Player.SetWantedLevel(level)
    static int Lua_Player_SetWantedLevel(lua_State* L)
    {
        int level = (int)luaL_checkinteger(L, 1);
        if (level < 0) level = 0;
        if (level > 6) level = 6;
        s_CachedWantedLevel = level;

        void* ped = pfnFindPlayerPed ? pfnFindPlayerPed(-1) : nullptr;
        if (!IsValidGameObject(ped)) return 0;

        void* wanted = nullptr;
        if (pfnFindPlayerWanted) wanted = pfnFindPlayerWanted(0);
        if (wanted && IsValidMemory(wanted, sizeof(void*)))
        {
            if (pfnSetWantedLevel)
            {
                pfnSetWantedLevel(wanted, level);
            }
            else if (pfnSetWantedLevelNoDrop)
            {
                pfnSetWantedLevelNoDrop(wanted, level);
            }

            int* pLvl = (int*)((uintptr_t)wanted + 0x24);
            if (IsValidMemory(pLvl, sizeof(int)))
            {
                *pLvl = level;
            }
        }
        return 0;
    }

    // Player.ClearWantedLevel()
    static int Lua_Player_ClearWantedLevel(lua_State* L)
    {
        s_CachedWantedLevel = 0;

        void* ped = pfnFindPlayerPed ? pfnFindPlayerPed(-1) : nullptr;
        if (!IsValidGameObject(ped)) return 0;

        void* wanted = nullptr;
        if (pfnFindPlayerWanted) wanted = pfnFindPlayerWanted(0);
        if (wanted && IsValidMemory(wanted, sizeof(void*)))
        {
            if (pfnSetWantedLevel) pfnSetWantedLevel(wanted, 0);
            else if (pfnSetWantedLevelNoDrop) pfnSetWantedLevelNoDrop(wanted, 0);

            int* pLvl = (int*)((uintptr_t)wanted + 0x24);
            if (IsValidMemory(pLvl, sizeof(int)))
            {
                *pLvl = 0;
            }
        }
        return 0;
    }

    // Player.GiveMoney(amount)
    static int Lua_Player_GiveMoney(lua_State* L)
    {
        int amount = (int)luaL_checkinteger(L, 1);
        void* pInfo = GetPlayerInfo();
        if (pInfo)
        {
            int* pMoney = (int*)((uintptr_t)pInfo + OFF_MONEY);
            int* pDispMoney = (int*)((uintptr_t)pInfo + OFF_DISP_MONEY);

            if (IsValidMemory(pMoney, sizeof(int)))
            {
                *pMoney += amount;
            }
            if (IsValidMemory(pDispMoney, sizeof(int)))
            {
                *pDispMoney += amount;
            }
        }
        return 0;
    }

    // Player.SetMoney(amount)
    static int Lua_Player_SetMoney(lua_State* L)
    {
        int amount = (int)luaL_checkinteger(L, 1);
        void* pInfo = GetPlayerInfo();
        if (pInfo)
        {
            int* pMoney = (int*)((uintptr_t)pInfo + OFF_MONEY);
            int* pDispMoney = (int*)((uintptr_t)pInfo + OFF_DISP_MONEY);

            if (IsValidMemory(pMoney, sizeof(int)))
            {
                *pMoney = amount;
            }
            if (IsValidMemory(pDispMoney, sizeof(int)))
            {
                *pDispMoney = amount;
            }
        }
        return 0;
    }

    // Player.GetMoney() -> integer
    static int Lua_Player_GetMoney(lua_State* L)
    {
        void* pInfo = GetPlayerInfo();
        if (pInfo)
        {
            int* pMoney = (int*)((uintptr_t)pInfo + OFF_MONEY);
            if (IsValidMemory(pMoney, sizeof(int)))
            {
                lua_pushinteger(L, *pMoney);
                return 1;
            }
        }
        lua_pushinteger(L, 0);
        return 1;
    }

    // ==========================================
    // Lua API: Vehicle
    // ==========================================

    // Vehicle.GetPlayerVehicle() -> lightuserdata or nil
    static int Lua_Vehicle_GetPlayerVehicle(lua_State* L)
    {
        return Lua_Player_GetVehicle(L);
    }

    // Vehicle.GetPosition([veh]) -> x, y, z
    static int Lua_Vehicle_GetPosition(lua_State* L)
    {
        void* veh = nullptr;
        if (lua_gettop(L) >= 1) veh = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(veh))
        {
            veh = GetLocalPlayerVehicle();
        }

        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (GetEntityPosition(veh, x, y, z))
        {
            lua_pushnumber(L, (lua_Number)x);
            lua_pushnumber(L, (lua_Number)y);
            lua_pushnumber(L, (lua_Number)z);
            return 3;
        }

        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        lua_pushnumber(L, 0.0);
        return 3;
    }

    // Vehicle.SetPosition(veh, x, y, z) or Vehicle.SetPosition(x, y, z) or Vehicle.Teleport(...)
    static int Lua_Vehicle_SetPosition(lua_State* L)
    {
        int top = lua_gettop(L);
        void* veh = nullptr;
        float x = 0.0f, y = 0.0f, z = 0.0f;

        if (top >= 4)
        {
            veh = GetPointerFromArg(L, 1);
            x = (float)luaL_checknumber(L, 2);
            y = (float)luaL_checknumber(L, 3);
            z = (float)luaL_checknumber(L, 4);
        }
        else if (top >= 3)
        {
            veh = GetLocalPlayerVehicle();
            x = (float)luaL_checknumber(L, 1);
            y = (float)luaL_checknumber(L, 2);
            z = (float)luaL_checknumber(L, 3);
        }
        else
        {
            return luaL_error(L, "Vehicle.SetPosition expects (x, y, z) or (veh, x, y, z)");
        }

        bool ok = SetEntityPosition(veh, x, y, z);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }

    // Vehicle.GetVelocity([veh]) -> vx, vy, vz
    static int Lua_Vehicle_GetVelocity(lua_State* L)
    {
        void* veh = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(veh))
        {
            veh = GetLocalPlayerVehicle();
        }
        if (!IsValidGameObject(veh))
        {
            lua_pushnumber(L, 0.0);
            lua_pushnumber(L, 0.0);
            lua_pushnumber(L, 0.0);
            return 3;
        }

        float* pMove = (float*)((uintptr_t)veh + OFF_MOVE_SPEED);
        if (IsValidMemory(pMove, sizeof(float) * 3))
        {
            lua_pushnumber(L, (lua_Number)pMove[0]);
            lua_pushnumber(L, (lua_Number)pMove[1]);
            lua_pushnumber(L, (lua_Number)pMove[2]);
        }
        else
        {
            lua_pushnumber(L, 0.0);
            lua_pushnumber(L, 0.0);
            lua_pushnumber(L, 0.0);
        }
        return 3;
    }

    // Vehicle.SetVelocity(veh, vx, vy, vz) or Vehicle.SetVelocity(vx, vy, vz)
    static int Lua_Vehicle_SetVelocity(lua_State* L)
    {
        int top = lua_gettop(L);
        void* veh = nullptr;
        float vx = 0.0f, vy = 0.0f, vz = 0.0f;

        if (top >= 4)
        {
            veh = GetPointerFromArg(L, 1);
            vx = (float)luaL_checknumber(L, 2);
            vy = (float)luaL_checknumber(L, 3);
            vz = (float)luaL_checknumber(L, 4);
        }
        else
        {
            veh = GetLocalPlayerVehicle();
            vx = (float)luaL_checknumber(L, 1);
            vy = (float)luaL_checknumber(L, 2);
            vz = (float)luaL_checknumber(L, 3);
        }

        if (!IsValidGameObject(veh)) return 0;

        float* pMove = (float*)((uintptr_t)veh + OFF_MOVE_SPEED);
        if (IsValidMemory(pMove, sizeof(float) * 3))
        {
            pMove[0] = vx;
            pMove[1] = vy;
            pMove[2] = vz;
        }
        return 0;
    }

    // Vehicle.GetSpeed([veh]) -> speed in km/h
    static int Lua_Vehicle_GetSpeed(lua_State* L)
    {
        void* veh = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(veh))
        {
            veh = GetLocalPlayerVehicle();
        }
        if (!IsValidGameObject(veh))
        {
            lua_pushnumber(L, 0.0);
            return 1;
        }

        float* pMove = (float*)((uintptr_t)veh + OFF_MOVE_SPEED);
        if (IsValidMemory(pMove, sizeof(float) * 3))
        {
            float speedUnits = sqrtf(pMove[0]*pMove[0] + pMove[1]*pMove[1] + pMove[2]*pMove[2]);
            float kmh = speedUnits * 200.0f;
            lua_pushnumber(L, (lua_Number)kmh);
        }
        else
        {
            lua_pushnumber(L, 0.0);
        }
        return 1;
    }

    // Vehicle.SetSpeed(veh, kmh) or Vehicle.SetSpeed(kmh)
    static int Lua_Vehicle_SetSpeed(lua_State* L)
    {
        int top = lua_gettop(L);
        void* veh = nullptr;
        float kmh = 0.0f;

        if (top >= 2)
        {
            veh = GetPointerFromArg(L, 1);
            kmh = (float)luaL_checknumber(L, 2);
        }
        else
        {
            veh = GetLocalPlayerVehicle();
            kmh = (float)luaL_checknumber(L, 1);
        }

        if (!IsValidGameObject(veh)) return 0;

        float targetUnits = kmh / 200.0f;
        float* pMove = (float*)((uintptr_t)veh + OFF_MOVE_SPEED);
        if (!IsValidMemory(pMove, sizeof(float) * 3)) return 0;

        float currentUnits = sqrtf(pMove[0]*pMove[0] + pMove[1]*pMove[1] + pMove[2]*pMove[2]);
        if (currentUnits > 0.001f)
        {
            float factor = targetUnits / currentUnits;
            pMove[0] *= factor;
            pMove[1] *= factor;
            pMove[2] *= factor;
        }
        else
        {
            // Pushed in heading direction if stopped
            float* pHeading = (float*)((uintptr_t)veh + OFF_HEADING);
            float heading = (IsValidMemory(pHeading, sizeof(float))) ? *pHeading : 0.0f;
            pMove[0] = -sinf(heading) * targetUnits;
            pMove[1] = cosf(heading) * targetUnits;
            pMove[2] = 0.0f;
        }
        return 0;
    }

    // Vehicle.Repair([veh])
    static int Lua_Vehicle_Repair(lua_State* L)
    {
        void* veh = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(veh))
        {
            veh = GetLocalPlayerVehicle();
        }
        if (!IsValidGameObject(veh)) return 0;

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

    // Vehicle.GetHealth([veh]) -> number
    static int Lua_Vehicle_GetHealth(lua_State* L)
    {
        void* veh = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(veh))
        {
            veh = GetLocalPlayerVehicle();
        }
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

    // Vehicle.SetHealth(veh, hp) or Vehicle.SetHealth(hp)
    static int Lua_Vehicle_SetHealth(lua_State* L)
    {
        int top = lua_gettop(L);
        void* veh = nullptr;
        float hp = 1000.0f;

        if (top >= 2)
        {
            veh = GetPointerFromArg(L, 1);
            hp = (float)luaL_checknumber(L, 2);
        }
        else
        {
            veh = GetLocalPlayerVehicle();
            hp = (float)luaL_checknumber(L, 1);
        }

        if (!IsValidGameObject(veh)) return 0;

        void* pVehHealth = (void*)((uintptr_t)veh + OFF_VEH_HEALTH);
        if (!IsValidMemory(pVehHealth, sizeof(float))) return 0;

        if (hp < 0.0f) hp = 0.0f;
        if (hp > 2000.0f) hp = 2000.0f;

        *(float*)pVehHealth = hp;
        return 0;
    }

    // Vehicle.BlowUp([veh]) / Vehicle.Explode([veh])
    static int Lua_Vehicle_BlowUp(lua_State* L)
    {
        void* veh = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(veh))
        {
            veh = GetLocalPlayerVehicle();
        }
        if (!IsValidGameObject(veh)) return 0;

        void* pVehHealth = (void*)((uintptr_t)veh + OFF_VEH_HEALTH);
        if (IsValidMemory(pVehHealth, sizeof(float)))
        {
            *(float*)pVehHealth = 0.0f;
        }

        if (pfnVehicleBlowUp)
        {
            pfnVehicleBlowUp(veh, nullptr, 0);
        }

        float x = 0, y = 0, z = 0;
        if (GetEntityPosition(veh, x, y, z))
        {
            CreateExplosionInternal(x, y, z, 4, 10.0f, true, 1.0f);
        }
        return 0;
    }

    // Vehicle.SetDoorLock(veh, lockType) or Vehicle.SetDoorLock(lockType)
    static int Lua_Vehicle_SetDoorLock(lua_State* L)
    {
        int top = lua_gettop(L);
        void* veh = nullptr;
        int lockType = 0;

        if (top >= 2)
        {
            veh = GetPointerFromArg(L, 1);
            lockType = (int)luaL_checkinteger(L, 2);
        }
        else
        {
            veh = GetLocalPlayerVehicle();
            lockType = (int)luaL_checkinteger(L, 1);
        }

        if (!IsValidGameObject(veh)) return 0;

        unsigned int* pDoorLock = (unsigned int*)((uintptr_t)veh + OFF_DOOR_LOCK);
        if (IsValidMemory(pDoorLock, sizeof(unsigned int)))
        {
            *pDoorLock = (unsigned int)lockType;
        }
        return 0;
    }

    // Vehicle.SetLocked(veh, bool) or Vehicle.SetLocked(bool)
    static int Lua_Vehicle_SetLocked(lua_State* L)
    {
        int top = lua_gettop(L);
        void* veh = nullptr;
        bool locked = true;

        if (top >= 2)
        {
            veh = GetPointerFromArg(L, 1);
            locked = lua_toboolean(L, 2) != 0;
        }
        else
        {
            veh = GetLocalPlayerVehicle();
            locked = lua_toboolean(L, 1) != 0;
        }

        if (!IsValidGameObject(veh)) return 0;

        unsigned int* pDoorLock = (unsigned int*)((uintptr_t)veh + OFF_DOOR_LOCK);
        if (IsValidMemory(pDoorLock, sizeof(unsigned int)))
        {
            *pDoorLock = locked ? 2 : 1; // 2 = DOORLOCK_LOCKED, 1 = DOORLOCK_UNLOCKED
        }
        return 0;
    }

    // Vehicle.IsLocked([veh]) -> boolean
    static int Lua_Vehicle_IsLocked(lua_State* L)
    {
        void* veh = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(veh))
        {
            veh = GetLocalPlayerVehicle();
        }
        if (!IsValidGameObject(veh))
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        unsigned int* pDoorLock = (unsigned int*)((uintptr_t)veh + OFF_DOOR_LOCK);
        if (IsValidMemory(pDoorLock, sizeof(unsigned int)))
        {
            lua_pushboolean(L, (*pDoorLock == 2 || *pDoorLock == 4) ? 1 : 0);
            return 1;
        }
        lua_pushboolean(L, 0);
        return 1;
    }

    // Vehicle.GetHeading([veh]) -> degrees (0-360)
    static int Lua_Vehicle_GetHeading(lua_State* L)
    {
        void* veh = nullptr;
        if (lua_gettop(L) >= 1) veh = GetPointerFromArg(L, 1);
        if (!IsValidGameObject(veh)) veh = GetLocalPlayerVehicle();
        if (!IsValidGameObject(veh))
        {
            lua_pushnumber(L, 0.0);
            return 1;
        }

        float* pHeading = (float*)((uintptr_t)veh + OFF_HEADING);
        float rad = (IsValidMemory(pHeading, sizeof(float))) ? *pHeading : 0.0f;
        float deg = rad * 57.2957795f;
        if (deg < 0.0f) deg += 360.0f;
        lua_pushnumber(L, (lua_Number)deg);
        return 1;
    }

    // Vehicle.SetHeading([veh], deg)
    static int Lua_Vehicle_SetHeading(lua_State* L)
    {
        int top = lua_gettop(L);
        void* veh = nullptr;
        float deg = 0.0f;

        if (top >= 2)
        {
            veh = GetPointerFromArg(L, 1);
            deg = (float)luaL_checknumber(L, 2);
        }
        else
        {
            veh = GetLocalPlayerVehicle();
            deg = (float)luaL_checknumber(L, 1);
        }

        if (!IsValidGameObject(veh)) return 0;

        float rad = deg * 0.0174532925f;
        float* pHeading = (float*)((uintptr_t)veh + OFF_HEADING);
        if (IsValidMemory(pHeading, sizeof(float)))
        {
            *pHeading = rad;
        }
        return 0;
    }

    // ==========================================
    // Lua API: Explosion
    // ==========================================

    // Explosion.Create(x, y, z, [type], [radius], [sound], [shake])
    static int Lua_Explosion_Create(lua_State* L)
    {
        float x = (float)luaL_checknumber(L, 1);
        float y = (float)luaL_checknumber(L, 2);
        float z = (float)luaL_checknumber(L, 3);
        int type = (int)luaL_optinteger(L, 4, 3); // default 3 = CAR / Medium explosion
        float radius = (float)luaL_optnumber(L, 5, 10.0);
        bool sound = lua_isboolean(L, 6) ? (lua_toboolean(L, 6) != 0) : true;
        float shake = (float)luaL_optnumber(L, 7, 1.0);

        bool ok = CreateExplosionInternal(x, y, z, type, radius, sound, shake);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }

    // Explosion.CreateAtPlayer([type], [radius], [sound], [shake])
    static int Lua_Explosion_CreateAtPlayer(lua_State* L)
    {
        void* ped = nullptr;
        if (pfnFindPlayerPed) ped = pfnFindPlayerPed(-1);
        if (!IsValidGameObject(ped))
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        float x = 0, y = 0, z = 0;
        if (!GetEntityPosition(ped, x, y, z))
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        int type = (int)luaL_optinteger(L, 1, 3);
        float radius = (float)luaL_optnumber(L, 2, 10.0);
        bool sound = lua_isboolean(L, 3) ? (lua_toboolean(L, 3) != 0) : true;
        float shake = (float)luaL_optnumber(L, 4, 1.0);

        bool ok = CreateExplosionInternal(x, y, z, type, radius, sound, shake);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }

    // Explosion.CreateAtVehicle([veh], [type], [radius], [sound], [shake])
    static int Lua_Explosion_CreateAtVehicle(lua_State* L)
    {
        int argStart = 1;
        void* veh = nullptr;
        if (lua_islightuserdata(L, 1) || lua_isinteger(L, 1))
        {
            veh = GetPointerFromArg(L, 1);
            argStart = 2;
        }
        else
        {
            veh = GetLocalPlayerVehicle();
        }

        if (!IsValidGameObject(veh))
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        float x = 0, y = 0, z = 0;
        if (!GetEntityPosition(veh, x, y, z))
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        int type = (int)luaL_optinteger(L, argStart, 4);
        float radius = (float)luaL_optnumber(L, argStart + 1, 10.0);
        bool sound = lua_isboolean(L, argStart + 2) ? (lua_toboolean(L, argStart + 2) != 0) : true;
        float shake = (float)luaL_optnumber(L, argStart + 3, 1.0);

        bool ok = CreateExplosionInternal(x, y, z, type, radius, sound, shake);
        lua_pushboolean(L, ok ? 1 : 0);
        return 1;
    }

    // ==========================================
    // Lua API: Game / World
    // ==========================================

    // Display text in GTA SA native top-right dialog box (CHud::SetHelpMessage)
    void DisplayHelpBox(const char* text, unsigned int duration)
    {
        if (!text || text[0] == '\0') return;

        // Circular buffer to ensure pointers passed to the engine remain valid across frames
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

    // Game.Teleport(x, y, z) - teleports player or current vehicle seamlessly
    static int Lua_Game_Teleport(lua_State* L)
    {
        return Lua_Player_SetPosition(L);
    }

    // Game.GetGroundZ(x, y, [z]) -> ground height
    static int Lua_Game_GetGroundZ(lua_State* L)
    {
        float x = (float)luaL_checknumber(L, 1);
        float y = (float)luaL_checknumber(L, 2);
        float z = (float)luaL_optnumber(L, 3, 100.0);

        float groundZ = 0.0f;
        bool found = false;

        if (pfnFindGroundZFor3DCoord)
        {
            bool bFound = false;
            void* entity = nullptr;
            groundZ = pfnFindGroundZFor3DCoord(x, y, z, &bFound, &entity);
            if (bFound) found = true;
        }

        if (!found && pfnFindGroundZForCoord)
        {
            groundZ = pfnFindGroundZForCoord(x, y);
            found = true;
        }

        if (found)
        {
            lua_pushnumber(L, (lua_Number)groundZ);
        }
        else
        {
            lua_pushnumber(L, (lua_Number)z);
        }
        return 1;
    }

    // Game.SetTime(hour, minute) / Game.SetClock(hour, minute)
    static int Lua_Game_SetTime(lua_State* L)
    {
        int hour = (int)luaL_checkinteger(L, 1);
        int minute = (int)luaL_optinteger(L, 2, 0);

        if (hour < 0) hour = 0;
        if (hour > 23) hour = 23;
        if (minute < 0) minute = 0;
        if (minute > 59) minute = 59;

        if (pfnSetGameClock)
        {
            pfnSetGameClock((unsigned char)hour, (unsigned char)minute, 0);
        }
        else
        {
            if (pClockHours && IsValidMemory(pClockHours, 1)) *pClockHours = (unsigned char)hour;
            if (pClockMinutes && IsValidMemory(pClockMinutes, 1)) *pClockMinutes = (unsigned char)minute;
        }
        return 0;
    }

    // Game.GetTime() / Game.GetClock() -> hour, minute
    static int Lua_Game_GetTime(lua_State* L)
    {
        int hour = 12;
        int min = 0;
        if (pClockHours && IsValidMemory(pClockHours, 1)) hour = *pClockHours;
        if (pClockMinutes && IsValidMemory(pClockMinutes, 1)) min = *pClockMinutes;

        lua_pushinteger(L, hour);
        lua_pushinteger(L, min);
        return 2;
    }

    // Game.SetWeather(weatherId)
    static int Lua_Game_SetWeather(lua_State* L)
    {
        int weatherId = (int)luaL_checkinteger(L, 1);
        if (pfnForceWeatherNow)
        {
            pfnForceWeatherNow((short)weatherId);
        }
        else if (pfnForceWeather)
        {
            pfnForceWeather((short)weatherId);
        }
        return 0;
    }

    // Game.ReleaseWeather()
    static int Lua_Game_ReleaseWeather(lua_State* L)
    {
        if (pfnReleaseWeather)
        {
            pfnReleaseWeather();
        }
        return 0;
    }

    // Game.SetGameSpeed(speed) / Game.SetTimeScale(speed)
    static int Lua_Game_SetGameSpeed(lua_State* L)
    {
        float speed = (float)luaL_checknumber(L, 1);
        if (speed < 0.0f) speed = 0.0f;
        if (speed > 10.0f) speed = 10.0f;

        if (pTimeScale && IsValidMemory(pTimeScale, sizeof(float)))
        {
            *pTimeScale = speed;
        }
        return 0;
    }

    // Game.GetGameSpeed() / Game.GetTimeScale() -> number
    static int Lua_Game_GetGameSpeed(lua_State* L)
    {
        float speed = 1.0f;
        if (pTimeScale && IsValidMemory(pTimeScale, sizeof(float)))
        {
            speed = *pTimeScale;
        }
        lua_pushnumber(L, (lua_Number)speed);
        return 1;
    }

    // Game.GetFPS() -> number
    static int Lua_Game_GetFPS(lua_State* L)
    {
        float fps = 30.0f;
        if (pGameFPS && IsValidMemory(pGameFPS, sizeof(float)))
        {
            fps = *pGameFPS;
        }
        lua_pushnumber(L, (lua_Number)fps);
        return 1;
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

    // -------------------------------------------------------------
    // Real-Time Timer & Interval Subsystem (Seconds-based)
    // -------------------------------------------------------------
    struct LuaTimer
    {
        int id;
        int luaFuncRef;      // reference in LUA_REGISTRYINDEX
        double intervalSec;  // duration in real seconds
        double remainingSec; // count down in real seconds
        bool isRepeating;    // true for SetInterval / Every, false for SetTimeout / After
        bool isAlive;        // true if active
    };

    static std::vector<LuaTimer> g_ActiveTimers;
    static int g_NextTimerId = 1;

    static void ClearAllTimers(lua_State* L)
    {
        if (!L) return;
        for (auto& t : g_ActiveTimers)
        {
            if (t.isAlive && t.luaFuncRef != LUA_NOREF && t.luaFuncRef != LUA_REFNIL)
            {
                luaL_unref(L, LUA_REGISTRYINDEX, t.luaFuncRef);
                t.luaFuncRef = LUA_NOREF;
            }
            t.isAlive = false;
        }
        g_ActiveTimers.clear();
    }

    // Flexible timer creation helper: accepts (fn, seconds) or (seconds, fn)
    static int CreateTimerHelper(lua_State* L, bool isRepeating)
    {
        int fnIndex = -1;
        double seconds = 0.0;

        if (lua_isfunction(L, 1) && lua_isnumber(L, 2))
        {
            fnIndex = 1;
            seconds = (double)lua_tonumber(L, 2);
        }
        else if (lua_isnumber(L, 1) && lua_isfunction(L, 2))
        {
            seconds = (double)lua_tonumber(L, 1);
            fnIndex = 2;
        }
        else if (lua_isfunction(L, 1) && lua_gettop(L) == 1)
        {
            fnIndex = 1;
            seconds = 1.0;
        }
        else
        {
            return luaL_error(L, "Timer expected (function, seconds) or (seconds, function)");
        }

        if (seconds < 0.001)
        {
            seconds = 0.001;
        }

        lua_pushvalue(L, fnIndex);
        int ref = luaL_ref(L, LUA_REGISTRYINDEX);

        LuaTimer timer;
        timer.id = g_NextTimerId++;
        timer.luaFuncRef = ref;
        timer.intervalSec = seconds;
        timer.remainingSec = seconds;
        timer.isRepeating = isRepeating;
        timer.isAlive = true;

        g_ActiveTimers.push_back(timer);

        lua_pushinteger(L, timer.id);
        return 1;
    }

    static int Lua_Game_SetInterval(lua_State* L)
    {
        return CreateTimerHelper(L, true);
    }

    static int Lua_Game_SetTimeout(lua_State* L)
    {
        return CreateTimerHelper(L, false);
    }

    static int Lua_Game_ClearTimer(lua_State* L)
    {
        int timerId = (int)luaL_checkinteger(L, 1);
        bool found = false;
        for (auto& t : g_ActiveTimers)
        {
            if (t.id == timerId && t.isAlive)
            {
                t.isAlive = false;
                if (t.luaFuncRef != LUA_NOREF && t.luaFuncRef != LUA_REFNIL)
                {
                    luaL_unref(L, LUA_REGISTRYINDEX, t.luaFuncRef);
                    t.luaFuncRef = LUA_NOREF;
                }
                found = true;
                break;
            }
        }
        lua_pushboolean(L, found ? 1 : 0);
        return 1;
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
        DisplayHelpBox(text.c_str(), 6000);
    }

    static int Lua_ShowScriptList(lua_State* L)
    {
        ShowScriptListDialog();
        return 0;
    }

    // Game.DoFile(path) / AMLua.DoFile(path)
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
            lua_pop(L, 2);
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
            lua_pop(L, 2);
            lua_pushboolean(L, 0);
            return 1;
        }

        lua_remove(L, errHandler);
        return lua_gettop(L) - (errHandler - 1);
    }

    // Game.RunString(code) / AMLua.RunString(code)
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

    void LoadScripts(const char* scriptsDir);

    // Game.ReloadScripts() / AMLua.ReloadScripts()
    static int Lua_ReloadScripts(lua_State* L)
    {
        ClearAllTimers(L);

        lua_pushnil(L);
        lua_setfield(L, LUA_REGISTRYINDEX, "AMLua_TickCallbacks");

        LoadScripts(g_ScriptsDirPath.c_str());

        DisplayHelpBox("~g~AMLua: Scripts Reloaded!", 3000);
        return 0;
    }

    // Touch event gesture detection
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

            bool isTopRight = (x > (int)(s_MaxX * 0.55f) && y < (int)(s_MaxY * 0.40f));
            bool isTopBar = (y < (int)(s_MaxY * 0.25f));

            if ((isTopRight || isTopBar) && elapsedMs < 650.0 && elapsedMs > 40.0)
            {
                ShowScriptListDialog();
                return;
            }

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

    // =========================================================================
    // v1.1.0 Extended Game & System APIs: Weapon, Device, Audio, File, Screen, Camera, Vehicle
    // =========================================================================

    // Helper: Safely resolve CPed pointer and SCM handle
    static void* GetPedFromArg(lua_State* L, int idx)
    {
        if (lua_isnoneornil(L, idx))
        {
            if (pfnFindPlayerPed) return pfnFindPlayerPed(-1);
            return nullptr;
        }
        if (lua_islightuserdata(L, idx))
        {
            void* p = lua_touserdata(L, idx);
            return IsValidGameObject(p) ? p : nullptr;
        }
        if (lua_isinteger(L, idx))
        {
            int val = (int)lua_tointeger(L, idx);
            if (pfnGetPed && val > 0 && val < 100000)
            {
                void* p = pfnGetPed(val);
                if (IsValidGameObject(p)) return p;
            }
            void* p = (void*)(uintptr_t)val;
            return IsValidGameObject(p) ? p : nullptr;
        }
        return nullptr;
    }

    static int GetPedHandle(void* ped)
    {
        if (!IsValidGameObject(ped)) return 0;
        if (pfnGetPedRef)
        {
            return pfnGetPedRef(ped);
        }
        return 0;
    }

    // Helper: Safely resolve CVehicle pointer and SCM handle
    static void* GetVehicleFromArg(lua_State* L, int idx)
    {
        if (lua_isnoneornil(L, idx))
        {
            return GetLocalPlayerVehicle();
        }
        if (lua_islightuserdata(L, idx))
        {
            void* p = lua_touserdata(L, idx);
            return IsValidGameObject(p) ? p : nullptr;
        }
        if (lua_isinteger(L, idx))
        {
            int val = (int)lua_tointeger(L, idx);
            if (pfnGetVehicle && val > 0 && val < 100000)
            {
                void* p = pfnGetVehicle(val);
                if (IsValidGameObject(p)) return p;
            }
            void* p = (void*)(uintptr_t)val;
            return IsValidGameObject(p) ? p : nullptr;
        }
        return nullptr;
    }

    static int GetVehicleHandle(void* veh)
    {
        if (!IsValidGameObject(veh)) return 0;
        if (pfnGetVehicleRef)
        {
            return pfnGetVehicleRef(veh);
        }
        return 0;
    }

    // -------------------------------------------------------------------------
    // Vehicle Extended Bindings
    // -------------------------------------------------------------------------

    // Vehicle.Create(modelId, [x, y, z, heading]) -> handle, pointer
    static int Lua_Vehicle_Create(lua_State* L)
    {
        int modelId = (int)luaL_checkinteger(L, 1);
        float x = 0, y = 0, z = 0;
        float heading = 0.0f;

        if (lua_gettop(L) >= 4)
        {
            x = (float)luaL_checknumber(L, 2);
            y = (float)luaL_checknumber(L, 3);
            z = (float)luaL_checknumber(L, 4);
            if (lua_gettop(L) >= 5)
            {
                heading = (float)luaL_checknumber(L, 5);
            }
        }
        else
        {
            // Default: 5 units ahead of player in facing direction
            void* ped = pfnFindPlayerPed ? pfnFindPlayerPed(-1) : nullptr;
            if (ped)
            {
                GetEntityPosition(ped, x, y, z);
                float pHeading = 0.0f;
                float* pH = (float*)((uintptr_t)ped + OFF_HEADING);
                if (IsValidMemory(pH, sizeof(float))) pHeading = *pH;
                heading = pHeading;
                float rad = pHeading * (3.14159265f / 180.0f);
                x -= sinf(rad) * 5.0f;
                y += cosf(rad) * 5.0f;
            }
        }

        if (!pfnProcessOneCommand)
        {
            lua_pushnil(L);
            lua_pushstring(L, "SCM execution engine not available");
            return 2;
        }

        // 1. Request model (0247)
        RunSCM(0x0247, "i", modelId);
        // 2. Load all models now (038B)
        RunSCM(0x038B, "");

        // 3. Create car into 0@ (00A5)
        s_Script.m_aLocalVars[0].iParam = 0;
        RunSCM(0x00A5, "ifffv", modelId, x, y, z, 0);
        int carHandle = s_Script.m_aLocalVars[0].iParam;

        if (carHandle > 0 && heading != 0.0f)
        {
            // 4. Set car heading (0175)
            RunSCM(0x0175, "vf", 0, heading);
        }

        // 5. Mark model as no longer needed (0249)
        RunSCM(0x0249, "i", modelId);

        void* pVeh = nullptr;
        if (pfnGetVehicle && carHandle > 0)
        {
            pVeh = pfnGetVehicle(carHandle);
        }

        lua_pushinteger(L, carHandle);
        if (pVeh)
        {
            lua_pushlightuserdata(L, pVeh);
            return 2;
        }
        return 1;
    }

    // Vehicle.SetColor(veh, primary, secondary)
    static int Lua_Vehicle_SetColor(lua_State* L)
    {
        int arg = 1;
        void* veh = nullptr;
        int h = 0;

        if (lua_islightuserdata(L, 1) || (lua_isinteger(L, 1) && lua_gettop(L) >= 3))
        {
            veh = GetVehicleFromArg(L, 1);
            if (lua_isinteger(L, 1)) h = (int)lua_tointeger(L, 1);
            arg = 2;
        }
        else
        {
            veh = GetLocalPlayerVehicle();
        }

        int primary = (int)luaL_checkinteger(L, arg);
        int secondary = (int)luaL_checkinteger(L, arg + 1);

        if (h <= 0 && veh)
        {
            h = GetVehicleHandle(veh);
        }

        if (h > 0)
        {
            RunSCM(0x0229, "iii", h, primary, secondary);
            lua_pushboolean(L, 1);
            return 1;
        }

        lua_pushboolean(L, 0);
        return 1;
    }

    // Vehicle.SetEngineState(veh, state)
    static int Lua_Vehicle_SetEngineState(lua_State* L)
    {
        int arg = 1;
        void* veh = nullptr;
        int h = 0;

        if (lua_islightuserdata(L, 1) || (lua_isinteger(L, 1) && lua_gettop(L) >= 2))
        {
            veh = GetVehicleFromArg(L, 1);
            if (lua_isinteger(L, 1)) h = (int)lua_tointeger(L, 1);
            arg = 2;
        }
        else
        {
            veh = GetLocalPlayerVehicle();
        }

        bool state = lua_toboolean(L, arg);
        if (h <= 0 && veh)
        {
            h = GetVehicleHandle(veh);
        }

        if (h > 0)
        {
            RunSCM(0x0918, "ii", h, state ? 1 : 0);
            lua_pushboolean(L, 1);
            return 1;
        }

        lua_pushboolean(L, 0);
        return 1;
    }

    // Vehicle.PopTyre(veh, tyreId)
    static int Lua_Vehicle_PopTyre(lua_State* L)
    {
        int arg = 1;
        void* veh = nullptr;
        int h = 0;

        if (lua_islightuserdata(L, 1) || (lua_isinteger(L, 1) && lua_gettop(L) >= 2))
        {
            veh = GetVehicleFromArg(L, 1);
            if (lua_isinteger(L, 1)) h = (int)lua_tointeger(L, 1);
            arg = 2;
        }
        else
        {
            veh = GetLocalPlayerVehicle();
        }

        int tyreId = (int)luaL_checkinteger(L, arg);
        if (h <= 0 && veh)
        {
            h = GetVehicleHandle(veh);
        }

        if (h > 0)
        {
            RunSCM(0x04FE, "ii", h, tyreId);
            lua_pushboolean(L, 1);
            return 1;
        }

        lua_pushboolean(L, 0);
        return 1;
    }

    // Vehicle.GetHandle(veh)
    static int Lua_Vehicle_GetHandle(lua_State* L)
    {
        void* veh = GetVehicleFromArg(L, 1);
        if (!veh) return 0;
        int h = GetVehicleHandle(veh);
        lua_pushinteger(L, h);
        return 1;
    }

    // Vehicle.GetPointer(veh)
    static int Lua_Vehicle_GetPointer(lua_State* L)
    {
        void* veh = GetVehicleFromArg(L, 1);
        if (!veh) return 0;
        lua_pushlightuserdata(L, veh);
        return 1;
    }

    // -------------------------------------------------------------------------
    // Weapon Bindings
    // -------------------------------------------------------------------------

    // Weapon.Give(ped or nil, weaponId, ammo)
    static int Lua_Weapon_Give(lua_State* L)
    {
        int arg = 1;
        void* ped = nullptr;

        if (lua_islightuserdata(L, 1) || (lua_isinteger(L, 1) && lua_gettop(L) >= 3))
        {
            ped = GetPedFromArg(L, 1);
            arg = 2;
        }
        else
        {
            ped = pfnFindPlayerPed ? pfnFindPlayerPed(-1) : nullptr;
        }

        if (!IsValidGameObject(ped))
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        int weaponId = (int)luaL_checkinteger(L, arg);
        int ammo = (int)luaL_checkinteger(L, arg + 1);

        if (pfnGiveWeapon)
        {
            pfnGiveWeapon(ped, weaponId, (unsigned int)ammo, true);
            lua_pushboolean(L, 1);
            return 1;
        }

        int h = GetPedHandle(ped);
        if (h > 0)
        {
            RunSCM(0x01B2, "iii", h, weaponId, ammo);
            lua_pushboolean(L, 1);
            return 1;
        }

        lua_pushboolean(L, 0);
        return 1;
    }

    // Weapon.SetCurrent(ped or nil, weaponId)
    static int Lua_Weapon_SetCurrent(lua_State* L)
    {
        int arg = 1;
        void* ped = nullptr;

        if (lua_islightuserdata(L, 1) || (lua_isinteger(L, 1) && lua_gettop(L) >= 2))
        {
            ped = GetPedFromArg(L, 1);
            arg = 2;
        }
        else
        {
            ped = pfnFindPlayerPed ? pfnFindPlayerPed(-1) : nullptr;
        }

        if (!IsValidGameObject(ped))
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        int weaponId = (int)luaL_checkinteger(L, arg);
        if (pfnSetCurrentWeapon)
        {
            pfnSetCurrentWeapon(ped, weaponId);
            lua_pushboolean(L, 1);
            return 1;
        }

        int h = GetPedHandle(ped);
        if (h > 0)
        {
            RunSCM(0x01B8, "iii", h, weaponId, 1);
            lua_pushboolean(L, 1);
            return 1;
        }

        lua_pushboolean(L, 0);
        return 1;
    }

    // Weapon.Remove(ped or nil, weaponId)
    static int Lua_Weapon_Remove(lua_State* L)
    {
        int arg = 1;
        void* ped = nullptr;

        if (lua_islightuserdata(L, 1) || (lua_isinteger(L, 1) && lua_gettop(L) >= 2))
        {
            ped = GetPedFromArg(L, 1);
            arg = 2;
        }
        else
        {
            ped = pfnFindPlayerPed ? pfnFindPlayerPed(-1) : nullptr;
        }

        if (!IsValidGameObject(ped))
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        int weaponId = (int)luaL_checkinteger(L, arg);
        if (pfnRemoveWeapon)
        {
            pfnRemoveWeapon(ped, weaponId);
            lua_pushboolean(L, 1);
            return 1;
        }

        int h = GetPedHandle(ped);
        if (h > 0)
        {
            RunSCM(0x017A, "ii", h, weaponId);
            lua_pushboolean(L, 1);
            return 1;
        }

        lua_pushboolean(L, 0);
        return 1;
    }

    // Weapon.RemoveAll(ped or nil)
    static int Lua_Weapon_RemoveAll(lua_State* L)
    {
        void* ped = GetPedFromArg(L, 1);
        if (!IsValidGameObject(ped))
        {
            lua_pushboolean(L, 0);
            return 1;
        }

        if (pfnClearWeapons)
        {
            pfnClearWeapons(ped);
            lua_pushboolean(L, 1);
            return 1;
        }

        int h = GetPedHandle(ped);
        if (h > 0)
        {
            RunSCM(0x048F, "i", h);
            lua_pushboolean(L, 1);
            return 1;
        }

        lua_pushboolean(L, 0);
        return 1;
    }

    static void RegisterWeaponConstants(lua_State* L)
    {
        lua_pushinteger(L, 0);  lua_setfield(L, -2, "FIST");
        lua_pushinteger(L, 1);  lua_setfield(L, -2, "BRASSKNUCKLE");
        lua_pushinteger(L, 2);  lua_setfield(L, -2, "GOLFCLUB");
        lua_pushinteger(L, 3);  lua_setfield(L, -2, "NIGHTSTICK");
        lua_pushinteger(L, 4);  lua_setfield(L, -2, "KNIFE");
        lua_pushinteger(L, 5);  lua_setfield(L, -2, "BASEBALLBAT");
        lua_pushinteger(L, 6);  lua_setfield(L, -2, "SHOVEL");
        lua_pushinteger(L, 7);  lua_setfield(L, -2, "POOLCUE");
        lua_pushinteger(L, 8);  lua_setfield(L, -2, "KATANA");
        lua_pushinteger(L, 9);  lua_setfield(L, -2, "CHAINSAW");
        lua_pushinteger(L, 14); lua_setfield(L, -2, "FLOWERS");
        lua_pushinteger(L, 15); lua_setfield(L, -2, "CANE");
        lua_pushinteger(L, 16); lua_setfield(L, -2, "GRENADE");
        lua_pushinteger(L, 17); lua_setfield(L, -2, "TEARGAS");
        lua_pushinteger(L, 18); lua_setfield(L, -2, "MOLOTOV");
        lua_pushinteger(L, 22); lua_setfield(L, -2, "PISTOL");
        lua_pushinteger(L, 23); lua_setfield(L, -2, "PISTOL_SILENCED");
        lua_pushinteger(L, 24); lua_setfield(L, -2, "DESERT_EAGLE");
        lua_pushinteger(L, 25); lua_setfield(L, -2, "SHOTGUN");
        lua_pushinteger(L, 26); lua_setfield(L, -2, "SAWNOFF");
        lua_pushinteger(L, 27); lua_setfield(L, -2, "SPAS12");
        lua_pushinteger(L, 28); lua_setfield(L, -2, "MICRO_UZI");
        lua_pushinteger(L, 29); lua_setfield(L, -2, "MP5");
        lua_pushinteger(L, 30); lua_setfield(L, -2, "AK47");
        lua_pushinteger(L, 31); lua_setfield(L, -2, "M4");
        lua_pushinteger(L, 32); lua_setfield(L, -2, "TEC9");
        lua_pushinteger(L, 33); lua_setfield(L, -2, "RIFLE");
        lua_pushinteger(L, 34); lua_setfield(L, -2, "SNIPER");
        lua_pushinteger(L, 35); lua_setfield(L, -2, "ROCKETLAUNCHER");
        lua_pushinteger(L, 36); lua_setfield(L, -2, "HEATSEEKER");
        lua_pushinteger(L, 37); lua_setfield(L, -2, "FLAMETHROWER");
        lua_pushinteger(L, 38); lua_setfield(L, -2, "MINIGUN");
        lua_pushinteger(L, 39); lua_setfield(L, -2, "SATCHEL");
        lua_pushinteger(L, 40); lua_setfield(L, -2, "DETONATOR");
        lua_pushinteger(L, 41); lua_setfield(L, -2, "SPRAYCAN");
        lua_pushinteger(L, 42); lua_setfield(L, -2, "EXTINGUISHER");
        lua_pushinteger(L, 43); lua_setfield(L, -2, "CAMERA");
        lua_pushinteger(L, 44); lua_setfield(L, -2, "NIGHTVISION");
        lua_pushinteger(L, 45); lua_setfield(L, -2, "THERMAL");
        lua_pushinteger(L, 46); lua_setfield(L, -2, "PARACHUTE");
    }

    // -------------------------------------------------------------------------
    // Device Bindings
    // -------------------------------------------------------------------------

    // Device.Vibrate(durationMs)
    static int Lua_Device_Vibrate(lua_State* L)
    {
        int ms = (int)luaL_optinteger(L, 1, 100);
        if (aml)
        {
            aml->DoVibro(ms);
            lua_pushboolean(L, 1);
            return 1;
        }
        lua_pushboolean(L, 0);
        return 1;
    }

    // Device.CancelVibrate()
    static int Lua_Device_CancelVibrate(lua_State* L)
    {
        if (aml)
        {
            aml->CancelVibro();
            lua_pushboolean(L, 1);
            return 1;
        }
        lua_pushboolean(L, 0);
        return 1;
    }

    // Device.Toast(message, [longerDuration])
    static int Lua_Device_Toast(lua_State* L)
    {
        const char* msg = luaL_checkstring(L, 1);
        bool longer = lua_toboolean(L, 2);
        if (aml)
        {
            aml->ShowToast(longer, "%s", msg);
            lua_pushboolean(L, 1);
            return 1;
        }
        lua_pushboolean(L, 0);
        return 1;
    }

    // Device.GetBatteryLevel() -> float
    static int Lua_Device_GetBatteryLevel(lua_State* L)
    {
        if (aml)
        {
            float level = aml->GetBatteryLevel();
            lua_pushnumber(L, (lua_Number)level);
            return 1;
        }
        lua_pushnumber(L, -1.0);
        return 1;
    }

    // Device.GetAndroidVersion() -> int
    static int Lua_Device_GetAndroidVersion(lua_State* L)
    {
        if (aml)
        {
            int ver = aml->GetAndroidVersion();
            lua_pushinteger(L, ver);
            return 1;
        }
        lua_pushinteger(L, 0);
        return 1;
    }

    // Device.GetDisplaySize() -> width, height
    static int Lua_Device_GetDisplaySize(lua_State* L)
    {
        int w = 0, h = 0;
        if (aml)
        {
            aml->GetDisplaySize(&w, &h);
        }
        lua_pushinteger(L, w);
        lua_pushinteger(L, h);
        return 2;
    }

    // -------------------------------------------------------------------------
    // Audio Bindings
    // -------------------------------------------------------------------------

    // Audio.PlaySound(soundId, [x, y, z])
    static int Lua_Audio_PlaySound(lua_State* L)
    {
        int soundId = (int)luaL_checkinteger(L, 1);
        if (lua_gettop(L) >= 4)
        {
            float x = (float)luaL_checknumber(L, 2);
            float y = (float)luaL_checknumber(L, 3);
            float z = (float)luaL_checknumber(L, 4);
            RunSCM(0x018C, "ifff", soundId, x, y, z);
        }
        else
        {
            RunSCM(0x018D, "i", soundId);
        }
        lua_pushboolean(L, 1);
        return 1;
    }

    // Audio.SetRadioStation(stationId)
    static int Lua_Audio_SetRadioStation(lua_State* L)
    {
        int station = (int)luaL_checkinteger(L, 1);
        RunSCM(0x0417, "i", station);
        lua_pushboolean(L, 1);
        return 1;
    }

    // Audio.GetRadioStation() -> stationId
    static int Lua_Audio_GetRadioStation(lua_State* L)
    {
        s_Script.m_aLocalVars[0].iParam = 0;
        RunSCM(0x051E, "v", 0);
        lua_pushinteger(L, s_Script.m_aLocalVars[0].iParam);
        return 1;
    }

    static void RegisterAudioConstants(lua_State* L)
    {
        lua_pushinteger(L, 0);  lua_setfield(L, -2, "RADIO_OFF");
        lua_pushinteger(L, 1);  lua_setfield(L, -2, "RADIO_BOUNCE");
        lua_pushinteger(L, 2);  lua_setfield(L, -2, "RADIO_CSR");
        lua_pushinteger(L, 3);  lua_setfield(L, -2, "RADIO_K_ROSE");
        lua_pushinteger(L, 4);  lua_setfield(L, -2, "RADIO_K_DST");
        lua_pushinteger(L, 5);  lua_setfield(L, -2, "RADIO_BOUNCE_FM");
        lua_pushinteger(L, 6);  lua_setfield(L, -2, "RADIO_SF_UR");
        lua_pushinteger(L, 7);  lua_setfield(L, -2, "RADIO_LOS_SANTOS");
        lua_pushinteger(L, 8);  lua_setfield(L, -2, "RADIO_RADIO_X");
        lua_pushinteger(L, 9);  lua_setfield(L, -2, "RADIO_CSR_103_9");
        lua_pushinteger(L, 10); lua_setfield(L, -2, "RADIO_K_JAH");
        lua_pushinteger(L, 11); lua_setfield(L, -2, "RADIO_MASTER_SOUNDS");
        lua_pushinteger(L, 12); lua_setfield(L, -2, "RADIO_WCTR");
        lua_pushinteger(L, 13); lua_setfield(L, -2, "RADIO_USER_TRACKS");
    }

    // -------------------------------------------------------------------------
    // Screen & Camera Bindings
    // -------------------------------------------------------------------------

    // Screen.Fade(fadeIn, durationMs)
    static int Lua_Screen_Fade(lua_State* L)
    {
        bool fadeIn = lua_toboolean(L, 1);
        int duration = (int)luaL_optinteger(L, 2, 1000);
        RunSCM(0x016A, "ii", fadeIn ? 1 : 0, duration);
        lua_pushboolean(L, 1);
        return 1;
    }

    // Screen.FadeIn(durationMs)
    static int Lua_Screen_FadeIn(lua_State* L)
    {
        int duration = (int)luaL_optinteger(L, 1, 1000);
        RunSCM(0x016A, "ii", 1, duration);
        lua_pushboolean(L, 1);
        return 1;
    }

    // Screen.FadeOut(durationMs)
    static int Lua_Screen_FadeOut(lua_State* L)
    {
        int duration = (int)luaL_optinteger(L, 1, 1000);
        RunSCM(0x016A, "ii", 0, duration);
        lua_pushboolean(L, 1);
        return 1;
    }

    // Camera.Shake(intensity)
    static int Lua_Camera_Shake(lua_State* L)
    {
        int intensity = (int)luaL_optinteger(L, 1, 70);
        RunSCM(0x0003, "i", intensity);
        lua_pushboolean(L, 1);
        return 1;
    }

    // Camera.Restore()
    static int Lua_Camera_Restore(lua_State* L)
    {
        RunSCM(0x015D, "");
        lua_pushboolean(L, 1);
        return 1;
    }

    // -------------------------------------------------------------------------
    // File I/O Bindings
    // -------------------------------------------------------------------------

    // File.Read(filePath) -> content or nil, error
    static int Lua_File_Read(lua_State* L)
    {
        const char* path = luaL_checkstring(L, 1);
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            lua_pushnil(L);
            lua_pushstring(L, "Could not open file for reading");
            return 2;
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        lua_pushlstring(L, content.data(), content.size());
        return 1;
    }

    // File.Write(filePath, content) -> bool, error
    static int Lua_File_Write(lua_State* L)
    {
        const char* path = luaL_checkstring(L, 1);
        size_t len = 0;
        const char* content = luaL_checklstring(L, 2, &len);

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            lua_pushboolean(L, 0);
            lua_pushstring(L, "Could not open file for writing");
            return 2;
        }

        file.write(content, len);
        lua_pushboolean(L, 1);
        return 1;
    }

    // File.Append(filePath, content) -> bool, error
    static int Lua_File_Append(lua_State* L)
    {
        const char* path = luaL_checkstring(L, 1);
        size_t len = 0;
        const char* content = luaL_checklstring(L, 2, &len);

        std::ofstream file(path, std::ios::binary | std::ios::app);
        if (!file.is_open())
        {
            lua_pushboolean(L, 0);
            lua_pushstring(L, "Could not open file for appending");
            return 2;
        }

        file.write(content, len);
        lua_pushboolean(L, 1);
        return 1;
    }

    // File.Exists(filePath) -> bool
    static int Lua_File_Exists(lua_State* L)
    {
        const char* path = luaL_checkstring(L, 1);
        struct stat st;
        lua_pushboolean(L, stat(path, &st) == 0);
        return 1;
    }

    // File.Delete(filePath) -> bool
    static int Lua_File_Delete(lua_State* L)
    {
        const char* path = luaL_checkstring(L, 1);
        lua_pushboolean(L, remove(path) == 0);
        return 1;
    }

    // File.List(dirPath) -> table of filenames
    static int Lua_File_List(lua_State* L)
    {
        const char* dirPath = luaL_checkstring(L, 1);
        DIR* d = opendir(dirPath);
        if (!d)
        {
            lua_pushnil(L);
            lua_pushstring(L, "Could not open directory");
            return 2;
        }

        lua_newtable(L);
        int idx = 1;
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr)
        {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            lua_pushstring(L, ent->d_name);
            lua_rawseti(L, -2, idx++);
        }
        closedir(d);
        return 1;
    }

    // File.GetScriptsPath() -> string
    static int Lua_File_GetScriptsPath(lua_State* L)
    {
        lua_pushstring(L, g_ScriptsDirPath.c_str());
        return 1;
    }

    // File.GetDataPath() -> string
    static int Lua_File_GetDataPath(lua_State* L)
    {
        size_t lastSlash = g_LogFilePath.find_last_of('/');
        if (lastSlash != std::string::npos)
        {
            lua_pushstring(L, g_LogFilePath.substr(0, lastSlash).c_str());
        }
        else
        {
            lua_pushstring(L, g_LogFilePath.c_str());
        }
        return 1;
    }

    // Sandboxing: disable dangerous os / io / package capabilities
    static void ApplySandbox(lua_State* L)
    {
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

        lua_getglobal(L, "package");
        if (lua_istable(L, -1))
        {
            lua_pushnil(L);
            lua_setfield(L, -2, "loadlib");
        }
        lua_pop(L, 1);

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
        lua_pushcfunction(L, Lua_Player_GetPosition);
        lua_setfield(L, -2, "GetPosition");
        lua_pushcfunction(L, Lua_Player_SetPosition);
        lua_setfield(L, -2, "SetPosition");
        lua_pushcfunction(L, Lua_Player_SetPosition);
        lua_setfield(L, -2, "Teleport");
        lua_pushcfunction(L, Lua_Player_GetVehicle);
        lua_setfield(L, -2, "GetVehicle");
        lua_pushcfunction(L, Lua_Player_IsInVehicle);
        lua_setfield(L, -2, "IsInVehicle");
        lua_pushcfunction(L, Lua_Player_GetHeading);
        lua_setfield(L, -2, "GetHeading");
        lua_pushcfunction(L, Lua_Player_SetHeading);
        lua_setfield(L, -2, "SetHeading");
        lua_pushcfunction(L, Lua_Player_GetWantedLevel);
        lua_setfield(L, -2, "GetWantedLevel");
        lua_pushcfunction(L, Lua_Player_SetWantedLevel);
        lua_setfield(L, -2, "SetWantedLevel");
        lua_pushcfunction(L, Lua_Player_ClearWantedLevel);
        lua_setfield(L, -2, "ClearWantedLevel");
        lua_pushcfunction(L, Lua_Player_GetMoney);
        lua_setfield(L, -2, "GetMoney");
        lua_pushcfunction(L, Lua_Player_SetMoney);
        lua_setfield(L, -2, "SetMoney");
        lua_pushcfunction(L, Lua_Player_GiveMoney);
        lua_setfield(L, -2, "GiveMoney");
        lua_setglobal(L, "Player");

        // Table: Vehicle
        lua_newtable(L);
        lua_pushcfunction(L, Lua_Vehicle_GetPlayerVehicle);
        lua_setfield(L, -2, "GetPlayerVehicle");
        lua_pushcfunction(L, Lua_Vehicle_GetPosition);
        lua_setfield(L, -2, "GetPosition");
        lua_pushcfunction(L, Lua_Vehicle_SetPosition);
        lua_setfield(L, -2, "SetPosition");
        lua_pushcfunction(L, Lua_Vehicle_SetPosition);
        lua_setfield(L, -2, "Teleport");
        lua_pushcfunction(L, Lua_Vehicle_GetVelocity);
        lua_setfield(L, -2, "GetVelocity");
        lua_pushcfunction(L, Lua_Vehicle_SetVelocity);
        lua_setfield(L, -2, "SetVelocity");
        lua_pushcfunction(L, Lua_Vehicle_GetSpeed);
        lua_setfield(L, -2, "GetSpeed");
        lua_pushcfunction(L, Lua_Vehicle_SetSpeed);
        lua_setfield(L, -2, "SetSpeed");
        lua_pushcfunction(L, Lua_Vehicle_Repair);
        lua_setfield(L, -2, "Repair");
        lua_pushcfunction(L, Lua_Vehicle_GetHealth);
        lua_setfield(L, -2, "GetHealth");
        lua_pushcfunction(L, Lua_Vehicle_SetHealth);
        lua_setfield(L, -2, "SetHealth");
        lua_pushcfunction(L, Lua_Vehicle_BlowUp);
        lua_setfield(L, -2, "BlowUp");
        lua_pushcfunction(L, Lua_Vehicle_BlowUp);
        lua_setfield(L, -2, "Explode");
        lua_pushcfunction(L, Lua_Vehicle_SetDoorLock);
        lua_setfield(L, -2, "SetDoorLock");
        lua_pushcfunction(L, Lua_Vehicle_SetLocked);
        lua_setfield(L, -2, "SetLocked");
        lua_pushcfunction(L, Lua_Vehicle_IsLocked);
        lua_setfield(L, -2, "IsLocked");
        lua_pushcfunction(L, Lua_Vehicle_GetHeading);
        lua_setfield(L, -2, "GetHeading");
        lua_pushcfunction(L, Lua_Vehicle_SetHeading);
        lua_setfield(L, -2, "SetHeading");

        // Vehicle Extended
        lua_pushcfunction(L, Lua_Vehicle_Create);
        lua_setfield(L, -2, "Create");
        lua_pushcfunction(L, Lua_Vehicle_Create);
        lua_setfield(L, -2, "Spawn");
        lua_pushcfunction(L, Lua_Vehicle_SetColor);
        lua_setfield(L, -2, "SetColor");
        lua_pushcfunction(L, Lua_Vehicle_SetEngineState);
        lua_setfield(L, -2, "SetEngineState");
        lua_pushcfunction(L, Lua_Vehicle_PopTyre);
        lua_setfield(L, -2, "PopTyre");
        lua_pushcfunction(L, Lua_Vehicle_GetHandle);
        lua_setfield(L, -2, "GetHandle");
        lua_pushcfunction(L, Lua_Vehicle_GetPointer);
        lua_setfield(L, -2, "GetPointer");
        lua_setglobal(L, "Vehicle");

        // Table: Explosion
        lua_newtable(L);
        lua_pushcfunction(L, Lua_Explosion_Create);
        lua_setfield(L, -2, "Create");
        lua_pushcfunction(L, Lua_Explosion_CreateAtPlayer);
        lua_setfield(L, -2, "CreateAtPlayer");
        lua_pushcfunction(L, Lua_Explosion_CreateAtVehicle);
        lua_setfield(L, -2, "CreateAtVehicle");
        lua_setglobal(L, "Explosion");

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

        // Timers in Game
        lua_pushcfunction(L, Lua_Game_SetInterval);
        lua_setfield(L, -2, "SetInterval");
        lua_pushcfunction(L, Lua_Game_SetInterval);
        lua_setfield(L, -2, "Every");
        lua_pushcfunction(L, Lua_Game_SetTimeout);
        lua_setfield(L, -2, "SetTimeout");
        lua_pushcfunction(L, Lua_Game_SetTimeout);
        lua_setfield(L, -2, "After");
        lua_pushcfunction(L, Lua_Game_ClearTimer);
        lua_setfield(L, -2, "ClearTimer");
        lua_pushcfunction(L, Lua_Game_ClearTimer);
        lua_setfield(L, -2, "ClearInterval");
        lua_pushcfunction(L, Lua_Game_ClearTimer);
        lua_setfield(L, -2, "ClearTimeout");

        // Script management in Game
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

        // World & Teleportation
        lua_pushcfunction(L, Lua_Game_Teleport);
        lua_setfield(L, -2, "Teleport");
        lua_pushcfunction(L, Lua_Game_GetGroundZ);
        lua_setfield(L, -2, "GetGroundZ");

        // Clock & Weather & TimeScale
        lua_pushcfunction(L, Lua_Game_SetTime);
        lua_setfield(L, -2, "SetTime");
        lua_pushcfunction(L, Lua_Game_SetTime);
        lua_setfield(L, -2, "SetClock");
        lua_pushcfunction(L, Lua_Game_GetTime);
        lua_setfield(L, -2, "GetTime");
        lua_pushcfunction(L, Lua_Game_GetTime);
        lua_setfield(L, -2, "GetClock");
        lua_pushcfunction(L, Lua_Game_SetWeather);
        lua_setfield(L, -2, "SetWeather");
        lua_pushcfunction(L, Lua_Game_ReleaseWeather);
        lua_setfield(L, -2, "ReleaseWeather");
        lua_pushcfunction(L, Lua_Game_SetGameSpeed);
        lua_setfield(L, -2, "SetGameSpeed");
        lua_pushcfunction(L, Lua_Game_SetGameSpeed);
        lua_setfield(L, -2, "SetTimeScale");
        lua_pushcfunction(L, Lua_Game_GetGameSpeed);
        lua_setfield(L, -2, "GetGameSpeed");
        lua_pushcfunction(L, Lua_Game_GetGameSpeed);
        lua_setfield(L, -2, "GetTimeScale");
        lua_pushcfunction(L, Lua_Game_GetFPS);
        lua_setfield(L, -2, "GetFPS");

        // Explosion alias in Game
        lua_pushcfunction(L, Lua_Explosion_Create);
        lua_setfield(L, -2, "CreateExplosion");
        lua_pushcfunction(L, Lua_Explosion_Create);
        lua_setfield(L, -2, "Explode");
        lua_setglobal(L, "Game");

        // Table: Timer
        lua_newtable(L);
        lua_pushcfunction(L, Lua_Game_SetInterval);
        lua_setfield(L, -2, "SetInterval");
        lua_pushcfunction(L, Lua_Game_SetInterval);
        lua_setfield(L, -2, "Every");
        lua_pushcfunction(L, Lua_Game_SetTimeout);
        lua_setfield(L, -2, "SetTimeout");
        lua_pushcfunction(L, Lua_Game_SetTimeout);
        lua_setfield(L, -2, "After");
        lua_pushcfunction(L, Lua_Game_ClearTimer);
        lua_setfield(L, -2, "Clear");
        lua_pushcfunction(L, Lua_Game_ClearTimer);
        lua_setfield(L, -2, "ClearInterval");
        lua_pushcfunction(L, Lua_Game_ClearTimer);
        lua_setfield(L, -2, "ClearTimeout");
        lua_setglobal(L, "Timer");

        // Table: Weapon
        lua_newtable(L);
        lua_pushcfunction(L, Lua_Weapon_Give);
        lua_setfield(L, -2, "Give");
        lua_pushcfunction(L, Lua_Weapon_SetCurrent);
        lua_setfield(L, -2, "SetCurrent");
        lua_pushcfunction(L, Lua_Weapon_Remove);
        lua_setfield(L, -2, "Remove");
        lua_pushcfunction(L, Lua_Weapon_RemoveAll);
        lua_setfield(L, -2, "RemoveAll");
        RegisterWeaponConstants(L);
        lua_setglobal(L, "Weapon");

        // Table: Device
        lua_newtable(L);
        lua_pushcfunction(L, Lua_Device_Vibrate);
        lua_setfield(L, -2, "Vibrate");
        lua_pushcfunction(L, Lua_Device_CancelVibrate);
        lua_setfield(L, -2, "CancelVibrate");
        lua_pushcfunction(L, Lua_Device_Toast);
        lua_setfield(L, -2, "Toast");
        lua_pushcfunction(L, Lua_Device_GetBatteryLevel);
        lua_setfield(L, -2, "GetBatteryLevel");
        lua_pushcfunction(L, Lua_Device_GetAndroidVersion);
        lua_setfield(L, -2, "GetAndroidVersion");
        lua_pushcfunction(L, Lua_Device_GetDisplaySize);
        lua_setfield(L, -2, "GetDisplaySize");
        lua_setglobal(L, "Device");

        // Table: Audio
        lua_newtable(L);
        lua_pushcfunction(L, Lua_Audio_PlaySound);
        lua_setfield(L, -2, "PlaySound");
        lua_pushcfunction(L, Lua_Audio_SetRadioStation);
        lua_setfield(L, -2, "SetRadioStation");
        lua_pushcfunction(L, Lua_Audio_GetRadioStation);
        lua_setfield(L, -2, "GetRadioStation");
        RegisterAudioConstants(L);
        lua_setglobal(L, "Audio");

        // Table: Screen
        lua_newtable(L);
        lua_pushcfunction(L, Lua_Screen_Fade);
        lua_setfield(L, -2, "Fade");
        lua_pushcfunction(L, Lua_Screen_FadeIn);
        lua_setfield(L, -2, "FadeIn");
        lua_pushcfunction(L, Lua_Screen_FadeOut);
        lua_setfield(L, -2, "FadeOut");
        lua_setglobal(L, "Screen");

        // Table: Camera
        lua_newtable(L);
        lua_pushcfunction(L, Lua_Camera_Shake);
        lua_setfield(L, -2, "Shake");
        lua_pushcfunction(L, Lua_Camera_Restore);
        lua_setfield(L, -2, "Restore");
        lua_setglobal(L, "Camera");

        // Table: File
        lua_newtable(L);
        lua_pushcfunction(L, Lua_File_Read);
        lua_setfield(L, -2, "Read");
        lua_pushcfunction(L, Lua_File_Write);
        lua_setfield(L, -2, "Write");
        lua_pushcfunction(L, Lua_File_Append);
        lua_setfield(L, -2, "Append");
        lua_pushcfunction(L, Lua_File_Exists);
        lua_setfield(L, -2, "Exists");
        lua_pushcfunction(L, Lua_File_Delete);
        lua_setfield(L, -2, "Delete");
        lua_pushcfunction(L, Lua_File_List);
        lua_setfield(L, -2, "List");
        lua_pushcfunction(L, Lua_File_GetScriptsPath);
        lua_setfield(L, -2, "GetScriptsPath");
        lua_pushcfunction(L, Lua_File_GetDataPath);
        lua_setfield(L, -2, "GetDataPath");
        lua_setglobal(L, "File");

        // Register Http & Json Modules
        Http::RegisterLua(L);
        Json::RegisterLua(L);

        // Table: AMLua
        lua_newtable(L);
        lua_pushstring(L, "1.1.0");
        lua_setfield(L, -2, "Version");
        lua_pushstring(L, "1.1.0");
        lua_setfield(L, -2, "VERSION");
        lua_pushcfunction(L, Lua_RegisterTick);
        lua_setfield(L, -2, "OnTick");

        // Timers in AMLua
        lua_pushcfunction(L, Lua_Game_SetInterval);
        lua_setfield(L, -2, "SetInterval");
        lua_pushcfunction(L, Lua_Game_SetInterval);
        lua_setfield(L, -2, "Every");
        lua_pushcfunction(L, Lua_Game_SetTimeout);
        lua_setfield(L, -2, "SetTimeout");
        lua_pushcfunction(L, Lua_Game_SetTimeout);
        lua_setfield(L, -2, "After");
        lua_pushcfunction(L, Lua_Game_ClearTimer);
        lua_setfield(L, -2, "ClearTimer");
        lua_pushcfunction(L, Lua_Game_ClearTimer);
        lua_setfield(L, -2, "ClearInterval");
        lua_pushcfunction(L, Lua_Game_ClearTimer);
        lua_setfield(L, -2, "ClearTimeout");

        // Game methods in AMLua
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
        lua_pushcfunction(L, Lua_Game_Teleport);
        lua_setfield(L, -2, "Teleport");

        // Submodules in AMLua
        lua_getglobal(L, "Player");
        lua_setfield(L, -2, "Player");
        lua_getglobal(L, "Vehicle");
        lua_setfield(L, -2, "Vehicle");
        lua_getglobal(L, "Explosion");
        lua_setfield(L, -2, "Explosion");
        lua_getglobal(L, "Game");
        lua_setfield(L, -2, "Game");
        lua_getglobal(L, "Timer");
        lua_setfield(L, -2, "Timer");
        lua_getglobal(L, "Weapon");
        lua_setfield(L, -2, "Weapon");
        lua_getglobal(L, "Device");
        lua_setfield(L, -2, "Device");
        lua_getglobal(L, "Audio");
        lua_setfield(L, -2, "Audio");
        lua_getglobal(L, "Screen");
        lua_setfield(L, -2, "Screen");
        lua_getglobal(L, "Camera");
        lua_setfield(L, -2, "Camera");
        lua_getglobal(L, "File");
        lua_setfield(L, -2, "File");
        lua_getglobal(L, "Http");
        lua_setfield(L, -2, "Http");
        lua_getglobal(L, "Json");
        lua_setfield(L, -2, "Json");

        lua_setglobal(L, "AMLua");

        // Global dofile override
        lua_pushcfunction(L, Lua_Game_DoFile);
        lua_setglobal(L, "dofile");

        // Global timer aliases
        lua_pushcfunction(L, Lua_Game_SetInterval);
        lua_setglobal(L, "setInterval");
        lua_pushcfunction(L, Lua_Game_SetTimeout);
        lua_setglobal(L, "setTimeout");
        lua_pushcfunction(L, Lua_Game_ClearTimer);
        lua_setglobal(L, "clearInterval");
        lua_pushcfunction(L, Lua_Game_ClearTimer);
        lua_setglobal(L, "clearTimeout");
        lua_pushcfunction(L, Lua_Game_ClearTimer);
        lua_setglobal(L, "clearTimer");
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
        Log("AMLua: Android Mod Lua Script Loader 1.1.0");
        Log("Target: libGTASA.so (base: %p, size: %zu)", (void*)g_pGTASA, (size_t)g_LibGTASASize);
        Log("Log target: %s", g_LogFilePath.c_str());
        Log("Scripts dir: %s", g_ScriptsDirPath.c_str());
        Log("=========================================");

        // Resolve game engine symbols via AML
        if (aml && g_pGTASA)
        {
            // 1. FindPlayerPed
            pfnFindPlayerPed = (FindPlayerPed_t)aml->GetSym(g_pGTASA, "_Z13FindPlayerPedi");
            if (!pfnFindPlayerPed) pfnFindPlayerPed = (FindPlayerPed_t)aml->GetSym(g_pGTASA, "FindPlayerPed");
            Log("Symbol FindPlayerPed: %p", (void*)pfnFindPlayerPed);

            // 2. FindPlayerVehicle
            pfnFindPlayerVehicle = (FindPlayerVehicle_t)aml->GetSym(g_pGTASA, "_Z17FindPlayerVehicleib");
            if (!pfnFindPlayerVehicle) pfnFindPlayerVehicle = (FindPlayerVehicle_t)aml->GetSym(g_pGTASA, "FindPlayerVehicle");
            Log("Symbol FindPlayerVehicle: %p", (void*)pfnFindPlayerVehicle);

            // 3. FindPlayerWanted
            pfnFindPlayerWanted = (FindPlayerWanted_t)aml->GetSym(g_pGTASA, "_Z16FindPlayerWantedi");
            if (!pfnFindPlayerWanted) pfnFindPlayerWanted = (FindPlayerWanted_t)aml->GetSym(g_pGTASA, "FindPlayerWanted");
            Log("Symbol FindPlayerWanted: %p", (void*)pfnFindPlayerWanted);

            // 3b. FindPlayerInfo
            pfnFindPlayerInfo = (FindPlayerInfo_t)aml->GetSym(g_pGTASA, "_Z14FindPlayerInfoi");
            if (!pfnFindPlayerInfo) pfnFindPlayerInfo = (FindPlayerInfo_t)aml->GetSym(g_pGTASA, "FindPlayerInfo");
            Log("Symbol FindPlayerInfo: %p", (void*)pfnFindPlayerInfo);

            // 4. SetWantedLevel
            pfnSetWantedLevel = (SetWantedLevel_t)aml->GetSym(g_pGTASA, "_ZN7CWanted14SetWantedLevelEi");
            pfnSetWantedLevelNoDrop = (SetWantedLevelNoDrop_t)aml->GetSym(g_pGTASA, "_ZN7CWanted20SetWantedLevelNoDropEi");
            pfnGetWantedLevel = (GetWantedLevel_t)aml->GetSym(g_pGTASA, "_ZN7CWanted14GetWantedLevelEv");
            if (!pfnGetWantedLevel) pfnGetWantedLevel = (GetWantedLevel_t)aml->GetSym(g_pGTASA, "_ZNK7CWanted14GetWantedLevelEv");
            Log("Symbol CWanted::SetWantedLevel: %p", (void*)(pfnSetWantedLevel ? (void*)pfnSetWantedLevel : (void*)pfnSetWantedLevelNoDrop));

            // 5. AsciiToGxtChar
            pfnAsciiToGxtChar = (AsciiToGxtChar_t)aml->GetSym(g_pGTASA, "_Z14AsciiToGxtCharPKcPt");
            if (!pfnAsciiToGxtChar) pfnAsciiToGxtChar = (AsciiToGxtChar_t)aml->GetSym(g_pGTASA, "AsciiToGxtChar");
            Log("Symbol AsciiToGxtChar: %p", (void*)pfnAsciiToGxtChar);

            // 6. CHud::SetHelpMessage
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

            // 7. CMessages::AddMessageJumpQ
            pfnAddMessageJumpQ = (AddMessageJumpQ_t)aml->GetSym(g_pGTASA, "_ZN9CMessages15AddMessageJumpQEPKcPtjtb");
            if (!pfnAddMessageJumpQ) pfnAddMessageJumpQ = (AddMessageJumpQ_t)aml->GetSym(g_pGTASA, "_ZN9CMessages15AddMessageJumpQEPKcjtb");
            Log("Symbol CMessages::AddMessageJumpQ: %p", (void*)pfnAddMessageJumpQ);

            // 8. CVehicle::Fix & BlowUpCar
            pfnVehicleFix = (VehicleFix_t)aml->GetSym(g_pGTASA, "_ZN8CVehicle3FixEv");
            if (!pfnVehicleFix) pfnVehicleFix = (VehicleFix_t)aml->GetSym(g_pGTASA, "_ZN8CVehicle6RepairEv");
            Log("Symbol CVehicle::Fix: %p", (void*)pfnVehicleFix);

            pfnVehicleBlowUp = (VehicleBlowUp_t)aml->GetSym(g_pGTASA, "_ZN8CVehicle9BlowUpCarEP7CEntityh");
            if (!pfnVehicleBlowUp) pfnVehicleBlowUp = (VehicleBlowUp_t)aml->GetSym(g_pGTASA, "_ZN8CVehicle9BlowUpCarEP7CEntity");
            Log("Symbol CVehicle::BlowUpCar: %p", (void*)pfnVehicleBlowUp);

            // 9. CEntity::Teleport
            pfnEntityTeleport = (EntityTeleport_t)aml->GetSym(g_pGTASA, "_ZN7CEntity8TeleportE7CVectorb");
            if (!pfnEntityTeleport) pfnEntityTeleport = (EntityTeleport_t)aml->GetSym(g_pGTASA, "_ZN7CEntity8TeleportE7CVector");
            Log("Symbol CEntity::Teleport: %p", (void*)pfnEntityTeleport);

            // 10. SCM Script Engine & CExplosion for Visual Explosions
            pfnProcessOneCommand = (ProcessOneCommand_t)aml->GetSym(g_pGTASA, "_ZN14CRunningScript17ProcessOneCommandEv");
            pfnProcessCommands200To299 = (ProcessCommands_t)aml->GetSym(g_pGTASA, "_ZN14CRunningScript23ProcessCommands200To299Ei");
            pfnProcessCommands500To599 = (ProcessCommands_t)aml->GetSym(g_pGTASA, "_ZN14CRunningScript23ProcessCommands500To599Ei");
            pfnProcessCommands900To999 = (ProcessCommands_t)aml->GetSym(g_pGTASA, "_ZN14CRunningScript23ProcessCommands900To999Ei");
            pfnScriptInit = (ScriptInit_t)aml->GetSym(g_pGTASA, "_ZN14CRunningScript4InitEv");
            Log("Symbol CRunningScript::ProcessOneCommand: %p", (void*)pfnProcessOneCommand);
            Log("Symbol CRunningScript::ProcessCommands200To299: %p", (void*)pfnProcessCommands200To299);

            // CExplosion::AddExplosion (By-Value CVector: float x, float y, float z)
            pfnAddExplosionByVal = (AddExplosionByVal_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_14eExplosionType7CVectorbbf");
            if (!pfnAddExplosionByVal) pfnAddExplosionByVal = (AddExplosionByVal_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_i7CVectorbbf");
            if (!pfnAddExplosionByVal) pfnAddExplosionByVal = (AddExplosionByVal_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_14eExplosionType7CVectorjbfb");
            if (!pfnAddExplosionByVal) pfnAddExplosionByVal = (AddExplosionByVal_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_14eExplosionType7CVectorjbf");
            if (!pfnAddExplosionByVal) pfnAddExplosionByVal = (AddExplosionByVal_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_14eExplosionType7CVectorjb");
            if (!pfnAddExplosionByVal) pfnAddExplosionByVal = (AddExplosionByVal_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_14eExplosionType7CVectorj");
            if (!pfnAddExplosionByVal) pfnAddExplosionByVal = (AddExplosionByVal_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_i7CVectorjbfb");
            if (!pfnAddExplosionByVal) pfnAddExplosionByVal = (AddExplosionByVal_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_i7CVectorjbf");
            if (!pfnAddExplosionByVal) pfnAddExplosionByVal = (AddExplosionByVal_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_i7CVectorjb");
            if (!pfnAddExplosionByVal) pfnAddExplosionByVal = (AddExplosionByVal_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_i7CVectorj");
            #ifdef AML32
            if (!pfnAddExplosionByVal) pfnAddExplosionByVal = (AddExplosionByVal_t)(g_pGTASA + 0x5A70D0);
            if (!pfnAddSoundlessExp)   pfnAddSoundlessExp   = (AddSoundlessExplosion_t)(g_pGTASA + 0x5A7220);
            #endif
            Log("Symbol CExplosion::AddExplosion (ByVal): %p", (void*)pfnAddExplosionByVal);

            // CExplosion::AddExplosion (By-Ref const CVector&)
            pfnAddExplosionByRef = (AddExplosionByRef_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_14eExplosionTypeRK7CVectorjbfb");
            if (!pfnAddExplosionByRef) pfnAddExplosionByRef = (AddExplosionByRef_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_14eExplosionTypeRK7CVectorjbf");
            if (!pfnAddExplosionByRef) pfnAddExplosionByRef = (AddExplosionByRef_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_14eExplosionTypeRK7CVectorjb");
            if (!pfnAddExplosionByRef) pfnAddExplosionByRef = (AddExplosionByRef_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_iRK7CVectorjbfb");
            if (!pfnAddExplosionByRef) pfnAddExplosionByRef = (AddExplosionByRef_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_iRK7CVectorjbf");
            if (!pfnAddExplosionByRef) pfnAddExplosionByRef = (AddExplosionByRef_t)aml->GetSym(g_pGTASA, "_ZN10CExplosion12AddExplosionEP7CEntityS1_iRK7CVectorjb");
            Log("Symbol CExplosion::AddExplosion (ByRef): %p", (void*)pfnAddExplosionByRef);

            pfnTriggerExplosion = (TriggerExplosion_t)aml->GetSym(g_pGTASA, "_ZN6CWorld16TriggerExplosionERK7CVectorffP7CEntityS4_bf");
            Log("Symbol CWorld::TriggerExplosion: %p", (void*)pfnTriggerExplosion);

            // 11. FindGroundZ
            pfnFindGroundZForCoord = (FindGroundZForCoord_t)aml->GetSym(g_pGTASA, "_ZN6CWorld19FindGroundZForCoordEff");
            pfnFindGroundZFor3DCoord = (FindGroundZFor3DCoord_t)aml->GetSym(g_pGTASA, "_ZN6CWorld21FindGroundZFor3DCoordEfffPbPP7CEntity");
            Log("Symbol FindGroundZForCoord: %p", (void*)pfnFindGroundZForCoord);

            // 12. Clock
            pfnSetGameClock = (SetGameClock_t)aml->GetSym(g_pGTASA, "_ZN6CClock12SetGameClockEhhh");
            pClockHours = (unsigned char*)aml->GetSym(g_pGTASA, "_ZN6CClock18ms_nGameClockHoursE");
            if (!pClockHours) pClockHours = (unsigned char*)aml->GetSym(g_pGTASA, "_ZN6CClock16ms_nGameClockHoursE");
            pClockMinutes = (unsigned char*)aml->GetSym(g_pGTASA, "_ZN6CClock20ms_nGameClockMinutesE");
            if (!pClockMinutes) pClockMinutes = (unsigned char*)aml->GetSym(g_pGTASA, "_ZN6CClock18ms_nGameClockMinutesE");
            #ifdef AML32
            if (!pClockHours) pClockHours = (unsigned char*)(g_pGTASA + 0x679B40);
            if (!pClockMinutes) pClockMinutes = (unsigned char*)(g_pGTASA + 0x676270);
            #else
            if (!pClockHours) pClockHours = (unsigned char*)(g_pGTASA + 0x8516A0);
            if (!pClockMinutes) pClockMinutes = (unsigned char*)(g_pGTASA + 0x84A540);
            #endif
            Log("Symbol CClock::SetGameClock: %p", (void*)pfnSetGameClock);

            // 13. Weather
            pfnForceWeatherNow = (ForceWeatherNow_t)aml->GetSym(g_pGTASA, "_ZN8CWeather15ForceWeatherNowEs");
            pfnForceWeather = (ForceWeather_t)aml->GetSym(g_pGTASA, "_ZN8CWeather12ForceWeatherEs");
            pfnReleaseWeather = (ReleaseWeather_t)aml->GetSym(g_pGTASA, "_ZN8CWeather14ReleaseWeatherEv");
            Log("Symbol CWeather::ForceWeatherNow: %p", (void*)pfnForceWeatherNow);

            // 14. Timer (TimeScale / FPS)
            pTimeScale = (float*)aml->GetSym(g_pGTASA, "_ZN6CTimer13ms_fTimeScaleE");
            pGameFPS = (float*)aml->GetSym(g_pGTASA, "_ZN6CTimer8game_FPSE");
            #ifdef AML32
            if (!pTimeScale) pTimeScale = (float*)(g_pGTASA + 0x6768A8);
            if (!pGameFPS) pGameFPS = (float*)(g_pGTASA + 0x67767C);
            #else
            if (!pTimeScale) pTimeScale = (float*)(g_pGTASA + 0x84B1A8);
            if (!pGameFPS) pGameFPS = (float*)(g_pGTASA + 0x84CD28);
            #endif
            Log("Symbol CTimer::ms_fTimeScale: %p", (void*)pTimeScale);

            // 15. CWorld::Players (Money)
            pWorldPlayers = (void*)aml->GetSym(g_pGTASA, "_ZN6CWorld7PlayersE");
            #ifdef AML32
            if (!pWorldPlayers) pWorldPlayers = (void*)(g_pGTASA + 0x6783C8);
            #else
            if (!pWorldPlayers) pWorldPlayers = (void*)(g_pGTASA + 0x84E7A8);
            #endif
            Log("Symbol CWorld::Players: %p", pWorldPlayers);

            // 16. CPools
            pfnGetPedRef = (GetPedRef_t)aml->GetSym(g_pGTASA, "_ZN6CPools10GetPedRefEP4CPed");
            if (!pfnGetPedRef) pfnGetPedRef = (GetPedRef_t)aml->GetSym(g_pGTASA, "GetPedRef");
            pfnGetPed = (GetPed_t)aml->GetSym(g_pGTASA, "_ZN6CPools6GetPedEi");
            if (!pfnGetPed) pfnGetPed = (GetPed_t)aml->GetSym(g_pGTASA, "GetPed");
            pfnGetVehicleRef = (GetVehicleRef_t)aml->GetSym(g_pGTASA, "_ZN6CPools13GetVehicleRefEP8CVehicle");
            if (!pfnGetVehicleRef) pfnGetVehicleRef = (GetVehicleRef_t)aml->GetSym(g_pGTASA, "GetVehicleRef");
            pfnGetVehicle = (GetVehicle_t)aml->GetSym(g_pGTASA, "_ZN6CPools10GetVehicleEi");
            if (!pfnGetVehicle) pfnGetVehicle = (GetVehicle_t)aml->GetSym(g_pGTASA, "GetVehicle");
            Log("Symbol CPools (GetPedRef: %p, GetVehicleRef: %p)", (void*)pfnGetPedRef, (void*)pfnGetVehicleRef);

            // 17. CPed Weapons
            pfnGiveWeapon = (GiveWeapon_t)aml->GetSym(g_pGTASA, "_ZN4CPed10GiveWeaponE11eWeaponTypejb");
            if (!pfnGiveWeapon) pfnGiveWeapon = (GiveWeapon_t)aml->GetSym(g_pGTASA, "_ZN4CPed10GiveWeaponE11eWeaponTypej");
            if (!pfnGiveWeapon) pfnGiveWeapon = (GiveWeapon_t)aml->GetSym(g_pGTASA, "_ZN4CPed10GiveWeaponEijb");
            if (!pfnGiveWeapon) pfnGiveWeapon = (GiveWeapon_t)aml->GetSym(g_pGTASA, "_ZN4CPed10GiveWeaponEij");
            pfnSetCurrentWeapon = (SetCurrentWeapon_t)aml->GetSym(g_pGTASA, "_ZN4CPed16SetCurrentWeaponEi");
            if (!pfnSetCurrentWeapon) pfnSetCurrentWeapon = (SetCurrentWeapon_t)aml->GetSym(g_pGTASA, "_ZN4CPed16SetCurrentWeaponE11eWeaponType");
            pfnClearWeapons = (ClearWeapons_t)aml->GetSym(g_pGTASA, "_ZN4CPed12ClearWeaponsEv");
            pfnRemoveWeapon = (RemoveWeapon_t)aml->GetSym(g_pGTASA, "_ZN4CPed12RemoveWeaponE11eWeaponType");
            if (!pfnRemoveWeapon) pfnRemoveWeapon = (RemoveWeapon_t)aml->GetSym(g_pGTASA, "_ZN4CPed12RemoveWeaponEi");
            Log("Symbol CPed Weapons (GiveWeapon: %p, SetCurrentWeapon: %p)", (void*)pfnGiveWeapon, (void*)pfnSetCurrentWeapon);
        }

        // Initialize Http subsystem
        Http::Init(nullptr, aml ? aml->GetJNIEnvironment() : nullptr);

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
            Http::Shutdown();
            ClearAllTimers(g_LuaState);
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

        std::sort(g_LoadedScripts.begin(), g_LoadedScripts.end());

        Log("Found %zu Lua script(s) to load.", g_LoadedScripts.size());

        for (const auto& fileName : g_LoadedScripts)
        {
            std::string fullPath = std::string(scriptsDir) + "/" + fileName;
            Log("----------------------------------------");
            Log("Loading script: %s", fileName.c_str());

            CrashHandler::SetCurrentScript(fileName.c_str());
            CrashHandler::SetCurrentAction("Script Execution (Startup)");

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

        // Process completed async HTTP responses on main game thread
        Http::ProcessTick(g_LuaState);

        // Calculate delta time in real seconds using high-precision steady_clock
        static auto s_LastTickTime = std::chrono::steady_clock::now();
        static bool s_HasLastTickTime = false;

        auto now = std::chrono::steady_clock::now();
        if (!s_HasLastTickTime)
        {
            s_LastTickTime = now;
            s_HasLastTickTime = true;
        }
        double dt = std::chrono::duration<double>(now - s_LastTickTime).count();
        s_LastTickTime = now;

        // Clamp dt between 0.0001s and 0.5s to prevent huge jumps across pause/loading screens
        if (dt <= 0.0) dt = 0.0166;
        if (dt > 0.5)  dt = 0.0333;

        // Display initial greeting dialog once player is active and world is stabilized (~30 frames)
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

        // 1. Process Seconds-Based Timers (Game.Every, Game.After, SetInterval, SetTimeout)
        if (!g_ActiveTimers.empty())
        {
            struct TriggeredTimer
            {
                int id;
                int luaFuncRef;
                bool isRepeating;
            };
            std::vector<TriggeredTimer> triggered;
            bool hasDeadTimers = false;

            for (auto& t : g_ActiveTimers)
            {
                if (!t.isAlive)
                {
                    hasDeadTimers = true;
                    continue;
                }

                t.remainingSec -= dt;
                if (t.remainingSec <= 0.0)
                {
                    if (t.isRepeating)
                    {
                        t.remainingSec += t.intervalSec;
                        if (t.remainingSec <= 0.0)
                        {
                            t.remainingSec = t.intervalSec;
                        }
                    }
                    else
                    {
                        t.isAlive = false;
                        hasDeadTimers = true;
                    }

                    triggered.push_back({t.id, t.luaFuncRef, t.isRepeating});
                }
            }

            // Execute triggered timers safely (isolated from vector resizing)
            for (const auto& trig : triggered)
            {
                if (trig.luaFuncRef != LUA_NOREF && trig.luaFuncRef != LUA_REFNIL)
                {
                    char timerAction[64];
                    snprintf(timerAction, sizeof(timerAction), "Timer #%d Callback", trig.id);
                    CrashHandler::SetCurrentAction(timerAction);

                    lua_pushcfunction(g_LuaState, Lua_TracebackHandler);
                    int errH = lua_gettop(g_LuaState);

                    lua_rawgeti(g_LuaState, LUA_REGISTRYINDEX, trig.luaFuncRef);
                    if (lua_isfunction(g_LuaState, -1))
                    {
                        lua_pushnumber(g_LuaState, (lua_Number)dt);

                        if (lua_pcall(g_LuaState, 1, 0, errH) != LUA_OK)
                        {
                            const char* err = lua_tostring(g_LuaState, -1);
                            Log("[Timer Error #%d]: %s", trig.id, err ? err : "Runtime error");
                            CrashHandler::LogError("[Timer Error #%d]: %s", trig.id, err ? err : "Runtime error");
                            lua_pop(g_LuaState, 1);
                        }
                    }
                    else
                    {
                        lua_pop(g_LuaState, 1);
                    }

                    lua_pop(g_LuaState, 1); // pop errH
                }

                if (!trig.isRepeating)
                {
                    if (trig.luaFuncRef != LUA_NOREF && trig.luaFuncRef != LUA_REFNIL)
                    {
                        luaL_unref(g_LuaState, LUA_REGISTRYINDEX, trig.luaFuncRef);
                    }
                }
            }

            // Clean up inactive/expired timers
            if (hasDeadTimers)
            {
                g_ActiveTimers.erase(
                    std::remove_if(g_ActiveTimers.begin(), g_ActiveTimers.end(),
                        [](const LuaTimer& t) { return !t.isAlive; }),
                    g_ActiveTimers.end()
                );
            }
        }

        // 2. Process Game.OnTick Frame Callbacks (Passes delta time 'dt' to callback)
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

                    lua_pushnumber(g_LuaState, (lua_Number)dt);

                    if (lua_pcall(g_LuaState, 1, 0, errHandler) != LUA_OK)
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
