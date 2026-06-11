#include "PLSLiveInfoDialogs.h"

#include "PLSPlatformApi.h"
#include "twitch/PLSLiveInfoTwitch.h"
#include "youtube/PLSLiveInfoYoutube.h"
#include "navertv/PLSLiveInfoNaverTV.h"
#include "band/PLSLiveInfoBand.h"
#include "facebook/PLSLiveInfoFacebook.h"
#include "naver-shopping-live/PLSLiveInfoNaverShoppingLIVE.h"
#include "pls-channel-const.h"
#include "PLSLiveInfoAfreecaTV.h"
#include "PLSChannelDataAPI.h"
#include "chzzk/PLSLiveInfoChzzk.h"
#include "ncb2b/PLSLiveInfoNCB2B.h"
#include "ChannelCommonFunctions.h"

int pls_exec_live_Info(const QVariantMap &info, QWidget *parent)
{
	auto channelName = info.value(ChannelData::g_fixPlatformName).toString();
	auto channelUUID = info.value(ChannelData::g_channelUUID).toString();

	if (TWITCH == channelName) {
		return pls_exec_live_Info_twitch(channelUUID, info, parent);
	} else if (YOUTUBE == channelName) {
		return pls_exec_live_Info_youtube(channelUUID, info, parent);
	} else if (VLIVE == channelName) {
		return pls_exec_live_Info_vlive(channelUUID, info, parent);
	} else if (NAVER_TV == channelName) {
		return pls_exec_live_Info_navertv(channelUUID, info, parent);
	} else if (BAND == channelName) {
		return pls_exec_live_Info_band(channelUUID, info, parent);
	} else if (WAV == channelName) {
		//TODO
	} else if (FACEBOOK == channelName) {
		return pls_exec_live_Info_facebook(channelUUID, info, parent);
	} else if (AFREECATV == channelName) {
		return pls_exec_live_Info_afreecatv(channelUUID, info, parent);
	} else if (NAVER_SHOPPING_LIVE == channelName) {
		return pls_exec_live_Info_naver_shopping_live(channelUUID, info, parent);
	} else if (CHZZK == channelName) {
		return pls_exec_live_Info_chzzk(channelUUID, info, parent);
	} else if (NCB2B == channelName) {
		return pls_exec_live_Info_bcb2b(channelUUID, info, parent);
	}

	auto handler = PLSCHANNELS_API->getPlatformHandler(channelName);
	if (handler == nullptr) {
		return QDialog::Rejected;
	}
	handler->showLiveInfo(channelUUID);

	return QDialog::Rejected;
}

int pls_exec_live_Info_twitch(const QString &which, const QVariantMap &info, QWidget *parent)
{
	auto pPlatform = PLS_PLATFORM_API->getPlatformById(which, info);

	if (nullptr == pPlatform) {
		return QDialog::Rejected;
	}

	PLSLiveInfoTwitch liveInfo(pPlatform, parent);
	return liveInfo.exec();
}

int pls_exec_live_Info_youtube(const QString &which, const QVariantMap &info, QWidget *parent)
{
	auto pPlatform = PLS_PLATFORM_API->getPlatformById(which, info);

	if (nullptr == pPlatform) {
		return QDialog::Rejected;
	}

	PLSLiveInfoYoutube liveInfo(pPlatform, parent);
	return liveInfo.exec();
}

int pls_exec_live_Info_vlive(const QString &which, const QVariantMap &info, QWidget *parent)
{
	return 0;
}

int pls_exec_live_Info_navertv(const QString &which, const QVariantMap &info, QWidget *parent)
{
	auto platform = PLS_PLATFORM_API->getPlatformById(which, info);

	if (!platform) {
		return PLSLiveInfoNaverTV::Rejected;
	}

	PLSLiveInfoNaverTV liveInfo(platform, info, parent);
	return liveInfo.exec();
}

int pls_exec_live_Info_navertv(PLSPlatformNaverTV *platform, QWidget *parent)
{
	PLSLiveInfoNaverTV liveInfo(platform, platform->getInitData(), parent);
	return liveInfo.exec();
}

int pls_exec_live_Info_band(const QString &which, const QVariantMap &info, QWidget *parent)
{
	auto pPlatform = PLS_PLATFORM_API->getPlatformById(which, info);

	if (nullptr == pPlatform) {
		return QDialog::Rejected;
	}

	PLSLiveInfoBand liveInfo(pPlatform, parent);
	return liveInfo.exec();
}

int pls_exec_live_Info_afreecatv(const QString &which, const QVariantMap &info, QWidget *parent)
{
	auto pPlatform = PLS_PLATFORM_API->getPlatformById(which, info);

	if (nullptr == pPlatform) {
		return QDialog::Rejected;
	}
	PLSLiveInfoAfreecaTV liveInfo(pPlatform, parent);
	return liveInfo.exec();
}

int pls_exec_live_Info_facebook(const QString &which, const QVariantMap &info, QWidget *parent)
{
	auto pPlatform = PLS_PLATFORM_API->getPlatformById(which, info);

	if (nullptr == pPlatform) {
		return QDialog::Rejected;
	}

	PLSLiveInfoFacebook liveInfo(pPlatform, parent);
	return liveInfo.exec();
}

int pls_exec_live_Info_naver_shopping_live(const QString &which, const QVariantMap &info, QWidget *parent)
{
	auto platform = PLS_PLATFORM_API->getPlatformById(which, info);

	if (!platform) {
		return PLSLiveInfoNaverShoppingLIVE::Rejected;
	}

	parent = (!parent) ? App()->getMainView() : parent;
	PLSLiveInfoNaverShoppingLIVE liveInfo(platform, info, parent);

	return liveInfo.exec();
}

int pls_exec_live_Info_naver_shopping_live(PLSPlatformNaverShoppingLIVE *platform, QWidget *parent)
{
	parent = (!parent) ? App()->getMainView() : parent;
	PLSLiveInfoNaverShoppingLIVE liveInfo(platform, platform->getInitData(), parent);

	return liveInfo.exec();
}

int pls_exec_live_Info_chzzk(const QString &which, const QVariantMap &info, QWidget *parent)
{
	auto pPlatform = PLS_PLATFORM_API->getPlatformById(which, info);

	if (nullptr == pPlatform) {
		return QDialog::Rejected;
	}
	PLSLiveInfoChzzk liveInfo(pPlatform, parent);
	return liveInfo.exec();
}

int pls_exec_live_Info_bcb2b(const QString &which, const QVariantMap &info, QWidget *parent)
{
	auto pPlatform = PLS_PLATFORM_API->getPlatformById(which, info);

	if (nullptr == pPlatform) {
		return QDialog::Rejected;
	}
	PLSLiveInfoNCB2B liveInfo(pPlatform, parent);
	return liveInfo.exec();
}
