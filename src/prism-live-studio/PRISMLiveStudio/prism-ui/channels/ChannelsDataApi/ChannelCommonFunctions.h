#ifndef CHANELCOMMONFUNCTION_H
#define CHANELCOMMONFUNCTION_H

#include <qdatastream.h>
#include <qnetworkreply.h>
#include <qscrollarea.h>
#include <QBitmap>
#include <QDesktopServices>
#include <QDir>
#include <QJsonDocument>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMap>
#include <QNetworkInterface>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QSemaphore>
#include <QString>
#include <QVariantMap>
#include "ChannelDefines.h"
#include "LogPredefine.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelDataHandler.h"
#include "PLSChannelsVirualAPI.h"
#include "frontend-api.h"
#include "libhttp-client.h"
#include "libresource.h"
#include "network-state.h"
#include "pls-channel-const.h"
#include "pls-common-define.hpp"
#include "pls-shared-functions.h"

#include "PLSErrorHandler.h"

#define ADD_CHANNELS_H(str) "Channels." #str

#define CHANNELS_TR(str) QObject::tr(ADD_CHANNELS_H(str))

#define TR_VLIVE CHANNELS_TR(vlive)
#define TR_NAVER_TV CHANNELS_TR(naver_tv)
#define TR_WAV CHANNELS_TR(wav)
#define TR_BAND CHANNELS_TR(band)
#define TR_TWITCH CHANNELS_TR(twitch)
#define TR_YOUTUBE CHANNELS_TR(youtube)
#define TR_FACEBOOK CHANNELS_TR(facebook)
#define TR_WHALE_SPACE CHANNELS_TR(whale_space)
#define TR_AFREECATV CHANNELS_TR(afreeca_tv)
#define TR_NOW CHANNELS_TR(now)
#define TR_NAVER_SHOPPING_LIVE CHANNELS_TR(naver_shopping_live)
#define TR_SELECT CHANNELS_TR(select)
#define TR_CUSTOM_RTMP CHANNELS_TR(custom_rtmp)
#define TR_CHZZK CHANNELS_TR(chzzk)
#define TR_RTMPT_DEFAULT_TYPE CHANNELS_TR(rtmp_default_type_channels)

const QString PRISM_API_SYSTEM_ERROR = "025";

/***********************debug******************************/
#ifdef QT_DEBUG

/*for debug view json or map data */

template<typename DataType> void ViewJsonByteArray(const DataType &arrayData)
{
	QString path = QDir::homePath() + "/tmpTest2022.json";
	writeFile(arrayData, path);
	QDesktopServices::openUrl(path);
}

template<typename DataType> void ViewMapData(const DataType &MapData)
{
	auto jsonDoc = QJsonDocument::fromVariant(MapData);
	auto arrayData = jsonDoc.toJson();
	ViewJsonByteArray(arrayData);
}

template<typename DataType> void ViewJsonDoc(const DataType &JsonDoc)
{
	auto arrayData = JsonDoc.toJson();
	ViewJsonByteArray(arrayData);
}

#else

#define ViewJsonByteArray(arrayData)
#define ViewMapData(MapData)
#define ViewJsonDoc(JsonDoc)

#endif // DEBUG
/*********************** debug end******************************/

/* function for get and Type value of any QvariantMap */
template<typename RetType = QString> inline auto getInfo(const QVariantMap &source, const QString &key, const RetType &defaultData = RetType()) -> RetType
{
	return source.value(key, QVariant::fromValue(defaultData)).template value<RetType>();
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

/* to find matched key file in dir path recursive */
QString findFileInResources(const QString &dirPath, const QString &key);

/* delete widget function for using with smart pointer */
template<typename WidgetType = QWidget> void deleteChannelWidget(WidgetType *channelWid)
{
	channelWid->setParent(nullptr);
	channelWid->deleteLater();
	return;
}
template<typename WidgetType = QWidget, typename ParentType = QWidget> auto createWidgetWidthDeleter(ParentType parent = nullptr) -> QSharedPointer<WidgetType>
{
	return QSharedPointer<WidgetType>(new WidgetType(parent), deleteChannelWidget<WidgetType>);
}

/* delete list item function for using with smart pointer */
void deleteItem(QListWidgetItem *item);

QString translatePlatformName(const QString &platformName);

/* convert pointer to qobeject */
template<typename SRCType> inline QObject *convertToObejct(SRCType *srcPt)
{
	return dynamic_cast<QObject *>(srcPt);
}

bool isVersionLessthan(const QString &leftVer, const QString &rightVer);

QVariantMap createDefaultChannelInfoMap(const QString &channelName, int defaultType = ChannelData::ChannelType, const QString &cmdStr = QString());
QString createUUID();
QString getDynamicChannelIcon(QString &imagePath);
QString getPlatformImageFromName(const QString &channelName, int imageType, const QString &prefix = "", const QString &surfix = ".*profile");
void getComplexImageOfChannel(const QString &uuid, int imageType, QString &userIcon, QString &platformIcon, const QString &prefix = "stremsetting-", const QString &surfix = "");
QString getChatIcon(const QString &channelName, int imageType, const QString &resChatIconPath);
QString getYoutubeShareUrl(const QString &broadCastID);

void loadPixmap(QPixmap &pix, const QString &pixmapPath, const QSize &pixSize = QSize(100, 100));

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
#define DECLARE_ENUM_SERIALIZATION(EnumType)                         \
	QDataStream &operator<<(QDataStream &out, const EnumType &); \
	QDataStream &operator>>(QDataStream &in, EnumType &)

#define IMPL_ENUM_SERIALIZATION(EnumType)                              \
	QDataStream &operator<<(QDataStream &out, const EnumType &src) \
	{                                                              \
		out << int(src);                                       \
		return out;                                            \
	}                                                              \
	QDataStream &operator>>(QDataStream &in, EnumType &target)     \
	{                                                              \
		int value;                                             \
		in >> value;                                           \
		target = EnumType(value);                              \
		return in;                                             \
	}

DECLARE_ENUM_SERIALIZATION(ChannelData::ChannelStatus);
DECLARE_ENUM_SERIALIZATION(ChannelData::ChannelUserStatus);
DECLARE_ENUM_SERIALIZATION(ChannelData::ChannelDataType);
DECLARE_ENUM_SERIALIZATION(ChannelData::LiveState);
DECLARE_ENUM_SERIALIZATION(ChannelData::RecordState);
DECLARE_ENUM_SERIALIZATION(ChannelData::ChannelDualOutput);

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

template<typename FunctionType, typename ClassPtr = PLSChannelDataAPI *> struct HolderReleaser {
	explicit HolderReleaser(FunctionType function, ClassPtr objPtr = PLSCHANNELS_API) : mpf(function), mObjPtr(objPtr) { (*mObjPtr.*mpf)(true); }
	~HolderReleaser() { (*mObjPtr.*mpf)(false); }

private:
	Q_DISABLE_COPY(HolderReleaser)
	FunctionType mpf;
	ClassPtr mObjPtr;
};

struct SemaphoreHolder {
	explicit SemaphoreHolder(QSemaphore &src) : mSem(src) { mSem.release(1); }
	~SemaphoreHolder() { mSem.acquire(1); }

private:
	Q_DISABLE_COPY(SemaphoreHolder)
	QSemaphore &mSem;
};

void ChannelsNetWorkPretestWithAlerts(const pls::http::Reply &reply, bool notify = true);

QString getImageCacheFilePath();
QString getChannelCacheFilePath();
QString getChannelCacheDir();
QString getTmpCacheDir();
const QStringList getDefaultPlatforms();
QString channleNameConvertFixPlatformName(const QString &channleName); //pandaTV -> NCB2B
QString NCB2BConvertChannelName(const QString &name);                  // NCB2B -> pandaTV
QString getNCB2BServiceName(const QString &name);
QString channelNameConvertMultiLang(const QString &name);
QStringList getChatChannelNameList(bool bAddNCPPrefix = false);
QString guessPlatformFromRTMP(const QString &rtmpUrl);

bool isPlatformOrderLessThan(const QString &left, const QString &right);
QString simplifiedString(const QString &src);

bool isStringEqual(const QString &left, const QString &right);

template<typename MapType, typename FuncType>
auto findMatchKeyFromMap(const MapType &src, const typename MapType::key_type &searchKey, FuncType Func = isStringEqual) -> typename MapType::const_iterator
{
	for (auto it = src.cbegin(); it != src.cend(); ++it) {
		if (Func(it.key(), searchKey)) {
			return it;
		}
	}
	return src.cend();
}

QPropertyAnimation *createShowAnimation(QWidget *wid, int msSec = 250);
void displayWidgetWithAnimation(QWidget *wid, int msSec = 250, bool show = true);
QPropertyAnimation *createHideAnimation(QWidget *wid, int msSec = 250);

void moveWidgetToParentCenter(QWidget *wid);

/* repolish any widget when you change some property of widget*/
void refreshStyle(QWidget *widget);

/*get elide text of widget*/
QString getElidedText(const QWidget *widget, const QString &srcTxt, int minWidth, Qt::TextElideMode mode = Qt::ElideRight, int flag = 0);

template<typename ParentType = QWidget *, typename ChildType = QWidget *> auto findParent(ChildType child) -> ParentType
{
	auto tmpP = child->parent();
	if (tmpP == nullptr) {
		return nullptr;
	}
	auto retP = dynamic_cast<ParentType>(tmpP);
	if (retP == nullptr) {
		return findParent<ParentType>(tmpP);
	}
	return retP;
}

template<typename ParentType = QScrollArea *, typename ChildType = QWidget *> bool isWidgetInView(ChildType wid)
{
	auto parent = findParent<ParentType>(wid);
	if (parent) {
		auto contentRec = parent->contentsRect();
		auto center = wid->contentsRect().center();
		center = wid->mapTo(parent, center);
		return contentRec.contains(center);
	}

	return true;
}

template<typename DestType, typename FunctionType> auto getMemberPointer(FunctionType func) -> DestType
{
	return *static_cast<DestType *>((reinterpret_cast<void *>(&func)));
}

template<typename FunType> ChannelData::TaskFun wrapFun(FunType &fun)
{
	return [fun](const QVariant &) { fun(); };
}

#define ADD_TASK_AUTO_NAME(taskMap)                                                                         \
	taskMap.insert(ChannelTransactionsKeys::g_taskName, QString(__FILE__) + QString::number(__LINE__)); \
	PLSCHANNELS_API->addTask(taskMap);

void downloadUserImage(const QString &url, const QString &Prefix = QString(), pls::rsm::IDownloader::ResultCb &&resultCb = nullptr);
QString toPlatformCodeID(const QString &srcName, bool toKeepSRC = false);

QList<QPair<QString, QString>> initTwitchServer();
QList<QPair<QString, QString>> getObsServer(QString serviceName);
#endif //CHANELCOMMONFUNCTION_H
