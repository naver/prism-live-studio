
#include "PLSPlatformPrism.h"

extern const QString translatePlatformName(const QString &platformName);

PLSPlatformPrism *PLSPlatformPrism::instance()
{
	static PLSPlatformPrism *_instance = nullptr;

	if (nullptr == _instance) {
		_instance = new PLSPlatformPrism();
		_instance->moveToThread(qApp->thread()); //zhangdewen force move to main thread
	}

	return _instance;
}

PLSPlatformPrism::PLSPlatformPrism() : m_iVideoSeq(0), m_iHistorySeq(0), m_timerHeartbeat(this)
{
	
}

string PLSPlatformPrism::getStreamKey() const
{
	return string();
}

string PLSPlatformPrism::getStreamServer() const
{
	return string();
}

void PLSPlatformPrism::onActive(PLSPlatformBase *platform, bool value)
{
	
}

void PLSPlatformPrism::onInactive(PLSPlatformBase *platform, bool value)
{
	
}

void PLSPlatformPrism::onPrepareLive(bool value)
{
	
}

void PLSPlatformPrism::onLiveStarted(bool value)
{
	liveStartedCallback(value);
}

void PLSPlatformPrism::onPrepareFinish()
{
	prepareFinishCallback();
}

void PLSPlatformPrism::onLiveStopped()
{
	liveStoppedCallback();
}

void PLSPlatformPrism::onLiveEnded()
{
	
}

std::string PLSPlatformPrism::getCharVideoSeq()
{
	return std::string();
}

void PLSPlatformPrism::activateCallback(PLSPlatformBase *platform, bool value)
{
	
}

void PLSPlatformPrism::deactivateCallback(PLSPlatformBase *platform, bool value)
{
	
}

void PLSPlatformPrism::prepareLiveCallback(bool value)
{
	
}

void PLSPlatformPrism::liveStartedCallback(bool value)
{
	
}

void PLSPlatformPrism::prepareFinishCallback()
{
	
}

void PLSPlatformPrism::liveStoppedCallback()
{
	
}

void PLSPlatformPrism::liveEndedCallback()
{
	
}

void PLSPlatformPrism::setCommonReplyBuilderCookie(PLSHmacNetworkReplyBuilder &builder)
{
	
}

void PLSPlatformPrism::postJson(const QUrl &url, const QJsonObject &jsonObjectRoot, dataFunction onSucceed, dataErrorFunction onFailed)
{
	
}

const QString PLSPlatformPrism::urlForPath(const QString &path)
{
	return QString();
}

string PLSPlatformPrism::formatDateTime(time_t now)
{
	string buf;
	return buf;
}

string PLSPlatformPrism::getPublishingTitle()
{
	return "PRISM Live";
}

void PLSPlatformPrism::requestStartSimulcastLive(bool bPrism)
{
	
}

void PLSPlatformPrism::requestHeartbeat()
{
	
}

void PLSPlatformPrism::requestFailedCallback(const QString &url, int code, QByteArray data)
{
	
}

void PLSPlatformPrism::requestStopSimulcastLive(bool bPrism)
{
	
}

void PLSPlatformPrism::requestLiveDirectStart()
{
	
}

void PLSPlatformPrism::requestLiveDirectEnd()
{
	
}

void PLSPlatformPrism::requestStopSingleLive(PLSPlatformBase *platform)
{
	
}

void PLSPlatformPrism::requestRefrshAccessToken(PLSPlatformBase *platform, function<void(bool)> onNext)
{
	
}

void PLSPlatformPrism::showWarningAlertWithMsg(const QString &msg)
{
	
}

void PLSPlatformPrism::mqttRequestRefreshToken(PLSPlatformBase *platform, function<void(bool)> callback)
{
	
}

void PLSPlatformPrism::getSendThumAPIJson(QJsonObject &parameter)
{
	
}

void PLSPlatformPrism::printStartLog()
{

}

void PLSPlatformPrism::onTokenExpired()
{
	
}
