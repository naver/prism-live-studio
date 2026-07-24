#pragma once

#include <login-info.hpp>

class PLSFacebookLoginInfo : public PLSLoginInfo {
public:
	PLSFacebookLoginInfo() = default;
	~PLSFacebookLoginInfo() override = default;

	UseFor useFor() const override;

	PLSPlatformType platform() const override;
	QString name() const override;
	QString icon(UseFor useFor) const override;
	ImplementType loginWithAccountImplementType() const override;
	void loginWithAccountAsync(const std::function<void(bool ok, const QVariantHash &)> &callback, UseFor useFor, QWidget *parent = nullptr) const override;
	PLSLoginInfo::ChannelSupport channelSupport() const override;
	QString rtmpUrl() const override;

private:
	QUrl getFacebookChannelUrl(const QString &redirectUri, const QString &state) const;

	Q_DISABLE_COPY(PLSFacebookLoginInfo)
};
