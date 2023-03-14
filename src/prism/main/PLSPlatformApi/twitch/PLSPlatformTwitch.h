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
#include "PLSHttpApi\PLSHttpHelper.h"
#include "..\PLSPlatformBase.hpp"

using namespace std;

class PLSPlatformTwitch : public PLSPlatformBase {
	Q_OBJECT

	struct TwitchServer {
		int _id;
		string name;
		string url_template;
		bool _default;
	};

public:
	PLSPlatformTwitch();

	PLSServiceType getServiceType() const override;

	int getServerIndex() const;
	void setServerIndex(int idxServer);

	vector<string> getServerNames() const;

	void saveSettings(const string &title, const string &category, const string &categoryId, int idxServer);

	void onPrepareLive(bool value) override;

	void requestChannel(bool showAlert);
	void requestCategory(const QString &query);

	bool isSendChatToMqtt() const override { return true; }

	QJsonObject getLiveStartParams() override;
	QJsonObject getWebChatParams() override;
	QString getServiceLiveLink() override;
	virtual QString getShareUrl() override;
	virtual QString getShareUrlEnc() override;
	virtual QString getServiceLiveLinkEnc() override;
signals:
	void onGetChannel(PLSPlatformApiResult);
	void onGetServer(PLSPlatformApiResult);
	void onUpdateChannel(PLSPlatformApiResult);
	void onGetCategory(QJsonObject content, const QString &request);
	void closeDialogByExpired();

private:
	void setHttpHead(PLSNetworkReplyBuilder &builder);
	void saveStreamServer();
	void requestStreamKey(bool showAlert);
	void requestServer(bool showAlert);
	void requestUpdateChannel(const string &title, const string &category, const string &categoryId);
	void requestStatisticsInfo();
	void requestVideos();

	static PLSPlatformApiResult getApiResult(int code, QNetworkReply::NetworkError error);
	void showApiRefreshError(PLSPlatformApiResult value);
	void showApiUpdateError(PLSPlatformApiResult value);

	void onAlLiveStarted(bool) override;
	void onLiveEnded() override;

	int m_idxServer;
	vector<TwitchServer> m_vecTwitchServers;
	string m_strOriginalTitle;
	QTimer m_statusTimer;
	QString m_strEndUrl;
};
