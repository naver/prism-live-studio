#pragma once

#include <login-info.hpp>

class PLSNCB2BLoginInfo : public PLSLoginInfo {
public:
	PLSNCB2BLoginInfo() = default;
	~PLSNCB2BLoginInfo() override = default;

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
	QString getOauthTokenFromUrl(const QString &urlStr) const;

	QString m_jumpUrl;
	Q_DISABLE_COPY(PLSNCB2BLoginInfo)
};
