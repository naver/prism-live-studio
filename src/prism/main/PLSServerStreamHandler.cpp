
#include "PLSServerStreamHandler.hpp"

static bool thumbnailRequestCallback(void *param, uint32_t width, uint32_t height, bool request_succeed);

PLSServerStreamHandler *PLSServerStreamHandler::instance()
{
	static PLSServerStreamHandler syncServerManager;
	return &syncServerManager;
}

PLSServerStreamHandler::~PLSServerStreamHandler() {}

PLSServerStreamHandler::PLSServerStreamHandler(QObject *parent) : QObject(parent) {}

bool PLSServerStreamHandler::getOutroInfo(QString &path, uint *timeout)
{
	return false;
}

QString PLSServerStreamHandler::getOutputResolution()
{
	return QString();
}

QString PLSServerStreamHandler::getOutputFps()
{
	return QString();
}

bool PLSServerStreamHandler::isSupportedResolutionFPS(QString &outTipString)
{
	return false;
}

void PLSServerStreamHandler::requestLiveDirectEnd() {}

void PLSServerStreamHandler::getWatermarkInfo(QString &guide, bool &enabled, bool &selected) {}

QVariantMap PLSServerStreamHandler::getWatermarkMap(QString &platformKey)
{
	return QVariantMap();
}

QVariantMap PLSServerStreamHandler::getOutroInfoMap(QString &platformKey)
{
	return QVariantMap();
}

bool PLSServerStreamHandler::isValidWatermark()
{
	return true;
}

bool PLSServerStreamHandler::isValidOutro()
{
	return true;
}

void PLSServerStreamHandler::updateWatermark() {}

void PLSServerStreamHandler::clearWatermark() {}

obs_watermark_policy PLSServerStreamHandler::showTypeByString(const QString &typeString)
{
	return OBS_WATERMARK_POLICY_ALWAYS_SHOW;
}

void PLSServerStreamHandler::startThumnailRequest() {}

void PLSServerStreamHandler::uploadThumbnailToRemote() {}

QHttpPart PLSServerStreamHandler::getHttpPart(const QString &key, const QString &body)
{
	return QHttpPart();
}

void PLSServerStreamHandler::onReplyErrorData(int, const QString &url, const QString &, const QString &errorInfo) {}

bool PLSServerStreamHandler::isLandscape()
{
	return false;
}

static bool thumbnailRequestCallback(void *, uint32_t, uint32_t, bool request_succeed)
{
	return false;
}
