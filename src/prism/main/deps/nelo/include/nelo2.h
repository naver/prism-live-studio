#ifndef __NELO2__CPPSDK__H__
#define __NELO2__CPPSDK__H__

#include "nelo2Common.h"

#define _CRT_SECURE_NO_WARNINGS

#include <crtdbg.h>
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
typedef void (__cdecl *nelo_send_failed_func_type)(const NELO_READONLY_PAIR *fields, size_t nFields, const char *errMsg, void *context);
typedef void (__cdecl *nelo_send_succeed_func_type)(const NELO_READONLY_PAIR *fields, size_t nFields, void *context);
typedef void (__cdecl *nelo_before_retry_func_type)(const NELO_READONLY_PAIR *fields, size_t nFields, unsigned int nthRetry, void *context);

#if (!defined(NELO2_DYNAMIC_MODE))&&(!defined(NELO2_MULTIINSTANCE))
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
NELO2_API bool enableProxy(const char *proxy);
NELO2_API void disableProxy();

typedef bool (__cdecl *enableProxy_func_type)(const char*);
typedef void (__cdecl *disableProxy_func_type)();

//send retry policy and callbacks
NELO2_API bool setRetryPolicy(unsigned int maxRetries, unsigned int intervalSeconds);
NELO2_API void setBeforeRetryCallback(const nelo_before_retry_func_type beforeRetry, void *context);
NELO2_API void setSendFailedCallback(const nelo_send_failed_func_type onFailure, void *context);
NELO2_API void setSendSucceedCallback(const nelo_send_succeed_func_type onSuccess, void *context);

typedef bool (__cdecl *setRetryPolicy_func_type)(unsigned int, unsigned int);
typedef void (__cdecl *setBeforeRetryCallback_func_type)(const nelo_before_retry_func_type, void *context);
typedef void (__cdecl *setSendFailedCallback_func_type)(const nelo_send_failed_func_type, void *context);
typedef void (__cdecl *setSendSucceedCallback_func_type)(const nelo_send_succeed_func_type, void *context);
#endif

#if defined(__cplusplus)||defined(c_plusplus)
}
#endif
//END: C interface
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//BEGIN: C++ interface
#if defined(__cplusplus)||defined(c_plusplus)
class NELO2_API NELO2Log
{
public:
    //custom field type define
    class NELO2_API CustomField
    {
        public:
            CustomField();
            ~CustomField();
        public:
            bool addField(const char *key, const char *value);

            void delField(const char *key);

            void clearAll();
        private:
            void *m_customField; 
            friend class NELO2Log;
    };
    
    NELO2Log();
    ~NELO2Log();

private:
    NELO2Log(const NELO2Log &rhs);
    NELO2Log& operator=(const NELO2Log &rhs);

private:
    void *m_logOrigin;

public:
    //return code:
    //  NELO2_OK: initial resources is successful.
    //  INVALID_PROJECT: the project name is empty
    //  INVALID_SERVER: the server's address is empty
    //  INVALID_PORT: the server's port is invalid
    int initialize(const char *projectName,
                   const char *projectVersion = NELO2_DEF_PROJECTVERSION,
                   const char *logSource = NELO2_DEF_LOGSOURCE,
                   const char *logType   = NELO2_DEF_LOGTYPE,
                   const char *serverAddr= NELO2_DEF_SERVERHOST,
                   const int  serverPort = NELO2_DEF_SERVERPORT,
                   const bool https      = false
                  );

    //[WARNING]: you must call destory() before exit your application
    void destory();

    //enable/disable collect the host information
    void enableHostField();
    void disableHostField();

    //enable/disable collect the platform information
    void enablePlatformField();
    void disablePlatformField();

    //open crash catch function and set crash report's cache path
    //return true: open crash catch function is successful.
    //      false: open crash catch function is failed, maybe can't access the cache path.
    bool openCrashCatcher(const bool bBackground = false, const NELO_LANG_TYPE eLangType = NELO_LANG_DEFAULT, const char* dumpFilePath = ".");

    //call the callback function when crash is happened and user already set it.
    void setCrashCallback(const crash_func_type fnCrashCallback, void *context = NULL);

    //close crash catch function
    void closeCrashCatcher();

    //add global custom field
    bool addGlobalField(const char* key, const char *value);

    //delete a specific field
    void delGlobalField(const char* key);

    //clear all global custom fields
    void clearGlobalFields();

    //set send log level, if priority of log level is higher than set value, the log can send to nelo2 system, if less, don't send it
    // default value is INFO
    void setLogLevel(const NELO_LOG_LEVEL eLevel = NELO_LL_INFO);

    //get currently log level
    NELO_LOG_LEVEL getLogLevel();

    //get user id
    const char *getUserId();

    //set user id
    void setUserId(const char *userID);

    //true: have some pending files
    //false: don't have any pending files
    bool hasPendingUploads();

    //true:  send log message is successful
    //false: send log message is failed
    //[WARNING]: SDK will use system UTC time as logTime
    bool sendLog(const NELO_LOG_LEVEL eLevel, const char *strMsg);
    bool sendLog(const NELO_LOG_LEVEL eLevel, const char *strMsg, const CustomField &customFields);

    //true:  send log message is successful
    //false: send log message is failed
    //[WARNING]: you can set log time, but the tUTCTime must is second/millisecond level
    bool sendLog(const time_t tUTCTime, const NELO_LOG_LEVEL eLevel, const char *strMsg);
    bool sendLog(const time_t tUTCTime, const NELO_LOG_LEVEL eLevel, const char *strMsg, const CustomField &customFields);

    bool enableProxy(const char* proxy);
    void disableProxy();

    bool setRetryPolicy(unsigned int maxRetries, unsigned int intervalSeconds = 1);
    void setBeforeRetryCallback(const nelo_before_retry_func_type beforeRetry, void *context = NULL);
    void setSendFailedCallback(const nelo_send_failed_func_type onFailure, void *context = NULL);
    void setSendSucceedCallback(const nelo_send_succeed_func_type onSuccess, void *context = NULL);
    
public:
    static void handleWindowsPureVitualCall();
    static void handleWindowsInvalidParameter(const wchar_t* expression,
                                              const wchar_t* function,
                                              const wchar_t* file,
                                              unsigned int line,
                                              uintptr_t reserved);
};
#endif
#define INIT_NELO2_WINDOWS_ENVIRONMENT _CrtSetReportMode(_CRT_ASSERT, 0);\
    _set_purecall_handler((_purecall_handler)NELO2Log::handleWindowsPureVitualCall); \
    _set_invalid_parameter_handler((_invalid_parameter_handler)NELO2Log::handleWindowsInvalidParameter);

#if defined(__cplusplus)||defined(c_plusplus)
class Nelo2WindowsIniter
{
public:
    Nelo2WindowsIniter()
    {
        _CrtSetReportMode(_CRT_ASSERT, 0);
        _set_purecall_handler((_purecall_handler)NELO2Log::handleWindowsPureVitualCall);
        _set_invalid_parameter_handler((_invalid_parameter_handler)NELO2Log::handleWindowsInvalidParameter);
    }
};
static Nelo2WindowsIniter __global__nelo2__initer__;

#endif
//END: C++ interface
////////////////////////////////////////////////////////////////////////////////

#endif //__NELO2__CPPSDK__H__

