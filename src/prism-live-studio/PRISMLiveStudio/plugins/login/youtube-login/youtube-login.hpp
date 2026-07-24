#pragma once

#include <login-info.hpp>

class PLSYoutubeLoginInfo : public PLSLoginInfo {
public:
	PLSYoutubeLoginInfo() = default;
	~PLSYoutubeLoginInfo() override = default;

	UseFor useFor() const override;

	PLSPlatformType platform() const override;
	QString name() const override;
	QString icon(UseFor useFor) const override;

	ImplementType loginWithAccountImplementType() const override;
	void loginWithAccountAsync(const std::function<void(bool ok, const QVariantHash &)> &callback, UseFor useFor, QWidget *parent = nullptr) const override;
	PLSLoginInfo::ChannelSupport channelSupport() const override;
	QString rtmpUrl() const override;

private:
	QUrl getYoutubeLoginUrl() const;

	mutable QString m_jumpUrl;
	mutable QString m_httpServerAddress;

	Q_DISABLE_COPY(PLSYoutubeLoginInfo)
};
