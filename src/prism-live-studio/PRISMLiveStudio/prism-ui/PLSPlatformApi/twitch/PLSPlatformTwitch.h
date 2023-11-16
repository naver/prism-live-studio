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
using streamKeyCallback = std::function<void()>;
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

	QStringList getServerNames() const;

	void saveSettings(const std::string &title, const std::string &category, const std::string &categoryId, int idxServer);

	void onPrepareLive(bool value) override;

	void requestCategory(const QString &query);

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
	void saveStreamServer();
	void requestServer(bool showAlert, const streamKeyCallback &callback);
	void requestUpdateChannel(const std::string &title, const std::string &category, const std::string &categoryId);
	void requestVideos();

	static PLSPlatformApiResult getApiResult(int code, QNetworkReply::NetworkError error);
	void showApiRefreshError(PLSPlatformApiResult value);
	void showApiUpdateError(PLSPlatformApiResult value, const QString &msg = QString());

	void onAlLiveStarted(bool) override;
	void onLiveEnded() override;
	void responseServerSuccessHandler(const QJsonDocument &doc, bool showAlert, const int &code, const QByteArray &data, const streamKeyCallback &callback);

	int m_idxServer = 0;
	std::vector<TwitchServer> m_vecTwitchServers;
	std::string m_strOriginalTitle;
	QString m_strEndUrl;
};
