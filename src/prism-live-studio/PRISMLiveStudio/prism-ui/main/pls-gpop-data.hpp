#ifndef PLSGPOPDATA_H
#define PLSGPOPDATA_H

#include <optional>
#include <QObject>

#include "pls-gpop-data-struct.hpp"
#include "json-data-handler.hpp"

class PLSGpopData : public QObject {
	Q_OBJECT

public:
	static PLSGpopData *instance();
	void getGpopData(const QByteArray &gpopData);

	//use these functions ,you should set your json file in 'DefaultSources.qrc' under path ':/Configs/DefaultResources'
	static QByteArray getDefaultValuesOf(const QString &key);
	template<typename DestType> static void useDefaultValues(const QString &key, DestType &dest)
	{
		auto data = getDefaultValuesOf(key);
		PLSJsonDataHandler::jsonTo(data, dest);
	}

	//public:
	Common getCommon();
	QMap<QString, SnsCallbackUrl> getSnscallbackUrls();
	QMap<QString, SnsCallbackUrl> getDefaultSnscallbackUrls();
	Connection getConnection();

	int getUIBlockingTimeS() const;

	int getYoutubeHealthStatusInterval() const;

private:
	void initDefaultValues();
	void initCommon();
	void initSnscallbackUrls();

	//private:
	explicit PLSGpopData(QObject *parent = nullptr);
	~PLSGpopData() override = default;

private slots:

	void initGpopData();

private:
	QByteArray m_gpopDataArray;
	Common m_common;
	QMap<QString, SnsCallbackUrl> m_snsCallbackUrls;
	QMap<QString, SnsCallbackUrl> m_defaultCallbackUrls;;

	Connection m_connection;

	int m_iMultiplePlatformMaxBitrate = 0;

	int camera_auto_restart_times = 3;
	int uiBlockingTimeS = 10;
};

#endif // PLSGPOPDATA_H