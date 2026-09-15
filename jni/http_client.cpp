#include "http_client.h"
#include "crash_handler.h"
#include <mod/aml.h>

#include <android/log.h>
#include <thread>
#include <mutex>
#include <queue>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstring>
#include <algorithm>

#define HTTP_LOG_TAG "AMLua_Http"
#define AMLUA_HTTP_LOG(fmt, ...) __android_log_print(ANDROID_LOG_INFO, HTTP_LOG_TAG, fmt, ##__VA_ARGS__)

namespace AMLua
{
    namespace Http
    {
        static JavaVM* g_JavaVM = nullptr;
        static std::mutex g_QueueMutex;

        struct CompletedTask
        {
            int callbackRef = LUA_NOREF;
            bool isDownload = false;
            Response response;
            bool downloadSuccess = false;
            std::string downloadError;
        };

        static std::vector<CompletedTask> g_CompletedTasks;

        void Init(JavaVM* vm, JNIEnv* env)
        {
            if (vm)
            {
                g_JavaVM = vm;
            }
            else if (env)
            {
                env->GetJavaVM(&g_JavaVM);
            }

            if (g_JavaVM)
            {
                AMLUA_HTTP_LOG("Http client initialized with JavaVM: %p", g_JavaVM);
            }
            else
            {
                AMLUA_HTTP_LOG("Warning: JavaVM not available at Init. Will retry on demand.");
            }
        }

        void Shutdown()
        {
            std::lock_guard<std::mutex> lock(g_QueueMutex);
            g_CompletedTasks.clear();
        }

        // Helper to obtain JNIEnv for the calling thread
        struct JNIThreadEnv
        {
            JNIEnv* env = nullptr;
            bool attached = false;

            JNIThreadEnv()
            {
                if (!g_JavaVM)
                {
                    if (aml)
                    {
                        JNIEnv* mainEnv = aml->GetJNIEnvironment();
                        if (mainEnv)
                        {
                            mainEnv->GetJavaVM(&g_JavaVM);
                        }
                    }
                }

                if (!g_JavaVM) return;

                jint res = g_JavaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
                if (res == JNI_EDETACHED)
                {
                    if (g_JavaVM->AttachCurrentThread(&env, nullptr) == JNI_OK)
                    {
                        attached = true;
                    }
                }
            }

            ~JNIThreadEnv()
            {
                if (attached && g_JavaVM)
                {
                    g_JavaVM->DetachCurrentThread();
                }
            }

            bool isValid() const { return env != nullptr; }
        };

        // Helper to extract exception message safely
        static std::string GetExceptionMessage(JNIEnv* env, jthrowable ex)
        {
            if (!ex) return "Unknown Java Exception";

            jclass exClass = env->GetObjectClass(ex);
            jmethodID getMsgMethod = env->GetMethodID(exClass, "getMessage", "()Ljava/lang/String;");
            jstring jMsg = (jstring)env->CallObjectMethod(ex, getMsgMethod);

            std::string result;
            if (jMsg)
            {
                const char* utf = env->GetStringUTFChars(jMsg, nullptr);
                if (utf)
                {
                    result = utf;
                    env->ReleaseStringUTFChars(jMsg, utf);
                }
                env->DeleteLocalRef(jMsg);
            }

            if (result.empty())
            {
                jmethodID toStrMethod = env->GetMethodID(exClass, "toString", "()Ljava/lang/String;");
                jstring jToStr = (jstring)env->CallObjectMethod(ex, toStrMethod);
                if (jToStr)
                {
                    const char* utf = env->GetStringUTFChars(jToStr, nullptr);
                    if (utf)
                    {
                        result = utf;
                        env->ReleaseStringUTFChars(jToStr, utf);
                    }
                    env->DeleteLocalRef(jToStr);
                }
            }

            env->DeleteLocalRef(exClass);
            return result.empty() ? "Java Exception (No message)" : result;
        }

        // Perform actual HTTP / HTTPS request via JNI HttpURLConnection
        Response RequestSync(const std::string& url,
                            const std::string& method,
                            const std::string& body,
                            const std::map<std::string, std::string>& headers,
                            int timeoutMs)
        {
            Response resp;
            resp.ok = false;
            resp.statusCode = 0;

            JNIThreadEnv jni;
            if (!jni.isValid())
            {
                resp.error = "JNI Environment not available";
                return resp;
            }

            JNIEnv* env = jni.env;

            // 1. URL url = new URL(urlStr);
            jclass urlClass = env->FindClass("java/net/URL");
            if (!urlClass)
            {
                env->ExceptionClear();
                resp.error = "Failed to find java.net.URL class";
                return resp;
            }

            jmethodID urlInit = env->GetMethodID(urlClass, "<init>", "(Ljava/lang/String;)V");
            jstring jUrl = env->NewStringUTF(url.c_str());
            jobject urlObj = env->NewObject(urlClass, urlInit, jUrl);
            env->DeleteLocalRef(jUrl);

            if (env->ExceptionCheck())
            {
                jthrowable ex = env->ExceptionOccurred();
                env->ExceptionClear();
                resp.error = GetExceptionMessage(env, ex);
                env->DeleteLocalRef(ex);
                env->DeleteLocalRef(urlClass);
                return resp;
            }

            // 2. HttpURLConnection conn = (HttpURLConnection) url.openConnection();
            jmethodID openConn = env->GetMethodID(urlClass, "openConnection", "()Ljava/net/URLConnection;");
            jobject connObj = env->CallObjectMethod(urlObj, openConn);
            env->DeleteLocalRef(urlObj);
            env->DeleteLocalRef(urlClass);

            if (env->ExceptionCheck())
            {
                jthrowable ex = env->ExceptionOccurred();
                env->ExceptionClear();
                resp.error = GetExceptionMessage(env, ex);
                env->DeleteLocalRef(ex);
                return resp;
            }

            jclass httpConnClass = env->FindClass("java/net/HttpURLConnection");
            if (!httpConnClass)
            {
                env->ExceptionClear();
                resp.error = "Failed to find java.net.HttpURLConnection";
                env->DeleteLocalRef(connObj);
                return resp;
            }

            // Set Request Method
            jmethodID setMethod = env->GetMethodID(httpConnClass, "setRequestMethod", "(Ljava/lang/String;)V");
            std::string upperMethod = method;
            for (auto& c : upperMethod) c = toupper(c);
            jstring jMethod = env->NewStringUTF(upperMethod.c_str());
            env->CallVoidMethod(connObj, setMethod, jMethod);
            env->DeleteLocalRef(jMethod);

            // Set Timeouts
            jmethodID setConnectTimeout = env->GetMethodID(httpConnClass, "setConnectTimeout", "(I)V");
            env->CallVoidMethod(connObj, setConnectTimeout, (jint)(timeoutMs > 0 ? timeoutMs : 10000));

            jmethodID setReadTimeout = env->GetMethodID(httpConnClass, "setReadTimeout", "(I)V");
            env->CallVoidMethod(connObj, setReadTimeout, (jint)(timeoutMs > 0 ? timeoutMs : 10000));

            // Set follow redirects
            jmethodID setFollowRedirects = env->GetMethodID(httpConnClass, "setInstanceFollowRedirects", "(Z)V");
            env->CallVoidMethod(connObj, setFollowRedirects, JNI_TRUE);

            // Set headers
            jmethodID setRequestProperty = env->GetMethodID(httpConnClass, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
            bool hasUserAgent = false;
            for (const auto& kv : headers)
            {
                std::string k = kv.first;
                for (auto& c : k) c = tolower(c);
                if (k == "user-agent") hasUserAgent = true;

                jstring jKey = env->NewStringUTF(kv.first.c_str());
                jstring jVal = env->NewStringUTF(kv.second.c_str());
                env->CallVoidMethod(connObj, setRequestProperty, jKey, jVal);
                env->DeleteLocalRef(jKey);
                env->DeleteLocalRef(jVal);
            }

            if (!hasUserAgent)
            {
                jstring jKey = env->NewStringUTF("User-Agent");
                jstring jVal = env->NewStringUTF("AMLua/1.1.0 (Android; GTA:SA)");
                env->CallVoidMethod(connObj, setRequestProperty, jKey, jVal);
                env->DeleteLocalRef(jKey);
                env->DeleteLocalRef(jVal);
            }

            // Write Body if required
            bool hasBody = !body.empty() || upperMethod == "POST" || upperMethod == "PUT" || upperMethod == "PATCH";
            if (hasBody)
            {
                jmethodID setDoOutput = env->GetMethodID(httpConnClass, "setDoOutput", "(Z)V");
                env->CallVoidMethod(connObj, setDoOutput, JNI_TRUE);

                if (!body.empty())
                {
                    jmethodID getOutputStream = env->GetMethodID(httpConnClass, "getOutputStream", "()Ljava/io/OutputStream;");
                    jobject outStream = env->CallObjectMethod(connObj, getOutputStream);

                    if (env->ExceptionCheck())
                    {
                        jthrowable ex = env->ExceptionOccurred();
                        env->ExceptionClear();
                        resp.error = GetExceptionMessage(env, ex);
                        env->DeleteLocalRef(ex);
                        env->DeleteLocalRef(connObj);
                        env->DeleteLocalRef(httpConnClass);
                        return resp;
                    }

                    if (outStream)
                    {
                        jclass outClass = env->FindClass("java/io/OutputStream");
                        jmethodID writeBytes = env->GetMethodID(outClass, "write", "([B)V");
                        jmethodID closeOut = env->GetMethodID(outClass, "close", "()V");

                        jbyteArray jBytes = env->NewByteArray(body.size());
                        env->SetByteArrayRegion(jBytes, 0, body.size(), (const jbyte*)body.data());
                        env->CallVoidMethod(outStream, writeBytes, jBytes);
                        env->DeleteLocalRef(jBytes);

                        env->CallVoidMethod(outStream, closeOut);
                        env->DeleteLocalRef(outStream);
                        env->DeleteLocalRef(outClass);

                        if (env->ExceptionCheck())
                        {
                            jthrowable ex = env->ExceptionOccurred();
                            env->ExceptionClear();
                            resp.error = GetExceptionMessage(env, ex);
                            env->DeleteLocalRef(ex);
                            env->DeleteLocalRef(connObj);
                            env->DeleteLocalRef(httpConnClass);
                            return resp;
                        }
                    }
                }
            }

            // Connect & Get Response Code
            jmethodID getResponseCode = env->GetMethodID(httpConnClass, "getResponseCode", "()I");
            jint statusCode = env->CallIntMethod(connObj, getResponseCode);

            if (env->ExceptionCheck())
            {
                jthrowable ex = env->ExceptionOccurred();
                env->ExceptionClear();
                resp.error = GetExceptionMessage(env, ex);
                env->DeleteLocalRef(ex);
                env->DeleteLocalRef(connObj);
                env->DeleteLocalRef(httpConnClass);
                return resp;
            }

            resp.statusCode = (int)statusCode;
            resp.ok = (resp.statusCode >= 200 && resp.statusCode < 400);

            // Read Response Body
            jobject inStream = nullptr;
            if (resp.statusCode >= 400)
            {
                jmethodID getErrorStream = env->GetMethodID(httpConnClass, "getErrorStream", "()Ljava/io/InputStream;");
                inStream = env->CallObjectMethod(connObj, getErrorStream);
            }
            else
            {
                jmethodID getInputStream = env->GetMethodID(httpConnClass, "getInputStream", "()Ljava/io/InputStream;");
                inStream = env->CallObjectMethod(connObj, getInputStream);
            }

            if (env->ExceptionCheck())
            {
                jthrowable ex = env->ExceptionOccurred();
                env->ExceptionClear();
                resp.error = GetExceptionMessage(env, ex);
                env->DeleteLocalRef(ex);
            }

            if (inStream)
            {
                jclass inClass = env->FindClass("java/io/InputStream");
                jmethodID readMethod = env->GetMethodID(inClass, "read", "([B)I");
                jmethodID closeIn = env->GetMethodID(inClass, "close", "()V");

                jbyteArray buffer = env->NewByteArray(8192);
                while (true)
                {
                    jint readBytes = env->CallIntMethod(inStream, readMethod, buffer);
                    if (env->ExceptionCheck())
                    {
                        env->ExceptionClear();
                        break;
                    }
                    if (readBytes <= 0) break;

                    jbyte* rawBuf = env->GetByteArrayElements(buffer, nullptr);
                    resp.body.append((const char*)rawBuf, readBytes);
                    env->ReleaseByteArrayElements(buffer, rawBuf, JNI_ABORT);
                }

                env->DeleteLocalRef(buffer);
                env->CallVoidMethod(inStream, closeIn);
                env->DeleteLocalRef(inStream);
                env->DeleteLocalRef(inClass);
            }

            // Disconnect
            jmethodID disconnectMethod = env->GetMethodID(httpConnClass, "disconnect", "()V");
            if (disconnectMethod)
            {
                env->CallVoidMethod(connObj, disconnectMethod);
            }

            env->DeleteLocalRef(connObj);
            env->DeleteLocalRef(httpConnClass);

            return resp;
        }

        // Asynchronous Request - worker thread
        void RequestAsync(const std::string& url,
                          const std::string& method,
                          const std::string& body,
                          const std::map<std::string, std::string>& headers,
                          int timeoutMs,
                          int luaCallbackRef)
        {
            std::thread([=]() {
                Response resp = RequestSync(url, method, body, headers, timeoutMs);

                CompletedTask task;
                task.callbackRef = luaCallbackRef;
                task.isDownload = false;
                task.response = std::move(resp);

                std::lock_guard<std::mutex> lock(g_QueueMutex);
                g_CompletedTasks.push_back(std::move(task));
            }).detach();
        }

        // Synchronous File Download via JNI stream directly into file
        bool DownloadFileSync(const std::string& url, const std::string& destPath, std::string& errorMsg)
        {
            // If AML has native DownloadFile, we can try it first
            if (aml)
            {
                if (aml->DownloadFile(url.c_str(), destPath.c_str()))
                {
                    return true;
                }
            }

            JNIThreadEnv jni;
            if (!jni.isValid())
            {
                errorMsg = "JNI Environment not available";
                return false;
            }

            JNIEnv* env = jni.env;

            jclass urlClass = env->FindClass("java/net/URL");
            if (!urlClass)
            {
                env->ExceptionClear();
                errorMsg = "Failed to find java.net.URL class";
                return false;
            }

            jmethodID urlInit = env->GetMethodID(urlClass, "<init>", "(Ljava/lang/String;)V");
            jstring jUrl = env->NewStringUTF(url.c_str());
            jobject urlObj = env->NewObject(urlClass, urlInit, jUrl);
            env->DeleteLocalRef(jUrl);

            if (env->ExceptionCheck())
            {
                jthrowable ex = env->ExceptionOccurred();
                env->ExceptionClear();
                errorMsg = GetExceptionMessage(env, ex);
                env->DeleteLocalRef(ex);
                env->DeleteLocalRef(urlClass);
                return false;
            }

            jmethodID openConn = env->GetMethodID(urlClass, "openConnection", "()Ljava/net/URLConnection;");
            jobject connObj = env->CallObjectMethod(urlObj, openConn);
            env->DeleteLocalRef(urlObj);
            env->DeleteLocalRef(urlClass);

            if (env->ExceptionCheck())
            {
                jthrowable ex = env->ExceptionOccurred();
                env->ExceptionClear();
                errorMsg = GetExceptionMessage(env, ex);
                env->DeleteLocalRef(ex);
                return false;
            }

            jclass httpConnClass = env->FindClass("java/net/HttpURLConnection");
            if (!httpConnClass)
            {
                env->ExceptionClear();
                errorMsg = "Failed to find java.net.HttpURLConnection";
                env->DeleteLocalRef(connObj);
                return false;
            }

            jmethodID setConnectTimeout = env->GetMethodID(httpConnClass, "setConnectTimeout", "(I)V");
            env->CallVoidMethod(connObj, setConnectTimeout, (jint)15000);

            jmethodID setReadTimeout = env->GetMethodID(httpConnClass, "setReadTimeout", "(I)V");
            env->CallVoidMethod(connObj, setReadTimeout, (jint)30000);

            jmethodID setFollowRedirects = env->GetMethodID(httpConnClass, "setInstanceFollowRedirects", "(Z)V");
            env->CallVoidMethod(connObj, setFollowRedirects, JNI_TRUE);

            jmethodID setRequestProperty = env->GetMethodID(httpConnClass, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
            jstring jKey = env->NewStringUTF("User-Agent");
            jstring jVal = env->NewStringUTF("AMLua/1.1.0 (Android; GTA:SA)");
            env->CallVoidMethod(connObj, setRequestProperty, jKey, jVal);
            env->DeleteLocalRef(jKey);
            env->DeleteLocalRef(jVal);

            jmethodID getResponseCode = env->GetMethodID(httpConnClass, "getResponseCode", "()I");
            jint statusCode = env->CallIntMethod(connObj, getResponseCode);

            if (env->ExceptionCheck())
            {
                jthrowable ex = env->ExceptionOccurred();
                env->ExceptionClear();
                errorMsg = GetExceptionMessage(env, ex);
                env->DeleteLocalRef(ex);
                env->DeleteLocalRef(connObj);
                env->DeleteLocalRef(httpConnClass);
                return false;
            }

            if (statusCode < 200 || statusCode >= 400)
            {
                errorMsg = "HTTP error " + std::to_string(statusCode);
                env->DeleteLocalRef(connObj);
                env->DeleteLocalRef(httpConnClass);
                return false;
            }

            jmethodID getInputStream = env->GetMethodID(httpConnClass, "getInputStream", "()Ljava/io/InputStream;");
            jobject inStream = env->CallObjectMethod(connObj, getInputStream);

            if (!inStream || env->ExceptionCheck())
            {
                jthrowable ex = env->ExceptionOccurred();
                env->ExceptionClear();
                errorMsg = GetExceptionMessage(env, ex);
                if (ex) env->DeleteLocalRef(ex);
                env->DeleteLocalRef(connObj);
                env->DeleteLocalRef(httpConnClass);
                return false;
            }

            FILE* outFile = fopen(destPath.c_str(), "wb");
            if (!outFile)
            {
                errorMsg = "Could not open destination file for writing: " + destPath;
                env->DeleteLocalRef(inStream);
                env->DeleteLocalRef(connObj);
                env->DeleteLocalRef(httpConnClass);
                return false;
            }

            jclass inClass = env->FindClass("java/io/InputStream");
            jmethodID readMethod = env->GetMethodID(inClass, "read", "([B)I");
            jmethodID closeIn = env->GetMethodID(inClass, "close", "()V");

            jbyteArray buffer = env->NewByteArray(16384);
            bool success = true;

            while (true)
            {
                jint bytesRead = env->CallIntMethod(inStream, readMethod, buffer);
                if (env->ExceptionCheck())
                {
                    jthrowable ex = env->ExceptionOccurred();
                    env->ExceptionClear();
                    errorMsg = GetExceptionMessage(env, ex);
                    if (ex) env->DeleteLocalRef(ex);
                    success = false;
                    break;
                }

                if (bytesRead <= 0) break;

                jbyte* rawBuf = env->GetByteArrayElements(buffer, nullptr);
                size_t written = fwrite(rawBuf, 1, bytesRead, outFile);
                env->ReleaseByteArrayElements(buffer, rawBuf, JNI_ABORT);

                if (written != (size_t)bytesRead)
                {
                    errorMsg = "Disk write failure while downloading";
                    success = false;
                    break;
                }
            }

            fclose(outFile);
            env->DeleteLocalRef(buffer);
            env->CallVoidMethod(inStream, closeIn);
            env->DeleteLocalRef(inStream);
            env->DeleteLocalRef(inClass);

            jmethodID disconnectMethod = env->GetMethodID(httpConnClass, "disconnect", "()V");
            if (disconnectMethod)
            {
                env->CallVoidMethod(connObj, disconnectMethod);
            }

            env->DeleteLocalRef(connObj);
            env->DeleteLocalRef(httpConnClass);

            return success;
        }

        // Asynchronous File Download - worker thread
        void DownloadFileAsync(const std::string& url, const std::string& destPath, int luaCallbackRef)
        {
            std::thread([=]() {
                std::string err;
                bool ok = DownloadFileSync(url, destPath, err);

                CompletedTask task;
                task.callbackRef = luaCallbackRef;
                task.isDownload = true;
                task.downloadSuccess = ok;
                task.downloadError = err;

                std::lock_guard<std::mutex> lock(g_QueueMutex);
                g_CompletedTasks.push_back(std::move(task));
            }).detach();
        }

        // Process completed async HTTP callbacks on main game thread
        void ProcessTick(lua_State* L)
        {
            if (!L) return;

            std::vector<CompletedTask> tasks;
            {
                std::lock_guard<std::mutex> lock(g_QueueMutex);
                if (g_CompletedTasks.empty()) return;
                tasks.swap(g_CompletedTasks);
            }

            for (const auto& t : tasks)
            {
                if (t.callbackRef == LUA_NOREF || t.callbackRef == LUA_REFNIL) continue;

                lua_rawgeti(L, LUA_REGISTRYINDEX, t.callbackRef);
                luaL_unref(L, LUA_REGISTRYINDEX, t.callbackRef);

                if (lua_isfunction(L, -1))
                {
                    if (t.isDownload)
                    {
                        // Callback for download: callback(success, errorMsg)
                        lua_pushboolean(L, t.downloadSuccess);
                        if (!t.downloadSuccess)
                        {
                            lua_pushstring(L, t.downloadError.c_str());
                        }
                        else
                        {
                            lua_pushnil(L);
                        }

                        if (lua_pcall(L, 2, 0, 0) != LUA_OK)
                        {
                            const char* err = lua_tostring(L, -1);
                            AMLUA_HTTP_LOG("Error in Http.Download callback: %s", err ? err : "unknown");
                            lua_pop(L, 1);
                        }
                    }
                    else
                    {
                        // Callback for request: callback(responseTable)
                        // responseTable = { ok = bool, status = int, body = string, error = string, headers = table }
                        lua_newtable(L);

                        lua_pushboolean(L, t.response.ok);
                        lua_setfield(L, -2, "ok");

                        lua_pushinteger(L, t.response.statusCode);
                        lua_setfield(L, -2, "status");

                        lua_pushlstring(L, t.response.body.data(), t.response.body.size());
                        lua_setfield(L, -2, "body");

                        if (!t.response.error.empty())
                        {
                            lua_pushstring(L, t.response.error.c_str());
                            lua_setfield(L, -2, "error");
                        }
                        else
                        {
                            lua_pushnil(L);
                            lua_setfield(L, -2, "error");
                        }

                        lua_newtable(L);
                        for (const auto& kv : t.response.headers)
                        {
                            lua_pushstring(L, kv.second.c_str());
                            lua_setfield(L, -2, kv.first.c_str());
                        }
                        lua_setfield(L, -2, "headers");

                        if (lua_pcall(L, 1, 0, 0) != LUA_OK)
                        {
                            const char* err = lua_tostring(L, -1);
                            AMLUA_HTTP_LOG("Error in Http request callback: %s", err ? err : "unknown");
                            lua_pop(L, 1);
                        }
                    }
                }
                else
                {
                    lua_pop(L, 1);
                }
            }
        }

        // ==========================================
        // Lua C Function Wrappers
        // ==========================================

        // Helper to parse Lua table options into C++ parameters
        static void ParseOptionsTable(lua_State* L, int idx,
                                     std::string& method,
                                     std::string& body,
                                     std::map<std::string, std::string>& headers,
                                     int& timeoutMs)
        {
            if (!lua_istable(L, idx)) return;

            // method
            lua_getfield(L, idx, "method");
            if (lua_isstring(L, -1)) method = lua_tostring(L, -1);
            lua_pop(L, 1);

            // body
            lua_getfield(L, idx, "body");
            if (lua_isstring(L, -1))
            {
                size_t len = 0;
                const char* b = lua_tolstring(L, -1, &len);
                body.assign(b, len);
            }
            lua_pop(L, 1);

            // timeout (ms)
            lua_getfield(L, idx, "timeout");
            if (lua_isnumber(L, -1)) timeoutMs = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);

            // headers
            lua_getfield(L, idx, "headers");
            if (lua_istable(L, -1))
            {
                lua_pushnil(L);
                while (lua_next(L, -2) != 0)
                {
                    if (lua_isstring(L, -2) && lua_isstring(L, -1))
                    {
                        headers[lua_tostring(L, -2)] = lua_tostring(L, -1);
                    }
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 1);
        }

        // Push Response struct as Lua table
        static void PushResponseTable(lua_State* L, const Response& resp)
        {
            lua_newtable(L);

            lua_pushboolean(L, resp.ok);
            lua_setfield(L, -2, "ok");

            lua_pushinteger(L, resp.statusCode);
            lua_setfield(L, -2, "status");

            lua_pushlstring(L, resp.body.data(), resp.body.size());
            lua_setfield(L, -2, "body");

            if (!resp.error.empty())
            {
                lua_pushstring(L, resp.error.c_str());
                lua_setfield(L, -2, "error");
            }
            else
            {
                lua_pushnil(L);
                lua_setfield(L, -2, "error");
            }

            lua_newtable(L);
            for (const auto& kv : resp.headers)
            {
                lua_pushstring(L, kv.second.c_str());
                lua_setfield(L, -2, kv.first.c_str());
            }
            lua_setfield(L, -2, "headers");
        }

        // Http.Get(url, [headers], callback)
        static int Lua_Http_Get(lua_State* L)
        {
            const char* url = luaL_checkstring(L, 1);
            std::map<std::string, std::string> headers;
            int cbIdx = 2;

            if (lua_istable(L, 2))
            {
                lua_pushnil(L);
                while (lua_next(L, 2) != 0)
                {
                    if (lua_isstring(L, -2) && lua_isstring(L, -1))
                    {
                        headers[lua_tostring(L, -2)] = lua_tostring(L, -1);
                    }
                    lua_pop(L, 1);
                }
                cbIdx = 3;
            }

            if (!lua_isfunction(L, cbIdx))
            {
                return luaL_error(L, "Http.Get: callback function is required at argument %d", cbIdx);
            }

            lua_pushvalue(L, cbIdx);
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);

            RequestAsync(url, "GET", "", headers, 10000, ref);
            return 0;
        }

        // Http.Post(url, body, [headers], callback)
        static int Lua_Http_Post(lua_State* L)
        {
            const char* url = luaL_checkstring(L, 1);
            size_t bodyLen = 0;
            const char* bodyStr = luaL_checklstring(L, 2, &bodyLen);
            std::string body(bodyStr, bodyLen);
            std::map<std::string, std::string> headers;
            int cbIdx = 3;

            if (lua_istable(L, 3))
            {
                lua_pushnil(L);
                while (lua_next(L, 3) != 0)
                {
                    if (lua_isstring(L, -2) && lua_isstring(L, -1))
                    {
                        headers[lua_tostring(L, -2)] = lua_tostring(L, -1);
                    }
                    lua_pop(L, 1);
                }
                cbIdx = 4;
            }

            if (!lua_isfunction(L, cbIdx))
            {
                return luaL_error(L, "Http.Post: callback function is required at argument %d", cbIdx);
            }

            lua_pushvalue(L, cbIdx);
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);

            RequestAsync(url, "POST", body, headers, 10000, ref);
            return 0;
        }

        // Http.Request({ url = "...", method = "GET", headers = {}, body = "", timeout = 10000 }, callback)
        static int Lua_Http_Request(lua_State* L)
        {
            luaL_checktype(L, 1, LUA_TTABLE);

            std::string url;
            std::string method = "GET";
            std::string body;
            std::map<std::string, std::string> headers;
            int timeoutMs = 10000;

            lua_getfield(L, 1, "url");
            if (lua_isstring(L, -1)) url = lua_tostring(L, -1);
            lua_pop(L, 1);

            if (url.empty())
            {
                return luaL_error(L, "Http.Request: 'url' field is required in options table");
            }

            ParseOptionsTable(L, 1, method, body, headers, timeoutMs);

            if (!lua_isfunction(L, 2))
            {
                return luaL_error(L, "Http.Request: callback function is required as 2nd argument");
            }

            lua_pushvalue(L, 2);
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);

            RequestAsync(url, method, body, headers, timeoutMs, ref);
            return 0;
        }

        // Http.GetSync(url, [headers], [timeoutMs]) -> Response table
        static int Lua_Http_GetSync(lua_State* L)
        {
            const char* url = luaL_checkstring(L, 1);
            std::map<std::string, std::string> headers;
            int timeoutMs = 10000;

            if (lua_istable(L, 2))
            {
                lua_pushnil(L);
                while (lua_next(L, 2) != 0)
                {
                    if (lua_isstring(L, -2) && lua_isstring(L, -1))
                    {
                        headers[lua_tostring(L, -2)] = lua_tostring(L, -1);
                    }
                    lua_pop(L, 1);
                }
            }

            if (lua_isnumber(L, 3))
            {
                timeoutMs = (int)lua_tointeger(L, 3);
            }

            Response resp = RequestSync(url, "GET", "", headers, timeoutMs);
            PushResponseTable(L, resp);
            return 1;
        }

        // Http.PostSync(url, body, [headers], [timeoutMs]) -> Response table
        static int Lua_Http_PostSync(lua_State* L)
        {
            const char* url = luaL_checkstring(L, 1);
            size_t bodyLen = 0;
            const char* bodyStr = luaL_checklstring(L, 2, &bodyLen);
            std::string body(bodyStr, bodyLen);
            std::map<std::string, std::string> headers;
            int timeoutMs = 10000;

            if (lua_istable(L, 3))
            {
                lua_pushnil(L);
                while (lua_next(L, 3) != 0)
                {
                    if (lua_isstring(L, -2) && lua_isstring(L, -1))
                    {
                        headers[lua_tostring(L, -2)] = lua_tostring(L, -1);
                    }
                    lua_pop(L, 1);
                }
            }

            if (lua_isnumber(L, 4))
            {
                timeoutMs = (int)lua_tointeger(L, 4);
            }

            Response resp = RequestSync(url, "POST", body, headers, timeoutMs);
            PushResponseTable(L, resp);
            return 1;
        }

        // Http.Download(url, destPath, callback)
        static int Lua_Http_Download(lua_State* L)
        {
            const char* url = luaL_checkstring(L, 1);
            const char* destPath = luaL_checkstring(L, 2);

            if (!lua_isfunction(L, 3))
            {
                return luaL_error(L, "Http.Download: callback function is required as 3rd argument");
            }

            lua_pushvalue(L, 3);
            int ref = luaL_ref(L, LUA_REGISTRYINDEX);

            DownloadFileAsync(url, destPath, ref);
            return 0;
        }

        // Http.DownloadSync(url, destPath) -> bool, errorMsg
        static int Lua_Http_DownloadSync(lua_State* L)
        {
            const char* url = luaL_checkstring(L, 1);
            const char* destPath = luaL_checkstring(L, 2);

            std::string err;
            bool ok = DownloadFileSync(url, destPath, err);

            lua_pushboolean(L, ok);
            if (!ok)
            {
                lua_pushstring(L, err.c_str());
                return 2;
            }
            return 1;
        }

        void RegisterLua(lua_State* L)
        {
            lua_newtable(L);

            lua_pushcfunction(L, Lua_Http_Get);
            lua_setfield(L, -2, "Get");

            lua_pushcfunction(L, Lua_Http_Post);
            lua_setfield(L, -2, "Post");

            lua_pushcfunction(L, Lua_Http_Request);
            lua_setfield(L, -2, "Request");

            lua_pushcfunction(L, Lua_Http_GetSync);
            lua_setfield(L, -2, "GetSync");

            lua_pushcfunction(L, Lua_Http_PostSync);
            lua_setfield(L, -2, "PostSync");

            lua_pushcfunction(L, Lua_Http_Download);
            lua_setfield(L, -2, "Download");

            lua_pushcfunction(L, Lua_Http_DownloadSync);
            lua_setfield(L, -2, "DownloadSync");

            // Register Http globally and in AMLua
            lua_pushvalue(L, -1);
            lua_setglobal(L, "Http");

            lua_getglobal(L, "AMLua");
            if (lua_istable(L, -1))
            {
                lua_pushvalue(L, -2);
                lua_setfield(L, -2, "Http");
            }
            lua_pop(L, 2); // pop AMLua and Http
        }
    }
}
