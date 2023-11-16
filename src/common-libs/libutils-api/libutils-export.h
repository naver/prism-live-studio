
#ifndef LIBUTILS_EXPORT_H
#define LIBUTILS_EXPORT_H

#include "qglobal.h"

#ifdef Q_OS_WIN

#ifdef LIBUTILSAPI_LIB
#define LIBUTILSAPI_API __declspec(dllexport)
#else
#define LIBUTILSAPI_API __declspec(dllimport)
#endif

#else
#define LIBUTILSAPI_API

#endif

#endif // !LIBUTILS_EXPORT_H
