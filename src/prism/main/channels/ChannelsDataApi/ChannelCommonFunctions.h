#ifndef CHANELCOMMONFUNCTION_H
#define CHANELCOMMONFUNCTION_H

#include <QMap>
#include <QString>
#include <QVariantMap>
#include <QListWidget>
#include <QListWidgetItem>
#include <QNetworkInterface>
#include <QJsonDocument>
#include "ChannelConst.h"
#include "PLSChannelDataHandler.h"
#include "ChannelDefines.h"
#include <QBitmap>
#include <QPixmap>
#include <QPainter>
#include <QDir>
#include <QDesktopServices>
#include <QPropertyAnimation>
#include "LogPredefine.h"
#include "alert-view.hpp"
#include "frontend-api.h"
#include "NetWorkCommonDefines.h"

/* function for get and Type value of any QvariantMap */
template<typename RetType = QString> inline auto getInfo(const QVariantMap &source, const QString &key, const RetType &defaultData = RetType()) -> RetType
{
	return source.value(key, QVariant::fromValue(defaultData)).value<RetType>();
}

/* function for get and Type value of any QvariantMap */
template<typename RetType = QString> inline auto takeInfo(QVariantMap &source, const QString &key, const RetType &defaultData = RetType()) -> RetType
{
	auto ite = source.find(key);
	if (ite == source.end()) {
		return defaultData;
	}
	auto retValue = ite->value<RetType>();
	source.erase(ite);
	return retValue;
}

template<typename RetType> inline RetType getInfoOfObject(const QObject *obj, const char *key, const RetType &defaultData = RetType())
{
	auto variantV = obj->property(key);
	if (!variantV.isValid()) {
		return defaultData;
	}
	return variantV.value<RetType>();
}
QVariantMap &removePointerKey(QVariantMap &src);

/* to find matched key file in dir path recursive */
QString findFileInResources(const QString &dirPath, const QString &key);

/* delete widget function for using with smart pointer */
template<typename WidgetType = QWidget> void deleteChannelWidget(WidgetType *channelWid)
{
	channelWid->deleteLater();
	return;
}

/* delete list item function for using with smart pointer */
void deleteItem(QListWidgetItem *item);

/* convert pointer to qobeject */
template<typename SRCType> inline QObject *convertToObejct(SRCType *srcPt)
{
	return dynamic_cast<QObject *>(srcPt);
}

/*create default prism rule header data for http */
const QVariantMap createDefaultHeaderMap();

/*create default prism rule url data for http */
const QVariantMap createDefaultUrlMap();

QVariantMap createDefaultChannelInfoMap(const QString &channelName, int defaultType = ChannelData::ChannelType);
const QString createUUID();
const QString getPlatformImageFromName(const QString &channelName, const QString &prefix = "", const QString &surfix = ".*profile");
void getComplexImageOfChannel(const QString &uuid, QString &userIcon, QString &platformIcon, const QString &prefix = "stremsetting-", const QString &surfix = "");
QString getYoutubeShareUrl(const QString &broadCastID);

/*get local machine info */
const QString getHostMacAddress();

/*write json array to file */
bool writeFile(const QByteArray &array, const QString &path);

/* to create a circle mask for user header */
template<typename SourceType> QBitmap createImageCircleMask(const SourceType &source)
{

	auto size = source.size();
	double radius = (qMin(size.height(), size.width())) / 2.0;
	if (radius < 10) {
		radius = 10;
		size.setHeight(radius);
		size.setWidth(radius);
	}

	QBitmap mask(size);
	mask.fill(Qt::white);
	QPainter painter;
	painter.begin(&mask);
	QPen tmpPen(Qt::black, 0.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	QBrush brush(Qt::black, Qt::SolidPattern);
	painter.setPen(tmpPen);
	painter.setBrush(brush);
	painter.setRenderHint(QPainter::Antialiasing);

	QPointF center(size.width() / 2.0, size.height() / 2.0);
	painter.translate(center);
	painter.drawEllipse(QPointF(0, 0), radius, radius);

	painter.end();
	return mask;
}
template<typename SourceType> SourceType &circleMaskImage(SourceType &source)
{
	auto mask = createImageCircleMask(source);
	if (mask.size() == source.size()) {
		source.setMask(mask);
	}
	return source;
}
template<typename SourceType> QBitmap createImageRoundRectMask(const SourceType &source, const SourceType &sourceInner)
{
	auto size = source.size();
	QBitmap mask(size);
	mask.fill(Qt::white);
	QPainter painter;
	painter.begin(&mask);
	QPen tmpPen(Qt::black, 0.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
	QBrush brush(Qt::black, Qt::SolidPattern);
	painter.setPen(tmpPen);
	painter.setBrush(brush);
	painter.setClipping(true);
	painter.setRenderHint(QPainter::Antialiasing);
	auto actRect = sourceInner.adjusted(2, 2, -5, -4);
	painter.drawRoundedRect(actRect, actRect.height() / 2.0 + 0.5, actRect.height() / 2.0 + 3.5);
	painter.end();
	return mask;
}

/* format json array and write to file */
template<typename DataType> bool writeJsonFile(const DataType &array, const QString &path)
{
	QJsonDocument document(array);
	return writeFile(document.toJson(QJsonDocument::Indented), path);
}

/* format json array */
void formatJson(QByteArray &array);

/* add srcmap to destmap */
template<typename MapType> auto addToMap(MapType &destMap, const MapType &srcMap) -> MapType
{
	auto srcIte = srcMap.cbegin();
	for (; srcIte != srcMap.cend(); ++srcIte) {
		destMap.insert(srcIte.key(), srcIte.value());
	}
	return destMap;
}

/* add srcmap to destmap */
template<typename MapType> auto addToMap(MapType &destMap, const MapType &srcMap, const MapType &mapper) -> MapType
{
	auto srcIte = srcMap.cbegin();
	for (; srcIte != srcMap.cend(); ++srcIte) {
		if (mapper.contains(srcIte.key())) {
			const auto &tagetKey = mapper[srcIte.key()].toString();
			destMap.insert(tagetKey, srcIte.value());
		}
	}
	return destMap;
}

QDataStream &operator<<(QDataStream &out, const ChannelDataHandlerPtr &);

QDataStream &operator<<(QDataStream &out, ChannelData::ChannelDataType type);

template<typename DataType> bool saveDataXToFile(const DataType &srcData, const QString &path)
{
	QFile file(path);
	if (file.open(QIODevice::WriteOnly)) {
		QDataStream out(&file);
		out.setVersion(QDataStream::Qt_5_12);
		out << srcData;
		return true;
	}
	return false;
}

template<typename DataType> bool loadDataFromFile(DataType &destData, const QString &path)
{
	QFile file(path);
	if (file.open(QIODevice::ReadOnly)) {
		QDataStream in(&file);
		in.setVersion(QDataStream::Qt_5_12);
		in >> destData;
		return true;
	}
	return false;
}

using HoldOnPf = void (PLSChannelDataAPI::*)(bool);

template<typename FunctionType, typename ClassPtr = PLSChannelDataAPI *> struct HolderReleaser {
	HolderReleaser(FunctionType function, ClassPtr objPtr = PLSCHANNELS_API) : mpf(function), mObjPtr(objPtr) { (*mObjPtr.*mpf)(true); }
	~HolderReleaser() { (*mObjPtr.*mpf)(false); }
	FunctionType mpf;
	ClassPtr mObjPtr;
};

struct SemaphoreHolder {
	SemaphoreHolder(QSemaphore &src) : mSem(src) { mSem.release(1); }
	~SemaphoreHolder() { mSem.acquire(1); }

private:
	QSemaphore &mSem;
};

int getReplyStatusCode(ReplyPtrs reply);

void requestStartLog(const QString &url, const QString &requestType);
void formatNetworkLogs(ReplyPtrs reply, const QByteArray &data);
void ChannelsNetWorkPretestWithAlerts(ReplyPtrs reply, const QByteArray &data, bool notify = true);

const QString getChannelCacheFilePath();
const QString getChannelCacheDir();
const QString getChannelSettingsFilePath();
const QStringList getDefaultPlatforms();

QPropertyAnimation *createShowAnimation(QWidget *wid, int msSec = 250);
void displayWidgetWithAnimation(QWidget *wid, int msSec = 250, bool show = true);
QPropertyAnimation *createHideAnimation(QWidget *wid, int msSec = 250);

void moveWidgetToParentCenter(QWidget *wid);

/* repolish any widget when you change some property of widget*/
void refreshStyle(QWidget *widget);

/*get elide text of widget*/
QString getElidedText(QWidget *widget, const QString &srcTxt, double minWidth, Qt::TextElideMode mode = Qt::ElideRight, int flag = 0);

/*set a single css of widget */
void setStyleSheetFromFile(const QString &fileStr, QWidget *wid);

/* set  css of multi file for wid */
template<typename... Args> void setStyleSheetFromFile(QWidget *wid, Args... args)
{
	QStringList files{args...};
	if (!files.isEmpty()) {
		QString styleSteets;
		for (const QString &fileStr : files) {
			QFile file(fileStr);
			if (file.open(QIODevice::ReadOnly)) {
				styleSteets.append(file.readAll() + "\n");
			}
		}
		wid->setStyleSheet(styleSteets);
	}
}

#define ADD_CHANNELS_H(str) "Channels." #str

#ifdef QT_DEBUG
#define REGISTERSTR(str) PLSCHANNELS_API->RegisterStr(ADD_CHANNELS_H(str))
#else
#define REGISTERSTR(str) 0
#endif

#define CHANNELS_TR(str) (REGISTERSTR(str), QObject::tr(ADD_CHANNELS_H(str)))

#ifdef QT_DEBUG

/*for debug view json or map data */
#define ViewJsonByteArray(arrayData)                               \
	{                                                          \
		QString path = QDir::homePath() + "/tmpTest.json"; \
		writeFile(arrayData, path);                        \
		QDesktopServices::openUrl(path);                   \
	}

#define ViewMapData(MapData)                                        \
	{                                                           \
		auto jsonDoc = QJsonDocument::fromVariant(MapData); \
		auto arrayData = jsonDoc.toJson();                  \
		ViewJsonByteArray(arrayData);                       \
	}

#define ViewJsonDoc(JsonDoc)                       \
	{                                          \
		auto arrayData = JsonDoc.toJson(); \
		ViewJsonByteArray(arrayData);      \
	}

#else

#define ViewMapData(MapData)
#define ViewJsonDoc(JsonDoc)

#endif // DEBUG

#endif //CHANELCOMMONFUNCTION_H
