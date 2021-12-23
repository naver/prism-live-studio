#ifndef PLSGPOPDATA_H
#define PLSGPOPDATA_H

#include <QObject>

#include "network-access-manager.hpp"
#include "pls-net-url.hpp"
#include "pls-gpop-data-struct.hpp"

class PLSBasic;

class PLSGpopData : public QObject {
	Q_OBJECT

public:
	static PLSGpopData *instance();
	void getGpopDataRequest();

	//use these functions ,you should set your json file in 'DefaultSources.qrc' under path ':/Configs/DefaultResources'
	static const QByteArray getDefaultValuesOf(const QString &key);
	template<typename DestType> static void useDefaultValues(const QString &key, DestType &dest)
	{
		auto data = getDefaultValuesOf(key);
		PLSJsonDataHandler::jsonTo(data, dest);
	}

public:
	Common getCommon();
	QMap<QString, SnsCallbackUrl> getSnscallbackUrls();
	QMap<int, RtmpDestination> getRtmpDestination();
	Connection getConnection();
	const QVariantMap &rtmpFPSMap();
	QMap<QString, QString> &getOpenSourceLicenseMap();
	VliveNotice getVliveNotice();
	BlackList getBlackList();
	bool getH265opened();

	QMap<QString, QString> &getNaverShoppingTermOfUse();
	QMap<QString, QString> &getNaverShoppingNotes();
	int getMultiplePlatformMaxBitrate();

	const QVariantMap &getPlatformVersionInfo();

	int getCameraRestartTimes();
	float getDropNetworkFramePrecentThreshold();
	float getDropRenderingFramePrecentThreshold();
	float getDropEncodingFramePrecentThreshold();
	int getBufferedDurationMs();
	int getUIBlockingTimeS();
	QStringList getNaverPlatformWhiteList();
	bool isPresetRTMP(const QString &url);

private:
	void getGpopDataFromLocal();
	void initCommon();
	void initSnscallbackUrls();
	void initOpenLicenseUrl();
	void initRtmpDestination();
	void initConnection();
	void initVliveNotice();
	void initBlackList();
	void initH265Param();
	void initWGCParam();
	void initCameraRestartTimes();
	void initFrameDropPercentThreshold();
	void initUIBlockingTimeS();

	void initNaverShoppingTermOfUse();
	void initNaverShoppingNotes();
	void initMultiplePlatformMaxBitrate();
	void initPlatformVersionInfo();
	void initNaverPlatformWhiteList();

private:
	explicit PLSGpopData(QObject *parent = nullptr);
	~PLSGpopData();
	void getGpopDataLog(const QByteArray &data, const QString &version);

private slots:

	void initGpopData();
	/**
     * @brief the reponse of http request
     */
	void onReplyResultData(int statusCode, const QString &url, const QByteArray array);
	/**
     * @brief the error reponse of http request
     */
	void onReplyErrorData(int statusCode, const QString &url, const QString &body, const QString &errorInfo);

signals:
	void finished();
	void initGpopDataFinished();
	void initH265Finished();
	void initCameraRestartTimesFinished();

private:
	QByteArray m_gpopDataArray;
	QString m_libraryUrl;
	Common m_common;
	QMap<QString, SnsCallbackUrl> m_snsCallbackUrls;
	QMap<int, RtmpDestination> m_destinations;
	Connection m_connection;
	QVariantMap m_rtmpFPSMap;
	QMap<QString, QString> m_openSourceLicenseMap;
	QString m_gpopURL;
	VliveNotice m_vliveNotice;
	BlackList m_blackList;

	QMap<QString, QString> m_navershoppingTermofUse;
	QMap<QString, QString> m_navershoppingNotes;
	int m_iMultiplePlatformMaxBitrate = 0;
	bool h265opened = false;
	QVariantMap m_platformVersionInfo;
	QStringList m_naverPlatformWhiteList;

	int camera_auto_restart_times = 3;
	float dropNetworkFramePrecentThreshold = 0.5;
	float dropRenderingFramePrecentThreshold = 0.5;
	float dropEncodingFramePrecentThreshold = 0.5;
	int bufferedDurationMs = 3000;
	int uiBlockingTimeS = 5;
};

#endif // PLSGPOPDATA_H
