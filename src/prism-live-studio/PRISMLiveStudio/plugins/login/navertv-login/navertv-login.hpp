#pragma once

#include <login-info.hpp>
#include <QVector>
#include <QVariantMap>

class PLSNaverTVLoginInfo : public PLSLoginInfo {
public:
	PLSNaverTVLoginInfo() = default;
	~PLSNaverTVLoginInfo() override = default;

	UseFor useFor() const override;

	PLSPlatformType platform() const override;
	QString name() const override;
	QString icon(UseFor useFor) const override;
	bool loginWithAccount(QVariantHash &result, UseFor useFor, QWidget *parent = nullptr) const override;
	PLSLoginInfo::ChannelSupport channelSupport() const override;
	QString rtmpUrl() const override;

private:
	static QString getCookies(const QMap<QString, QString> &cookies);
	static void removeNaverTvCookies();

	QString m_jumpUrl;
	Q_DISABLE_COPY(PLSNaverTVLoginInfo)
};
