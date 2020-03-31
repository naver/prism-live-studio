#include "ChannelCommonFunctions.h"
#include "CommonDefine.h"
#include <QUrl>
#include <QJsonDocument>
#include <QFile>
#include <QDatastream>
#include <QDir>
#include <QCoreApplication>
#include "ChannelConst.h"
#include <QRegularExpression>
#include <QUUid>
#include "frontend-api.h"
#include "PLSChannelsVirualAPI.h"
#include "pls-net-url.hpp"

using namespace ChannelData;

QVariantMap &removePointerKey(QVariantMap &src)
{
	src.remove(g_channelWidget);
	src.remove(g_channelHandler);
	return src;
}

QString findFileInResources(const QString &dirPath, const QString &key)
{
	QDir dir(dirPath);
	QRegularExpression rex(key, QRegularExpression::CaseInsensitiveOption);
	auto filesList = dir.entryInfoList();
	for (const auto &file : filesList) {
		QString filename = file.fileName();
		auto match = rex.match(filename);
		if (match.hasMatch()) {
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
	auto widget = listWid->itemWidget(item);
	if (widget) {
		widget->deleteLater();
	}
	if (listWid) {
		listWid->removeItemWidget(item);
		listWid->takeItem(listWid->row(item));
	}

	delete item;
}

const QVariantMap createDefaultHeaderMap()
{
	QVariantMap ret;
	ret[HTTP_HEAD_CONTENT_TYPE] = HTTP_CONTENT_TYPE_URL_ENCODED_VALUE;
	QString userAgent = QString(PRISM_LIVE_STUDIO)
				    .append(SLASH)
				    .append("1.0version") //to be done get version
				    .append(PRISM_BUILD)
				    .append("")
				    .append(PRISM_ARCHITECTURE)
				    .append("")
				    .append(PRISM_LANGUAGE)
				    .append("EN_US"); //to be done get language

	ret[HTTP_USER_AGENT] = userAgent;

	return ret;
}

const QVariantMap createDefaultUrlMap()
{
	QVariantMap m_urlParamMap;
	m_urlParamMap[HTTP_DEVICE_ID] = QUrl::toPercentEncoding(getHostMacAddress());
	m_urlParamMap[HTTP_GCC] = "gcc";

	m_urlParamMap[HTTP_VERSION] = "1";

	m_urlParamMap[HTTP_WITH_ACTIVITY_INFO] = STATUS_FALSE;

	return m_urlParamMap;
}

static long order = 100;
QVariantMap createDefaultChannelInfoMap(const QString &channelName, int defaultType)
{
	QVariantMap channelInfo;
	QString uuid = createUUID();
	channelInfo.insert(g_channelUUID, uuid);
	channelInfo.insert(g_channelName, channelName);
	channelInfo.insert(g_nickName, channelName);
	channelInfo.insert(g_userName, channelName);
	channelInfo.insert(g_data_type, defaultType);
	channelInfo.insert(g_channelStatus, InValid);
	channelInfo.insert(g_channelUserStatus, Disabled);
	channelInfo.insert(g_createTime, QDateTime::currentDateTime());
	channelInfo.insert(g_isUpdated, false);

	int index = -1;
	if (defaultType == ChannelType) {
		auto defaultPlatforms = getDefaultPlatforms();
		index = defaultPlatforms.indexOf(channelName);
	} else if (defaultType == RTMPType) {
		channelInfo.insert(g_userID, "");
		channelInfo.insert(g_password, "");
		channelInfo.insert(g_channelName, RTMPT_DEFAULT_TYPE);
	}
	channelInfo.insert(g_displayOrder, index);
	return channelInfo;
}

const QString createUUID()
{
	return QUuid::createUuid().toString();
}
const QString getPlatformImageFromName(const QString &channelName, const QString &prefix, const QString &surfix)
{
	auto searchKey = prefix + channelName + surfix;
	searchKey.remove(" ");
	auto platformIcon = findFileInResources(":/Images/skin", searchKey);
	if (platformIcon.isEmpty()) {
		platformIcon = g_defualtPlatformIcon;
	}
	return platformIcon;
}

QString getYoutubeShareUrl(const QString &broadCastID)
{
	QString url(g_youtubeUrl);
	url += "/watch?v=";
	url += broadCastID;
	return url;
}

const QString getHostMacAddress()
{
	QList<QNetworkInterface> nets = QNetworkInterface::allInterfaces();
	QString strMacAddr = "";
	for (int i = 0; i < nets.count(); i++) {
		if (nets[i].flags().testFlag(QNetworkInterface::IsUp) && nets[i].flags().testFlag(QNetworkInterface::IsRunning) && !nets[i].flags().testFlag(QNetworkInterface::IsLoopBack)) {
			strMacAddr = nets[i].hardwareAddress();
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

void getComplexImageOfChannel(const QString &uuid, QString &userIcon, QString &platformIcon, const QString &prefix, const QString &surfix)
{
	const auto &srcData = PLSCHANNELS_API->getChanelInfoRef(uuid);

	QString platformName = getInfo(srcData, g_channelName, QString());
	if (srcData.isEmpty()) {
		return;
	}
	int type = getInfo(srcData, g_data_type, NoType);
	if (type == RTMPType) {
		userIcon = getPlatformImageFromName(platformName);
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

QDataStream &operator<<(QDataStream &out, const ChannelDataHandlerPtr &)
{
	out << "ChannelDataHandlerPtr";
	return out;
}

QDataStream &operator<<(QDataStream &out, ChannelDataType type)
{
	out << char(type);
	return out;
}

int getReplyStatusCode(ReplyPtrs reply)
{
	return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}

void requestStartLog(const QString &url, const QString &requestType)
{
	PLS_INFO("Channels", "%s", QString("http request start:%1, url = " + url + ".").arg(requestType).toStdString().c_str());
}

void formatNetworkLogs(ReplyPtrs reply, const QByteArray &data)
{
	int code = 0;
	auto doc = QJsonDocument::fromJson(data);
	if (!doc.isNull()) {
		auto obj = doc.object();
		if (!obj.isEmpty()) {
			code = obj.value("code").toInt();
		}
	}
	QString msg = reply->url().toString() + " statusCode = " + QString::number(getReplyStatusCode(reply)) + " code = " + QString::number(code) + ".";
	if (code != QNetworkReply::NoError) {
		msg.prepend("http request error! url = ");
		msg += "\n error string :" + reply->errorString();
		PLS_ERROR("Channels", "%s", msg.toStdString().c_str());
		PRE_LOG_MSG(("reply content :" + reply->readAll()).constData(), INFO);
	} else {
		msg.prepend("http request successfull! url = ");
		PLS_INFO("Channels", "%s", msg.toStdString().c_str());
	}
}

void ChannelsNetWorkPretestWithAlerts(ReplyPtrs reply, const QByteArray &data, bool notify)
{
	formatNetworkLogs(reply, data);
	if (!notify) {
		return;
	}
	QVariantMap errormap;
	auto errorValue = reply->error();
	if (errorValue <= QNetworkReply::UnknownNetworkError || errorValue == QNetworkReply::UnknownServerError) {
		errormap.insert(g_errorTitle, CHANNELS_TR(Check.Alert.Title));
		errormap.insert(g_errorString, CHANNELS_TR(Check.Network.Error));

	} else {
		return;
	}
	PLSCHANNELS_API->addError(errormap);
	PLSCHANNELS_API->networkInvalidOcurred();
}

const QString getChannelCacheFilePath()
{
	return getChannelCacheDir() + "/" + g_channelCacheFile;
}

const QString getChannelCacheDir()
{
	QString ret = pls_get_user_path(QCoreApplication::applicationName() + QDir::separator() + "Cache");
	QDir dir(ret);
	if (!dir.exists(ret)) {
		dir.mkpath(ret);
	}
	return ret;
}

const QString getChannelSettingsFilePath()
{
	return getChannelCacheDir() + "/" + g_channelSettingsFile;
}

const QStringList getDefaultPlatforms()
{
	return gDefaultPlatform;
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
	//qDebug() << " geo " << wid->parentWidget()->frameGeometry();
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

QString getElidedText(QWidget *widget, const QString &srcTxt, double minWidth, Qt::TextElideMode mode, int flag)
{
	return widget->fontMetrics().elidedText(srcTxt, mode, minWidth, flag);
}

void setStyleSheetFromFile(const QString &fileStr, QWidget *wid)
{
	QFile file(fileStr);
	if (file.open(QIODevice::ReadOnly)) {
		wid->setStyleSheet(file.readAll());
	}
}
