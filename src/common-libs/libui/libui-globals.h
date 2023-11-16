#ifndef LIBUI_GLOBALS_H
#define LIBUI_GLOBALS_H

#include <qglobal.h>

#ifdef Q_OS_WIN
#ifdef LIBUI_LIB
#define LIBUI_API __declspec(dllexport)
#else
#define LIBUI_API __declspec(dllimport)
#endif
#else
#define LIBUI_API
#endif

#endif // !LIBUI_GLOBALS_H
