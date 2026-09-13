#ifndef AMLUA_CRASH_HANDLER_H
#define AMLUA_CRASH_HANDLER_H

#include <string>

namespace AMLua
{
    namespace CrashHandler
    {
        // Initialize signal handlers and alternate signal stack
        void Install(const char* crashLogDir);

        // Remove signal handlers (on unload)
        void Uninstall();

        // Update current script/execution context for diagnostics
        void SetCurrentScript(const char* scriptName);
        void SetCurrentAction(const char* action);
        void ClearContext();

        // Dedicated error logging for Lua and AMLua runtime errors
        void LogError(const char* fmt, ...);

        // Return current crash log file path
        const char* GetCrashLogPath();

        // Return current error log file path
        const char* GetErrorLogPath();
    }
}

#endif // AMLUA_CRASH_HANDLER_H
