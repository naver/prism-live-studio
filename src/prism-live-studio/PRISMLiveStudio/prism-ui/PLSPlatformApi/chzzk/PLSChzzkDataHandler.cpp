#include "PLSChzzkDataHandler.h"

#include <QUrl>
#include <QJsonObject>
#include <QJsonArray>

#include "pls-net-url.hpp"
#include "pls-common-define.hpp"
#include "../PLSPlatformApi.h"
#include "PLSLiveInfoChzzk.h"

PLSChzzkDataHandler::~PLSChzzkDataHandler()
{
	qDebug() << "PlatformAPI PLSChzzkDataHandler release";
}

QString PLSChzzkDataHandler::getPlatformName()
{
	return CHZZK;
}

bool PLSChzzkDataHandler::tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &finishedCall)
{
	myLastInfo() = srcInfo;

	QString channelUUID = srcInfo[ChannelData::g_channelUUID].toString();
	auto cookie = srcInfo.value(ChannelData::g_channelCookie).toString();
	PLS_INFO(MODULE_PLATFORM_CHZZK, "PlatformAPI tryToUpdate chzzk platform, channel type is %d , channel uuid is %s", srcInfo.value(ChannelData::g_data_type).toInt(),
		 channelUUID.toStdString().c_str());
	auto chzzk = dynamic_cast<PLSPlatformChzzk *>(PLS_PLATFORM_API->getPlatformById(channelUUID, srcInfo));
	if (!chzzk) {
		PLS_ERROR(MODULE_PLATFORM_CHZZK, "%s %s Chzzk refresh failed, platform not exists", PrepareInfoPrefix, __FUNCTION__);
		QVariantMap info = srcInfo;
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
		finishedCall(QList<QVariantMap>{myLastInfo()});
		return false;
	} else {
		chzzk->setInitData(srcInfo);
		QMetaObject::invokeMethod(chzzk, [chzzk, srcInfo, finishedCall]() { chzzk->requestChannelInfo(srcInfo, finishedCall); });
	}

	return true;
}
QList<QPair<QString, QPixmap>> PLSChzzkDataHandler::getEndLiveViewList(const QVariantMap &sourceData)
{
	int originSize = 15;
	int scale = 3;
	int scaleSize = originSize * scale;

	QList<QPair<QString, QPixmap>> ret;
	QString count1 = sourceData.value(ChannelData::g_viewers, "").toString();

	QPixmap pix1(pls_shared_paint_svg(getInfo(sourceData, ChannelData::g_viewersPix, ChannelData::g_defaultViewerIcon), QSize(scaleSize, scaleSize)));

	EndLivePair p1(count1, pix1);
	ret.append(p1);

	return ret;
}
