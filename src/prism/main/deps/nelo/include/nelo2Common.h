#ifndef __NELO2__COMMON__H__
#define __NELO2__COMMON__H__

#include <time.h>
#include <stdio.h>
#include <stdlib.h>

//init function error code list
#define NELO2_OK         0
#define NELO2_ERROR     -1
#define INVALID_PROJECT -2
#define INVALID_VERSION -3
#define INVALID_SERVER  -4
#define INVALID_PORT    -5

//default variant
#define NELO2_DEF_PROJECTVERSION "1.0.0"
#ifdef WIN32
#define NELO2_DEF_LOGSOURCE  "nelo2-windows"
#define DEPRECATED __declspec(deprecated("** This is a deprecated function, it will be removed from next version. **"))
#else
#ifdef ANDROID
#define NELO2_DEF_LOGSOURCE  "nelo2-AndroidNDK"
#else
#define NELO2_DEF_LOGSOURCE  "nelo2-linux"
#endif
#endif
#define NELO2_DEF_LOGTYPE    "nelo2-log"
#define NELO2_DEF_SERVERHOST "nelo2-col.navercorp.com"
#define NELO2_DEF_SERVERPORT 80

//log level
typedef enum _NELO_LOG_LEVEL
{
    NELO_LL_FATAL = 0,
    NELO_LL_ERROR = 3,
    NELO_LL_WARN  = 4,
    NELO_LL_INFO  = 5,
    NELO_LL_DEBUG = 7,
#define NELO_LL_TRACE NELO_LL_DEBUG
    NELO_LL_INVALID = -1
}NELO_LOG_LEVEL;

//language support
typedef enum
{
    NELO_LANG_DEFAULT = 0,
    NELO_LANG_ENGLISH = 1,
    NELO_LANG_KOREAN  = 2,
    NELO_LANG_JAPANESE= 3,
    NELO_LANG_CHINESE = 4,
    NELO_LANG_CHINESE_TRADITIONAL= 5,
#define NELO_LANG_CHINESE_SIMPLIFIED NELO_LANG_CHINESE
}NELO_LANG_TYPE;

//retry support
typedef struct
{
    const char* key;
    const char* value;
} NELO_READONLY_PAIR;
#endif // __NELO2__COMMON__H__
