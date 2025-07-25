#pragma once
#include <qobject.h>
#include <vector>
#include "../PLSPlatformBase.hpp"
#include "PLSChannelDataHandler.h"

struct PLSAfreecaTVLiveinfoData {

	QString user_id;

	bool broad_pwd_chk{false}; //is contain password (from  requestMainHtml)
	QString access_code;       //password  (from  requestMainHtml)

	QString categoryID; //from "category": "00360030",
	QString categoryStr;
	QString title;

	QString rtmp_server;
	QString stremKey; //jixxx17-161xxx061   (from  requestMainHtml)

	//not change
	QString hashtags;
	QString broad_grade;
	QString broad_hidden;
	QString broad_tune_out;
	QString paid_promotion;
};

struct PLSAfreecaTVCategory {
	QString cate_name;
	QString cate_no;
	QString ucc_cate;
	QString display = "1";
	std::vector<PLSAfreecaTVCategory> child;
};

class PLSPlatformAfreecaTV : public PLSPlatformBase {
	Q_OBJECT

public:
	PLSPlatformAfreecaTV();
	PLSServiceType getServiceType() const override;
	void reInitLiveInfo(bool isReset);
	const PLSAfreecaTVLiveinfoData &getSelectData() const;
	void requestChannelInfo(const QVariantMap &srcInfo, const UpdateCallback &finishedCall) const;
	void dealRequestChannelInfoSucceed(const QVariantMap &srcInfo, const QByteArray &data, const UpdateCallback &finishedCall) const;
	void requestDashborad(const std::function<void(bool)> &onNext, const QObject *receiver, bool isForUpdate);
	void dealRequestDashborad(const QByteArray &data, const std::function<void(bool)> &onNext, const QObject *receiver, bool isForUpdate);
	void requestUserNickName(const QVariantMap &srcInfo, const UpdateCallback &finishedCall) const;
	void dealUserNickNameSucceed(const QVariantMap &srcInfo, const QByteArray &data, QList<QVariantMap> &dstInfos) const;
	void saveSettings(const std::function<void(bool)> &onNext, const QString &title);

	bool isSendChatToMqtt() const override { return true; }
	const std::vector<PLSAfreecaTVCategory> &getCategories() const { return m_vecCategories; }
	void setCategories(const std::vector<PLSAfreecaTVCategory> &cates) { m_vecCategories = cates; }
	QJsonObject getLiveStartParams() override;
	QJsonObject getMqttChatParams() override;
	QJsonObject getWebChatParams() override;
	bool getIsChatDisabled() const;
	void setIsChatDisabled(bool needAuth) { m_IsChatDisabled = needAuth; }

	QString getServiceLiveLink() override;
	QString getShareUrl() override;
	QString getShareUrlEnc() override;

	const QString &getFailedErr() const { return m_startFailedStr; }
	void setFailedErr(const QString &failedStr) { m_startFailedStr = failedStr; }

signals:
	void closeDialogByExpired();

private:
	PLSAfreecaTVLiveinfoData m_selectData;
	std::vector<PLSAfreecaTVCategory> m_vecCategories;
	bool m_IsChatDisabled{false};
	QString m_startFailedStr{};

	QString getShareUrl(const QString &id, bool isLiveUrl = false, bool isEnc = false) const;
	void onPrepareLive(bool value) override;
	void showApiError(const QString &url, const QString &apiName, const QString &customErrName, int code = 0, QByteArray data = {}, QNetworkReply::NetworkError error = QNetworkReply::NoError,
			  bool isShowExpired = false);

	void requestCategories(const std::function<void(bool)> &onNext, const QObject *receiver);
	void updateLiveinfo(const std::function<void(bool)> &onNext, const QObject *receiver, const QString &title);

	void dealUpdateLiveinfoSucceed(const QByteArray &data, const std::function<void(bool)> &onNext);
	void requestStreamKeyAndPassword(const std::function<void(bool)> &onNext, const QObject *receiver);

	void onLiveEnded() override;
	void onAlLiveStarted(bool) override;

	void setSelectData(const PLSAfreecaTVLiveinfoData &data);
};
