#ifndef GIPHYWEBHANDLER_H
#define GIPHYWEBHANDLER_H

#include <QObject>
#include <QQueue>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMutex>
#include <QThread>
#include <QTimer>
#include "GiphyDefine.h"
#include "libhttp-client.h"

class GiphyWebHandler : public QObject {
	Q_OBJECT

public:
	explicit GiphyWebHandler(QObject *parent = nullptr);
	~GiphyWebHandler() = default;

	static bool isHttpRedirect(int statusCode);
	void Get(const RequestTaskData &task);
	void DiscardTask();

private:
	bool DealResponData(const pls::http::Reply &reply, ResponData &data) const;
	void GetImageInfo(const QJsonObject &imgObj, GiphyData &data) const;
	void Append(const RequestTaskData &task);

public slots:
	void GiphyFetch(const RequestTaskData &task);
	void ClearTask();

private slots:
	void StartNextRequest();
	void RequestFinished(const pls::http::Reply &reply);

signals:
	void FetchResult(const ResponData &data);
	void FetchError(const RequestTaskData &task, const RequestErrorInfo &errorInfo);
	void LoadingVisible(const RequestTaskData &task, bool visible);

private:
	QQueue<std::pair<QUrl, RequestTaskData>> requestQueue;
	std::pair<QUrl, RequestTaskData> currentRequestTask;
	QMutex taskMutex;
};

#endif // GIPHYWEBHANDLER_H
