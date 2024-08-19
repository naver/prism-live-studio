#pragma once
#include "PLSAlertView.h"
#include <qobject.h>

namespace PLSNCB2BError {
enum ErrorModule { Login, Living, dashBord };
enum ErrorCode {
	Service_Systemtime_Error = 25,
	Service_No_Found = 1104,
	Service_Del = 1022,
	Service_Disable = 1101,
	Channel_Disable = 1102,
	Service_No_Auth = 3005,
	partnerChannelDisabledByTrial = 1103
};

bool ncb2bErrorHandler(int code, ErrorModule module);
QString getAlertString(int code, ErrorModule module);
QString getMqttBroadcastEndAlertString(const QString &code, const QString &displayPlatformName);
};
