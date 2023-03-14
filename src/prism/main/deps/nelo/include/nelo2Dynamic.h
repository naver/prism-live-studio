#ifndef __NELO2__CPPSDK__H__
#define __NELO2__CPPSDK__H__

#include "nelo2Common.h"

#define _CRT_SECURE_NO_WARNINGS

#include <crtdbg.h>
#define NELO2_DYNAMIC_MODE
#if (defined(NELO2_DYNAMIC_MODE) || defined(NELO2_CLANG_MODE))
    #define NELO2_API
#else
    #ifdef NELO2_EXPORT_API
        #define NELO2_API __declspec(dllexport)
    #else
        #define NELO2_API __declspec(dllimport)
        #ifdef _DEBUG
           #pragma comment(lib, "libnelo2d.lib")
        #else
           #pragma comment(lib, "libnelo2.lib")
        #endif
    #endif
#endif


//for pure C have no bool type
#if !defined(__cplusplus) && !defined(c_plusplus)
typedef enum { false, true } bool;
#endif

////////////////////////////////////////////////////////////////////////////////
//BEGIN: C interface
#if defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif

//crash callback function type
typedef void (__cdecl *crash_func_type)(void *context);

//send callback function type
typedef void (__cdecl *nelo_send_failed_func_type)(const NELO_READONLY_PAIR *fields, size_t nFields, const char* errMsg, void* context);
typedef void (__cdecl *nelo_send_succeed_func_type)(const NELO_READONLY_PAIR *fields, size_t nFields, void* context);
typedef void (__cdecl *nelo_before_retry_func_type)(const NELO_READONLY_PAIR *fields, size_t nFields, unsigned int nthRetry, void* context);

//init/destory function
//[WARNING]: please call destory() function before you close your application.
NELO2_API int initialize(const char *projectName,
                         const char *projectVersion = NELO2_DEF_PROJECTVERSION,
                         const char *logSource = NELO2_DEF_LOGSOURCE,
                         const char *logType   = NELO2_DEF_LOGTYPE,
                         const char *serverAddr= NELO2_DEF_SERVERHOST,
                         const int  serverPort = NELO2_DEF_SERVERPORT,
                         const int  https      = 0
                        );
NELO2_API void destory();
typedef int  (__cdecl *initialize_func_type)(const char*, const char*, const char*, const char*, const char*, const int, const int);
typedef void (__cdecl *destory_func_type)();

//enable/disable collect the host information
NELO2_API void enableHostField();
NELO2_API void disableHostField();

typedef void (__cdecl *enableHostField_func_type)();
typedef void (__cdecl *disableHostField_func_type)();

//enable/disable collect the platform information
NELO2_API void enablePlatformField();
NELO2_API void disablePlatformField();

typedef void (__cdecl *enablePlatformField_func_type)();
typedef void (__cdecl *disablePlatformField_func_type)();

//add/delete custom field
NELO2_API bool addField(const char *key, const char *val);
NELO2_API void removeField(const char *key);
NELO2_API void removeAllFields();

typedef bool (__cdecl *addField_func_type)(const char*, const char*);
typedef void (__cdecl *removeField_func_type)(const char*);
typedef void (__cdecl *removeAllFields_func_type)();

//get/set log level function, the default log level is NELO_LL_INFO
NELO2_API NELO_LOG_LEVEL getLogLevel();
NELO2_API void setLogLevel(const NELO_LOG_LEVEL eLevel = NELO_LL_INFO);

typedef NELO_LOG_LEVEL (__cdecl *getLogLevel_func_type)();
typedef void (__cdecl *setLogLevel_func_type)(const NELO_LOG_LEVEL);

//get/set user id function
NELO2_API const char *getUserId();
NELO2_API void setUserId(const char *userID);

typedef const char* (__cdecl *getUserId_func_type)();
typedef void (__cdecl *setUserId_func_type)(const char*);

//set crash report UI window
NELO2_API bool openCrashCatcher(const bool bBackground = false, const NELO_LANG_TYPE eLangType = NELO_LANG_DEFAULT, const char* dumpFilePath = ".");
NELO2_API void setCrashCallback(const crash_func_type fnCrashCallback, void *context = NULL);
NELO2_API void closeCrashCatcher();

typedef bool (__cdecl *openCrashCatcher_func_type)(const bool, const NELO_LANG_TYPE, const char*);
typedef void (__cdecl *setCrashCallback_func_type)(const crash_func_type, void *context);
typedef void (__cdecl *closeCrashCatcher_func_type)();

//send log messages
NELO2_API bool sendLog(const NELO_LOG_LEVEL eLevel, const char *strMsg);
NELO2_API bool sendLogEx(const time_t tUTCTime, const NELO_LOG_LEVEL eLevel, const char *strMsg);

typedef bool (__cdecl *sendLog_func_type)(const NELO_LOG_LEVEL, const char*);
typedef bool (__cdecl *sendLogEx_func_type)(const time_t, const NELO_LOG_LEVEL, const char*);

//for sending job check
NELO2_API bool hasPendingUploads();
typedef bool (__cdecl *hasPendingUploads_func_type)();

//proxy
NELO2_API bool enableProxy(const char* proxy);
NELO2_API void disableProxy();

typedef bool (__cdecl *enableProxy_func_type)(const char*);
typedef void (__cdecl *disableProxy_func_type)();

//send retry policy and callbacks
NELO2_API bool setRetryPolicy(unsigned int maxRetries, unsigned int intervalSeconds);
NELO2_API void setBeforeRetryCallback(const nelo_before_retry_func_type beforeRetry, void* context);
NELO2_API void setSendFailedCallback(const nelo_send_failed_func_type onFailure, void* context);
NELO2_API void setSendSucceedCallback(const nelo_send_succeed_func_type onSuccess, void* context);

typedef bool (__cdecl *setRetryPolicy_func_type)(unsigned int, unsigned int);
typedef void (__cdecl *setBeforeRetryCallback_func_type)(const nelo_before_retry_func_type, void*);
typedef void (__cdecl *setSendFailedCallback_func_type)(const nelo_send_failed_func_type, void*);
typedef void (__cdecl *setSendSucceedCallback_func_type)(const nelo_send_succeed_func_type, void*);

#if defined(__cplusplus)||defined(c_plusplus)
}
#endif
//END: C interface
////////////////////////////////////////////////////////////////////////////////

#endif //__NELO2__CPPSDK__H__
