#pragma once

#include <stdint.h>
#include <string>
#include <vector>

// Lua 5.4 C API
extern "C" {
#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"
}

namespace AMLua
{
    // Initialize Lua VM, resolve game symbols, register APIs, and sandbox
    void Init(uintptr_t libGTASA);

    // Shutdown Lua VM and release resources
    void Shutdown();

    // Scan and load all .lua files from specified directory
    void LoadScripts(const char* scriptsDir);

    // Execute registered tick/frame callbacks
    void ProcessTick();

    // Log message to Android logcat and amlua.log
    void Log(const char* fmt, ...);

    // Return current log file path
    const char* GetLogFilePath();

    // Display text in GTA SA native top-right dialog box (CHud::SetHelpMessage)
    void DisplayHelpBox(const char* text, unsigned int duration = 4000);

    // Show native dialog listing all loaded Lua mods
    void ShowScriptListDialog();

    // Get list of loaded script filenames
    const std::vector<std::string>& GetLoadedScripts();

    // Handle touch input for gestures (e.g. double-tap top of screen to view mod list)
    void OnTouchEvent(int actionType, int trackNum, int x, int y);
}
