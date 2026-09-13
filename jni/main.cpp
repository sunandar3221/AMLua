#include <mod/aml.h>
#include "lua_bindings.h"
#include <string>

// AML Plugin Metadata
MYMOD(net.amlua.loader, AMLua, "1.0", "AuthorName")
NEEDGAME(com.rockstargames.gtasa)

uintptr_t g_pLibGTASA = 0;
static void (*orig_CGame_Process)() = nullptr;

// Game loop hook: calls original engine frame update, then dispatches Lua tick event
static void HookOf_CGame_Process()
{
    if (orig_CGame_Process)
    {
        orig_CGame_Process();
    }

    // Process per-frame tick callbacks in Lua
    AMLua::ProcessTick();
}

extern "C" JNIEXPORT void OnModLoad()
{
    logger->SetTag("AMLua");
    logger->Info("========================================");
    logger->Info("AMLua: Android Mod Lua Script Loader 1.0");
    logger->Info("Initializing plugin for GTA San Andreas");
    logger->Info("========================================");

    // Get libGTASA.so library handle
    g_pLibGTASA = aml->GetLib("libGTASA.so");
    if (!g_pLibGTASA)
    {
        logger->Error("Failed to find libGTASA.so in memory!");
        return;
    }
    logger->Info("Found libGTASA.so at: " PTRFMT, (void*)g_pLibGTASA);

    // Initialize Lua VM and game bindings
    AMLua::Init(g_pLibGTASA);

    // Hook game loop frame process function (CGame::Process)
    uintptr_t symProcess = aml->GetSym(g_pLibGTASA, "_ZN5CGame7ProcessEv");
    if (!symProcess) symProcess = aml->GetSym(g_pLibGTASA, "CGameProcess");
    if (!symProcess) symProcess = aml->GetSym(g_pLibGTASA, "_Z12GameProcessv");

    if (symProcess)
    {
        aml->Hook((void*)symProcess, (void*)HookOf_CGame_Process, (void**)&orig_CGame_Process);
        logger->Info("Successfully hooked CGame::Process at: " PTRFMT, (void*)symProcess);
    }
    else
    {
        logger->Error("Failed to resolve CGame::Process symbol!");
    }

    // Determine scripts directory and load scripts
    std::string scriptsDir = "/storage/emulated/0/Android/data/com.rockstargames.gtasa/files/scripts";
    const char* amlDataPath = aml->GetAndroidDataPath();
    if (amlDataPath && amlDataPath[0] != '\0')
    {
        scriptsDir = std::string(amlDataPath) + "/scripts";
    }

    AMLua::LoadScripts(scriptsDir.c_str());
    logger->Info("AMLua 1.0 initialized and ready.");
}

extern "C" JNIEXPORT void OnModUnload()
{
    logger->Info("AMLua unloading...");
    AMLua::Shutdown();
}
