#pragma once

#include <login-info.hpp>
#include <qvector.h>

class PLSNaverShoppingLiveLoginInfo : public PLSLoginInfo {
public:
	PLSNaverShoppingLiveLoginInfo() = default;
	~PLSNaverShoppingLiveLoginInfo() override = default;

	UseFor useFor() const override;
	PLSPlatformType platform() const override;
	QString name() const override;
	QString icon(UseFor useFor) const override;
	bool loginWithAccount(QVariantHash &result, UseFor useFor, QWidget *parent = nullptr) const override;
	PLSResultCheckingResult naverShoppingWebCallback(const QString &url, QVariantHash &l_result, bool &loginAgain, const QMap<QString, QString> &cookies) const;
	PLSLoginInfo::ChannelSupport channelSupport() const override;
	QString rtmpUrl() const override;

private:
	static QString getCookies(const QMap<QString, QString> &cookies);
};
