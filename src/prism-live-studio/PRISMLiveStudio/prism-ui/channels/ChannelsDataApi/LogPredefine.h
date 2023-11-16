#pragma once

#include <QDateTime>
#include <QDebug>
#include <QString>
#include <typeinfo>

#include "./log/log.h"
/* this file just for log anchor while log module is in progress */

#define PRE_LOG(x, type)                                     \
	{                                                    \
		PLS_##type("Channels", "Channels :%s ", #x); \
	}

#define PRE_LOG_MSG(x, type)                                                              \
	{                                                                                 \
		PLS_##type("Channels", "Channels :%s ", QString(x).toUtf8().constData()); \
	}

#define PRE_LOG_MSG_STEP(msg, step, type)                                                                                                                              \
	{                                                                                                                                                              \
		pls_flow_log(false, PLS_LOG_##type, "Channels", QString(step).toLocal8Bit().constData(), __FILE__, __LINE__, "%s", QString(msg).toUtf8().constData()); \
	}

#define PRE_LOG_UI_MSG(x, type)                                                                                  \
	{                                                                                                        \
		PLS_UI_STEP("Channels", QString(" UI :%1 , ").arg(typeid(type).name()).toUtf8().constData(), x); \
	}

#define PRE_LOG_UI_MSG_STRING(UI, action)                                                                        \
	{                                                                                                        \
		PLS_UI_STEP("Channels", QString(UI).toUtf8().constData(), QString(action).toUtf8().constData()); \
	}
