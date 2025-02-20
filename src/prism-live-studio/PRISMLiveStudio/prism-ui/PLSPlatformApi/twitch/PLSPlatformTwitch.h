/*
* @file		PLSPlatformTwitch.h
* @brief	All twitch relevant api is implemented in this file
* @author	wu.longyue@navercorp.com
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
	bool onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject) override;
	void requestStreamKey(bool showAlert, const streamKeyCallback &callback);
	void getChannelInfo();

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
