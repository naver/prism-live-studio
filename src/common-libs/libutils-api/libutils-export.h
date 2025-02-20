
#ifndef LIBUTILS_EXPORT_H
#define LIBUTILS_EXPORT_H

#include "qglobal.h"

#ifdef LIBUTILSAPI_LIB
#define LIBUTILSAPI_API Q_DECL_EXPORT
#else
#define LIBUTILSAPI_API Q_DECL_IMPORT
#endif

#endif // !LIBUTILS_EXPORT_H
