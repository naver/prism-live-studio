#pragma once

#include <login-info.hpp>

class PLSTwitchLoginInfo : public PLSLoginInfo {
public:
	PLSTwitchLoginInfo() = default;
	~PLSTwitchLoginInfo() override = default;

	UseFor useFor() const override;

	PLSPlatformType platform() const override;
	QString name() const override;
	QString icon(UseFor useFor) const override;

	bool loginWithAccount(QVariantHash &result, UseFor useFor, QWidget *parent = nullptr) const override;

	PLSLoginInfo::ChannelSupport channelSupport() const override;
	QString rtmpUrl() const override;

private:
	bool prismLoginWithAccount(const QJsonObject &result, const QWidget *parent = nullptr) const;
	bool channelLoginWithAccount(QVariantHash &result, QWidget *parent = nullptr) const;
	QUrl getTwitchLoginUrl() const;

	QString m_jumpUrl;
	Q_DISABLE_COPY(PLSTwitchLoginInfo)
};
