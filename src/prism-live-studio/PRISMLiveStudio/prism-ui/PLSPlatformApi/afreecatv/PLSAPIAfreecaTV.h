#pragma once

#include <QObject>
#include <libhttp-client.h>
#include "../common/PLSAPICommon.h"

struct PLSAfreecaTVLiveinfoData;
struct PLSAfreecaTVCategory;

class PLSAPIAfreecaTV : public QObject {
	Q_OBJECT

public:
	explicit PLSAPIAfreecaTV(QObject *parent = nullptr);
	static void configDefaultRequest(const pls::http::Request &_request, const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed,
					 bool isForceKo = false);

	static void requestDashboradData(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed);
	static void requestUsersInfoAndChannel(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed);
	static void requestUsersNickName(const QString &userID, const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed);
	static void requestCategoryList(const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed);
	static void updateLiveInfo(const QObject *receiver, const QString &title, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed);
	static void readDataByRegu(PLSAfreecaTVLiveinfoData &data, const QString &originStr = "");
	static void parseCategory(const QString &originStr);
	static QString getSelectCategoryString(const QString &selectID);
	static void requestCheckIsOnline(const QString &userID, const QObject *receiver, const PLSAPICommon::dataCallback &onSucceed, const PLSAPICommon::errorCallback &onFailed);
	static bool getIsRemoteOnline(const QString &originStr, int &oId);

private:
	static void addCommonData(const pls::http::Request &builder, bool forceKo = false);

	static QString getFirstReg(const QString &htmlStr, const QString &regStr);
	static QString getValue(const QString &originStr, const QString &keyStr = "value");
	static QString getValueOfList(const QString &originStr);
	static QString getValueOfTags(const QString &originStr);
	static QString getValueOfFrmByeBye(const QString &originStr);
	static QString getServerUrlValue(const QString &originStr);
	static bool getIsCheck(const QString &htmlStr, const QString &getIdStr);

	static void recursiveConvertCategory(const QJsonArray &categories, std::vector<PLSAfreecaTVCategory> &recieveData);
	static bool recursiveFindSelectStr(const QString &selectID, QStringList &allLevel, const std::vector<PLSAfreecaTVCategory> &recieveData);

	static QString decodeHtmlContent(const QString &str);
};
