#pragma once
#include <qobject.h>
#include <vector>
#include "../PLSPlatformBase.hpp"
#include "PLSChannelDataHandler.h"

struct PLSAfreecaTVLiveinfoData {
	QString work;
	QString frmCategoryID;
	QString frmCategoryStr;
	QString is_wait;
	QString frmWait;
	bool b_showFrmWait = false;
	QString waiting_time;
	QString frmTitle;
	QString frmWaitTime;
	QString frmViewer;
	QString frmWaterMark;
	QString frmHashTags;
	QString frmByeBye;
	QString encode_type = "normal"; //normal || vr
	QString frmStreamKey;
	QString frmServerUrl;
	QString _id;
	bool b_frmAdult = false;
	QString frmAdult;
	bool b_frmHidden = false;
	QString frmHidden;
	bool b_frmTuneOut = false;
	QString frmTuneOut;
	bool b_containFrmAccess = false; //which is only showen in ko
	QString frmAccess;
	QString frmAccessCode;
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
	void requestDashborad(const std::function<void(bool)> &onNext, const QObject *receiver);
	void dealRequestDashborad(const QByteArray &data, const std::function<void(bool)> &onNext, const QObject *receiver);
	void requestUserNickName(const QVariantMap &srcInfo, const UpdateCallback &finishedCall) const;
	void dealUserNickNameSucceed(const QVariantMap &srcInfo, const QByteArray &data, QList<QVariantMap> &dstInfos) const;
	void saveSettings(const std::function<void(bool)> &onNext, const QString &title);
	bool onMQTTMessage(PLSPlatformMqttTopic top, const QJsonObject &jsonObject) override;

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
	void showApiUpdateError(const QString &customErrName, int code = 0, QByteArray data = {}, QNetworkReply::NetworkError error = QNetworkReply::NoError);

	void requestCategories(const std::function<void(bool)> &onNext, const QObject *receiver);
	void updateLiveinfo(const std::function<void(bool)> &onNext, const QObject *receiver, const QString &title);

	void dealUpdateLiveinfoSucceed(const QByteArray &data, const std::function<void(bool)> &onNext);

	void onLiveEnded() override;
	void onAlLiveStarted(bool) override;

	void setSelectData(const PLSAfreecaTVLiveinfoData &data);
};
