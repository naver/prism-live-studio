/*
* @file		PLSPlatformTwitch.h
* @brief	All twitch relevant api is implemented in this file
* @date		2020-01-06
*/

#pragma once

#include <vector>
#include <functional>
#include <QTimer>
#include "../PLSPlatformBase.hpp"
#include <QStringList>
#include "PLSErrorHandler.h"

using streamKeyCallback = std::function<void(bool isSuccess)>;
using refreshTokenCallback = std::function<void(bool isRefreshok)>;
class PLSPlatformTwitch : public PLSPlatformBase {
	Q_OBJECT

	struct TwitchServer {
		int _id;
		std::string name;
		std::string url_template;
		bool _default;
	};

public:
	PLSPlatformTwitch() = default;

	PLSServiceType getServiceType() const override;

	int getServerIndex() const;
	void setServerIndex(int idxServer);

	void onPrepareLive(bool value) override;

	bool isSendChatToMqtt() const override { return true; }

	QJsonObject getLiveStartParams() override;
	QJsonObject getWebChatParams() override;
	QString getServiceLiveLink() override;
	QString getShareUrl() override;
	QString getShareUrlEnc() override;
	QString getServiceLiveLinkEnc() override;
	void requestStreamKey(bool showAlert, const streamKeyCallback &callback);
	void getChannelInfo(const std::function<void(bool)> &channelInfoCallback);
	void pollingCheckToken(bool isFoceUpdate = false, const refreshTokenCallback &callback = nullptr);

protected:
	void onResumeStreaming(const QMap<QString, QVariant> &params) override;
	QMap<QString, QVariant> getResumeStreamingParams() const override;

signals:
	void onGetChannel(PLSPlatformApiResult);
	void onGetServer(PLSPlatformApiResult);
	void onUpdateChannel(PLSPlatformApiResult);
	void onGetCategory(QJsonObject content, const QString &request);
	void closeDialogByExpired();

private:
	QVariantMap setHttpHead() const;
	void requestVideos();

	void showApiRefreshError(const PLSErrorHandler::RetData &retData);
	void showApiUpdateError(PLSPlatformApiResult value, const QString &msg = QString());

	void onAlLiveStarted(bool) override;
	void onLiveEnded() override;
	void serverHandler();

	std::string m_strOriginalTitle;
	QString m_strEndUrl;
};