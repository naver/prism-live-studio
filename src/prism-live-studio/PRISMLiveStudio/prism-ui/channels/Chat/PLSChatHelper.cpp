#include "PLSChatHelper.h"
#include <qobject.h>
#include <QCoreApplication>
#include "../PLSPlatformApi/PLSPlatformApi.h"
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataAPI.h"
#include "PLSSyncServerManager.hpp"
#include "pls-channel-const.h"
#include "pls-common-define.hpp"
#include "pls-frontend-api.h"

using namespace std;
using namespace common;
static const string defaultUrl = " "; //because cef can't use empty url, so use this temp error url.

const char *const s_ChatStatusType = "status";
const char *const s_ChatStatusNormal = "normal";
const char *const s_ChatStatusOnlyOne = "onlyOne";
const char *const s_ChatStatusSelect = "select";

const char *const s_fontSessionName = "PLSChatFontZoom";
const char *const s_fontSessionKey = "scale";
static QString getChatUrl(const QString &name)
{
	return "";
}

PLSChatHelper *PLSChatHelper::instance()
{
	static PLSChatHelper _instance;
	return &_instance;
}

static void onPrismUserLogout(pls_frontend_event event, const QVariantList &, void *ctx)
{
	if (event != pls_frontend_event::PLS_FRONTEND_EVENT_PRISM_LOGOUT)
		return;

	pls_unused(ctx);
	config_set_string(App()->GlobalConfig(), "BasicWindow", "geometryChat", nullptr);
	config_set_bool(App()->GlobalConfig(), "Basic", "chatIsHidden", false);
	config_set_uint(App()->GlobalConfig(), KeyConfigLiveInfo, KeyTwitchServer, 0);
	config_save(App()->GlobalConfig());
}

PLSChatHelper::~PLSChatHelper()
{
	pls_frontend_remove_event_callback(onPrismUserLogout, this);
}

void PLSChatHelper::startToNotify()
{
	pls_frontend_add_event_callback(onPrismUserLogout, this);
}

static QString getChatTabImagePath(const QString &objectName, const QString &platName, const QString &platNameNoLower, channel_data::ImageType type)
{
	QString typeString;
	QString isOnOrOff;
	switch (type) {
	case channel_data::ImageType::chatIcon_offDisable:
		typeString = "disable";
		isOnOrOff = "off";
		break;
	case channel_data::ImageType::chatIcon_onNormal:
		typeString = "normal";
		isOnOrOff = "on";
		break;
	case channel_data::ImageType::chatIcon_onHover:
		typeString = "over";
		isOnOrOff = "on";
		break;
	case channel_data::ImageType::chatIcon_onClick:
		typeString = "click";
		isOnOrOff = "on";
		break;
	case channel_data::ImageType::chatIcon_offHover:
		typeString = "over";
		isOnOrOff = "off";
		break;
	case channel_data::ImageType::chatIcon_offClick:
		typeString = "click";
		isOnOrOff = "off";
		break;
	case channel_data::ImageType::chatIcon_offNormal:
		typeString = "normal";
		isOnOrOff = "off";
		break;
	default:
		assert(false);
		break;
	}
	auto img = getChatIcon(platNameNoLower, type, QString(":/resource/images/chat/btn-tab-%1-%2-%3.svg").arg(platName).arg(isOnOrOff).arg(typeString));
	img = !img.isEmpty() ? img : QString(":/resource/images/chat/btn-tab-default-%1-%2.svg").arg(isOnOrOff).arg(typeString);
	img = QDir::fromNativeSeparators(img);
	return img;
}

#define GET_IMAGE_PATH(Type) getChatTabImagePath(objectName, platName, platNameNoLower, Type)
QString PLSChatHelper::getTabButtonCss(const QString &objectName, const QString &platName, const QString &platNameNoLower) const
{
	auto img = GET_IMAGE_PATH(channel_data::ImageType::chatIcon_offNormal);
	auto shNormal = QString(R"(PLSChatDialog #%1 {image:url("%2");})").arg(objectName).arg(img);

	img = GET_IMAGE_PATH(channel_data::ImageType::chatIcon_offDisable);
	QString shDisable = QString(R"(PLSChatDialog #%1:disable {image:url("%3");})").arg(objectName).arg(img);

	img = GET_IMAGE_PATH(channel_data::ImageType::chatIcon_onNormal);
	auto shSelect = QString(R"(PLSChatDialog #%1[status=%2] {image:url("%3");})").arg(objectName).arg(s_ChatStatusSelect).arg(img);

	img = GET_IMAGE_PATH(channel_data::ImageType::chatIcon_onNormal);
	auto shOnlyOne = QString(R"(PLSChatDialog #%1[status=%2] {image:url("%3");})").arg(objectName).arg(s_ChatStatusOnlyOne).arg(img);

	img = GET_IMAGE_PATH(channel_data::ImageType::chatIcon_onNormal);
	auto shOnlyOneHover = QString(R"(PLSChatDialog #%1[status=%2]:hover {image:url("%3");})").arg(objectName).arg(s_ChatStatusOnlyOne).arg(img);

	img = GET_IMAGE_PATH(channel_data::ImageType::chatIcon_onNormal);
	auto shOnlyOnePressed = QString(R"(PLSChatDialog #%1[status=%2]:pressed {image:url("%3");})").arg(objectName).arg(s_ChatStatusOnlyOne).arg(img);

	img = GET_IMAGE_PATH(channel_data::ImageType::chatIcon_onHover);
	auto shSelectHover = QString(R"(PLSChatDialog #%1[status=%2]:hover {image:url("%3");})").arg(objectName).arg(s_ChatStatusSelect).arg(img);

	img = GET_IMAGE_PATH(channel_data::ImageType::chatIcon_onClick);
	auto shSelectPressed = QString(R"(PLSChatDialog #%1[status=%2]:pressed {image:url("%3");})").arg(objectName).arg(s_ChatStatusSelect).arg(img);

	img = GET_IMAGE_PATH(channel_data::ImageType::chatIcon_offHover);
	auto shNormalHover = QString(R"(PLSChatDialog #%1[status=%2]:hover {image:url("%3");})").arg(objectName).arg(s_ChatStatusNormal).arg(img);

	img = GET_IMAGE_PATH(channel_data::ImageType::chatIcon_offClick);
	auto shNormalPressed = QString(R"(PLSChatDialog #%1[status=%2]:pressed {image:url("%3");})").arg(objectName).arg(s_ChatStatusNormal).arg(img);

	QString styleSheet = shNormal.append(shDisable)
				     .append(shSelect)
				     .append(shOnlyOne)
				     .append(shOnlyOneHover)
				     .append(shOnlyOnePressed)
				     .append(shSelectHover)
				     .append(shSelectPressed)
				     .append(shNormalHover)
				     .append(shNormalPressed);
	//	qDebug() << "\n" << styleSheet << "\n";
	return styleSheet;
}

void PLSChatHelper::sendWebShownEventIfNeeded(int index) const
{
	if (!isLocalHtmlPage(index)) {
		return;
	}

	auto name = PLS_CHAT_HELPER->getPlatformNameFromIndex(index);
	if (name.isEmpty() && index != ChatPlatformIndex::All) {
		return;
	}

	PLS_PLATFORM_API->sendWebChatTabShown(name, index == ChatPlatformIndex::All);
}

bool PLSChatHelper::isLocalHtmlPage(int index) const
{
	return (index == ChatPlatformIndex::All || index == ChatPlatformIndex::NaverTV || index == ChatPlatformIndex::VLive || index == ChatPlatformIndex::AfreecaTV ||
		index == ChatPlatformIndex::Facebook || index == ChatPlatformIndex::NaverShopping || index == ChatPlatformIndex::NCB2B);
}
bool PLSChatHelper::isRemoteHtmlPage(int index) const
{
	if (isLocalHtmlPage(index)) {
		return false;
	}
	if (index == ChatPlatformIndex::RTMP || index == ChatPlatformIndex::UnDefine) {
		return false;
	}
	return true;
}
QString PLSChatHelper::getString(int index, bool toLowerSpace) const
{
	QString name = "";
	switch (index) {
	case ChatPlatformIndex::All:
		name = "All";
		break;
	case ChatPlatformIndex::RTMP:
		name = "Rtmp";
		break;
	case ChatPlatformIndex::Twitch:
		name = "Twitch";
		break;
	case ChatPlatformIndex::Youtube:
		name = "YouTube";
		break;
	case ChatPlatformIndex::NaverTV:
		name = "NAVER TV";
		break;
	case ChatPlatformIndex::VLive:
		name = "V LIVE";
		break;
	case ChatPlatformIndex::Facebook:
		name = "Facebook";
		break;
	case ChatPlatformIndex::AfreecaTV:
		name = "afreecaTV";
		break;
	case ChatPlatformIndex::NaverShopping:
		name = "Naver Shopping";
		break;
	case ChatPlatformIndex::NCB2B:
		name = NCB2BConvertChannelName(NCB2B);
		break;
	case ChatPlatformIndex::Chzzk:
		name = "Chzzk";
		break;
	default:
		name = "Other";
		break;
	}
	if (toLowerSpace) {
		name = name.toLower();
		name = name.replace(" ", "");
	}
	return name;
}
int PLSChatHelper::getIndexFromInfo(const QVariantMap &info) const
{
	int index = ChatPlatformIndex::UnDefine;

	if (info.empty()) {
		return index;
	}
	QString platformName = info.value(ChannelData::g_fixPlatformName, "").toString();
	auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt();
	PLS_INFO("chatHelper", "platformName = %s", platformName.toUtf8().constData());
	if (dataType >= ChannelData::CustomType) {
		index = ChatPlatformIndex::RTMP;
	} else if (dataType == ChannelData::ChannelType) {
		if (platformName == TWITCH) {
			index = ChatPlatformIndex::Twitch;
		} else if (platformName == YOUTUBE) {
			index = ChatPlatformIndex::Youtube;
		} else if (platformName == NAVER_TV) {
			index = ChatPlatformIndex::NaverTV;
		} else if (platformName == VLIVE) {
			index = ChatPlatformIndex::VLive;
		} else if (platformName == FACEBOOK) {
			index = ChatPlatformIndex::Facebook;
		} else if (platformName == AFREECATV) {
			index = ChatPlatformIndex::AfreecaTV;
		} else if (platformName == NAVER_SHOPPING_LIVE) {
			index = ChatPlatformIndex::NaverShopping;
		} else if (platformName == NCB2B) {
			index = ChatPlatformIndex::NCB2B;
		} else if (platformName == CHZZK) {
			index = ChatPlatformIndex::Chzzk;
		}
	}
	return index;
}

void PLSChatHelper::getSelectInfoFromIndex(int index, QVariantMap &getInfo) const
{
	for (const auto &info : PLSCHANNELS_API->getCurrentSelectedChannels()) {
		if (info.empty()) {
			continue;
		}
		auto dataType = info.value(ChannelData::g_data_type, ChannelData::RTMPType).toInt();
		if (dataType != ChannelData::ChannelType) {
			continue;
		}
		auto name = getPlatformNameFromIndex(index);
		QString platformName = info.value(ChannelData::g_channelName, "").toString();
		if (!name.isEmpty() && name == platformName) {
			getInfo = info;
			break;
		}
	}
}

QString PLSChatHelper::getPlatformNameFromIndex(int index) const
{
	switch (index) {
	case ChatPlatformIndex::RTMP:
	case ChatPlatformIndex::UnDefine:
		return QString();
	case ChatPlatformIndex::Twitch:
		return TWITCH;
	case ChatPlatformIndex::Youtube:
		return YOUTUBE;
	case ChatPlatformIndex::NaverTV:
		return NAVER_TV;
	case ChatPlatformIndex::VLive:
		return VLIVE;
	case ChatPlatformIndex::Facebook:
		return FACEBOOK;
	case ChatPlatformIndex::AfreecaTV:
		return AFREECATV;
	case ChatPlatformIndex::NaverShopping:
		return NAVER_SHOPPING_LIVE;
	case ChatPlatformIndex::NCB2B:
		return NCB2BConvertChannelName(NCB2B);
	case ChatPlatformIndex::Chzzk:
		return CHZZK;
	default:
		return QString();
	}
}

QString PLSChatHelper::getRtmpPlaceholderString() const
{
	// case no channel
	if (PLSCHANNELS_API->getCurrentSelectedChannels().size() == 0) {
		return QObject::tr("chat.rtmp.placehoder.nochannel");
	}
	QString showStr;
	auto hasRtmp = false;
	auto hasBand = false;
	auto hasTwitter = false;

	auto platformActived = PLS_PLATFORM_ACTIVIED;
	for (auto item : platformActived) {
		if (PLSServiceType::ST_CUSTOM == item->getServiceType()) {
			hasRtmp = true;
			continue;
		}
		if (PLSServiceType::ST_BAND == item->getServiceType()) {
			hasBand = true;
			continue;
		}
		if (PLSServiceType::ST_TWITTER == item->getServiceType()) {
			hasTwitter = true;
			continue;
		}
	}

	//not show rtmp same as band and twitter
	if (hasRtmp && hasBand && hasTwitter) {
		return QTStr("Chat.Band.And.Twitter.Not.Supported");
	}
	//not show rtmp same as band
	if (hasRtmp && hasBand) {
		return QTStr("Chat.Band.Not.Supported");
	}
	//not show rtmp same as twitter
	if (hasRtmp && hasTwitter) {
		return QTStr("Chat.Twitter.Not.Supported");
	}
	if (hasTwitter && hasBand) {
		return QTStr("Chat.Band.And.Twitter.Not.Supported");
	}
	if (hasRtmp) {
		return QObject::tr("chat.rtmp.placehoder.onlyrtmp");
	}
	if (hasBand) {
		return QTStr("Chat.Band.Not.Supported");
	}
	if (hasTwitter) {
		return QTStr("Chat.Twitter.Not.Supported");
	}
	return showStr;
}

bool PLSChatHelper::isCefWidgetIndex(int index) const
{
	return (ChatPlatformIndex::RTMP != index && ChatPlatformIndex::UnDefine != index);
}

bool PLSChatHelper::canChatYoutube(const QVariantMap &info, bool checkForKids) const
{
	QString category_en = info.value(ChannelData::g_displayLine2, "").toString();
	if (category_en.isEmpty()) {
		category_en = info.value(ChannelData::g_catogry, "").toString();
	}

	bool isPrivate = s_youtube_private_en == category_en;
	if (isPrivate || (checkForKids && PLS_PLATFORM_YOUTUBE->getSelectData().isForKids)) {
		return false;
	}
	return true;
}

std::string PLSChatHelper::getChatUrlWithIndex(int index, const QVariantMap &info) const
{
	auto buildUrl_VL_NA = [](const QString &platform) {
		QString lang(App()->GetLocale());
		return getChatUrl("naver.html").append(QString("?lang=%1&platform=%2").arg(pls_get_current_language_short_str()).arg(platform)).toStdString();
	};
	auto buildUrl_FB_AF = [](const QString &platform) {
		QString lang(App()->GetLocale());
		return getChatUrl("mqtt.html").append(QString("?lang=%1&platform=%2").arg(pls_get_current_language_short_str()).arg(platform)).toStdString();
	};

	auto buildYoutubeUrl = [](const QString &id) {
		if (!id.isEmpty()) {
			return g_plsYoutubeChatRemoteUrl.arg(id).toStdString();
		}
		QString lang(App()->GetLocale());
		QString url = getChatUrl("youtube.html").append(QString("?disableScale=1&lang=%1").arg(pls_get_current_language_short_str()));
		return url.toStdString();
	};

	auto buildUrl = [](const QString &smallUrl) {
		QString lang(App()->GetLocale());
		return getChatUrl(smallUrl).append(QString("?lang=%1").arg(pls_get_current_language_short_str())).toStdString();
	};

	string showUrl = defaultUrl;
	QDir appDir(QCoreApplication::applicationDirPath());
	auto channelName = info.value(ChannelData::g_userName, "").toString();

	switch (index) {
	case ChatPlatformIndex::All:
		showUrl = getChatUrl("all.html").append(QString("?lang=%1").arg(pls_get_current_language_short_str())).toStdString();

		break;
	case ChatPlatformIndex::NaverTV:
		showUrl = buildUrl_VL_NA("NAVERTV");
		break;
	case ChatPlatformIndex::VLive:
		showUrl = buildUrl_VL_NA("VLIVE");
		break;
	case ChatPlatformIndex::Twitch:
		if (info.isEmpty()) {
			break;
		}

		if (!channelName.isEmpty()) {
			showUrl = QString(g_plsTwitchChatUrl).arg(channelName).toStdString();
		}
		break;
	case ChatPlatformIndex::Youtube:
		if (info.isEmpty()) {
			break;
		}
		showUrl = buildYoutubeUrl(PLS_PLATFORM_YOUTUBE ? PLS_PLATFORM_YOUTUBE->getSelectID() : QString());
		break;
	case ChatPlatformIndex::AfreecaTV:
		showUrl = buildUrl_FB_AF("AFREECATV");
		break;
	case ChatPlatformIndex::Facebook:
		showUrl = buildUrl_FB_AF("FACEBOOK");
		break;
	case ChatPlatformIndex::NaverShopping:
		showUrl = buildUrl("shopping.html");
		break;
	case ChatPlatformIndex::NCB2B:
		showUrl = getChatUrl("ncp.html")
				  .append(QString("?lang=%1&platform=%2&sub_name=%3").arg(pls_get_current_language_short_str()).arg("NCP").arg(NCB2BConvertChannelName(NCB2B)))
				  .toStdString();
		break;
	case ChatPlatformIndex::Chzzk: {
		auto channelID = info[ChannelData::g_subChannelId].toString();
		if (channelID.isEmpty()) {
			break;
		}
		showUrl = QString("%1/live/%2/chat").arg(g_plsChzzkApiHost).arg(channelID).toStdString();
	} break;
	default:
		break;
	}
	if (QString::fromStdString(showUrl).contains(".html")) {
			return "https://prismlive.com";
		}
	return showUrl;
}

bool PLSChatHelper::showToastWhenStart(QString &showStr) const
{
	//when living toast should not show.
	if (!PLS_PLATFORM_API->isPrismLive()) {
		return false;
	}

	auto hasPlatforms = false;
	auto platformActived = PLS_PLATFORM_ACTIVIED;
	for (auto item : platformActived) {
		if (PLSServiceType::ST_CUSTOM == item->getServiceType()) {

			continue;
		}
		if (PLSServiceType::ST_BAND == item->getServiceType()) {

			continue;
		}
		if (PLSServiceType::ST_TWITTER == item->getServiceType()) {

			continue;
		}
		hasPlatforms = true;
	}

	//when there is no some other platforms eg.youtube ,chat dialog has show placeholder text,toast need't.
	if (!hasPlatforms) {
		return false;
	}
	showStr = getToastString();
	return !showStr.isEmpty();
}

QString PLSChatHelper::getToastString() const
{
	// case no channel
	if (PLSCHANNELS_API->getCurrentSelectedChannels().size() == 0) {
		return QObject::tr("chat.rtmp.placehoder.nochannel");
	}
	QString showStr;
	auto hasRtmp = false;
	auto hasBand = false;
	auto hasTwitter = false;

	auto platformActived = PLS_PLATFORM_ACTIVIED;
	for (auto item : platformActived) {
		if (PLSServiceType::ST_CUSTOM == item->getServiceType()) {
			hasRtmp = true;
			continue;
		}
		if (PLSServiceType::ST_BAND == item->getServiceType()) {
			hasBand = true;
			continue;
		}
		if (PLSServiceType::ST_TWITTER == item->getServiceType()) {
			hasTwitter = true;
			continue;
		}
	}

	if (hasRtmp && hasBand && hasTwitter) {
		return QTStr("Chat.Band.Twitter.And.RTMP.Not.Supported");
	}
	if (hasRtmp && hasBand) {
		return QTStr("Chat.Band.And.RTMP.Not.Supported");
	}
	if (hasRtmp && hasTwitter) {
		return QTStr("Chat.Twitter.And.RTMP.Not.Supported");
	}
	if (hasTwitter && hasBand) {
		return QTStr("Chat.Band.And.Twitter.Not.Supported");
	}
	if (hasRtmp) {
		return QTStr("Chat.Toast.RTMP.Not.Supported");
	}
	if (hasBand) {
		return QTStr("Chat.Band.Not.Supported");
	}
	if (hasTwitter) {
		return QTStr("Chat.Twitter.Not.Supported");
	}
	return showStr;
}

PLSChatHelper::ChatFontSacle PLSChatHelper::getFontBtnStatus(int scaleSize)
{
	if (m_fontScales.back() == scaleSize) {
		return PLSChatHelper::ChatFontSacle::PlusDisable;
	}
	if (m_fontScales.front() == scaleSize) {
		return PLSChatHelper::ChatFontSacle::MinusDisable;
	}

	return PLSChatHelper::ChatFontSacle::Normal;
}

int PLSChatHelper::getNextSacelSize(bool isToPlus)
{
	int oldSize = PLSChatHelper::getFontSacleSize();
	int oldIndex = 0;

	auto len = m_fontScales.size();
	for (int i = 0; i < len; i++) {
		if (m_fontScales[i] == oldSize) {
			oldIndex = i;
			break;
		}
	}
	if (oldIndex == 0 && !isToPlus) {
		return oldSize;
	}
	if (oldIndex == (len - 1) && isToPlus) {
		return oldSize;
	}
	return m_fontScales[isToPlus ? ++oldIndex : --oldIndex];
}

int PLSChatHelper::getFontSacleSize()
{
	int scaleSize = 100;
	if (config_has_user_value(App()->GlobalConfig(), s_fontSessionName, s_fontSessionKey)) {
		scaleSize = static_cast<int>(config_get_int(App()->GlobalConfig(), s_fontSessionName, s_fontSessionKey));
	}
	return scaleSize;
}

void PLSChatHelper::sendWebChatFontSizeChanged(int scaleSize)
{
	scaleSize = scaleSize <= 5 ? scaleSize * 100 : scaleSize;
	QJsonObject data;
	data.insert("scaleSize", scaleSize / 100.0);

	QJsonObject root;
	root.insert("type", "scale");
	root.insert("data", data);

	config_set_int(App()->GlobalConfig(), s_fontSessionName, s_fontSessionKey, scaleSize);
	config_save(App()->GlobalConfig());

	pls_frontend_call_dispatch_js_event_cb("prism_events", QJsonDocument(root).toJson().constData());

	PLS_INFO("PLSChat", QString("PLSChat send font scale size: %1").arg(scaleSize).toUtf8().constData());
}

QString PLSChatHelper::getDispatchJS(int index)
{
	if (PLS_CHAT_HELPER->isLocalHtmlPage(index)) {
		return "sendToPrism('{\"type\":\"onReady\", \"data\": {}}')";
	}
	if (index == ChatPlatformIndex::Youtube) {
		return getYoutubeDisableBackupJS();
	}
	return "";
}

QString PLSChatHelper::getYoutubeDisableBackupJS()
{

	QDir appDir(QCoreApplication::applicationDirPath());
	QString path = appDir.absoluteFilePath(QString("data/prism-studio/webpage/youtube_backup_disable.js"));
	QFile file(path);
	file.open(QIODevice::ReadOnly | QIODevice::Text);
	QByteArray byteArray = file.readAll();
	file.close();

	return byteArray;
}
