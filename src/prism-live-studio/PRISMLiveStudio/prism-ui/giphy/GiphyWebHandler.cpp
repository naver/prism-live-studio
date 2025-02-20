#include "GiphyWebHandler.h"
#include "log/module_names.h"
#include "liblog.h"
#include "utils-api.h"
#include "libutils-api.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonArray>
#include <QEventLoop>

constexpr auto TIME_OUT_MS = 10 * 1000;
constexpr auto SHOW_LOADING_TIME_OUT_MS = 500;

GiphyWebHandler::GiphyWebHandler(QObject *parent) : QObject(parent) {}

void GiphyWebHandler::GiphyFetch(const RequestTaskData &task)
{
#ifdef DEBUG
	qDebug() << "GiphyWebHandler: requst from " << task.url;
#endif // DEBUG

	Append(task);
}

bool GiphyWebHandler::isHttpRedirect(int statusCode)
{
	return statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 305 || statusCode == 307 || statusCode == 308;
}

void GiphyWebHandler::Get(const RequestTaskData &task)
{
	QMutexLocker locker(&taskMutex);
	if (task.url != currentRequestTask.second.url)
		QMetaObject::invokeMethod(this, "GiphyFetch", Qt::QueuedConnection, Q_ARG(const RequestTaskData &, task));
}

void GiphyWebHandler::DiscardTask()
{
	QMetaObject::invokeMethod(this, "ClearTask", Qt::QueuedConnection);
}

bool GiphyWebHandler::DealResponData(const pls::http::Reply &reply, ResponData &responData) const
{
	QJsonObject obj;
	QJsonDocument document;
	QJsonParseError error;
	QJsonArray arrayData;
	QByteArray byteArray = reply.data();
	document = QJsonDocument::fromJson(byteArray, &error);

	if (QJsonParseError::NoError != error.error)
		return false;

	obj = document.object();
	if (!obj.contains("meta"))
		return false;

	QJsonObject meta = obj.value("meta").toObject();
	if (!meta.contains("status"))
		return false;

	int status = meta.value("status").toInt();
	responData.metaData.status = status;
	responData.metaData.msg = meta.value("msg").toString();
	responData.metaData.responId = meta.value("response_id").toString();
	if (status != 200) {
		return true;
	}

	if (obj.contains("pagination")) {
		QJsonObject pageObj = obj.value("pagination").toObject();
		responData.pageData.totalCount = pageObj.value("total_count").toInt();
		responData.pageData.count = pageObj.value("count").toInt();
		responData.pageData.offset = pageObj.value("offset").toInt();
	}

	if (obj.contains("data") && obj.value("data").isArray()) {
		arrayData = obj.value("data").toArray();
		auto iter = arrayData.constBegin();
		while (iter != arrayData.constEnd()) {
			GiphyData data;
			QJsonObject objData = (*iter).toObject();
			data.id = objData.value("id").toString();
			data.type = objData.value("type").toString();
			data.title = objData.value("title").toString();
			data.rating = objData.value("rating").toString();
			QJsonObject imageObj = objData.value("images").toObject();
			GetImageInfo(imageObj, data);
			responData.giphyData.push_back(data);
			iter++;
		}
	}

	return true;
}

void GiphyWebHandler::GetImageInfo(const QJsonObject &imageObj, GiphyData &data) const
{
	// some GIFs don’t have every property available,so we should check which property should be used.
	QJsonObject obj_preview = imageObj.value("fixed_width_downsampled").toObject();
	if (obj_preview.isEmpty())
		obj_preview = imageObj.value("preview_gif").toObject();
	if (obj_preview.isEmpty())
		obj_preview = imageObj.value("fixed_width_still").toObject();

	data.previewUrl = obj_preview.value("url").toString();
	data.sizePreview = QSize(obj_preview.value("width").toString().toInt(), obj_preview.value("height").toString().toInt());

	// Get the original info.
	QJsonObject obj_original = imageObj.value("original").toObject();
	data.originalUrl = obj_original.value("url").toString();
	qDebug() << "obj_original: " << obj_original;
	data.sizeOriginal = QSize(obj_original.value("width").toString().toInt(), obj_original.value("height").toString().toInt());
}

void GiphyWebHandler::Append(const RequestTaskData &task)
{
	if (requestQueue.isEmpty())
		pls_async_call(this, [this]() { StartNextRequest(); });

	QUrl requestUrl = QUrl::fromEncoded(task.url.toUtf8());

	// refuse consequent same request.
	if (!requestQueue.isEmpty()) {
		if (task.url != requestQueue.head().second.url)
			requestQueue.enqueue(std::make_pair(requestUrl, task));
	} else
		requestQueue.enqueue(std::make_pair(requestUrl, task));
}

void GiphyWebHandler::ClearTask()
{
	requestQueue.clear();
}

void GiphyWebHandler::StartNextRequest()
{
	taskMutex.lock();
	currentRequestTask.second.url = "";
	taskMutex.unlock();

	if (requestQueue.isEmpty())
		return;

	auto task = requestQueue.dequeue();
	const auto task_data = task.second;
	taskMutex.lock();
	currentRequestTask = task;
	taskMutex.unlock();

	QUrl url = task.first;

	emit LoadingVisible(task_data, true);
	pls::http::request(pls::http::Request()
				   .timeout(TIME_OUT_MS)           //
				   .method(pls::http::Method::Get) //
				   .hmacUrl(url, "")               //
				   .forDownload(false)             //
				   .saveFilePath("")               //
				   .withLog()                      //
				   .receiver(qApp)                 //
				   .result([task_data, this](const pls::http::Reply &reply) {
					   if (pls_get_app_exiting())
						   return;

					   if (reply.isTimeout()) {
						   emit LoadingVisible(task_data, false);
						   PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "Request time out %d s.", TIME_OUT_MS);
						   RequestErrorInfo errorInfo;
						   errorInfo.errorText = QString("Request time out.");
						   errorInfo.errorType = RequestErrorType::RequstTimeOut;
						   emit FetchError(task_data, errorInfo);
						   requestQueue.clear();
						   taskMutex.lock();
						   currentRequestTask.second.url = "";
						   taskMutex.unlock();
					   } else {
						   RequestFinished(reply);
					   }
				   }));
}

void GiphyWebHandler::RequestFinished(const pls::http::Reply &reply)
{
	QUrl url = reply.url();
	taskMutex.lock();
	RequestTaskData task = currentRequestTask.second;
	taskMutex.unlock();
	emit LoadingVisible(task, false);
	if (reply.hasErrors()) {
		PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "http response failed! url = %s, error info = %s\n", url.toEncoded().constData(), qPrintable(reply.errors()));
		RequestErrorInfo errorInfo;
		errorInfo.errorText = reply.errors();
		errorInfo.networkError = reply.error();
		emit FetchError(task, errorInfo);
	} else {
		if (isHttpRedirect(reply.statusCode())) {
			PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "Request was redirected.\n");
			RequestErrorInfo errorInfo;
			errorInfo.errorText = QString("Request was redirected.");
			errorInfo.networkError = reply.error();
			emit FetchError(task, errorInfo);
		} else {
			struct ResponData responData;
			responData.task = task;
			if (DealResponData(reply, responData))
				emit FetchResult(responData);
			else {
				PLS_ERROR(MAIN_GIPHY_STICKER_MODULE, "parse sticker list json data error.");
				RequestErrorInfo errorInfo;
				errorInfo.errorText = QString("parse sticker list json data error.");
				errorInfo.networkError = reply.error();
				emit FetchError(task, errorInfo);
			}
		}
	}
	StartNextRequest();
}
