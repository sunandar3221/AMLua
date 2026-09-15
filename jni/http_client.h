#pragma once

#include <string>
#include <map>
#include <vector>
#include <jni.h>

extern "C" {
#include "lua/lua.h"
#include "lua/lauxlib.h"
}

namespace AMLua
{
    namespace Http
    {
        struct Response
        {
            int statusCode = 0;
            std::string body;
            std::string error;
            std::map<std::string, std::string> headers;
            bool ok = false;
        };

        // Initialize Http subsystem with JavaVM and JNIEnv
        void Init(JavaVM* vm, JNIEnv* env);

        // Process completed async HTTP callbacks on main game thread
        void ProcessTick(lua_State* L);

        // Shutdown and terminate worker threads
        void Shutdown();

        // Perform synchronous HTTP/HTTPS request
        Response RequestSync(const std::string& url,
                            const std::string& method = "GET",
                            const std::string& body = "",
                            const std::map<std::string, std::string>& headers = {},
                            int timeoutMs = 10000);

        // Perform asynchronous HTTP/HTTPS request
        void RequestAsync(const std::string& url,
                          const std::string& method,
                          const std::string& body,
                          const std::map<std::string, std::string>& headers,
                          int timeoutMs,
                          int luaCallbackRef);

        // Synchronous file download
        bool DownloadFileSync(const std::string& url, const std::string& destPath, std::string& errorMsg);

        // Asynchronous file download
        void DownloadFileAsync(const std::string& url, const std::string& destPath, int luaCallbackRef);

        // Register Lua Http API
        void RegisterLua(lua_State* L);
    }
}
