#pragma once

#include <login-info.hpp>
#include <qvector.h>

class PLSBandLoginInfo : public PLSLoginInfo {
public:
	PLSBandLoginInfo();
	~PLSBandLoginInfo() override = default;

	UseFor useFor() const override;

	PLSPlatformType platform() const override;
	QString name() const override;
	QString icon(UseFor useFor) const override;

	bool loginWithAccount(QVariantHash &result, UseFor useFor, QWidget *parent = nullptr) const override;
	PLSLoginInfo::ChannelSupport channelSupport() const override;
	QString rtmpUrl() const override;

private:
	QUrl getBandLoginUrl() const;
	void saveUrls(const QString &url);

	QString m_jumpUrl;
	QVector<QString> m_urls;
	//Q_DISABLE_COPY(PLSBandLoginInfo)
};
