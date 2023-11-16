#pragma once

#include <qglobal.h>

#ifdef Q_OS_WIN

#ifdef FRONTEND_API_LIB
#define FRONTEND_API Q_DECL_EXPORT
#else
#define FRONTEND_API Q_DECL_IMPORT
#endif

#else
#define FRONTEND_API

#endif

constexpr auto MODULE_FRONTEND_API = "frontend-api";
constexpr auto MODULE_FRONTEND_APIW = L"frontend-api";
