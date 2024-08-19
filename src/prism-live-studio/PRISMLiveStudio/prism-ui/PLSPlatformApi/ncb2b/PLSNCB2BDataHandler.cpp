#include "PLSNCB2BDataHandler.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QMetaMethod>
#include "../PLSPlatformApi.h"
#include "PLSAPINCB2B.h"
#include "PLSChannelDataAPI.h"
#include "pls-common-define.hpp"
#include "pls-net-url.hpp"
#include "ChannelCommonFunctions.h"

QString PLSNCB2BDataHandler::getPlatformName()
{
	return NCB2B;
}

bool PLSNCB2BDataHandler::isMultiChildren()
{
	return true;
}

bool PLSNCB2BDataHandler::tryToUpdate(const QVariantMap &srcInfo, const UpdateCallback &finishedCall)
{
	QString channelUUID = srcInfo[ChannelData::g_channelUUID].toString();
	auto platform = dynamic_cast<PLSPlatformNCB2B *>(PLS_PLATFORM_API->getPlatformById(channelUUID, srcInfo));
	if (!platform) {
		PLS_ERROR(MODULE_PLATFORM_NCB2B, "%s %s NCB2B refresh failed, platform not exists", PrepareInfoPrefix, __FUNCTION__);
		QVariantMap info = srcInfo;
		info[ChannelData::g_channelStatus] = ChannelData::ChannelStatus::Error;
		finishedCall(QList<QVariantMap>{info});
		return false;
	}
	PLS_INFO(MODULE_PLATFORM_NCB2B, "%s %s channelUUID(%s) NCB2B platform(%p) refresh", PrepareInfoPrefix, __FUNCTION__, channelUUID.toStdString().c_str(), platform);
	platform->setInitData(srcInfo);
	QMetaObject::invokeMethod(platform, [platform, srcInfo, finishedCall]() {
		platform->reInitLiveInfo();
		platform->requestChannelInfo(srcInfo, finishedCall);
	});
	return true;
}

QList<QPair<QString, QPixmap>> PLSNCB2BDataHandler::getEndLiveViewList(const QVariantMap &sourceData)
{
	int originSize = 15;
	int scale = 3;
	int scaleSize = originSize * scale;

	QList<QPair<QString, QPixmap>> ret;
	QString count1 = sourceData.value(ChannelData::g_viewers, "").toString();
	QString count2 = sourceData.value(ChannelData::g_likes, "").toString();
	QString count3 = sourceData.value(ChannelData::g_comments, "").toString();

	QPixmap pix1(pls_shared_paint_svg(getInfo(sourceData, ChannelData::g_viewersPix, ChannelData::g_defaultViewerIcon), QSize(scaleSize, scaleSize)));
	QPixmap pix2(pls_shared_paint_svg(getInfo(sourceData, ChannelData::g_likesPix, ChannelData::g_defaultViewerIcon), QSize(scaleSize, scaleSize)));
	QPixmap pix3(pls_shared_paint_svg(getInfo(sourceData, ChannelData::g_commentsPix, QString(":/channels/resource/images/ChannelsSource/statistics/ic-liveend-chat-fb.svg")),
					  QSize(scaleSize, scaleSize)));

	EndLivePair p1(count1, pix1);
	ret.append(p1);

	EndLivePair p2(count2, pix2);
	ret.append(p2);

	ret.append({count3, pix3});

	return ret;
}
