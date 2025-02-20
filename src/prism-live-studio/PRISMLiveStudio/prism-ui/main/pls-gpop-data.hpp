#ifndef PLSGPOPDATA_H
#define PLSGPOPDATA_H

#include <optional>
#include <QObject>

#include "pls-gpop-data-struct.hpp"

class PLSGpopData : public QObject {
	Q_OBJECT

public:
	static PLSGpopData *instance();
	void getGpopData(const QByteArray &gpopData);

	//public:
	Common getCommon();
	QMap<QString, SnsCallbackUrl> getSnscallbackUrls();
	QMap<QString, SnsCallbackUrl> getDefaultSnscallbackUrls();
	const QStringList &getChannelList();
	const QStringList &getChannelResolutionGuidList();
	const QStringList &getLoginList() const;
	Connection getConnection();

	int getUIBlockingTimeS() const;

	int getYoutubeHealthStatusInterval() const;

private:
	void initDefaultValues();
	void initCommon();
	void initSnscallbackUrls();
	void initSupportedPlatforms();

	//private:
	explicit PLSGpopData(QObject *parent = nullptr);
	~PLSGpopData() override = default;

private slots:

	void initGpopData();

private:
	QByteArray m_gpopDataArray;
	Common m_common;
	QMap<QString, SnsCallbackUrl> m_snsCallbackUrls;
	QMap<QString, SnsCallbackUrl> m_defaultCallbackUrls;
	QStringList m_channelList;
	QStringList m_channelResolutionGuidList;
	QStringList m_loginList;
	QStringList m_defaultChannelList;
	QStringList m_defaultChannelResolutionGuidList;
	QStringList m_defaultLoginList;
	Connection m_connection;

	int m_iMultiplePlatformMaxBitrate = 0;

	int camera_auto_restart_times = 3;
	int uiBlockingTimeS = 10;
};

#endif // PLSGPOPDATA_H