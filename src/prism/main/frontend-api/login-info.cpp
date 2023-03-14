#include "login-info.hpp"

#include "frontend-api.h"

PLSLoginInfo::PLSLoginInfo() {}
PLSLoginInfo::~PLSLoginInfo() {}

PLSLoginInfo::ChannelSupport PLSLoginInfo::channelSupport() const
{
	return PLSLoginInfo::ChannelSupport::Account;
}

PLSLoginInfo::ImplementType PLSLoginInfo::loginWithAccountImplementType() const
{
	return ImplementType::Synchronous;
}

bool PLSLoginInfo::loginWithAccount(QJsonObject &, UseFor, QWidget *) const
{
	return false;
}

void PLSLoginInfo::loginWithAccountAsync(std::function<void(bool ok, const QJsonObject &result)> callback, UseFor, QWidget *) const
{
	callback(false, QJsonObject());
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
