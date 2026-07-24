#pragma once

#include <login-info.hpp>
#include <QVector>
#include <QVariantMap>
#include <qobject.h>
#include "libbrowser.h"
#include "PLSErrorHandler.h"

class PLSChzzkLoginInfo : public QObject, public PLSLoginInfo {
	Q_OBJECT
public:
	PLSChzzkLoginInfo() = default;
	~PLSChzzkLoginInfo() override = default;

	UseFor useFor() const override;

	PLSPlatformType platform() const override;
	QString name() const override;
	QString icon(UseFor useFor) const override;

	bool loginWithAccount(QVariantHash &result, UseFor useFor, QWidget *parent = nullptr) const override;
	PLSLoginInfo::ChannelSupport channelSupport() const override;
	QString rtmpUrl() const override;
	void getChzzkUserId(const QString &accessToken, QEventLoop &loop) const;
	void getChzzkChannelList(const QString &accessToken, QEventLoop &loop) const;

private:
	PLSErrorHandler::RetData getErrorRetData(const pls::http::Reply &reply) const;
	bool errorHandler(const PLSErrorHandler::RetData &retData) const;
	static QString getCookies(const QMap<QString, QString> &cookies);
	static void removeChzzkCookies();

	QString m_jumpUrl;
	mutable bool m_isShowTerm = false;
	QString m_jsCode;
	mutable QPointer<pls::browser::BrowserDialog> m_dialog;
	mutable QString m_cookie;
	mutable PLSErrorHandler::RetData m_retData;
	Q_DISABLE_COPY(PLSChzzkLoginInfo)
};
