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

#include "pls-channel-const.h"

constexpr auto QRCPATH = ":/channels/resource";
using namespace ChannelData;

IMPL_ENUM_SERIALIZATION(ChannelData::ChannelStatus);
IMPL_ENUM_SERIALIZATION(ChannelData::ChannelUserStatus);
IMPL_ENUM_SERIALIZATION(ChannelData::ChannelDataType);
IMPL_ENUM_SERIALIZATION(ChannelData::LiveState);
IMPL_ENUM_SERIALIZATION(ChannelData::RecordState);

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

	return platformName;
}

bool isVersionLessthan(const QString &leftVer, const QString &rightVer)
{
	return QVersionNumber::fromString(leftVer) < QVersionNumber::fromString(rightVer);
}

ChannelsMap getMatchKeysInfos(const QVariantMap &keysMap)
{
	ChannelsMap ret;
	auto isMatched = [&](const QVariantMap &info) {
		for (auto srcIte = keysMap.cbegin(); srcIte != keysMap.cend(); ++srcIte) {
			if (srcIte.value() != getInfo(info, srcIte.key(), QVariant())) {
				return false;
			}
		}
		return true;
	};

	const auto &allInfos = PLSCHANNELS_API->getAllChannelInfoReference();
	for (auto ite = allInfos.cbegin(); ite != allInfos.cend(); ++ite) {
		const auto &info = ite.value();
		if (isMatched(info)) {
			ret.insert(ite.key(), info);
		}
	}

	return ret;
}

QVariantMap createDefaultChannelInfoMap(const QString &platformName, int defaultType)
{
	QVariantMap channelInfo;
	QString uuid = createUUID();
	channelInfo.insert(g_channelUUID, uuid);
	channelInfo.insert(g_platformName, platformName);

	channelInfo.insert(g_nickName, platformName);
	channelInfo.insert(g_userName, platformName);
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
		index = defaultPlatforms.indexOf(platformName);
	} else if (defaultType >= CustomType) {
		channelInfo.insert(g_rtmpUserID, "");
		channelInfo.insert(g_password, "");
		channelInfo.insert(g_platformName, RTMPT_DEFAULT_TYPE);
	}
	channelInfo.insert(g_displayOrder, index);
	return channelInfo;
}

QString createUUID()
{
	return QUuid::createUuid().toString();
}
QString getPlatformImageFromName(const QString &channelName, const QString &prefix, const QString &surfix)
{
	auto searchKey = prefix + channelName + surfix;
	searchKey.remove(" ");
	return findFileInResources(QRCPATH, searchKey);
}

QString getYoutubeShareUrl(const QString &broadCastID)
{
	QString url(g_youtubeUrl);
	url += "/watch?v=";
	url += broadCastID;
	return url;
}

QString getHostMacAddress()
{
	QList<QNetworkInterface> nets = QNetworkInterface::allInterfaces();
	QString strMacAddr = "";
	auto netCount = nets.count();
	for (int i = 0; i < netCount; i++) {
		auto add = nets[i];
		auto flags = add.flags();
		if (flags.testFlag(QNetworkInterface::IsUp) && flags.testFlag(QNetworkInterface::IsRunning) && !flags.testFlag(QNetworkInterface::IsLoopBack)) {
			strMacAddr = add.hardwareAddress();
			break;
		}
	}
	return strMacAddr;
}

bool writeFile(const QByteArray &array, const QString &path)
{
	QFile file(path);
	if (!file.open(QFile::WriteOnly | QIODevice::Text)) {
		return false;
	}

	file.write(array);
	file.close();
	return true;
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

void getComplexImageOfChannel(const QString &uuid, QString &userIcon, QString &platformIcon, const QString &prefix, const QString &surfix)
{
	const auto &srcData = PLSCHANNELS_API->getChanelInfoRef(uuid);

	QString platformName = getInfo(srcData, g_platformName, QString());
	if (srcData.isEmpty()) {
		return;
	}
	int type = getInfo(srcData, g_data_type, NoType);
	if (type >= CustomType) {
		userIcon = getPlatformImageFromName(platformName);
		if (userIcon.isEmpty()) {
			userIcon = g_defualtPlatformIcon;
		}
		platformIcon = g_defualtPlatformSmallIcon;
	} else {
		userIcon = getInfo(srcData, g_userIconCachePath, g_defaultHeaderIcon);
		platformIcon = getPlatformImageFromName(platformName, prefix, surfix);
	}
}

void formatJson(QByteArray &array)
{
	auto document = QJsonDocument::fromJson(array);
	array = document.toJson(QJsonDocument::Indented);
}

void requestStartLog(const QString &url, const QString &requestType)
{
	PLS_INFO("Channels", "%s", QString("http request start:%1, url = " + url + ".").arg(requestType).toStdString().c_str());
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

QString getChannelSettingsFilePath()
{
	return getChannelCacheDir() + QDir::separator() + g_channelSettingsFile;
}

const QStringList &getDefaultPlatforms()
{
	return gDefaultPlatform;
}

QString guessPlatformFromRTMP(const QString &rtmpUrl)
{
	const auto &rtmpInfos = PLSCHANNELS_API->getRTMPInfos();
	auto isUrlMatched = [&](const QString &url) { return rtmpUrl.compare(url, Qt::CaseInsensitive) == 0; };

	if (auto retIte = std::find_if(rtmpInfos.constBegin(), rtmpInfos.constEnd(), isUrlMatched); retIte != rtmpInfos.constEnd()) {
		return retIte.key();
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

void setStyleSheetFromFile(const QString &fileStr, QWidget *wid)
{
	QFile file(fileStr);
	if (file.open(QIODevice::ReadOnly)) {
		wid->setStyleSheet(file.readAll());
	}
}

bool isCacheFileMatchCurrentLang()
{
	static bool isMacth = false;
	if (isMacth) {
		return true;
	}

	auto fileStr = getChannelCacheDir() + "/transFlag.txt";

	if (QFile file(fileStr); file.open(QIODevice::ReadWrite)) {

		if (QString txt = file.readLine(); !pls_is_match_current_language(txt)) {
			file.seek(0);
			file.write(pls_get_current_language().toUtf8());
			file.write("\n");
			return false;
		}
		isMacth = true;
	}
	return isMacth;
}

void translateSVGText(const QString &srcPath, const QString &txt, const QString &resultTxt)
{
	QFile file(srcPath);
	QFile output(getChannelCacheDir() + "/" + QFileInfo(srcPath).fileName());
	if (file.open(QIODevice::ReadOnly) && output.open(QIODevice::WriteOnly)) {
		QTextStream in(&file);
		QTextStream out(&output);

		while (!in.atEnd()) {
			auto line = in.readLine();
			line.replace(txt, resultTxt);
			out << line;
		}
	}
}

void downloadUserImage(const QString &url, const QString &dirPath, const QString &Prefix, const std::function<void(const pls::http::Reply &reply)> &finish)
{
	pls::http::Request request;
	request.url(url);
	request.forDownload(true);
	request.withLog(pls_masking_person_info(url));
	if (!Prefix.isEmpty()) {
		request.saveFileNamePrefix(Prefix);
	}

	request.saveDir(dirPath);
	request.receiver(PLSCHANNELS_API);
	request.timeout(PRISM_NET_DOWNLOAD_TIMEOUT);
	request.method(pls::http::Method::Get);

	request.okResult(finish);

	pls::http::request(request);
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

QVariantMap queryStringToMap(const QString &srcStr)
{
	QUrlQuery query(srcStr);
	QVariantMap varMap;
	auto items = query.queryItems();
	for (const auto &item : items) {
		varMap.insert(item.first, item.second);
	}
	return varMap;
}
