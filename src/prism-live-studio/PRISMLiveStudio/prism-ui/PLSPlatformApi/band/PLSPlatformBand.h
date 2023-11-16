#ifndef PLSPLATFORMBAND_H
#define PLSPLATFORMBAND_H

#include <functional>
#include <QJsonDocument>

#include "../PLSPlatformBase.hpp"
#include "../channels/ChannelsDataApi/PLSChannelDataHandler.h"

constexpr auto MODULE_PLATFORM_BAND = "Platform/Band";

using refreshTokenCallback = std::function<void(bool isRefreshok)>;
using streamLiveKeyCallback = std::function<void(int value)>;
using requesetLiveEndCallback = std::function<void()>;

class PLSPlatformBand : public PLSPlatformBase {
public:
	~PLSPlatformBand() override = default;

	void initLiveInfo(bool isUpdate) const;
	void clearBandInfos();
	void getBandRefreshTokenInfo(const refreshTokenCallback &callback = nullptr, bool isForceUpdate = false);
	void getBandTokenInfo(const QVariantMap &srcInfo, const UpdateCallback &finishedCall);
	void getBandCategoryInfo(const QVariantMap &srcInfo, const UpdateCallback &finishedCall);
	void getChannelEmblemAsync() const;
	bool onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject) override;

	PLSServiceType getServiceType() const override;
	void onPrepareLive(bool value) override;
	QString getShareUrl() override;
	QString getLiveId() const;
	int getMaxLiveTime() const;
	void requestLiveStreamKey(const streamLiveKeyCallback &);
	QJsonObject getLiveStartParams() override;

private:
	void onAlLiveStarted(bool) override;
	void onAllPrepareLive(bool isOk) override;
	void onLiveEnded() override;

	void requesetLiveEnd(const requesetLiveEndCallback &callback);
	void setMaxLiveTime(const int &min);
	void setLiveId(const QString &liveId);

private slots:
	PLSPlatformApiResult getApiResult(int code, QNetworkReply::NetworkError error) const;
	void showApiRefreshError(PLSPlatformApiResult value) const;
	void showChannelInfoError(PLSPlatformApiResult value);
	void responseRefreshTokenHandler(const refreshTokenCallback &callbackfunc, const QByteArray &data, int code);
	void responseTokenHandler(const UpdateCallback &finishedCall, const QByteArray &data, int code);
	template<typename finshedCallFun> void responseBandCategoryHandler(const finshedCallFun &finishedCall, const QByteArray &data, int code);
	template<typename responseCallbackFunc> void responseStreamLiveKeyHandler(const responseCallbackFunc &callback, const QByteArray &data, int code);

private:
	int m_maxLiveTime = 0;
	QString m_liveId;
	QList<QVariantMap> m_bandInfos;
	QVariantMap m_bandLoginInfo;
	bool m_isRequestStart = false;
};

#endif // PLSBANDDATAHANDLER_H
