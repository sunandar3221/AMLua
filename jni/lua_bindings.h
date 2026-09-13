#pragma once

#include <stdint.h>
#include <string>

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
}
