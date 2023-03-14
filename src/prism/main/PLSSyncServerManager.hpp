
#ifndef PLSSYNCSERVERMANAGER_H
#define PLSSYNCSERVERMANAGER_H

#include <QObject>
#include <QMap>

#define Watermark_StartUpMs QStringLiteral("startUpMs")
#define Watermark_IntervalMs QStringLiteral("intervalMs")
#define Watermark_PeriodicMs QStringLiteral("periodicMs")
#define Watermark_AnimationMs QStringLiteral("animationMs")
#define Watermark_ResourceUrl QStringLiteral("resourceUrl")
#define Watermark_ResourcePath QStringLiteral("resourcePath")
#define Watermark_Schema QStringLiteral("schema")
#define Watermark_RightMarginRatio QStringLiteral("rightMarginRatio")
#define Watermark_BottomMarginRatio QStringLiteral("bottomMarginRatio")
#define Watermark_ScaleRatio QStringLiteral("scaleRatio")
#define Watermark_CanOnOff QStringLiteral("canOnOff")
#define Outro_ResourceLand QStringLiteral("resource_land")
#define Outro_ResourcePortrait QStringLiteral("resource_portrait")
#define Outro_ResourceTimeoutMs QStringLiteral("timeoutMs")
#define Outro_ResourceLandPath QStringLiteral("resourceLandPath")
#define Outro_ResourcePortraitPath QStringLiteral("resourcePortraitPath")

#define DOWNLOAD_OUTRO_FILE_PATH QStringLiteral("PRISMLiveStudio/user/library_Policy_PC/")

class PLSSyncServerManager : public QObject {
	Q_OBJECT
public:
	static PLSSyncServerManager *instance();
	const QMap<QString, QVariantMap> &outroMap();
	const QMap<QString, QVariantMap> &watermarkMap();
	const QVariantMap &platformFPSMap();
	const QVariantList &getResolutionsList();
	const QMap<QString, QVariantList> &getStickerReaction();
	bool initWatermark(const QString &path);
	bool initOutroPolicy(const QString &path);

private:
	explicit PLSSyncServerManager(QObject *parent = nullptr);
	bool initSupportedResolutionFPS();
	QString getPolicyFileNameByURL(const QString &url);
	QString getPolicyFilePathByURL(const QString &url);
	bool isExistPolicyFileByURL(const QString &url);
	void appendMap(QVariantMap &destMap, const QVariantMap &srcMap);

private:
	QVariantMap m_platformFPSMap;
	QMap<QString, QVariantMap> m_watermarkMap;
	QMap<QString, QVariantMap> m_outroMap;
	QVariantList m_resolutionsInfos;
	QMap<QString, QVariantList> m_reaction;
};

#endif
