#ifndef PLSRESOURCEHANDLE_GLOBAL_H
#define PLSRESOURCEHANDLE_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(PLSRESOURCEHANDLE_LIBRARY)
#define PLSRESOURCEHANDLE_EXPORT Q_DECL_EXPORT
#else
#define PLSRESOURCEHANDLE_EXPORT Q_DECL_IMPORT
#endif

#endif // PLSRESOURCEHANDLE_GLOBAL_H
