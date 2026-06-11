#ifndef LIBUI_GLOBALS_H
#define LIBUI_GLOBALS_H

#include <qglobal.h>

#ifdef LIBUI_LIB
#define LIBUI_API Q_DECL_EXPORT
#else
#define LIBUI_API Q_DECL_IMPORT
#endif

#endif // !LIBUI_GLOBALS_H
