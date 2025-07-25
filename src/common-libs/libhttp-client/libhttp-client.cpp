#include "libhttp-client.h"
#ifdef Q_OS_WIN
#include <Windows.h>
#endif
#include <map>
#include <mutex>
#include <set>
#include <tuple>
#include <qcoreapplication.h>
#include <qjsondocument.h>
#include <qthread.h>
#include <qtimer.h>
#include <quuid.h>
#include <qeventloop.h>
#include <qfileinfo.h>
#include <qfile.h>
#include <qsemaphore.h>
#include <qurlquery.h>

#include <liblog.h>

namespace pls {
namespace http {

const auto LIBHTTP_CLIENT_MODULE = "libhttp-client";
const QString DEFAULT_WORKER_GROUP = "default-worker-group";
const long DEFAULT_WORKER_GROUP_SHARE_COUNT = 20;
const long RS_DOWNLOAD_WORKER_GROUP_SHARE_COUNT = 5;
const QByteArray X_LOGTRACE_TRACEID = "X-Logtrace-TraceId";
const auto X_LOGTRACE_TRACEID_FIELD = "XLogtraceTraceId";

LIBHTTPCLIENT_API const QString RS_DOWNLOAD_WORKER_GROUP = "rs-download-worker-group";

// clang-format off
#define makeShared std::make_shared
#define optionalSetValueRet(variable, newValue) \
	variable = newValue;                    \
	return *this
#define optionalSetValueChk(variable, condition, value) \
	if (condition)                                  \
		variable = value;                       \
	else                                            \
		variable.reset()
#define optionalSetValueChkRetv(variable, condition, value, ret) \
	optionalSetValueChk(variable, condition, value);         \
	return ret
#define optionalSetValueChkRet(variable, condition, value) optionalSetValueChkRetv(variable, condition, value, *this)
#define optionalAddValueRet(variable, newValue) \
	variable.push_back(newValue);           \
	return *this
#define optionalAddValueChk(variable, newValue)       \
	if (variable.has_value())                     \
		variable.value().push_back(newValue); \
	else                                          \
		variable = std::decay_t<decltype(variable)>::value_type{newValue}
#define optionalAddValueChkRet(variable, newValue) \
	optionalAddValueChk(variable, newValue);   \
	return *this
#define optionalAddHashValueChkRet(variable, hashKey, haskValue)                                 \
	if (variable.has_value())                                                                \
		variable.value().insert(hashKey, haskValue);                                     \
	else                                                                                     \
		variable = std::decay_t<decltype(variable)>::value_type{{hashKey, haskValue}}; \
	return *this
#define optionalGetValueChkRet(variable, defaultValue) \
	if (variable.has_value())                      \
		return variable.value();               \
	return defaultValue
#define requestBody(method, manager, requestImpl)                                                  \
	if (requestImpl->hasForm()) {                                                              \
		if (QHttpMultiPart *multiPart = requestImpl->multiPart(); multiPart) {             \
			QNetworkReply *reply = manager->method(requestImpl->m_request, multiPart); \
			multiPart->setParent(reply);                                               \
			return reply;                                                              \
		}                                                                                  \
		return manager->method(requestImpl->m_request, requestImpl->formBody());           \
	}                                                                                          \
	return manager->method(requestImpl->m_request, requestImpl->body())
#define customBody(method, manager, requestImpl)                                                                      \
	if (requestImpl->hasForm()) {                                                                                 \
		if (QHttpMultiPart *multiPart = requestImpl->multiPart(); multiPart) {                                \
			QNetworkReply *reply = manager->sendCustomRequest(requestImpl->m_request, method, multiPart); \
			multiPart->setParent(reply);                                                                  \
			return reply;                                                                                 \
		}                                                                                                     \
		return manager->sendCustomRequest(requestImpl->m_request, method, requestImpl->formBody());           \
	}                                                                                                             \
	return manager->sendCustomRequest(requestImpl->m_request, method, requestImpl->body())
#define callReplyMethod(method_params)         \
	if (auto reply = get(); reply) \
		reply->method_params;
#define callReplyMethodRet(method_params, defval) \
	if (auto reply = get(); reply)    \
		return reply->method_params;      \
	return defval
#define getAttrValue(variable, getter) \
	if (!variable)                 \
		variable = getter;     \
	return variable.value()
#define checkCompleted(replyImpl, ...) \
	if (replyImpl->isCompleted()) \
		return __VA_ARGS__
// clang-format on

#define g_clientStatus Client ::s_clientStatus
#define g_client Client::s_client
#define g_proxy Client::s_client->m_proxy
#define g_workers Client::s_client->m_workers
#define g_cleanupWaitTimeout Client::s_client->s_cleanupWaitTimeout



enum class ClientStatus { Uninitialized, Initialized, Initializing, Destroying };

qint64 genRequestId();

struct RequestImpl {
	mutable qint64 m_requestId = -1;
	mutable QNetworkRequest m_request;
	mutable QString m_id;
	mutable Method m_method = Method::Get;
	mutable QUrl m_originalUrl;
	mutable QUrl m_url;
	mutable QMap<QString, QString> m_urlParams;
	mutable std::optional<QByteArray> m_hmacKey;
	mutable std::optional<QByteArray> m_customMethod;
	mutable std::optional<QList<QNetworkCookie>> m_cookie;
	mutable std::optional<QByteArray> m_body;
	mutable std::optional<QHash<QString, QPair<QStringList, bool>>> m_form;
	mutable QSet<pls::QObjectPtr<QObject>> m_receiver;
	mutable std::optional<IsValid> m_isValid;
	mutable std::optional<QThread *> m_worker;
	mutable std::optional<bool> m_workInNewThread;
	mutable std::optional<QString> m_workInGroup;
	mutable std::optional<int> m_timeout;
	mutable std::optional<bool> m_forDownload;
	mutable std::optional<QString> m_saveDir;
	mutable std::optional<QString> m_saveFileName;
	mutable std::optional<QString> m_saveFileNamePrefix;
	mutable std::optional<QString> m_saveFilePath;
	mutable std::optional<QVariantHash> m_attrs;
	mutable bool m_allowAbort = true;
	mutable std::optional<CheckResult> m_checkResult;
	mutable std::optional<Before> m_before;
	mutable std::optional<After> m_after;
	mutable std::optional<Before> m_beforeLog;
	mutable std::optional<After> m_afterLog;
	mutable std::optional<Result> m_result;
	mutable std::optional<Result> m_okResult;
	mutable std::optional<Result> m_failResult;
	mutable std::optional<Progress> m_progress;
	mutable std::optional<QStringList> m_errors;
	mutable std::optional<ReplyHook> m_hook;
	mutable std::optional<ReplyMonitor> m_monitor;
	mutable std::optional<HeaderLog> m_requestHeaderBodyLog;
	mutable std::optional<HeaderLog> m_replyHeaderLog;
	mutable QDateTime m_startTime;
	mutable QDateTime m_endTime;
	mutable QByteArray m_textBody;

	RequestImpl() : m_requestId(genRequestId()) {}

	const QNetworkRequest &request() const { return m_request; }

	QByteArray method() const
	{
		switch (m_method) {
		case Method::Head:
			return QByteArrayLiteral("Head");
		case Method::Get:
			return QByteArrayLiteral("Get");
		case Method::Post:
			return QByteArrayLiteral("Post");
		case Method::Put:
			return QByteArrayLiteral("Put");
		case Method::Delete:
			return QByteArrayLiteral("Delete");
		case Method::Custom:
			return customMethod();
		default:
			return QByteArray();
		}
	}
	QByteArray customMethod() const
	{
		if (m_customMethod.has_value()) {
			return m_customMethod.value();
		}
		return QByteArray();
	}

	void buildUrl() const
	{
		m_url = m_originalUrl;
		QUrlQuery urlQuery(m_url);
		pls_for_each(m_urlParams, [&urlQuery](const QString &name, const QString &value) { urlQuery.addQueryItem(name, value); });
		m_url.setQuery(urlQuery);
		if (m_hmacKey.has_value()) {
			m_url = buildHmacUrl(m_url, m_hmacKey.value());
		}
		m_request.setUrl(m_url);
	}

	bool hasBody() const { return m_body.has_value(); }
	QByteArray body() const
	{
		if (m_body.has_value()) {
			return m_body.value();
		}
		return {};
	}

	bool hasForm() const { return m_form.has_value(); }
	bool formHasFile() const
	{
		if (hasForm()) {
			for (const auto &item : m_form.value()) {
				if (item.second) {
					return true;
				}
			}
		}
		return false;
	}
	QHttpMultiPart *multiPart() const
	{
		if (!formHasFile()) {
			return nullptr;
		}

		m_textBody = "form body: \n";
		QHttpMultiPart *multiPart = pls_new<QHttpMultiPart>(QHttpMultiPart::FormDataType);
		pls_for_each(m_form.value(), [multiPart, this](const QString &name, const QPair<QStringList, bool> &value) {
			if (!value.second) { // not file
				for (const QString &text : value.first) {
					addFieldPart(multiPart, name, text);
					m_textBody.append(name.toUtf8());
					m_textBody.append('=');
					m_textBody.append(text.toUtf8());
					m_textBody.append('\n');
				}
			} else { // file
				for (const QString &filePath : value.first) {
					addFilePart(multiPart, name, filePath);
					m_textBody.append(name.toUtf8());
					m_textBody.append('=');
					m_textBody.append(pls_get_path_file_name(filePath).toUtf8());
					m_textBody.append('\n');
				}
			}
		});
		return multiPart;
	}
	void addFieldPart(QHttpMultiPart *multiPart, const QString &name, const QString &value) const
	{
		QHttpPart part;
		part.setHeader(QNetworkRequest::ContentDispositionHeader, QString("form-data; name=\"%1\"").arg(name));
		part.setBody(value.toUtf8());
		multiPart->append(part);
	}
	void addFilePart(QHttpMultiPart *multiPart, const QString &name, const QString &filePath) const
	{
		QFileInfo fileInfo(filePath);
		if (!fileInfo.isFile()) {
			addError(QString("form file not found, file path: %1").arg(filePath));
			return;
		}

		if (QFile *file = pls_new<QFile>(filePath); file->open(QFile::ReadOnly)) {
			QHttpPart part;
			part.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
			part.setHeader(QNetworkRequest::ContentDispositionHeader, QString("form-data; name=\"%1\"; filename=\"%2\"").arg(name, fileInfo.fileName()));
			file->setParent(multiPart);
			part.setBodyDevice(file);
			multiPart->append(part);
		} else {
			addError(QString("open form file failed, file path: %1, reason: %2").arg(filePath, file->errorString()));
			pls_delete(file);
		}
	}
	QByteArray formBody() const
	{
		if (!hasForm() || formHasFile()) {
			return {};
		}

		QUrlQuery urlQuery;
		pls_for_each(m_form.value(), [&urlQuery](const QString &name, const QPair<QStringList, bool> &value) {
			for (const QString &text : value.first) {
				urlQuery.addQueryItem(name, QUrl::toPercentEncoding(text));
			}
		});

		m_textBody = urlQuery.toString(QUrl::FullyEncoded).toUtf8();
		return m_textBody;
	}

	QThread *worker() const { return m_worker.value_or(nullptr); }
	bool isWorkInNewThread() const { return m_workInNewThread.value_or(false); }
	QString workerGroup() const { return m_workInGroup.has_value() ? m_workInGroup.value() : DEFAULT_WORKER_GROUP; }

	int timeout(int defvalue = -1) const
	{
		if (m_timeout.has_value()) {
			return m_timeout.value();
		} else if (!isForDownload()) {
			return (defvalue > 0) ? defvalue : REQUEST_DEFAULT_TIMEOUT;
		}
		return defvalue;
	}

	bool isForDownload() const { return m_forDownload.value_or(false); }

	bool allowAbort(bool check) const { return check ? m_allowAbort : true; }

	void before(const NetworkAccessManagerPtr &manager, const RequestImplPtr &requestImpl) const
	{
		pls_invoke_safe(m_beforeLog, manager.get(), requestImpl);
		pls_invoke_safe(m_before, manager.get(), requestImpl);
	}
	void requestHeaderBodyLog(const ReplyImplPtr &replyImpl) const { pls_invoke_safe(m_requestHeaderBodyLog, replyImpl); }

	void addError(const QString &error) const
	{
		if (!error.isEmpty()) {
			optionalAddValueChk(m_errors, error);
		}
	}
	bool hasErrors() const
	{
		if (!m_errors.has_value()) {
			return false;
		} else if (m_errors.value().isEmpty()) {
			return false;
		}
		return true;
	}
	QString errors() const
	{
		if (hasErrors()) {
			return m_errors.value().join('\n');
		}
		return {};
	}

	const Request &beforeLog(const Request &request, const Before &beforeLog) const { optionalSetValueChkRetv(m_beforeLog, beforeLog, beforeLog, request); }
	const Request &afterLog(const Request &request, const After &afterLog) const { optionalSetValueChkRetv(m_afterLog, afterLog, afterLog, request); }
	const Request &requestHeaderBodyLog(const Request &request, const HeaderLog &requestHeaderBodyLog) const
	{
		optionalSetValueChkRetv(m_requestHeaderBodyLog, requestHeaderBodyLog, requestHeaderBodyLog, request);
	}
	const Request &replyHeaderLog(const Request &request, const HeaderLog &replyHeaderLog) const { optionalSetValueChkRetv(m_replyHeaderLog, replyHeaderLog, replyHeaderLog, request); }

	QString buildSaveFilePath(const QString &suffix) const
	{
		if (m_saveFilePath.has_value()) {
			return m_saveFilePath.value();
		} else if (m_saveDir.has_value()) {
			if (m_saveFileNamePrefix.has_value() && m_saveFileName.has_value()) {
				return m_saveDir.value() + '/' + m_saveFileNamePrefix.value() + m_saveFileName.value();
			} else if (m_saveFileName.has_value()) {
				return m_saveDir.value() + '/' + m_saveFileName.value();
			} else if (m_saveFileNamePrefix.has_value()) {
				return m_saveDir.value() + '/' + m_saveFileNamePrefix.value() + pls_gen_uuid() + suffix;
			} else {
				return m_saveDir.value() + '/' + pls_gen_uuid() + suffix;
			}
		}
		return {};
	}
	IReplyHookPtr hook(const Request &request) const { return pls_invoke_safe(nullptr, m_hook, request); }
	void monitor(const Request &request, const Reply &reply) { pls_invoke_safe(m_monitor, request, reply); }

	void start() { m_startTime = QDateTime::currentDateTime(); }
	void end() { m_endTime = QDateTime::currentDateTime(); }
};
struct ReplyImpl {
	mutable NetworkAccessManagerPtr m_manager;
	mutable RequestImplPtr m_requestImpl;
	mutable NetworkReplyPtr m_reply;
	mutable std::optional<QByteArray> m_data;
	mutable std::optional<qint64> m_downloadedBytes;
	mutable std::optional<qint64> m_downloadTotalBytes;
	mutable std::optional<QFile> m_downloadFile;
	mutable std::optional<QString> m_downloadFilePath;
	mutable std::optional<QString> m_downloadTempFilePath;
	mutable bool m_completed = false;
	mutable int m_percent = 0;
	mutable QTimer *m_checkTimeoutTimer = nullptr;
	mutable QMetaObject::Connection m_abortConn;
	mutable QMetaObject::Connection m_readyToCloseConn;

	explicit ReplyImpl(const NetworkAccessManagerPtr &manager, const RequestImplPtr &requestImpl, NetworkReplyPtr reply) : m_manager(manager), m_requestImpl(requestImpl), m_reply(reply)
	{
		m_reply->init();
	}
	~ReplyImpl() { stopCheckTimer(); }

	NetworkAccessManager *manager() const { return m_manager.get(); }
	Request request() const { return m_requestImpl; }

	QUrl url() const { return m_reply->url().value_or(m_requestImpl->m_originalUrl); }
	int statusCode() const { return m_reply->statusCode(); }

	QNetworkReply::NetworkError error() const { return m_reply->error(); }
	QString errorString() const { return m_reply->errorString(); }

	QVariant header(QNetworkRequest::KnownHeaders header) const { return m_reply->header(header); }
	bool hasRawHeader(const QByteArray &headerName) const { return m_reply->hasRawHeader(headerName); }
	QByteArray rawHeader(const QByteArray &header) const { return m_reply->rawHeader(header); }
	QString contentType() const { return header(QNetworkRequest::ContentTypeHeader).toString(); }

	void addError(const QString &error) const { m_requestImpl->addError(error); }

	Status status() const { return m_reply->status(); }
	void setStatus(bool ok) const { setStatus(ok ? Status::Ok : Status::Failed); }
	void setStatus(Status status) const { m_reply->setStatus(status); }

	bool isFinished() const { return isOk() || isFailed() || isTimeout() || isAborted() || isRenameFailed(); }
	bool isOk() const { return status() == Status::Ok; }
	bool isFailed() const { return status() == Status::Failed; }
	bool isTimeout() const { return status() == Status::Timeout; }
	bool isAborted() const { return status() == Status::Aborted; }
	bool isRenameFailed() const { return status() == Status::RenameFailed; }

	bool isCompleted() const { return m_completed; }

	void trigger() { m_reply->trigger(); }
	void abort(Status status, const QString &error = QString()) const
	{
		if (!isFinished()) {
			setStatus(status);
			m_requestImpl->addError(error);
			m_reply->abort();
		}
	}

	QByteArray data() const { getAttrValue(m_data, m_reply->data()); }
	bool json(QJsonDocument &doc, const JsonParseFail &fail) const
	{
		QJsonParseError error;
		doc = QJsonDocument::fromJson(data(), &error);
		if (error.error == QJsonParseError::NoError) {
			return true;
		}

		pls_invoke_safe(fail, error);
		return false;
	}
	QJsonDocument json(const JsonParseFail &fail) const
	{
		QJsonDocument json;
		this->json(json, fail);
		return json;
	}
	void json(const JsonParseOk<QJsonDocument> &ok, const JsonParseFail &fail) const
	{
		if (QJsonDocument json; this->json(json, fail)) {
			pls_invoke_safe(ok, json);
		}
	}
	QJsonArray array(const JsonParseFail &fail) const
	{
		if (QJsonDocument json; this->json(json, fail)) {
			return json.array();
		}
		return {};
	}
	void array(const JsonParseOk<QJsonArray> &ok, const JsonParseFail &fail) const
	{
		if (QJsonDocument json; this->json(json, fail)) {
			pls_invoke_safe(ok, json.array());
		}
	}
	QJsonObject object(const JsonParseFail &fail) const
	{
		if (QJsonDocument json; this->json(json, fail)) {
			return json.object();
		}
		return {};
	}
	void object(const JsonParseOk<QJsonObject> &ok, const JsonParseFail &fail) const
	{
		if (QJsonDocument json; this->json(json, fail)) {
			pls_invoke_safe(ok, json.object());
		}
	}

	bool isDownloadOk(const ReplyImplPtr &replyImpl) const
	{
		if (!checkResult(replyImpl)) {
			return false;
		} else if ((downloadTotalBytes() > 0) && (downloadedBytes() == downloadTotalBytes())) {
			return true;
		}
		return false;
	}
	qint64 downloadedBytes() const { return m_downloadedBytes.value_or(-1); }
	qint64 downloadTotalBytes() const { return m_downloadTotalBytes.value_or(-1); }
	int downloadPercent() const
	{
		qint64 downloadedBytes = this->downloadedBytes();
		qint64 downloadTotalBytes = this->downloadTotalBytes();
		if ((downloadedBytes >= 0) && (downloadTotalBytes > 0)) {
			return int(downloadedBytes * 100 / downloadTotalBytes);
		}
		return 0;
	}
	QString downloadFilePath() const { return m_downloadFilePath.value_or(QString()); }
	bool openFile() const
	{
		if (m_downloadFile.has_value()) {
			return true;
		}

		auto contentType = this->contentType();
		auto suffix = contentType2Suffix(contentType);
		if (suffix.isEmpty()) {
			auto fileName = m_requestImpl->m_url.fileName();
			suffix = pls_get_path_file_suffix(fileName);
		}

		QString saveFilePath = m_requestImpl->buildSaveFilePath(suffix);
		if (saveFilePath.isEmpty()) {
			m_requestImpl->addError("file save path not specified");
			return false;
		}

		if (pls_get_path_file_suffix(saveFilePath).isEmpty())
			saveFilePath += suffix;

		QString saveTempFilePath = saveFilePath + ".temp";

		QString error;
		if (!pls_remove_file(saveTempFilePath, &error)) {
			m_requestImpl->addError(QString("remove temp file failed. file name: %1, reason: %2").arg(pls_get_path_file_name(saveTempFilePath), error));
			return false;
		}

		if (!pls_open_file(m_downloadFile, saveTempFilePath, QFile::WriteOnly | QFile::Truncate, error)) {
			m_requestImpl->addError(error);
			return false;
		}

		m_downloadFilePath = saveFilePath;
		m_downloadTempFilePath = saveTempFilePath;
		return true;
	}
	void saveDownloadData(const ReplyImplPtr &replyImpl, bool finished = false) const
	{
		if (openFile()) {
			m_downloadFile.value().write(m_reply->data());
		} else {
			abort(Status::Failed);
		}

		if (finished) {
			m_downloadFile.reset();
			renameTempFile(replyImpl);
		}
	}
	void renameTempFile(const ReplyImplPtr &replyImpl) const
	{
		if (!m_downloadTempFilePath.has_value() || !m_downloadFilePath.has_value()) {
			return;
		}

		QString saveTempFilePath = m_downloadTempFilePath.value();

		if (!isDownloadOk(replyImpl)) {
			pls_remove_file(saveTempFilePath);
			return;
		}

		QString saveFilePath = m_downloadFilePath.value();

		QString error;
		if (!pls_rename_file(saveTempFilePath, saveFilePath, &error)) {
			setStatus(Status::RenameFailed);
			m_requestImpl->addError(QString("rename file from %1 to %2 failed, reason: %3") //
							.arg(pls_get_path_file_name(saveTempFilePath), pls_get_path_file_name(saveFilePath), error));
		}
	}

	bool checkResult(const ReplyImplPtr &replyImpl) const
	{
		if (isTimeout() || isFailed() || isAborted() || isRenameFailed()) {
			return false;
		} else if (isOk()) {
			return true;
		} else if (!m_reply || !m_reply->valid()) {
			return false;
		} else if (m_requestImpl->m_checkResult.has_value()) {
			return pls_invoke_safe(m_requestImpl->m_checkResult.value(), replyImpl);
		}
		return pls::http::checkResult(replyImpl);
	}
	void result(const ReplyImplPtr &replyImpl) const { pls_invoke_safe(m_requestImpl->m_result, replyImpl); }
	void resultSafe(const ReplyImplPtr &replyImpl) const
	{
		if (pls_objects_is_valid(m_requestImpl->m_receiver, m_requestImpl->m_isValid.value_or(nullptr))) {
			result(replyImpl);
		}
	}
	void failResult(const ReplyImplPtr &replyImpl) const { pls_invoke_safe(m_requestImpl->m_failResult, replyImpl); }
	void autoResult(const ReplyImplPtr &replyImpl) const { pls_invoke_safe(isOk() ? m_requestImpl->m_okResult : m_requestImpl->m_failResult, replyImpl); }
	void autoResultSafe(const ReplyImplPtr &replyImpl) const
	{
		if (pls_objects_is_valid(m_requestImpl->m_receiver, m_requestImpl->m_isValid.value_or(nullptr))) {
			autoResult(replyImpl);
		}
	}
	void progress(const ReplyImplPtr &replyImpl, qint64 downloadedBytes, qint64 downloadTotalBytes) const
	{
		if ((downloadedBytes >= 0) && (downloadTotalBytes > 0) && checkResult(replyImpl)) {
			m_downloadedBytes = downloadedBytes;
			m_downloadTotalBytes = downloadTotalBytes;
			m_percent = downloadPercent();
			pls_invoke_safe(m_requestImpl->m_progress, replyImpl);
		}
	}
	void progress(const ReplyImplPtr &replyImpl, int percent) const
	{
		m_downloadedBytes.reset();
		m_downloadTotalBytes.reset();
		m_percent = percent;
		pls_invoke_safe(m_requestImpl->m_progress, replyImpl);
	}

	void newTimer(QTimer *&timer, const ReplyImplPtr &replyImpl, int timeout) const
	{
		timer = pls_new<QTimer>();
		timer->setSingleShot(true);
		timer->setInterval(timeout);

		ReplyImplWeakPtr replyImplWeakPtr = replyImpl;
		QObject::connect(timer, &QTimer::timeout, manager(), [replyImplWeakPtr, timeout]() {
			if (auto replyImplTmp = replyImplWeakPtr.lock(); replyImplTmp) {
				PLS_ERROR(LIBHTTP_CLIENT_MODULE, "request timeout. url: %s", replyImplTmp->m_requestImpl->m_url.toString(QUrl::FullyEncoded).toUtf8().constData());
				replyImplTmp->abort(Status::Timeout, QString("request timed out, timeout: %1ms").arg(timeout));
			}
		});
	}
	void deleteTimer(QTimer *&timer) const
	{
		if (timer) {
			timer->stop();
			pls_delete(timer, nullptr);
		}
	}
	void checkTimeout(const ReplyImplPtr &replyImpl) const
	{
		if (int timeout = m_requestImpl->timeout(); timeout > 0) {
			newTimer(m_checkTimeoutTimer, replyImpl, timeout);
			m_checkTimeoutTimer->start();
		}
	}
	void recheckTimeout() const
	{
		if (m_checkTimeoutTimer) {
			m_checkTimeoutTimer->stop();
			m_checkTimeoutTimer->start();
		}
	}
	void stopCheckTimer() const { deleteTimer(m_checkTimeoutTimer); }

	void monitor(const ReplyImplPtr &replyImpl) const
	{
		auto requestImpl = replyImpl->m_requestImpl;
		requestImpl->monitor(requestImpl, replyImpl);
	}
	void after(const ReplyImplPtr &replyImpl) const
	{
		pls_invoke_safe(m_requestImpl->m_afterLog, replyImpl);
		pls_invoke_safe(m_requestImpl->m_replyHeaderLog, replyImpl);
		pls_invoke_safe(m_requestImpl->m_after, replyImpl);
	}
	void completed() const { m_completed = true; }
	void deleteLater() const
	{
		QObject::disconnect(m_abortConn);
		QObject::disconnect(m_readyToCloseConn);
		m_reply->destroy();
		m_reply = nullptr;
	}

	void end() { m_requestImpl->end(); }
};
struct RequestsImpl {
	mutable QList<RequestImplPtr> m_requestImpls;
	mutable QSet<pls::QObjectPtr<QObject>> m_receiver;
	mutable std::optional<IsValid> m_isValid;
	mutable std::optional<QThread *> m_worker;
	mutable std::optional<bool> m_workInNewThread;
	mutable std::optional<QString> m_workInGroup;
	mutable std::optional<CheckResult> m_checkResult;
	mutable std::optional<Befores> m_before;
	mutable std::optional<Afters> m_after;
	mutable std::optional<Results> m_results;
	mutable std::optional<Progresses> m_progress;

	QThread *worker() const { return m_worker.value_or(nullptr); }
	bool isWorkInNewThread() const { return m_workInNewThread.value_or(false); }
	QString workerGroup() const { return m_workInGroup.has_value() ? m_workInGroup.value() : DEFAULT_WORKER_GROUP; }

	void buildUrl() const
	{
		for (auto requestImpl : m_requestImpls) {
			requestImpl->buildUrl();
		}
	}
	void abort(bool allowAbortCheck) const
	{
		for (auto requestImpl : m_requestImpls) {
			Request request{requestImpl};
			request.abort(allowAbortCheck);
		}
	}

	void before(const NetworkAccessManagerPtr &manager, const RequestsImplPtr &requestsImpl) const { pls_invoke_safe(m_before, manager.get(), requestsImpl); }
	void start() const
	{
		pls_for_each(m_requestImpls, [](RequestImplPtr requestImpl) { requestImpl->start(); });
	}
};
struct RepliesImpl {
	mutable NetworkAccessManagerPtr m_manager;
	mutable RequestsImplPtr m_requestsImpl;
	mutable QList<ReplyImplPtr> m_replyImpls;
	mutable int m_percent = 0;

	RepliesImpl(const NetworkAccessManagerPtr &manager, const RequestsImplPtr &requestsImpl) : m_manager(manager), m_requestsImpl(requestsImpl) {}

	bool isCompleted() const
	{
		for (auto replyImpl : m_replyImpls) {
			if (!replyImpl->isCompleted()) {
				return false;
			}
		}
		return true;
	}

	bool isDownloadOk(const ReplyImplPtr &replyImpl) const
	{
		if (!checkResult(replyImpl)) {
			return false;
		} else if (replyImpl->isDownloadOk(replyImpl)) {
			return true;
		}
		return false;
	}
	bool checkResult(const ReplyImplPtr &replyImpl) const
	{
		if (replyImpl->isTimeout() || replyImpl->isFailed() || replyImpl->isAborted() || replyImpl->isRenameFailed()) {
			return false;
		} else if (replyImpl->isOk()) {
			return true;
		} else if (!replyImpl->m_reply || !replyImpl->m_reply->valid()) {
			return false;
		} else if (m_requestsImpl->m_checkResult.has_value()) {
			return pls_invoke_safe(m_requestsImpl->m_checkResult.value(), replyImpl);
		}
		return replyImpl->checkResult(replyImpl);
	}
	void resultSafe(const ReplyImplPtr &replyImpl) const
	{
		if (pls_objects_is_valid(m_requestsImpl->m_receiver, m_requestsImpl->m_isValid.value_or(nullptr))) {
			replyImpl->result(replyImpl);
		}
	}
	void autoResultSafe(const ReplyImplPtr &replyImpl) const
	{
		if (pls_objects_is_valid(m_requestsImpl->m_receiver, m_requestsImpl->m_isValid.value_or(nullptr))) {
			replyImpl->autoResult(replyImpl);
		}
	}
	void resultsSafe(const RepliesImplPtr &repliesImpl) const
	{
		if (pls_objects_is_valid(m_requestsImpl->m_receiver, m_requestsImpl->m_isValid.value_or(nullptr))) {
			pls_invoke_safe(m_requestsImpl->m_results, repliesImpl);
		}
	}
	void after(const RepliesImplPtr &repliesImpl) const { pls_invoke_safe(m_requestsImpl->m_after, repliesImpl); }
	void progress(const ReplyImplPtr &replyImpl, qint64 downloadedBytes, qint64 downloadTotalBytes) const
	{
		if ((downloadedBytes >= 0) && (downloadTotalBytes > 0) && checkResult(replyImpl)) {
			replyImpl->progress(replyImpl, downloadedBytes, downloadTotalBytes);
		}
	}
	void progress(const RepliesImplPtr &repliesImpl) const
	{
		if (!m_replyImpls.empty()) {
			auto count = std::count_if(m_replyImpls.begin(), m_replyImpls.end(), [](const auto &replyImpl) { return replyImpl->isFinished(); });
			m_percent = int(count * 100 / m_replyImpls.size());
		} else {
			m_percent = 100;
		}

		pls_invoke_safe(m_requestsImpl->m_progress, repliesImpl);
	}
	void completed(const ReplyImplPtr &replyImpl) const { replyImpl->completed(); }
	void completed(const RepliesImplPtr &repliesImpl) const
	{
		if (isCompleted()) {
			after(repliesImpl);
			resultsSafe(repliesImpl);
			deleteLater();
		}
	}
	void deleteLater() const
	{
		for (auto replyImpl : m_replyImpls) {
			replyImpl->deleteLater();
		}
	}
};

class NetworkReplyHook : public NetworkReply {
	Q_OBJECT

public:
	NetworkReplyHook(NetworkAccessManagerPtr manager, IReplyHookPtr hook) : NetworkReply(manager, nullptr), m_hook(hook) {}

	QByteArray genData() const
	{
		if (auto data = m_hook->data(); data)
			return data.value();
		else if (auto json = m_hook->json(); json)
			return json.value().toJson(QJsonDocument::Compact);
		else if (auto array = m_hook->array(); array)
			return QJsonDocument(array.value()).toJson(QJsonDocument::Compact);
		else if (auto object = m_hook->object(); object)
			return QJsonDocument(object.value()).toJson(QJsonDocument::Compact);
		else if (auto filePath = m_hook->filePath(); !filePath)
			return {};
		else if (QFileInfo fi(filePath.value()); fi.isFile())
			return pls_read_data(fi.filePath());
		return {};
	}
	QByteArray getData() const
	{
		if (auto status = this->status(); !(status == Status::Aborted || status == Status::Timeout))
			return m_data.value();
		return {};
	}
	void emitFinished()
	{
		if (!m_finished) {
			m_finished = true;
			emit finished();
		}
	}
	void triggered()
	{
		deleteTimer(m_delayTimer);
		if (m_hook->m_request->isForDownload())
			emit downloadProgress(0, m_data.value().length());
		emitFinished();
	}
	void newTimer(QTimer *&timer, int delay)
	{
		deleteTimer(timer);
		timer = pls_new<QTimer>();
		timer->setSingleShot(true);
		QObject::connect(timer, &QTimer::timeout, this, &NetworkReplyHook::triggered, Qt::QueuedConnection);
		timer->start(delay);
	}
	void deleteTimer(QTimer *&timer)
	{
		if (timer) {
			timer->stop();
			pls_delete(timer, nullptr);
		}
	}

	void init() override
	{
		NetworkReply::init();
		m_data = genData();
	}
	void destroy() override
	{
		NetworkReply::destroy();
		deleteTimer(m_delayTimer);
		m_hook = nullptr;
		m_data = std::nullopt;
	}

	bool valid() const { return true; }

	std::optional<QUrl> url() const override { return m_hook->url(); }
	int statusCode() const override { return m_hook->statusCode(); }

	Status status() const override { return m_hook->status(); }

	QNetworkReply::NetworkError error() const override { return m_hook->error(); }
	QString errorString() const override { return m_hook->errorString(); }

	QVariant header(QNetworkRequest::KnownHeaders header) const override
	{
		if (header != QNetworkRequest::ContentLengthHeader)
			return m_hook->header(header);
		else
			return getData().length();
	}
	bool hasRawHeader(const QByteArray &header) const override { return m_hook->hasRawHeader(header); }
	QByteArray rawHeader(const QByteArray &header) const override { return m_hook->rawHeader(header); }

	QByteArray data() override { return getData(); }

	void trigger() override
	{
		if (auto delay = m_hook->triggerDelay(); delay > 0) {
			newTimer(m_delayTimer, delay);
		} else {
			triggered();
		}
	}

	void abort() override
	{
		if (!m_delayTimer)
			return;

		deleteTimer(m_delayTimer);
		emitFinished();
	}

	QList<QNetworkReply::RawHeaderPair> replyRawHeaders() const override { return m_hook->replyRawHeaders(); }

private:
	IReplyHookPtr m_hook;
	std::optional<QByteArray> m_data;
	QTimer *m_delayTimer = nullptr;
	bool m_finished = false;
};

class Proxy : public QObject {
	Q_OBJECT

public:
	explicit Proxy(Worker *worker) { moveToThread(worker); }

signals:
	void abort(bool allowAbortCheck, const QString &error, qint64 requestId = -1);
	void noWorkers();
};

class Client : public ExclusiveWorker {
	Q_OBJECT

public:
	DefaultRequestHeadersFactory m_factory;
	std::map<QString, int> m_shareCounts;

	Proxy *m_proxy = nullptr;
	std::map<QThread *, NetworkAccessManagerWeakPtr> m_workers; // worker -> NetworkAccessManager
	std::set<QString> m_rids;
	std::map<QString, ReplyHook> m_hooks;
	std::map<QString, ReplyMonitor> m_monitors;

	static ClientStatus s_clientStatus;
	static std::atomic<qint64> s_requestId;
	static Client *s_client;
	static int s_cleanupWaitTimeout;

	Client()
	{
		m_proxy = pls_new<Proxy>(this);
		m_shareCounts[RS_DOWNLOAD_WORKER_GROUP] = RS_DOWNLOAD_WORKER_GROUP_SHARE_COUNT;
	}

	template<typename ReqImpl> NetworkAccessManagerPtr getManager(const ReqImpl &reqImpl, QThread *workThread, bool isWorkInNewThread)
	{
		if (workThread) {
			if (auto iter = m_workers.find(workThread); iter != m_workers.end()) {
				if (auto manager = iter->second.lock(); manager) {
					return manager;
				}

				m_workers.erase(iter);
			}
			return newManager(reqImpl, workThread);
		}

		if (isWorkInNewThread) {
			return newManager<ExclusiveWorker>(reqImpl);
		}

		for (const auto &it : m_workers) {
			if (auto worker = dynamic_cast<Worker *>(it.first); worker && worker->isShared()) {
				if (auto manager = it.second.lock(); manager && (manager.use_count() <= workerSharedCount(worker))) {
					return manager;
				}
			}
		}
		return newManager<SharedWorker>(reqImpl);
	}
	void getHookAndMonitor(const RequestImplPtr &requestImpl)
	{
		if (!requestImpl->m_id.isEmpty())
			m_rids.insert(requestImpl->m_id);
		requestImpl->m_hook = pls_get_value<std::optional<ReplyHook>>(m_hooks, requestImpl->m_id, std::nullopt);
		requestImpl->m_monitor = pls_get_value<std::optional<ReplyMonitor>>(m_monitors, requestImpl->m_id, std::nullopt);
	}
	void getHookAndMonitor(const RequestsImplPtr &requestsImpl)
	{
		for (auto requestImpl : requestsImpl->m_requestImpls) {
			getHookAndMonitor(requestImpl);
		}
	}

private:
	static bool isWorkThreadUseForHttpClient(QThread *workThread)
	{
		if (!pls_object_is_valid(workThread)) {
			return false;
		} else if (!Worker::useForHttpClient(workThread)) {
			return false;
		} else if (auto worker = dynamic_cast<Worker *>(workThread); !worker) {
			return false;
		}
		return true;
	}
	long workerSharedCount(const Worker *worker) const
	{
		QString workerGroup = worker->property("$libhttp-client.workerGroup").toString();
		if (auto iter = m_shareCounts.find(workerGroup); (iter != m_shareCounts.end()) && (iter->second > 0)) {
			return iter->second;
		}
		return DEFAULT_WORKER_GROUP_SHARE_COUNT;
	}
	template<typename WorkThread, typename ReqImpl> NetworkAccessManagerPtr newManager(const ReqImpl &reqImpl, WorkThread *workThread = nullptr)
	{
		if (!workThread) {
			workThread = pls_new<WorkThread>();
			Worker::setUseForHttpClient(workThread, true);
			workThread->setProperty("$libhttp-client.workerGroup", reqImpl->workerGroup());
		}

		NetworkAccessManagerPtr manager{pls_new<NetworkAccessManager>(), [](NetworkAccessManager *ptr) { ptr->deleteLater(); }};
		manager->moveToThread(workThread);
		m_workers[workThread] = manager;

		connect(manager.get(), &QObject::destroyed, m_proxy, [workThread]() {
			if (pls_object_is_valid(g_client) && pls_object_is_valid(g_proxy)) {
				g_client->eraseWorker(workThread);
			}
		});

		if (!isWorkThreadUseForHttpClient(workThread)) {
			connect(workThread, &QThread::finished, manager.get(), [workThread, managerWeakPtr = NetworkAccessManagerWeakPtr(manager)]() {
				if (auto mgr = managerWeakPtr.lock(); mgr) {
					emit mgr->readyToClose();
				}

				pls_async_call(g_client, [workThread]() { //
					g_client->eraseWorker(workThread, false);
				});
			});
		}
		return manager;
	}
	void checkWorkers()
	{
		std::list<QThread *> finishedThreads;
		for (auto worker : m_workers) {
			if (!pls_object_is_valid(worker.first)) {
				finishedThreads.push_back(worker.first);
			} else if (auto manager = worker.second.lock(); !manager) {
				finishedThreads.push_back(worker.first);
			}
		}

		for (auto finishedThread : finishedThreads) {
			eraseWorker(finishedThread);
		}
	}
	void eraseWorker(QThread *workThread, bool deleteWorker = true)
	{
		if (auto iter = m_workers.find(workThread); iter != m_workers.end()) {
			m_workers.erase(iter);
		}

		if (deleteWorker && isWorkThreadUseForHttpClient(workThread)) {
			pls_delete_thread(workThread);
		}

		if (m_workers.empty()) {
			m_proxy->noWorkers();
		}
	}

protected:
	void run() override
	{
		ExclusiveWorker::run();
		pls_delete(m_proxy, nullptr);
	}

public:
	static bool isInitialized()
	{
		if ((s_clientStatus == ClientStatus::Initialized) && s_client) {
			return true;
		}
		return false;
	}
	static void setFactory(const DefaultRequestHeadersFactory &factory)
	{
		pls_async_call(g_proxy, g_client, [factory]() { g_client->m_factory = factory; });
	}
	static void setShareCount(const QString &group, int shareCount)
	{
		pls_async_call(g_proxy, g_client, [group, shareCount]() { g_client->m_shareCounts[group] = shareCount; });
	}
	static void destroy()
	{
		QEventLoop eventLoop;
		QObject::connect(g_proxy, &Proxy::noWorkers, &eventLoop, &QEventLoop::quit, Qt::QueuedConnection);
		pls_async_call(g_proxy, g_client, []() {
			g_client->checkWorkers();
			if (g_workers.empty()) {
				g_proxy->noWorkers();
			} else if (!g_cleanupWaitTimeout) {
				g_proxy->abort(true, "destroy http client");
			} else if (g_cleanupWaitTimeout > 0) {
				QTimer::singleShot(g_cleanupWaitTimeout, g_proxy, []() { g_proxy->abort(true, "destroy http client"); });
			}
		});
		eventLoop.exec();
		pls_delete_thread(g_client, nullptr);
	}
	static QNetworkReply *buildRequest(const RequestImplPtr &requestImpl, NetworkAccessManager *manager)
	{
		switch (requestImpl->m_method) {
		case Method::Head:
			return manager->head(requestImpl->m_request);
		case Method::Get:
			return manager->get(requestImpl->m_request);
		case Method::Post:
			requestBody(post, manager, requestImpl);
		case Method::Put:
			requestBody(put, manager, requestImpl);
		case Method::Delete:
			return manager->deleteResource(requestImpl->m_request);
		case Method::Custom:
			customBody(requestImpl->customMethod(), manager, requestImpl);
		default:
			return nullptr;
		}
	}
	static NetworkReplyPtr newNetworkReply(const NetworkAccessManagerPtr &manager, const RequestImplPtr &requestImpl)
	{
		if (auto hook = requestImpl->hook(requestImpl); !hook) {
			return makeShared<NetworkReply>(manager, buildRequest(requestImpl, manager.get()));
		} else {
			return makeShared<NetworkReplyHook>(manager, hook);
		}
	}
	static ReplyImplPtr newReply(const NetworkAccessManagerPtr &manager, const RequestImplPtr &requestImpl)
	{
		requestImpl->before(manager, requestImpl);
		auto replyImpl = makeShared<ReplyImpl>(manager, requestImpl, newNetworkReply(manager, requestImpl));
		requestImpl->requestHeaderBodyLog(replyImpl);
		ReplyImplWeakPtr replyImplWeakPtr = replyImpl;
		replyImpl->m_abortConn = QObject::connect(g_proxy, &Proxy::abort, manager.get(), [replyImplWeakPtr](bool allowAbortCheck, const QString &error, qint64 requestId) {
			if (auto replyImplTmp = replyImplWeakPtr.lock(); !replyImplTmp) {

			} else if (auto requestImplTmp = replyImplTmp->m_requestImpl; !requestImplTmp->allowAbort(allowAbortCheck)) {
				PLS_WARN(LIBHTTP_CLIENT_MODULE, "request abort not allowed. url: %s", requestImplTmp->m_url.toString(QUrl::FullyEncoded).toUtf8().constData());
			} else if ((requestId < 0) || (requestId == requestImplTmp->m_requestId)) {
				PLS_WARN(LIBHTTP_CLIENT_MODULE, "request aborted. url: %s", requestImplTmp->m_url.toString(QUrl::FullyEncoded).toUtf8().constData());
				replyImplTmp->abort(Status::Aborted, error.isEmpty() ? QStringLiteral("request aborted.") : error);
			}
		});
		replyImpl->m_readyToCloseConn = QObject::connect(manager.get(), &NetworkAccessManager::readyToClose, [replyImplWeakPtr]() {
			if (auto replyImplTmp = replyImplWeakPtr.lock(); !replyImplTmp) {
			} else {
				PLS_WARN(LIBHTTP_CLIENT_MODULE, "worker thread quit, request aborted. url: %s", replyImplTmp->m_requestImpl->m_url.toString(QUrl::FullyEncoded).toUtf8().constData());
				replyImplTmp->abort(Status::Aborted, "worker thread quit, request aborted.");
			}
		});
		return replyImpl;
	}
	static RepliesImplPtr newReplies(const NetworkAccessManagerPtr &manager, const RequestsImplPtr &requestsImpl)
	{
		requestsImpl->before(manager, requestsImpl);
		auto repliesImpl = makeShared<RepliesImpl>(manager, requestsImpl);
		for (auto requestImpl : requestsImpl->m_requestImpls) {
			auto replyImpl = newReply(manager, requestImpl);
			repliesImpl->m_replyImpls.push_back(replyImpl);
		}
		return repliesImpl;
	}
	static void processReply(const RequestImplPtr &requestImpl, const ReplyImplPtr &replyImpl)
	{
		if (!requestImpl->isForDownload()) {
			QObject::connect(replyImpl->m_reply.get(), &NetworkReply::finished, replyImpl->manager(), [replyImpl]() {
				checkCompleted(replyImpl);
				replyImpl->stopCheckTimer();
				replyImpl->progress(replyImpl, 100);
				replyImpl->setStatus(replyImpl->checkResult(replyImpl));
				replyImpl->end();
				replyImpl->monitor(replyImpl);
				replyImpl->after(replyImpl);
				replyImpl->autoResultSafe(replyImpl);
				replyImpl->resultSafe(replyImpl);
				replyImpl->completed();
				replyImpl->deleteLater();
			});

			replyImpl->checkTimeout(replyImpl);
		} else {
			QObject::connect(replyImpl->m_reply.get(), &NetworkReply::downloadProgress, replyImpl->manager(), [replyImpl](qint64 downloadedBytes, qint64 downloadTotalBytes) {
				checkCompleted(replyImpl);
				replyImpl->progress(replyImpl, downloadedBytes, downloadTotalBytes);
			});
			QObject::connect(replyImpl->m_reply.get(), &NetworkReply::readyRead, replyImpl->manager(), [replyImpl]() {
				checkCompleted(replyImpl);
				replyImpl->recheckTimeout();
				replyImpl->saveDownloadData(replyImpl);
			});
			QObject::connect(replyImpl->m_reply.get(), &NetworkReply::finished, replyImpl->manager(), [replyImpl]() {
				checkCompleted(replyImpl);
				replyImpl->stopCheckTimer();
				replyImpl->saveDownloadData(replyImpl, true);
				replyImpl->setStatus(replyImpl->isDownloadOk(replyImpl));
				replyImpl->end();
				replyImpl->monitor(replyImpl);
				replyImpl->after(replyImpl);
				replyImpl->autoResultSafe(replyImpl);
				replyImpl->resultSafe(replyImpl);
				replyImpl->completed();
				replyImpl->deleteLater();
			});

			replyImpl->checkTimeout(replyImpl);
		}
	}
	static void triggerReply(const ReplyImplPtr &replyImpl) { replyImpl->trigger(); }
	static void request(const Request &request)
	{
		pls_async_call(
			g_proxy, g_client, [](const QObject *) { return isInitialized(); },
			[requestImpl = request.m_impl]() {
				auto manager = g_client->getManager(requestImpl, requestImpl->worker(), requestImpl->isWorkInNewThread());
				g_client->getHookAndMonitor(requestImpl);
				pls_async_call(
					manager.get(), g_proxy, [](const QObject *) { return isInitialized(); },
					[manager, requestImpl]() {
						requestImpl->buildUrl();
						requestImpl->start();
						auto replyImpl = newReply(manager, requestImpl);
						processReply(requestImpl, replyImpl);
						triggerReply(replyImpl);
					});
			});
	}
	static void processReplies(const RepliesImplPtr &repliesImpl)
	{
		for (const ReplyImplPtr &replyImpl : repliesImpl->m_replyImpls) {
			if (!replyImpl->m_requestImpl->isForDownload()) {
				QObject::connect(replyImpl->m_reply.get(), &NetworkReply::finished, replyImpl->manager(), [repliesImpl, replyImpl]() {
					checkCompleted(replyImpl);
					replyImpl->stopCheckTimer();
					replyImpl->progress(replyImpl, 100);
					replyImpl->setStatus(repliesImpl->checkResult(replyImpl));
					replyImpl->end();
					replyImpl->monitor(replyImpl);
					replyImpl->after(replyImpl);
					repliesImpl->progress(repliesImpl);
					repliesImpl->autoResultSafe(replyImpl);
					repliesImpl->resultSafe(replyImpl);
					repliesImpl->completed(replyImpl);
					repliesImpl->completed(repliesImpl);
				});

				replyImpl->checkTimeout(replyImpl);
			} else {
				QObject::connect(replyImpl->m_reply.get(), &NetworkReply::downloadProgress, replyImpl->manager(),
						 [repliesImpl, replyImpl](qint64 downloadedBytes, qint64 downloadTotalBytes) {
							 checkCompleted(replyImpl);
							 repliesImpl->progress(replyImpl, downloadedBytes, downloadTotalBytes);
						 });
				QObject::connect(replyImpl->m_reply.get(), &NetworkReply::readyRead, replyImpl->manager(), [replyImpl]() {
					checkCompleted(replyImpl);
					replyImpl->recheckTimeout();
					replyImpl->saveDownloadData(replyImpl);
				});
				QObject::connect(replyImpl->m_reply.get(), &NetworkReply::finished, replyImpl->manager(), [repliesImpl, replyImpl]() {
					checkCompleted(replyImpl);
					replyImpl->stopCheckTimer();
					replyImpl->saveDownloadData(replyImpl, true);
					replyImpl->setStatus(repliesImpl->isDownloadOk(replyImpl));
					replyImpl->end();
					replyImpl->monitor(replyImpl);
					replyImpl->after(replyImpl);
					repliesImpl->progress(repliesImpl);
					repliesImpl->autoResultSafe(replyImpl);
					repliesImpl->resultSafe(replyImpl);
					repliesImpl->completed(replyImpl);
					repliesImpl->completed(repliesImpl);
				});

				replyImpl->checkTimeout(replyImpl);
			}
		}
	}
	static void triggerReplies(const RepliesImplPtr &repliesImpl)
	{
		for (const ReplyImplPtr &replyImpl : repliesImpl->m_replyImpls)
			replyImpl->trigger();
	}
	static void requests(const Requests &requests)
	{
		pls_async_call(
			g_proxy, g_client, [](const QObject *) { return isInitialized(); },
			[requestsImpl = requests.m_impl]() {
				auto manager = g_client->getManager(requestsImpl, requestsImpl->worker(), requestsImpl->isWorkInNewThread());
				g_client->getHookAndMonitor(requestsImpl);
				pls_async_call(
					manager.get(), g_proxy, [](const QObject *) { return isInitialized(); },
					[manager, requestsImpl]() {
						requestsImpl->buildUrl();
						requestsImpl->start();
						auto repliesImpl = newReplies(manager, requestsImpl);
						processReplies(repliesImpl);
						triggerReplies(repliesImpl);
					});
			});
	}
	static void abort(bool allowAbortCheck, qint64 requestId)
	{
		pls_async_call(
			g_proxy, g_client, [](const QObject *) { return isInitialized(); },
			[allowAbortCheck, requestId]() {
				g_proxy->abort(allowAbortCheck, "Manual termination", requestId); // Manual termination
			});
	}
	static void abortAll(bool allowAbortCheck)
	{
		pls_async_call(
			g_proxy, g_client, [](const QObject *) { return isInitialized(); },
			[allowAbortCheck]() {
				g_proxy->abort(allowAbortCheck, "Manual termination"); // Manual termination
			});
	}
	static void hook(const QString &id, ReplyHook hook)
	{
		pls_async_call(
			g_proxy, g_client, [](const QObject *) { return isInitialized(); }, [id, hook]() { //
				g_client->m_hooks[id] = hook;
			});
	}
	static void unhook(const QString &id)
	{
		pls_async_call(
			g_proxy, g_client, [](const QObject *) { return isInitialized(); }, [id]() { //
				g_client->m_hooks.erase(id);
			});
	}
	static void monitor(const QString &id, const ReplyMonitor &monitor)
	{
		pls_async_call(
			g_proxy, g_client, [](const QObject *) { return isInitialized(); }, [id, monitor]() { //
				g_client->m_monitors[id] = monitor;
			});
	}
	static void unmonitor(const QString &id)
	{
		pls_async_call(
			g_proxy, g_client, [](const QObject *) { return isInitialized(); }, [id]() { //
				g_client->m_monitors.erase(id);
			});
	}
	static QStringList requestIds()
	{
		QStringList rids;
		pls_sync_call(
			g_proxy, g_client, [](const QObject *) { return isInitialized(); }, [&rids]() { //
				for (const auto &rid : g_client->m_rids)
					rids.append(rid);
			});
		return rids;
	}
};

class Initializer {
public:
	Initializer()
	{
		pls_qapp_construct_add_cb([this]() { this->init(); });
		pls_qapp_deconstruct_add_cb([this]() { this->cleanup(); });
	}

	static Initializer *initializer() { return &pls::Initializer<Initializer>::s_initializer; }

	void init() const
	{
		if (g_clientStatus != ClientStatus::Uninitialized) {
			return;
		}

		g_clientStatus = ClientStatus::Initializing;
		g_client = pls_new<Client>();
		g_clientStatus = ClientStatus::Initialized;
	}
	void cleanup() const
	{
		if (g_clientStatus != ClientStatus::Initialized) {
			return;
		}

		g_clientStatus = ClientStatus::Destroying;
		try {
			Client::destroy();
		} catch (...) {
			PLS_ERROR(LIBHTTP_CLIENT_MODULE, "http client destory exception");
			assert(false);
		}
		g_clientStatus = ClientStatus::Uninitialized;
	}
};

ClientStatus Client::s_clientStatus = ClientStatus::Uninitialized;
std::atomic<qint64> Client::s_requestId = 1;
Client *Client::s_client = nullptr;
int Client::s_cleanupWaitTimeout = 0; // no wait

qint64 genRequestId()
{
	return Client::s_requestId++;
}

bool isTextContent(const QString &contentType)
{
	if (contentType.contains(QStringLiteral("application/json")) || contentType.contains(QStringLiteral("text/html")) || contentType.contains(QStringLiteral("text/plain")) ||
	    contentType.contains(QStringLiteral("application/javascript")) || contentType.contains(QStringLiteral("text/javascript"))) {
		return true;
	}
	return false;
}

NetworkAccessManager::NetworkAccessManager() : QNetworkAccessManager(nullptr) {}

NetworkReply::NetworkReply(NetworkAccessManagerPtr manager, QNetworkReply *reply) : m_manager(manager), m_reply(reply) {}

void NetworkReply::init()
{
	if (auto reply = get(); reply) {
		QObject::connect(reply, &QNetworkReply::downloadProgress, this, &NetworkReply::downloadProgress);
		QObject::connect(reply, &QNetworkReply::readyRead, this, &NetworkReply::readyRead);
		QObject::connect(reply, &QNetworkReply::finished, this, &NetworkReply::finished);
		QObject::connect(reply, &QNetworkReply::destroyed, [weakThis = weak_from_this()]() mutable {
			if (auto pthis = weakThis.lock(); pthis) {
				pthis->m_manager = nullptr;
				pthis->m_reply = nullptr;
			}
		});
	}
}

void NetworkReply::destroy()
{
	if (auto reply = get(); reply) {
		QObject::disconnect(reply);
		reply->deleteLater();
	}

	m_manager = nullptr;
	m_reply = nullptr;
}

bool NetworkReply::valid() const
{
	return m_reply.valid();
}

std::optional<QUrl> NetworkReply::url() const
{
	callReplyMethodRet(url(), std::nullopt);
}
int NetworkReply::statusCode() const
{
	callReplyMethodRet(attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), -1);
}

Status NetworkReply::status() const
{
	return m_status.value_or(Status::InProgress);
}

void NetworkReply::setStatus(Status status)
{
	if (this->status() == Status::InProgress) {
		m_status = status;
	}
}

QNetworkReply::NetworkError NetworkReply::error() const
{
	callReplyMethodRet(error(), QNetworkReply::NoError);
}
QString NetworkReply::errorString() const
{
	callReplyMethodRet(errorString(), {});
}

QVariant NetworkReply::header(QNetworkRequest::KnownHeaders header) const
{
	callReplyMethodRet(header(header), {});
}
bool NetworkReply::hasRawHeader(const QByteArray &header) const
{
	callReplyMethodRet(hasRawHeader(header), false);
}
QByteArray NetworkReply::rawHeader(const QByteArray &header) const
{
	callReplyMethodRet(rawHeader(header), {});
}

QByteArray NetworkReply::data()
{
	callReplyMethodRet(readAll(), {});
}

void NetworkReply::trigger()
{
	// not implement
}

void NetworkReply::abort()
{
	callReplyMethod(abort());
}

QString NetworkReply::requestContentType() const
{
	callReplyMethodRet(request().header(QNetworkRequest::ContentTypeHeader).toString(), {});
}
QList<QNetworkReply::RawHeaderPair> NetworkReply::requestRawHeaders() const
{
	auto getRequest = [this]() -> QNetworkRequest { callReplyMethodRet(request(), {}); };
	auto request = getRequest();

	QList<QNetworkReply::RawHeaderPair> pairs;
	for (const auto &name : request.rawHeaderList())
		pairs.append({name, request.rawHeader(name)});
	return pairs;
}
QList<QNetworkReply::RawHeaderPair> NetworkReply::replyRawHeaders() const
{
	callReplyMethodRet(rawHeaderPairs(), {});
}

Worker::Worker(bool isShared) : m_isShared(isShared)
{
	m_callProxy = pls_new<QObject>();
	m_callProxy->moveToThread(this);

	QSemaphore *semaphore = pls_new<QSemaphore>();
	setProperty("$libhttp-client.startWaitSemaphore", QVariant::fromValue((void *)semaphore));
	start();
	semaphore->acquire();

	setProperty("$libhttp-client.startWaitSemaphore", QVariant());
	pls_delete(semaphore);
}

bool Worker::isShared() const
{
	return m_isShared;
}
QObject *Worker::callProxy() const
{
	return m_callProxy;
}

void Worker::syncCall(const std::function<void()> &fn) const
{
	if (pls_object_is_valid(this)) {
		pls_sync_call(m_callProxy, fn);
	}
}
void Worker::asyncCall(const std::function<void()> &fn) const
{
	if (pls_object_is_valid(this)) {
		pls_async_call(m_callProxy, fn);
	}
}

void Worker::quitAndWait()
{
	quit();
	wait();
}

bool Worker::useForHttpClient(const QThread *worker)
{
	return worker->property("$worker.use-for-http-client").toBool();
}
void Worker::setUseForHttpClient(QThread *worker, bool useForHttpClient)
{
	worker->setProperty("$worker.use-for-http-client", useForHttpClient);
}

void Worker::run()
{
	auto semaphore = (QSemaphore *)property("$libhttp-client.startWaitSemaphore").value<void *>();
	semaphore->release();

	exec();

	pls_delete(m_callProxy, nullptr);
}

SharedWorker::SharedWorker() : Worker(true)
{
	//constructor
}

ExclusiveWorker::ExclusiveWorker() : Worker(false)
{
	//constructor
}

Request::Request(Exclude exclude)
{
	m_impl = makeShared<RequestImpl>();
	m_impl->m_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
	m_impl->m_request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
	if (Client::isInitialized()) {
		if (!(exclude & NoDefaultRequestHeaders)) {
			rawHeaders(pls_invoke_safe(g_client->m_factory));
		}
	}
}
Request::Request(const RequestImplPtr &impl) : m_impl(impl)
{
	//constructor
}
Request &Request::operator=(const RequestImplPtr &impl)
{
	optionalSetValueRet(m_impl, impl);
}

RequestImpl *Request::operator->() const
{
	return m_impl.get();
}

const QNetworkRequest &Request::request() const
{
	m_impl->buildUrl();
	return m_impl->m_request;
}

QString Request::id() const
{
	return m_impl->m_id;
}
const Request &Request::id(const QString &id) const
{
	optionalSetValueRet(m_impl->m_id, id);
}

QByteArray Request::method() const
{
	return m_impl->method();
}
const Request &Request::method(Method method) const
{
	optionalSetValueRet(m_impl->m_method, method);
}
const Request &Request::method(const QByteArray &method) const
{
	if (method == QByteArrayLiteral("Head")) {
		m_impl->m_method = Method::Head;
	} else if (method == QByteArrayLiteral("Get")) {
		m_impl->m_method = Method::Get;
	} else if (method == QByteArrayLiteral("Post")) {
		m_impl->m_method = Method::Post;
	} else if (method == QByteArrayLiteral("Put")) {
		m_impl->m_method = Method::Put;
	} else if (method == QByteArrayLiteral("Delete")) {
		m_impl->m_method = Method::Delete;
	} else {
		m_impl->m_method = Method::Custom;
		m_impl->m_customMethod = method;
	}
	return *this;
}

QUrl Request::originalUrl() const
{
	return m_impl->m_originalUrl;
}
QUrl Request::url() const
{
	return m_impl->m_url;
}
const Request &Request::url(const QUrl &url) const
{
	m_impl->m_originalUrl = url;
	return *this;
}
const Request &Request::hmacKey(const QByteArray &hmacKey) const
{
	m_impl->m_hmacKey = hmacKey;
	return *this;
}
const Request &Request::hmacUrl(const QUrl &url, const QByteArray &hmacKey) const
{
	m_impl->m_originalUrl = url;
	m_impl->m_hmacKey = hmacKey;
	return *this;
}
const Request &Request::urlParams(const QMap<QString, QString> &params) const
{
	m_impl->m_urlParams.insert(params);
	return *this;
}
const Request &Request::urlParams(const QVariantMap &params) const
{
	pls_for_each(params, [this](const QString &name, const QVariant &value) { m_impl->m_urlParams.insert(name, value.toString()); });
	return *this;
}
const Request &Request::urlParams(const QVariantHash &params) const
{
	pls_for_each(params, [this](const QString &name, const QVariant &value) { m_impl->m_urlParams.insert(name, value.toString()); });
	return *this;
}
const Request &Request::urlParam(const QString &name, const QString &value) const
{
	m_impl->m_urlParams.insert(name, value);
	return *this;
}
const Request &Request::urlParamPercentEncoding(const QString &name, const QString &value) const
{
	m_impl->m_urlParams.insert(name, QUrl::toPercentEncoding(value));
	return *this;
}
const Request &Request::contentType(const QString &contentType) const
{
	m_impl->m_request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
	return *this;
}
const Request &Request::jsonContentType() const
{
	return contentType("application/json;charset=UTF-8");
}
const Request &Request::header(QNetworkRequest::KnownHeaders header, const QVariant &value) const
{
	m_impl->m_request.setHeader(header, value);
	return *this;
}
const Request &Request::rawHeader(const QString &header, const QString &value) const
{
	m_impl->m_request.setRawHeader(header.toUtf8(), value.toUtf8());
	return *this;
}
const Request &Request::rawHeader(const QByteArray &header, const QByteArray &value) const
{
	m_impl->m_request.setRawHeader(header, value);
	return *this;
}
const Request &Request::rawHeader(const std::string &header, const std::string &value) const
{
	m_impl->m_request.setRawHeader(header.c_str(), value.c_str());
	return *this;
}
const Request &Request::rawHeaders(const QMap<QString, QString> &rawHeaders) const
{
	pls_for_each(rawHeaders, [this](const QString &name, const QString &value) { m_impl->m_request.setRawHeader(name.toUtf8(), value.toUtf8()); });
	return *this;
}
const Request &Request::rawHeaders(const QVariantMap &rawHeaders) const
{
	pls_for_each(rawHeaders, [this](const QString &name, const QVariant &value) { m_impl->m_request.setRawHeader(name.toUtf8(), value.toString().toUtf8()); });
	return *this;
}
const Request &Request::rawHeaders(const QVariantHash &rawHeaders) const
{
	pls_for_each(rawHeaders, [this](const QString &name, const QVariant &value) { m_impl->m_request.setRawHeader(name.toUtf8(), value.toString().toUtf8()); });
	return *this;
}
const Request &Request::rawHeaders(const pls::map<std::string, std::string> &rawHeaders) const
{
	std::for_each(rawHeaders.begin(), rawHeaders.end(), [this](const pls::map<std::string, std::string>::value_type &i) { m_impl->m_request.setRawHeader(i.first.c_str(), i.second.c_str()); });
	return *this;
}
const Request &Request::cookie(const QList<QNetworkCookie> &cookie) const
{
	pls_for_each(cookie, [this](const QNetworkCookie &c) { this->cookie(c); });
	return *this;
}
const Request &Request::cookie(const QNetworkCookie &cookie) const
{
	optionalAddValueChk(m_impl->m_cookie, cookie);
	m_impl->m_request.setHeader(QNetworkRequest::CookieHeader, QVariant::fromValue(m_impl->m_cookie.value()));
	return *this;
}
const Request &Request::cookie(const QString &cookie) const
{
	return this->cookie(pls_parse(cookie, QRegularExpression("[;\n]")));
}
const Request &Request::cookie(const QMap<QString, QString> &cookie) const
{
	pls_for_each(cookie, [this](const QString &name, const QString &value) { this->cookie(name, value); });
	return *this;
}
const Request &Request::cookie(const QString &name, const QString &value) const
{
	return this->cookie(QNetworkCookie(name.toUtf8(), value.toUtf8()));
}
const Request &Request::userAgent(const QString &userAgent) const
{
	return header(QNetworkRequest::UserAgentHeader, userAgent);
}
const Request &Request::ignoreCache() const
{
	m_impl->m_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
	m_impl->m_request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);
	return *this;
}
const Request &Request::additional(const std::function<void(QNetworkRequest *request)> &additional) const
{
	pls_invoke_safe(additional, &m_impl->m_request);
	return *this;
}

QByteArray Request::body() const
{
	return m_impl->body();
}
const Request &Request::body(const QByteArray &body) const
{
	optionalSetValueChkRet(m_impl->m_body, !body.isEmpty(), body);
}
const Request &Request::body(const QJsonObject &body) const
{
	return this->body(m_impl->m_textBody = QJsonDocument(body).toJson());
}
const Request &Request::body(const QJsonArray &body) const
{
	return this->body(m_impl->m_textBody = QJsonDocument(body).toJson());
}

QHash<QString, QPair<QStringList, bool>> Request::form() const
{
	optionalGetValueChkRet(m_impl->m_form, {});
}
const Request &Request::form(const QHash<QString, QString> &form) const
{
	pls_for_each(form, [this](const QString &name, const QString &value) { this->form(name, value); });
	return *this;
}
const Request &Request::form(const QVariantMap &form) const
{
	pls_for_each(form, [this](const QString &name, const QVariant &value) { this->form(name, value.toString()); });
	return *this;
}
const Request &Request::form(const QVariantHash &form) const
{
	pls_for_each(form, [this](const QString &name, const QVariant &value) { this->form(name, value.toString()); });
	return *this;
}
const Request &Request::form(const QHash<QString, QPair<QString, bool>> &form) const
{
	pls_for_each(form, [this](const QString &name, const QPair<QString, bool> &value) { this->form(name, value); });
	return *this;
}
const Request &Request::form(const QString &name, const QString &value, bool isFile) const
{
	return this->form(name, QPair<QStringList, bool>({value}, isFile));
}
const Request &Request::form(const QString &name, const QStringList &value, bool isFile) const
{
	return this->form(name, QPair<QStringList, bool>(value, isFile));
}
const Request &Request::form(const QString &name, const QPair<QString, bool> &value) const
{
	return this->form(name, QPair<QStringList, bool>({value.first}, value.second));
}
const Request &Request::form(const QString &name, const QPair<QStringList, bool> &value) const
{
	optionalAddHashValueChkRet(m_impl->m_form, name, value);
}
const Request &Request::formPercentEncoding(const QString &name, const QString &value) const
{
	return form(name, QUrl::toPercentEncoding(value));
}
const Request &Request::formPercentEncoding(const QString &name, const QStringList &values) const
{
	QStringList percentEncodingValues;
	pls_for_each(values, [&percentEncodingValues](const QString &value) { percentEncodingValues.append(QUrl::toPercentEncoding(value)); });
	return form(name, percentEncodingValues);
}

QSet<const QObject *> Request::receiver() const
{
	QSet<const QObject *> receivers;
	pls_for_each(m_impl->m_receiver, [&receivers](const auto &v) { receivers.insert(pls::get_object(v)); });
	return receivers;
}
QSet<pls::QObjectPtr<QObject>> Request::receiverex() const
{
	return m_impl->m_receiver;
}
const Request &Request::receiver(const QObject *receiver, const IsValid &isValid) const
{
	m_impl->m_receiver.insert(pls_qobject_ptr<QObject>(receiver));
	optionalSetValueChkRet(m_impl->m_isValid, isValid, isValid);
}
const Request &Request::receiver(const QSet<const QObject *> &receiver, const IsValid &isValid) const
{
	m_impl->m_receiver = pls_qobject_ptr_set<QObject>(receiver);
	optionalSetValueChkRet(m_impl->m_isValid, isValid, isValid);
}
QThread *Request::worker() const
{
	return m_impl->m_worker.value_or(nullptr);
}
const Request &Request::worker(QThread *worker) const
{
	optionalSetValueChkRet(m_impl->m_worker, worker, worker);
}
const Request &Request::workInMainThread() const
{
	return worker(qApp->thread());
}
const Request &Request::workInNewThread() const
{
	optionalSetValueRet(m_impl->m_workInNewThread, true);
}
const Request &Request::workInGroup(const QString &group) const
{
	optionalSetValueChkRet(m_impl->m_workInGroup, !group.isEmpty(), group);
}

int Request::timeout() const
{
	return m_impl->timeout();
}
const Request &Request::timeout(int timeout) const
{
	optionalSetValueChkRet(m_impl->m_timeout, timeout > 0, timeout);
}
bool Request::forDownload() const
{
	return m_impl->m_forDownload.value_or(false);
}
const Request &Request::forDownload(bool forDownload) const
{
	optionalSetValueChkRet(m_impl->m_forDownload, forDownload, forDownload);
}
const Request &Request::saveDir(const QString &saveDir) const
{
	optionalSetValueChkRet(m_impl->m_saveDir, !saveDir.isEmpty(), saveDir);
}
const Request &Request::saveFileName(const QString &saveFileName) const
{
	optionalSetValueChkRet(m_impl->m_saveFileName, !saveFileName.isEmpty(), saveFileName);
}
const Request &Request::saveFileNamePrefix(const QString &saveFileNamePrefix) const
{
	optionalSetValueChkRet(m_impl->m_saveFileNamePrefix, !saveFileNamePrefix.isEmpty(), saveFileNamePrefix);
}
const Request &Request::saveFilePath(const QString &saveFilePath) const
{
	optionalSetValueChkRet(m_impl->m_saveFilePath, !saveFilePath.isEmpty(), saveFilePath);
}

QVariant Request::attr(const QString &name) const
{
	if (m_impl->m_attrs.has_value()) {
		return m_impl->m_attrs.value().value(name);
	}
	return {};
}
const Request &Request::attrs(const QVariantHash &attrs) const
{
	pls_for_each(attrs, [this](const QString &name, const QVariant &value) { this->attr(name, value); });
	return *this;
}
const Request &Request::attr(const QString &name, const QVariant &value) const
{
	optionalAddHashValueChkRet(m_impl->m_attrs, name, value);
}

bool Request::allowAbort() const
{
	return m_impl->m_allowAbort;
}
const Request &Request::allowAbort(bool allowAbort) const
{
	optionalSetValueRet(m_impl->m_allowAbort, allowAbort);
}

const Request &Request::checkResult(const CheckResult &checkResult) const
{
	optionalSetValueChkRet(m_impl->m_checkResult, checkResult, checkResult);
}
const Request &Request::before(const Before &before) const
{
	optionalSetValueChkRet(m_impl->m_before, before, before);
}
const Request &Request::after(const After &after) const
{
	optionalSetValueChkRet(m_impl->m_after, after, after);
}
const Request &Request::result(const Result &result) const
{
	optionalSetValueChkRet(m_impl->m_result, result, result);
}
const Request &Request::okResult(const Result &okResult) const
{
	optionalSetValueChkRet(m_impl->m_okResult, okResult, okResult);
}
const Request &Request::failResult(const Result &failResult) const
{
	optionalSetValueChkRet(m_impl->m_failResult, failResult, failResult);
}
const Request &Request::progress(const Progress &progress) const
{
	optionalSetValueChkRet(m_impl->m_progress, progress, progress);
}

const Request &Request::withLog(const QString &urlMasking) const
{
	return withBeforeLog(urlMasking) //
		.withRequestHeaderBodyLog()
		.withAfterLog(urlMasking)
		.withReplyHeaderLog();
}
const Request &Request::withLog(const UrlMasking &urlMasking) const
{
	return withBeforeLog(urlMasking) //
		.withRequestHeaderBodyLog()
		.withAfterLog(urlMasking)
		.withReplyHeaderLog();
}
std::pair<QString, bool> getMaskingUrl(const UrlMasking &urlMasking, const QString &url)
{
	if (QString maskingUrl = pls_invoke_safe(urlMasking, url); !maskingUrl.isEmpty())
		return {maskingUrl, true};
	return {url, false};
}
QString getMaskedError(const pls::http::Reply &reply, const QString &_originalUrl, const QString &maskedUrl)
{
	auto errorStr = reply.errors();
	if (errorStr.contains(_originalUrl)) {
		errorStr.replace(_originalUrl, maskedUrl);
	}
	return errorStr;
}
bool isLogIncludeOk(bool ok, LogInclude logInclude)
{
	if (ok) {
		return logInclude == LogInclude::All || logInclude == LogInclude::Ok;
	}
	return false;
}
bool isLogIncludeFail(bool ok, LogInclude logInclude)
{
	if (!ok) {
		return logInclude == LogInclude::All || logInclude == LogInclude::Fail;
	}
	return false;
}
const Request &Request::withBeforeLog(const QString &urlMasking) const
{
	if (!urlMasking.isEmpty())
		return withBeforeLog([urlMasking](const QString &) { return urlMasking; });
	return withBeforeLog(UrlMasking());
}
const Request &Request::withBeforeLog(const UrlMasking &urlMasking) const
{
	return m_impl->beforeLog(*this, [urlMasking](const NetworkAccessManager *, const Request &request) {
		QByteArray method = request.method();
		QString originalUrl = request.url().toString();
		auto maskingUrl = getMaskingUrl(urlMasking, originalUrl);
		PLS_INFO(LIBHTTP_CLIENT_MODULE, "http request start: %s, url = %s.", method.constData(), maskingUrl.first.toUtf8().constData());
		if (maskingUrl.second) {
			PLS_INFO_KR(LIBHTTP_CLIENT_MODULE, "http request start: %s, url = %s.", method.constData(), originalUrl.toUtf8().constData());
		}
	});
}
const Request &Request::withAfterLog(const QString &urlMasking, LogInclude logInclude) const
{
	if (!urlMasking.isEmpty())
		return withAfterLog([urlMasking](const QString &) { return urlMasking; }, logInclude);
	return withAfterLog(UrlMasking(), logInclude);
}

template<typename OriginalUrl, typename MaskingUrl> static void processOkLog(const OriginalUrl &originalUrl, const MaskingUrl &maskingUrl, const Reply &reply)
{
	auto contentType = reply.contentType();
	bool hasLogtraceId = reply.hasRawHeader(X_LOGTRACE_TRACEID);
	if (hasLogtraceId) {
		PLS_LOGEX(PLS_LOG_INFO, LIBHTTP_CLIENT_MODULE, {{X_LOGTRACE_TRACEID_FIELD, reply.rawHeader(X_LOGTRACE_TRACEID).constData()}},
			  "http response success, url = %s, statusCode = %d, contentType = %s", maskingUrl.first.toUtf8().constData(), reply.statusCode(), contentType.toUtf8().constData());
	} else {
		PLS_INFO(LIBHTTP_CLIENT_MODULE, "http response success, url = %s, statusCode = %d, contentType = %s", maskingUrl.first.toUtf8().constData(), reply.statusCode(),
			 contentType.toUtf8().constData());
	}

	if (isTextContent(contentType)) {
		if (hasLogtraceId) {
			PLS_LOGEX_KR(PLS_LOG_INFO, LIBHTTP_CLIENT_MODULE, {{X_LOGTRACE_TRACEID_FIELD, reply.rawHeader(X_LOGTRACE_TRACEID).constData()}},
				     "http response success, url = %s, statusCode = %d, response data = %s", originalUrl.toUtf8().constData(), reply.statusCode(), reply.data().constData());
		} else {
			PLS_INFO_KR(LIBHTTP_CLIENT_MODULE, "http response success, url = %s, statusCode = %d, response data = %s", originalUrl.toUtf8().constData(), reply.statusCode(),
				    reply.data().constData());
		}
	}
}
template<typename OriginalUrl, typename MaskingUrl> static void processFailLog(const OriginalUrl &originalUrl, const MaskingUrl &maskingUrl, const Reply &reply)
{
	auto contentType = reply.contentType();
	bool hasLogtraceId = reply.hasRawHeader(X_LOGTRACE_TRACEID);
	if (hasLogtraceId) {
		//log_level, module_name, fields, format, ...
		PLS_LOGEX(PLS_LOG_ERROR, LIBHTTP_CLIENT_MODULE, {{X_LOGTRACE_TRACEID_FIELD, reply.rawHeader(X_LOGTRACE_TRACEID).constData()}},
			  "http response failed, url = %s, networkReplyError = %d, statusCode = %d, contentType = %s, Qt error: %s", maskingUrl.first.toUtf8().constData(), reply.error(),
			  reply.statusCode(), contentType.toUtf8().constData(), getMaskedError(reply, originalUrl, maskingUrl.first).toUtf8().constData());
	} else {
		PLS_ERROR(LIBHTTP_CLIENT_MODULE, "http response failed, url = %s, networkReplyError = %d, statusCode = %d, contentType = %s, Qt error: %s", maskingUrl.first.toUtf8().constData(),
			  reply.error(), reply.statusCode(), contentType.toUtf8().constData(), getMaskedError(reply, originalUrl, maskingUrl.first).toUtf8().constData());
	}

	if (isTextContent(contentType)) {
		if (hasLogtraceId) {
			PLS_LOGEX_KR(PLS_LOG_ERROR, LIBHTTP_CLIENT_MODULE, {{X_LOGTRACE_TRACEID_FIELD, reply.rawHeader(X_LOGTRACE_TRACEID).constData()}},
				     "http response failed, url = %s, networkReplyError = %d, statusCode = %d, Qt error: %s, response data = %s", maskingUrl.first.toUtf8().constData(), reply.error(),
				     reply.statusCode(), getMaskedError(reply, originalUrl, maskingUrl.first).toUtf8().constData(), reply.data().constData());
		} else {
			PLS_ERROR_KR(LIBHTTP_CLIENT_MODULE, "http response failed, url = %s, networkReplyError = %d, statusCode = %d, Qt error: %s, response data = %s",
				     maskingUrl.first.toUtf8().constData(), reply.error(), reply.statusCode(), getMaskedError(reply, originalUrl, maskingUrl.first).toUtf8().constData(),
				     reply.data().constData());
		}
	}
}
const Request &Request::withAfterLog(const UrlMasking &urlMasking, LogInclude logInclude) const
{
	return m_impl->afterLog(*this, [urlMasking, logInclude](const pls::http::Reply &reply) {
		QString originalUrl = reply.request().url().toString();
		auto maskingUrl = getMaskingUrl(urlMasking, originalUrl);
		bool ok = reply.isOk();
		if (isLogIncludeOk(ok, logInclude)) {
			processOkLog(originalUrl, maskingUrl, reply);
		}

		if (isLogIncludeFail(ok, logInclude)) {
			processFailLog(originalUrl, maskingUrl, reply);
		}
	});
}

const Request &Request::withRequestHeaderBodyLog() const
{
	return m_impl->requestHeaderBodyLog(*this, [](const Reply &reply) {
		auto request = reply.request();
		auto url = request->m_url.toString().toUtf8();

		QByteArrayList headers;
		pls_for_each(reply.requestRawHeaders(), [&headers](const auto &header) { headers.append(header.first + "=" + header.second); });
		PLS_INFO_KR(LIBHTTP_CLIENT_MODULE, "http request headers, url = %s\n%s", url.constData(), headers.join('\n').constData());
		if (!request->m_textBody.isEmpty())
			PLS_INFO_KR(LIBHTTP_CLIENT_MODULE, "http request text body, url = %s\n%s", url.constData(), request->m_textBody.constData());
	});
}
const Request &Request::withReplyHeaderLog() const
{
	return m_impl->replyHeaderLog(*this, [](const Reply &reply) {
		auto request = reply.request();
		auto url = request->m_url.toString().toUtf8();

		QByteArrayList headers;
		pls_for_each(reply.replyRawHeaders(), [&headers](const auto &header) { headers.append(header.first + "=" + header.second); });
		PLS_INFO_KR(LIBHTTP_CLIENT_MODULE, "http reply headers, url = %s\n%s", url.constData(), headers.join('\n').constData());
	});
}

const Request &Request::jsonOkResult(const JsonResult<QJsonDocument> &jsonResult) const
{
	return okResult([jsonResult](const Reply &reply) {
		reply->json([reply, jsonResult](const QJsonDocument &json) { pls_invoke_safe(jsonResult, reply, json); },
			    [reply](const QJsonParseError &error) {
				    reply->addError(error.errorString());
				    reply->failResult(reply.m_impl);
			    });
	});
}
const Request &Request::jsonOkResult(const JsonResult<QJsonDocument> &ok, const JsonResultFail &fail) const
{
	return okResult([ok, fail](const Reply &reply) {
		reply->json([reply, ok](const QJsonDocument &json) { pls_invoke_safe(ok, reply, json); }, [reply, fail](const QJsonParseError &error) { pls_invoke_safe(fail, reply, error); });
	});
}
const Request &Request::arrayOkResult(const JsonResult<QJsonArray> &arrayResult) const
{
	return okResult([arrayResult](const Reply &reply) {
		reply->array([reply, arrayResult](const QJsonArray &array) { pls_invoke_safe(arrayResult, reply, array); },
			     [reply](const QJsonParseError &error) {
				     reply->addError(error.errorString());
				     reply->failResult(reply.m_impl);
			     });
	});
}
const Request &Request::arrayOkResult(const JsonResult<QJsonArray> &ok, const JsonResultFail &fail) const
{
	return okResult([ok, fail](const Reply &reply) {
		reply->array([reply, ok](const QJsonArray &array) { pls_invoke_safe(ok, reply, array); }, [reply, fail](const QJsonParseError &error) { pls_invoke_safe(fail, reply, error); });
	});
}
const Request &Request::objectOkResult(const JsonResult<QJsonObject> &objectResult) const
{
	return okResult([objectResult](const Reply &reply) {
		reply->object([reply, objectResult](const QJsonObject &object) { pls_invoke_safe(objectResult, reply, object); },
			      [reply](const QJsonParseError &error) {
				      reply->addError(error.errorString());
				      reply->failResult(reply.m_impl);
			      });
	});
}
const Request &Request::objectOkResult(const JsonResult<QJsonObject> &ok, const JsonResultFail &fail) const
{
	return okResult([ok, fail](const Reply &reply) {
		reply->object([reply, ok](const QJsonObject &object) { pls_invoke_safe(ok, reply, object); }, [reply, fail](const QJsonParseError &error) { pls_invoke_safe(fail, reply, error); });
	});
}

const Request &Request::jsonFailResult(const JsonResult<QJsonDocument> &jsonResult) const
{
	return failResult([jsonResult](const Reply &reply) {
		reply->json([reply, jsonResult](const QJsonDocument &json) { pls_invoke_safe(jsonResult, reply, json); },
			    [reply, jsonResult](const QJsonParseError &) { pls_invoke_safe(jsonResult, reply, QJsonDocument()); });
	});
}
const Request &Request::jsonFailResult(const JsonResult<QJsonDocument> &ok, const JsonResultFail &fail) const
{
	return failResult([ok, fail](const Reply &reply) {
		reply->json([reply, ok](const QJsonDocument &json) { pls_invoke_safe(ok, reply, json); }, [reply, fail](const QJsonParseError &error) { pls_invoke_safe(fail, reply, error); });
	});
}
const Request &Request::arrayFailResult(const JsonResult<QJsonArray> &arrayResult) const
{
	return failResult([arrayResult](const Reply &reply) {
		reply->array([reply, arrayResult](const QJsonArray &array) { pls_invoke_safe(arrayResult, reply, array); },
			     [reply, arrayResult](const QJsonParseError &) { pls_invoke_safe(arrayResult, reply, QJsonArray()); });
	});
}
const Request &Request::arrayFailResult(const JsonResult<QJsonArray> &ok, const JsonResultFail &fail) const
{
	return failResult([ok, fail](const Reply &reply) {
		reply->array([reply, ok](const QJsonArray &array) { pls_invoke_safe(ok, reply, array); }, [reply, fail](const QJsonParseError &error) { pls_invoke_safe(fail, reply, error); });
	});
}
const Request &Request::objectFailResult(const JsonResult<QJsonObject> &objectResult) const
{
	return failResult([objectResult](const Reply &reply) {
		reply->object([reply, objectResult](const QJsonObject &object) { pls_invoke_safe(objectResult, reply, object); },
			      [reply, objectResult](const QJsonParseError &) { pls_invoke_safe(objectResult, reply, QJsonObject()); });
	});
}
const Request &Request::objectFailResult(const JsonResult<QJsonObject> &ok, const JsonResultFail &fail) const
{
	return failResult([ok, fail](const Reply &reply) {
		reply->object([reply, ok](const QJsonObject &object) { pls_invoke_safe(ok, reply, object); }, [reply, fail](const QJsonParseError &error) { pls_invoke_safe(fail, reply, error); });
	});
}

void Request::abort(bool allowAbortCheck) const
{
	if (g_client && g_proxy) {
		Client::abort(allowAbortCheck, m_impl->m_requestId);
	}
}
bool Request::hasErrors() const
{
	return m_impl->hasErrors();
}
QString Request::errors() const
{
	return m_impl->errors();
}

QDateTime Request::startTime() const
{
	return m_impl->m_startTime;
}

void Request::start() const
{
	m_impl->start();
}

QDateTime Request::endTime() const
{
	return m_impl->m_endTime;
}
void Request::end() const
{
	m_impl->end();
}

Reply::Reply(const ReplyImplPtr &impl) : m_impl(impl)
{
	//constructor
}
Reply &Reply::operator=(const ReplyImplPtr &impl)
{
	optionalSetValueRet(m_impl, impl);
}

ReplyImpl *Reply::operator->() const
{
	return m_impl.get();
}

NetworkAccessManager *Reply::manager() const
{
	return m_impl->manager();
}
Request Reply::request() const
{
	return m_impl->request();
}

bool Reply::isOk() const
{
	if (request().forDownload()) {
		return m_impl->isDownloadOk(m_impl);
	}
	return m_impl->isOk();
}
bool Reply::isFailed() const
{
	return m_impl->isFailed();
}
bool Reply::isTimeout() const
{
	return m_impl->isTimeout();
}
bool Reply::isAborted() const
{
	return m_impl->isAborted();
}
bool Reply::isRenameFailed() const
{
	return m_impl->isRenameFailed();
}

QUrl Reply::url() const
{
	return m_impl->url();
}
int Reply::statusCode() const
{
	return m_impl->statusCode();
}
QNetworkReply::NetworkError Reply::error() const
{
	return m_impl->error();
}
int Reply::percent() const
{
	return m_impl->m_percent;
}
bool Reply::hasErrors() const
{
	return m_impl->m_requestImpl->hasErrors() || (m_impl->error() != QNetworkReply::NoError);
}
QString Reply::errors() const
{
	if (bool qterr = m_impl->error() != QNetworkReply::NoError, innererr = m_impl->m_requestImpl->hasErrors(); qterr && innererr) {
		return m_impl->errorString() + '\n' + m_impl->m_requestImpl->errors();
	} else if (qterr) {
		return m_impl->errorString();
	} else if (innererr) {
		return m_impl->m_requestImpl->errors();
	}
	return {};
}

void Reply::abort(bool allowAbortCheck) const
{
	request().abort(allowAbortCheck);
}

QVariant Reply::header(QNetworkRequest::KnownHeaders header) const
{
	return m_impl->header(header);
}
bool Reply::hasRawHeader(const QByteArray &header) const
{
	return m_impl->hasRawHeader(header);
}
QByteArray Reply::rawHeader(const QByteArray &header) const
{
	return m_impl->rawHeader(header);
}
QString Reply::contentType() const
{
	return m_impl->contentType();
}

QByteArray Reply::data() const
{
	return m_impl->data();
}
QJsonDocument Reply::json(const JsonParseFail &fail) const
{
	return m_impl->json(fail);
}
void Reply::json(const JsonParseOk<QJsonDocument> &ok, const JsonParseFail &fail) const
{
	m_impl->json(ok, fail);
}
QJsonArray Reply::array(const JsonParseFail &fail) const
{
	return m_impl->array(fail);
}
void Reply::array(const JsonParseOk<QJsonArray> &ok, const JsonParseFail &fail) const
{
	m_impl->array(ok, fail);
}
QJsonObject Reply::object(const JsonParseFail &fail) const
{
	return m_impl->object(fail);
}
void Reply::object(const JsonParseOk<QJsonObject> &ok, const JsonParseFail &fail) const
{
	m_impl->object(ok, fail);
}

bool Reply::isDownloadOk() const
{
	return m_impl->isDownloadOk(m_impl);
}
qint64 Reply::downloadedBytes() const
{
	return m_impl->downloadedBytes();
}
qint64 Reply::downloadTotalBytes() const
{
	return m_impl->downloadTotalBytes();
}
QString Reply::downloadFilePath() const
{
	optionalGetValueChkRet(m_impl->m_downloadFilePath, {});
}

QString Reply::requestContentType() const
{
	return m_impl->m_reply->requestContentType();
}
QList<QNetworkReply::RawHeaderPair> Reply::requestRawHeaders() const
{
	return m_impl->m_reply->requestRawHeaders();
}
QList<QNetworkReply::RawHeaderPair> Reply::replyRawHeaders() const
{
	return m_impl->m_reply->replyRawHeaders();
}

Requests::Requests() : Requests(makeShared<RequestsImpl>())
{
	//constructor
}
Requests::Requests(const RequestsImplPtr &impl) : m_impl(impl)
{
	//constructor
}
Requests &Requests::operator=(const RequestsImplPtr &impl)
{
	optionalSetValueRet(m_impl, impl);
}

int Requests::count() const
{
	return m_impl->m_requestImpls.count();
}
Request Requests::get(int i) const
{
	return m_impl->m_requestImpls.at(i);
}
const Requests &Requests::add(const Request &request) const
{
	optionalAddValueRet(m_impl->m_requestImpls, request.m_impl);
}

QSet<const QObject *> Requests::receiver() const
{
	QSet<const QObject *> receivers;
	pls_for_each(m_impl->m_receiver, [&receivers](const auto &v) { receivers.insert(pls::get_object(v)); });
	return receivers;
}
QSet<pls::QObjectPtr<QObject>> Requests::receiverex() const
{
	return m_impl->m_receiver;
}
const Requests &Requests::receiver(const QObject *receiver, const IsValid &isValid) const
{
	m_impl->m_receiver.insert(pls_qobject_ptr<QObject>(receiver));
	optionalSetValueChkRet(m_impl->m_isValid, isValid, isValid);
}
const Requests &Requests::receiver(const QSet<const QObject *> &receiver, const IsValid &isValid) const
{
	m_impl->m_receiver = pls_qobject_ptr_set<QObject>(receiver);
	optionalSetValueChkRet(m_impl->m_isValid, isValid, isValid);
}
QThread *Requests::worker() const
{
	return m_impl->m_worker.value_or(nullptr);
}
const Requests &Requests::worker(QThread *worker) const
{
	optionalSetValueChkRet(m_impl->m_worker, worker, worker);
}
const Requests &Requests::workInMainThread() const
{
	return worker(qApp->thread());
}
const Requests &Requests::workInNewThread() const
{
	optionalSetValueRet(m_impl->m_workInNewThread, true);
}
const Requests &Requests::workInGroup(const QString &group) const
{
	optionalSetValueChkRet(m_impl->m_workInGroup, !group.isEmpty(), group);
}

const Requests &Requests::checkResult(const CheckResult &checkResult) const
{
	optionalSetValueChkRet(m_impl->m_checkResult, checkResult, checkResult);
}
const Requests &Requests::before(const Befores &before) const
{
	optionalSetValueChkRet(m_impl->m_before, before, before);
}
const Requests &Requests::after(const Afters &after) const
{
	optionalSetValueChkRet(m_impl->m_after, after, after);
}
const Requests &Requests::results(const Results &results) const
{
	optionalSetValueChkRet(m_impl->m_results, results, results);
}
const Requests &Requests::progress(const Progresses &progress) const
{
	optionalSetValueChkRet(m_impl->m_progress, progress, progress);
}

void Requests::abort(bool allowAbortCheck) const
{
	m_impl->abort(allowAbortCheck);
}

void Requests::start() const
{
	m_impl->start();
}

Replies::Replies(const RepliesImplPtr &impl) : m_impl(impl)
{
	//constructor
}

NetworkAccessManager *Replies::manager() const
{
	return m_impl->m_manager.get();
}
Requests Replies::requests() const
{
	return m_impl->m_requestsImpl;
}
QList<ReplyImplPtr> Replies::replies() const
{
	return m_impl->m_replyImpls;
}
void Replies::replies(const Result &result) const
{
	for (ReplyImplPtr reply : m_impl->m_replyImpls) {
		pls_invoke_safe(result, reply);
	}
}
Reply Replies::reply(int i) const
{
	return m_impl->m_replyImpls.at(i);
}
int Replies::percent() const
{
	return m_impl->m_percent;
}
bool Replies::isOk() const // all ok
{
	for (ReplyImplPtr reply : m_impl->m_replyImpls) {
		if (!reply->isOk()) {
			return false;
		}
	}
	return true;
}
bool Replies::isAnyOk() const // some ok
{
	for (ReplyImplPtr reply : m_impl->m_replyImpls) {
		if (reply->isOk()) {
			return true;
		}
	}
	return false;
}
bool Replies::isFailed() const // all failed
{
	for (ReplyImplPtr reply : m_impl->m_replyImpls) {
		if (!reply->isFailed()) {
			return false;
		}
	}
	return true;
}
bool Replies::isAnyFailed() const // some failed
{
	for (ReplyImplPtr reply : m_impl->m_replyImpls) {
		if (reply->isFailed()) {
			return true;
		}
	}
	return false;
}
bool Replies::isTimeout() const // all timeout
{
	for (ReplyImplPtr reply : m_impl->m_replyImpls) {
		if (!reply->isTimeout()) {
			return false;
		}
	}
	return true;
}
bool Replies::isAnyTimeout() const // some timeout
{
	for (ReplyImplPtr reply : m_impl->m_replyImpls) {
		if (reply->isTimeout()) {
			return true;
		}
	}
	return false;
}
bool Replies::isAborted() const // all aborted
{
	for (ReplyImplPtr reply : m_impl->m_replyImpls) {
		if (!reply->isAborted()) {
			return false;
		}
	}
	return true;
}
bool Replies::isAnyAborted() const // some aborted
{
	for (ReplyImplPtr reply : m_impl->m_replyImpls) {
		if (reply->isAborted()) {
			return true;
		}
	}
	return false;
}
bool Replies::isRenameFailed() const // all rename failed
{
	for (ReplyImplPtr reply : m_impl->m_replyImpls) {
		if (!reply->isRenameFailed()) {
			return false;
		}
	}
	return true;
}
bool Replies::isAnyRenameFailed() const // any rename failed
{
	for (ReplyImplPtr reply : m_impl->m_replyImpls) {
		if (reply->isRenameFailed()) {
			return true;
		}
	}
	return false;
}
void Replies::abort(bool allowAbortCheck) const
{
	requests().abort(allowAbortCheck);
}

IReplyHook::IReplyHook(const Request &request) : m_request(request.m_impl) {}

Request IReplyHook::request() const
{
	return m_request;
}

int IReplyHook::triggerDelay() const
{
	return 0;
}

QUrl IReplyHook::url() const
{
	return m_request->m_originalUrl;
}
int IReplyHook::statusCode() const
{
	return 200;
}

Status IReplyHook::status() const
{
	return Status::Ok;
}
QNetworkReply::NetworkError IReplyHook::error() const
{
	return QNetworkReply::NoError;
}
QString IReplyHook::errorString() const
{
	return {};
}

QVariant IReplyHook::header(QNetworkRequest::KnownHeaders header) const
{
	return {};
}
bool IReplyHook::hasRawHeader(const QByteArray &header) const
{
	return false;
}
QByteArray IReplyHook::rawHeader(const QByteArray &header) const
{
	return {};
}
QList<QNetworkReply::RawHeaderPair> IReplyHook::replyRawHeaders() const
{
	return {};
}

std::optional<QByteArray> IReplyHook::data() const
{
	return std::nullopt;
}
std::optional<QJsonDocument> IReplyHook::json() const
{
	return std::nullopt;
}
std::optional<QJsonArray> IReplyHook::array() const
{
	return std::nullopt;
}
std::optional<QJsonObject> IReplyHook::object() const
{
	return std::nullopt;
}
std::optional<QString> IReplyHook::filePath() const
{
	return std::nullopt;
}

LIBHTTPCLIENT_API bool checkResult(const Reply &reply)
{
	if (reply.error() != QNetworkReply::NoError) { // qt failed
		return false;
	} else if (reply.statusCode() >= 200 && reply.statusCode() < 300) { // http success 2xx
		return true;
	}
	return false;
}

LIBHTTPCLIENT_API QUrl buildHmacUrl(const QUrl &url, const QByteArray &hmacKey)
{
	return QUrl();
}

LIBHTTPCLIENT_API QString contentType2Suffix(const QString &contentType)
{
	if (contentType.isEmpty()) {
		return {};
	} else if (QString contentTypeLower = contentType.toLower(); contentTypeLower == QStringLiteral("image/png")) {
		return QStringLiteral(".png");
	} else if (contentTypeLower == QStringLiteral("image/gif")) {
		return QStringLiteral(".gif");
	} else if (contentTypeLower == QStringLiteral("image/jpeg")) {
		return QStringLiteral(".jpeg");
	} else if (contentTypeLower == QStringLiteral("image/bmp")) {
		return QStringLiteral(".bmp");
	} else if (contentTypeLower == QStringLiteral("image/webp")) {
		return QStringLiteral(".webp");
	} else if (contentTypeLower == QStringLiteral("image/tiff")) {
		return QStringLiteral(".tiff");
	} else if (contentTypeLower.contains(QStringLiteral("text/html"))) {
		return QStringLiteral(".html");
	} else if (contentTypeLower == QStringLiteral("application/zip")) {
		return QStringLiteral(".zip");
	} else if (contentTypeLower.contains(QStringLiteral("application/json"))) {
		return QStringLiteral(".json");
	} else if (contentTypeLower == QStringLiteral("audio/mpeg")) {
		return QStringLiteral(".mp3");
	} else {
		return {};
	}
}
LIBHTTPCLIENT_API QString suffix2ContentType(const QString &suffix)
{
	if (suffix.isEmpty()) {
		return {};
	}

	QString suffixLower = suffix.toLower();
	if (suffixLower == QStringLiteral(".png")) {
		return QStringLiteral("image/png");
	} else if (suffixLower == QStringLiteral(".gif")) {
		return QStringLiteral("image/gif");
	} else if (suffixLower == QStringLiteral(".jpg") || suffixLower == QStringLiteral(".jpeg")) {
		return QStringLiteral("image/jpeg");
	} else if (suffixLower == ".bmp") {
		return QStringLiteral("image/bmp");
	} else if (suffixLower == QStringLiteral(".webp")) {
		return QStringLiteral("image/webp");
	} else if (suffixLower == QStringLiteral(".tiff")) {
		return QStringLiteral("image/tiff");
	}
	return {};
}

LIBHTTPCLIENT_API void setCleanupWaitTimeout(int timeout)
{
	g_cleanupWaitTimeout = timeout;
}
LIBHTTPCLIENT_API void setDefaultRequestHeadersFactory(const DefaultRequestHeadersFactory &factory)
{
	if (Client::isInitialized()) {
		Client::setFactory(factory);
	}
}
LIBHTTPCLIENT_API void setWorkerGroupShareCount(const QString &group, int shareCount)
{
	if (Client::isInitialized() && (!group.isEmpty()) && (shareCount > 0)) {
		Client::setShareCount(group, shareCount);
	}
}

LIBHTTPCLIENT_API void request(const Request &request)
{
	if (Client::isInitialized()) {
		Client::request(request);
	}
}

LIBHTTPCLIENT_API void requests(const Requests &requests)
{
	if (Client::isInitialized()) {
		Client::requests(requests);
	}
}

LIBHTTPCLIENT_API void abortAll(bool allowAbortCheck)
{
	if (Client::isInitialized()) {
		Client::abortAll(allowAbortCheck);
	}
}

LIBHTTPCLIENT_API void hook(const QString &id, const ReplyHook &hook)
{
	if (Client::isInitialized()) {
		Client::hook(id, hook);
	}
}
LIBHTTPCLIENT_API void unhook(const QString &id)
{
	if (Client::isInitialized()) {
		Client::unhook(id);
	}
}

LIBHTTPCLIENT_API void monitor(const QString &id, const ReplyMonitor &monitor)
{
	if (Client::isInitialized()) {
		Client::monitor(id, monitor);
	}
}
LIBHTTPCLIENT_API void unmonitor(const QString &id)
{
	if (Client::isInitialized()) {
		Client::unmonitor(id);
	}
}

LIBHTTPCLIENT_API QStringList requestIds()
{
	if (Client::isInitialized()) {
		return Client::requestIds();
	}
	return {};
}
}
}

#include "libhttp-client.moc"
