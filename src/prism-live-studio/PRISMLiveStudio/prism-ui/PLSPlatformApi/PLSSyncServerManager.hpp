
#ifndef PLSSYNCSERVERMANAGER_H
#define PLSSYNCSERVERMANAGER_H

#include <QObject>
#include <QMap>
#include <qjsonarray.h>
#include "pls-gpop-data-struct.hpp"
#include <QJsonObject>
#include <optional>

class PLSSyncServerManager : public QObject {
	Q_OBJECT

public:
	static PLSSyncServerManager *instance();
	explicit PLSSyncServerManager(QObject *parent = nullptr);
	void updatePolicyPublishByteArray();
	void updateSupportedPlatforms();
	void updateChatTagIcon();

private:
	bool getSyncServerAppBundleJsonObject(const QString &jsonName, QJsonObject &appBundleJsonObject, int &appBundleVersion);
	bool getSyncServerUserFolderJsonObject(const QString &jsonName, QJsonObject &userFolderJsonObject, int &userFolderVersion);
	void getSyncServerJsonObject(const QString &jsonName, QJsonObject &jsonObject);
	bool initSupportedResolutionFPS(const QJsonObject &policyPublishJsonObject);
	void initRtmpDestination(const QJsonObject &policyPublishJsonObject);
	void initMultiplePlatformMaxBitrate(const QJsonObject &policyPublishJsonObject);
	void initNaverPlatformWhiteList(const QJsonObject &policyPublishJsonObject);
	void initStreamServiceList(const QJsonObject &policyPublishJsonObject);
	void initPlatformLiveTimeLimit(const QJsonObject &policyPublishJsonObject);
	void initPlatformVersionInfo(const QJsonObject &policyPublishJsonObject);
	void initRemoteControlInfo(const QJsonObject &policyPublishJsonObject);
	void initNaverShoppingInfo(const QJsonObject &policyPlatformJsonObject);
	void initOpenSourceInfo();
	void initTwitchWhipServer(const QJsonObject &policyPlatformJsonObject);
	void initSupportedPlatforms(const QJsonObject &policyPublishJsonObject);
	void initWaterMark(const QJsonObject &waterMarkDefaultValueObject);
	void initOutroPolicy(const QJsonObject &outroDefaultValueObject);
	void initDiscordInfo(const QJsonObject &policyPublishJsonObject);
	void initPlusUrl(const QJsonObject &policyPublishJsonObject);

public:
	const QVariantList &getResolutionsList();
	const QMap<QString, QVariantList> &getStickerReaction();
	QVariantMap getSupportedResolutionFPSMap();
	QVariantMap getLivePlatformResolutionFPSMap();
	const QVariantMap &getRtmpPlatformFPSMap();
	QMap<int, RtmpDestination> getRtmpDestination();
	int getMultiplePlatformMaxBitrate();
	QStringList getNaverPlatformWhiteList();
	const QVariantMap &getStreamService();
	PlatformLiveTime getPlatformLiveTime(bool isDirect, const QString &platformName);
	QString getRemoteControlMobilePlatform(const QString &platformName);
	const QVariantMap &getPlatformVersionInfo();
	bool isPresetRTMP(const QString &url);
	const QString &getNaverShoppingTermOfUse();
	const QString &getNaverShoppingOperationPolicy();
	const QString &getNaverShoppingNotes();
	const QString &getVoluntaryReviewProducts();
	const QString &getNoticeOnAutomaticExtractionOfProductSections();
	const QJsonArray &getProductCategoryJsonArray();
	std::vector<std::string> getGPUBlacklist(const QString &jsonName);
	const QString &getOpenSourceLicense();
	const QString &getTwitchWhipServer();
	const QStringList &getSupportedPlatformsList();
	const QVariantMap &getSupportedPlatformsMap();
	const QVariantList &getNewResolutionGuide();
	const QJsonObject &getLoginObject();
	const QJsonObject &getWaterMarkConfigObject();
	const QJsonObject &getOutroPolicyConfigObject();
	const QString getDiscordUrl();
	const QString getPlusUrl();

	const QString &getWaterMarkResLocalPath(const QString &platformName);
	const QVariantMap &getOutroResLocalPathAndText(const QString &platformName);
	int compareVersion(const QString &v1, const QString &v2) const;

signals:
	void libraryNeedUpdate(bool isSuccess);

private slots:
	void onReceiveLibraryNeedUpdate(bool isSucceed);

private:
	QJsonObject m_policyPublishDefaultValueObject;
	QJsonObject m_policyPlatformDefaultValueObject;
	QJsonObject m_waterMarkDefaultValueObject;
	QJsonObject m_outroDefaultValueObject;
	QVariantMap m_platformFPSMap;
	QVariantList m_resolutionsInfos;
	QMap<QString, QVariantList> m_reaction;
	QJsonArray m_productCategoryJsonArray;
	std::optional<int> m_iMultiplePlatformMaxBitrate;
	QStringList m_naverPlatformWhiteList;
	QVariantMap m_streamService;
	QVariantMap m_platformLiveTimeLimit;
	QVariantMap m_remoteControlPlatformsInfo;
	QVariantMap m_platformVersionInfo;
	QMap<int, RtmpDestination> m_destinations;
	QVariantMap m_rtmpFPSMap;
	QString m_navershoppingOperationPolicy;
	QString m_navershoppingTermofUse;
	QString m_navershoppingNotes;
	QString m_voluntaryReviewProducts;
	QString m_noticeOnAutomaticExtractionOfProductSections;
	QString m_openSourceLicense;
	QString m_twitchWhipServer;
	QVariantMap m_supportedPlatformsMap;
	QStringList m_supportedPlatformsList;
	QJsonObject m_loginObject;
	QJsonObject m_channelObject;
	QVariantList m_newResolutionGuide;
	QJsonObject m_watermarkObject;
	QJsonObject m_outroObject;
	QString m_watermarkLocalPath;
	QVariantMap m_outroPathAndText;
	QString m_strDiscordUrl;
	QString m_plusUrl;
};

#define PLS_SYNC_SERVER_MANAGE PLSSyncServerManager::instance()

#endif