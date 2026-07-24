#pragma once

#include <QObject>
#include <libhttp-client.h>
#include <vector>
#include "PLSDateFormate.h"
#include <QFormLayout>
#include <QLabel>
#include "PLSLabel.h"
#include <QTextLayout>
class PLSDialogView;

enum class PLSPlatformApiResult;
class PLSAPICommon : public QObject {
	Q_OBJECT
public:
	enum class RefreshType {
		NotRefresh = 0,
		CheckRefresh,
		ForceRefresh,
	};
	enum class PLSApiType { Normal = 0, StartLive = 1, Update = 2, Rehearsal = 3, UploadImage = 4 };

	using dataCallback = std::function<void(QByteArray data)>;
	using errorCallback = std::function<void(int code, QByteArray data, QNetworkReply::NetworkError error)>;
	using imageCallback = std::function<void(bool ok, const QString &imagePath)>;
	using uploadImageCallback = std::function<void(bool isOK, const QString &imageUrl)>;
	using privacyVec = std::vector<std::pair<QString /*english*/, QString /*localized*/>>;

	static void downloadImageAsync(const QObject *receiver, const QString &imageUrl, const imageCallback &callback, bool ignoreCache = false, const QString &savePath = {});
	static void maskingUrlKeys(const pls::http::Request &_request, const QStringList &keys);
	static void maskingAfterUrlKeys(const pls::http::Request &_request, const QStringList &keys);
	static QString maskingUrlKeys(const QString &originUrl, const QStringList &keys);

	static bool isTokenValid(long timestamp);

	static QString getMd5ImagePath(const QString &url);

	static QString getPairedString(const PLSAPICommon::privacyVec &pairs, const QString cmpStr, bool isCmpFirst, bool isCaseInsensitive = true);
	static void downloadChannelImageAsync(const QString &platformName);

	template<typename T> static void sortScheduleListsByCustom(std::vector<T> &datas)
	{
		sort(datas.begin(), datas.end(), [](const T &lhs, const T &rhs) { return lhs.timeStamp > rhs.timeStamp; });
		std::vector<T> lists = datas;
		datas.clear();

		auto nowTime = PLSDateFormate::getNowTimeStamp();
		for (const auto &info : lists) {
			auto scheduleTime = info.timeStamp;
			bool expired = nowTime > scheduleTime;
			if (expired) {
				datas.push_back(info);
			} else {
				datas.insert(datas.begin(), info);
			}
		}
	};

	template<typename T> inline static bool getErrorCallBack(const QByteArray &data, T &refObj, QString &errorMsg, const QString &key)
	{
		auto doc = QJsonDocument::fromJson(data);
		if (!doc.isObject()) {
			errorMsg = " doc is not object";
			return false;
		}
		auto obj = doc.object();
		if (!obj.contains(key)) {
			errorMsg = key + " does not exist";
			return false;
		}

		QJsonValue valJson = obj[key];
		bool isIgnoreEmpty = false;
		T val;
		if constexpr (std::is_same_v<QString, T>) {
			val = valJson.toString();
		} else if constexpr (std::is_same_v<QJsonObject, T>) {
			val = valJson.toObject();
		} else if constexpr (std::is_same_v<QJsonArray, T>) {
			val = valJson.toArray();
			isIgnoreEmpty = true;
		} else {
			qDebug() << "not support this type, please add it";
			assert(false);
		}
		if (val.isEmpty() && !isIgnoreEmpty) {
			errorMsg = key + " is empty";
			return false;
		}
		refObj = val;
		return true;
	};

	static void createHelpIconWidget(QLabel *originalLabel, const QString &tooltip, QFormLayout *formLayout, PLSDialogView *sender);

private:
	enum class PLSNetMehod {
		PUT = 0,
		POST,
		GET,
	};
};
