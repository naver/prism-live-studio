#include "ChannelCommonFunctions.h"
#include <qdatastream.h>
#include <qurlquery.h>
#include <quuid.h>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QUrl>
#include <QVersionNumber>
#include "PLSChannelDataAPI.h"
#include "PLSChannelsVirualAPI.h"
#include "frontend-api.h"
#include "libhttp-client.h"

#include <qapplication.h>
#include "PLSLoginDataHandler.h"
#include "PLSSyncServerManager.hpp"
#include "login-user-info.hpp"
#include "obs-app.hpp"
#include "pls-channel-const.h"

constexpr auto QRCPATH = ":/channels/resource";
using namespace ChannelData;

IMPL_ENUM_SERIALIZATION(ChannelData::ChannelStatus);
IMPL_ENUM_SERIALIZATION(ChannelData::ChannelUserStatus);
IMPL_ENUM_SERIALIZATION(ChannelData::ChannelDataType);
IMPL_ENUM_SERIALIZATION(ChannelData::LiveState);
IMPL_ENUM_SERIALIZATION(ChannelData::RecordState);
IMPL_ENUM_SERIALIZATION(ChannelData::ChannelDualOutput);

QString findFileInResources(const QString &dirPath, const QString &key)
{
	QDir dir(dirPath);
	QRegularExpression rex(key, QRegularExpression::CaseInsensitiveOption);
	auto filesList = dir.entryInfoList();
	for (const auto &file : filesList) {
		QString filename = file.fileName();

		if (auto match = rex.match(filename); match.hasMatch()) {
			return file.absoluteFilePath();
		}
		if (file.isDir()) {
			auto retStr = findFileInResources(file.absoluteFilePath(), key);
			if (!retStr.isEmpty()) {
				return retStr;
			}
		}
	}
	return QString();
}

void deleteItem(QListWidgetItem *item)
{
	auto listWid = item->listWidget();

	if (auto widget = listWid->itemWidget(item); widget) {
		widget->deleteLater();
	}
	if (listWid) {
		listWid->removeItemWidget(item);
		listWid->takeItem(listWid->row(item));
	}

	delete item;
}

QString translatePlatformName(const QString &platformName)
{

	if (platformName.contains(VLIVE, Qt::CaseInsensitive)) {
		return TR_VLIVE;
	}

	if (platformName.contains(NAVER_TV, Qt::CaseInsensitive)) {
		return TR_NAVER_TV;
	}

	if (platformName.contains(WAV, Qt::CaseInsensitive)) {
		return TR_WAV;
	}

	if (platformName.contains(BAND, Qt::CaseInsensitive)) {
		return TR_BAND;
	}

	if (platformName.contains(TWITCH, Qt::CaseInsensitive)) {
		return TR_TWITCH;
	}

	if (platformName.contains(YOUTUBE, Qt::CaseInsensitive)) {
		return TR_YOUTUBE;
	}

	if (platformName.contains(FACEBOOK, Qt::CaseInsensitive)) {
		return TR_FACEBOOK;
	}

	if (platformName.contains(WHALE_SPACE, Qt::CaseInsensitive)) {
		return TR_WHALE_SPACE;
	}

	if (platformName.contains(AFREECATV, Qt::CaseInsensitive)) {
		return TR_AFREECATV;
	}

	if (platformName.contains(NOW, Qt::CaseInsensitive)) {
		return TR_NOW;
	}

	if (platformName.contains(NAVER_SHOPPING_LIVE, Qt::CaseInsensitive)) {
		return TR_NAVER_SHOPPING_LIVE;
	}

	if (platformName.contains(CHZZK, Qt::CaseInsensitive)) {
		return TR_CHZZK;
	}

	return channelNameConvertMultiLang(platformName);
}

bool isVersionLessthan(const QString &leftVer, const QString &rightVer)
{
	return QVersionNumber::fromString(leftVer) < QVersionNumber::fromString(rightVer);
}

QVariantMap createDefaultChannelInfoMap(const QString &platformName, int defaultType, const QString &cmdStr)
{
	QVariantMap channelInfo;
	QString uuid = createUUID();
	channelInfo.insert(g_channelUUID, uuid);
	QString tempPlatformName = platformName;
	if (SELECT_TYPE == platformName) {
		tempPlatformName = CUSTOM_RTMP;
	}
	channelInfo.insert(g_fixPlatformName, tempPlatformName);

	QString tempName = platformName;
	if (!cmdStr.isEmpty()) {
		tempName = cmdStr;
	}
	channelInfo.insert(g_channelName, tempName);

	channelInfo.insert(g_nickName, tempName);
	channelInfo.insert(g_userName, tempName);
	channelInfo.insert(g_data_type, defaultType);
	channelInfo.insert(g_channelStatus, InValid);
	channelInfo.insert(g_channelUserStatus, Disabled);
	channelInfo.insert(g_createTime, QDateTime::currentDateTime());
	channelInfo.insert(g_isUpdated, false);
	channelInfo.insert(g_displayState, true);
	channelInfo.insert(g_isLeader, true);

	qsizetype index = -1;
	if (defaultType == ChannelType) {
		auto defaultPlatforms = getDefaultPlatforms();
		index = defaultPlatforms.indexOf(tempName);
	} else if (defaultType >= CustomType) {
		channelInfo.insert(g_rtmpUserID, "");
		channelInfo.insert(g_password, "");
		channelInfo.insert(g_channelName, RTMPT_DEFAULT_TYPE);
	}
	channelInfo.insert(g_displayOrder, index);
	channelInfo.insert(g_channelDualOutput, NoSet);
	return channelInfo;
}

QString createUUID()
{
	return QUuid::createUuid().toString();
}

QString getDynamicChannelIcon(QString &imagePath)
{
	if (!imagePath.isEmpty()) {
		QString tmpPath = QString("PRISMLiveStudio/resources/library/library_Policy_PC/%1").arg(imagePath);
		tmpPath = pls_get_user_path(tmpPath);
		if (QFile::exists(tmpPath)) {
			return tmpPath;
		} else {
#if defined(Q_OS_WIN)
			auto localeTempPath = QApplication::applicationDirPath() + QStringLiteral("/../../data/prism-studio/DynamicChannelIcon/%1");
#elif defined(Q_OS_MACOS)
			auto localeTempPath = pls_get_app_resource_dir() + "/data/prism-studio/DynamicChannelIcon/%1";
#endif
			QString localePath = localeTempPath.arg(imagePath);
			if (QFile::exists(localePath)) {
				return localePath;
			} else {
				PLS_ERROR("channle", "image path is not exist %s", localePath.toUtf8().constData());
			}
		}
	}
	return QString();
}

QString getChatIcon(const QString &channelName, int imageType, const QString &resChatIconPath)
{
	auto map = PLSSyncServerManager::instance()->getSupportedPlatformsMap();
	auto channelMap = map.value(channelName).toMap();
	auto chatIconMap = channelMap.value("chatIcon").toMap();
	QString imagePath;
	switch (imageType) {
	case ImageType::chatIcon_offNormal:
		imagePath = getInfo(chatIconMap, "offNormal", QString());
		break;
	case ImageType::chatIcon_offHover:
		imagePath = getInfo(chatIconMap, "offHover", QString());
		break;
	case ImageType::chatIcon_offClick:
		imagePath = getInfo(chatIconMap, "offClick", QString());
		break;
	case ImageType::chatIcon_offDisable:
		imagePath = getInfo(chatIconMap, "offDisable", QString());
		break;
	case ImageType::chatIcon_onNormal:
		imagePath = getInfo(chatIconMap, "onNormal", QString());
		break;
	case ImageType::chatIcon_onHover:
		imagePath = getInfo(chatIconMap, "onHover", QString());
		break;
	case ImageType::chatIcon_onClick:
		imagePath = getInfo(chatIconMap, "onClick", QString());
		break;
	default:
		break;
	}

	auto tmpPath = getDynamicChannelIcon(imagePath);
	if (!tmpPath.isEmpty()) {
		return tmpPath;
	}
	if (QFile::exists(resChatIconPath)) {
		return resChatIconPath;
	}
	return QString();
}
QString getPlatformImageFromName(const QString &channelName, int imageType, const QString &prefix, const QString &surfix)
{
	auto map = PLSSyncServerManager::instance()->getSupportedPlatformsMap();
	auto channelMap = map.value(channelName).toMap();
	QString imagePath, defaultPath;
	switch (imageType) {
	case ImageType::tagIcon:
		imagePath = getInfo(channelMap, "tagIcon", QString());
		defaultPath = g_tagIcon;
		if (imagePath.isEmpty()) {
			defaultPath.clear();
		}
		break;
	case ImageType::dashboardButtonIcon:
		imagePath = getInfo(channelMap, "dashboardButtonIcon", QString());
		defaultPath = g_dashboardButtonIcon;
		break;
	case ImageType::addChannelButtonIcon:
		imagePath = getInfo(channelMap, "addChannelButtonIcon", QString());
		defaultPath = g_addChannelButtonIcon;
		break;
	case ImageType::addChannelButtonConnectedIcon:
		imagePath = getInfo(channelMap, "addChannelButtonConnectedIcon", QString());
		defaultPath = g_addChannelButtonConnectedIcon;
		break;
	case ImageType::channelSettingBigIcon:
		imagePath = getInfo(channelMap, "channelSettingBigIcon", QString());
		defaultPath = g_channelSettingBigIcon;
		break;
	default:
		break;
	}

	auto tmpPath = getDynamicChannelIcon(imagePath);
	if (!tmpPath.isEmpty()) {
		return tmpPath;
	}
	auto searchKey = prefix + channelName + surfix;
	searchKey.remove(" ");
	imagePath = findFileInResources(QRCPATH, searchKey);

	if (imagePath.isEmpty()) {
		imagePath = defaultPath;
	}
	return imagePath;
}

QString getYoutubeShareUrl(const QString &broadCastID)
{
	QString url(g_youtubeUrl);
	url += "/watch?v=";
	url += broadCastID;
	return url;
}

void loadPixmap(QPixmap &pix, const QString &pixmapPath, const QSize &pixSize)
{
	if (pixmapPath.isEmpty()) {
		pix = QPixmap();
		return;
	}

	if (pixmapPath.toLower().endsWith(".svg")) {
		pix = pls_shared_paint_svg(pixmapPath, pixSize);
	} else {
		if (!pix.load(pixmapPath)) {
			pix.load(pixmapPath, "PNG");
			PRE_LOG_MSG(QString("error when load image: " + QFileInfo(pixmapPath).fileName() + " , may be the image suffix is not right").toStdString().c_str(), INFO)
		}
	}
}

void getComplexImageOfChannel(const QString &uuid, int imageType, QString &userIcon, QString &platformIcon, const QString &prefix, const QString &surfix)
{
	const auto &srcData = PLSCHANNELS_API->getChanelInfoRef(uuid);

	QString platformName = getInfo(srcData, g_channelName, QString());
	if (srcData.isEmpty()) {
		return;
	}
	int type = getInfo(srcData, g_data_type, NoType);
	if (type >= CustomType) {
		userIcon = getPlatformImageFromName(platformName, imageType);
		if (userIcon.isEmpty()) {
			userIcon = g_defualtPlatformIcon;
		}
		platformIcon = g_defualtPlatformSmallIcon;
	} else {
		userIcon = getInfo(srcData, g_userIconCachePath, g_defaultHeaderIcon);
		platformIcon = getPlatformImageFromName(platformName, imageType, prefix, surfix);
	}
}

QString getImageCacheFilePath()
{
	return getChannelCacheDir() + QDir::separator() + "image.dat";
}

QString getChannelCacheFilePath()
{
	return getChannelCacheDir() + QDir::separator() + g_channelCacheFile;
}

QString getChannelCacheDir()
{
	QString ret = pls_get_user_path(QCoreApplication::applicationName() + QDir::separator() + "Cache");

	if (QDir dir(ret); !dir.exists(ret)) {
		dir.mkpath(ret);
	}

	if (QDir::searchPaths("Cache").isEmpty()) {
		QDir::addSearchPath("Cache", ret);
	}
	return QDir::toNativeSeparators(ret);
}

QString getTmpCacheDir()
{
	static QTemporaryDir dir;
	return dir.path();
}

const QStringList getDefaultPlatforms()
{
	QStringList SupportedPlatforms = PLSSyncServerManager::instance()->getSupportedPlatformsList();
	if (SupportedPlatforms.isEmpty()) {
		return gDefaultPlatform;
	}

	return SupportedPlatforms;
}

QString channleNameConvertFixPlatformName(const QString &channleName)
{
	auto map = PLSSyncServerManager::instance()->getSupportedPlatformsMap();
	auto channelMap = map.value(channleName).toMap();
	QString platform = channelMap.value("platform").toString();
	if (platform.isEmpty()) {
		return channleName;
	}
	return platform;
}

QString NCB2BConvertChannelName(const QString &name)
{
	if (name == NCB2B) {
		auto map = PLSSyncServerManager::instance()->getSupportedPlatformsMap();
		foreach(QVariant value, map.values())
		{
			auto platform = value.toMap().value("platform").toString();
			auto serviceName = value.toMap().value("serviceName").toString();
			QString userServiceName = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName();
			if (platform == name && serviceName == userServiceName) {
				return map.key(value.toMap());
			}
		}
	}
	return name;
}

QString getNCB2BServiceName(const QString &name)
{
	auto map = PLSSyncServerManager::instance()->getSupportedPlatformsMap();
	auto channelMap = map.value(name).toMap();
	return channelMap.value("serviceName").toString();
}

QString channelNameConvertMultiLang(const QString &name)
{
	auto map = PLSSyncServerManager::instance()->getSupportedPlatformsMap();
	auto channelMap = map.value(name).toMap();

	QString multiLang;
	auto var = channelMap.value("name");
	if (var.typeId() == QMetaType::QString) {
		multiLang = var.toString();
	} else if (var.typeId() == QMetaType::QVariantMap) {
		auto nameMap = var.toMap();
		auto enLang = nameMap.value("EN").toString();
		auto lang = pls_get_current_language_short_str().toUpper();
		multiLang = nameMap.value(lang).toString();
		if (multiLang.isEmpty()) {
			multiLang = enLang;
		}
	}

	if (multiLang.isEmpty()) {
		return name;
	}
	return multiLang;
}

QStringList getChatChannelNameList(bool bAddNCPPrefix)
{
	QStringList channelList;

	auto platformsList = PLSSyncServerManager::instance()->getSupportedPlatformsList();
	for (QString chName : platformsList) {
		if (chName == CHZZK || chName == TWITCH || chName == YOUTUBE || chName == FACEBOOK || chName == NAVER_SHOPPING_LIVE || chName == NAVER_TV || chName == AFREECATV) {
			channelList.append(chName);
		}
	}
	auto map = PLSSyncServerManager::instance()->getSupportedPlatformsMap();
	foreach(QVariant value, map.values())
	{
		auto platform = value.toMap().value("platform").toString();
		if (!platform.isEmpty() && platform != CUSTOM_RTMP && platform != CUSTOM_SRT && platform != CUSTOM_RIST) {
			auto chName = map.key(value.toMap());
			if (bAddNCPPrefix) {
				chName.insert(0, "NCP_");
			}
			channelList.insert(0, chName);
		}
	}
	return channelList;
}

QString guessPlatformFromRTMP(const QString &rtmpUrl)
{
	const auto &rtmpInfos = PLSCHANNELS_API->getRTMPInfos();
	auto isUrlMatched = [&](const QString &url) { return rtmpUrl.compare(url, Qt::CaseInsensitive) == 0; };

	if (auto retIte = std::find_if(rtmpInfos.constBegin(), rtmpInfos.constEnd(), isUrlMatched); retIte != rtmpInfos.constEnd()) {
		return retIte.key();
	}
	auto apiTwitchServers = PLSLoginDataHandler::instance()->getTwitchServer();
	for (auto pair : apiTwitchServers) {
		if (rtmpUrl.contains(pair.second)) {
			return TWITCH;
		}
	}

	auto obsTwitchServers = getObsServer(TWITCH_SERVICE);
	for (auto pair : obsTwitchServers) {
		if (rtmpUrl.contains(pair.second)) {
			return TWITCH;
		}
	}

	auto obsYoutubeServers = getObsServer(YOUTUBE_RTMP);
	for (auto pair : obsYoutubeServers) {
		if (rtmpUrl.contains(pair.second)) {
			return YOUTUBE;
		}
	}

	return QString(CUSTOM_RTMP);
}

bool isPlatformOrderLessThan(const QString &left, const QString &right)
{
	auto defaultPlatforms = getDefaultPlatforms();
	auto lIndex = defaultPlatforms.indexOf(left);
	auto rIndex = defaultPlatforms.indexOf(right);
	return lIndex < rIndex;
}

QString simplifiedString(const QString &src)
{
	return src.toLower().remove(QRegularExpression("\\W+"));
}

bool isStringEqual(const QString &left, const QString &right)
{
	return simplifiedString(left) == simplifiedString(right);
}

QPropertyAnimation *createShowAnimation(QWidget *wid, int msSec)
{
	auto animation = new QPropertyAnimation(wid, "pos");

	animation->setDuration(msSec);
	animation->setStartValue(QPoint(wid->width(), 0));
	animation->setEndValue(QPoint(0, 0));
	return animation;
}

void displayWidgetWithAnimation(QWidget *wid, int msSec, bool show)
{

	if (show) {
		wid->show();
		auto animation = createShowAnimation(wid, msSec);
		animation->start(QAbstractAnimation::DeleteWhenStopped);
	} else {
		auto animation = createHideAnimation(wid);
		animation->start(QAbstractAnimation::DeleteWhenStopped);
		QWidget::connect(animation, &QPropertyAnimation::finished, wid, &QWidget::hide);
	}
}

QPropertyAnimation *createHideAnimation(QWidget *wid, int msSec)
{
	auto animation = new QPropertyAnimation(wid, "pos");

	animation->setDuration(msSec);
	animation->setStartValue(QPoint(0, 0));
	animation->setEndValue(QPoint(wid->width(), 0));
	return animation;
}

void moveWidgetToParentCenter(QWidget *wid)
{
	auto parentGeo = wid->parentWidget()->contentsRect();
	auto parentCenter = wid->parentWidget()->mapToGlobal(parentGeo.center() - QPoint(wid->frameGeometry().width() / 2, wid->frameGeometry().height() / 2) - parentGeo.topLeft());
	auto realCenter = parentCenter;

	if (wid->windowFlags() | Qt::Widget) {
		wid->move(wid->mapFromGlobal(realCenter));
	} else {
		wid->move(realCenter);
	}
}

void refreshStyle(QWidget *widget)
{
	assert(widget != nullptr);
	widget->style()->unpolish(widget);
	widget->style()->polish(widget);
}

QString getElidedText(const QWidget *widget, const QString &srcTxt, int minWidth, Qt::TextElideMode mode, int flag)
{
	if (srcTxt.isEmpty()) {
		return srcTxt;
	}
	return widget->fontMetrics().elidedText(srcTxt, mode, minWidth, flag);
}

void downloadUserImage(const QString &url, const QString &Prefix, pls::rsm::IDownloader::ResultCb &&resultCb)
{
	auto urlAndHowSave = pls::rsm::UrlAndHowSave()                      //
				     .url(url)                              //
				     .fileName(pls::rsm::FileName::FromUrl) //
				     .keyPrefix(Prefix);                    //
	pls::rsm::getDownloader()->download(urlAndHowSave, PLSCHANNELS_API, std::move(resultCb));
}

QString toPlatformCodeID(const QString &srcName, bool toKeepSRC)
{
	auto tmp = srcName;
	tmp = tmp.remove(" ").toUpper();
	const auto &rmtpNames = g_allPlatforms;
	bool isFind = false;
	for (int i = 0; i < rmtpNames.size(); ++i) {
		auto name = rmtpNames[i];
		name = name.remove(" ").toUpper();
		if (name == tmp) {
			tmp = rmtpNames[i];
			isFind = true;
			break;
		}
	}
	if (!isFind) {
		if (toKeepSRC) {
			tmp = srcName;
		} else {
			tmp = CUSTOM_RTMP;
		}
	}
	return tmp;
}

QList<QPair<QString, QString>> initTwitchServer()
{
	auto apiTwitchServerList = PLSLoginDataHandler::instance()->getTwitchServer();
	if (apiTwitchServerList.size() > 0) {
		GlobalVars::g_bUseAPIServer = true;
		return apiTwitchServerList;
	}

	GlobalVars::g_bUseAPIServer = false;
	auto obsTwitchServerList = getObsServer(TWITCH_SERVICE);
	if (obsTwitchServerList.size() > 0) {
		if (obsTwitchServerList.at(0).second == "auto") {
			obsTwitchServerList.replace(0, QPair<QString, QString>(QTStr("setting.output.server.auto"), PLSCHANNELS_API->getRTMPInfos().value(TWITCH)));
		}
		return obsTwitchServerList;
	}
	return QList<QPair<QString, QString>>{};
}

QList<QPair<QString, QString>> getObsServer(QString serviceName)
{
	QList<QPair<QString, QString>> obsServer;
	obs_properties_t *props = obs_get_service_properties("rtmp_common");
	obs_property_t *services = obs_properties_get(props, "service");

	OBSDataAutoRelease settings = obs_data_create();

	obs_data_set_string(settings, "service", serviceName.toUtf8().constData());
	obs_property_modified(services, settings);

	obs_property_t *servers = obs_properties_get(props, "server");

	size_t servers_count = obs_property_list_item_count(servers);
	for (size_t i = 0; i < servers_count; i++) {
		const char *name = obs_property_list_item_name(servers, i);
		const char *server = obs_property_list_item_string(servers, i);
		if (!pls_is_empty(name) && !pls_is_empty(server)) {
			obsServer.append(QPair<QString, QString>(name, server));
		}
	}
	obs_properties_destroy(props);
	return obsServer;
}

void ChannelsNetWorkPretestWithAlerts(const pls::http::Reply &reply, bool notify)
{
	if (!notify) {
		return;
	}

	auto errorValue = reply.error();
	auto statusCode = reply.statusCode();
	PLSErrorHandler::ExtraData exData;
	QString method = reply.request().method().constData();
	exData.urlEn = method + reply.request().originalUrl().path();

	auto retData = PLSErrorHandler::getAlertString({statusCode, errorValue, reply.data()}, CUSTOM_RTMP, "", exData);
	if (retData.prismCode == PLSErrorHandler::CHANNEL_CUSTOM_RTMP_TOKEN_EXPIRED) {
		PLSCHANNELS_API->prismTokenExpired(retData);
		return;
	}
	addErrorFromRetData(retData);
	PLSCHANNELS_API->networkInvalidOcurred();
}
