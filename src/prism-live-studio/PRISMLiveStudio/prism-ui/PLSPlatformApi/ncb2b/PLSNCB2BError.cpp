#include "PLSNCB2BError.h"

const QString RBE_DUPLICATED = "DUPLICATED";
const QString RBE_CONNECTION_ERROR = "CONNECTION_ERROR";
const QString RBE_NOTICE_LONG_BROADCAST = "NOTICE_LONG_BROADCAST";
const QString RBE_PARTNER_SERVICE_DISABLED = "PARTNER_SERVICE_DISABLED";

bool PLSNCB2BError::ncb2bErrorHandler(int code, ErrorModule module)
{
	bool isHandler = true;
	auto alertString = getAlertString(code, module);
	if (alertString.isEmpty()) {
		isHandler = false;
	} else {
		PLSAlertView::warning(nullptr, QObject::tr("Alert.Title"), alertString);
	}
	return isHandler;
}

QString PLSNCB2BError::getAlertString(int code, PLSNCB2BError::ErrorModule module)
{
	auto codeEnum = static_cast<PLSNCB2BError::ErrorCode>(code);
	switch (codeEnum) {
	case PLSNCB2BError::Service_No_Found:
		if (module == ErrorModule::Login || module == ErrorModule::dashBord)
			return QObject::tr("Ncb2b.Login.Service.No.Found");
		if (module == ErrorModule::Living)
			return QObject::tr("Ncb2b.Living.Service.No.Found");
		break;
	case PLSNCB2BError::Service_Del:
		if (module == ErrorModule::Login || module == ErrorModule::dashBord)
			return QObject::tr("Ncb2b.Login.Service.Deleted");
		if (module == ErrorModule::Living)
			return QObject::tr("Ncb2b.Living.Service.Deleted");
		break;
	case PLSNCB2BError::Service_Disable:
		if (module == ErrorModule::Living || module == ErrorModule::Login)
			return QObject::tr("Ncb2b.Living.Service.Disable");
		break;
	case PLSNCB2BError::Channel_Disable:
		if (module == ErrorModule::Living)
			return QObject::tr("Ncb2b.Living.Service.Channel.Disable");
		break;
	case PLSNCB2BError::partnerChannelDisabledByTrial:
		return QObject::tr("ncpb2b.channel.exceeded.trial.limit");
	case PLSNCB2BError::Service_Systemtime_Error:
		return QObject::tr("Prism.Login.Systemtime.Error");
	default:
		return {};
	}

	return {};
}

QString PLSNCB2BError::getMqttBroadcastEndAlertString(const QString &code, const QString &displayPlatformName)
{
	QString ncpAlertContent;
	if (code == RBE_DUPLICATED) {
		ncpAlertContent = QObject::tr("Ncpb2b.MQTT.Interrupt.Duplicated");
	} else if (code == RBE_NOTICE_LONG_BROADCAST) {
		ncpAlertContent = QObject::tr("MQTT.Max.Live.Time.Band.NaverShopping.End.Live").arg(displayPlatformName);
	} else if (code == RBE_PARTNER_SERVICE_DISABLED) {
		ncpAlertContent = QObject::tr("Ncpb2b.MQTT.Interrupt.Service.Disabled");
	}
	return ncpAlertContent;
}
