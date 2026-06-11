#pragma once

#include <qglobal.h>

#ifdef FRONTEND_API_LIB
#define FRONTEND_API Q_DECL_EXPORT
#else
#define FRONTEND_API Q_DECL_IMPORT
#endif

constexpr auto MODULE_FRONTEND_API = "frontend-api";
constexpr auto MODULE_FRONTEND_APIW = L"frontend-api";
