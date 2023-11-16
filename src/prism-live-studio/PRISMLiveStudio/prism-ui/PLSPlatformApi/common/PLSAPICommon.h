#pragma once

#include <QObject>
#include <libhttp-client.h>

enum class PLSPlatformApiResult;
class PLSAPICommon : public QObject {
	Q_OBJECT
public:
	using dataCallback = std::function<void(QByteArray data)>;
	using errorCallback = std::function<void(int code, QByteArray data, QNetworkReply::NetworkError error)>;
	using imageCallback = std::function<void(bool ok, const QString &imagePath)>;
	using uploadImageCallback = std::function<void(PLSPlatformApiResult result, const QString &imageUrl)>;

	static QPair<bool, QString> downloadImageSync(const QObject *receive, const QString &url);
	static void downloadImageAsync(const QObject *receiver, const QString &imageUrl, const imageCallback &callback, bool ignoreCache = false);
	static void maskingUrlKeys(const pls::http::Request &_request, const QStringList &keys);
	static void maskingAfterUrlKeys(const pls::http::Request &_request, const QStringList &keys);
	static QString maskingUrlKeys(const QString &originUrl, const QStringList &keys);

private:
	enum class PLSNetMehod {
		PUT = 0,
		POST,
		GET,
	};
};