#include "crash_handler.h"
#include <signal.h>
#include <ucontext.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <android/log.h>

#define LOG_TAG "AMLua-Crash"

namespace AMLua
{
    namespace CrashHandler
    {
        static std::string g_CrashLogPath = "/storage/emulated/0/Android/data/com.rockstargames.gtasa/files/amlua_crash.log";
        static std::string g_ErrorLogPath = "/storage/emulated/0/Android/data/com.rockstargames.gtasa/files/amlua_error.log";
        static char g_CurrentScript[256] = "None";
        static char g_CurrentAction[256] = "Idle";

        static struct sigaction g_OldSigSegv;
        static struct sigaction g_OldSigAbrt;
        static struct sigaction g_OldSigBus;
        static struct sigaction g_OldSigFpe;
        static struct sigaction g_OldSigIll;

        static stack_t g_AltStack;
        static char g_AltStackBuffer[SIGSTKSZ * 4];

        const char* GetCrashLogPath()
        {
            return g_CrashLogPath.c_str();
        }

        const char* GetErrorLogPath()
        {
            return g_ErrorLogPath.c_str();
        }

        void SetCurrentScript(const char* scriptName)
        {
            if (scriptName)
            {
                strncpy(g_CurrentScript, scriptName, sizeof(g_CurrentScript) - 1);
                g_CurrentScript[sizeof(g_CurrentScript) - 1] = '\0';
            }
            else
            {
                strcpy(g_CurrentScript, "None");
            }
        }

        void SetCurrentAction(const char* action)
        {
            if (action)
            {
                strncpy(g_CurrentAction, action, sizeof(g_CurrentAction) - 1);
                g_CurrentAction[sizeof(g_CurrentAction) - 1] = '\0';
            }
            else
            {
                strcpy(g_CurrentAction, "Idle");
            }
        }

        void ClearContext()
        {
            strcpy(g_CurrentScript, "None");
            strcpy(g_CurrentAction, "Idle");
        }

        // Dedicated error logger for script syntax/runtime errors
        void LogError(const char* fmt, ...)
        {
            char msgBuf[4096];
            va_list args;
            va_start(args, fmt);
            vsnprintf(msgBuf, sizeof(msgBuf), fmt, args);
            va_end(args);

            // 1. Android Logcat
            __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", msgBuf);

            // 2. amlua_error.log
            time_t now = time(nullptr);
            struct tm* tmInfo = localtime(&now);
            char timeStr[32] = {0};
            if (tmInfo)
            {
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tmInfo);
            }

            FILE* fp = fopen(g_ErrorLogPath.c_str(), "a");
            if (fp)
            {
                fprintf(fp, "[%s] %s\n", timeStr, msgBuf);
                fflush(fp);
                fclose(fp);
            }
        }

        static const char* GetSignalName(int sig)
        {
            switch (sig)
            {
                case SIGSEGV: return "SIGSEGV (Segmentation Fault - Invalid Memory Access)";
                case SIGABRT: return "SIGABRT (Abort Signal - Assertion Failure or Engine Abort)";
                case SIGBUS:  return "SIGBUS (Bus Error - Alignment or Memory Fault)";
                case SIGFPE:  return "SIGFPE (Floating Point / Arithmetic Exception)";
                case SIGILL:  return "SIGILL (Illegal Instruction)";
                default:      return "Unknown Signal";
            }
        }

        static const char* GetSignalCodeDesc(int sig, int code)
        {
            if (sig == SIGSEGV)
            {
                switch (code)
                {
                    case SEGV_MAPERR: return "SEGV_MAPERR (Address not mapped to object)";
                    case SEGV_ACCERR: return "SEGV_ACCERR (Invalid permissions for mapped object)";
                    default: return "SEGV (Memory access violation)";
                }
            }
            else if (sig == SIGBUS)
            {
                switch (code)
                {
                    case BUS_ADRALN: return "BUS_ADRALN (Invalid address alignment)";
                    case BUS_ADRERR: return "BUS_ADRERR (Non-existent physical address)";
                    case BUS_OBJERR: return "BUS_OBJERR (Object-specific hardware error)";
                    default: return "BUS (Bus error)";
                }
            }
            return "";
        }

        // Async-signal-safe crash handler
        static void CrashSignalHandler(int sig, siginfo_t* info, void* context)
        {
            ucontext_t* uc = (ucontext_t*)context;
            uintptr_t pc = 0;
            uintptr_t lr = 0;
            uintptr_t sp = 0;

            #if defined(__aarch64__)
            if (uc)
            {
                pc = (uintptr_t)uc->uc_mcontext.pc;
                sp = (uintptr_t)uc->uc_mcontext.sp;
                lr = (uintptr_t)uc->uc_mcontext.regs[30];
            }
            #elif defined(__arm__)
            if (uc)
            {
                pc = (uintptr_t)uc->uc_mcontext.arm_pc;
                sp = (uintptr_t)uc->uc_mcontext.arm_sp;
                lr = (uintptr_t)uc->uc_mcontext.arm_lr;
            }
            #endif

            // Identify fault location using dladdr
            Dl_info pcInfo;
            memset(&pcInfo, 0, sizeof(pcInfo));
            dladdr((void*)pc, &pcInfo);

            Dl_info lrInfo;
            memset(&lrInfo, 0, sizeof(lrInfo));
            dladdr((void*)lr, &lrInfo);

            char crashBuffer[8192];
            size_t offset = 0;

            time_t now = time(nullptr);
            struct tm* tmInfo = localtime(&now);
            char timeStr[32] = "N/A";
            if (tmInfo)
            {
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tmInfo);
            }

            offset += snprintf(crashBuffer + offset, sizeof(crashBuffer) - offset,
                "================================================================================\n"
                "                           AMLUA CRASH LOG REPORT                               \n"
                "================================================================================\n"
                "Date/Time       : %s\n"
                "Signal Caught   : %s (%d)\n"
                "Signal Code     : %d %s\n"
                "Fault Address   : %p\n"
                "Active Script   : %s\n"
                "Last Action     : %s\n"
                "--------------------------------------------------------------------------------\n",
                timeStr,
                GetSignalName(sig), sig,
                info ? info->si_code : 0, info ? GetSignalCodeDesc(sig, info->si_code) : "",
                info ? info->si_addr : nullptr,
                g_CurrentScript,
                g_CurrentAction
            );

            // Crash PC location
            if (pcInfo.dli_fname)
            {
                uintptr_t libBase = (uintptr_t)pcInfo.dli_fbase;
                uintptr_t libOffset = pc - libBase;
                offset += snprintf(crashBuffer + offset, sizeof(crashBuffer) - offset,
                    "Crash PC Address: 0x%zx (%s + 0x%zx)\n"
                    "Crash Function  : %s\n",
                    (size_t)pc,
                    pcInfo.dli_fname, (size_t)libOffset,
                    pcInfo.dli_sname ? pcInfo.dli_sname : "(unknown/hidden)"
                );
            }
            else
            {
                offset += snprintf(crashBuffer + offset, sizeof(crashBuffer) - offset,
                    "Crash PC Address: 0x%zx (unmapped or anonymous code memory)\n",
                    (size_t)pc
                );
            }

            // Caller LR location
            if (lrInfo.dli_fname)
            {
                uintptr_t libBase = (uintptr_t)lrInfo.dli_fbase;
                uintptr_t libOffset = lr - libBase;
                offset += snprintf(crashBuffer + offset, sizeof(crashBuffer) - offset,
                    "Caller LR Return: 0x%zx (%s + 0x%zx)\n"
                    "Caller Function : %s\n",
                    (size_t)lr,
                    lrInfo.dli_fname, (size_t)libOffset,
                    lrInfo.dli_sname ? lrInfo.dli_sname : "(unknown/hidden)"
                );
            }
            else
            {
                offset += snprintf(crashBuffer + offset, sizeof(crashBuffer) - offset,
                    "Caller LR Return: 0x%zx\n",
                    (size_t)lr
                );
            }

            offset += snprintf(crashBuffer + offset, sizeof(crashBuffer) - offset,
                "Stack Pointer SP: 0x%zx\n"
                "--------------------------------------------------------------------------------\n"
                "CPU REGISTERS DUMP:\n",
                (size_t)sp
            );

            #if defined(__aarch64__)
            if (uc)
            {
                for (int i = 0; i < 31; i += 2)
                {
                    if (i + 1 < 31)
                    {
                        offset += snprintf(crashBuffer + offset, sizeof(crashBuffer) - offset,
                            "  X%02d: 0x%016zx   X%02d: 0x%016zx\n",
                            i, (size_t)uc->uc_mcontext.regs[i],
                            i + 1, (size_t)uc->uc_mcontext.regs[i + 1]
                        );
                    }
                    else
                    {
                        offset += snprintf(crashBuffer + offset, sizeof(crashBuffer) - offset,
                            "  X%02d: 0x%016zx\n",
                            i, (size_t)uc->uc_mcontext.regs[i]
                        );
                    }
                }
            }
            #elif defined(__arm__)
            if (uc)
            {
                offset += snprintf(crashBuffer + offset, sizeof(crashBuffer) - offset,
                    "  R0: 0x%08zx   R1: 0x%08zx   R2: 0x%08zx   R3: 0x%08zx\n"
                    "  R4: 0x%08zx   R5: 0x%08zx   R6: 0x%08zx   R7: 0x%08zx\n"
                    "  R8: 0x%08zx   R9: 0x%08zx  R10: 0x%08zx   FP: 0x%08zx\n"
                    "  IP: 0x%08zx   SP: 0x%08zx   LR: 0x%08zx   PC: 0x%08zx\n",
                    (size_t)uc->uc_mcontext.arm_r0, (size_t)uc->uc_mcontext.arm_r1,
                    (size_t)uc->uc_mcontext.arm_r2, (size_t)uc->uc_mcontext.arm_r3,
                    (size_t)uc->uc_mcontext.arm_r4, (size_t)uc->uc_mcontext.arm_r5,
                    (size_t)uc->uc_mcontext.arm_r6, (size_t)uc->uc_mcontext.arm_r7,
                    (size_t)uc->uc_mcontext.arm_r8, (size_t)uc->uc_mcontext.arm_r9,
                    (size_t)uc->uc_mcontext.arm_r10, (size_t)uc->uc_mcontext.arm_fp,
                    (size_t)uc->uc_mcontext.arm_ip, (size_t)uc->uc_mcontext.arm_sp,
                    (size_t)uc->uc_mcontext.arm_lr, (size_t)uc->uc_mcontext.arm_pc
                );
            }
            #endif

            offset += snprintf(crashBuffer + offset, sizeof(crashBuffer) - offset,
                "================================================================================\n\n"
            );

            // 1. Write to Logcat
            __android_log_print(ANDROID_LOG_FATAL, LOG_TAG, "%s", crashBuffer);

            // 2. Write to amlua_crash.log (using low-level async-signal-safe open/write)
            int fd = open(g_CrashLogPath.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (fd >= 0)
            {
                write(fd, crashBuffer, strlen(crashBuffer));
                close(fd);
            }

            // 3. Also write a summary copy to amlua_error.log
            int fdErr = open(g_ErrorLogPath.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
            if (fdErr >= 0)
            {
                write(fdErr, crashBuffer, strlen(crashBuffer));
                close(fdErr);
            }

            // Chain to original signal handler if available
            struct sigaction* oldAct = nullptr;
            switch (sig)
            {
                case SIGSEGV: oldAct = &g_OldSigSegv; break;
                case SIGABRT: oldAct = &g_OldSigAbrt; break;
                case SIGBUS:  oldAct = &g_OldSigBus; break;
                case SIGFPE:  oldAct = &g_OldSigFpe; break;
                case SIGILL:  oldAct = &g_OldSigIll; break;
                default: break;
            }

            if (oldAct && oldAct->sa_sigaction && oldAct->sa_sigaction != CrashSignalHandler)
            {
                oldAct->sa_sigaction(sig, info, context);
            }
            else if (oldAct && oldAct->sa_handler && oldAct->sa_handler != SIG_DFL && oldAct->sa_handler != SIG_IGN)
            {
                oldAct->sa_handler(sig);
            }
            else
            {
                // Restore default handler and re-raise so process terminates cleanly
                signal(sig, SIG_DFL);
                raise(sig);
            }
        }

        void Install(const char* crashLogDir)
        {
            if (crashLogDir && crashLogDir[0] != '\0')
            {
                g_CrashLogPath = std::string(crashLogDir) + "/amlua_crash.log";
                g_ErrorLogPath = std::string(crashLogDir) + "/amlua_error.log";
            }

            // Set up alternate signal stack
            memset(&g_AltStack, 0, sizeof(g_AltStack));
            g_AltStack.ss_sp = g_AltStackBuffer;
            g_AltStack.ss_size = sizeof(g_AltStackBuffer);
            g_AltStack.ss_flags = 0;
            sigaltstack(&g_AltStack, nullptr);

            struct sigaction act;
            memset(&act, 0, sizeof(act));
            act.sa_sigaction = CrashSignalHandler;
            act.sa_flags = SA_SIGINFO | SA_ONSTACK;

            sigaction(SIGSEGV, &act, &g_OldSigSegv);
            sigaction(SIGABRT, &act, &g_OldSigAbrt);
            sigaction(SIGBUS,  &act, &g_OldSigBus);
            sigaction(SIGFPE,  &act, &g_OldSigFpe);
            sigaction(SIGILL,  &act, &g_OldSigIll);

            __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "Crash handler installed. Log path: %s", g_CrashLogPath.c_str());
        }

        void Uninstall()
        {
            sigaction(SIGSEGV, &g_OldSigSegv, nullptr);
            sigaction(SIGABRT, &g_OldSigAbrt, nullptr);
            sigaction(SIGBUS,  &g_OldSigBus, nullptr);
            sigaction(SIGFPE,  &g_OldSigFpe, nullptr);
            sigaction(SIGILL,  &g_OldSigIll, nullptr);
        }
    }
}
