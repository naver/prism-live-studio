#include "login-info.hpp"

#include "frontend-api.h"

PLSLoginInfo::ChannelSupport PLSLoginInfo::channelSupport() const
{
	return PLSLoginInfo::ChannelSupport::Account;
}

PLSLoginInfo::ImplementType PLSLoginInfo::loginWithAccountImplementType() const
{
	return ImplementType::Synchronous;
}

bool PLSLoginInfo::loginWithAccount(QVariantHash &result, UseFor, QWidget *) const
{
	return false;
}

void PLSLoginInfo::loginWithAccountAsync(const std::function<void(bool ok, const QVariantHash &result)> &callback, UseFor, QWidget *) const
{
	callback(false, {});
}

QString PLSLoginInfo::rtmpUrl() const
{
	return QString();
}

bool PLSLoginInfo::rtmpView(QJsonObject &result, UseFor useFor, QWidget *parent) const
{
	Q_UNUSED(useFor)

	ChannelSupport cs = channelSupport();
	if ((cs == ChannelSupport::Rtmp) || (cs == ChannelSupport::Both)) {
		return pls_rtmp_view(result, const_cast<PLSLoginInfo *>(this), parent);
	}
	return false;
}
