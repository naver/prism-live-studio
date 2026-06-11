#if !defined(_PRISM_COMMON_LIBHTTPCLIENT_LIBHTTPCLIENT_H)
#define _PRISM_COMMON_LIBHTTPCLIENT_LIBHTTPCLIENT_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <list>
#include <memory>
#include <string>
#include <qstring.h>
#include <qstringlist.h>
#include <qjsondocument.h>
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qurl.h>
#include <qcoreapplication.h>
#include <qnetworkaccessmanager.h>
#include <qnetworkrequest.h>
#include <qnetworkreply.h>
#include <qnetworkcookie.h>
#include <qhttpmultipart.h>
#include <qmap.h>
#include <qobject.h>
#include <qvariant.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qpair.h>
#include <qset.h>

#include <libutils-api.h>

#ifdef Q_OS_WIN

#ifdef LIBHTTPCLIENT_LIB
#define LIBHTTPCLIENT_API __declspec(dllexport)
#else
#define LIBHTTPCLIENT_API __declspec(dllimport)
#endif

#else
#define LIBHTTPCLIENT_API

#endif

namespace pls {
namespace http {

class NetworkAccessManager;
class NetworkReply;
class Worker;
class SharedWorker;
class ExclusiveWorker;

struct Request;
struct Reply;
struct Requests;
struct Replies;

struct RequestImpl;
struct ReplyImpl;
struct RequestsImpl;
struct RepliesImpl;

using RequestImplPtr = std::shared_ptr<RequestImpl>;
using RequestsImplPtr = std::shared_ptr<RequestsImpl>;
using ReplyImplPtr = std::shared_ptr<ReplyImpl>;
using ReplyImplWeakPtr = std::weak_ptr<ReplyImpl>;
using RepliesImplPtr = std::shared_ptr<RepliesImpl>;
using NetworkAccessManagerPtr = std::shared_ptr<NetworkAccessManager>;
using NetworkAccessManagerWeakPtr = std::weak_ptr<NetworkAccessManager>;
using NetworkReplyPtr = std::shared_ptr<NetworkReply>;
using NetworkReplyWeakPtr = std::weak_ptr<NetworkReply>;

using IsValid = std::function<bool(const QObject *object)>;
using Result = std::function<void(Reply reply)>;
using Results = std::function<void(Replies replies)>;
using CheckResult = std::function<bool(Reply reply)>;
template<typename T> using JsonParseOk = std::function<void(const T &json)>;
using JsonParseFail = std::function<void(const QJsonParseError &error)>;
using Progress = std::function<void(Reply reply)>;
using Progresses = std::function<void(Replies replies)>;
using Before = std::function<void(NetworkAccessManager *manager, Request request)>;
using After = std::function<void(Reply reply)>;
using Befores = std::function<void(NetworkAccessManager *manager, Requests requests)>;
using Afters = std::function<void(Replies replies)>;
using HeaderLog = std::function<void(Reply reply)>;
template<typename T> using JsonResult = std::function<void(Reply reply, const T &json)>;
using JsonResultFail = std::function<void(Reply reply, const QJsonParseError &error)>;
using DefaultRequestHeadersFactory = std::function<QVariantMap()>;
using UrlMasking = std::function<QString(const QString &url)>;

enum class Method {
	Head,
	Get,    // get request
	Post,   // post request
	Put,    // put request
	Delete, // delete request
	Custom
};
enum class Status { //
	InProgress,
	Ok,
	Failed,
	Timeout,
	Aborted,
	RenameFailed
};
enum Exclude {
	NoExclude = 0,                // no exclude
	NoDefaultRequestHeaders = 0x1 // no default headers
};
enum class LogInclude {
	Ok,   // ok log
	Fail, // fail log
	All   // ok and fail log
};

LIBHTTPCLIENT_API extern const QString RS_DOWNLOAD_WORKER_GROUP;

const int REQUEST_DEFAULT_TIMEOUT = 15000; // 15s

class LIBHTTPCLIENT_API NetworkAccessManager : public QNetworkAccessManager {
	Q_OBJECT

public:
	explicit NetworkAccessManager();

signals:
	void readyToClose();
};
class LIBHTTPCLIENT_API NetworkReply : public QObject, public std::enable_shared_from_this<NetworkReply> {
	Q_OBJECT

public:
	explicit NetworkReply(NetworkAccessManagerPtr manager, QNetworkReply *reply);

public:
	QNetworkReply *get() { return m_reply.object(); }
	const QNetworkReply *get() const { return m_reply.object(); }

	virtual void init();
	virtual void destroy();

	virtual bool valid() const;

	virtual std::optional<QUrl> url() const;
	virtual int statusCode() const;

	virtual Status status() const;
	void setStatus(Status status);

	virtual QNetworkReply::NetworkError error() const;
	virtual QString errorString() const;

	virtual QVariant header(QNetworkRequest::KnownHeaders header) const;
	virtual bool hasRawHeader(const QByteArray &header) const;
	virtual QByteArray rawHeader(const QByteArray &header) const;

	virtual QByteArray data();

	virtual void trigger();

	virtual void abort();

	virtual QString requestContentType() const;
	virtual QList<QNetworkReply::RawHeaderPair> requestRawHeaders() const;
	virtual QList<QNetworkReply::RawHeaderPair> replyRawHeaders() const;

signals:
	void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
	void readyRead();
	void finished();

protected:
	NetworkAccessManagerPtr m_manager;
	pls::QObjectPtr<QNetworkReply> m_reply;
	std::optional<Status> m_status = Status::InProgress;
};
class LIBHTTPCLIENT_API Worker : public QThread {
	Q_OBJECT

public:
	explicit Worker(bool isShared);

	bool isShared() const;
	QObject *callProxy() const;

	void syncCall(const std::function<void()> &fn) const;
	void asyncCall(const std::function<void()> &fn) const;

	void quitAndWait();

	static bool useForHttpClient(const QThread *worker);
	static void setUseForHttpClient(QThread *worker, bool useForHttpClient);

private:
	using QThread::start;

protected:
	void run() override;

private:
	bool m_isShared;
	QObject *m_callProxy = nullptr;
};
class LIBHTTPCLIENT_API SharedWorker : public Worker {
	Q_OBJECT

public:
	SharedWorker();
};
class LIBHTTPCLIENT_API ExclusiveWorker : public Worker {
	Q_OBJECT

public:
	ExclusiveWorker();
};

struct LIBHTTPCLIENT_API Request {
	mutable RequestImplPtr m_impl;

	explicit Request(Exclude exclude = Exclude::NoExclude);
	Request(const RequestImplPtr &impl);
	Request &operator=(const RequestImplPtr &impl);

	RequestImpl *operator->() const;

	const QNetworkRequest &request() const;

	QString id() const;
	const Request &id(const QString &id) const;

	QByteArray method() const;
	const Request &method(Method method) const;
	const Request &method(const QByteArray &method) const;

	QUrl originalUrl() const;
	QUrl url() const;
	const Request &url(const QUrl &url) const;
	const Request &hmacKey(const QByteArray &hmacKey) const;
	const Request &hmacUrl(const QUrl &url, const QByteArray &hmacKey) const;
	const Request &urlParams(const QMap<QString, QString> &params) const;
	const Request &urlParams(const QVariantMap &params) const;
	const Request &urlParams(const QVariantHash &params) const;
	const Request &urlParam(const QString &name, const QString &value) const;
	const Request &urlParamPercentEncoding(const QString &name, const QString &value) const;
	const Request &contentType(const QString &contentType) const;
	const Request &jsonContentType() const;
	const Request &header(QNetworkRequest::KnownHeaders header, const QVariant &value) const;
	const Request &rawHeader(const QString &header, const QString &value) const;
	const Request &rawHeader(const QByteArray &header, const QByteArray &value) const;
	const Request &rawHeader(const std::string &header, const std::string &value) const;
	const Request &rawHeaders(const QMap<QString, QString> &rawHeaders) const;
	const Request &rawHeaders(const QVariantMap &rawHeaders) const;
	const Request &rawHeaders(const QVariantHash &rawHeaders) const;
	const Request &rawHeaders(const pls::map<std::string, std::string> &rawHeaders) const;
	const Request &cookie(const QList<QNetworkCookie> &cookie) const;
	const Request &cookie(const QNetworkCookie &cookie) const;
	const Request &cookie(const QString &cookie) const;
	const Request &cookie(const QMap<QString, QString> &cookie) const;
	const Request &cookie(const QString &name, const QString &value) const;
	const Request &userAgent(const QString &userAgent) const;
	const Request &ignoreCache() const;
	const Request &additional(const std::function<void(QNetworkRequest *request)> &additional) const;

	QByteArray body() const;
	const Request &body(const QByteArray &body) const;
	const Request &body(const QJsonObject &body) const;
	const Request &body(const QJsonArray &body) const;

	QHash<QString, QPair<QStringList, bool>> form() const;
	const Request &form(const QHash<QString, QString> &form) const;
	const Request &form(const QVariantMap &form) const;
	const Request &form(const QVariantHash &form) const;
	const Request &form(const QHash<QString, QPair<QString, bool>> &form) const;
	const Request &form(const QString &name, const QString &value, bool isFile = false) const;
	const Request &form(const QString &name, const QStringList &value, bool isFile = false) const;
	const Request &form(const QString &name, const QPair<QString, bool> &value) const;
	const Request &form(const QString &name, const QPair<QStringList, bool> &value) const;
	const Request &formPercentEncoding(const QString &name, const QString &value) const;
	const Request &formPercentEncoding(const QString &name, const QStringList &values) const;

	QSet<const QObject *> receiver() const;
	QSet<pls::QObjectPtr<QObject>> receiverex() const;
	const Request &receiver(const QObject *receiver, const IsValid &isValid = nullptr) const;
	const Request &receiver(const QSet<const QObject *> &receiver, const IsValid &isValid = nullptr) const;

	QThread *worker() const;
	const Request &worker(QThread *worker) const;
	const Request &workInMainThread() const;
	const Request &workInNewThread() const;
	const Request &workInGroup(const QString &group) const;

	int timeout() const;
	const Request &timeout(int timeout) const;

	bool forDownload() const;
	const Request &forDownload(bool forDownload) const;
	const Request &saveDir(const QString &saveDir) const;
	const Request &saveFileName(const QString &saveFileName) const;
	const Request &saveFileNamePrefix(const QString &saveFileNamePrefix) const;
	const Request &saveFilePath(const QString &saveFilePath) const;

	QVariant attr(const QString &name) const;
	const Request &attrs(const QVariantHash &attrs) const;
	const Request &attr(const QString &name, const QVariant &value) const;

	bool allowAbort() const;
	const Request &allowAbort(bool allowAbort) const;

	const Request &checkResult(const CheckResult &checkResult) const;
	const Request &before(const Before &before) const;
	const Request &after(const After &after) const;
	const Request &result(const Result &result) const;
	const Request &okResult(const Result &okResult) const;
	const Request &failResult(const Result &failResult) const;
	const Request &progress(const Progress &progress) const;

	const Request &withLog(const QString &urlMasking = QString()) const;
	const Request &withLog(const UrlMasking &urlMasking) const;
	const Request &withBeforeLog(const QString &urlMasking = QString()) const;
	const Request &withBeforeLog(const UrlMasking &urlMasking) const;
	const Request &withAfterLog(const QString &urlMasking = QString(), LogInclude logInclude = LogInclude::All) const;
	const Request &withAfterLog(const UrlMasking &urlMasking, LogInclude logInclude = LogInclude::All) const;
	const Request &withRequestHeaderBodyLog() const;
	const Request &withReplyHeaderLog() const;

	const Request &jsonOkResult(const JsonResult<QJsonDocument> &jsonResult) const;
	const Request &jsonOkResult(const JsonResult<QJsonDocument> &ok, const JsonResultFail &fail) const;
	const Request &arrayOkResult(const JsonResult<QJsonArray> &arrayResult) const;
	const Request &arrayOkResult(const JsonResult<QJsonArray> &ok, const JsonResultFail &fail) const;
	const Request &objectOkResult(const JsonResult<QJsonObject> &objectResult) const;
	const Request &objectOkResult(const JsonResult<QJsonObject> &ok, const JsonResultFail &fail) const;

	const Request &jsonFailResult(const JsonResult<QJsonDocument> &jsonResult) const;
	const Request &jsonFailResult(const JsonResult<QJsonDocument> &ok, const JsonResultFail &fail) const;
	const Request &arrayFailResult(const JsonResult<QJsonArray> &arrayResult) const;
	const Request &arrayFailResult(const JsonResult<QJsonArray> &ok, const JsonResultFail &fail) const;
	const Request &objectFailResult(const JsonResult<QJsonObject> &objectResult) const;
	const Request &objectFailResult(const JsonResult<QJsonObject> &ok, const JsonResultFail &fail) const;

	void abort(bool allowAbortCheck = false) const;
	bool hasErrors() const;
	QString errors() const;

	QDateTime startTime() const;
	void start() const;
	QDateTime endTime() const;
	void end() const;
};
struct LIBHTTPCLIENT_API Reply {
	mutable ReplyImplPtr m_impl;

	Reply(const ReplyImplPtr &impl);
	Reply &operator=(const ReplyImplPtr &impl);

	ReplyImpl *operator->() const;

	NetworkAccessManager *manager() const;
	Request request() const;

	bool isOk() const;
	bool isFailed() const;
	bool isTimeout() const;
	bool isAborted() const;
	bool isRenameFailed() const;

	QUrl url() const;
	int statusCode() const;
	QNetworkReply::NetworkError error() const;
	int percent() const; // 0~100

	bool hasErrors() const;
	QString errors() const;

	void abort(bool allowAbortCheck = false) const;

	QVariant header(QNetworkRequest::KnownHeaders header) const;
	bool hasRawHeader(const QByteArray &header) const;
	QByteArray rawHeader(const QByteArray &header) const;
	QString contentType() const;

	QByteArray data() const;
	QJsonDocument json(const JsonParseFail &fail = nullptr) const;
	void json(const JsonParseOk<QJsonDocument> &ok, const JsonParseFail &fail = nullptr) const;
	QJsonArray array(const JsonParseFail &fail = nullptr) const;
	void array(const JsonParseOk<QJsonArray> &ok, const JsonParseFail &fail = nullptr) const;
	QJsonObject object(const JsonParseFail &fail = nullptr) const;
	void object(const JsonParseOk<QJsonObject> &ok, const JsonParseFail &fail = nullptr) const;

	bool isDownloadOk() const;
	qint64 downloadedBytes() const;
	qint64 downloadTotalBytes() const;
	QString downloadFilePath() const;

	QString requestContentType() const;
	QList<QNetworkReply::RawHeaderPair> requestRawHeaders() const;
	QList<QNetworkReply::RawHeaderPair> replyRawHeaders() const;
};

struct LIBHTTPCLIENT_API Requests {
	mutable RequestsImplPtr m_impl;

	Requests();
	Requests(const RequestsImplPtr &impl);
	Requests &operator=(const RequestsImplPtr &impl);

	int count() const;
	Request get(int i) const;
	const Requests &add(const Request &request) const;

	QSet<const QObject *> receiver() const;
	QSet<pls::QObjectPtr<QObject>> receiverex() const;
	const Requests &receiver(const QObject *receiver, const IsValid &isValid = nullptr) const;
	const Requests &receiver(const QSet<const QObject *> &receiver, const IsValid &isValid = nullptr) const;

	QThread *worker() const;
	const Requests &worker(QThread *worker) const;
	const Requests &workInMainThread() const;
	const Requests &workInNewThread() const;
	const Requests &workInGroup(const QString &group) const;

	const Requests &checkResult(const CheckResult &checkResult) const;
	const Requests &before(const Befores &before) const;
	const Requests &after(const Afters &after) const;
	const Requests &results(const Results &results) const;
	const Requests &progress(const Progresses &progress) const;

	void abort(bool allowAbortCheck = false) const;

	void start() const;
};
struct LIBHTTPCLIENT_API Replies {
	mutable RepliesImplPtr m_impl;

	Replies(const RepliesImplPtr &impl);

	NetworkAccessManager *manager() const;
	Requests requests() const;
	QList<ReplyImplPtr> replies() const;
	void replies(const Result &result) const;
	Reply reply(int i) const;
	int percent() const;            // 0~100
	bool isOk() const;              // all ok
	bool isAnyOk() const;           // some ok
	bool isFailed() const;          // all failed
	bool isAnyFailed() const;       // some failed
	bool isTimeout() const;         // all timeout
	bool isAnyTimeout() const;      // some timeout
	bool isAborted() const;         // all aborted
	bool isAnyAborted() const;      // some aborted
	bool isRenameFailed() const;    // all rename failed
	bool isAnyRenameFailed() const; // any rename failed
	void abort(bool allowAbortCheck = false) const;
};

struct LIBHTTPCLIENT_API IReplyHook {
	Request m_request;

	IReplyHook(const Request &request);
	virtual ~IReplyHook() = default;

	Request request() const;

	virtual int triggerDelay() const; // milliseconds

	virtual QUrl url() const;
	virtual int statusCode() const;

	virtual Status status() const;

	virtual QNetworkReply::NetworkError error() const;
	virtual QString errorString() const;

	virtual QVariant header(QNetworkRequest::KnownHeaders header) const;
	virtual bool hasRawHeader(const QByteArray &header) const;
	virtual QByteArray rawHeader(const QByteArray &header) const;
	virtual QList<QNetworkReply::RawHeaderPair> replyRawHeaders() const;

	virtual std::optional<QByteArray> data() const;
	virtual std::optional<QJsonDocument> json() const;
	virtual std::optional<QJsonArray> array() const;
	virtual std::optional<QJsonObject> object() const;
	virtual std::optional<QString> filePath() const;
};

LIBHTTPCLIENT_API bool checkResult(const Reply &reply);

LIBHTTPCLIENT_API QUrl buildHmacUrl(const QUrl &url, const QByteArray &hmacKey);
LIBHTTPCLIENT_API QString contentType2Suffix(const QString &contentType); // image/png, ...
LIBHTTPCLIENT_API QString suffix2ContentType(const QString &suffix);      // .png, ...

LIBHTTPCLIENT_API void setCleanupWaitTimeout(int timeout);
LIBHTTPCLIENT_API void setDefaultRequestHeadersFactory(const DefaultRequestHeadersFactory &factory);
LIBHTTPCLIENT_API void setWorkerGroupShareCount(const QString &group, int shareCount);

LIBHTTPCLIENT_API void request(const Request &request);
LIBHTTPCLIENT_API void requests(const Requests &requests);
LIBHTTPCLIENT_API void abortAll(bool allowAbortCheck = false);

using IReplyHookPtr = std::shared_ptr<IReplyHook>;
using ReplyHook = std::function<IReplyHookPtr(const Request &request)>;
LIBHTTPCLIENT_API void hook(const QString &id, const ReplyHook &hook);
LIBHTTPCLIENT_API void unhook(const QString &id);

using ReplyMonitor = std::function<void(const Request &request, const Reply &reply)>;
LIBHTTPCLIENT_API void monitor(const QString &id, const ReplyMonitor &monitor);
LIBHTTPCLIENT_API void unmonitor(const QString &id);

LIBHTTPCLIENT_API QStringList requestIds();
}
}

template<typename Fn> void pls_sync_call(QObject *proxy, const pls::http::Reply &reply, const Fn &fn)
{
	pls_sync_call(proxy, reply.request().receiverex(), fn);
}
template<typename Fn> void pls_async_call(QObject *proxy, const pls::http::Reply &reply, const Fn &fn)
{
	pls_async_call(proxy, reply.request().receiverex(), fn);
}
template<typename Fn> void pls_sync_call(QObject *proxy, const pls::http::Reply &reply, const QObject *object, const Fn &fn)
{
	pls_sync_call(proxy, reply.request().receiverex(), object, fn);
}
template<typename Fn> void pls_async_call(QObject *proxy, const pls::http::Reply &reply, const QObject *object, const Fn &fn)
{
	pls_async_call(proxy, reply.request().receiverex(), object, fn);
}
template<typename Fn> void pls_sync_call(QObject *proxy, const pls::http::Reply &reply, const QSet<const QObject *> &objects, const Fn &fn)
{
	pls_sync_call(proxy, reply.request().receiverex(), objects, fn);
}
template<typename Fn> void pls_async_call(QObject *proxy, const pls::http::Reply &reply, const QSet<const QObject *> &objects, const Fn &fn)
{
	pls_async_call(proxy, reply.request().receiverex(), objects, fn);
}
template<typename IsValid, typename Fn> void pls_sync_call(QObject *proxy, const pls::http::Reply &reply, const QObject *object, const IsValid &is_valid, const Fn &fn)
{
	pls_sync_call(proxy, reply.request().receiverex(), object, is_valid, fn);
}
template<typename IsValid, typename Fn> void pls_async_call(QObject *proxy, const pls::http::Reply &reply, const QObject *object, const IsValid &is_valid, const Fn &fn)
{
	pls_async_call(proxy, reply.request().receiverex(), object, is_valid, fn);
}
template<typename IsValid, typename Fn> void pls_sync_call(QObject *proxy, const pls::http::Reply &reply, const QSet<const QObject *> &objects, const IsValid &is_valid, const Fn &fn)
{
	pls_sync_call(proxy, reply.request().receiverex(), objects, is_valid, fn);
}
template<typename IsValid, typename Fn> void pls_async_call(QObject *proxy, const pls::http::Reply &reply, const QSet<const QObject *> &objects, const IsValid &is_valid, const Fn &fn)
{
	pls_async_call(proxy, reply.request().receiverex(), objects, is_valid, fn);
}

template<typename Fn> void pls_sync_call_mt(const pls::http::Reply &reply, const Fn &fn)
{
	pls_sync_call_mt(reply.request().receiverex(), fn);
}
template<typename Fn> void pls_async_call_mt(const pls::http::Reply &reply, const Fn &fn)
{
	pls_async_call_mt(reply.request().receiverex(), fn);
}
template<typename Fn> void pls_sync_call_mt(const pls::http::Reply &reply, const QObject *object, const Fn &fn)
{
	pls_sync_call_mt(reply.request().receiverex(), object, fn);
}
template<typename Fn> void pls_async_call_mt(const pls::http::Reply &reply, const QObject *object, const Fn &fn)
{
	pls_async_call_mt(reply.request().receiverex(), object, fn);
}
template<typename Fn> void pls_sync_call_mt(const pls::http::Reply &reply, const QSet<const QObject *> &objects, const Fn &fn)
{
	pls_sync_call_mt(reply.request().receiverex(), objects, fn);
}
template<typename Fn> void pls_async_call_mt(const pls::http::Reply &reply, const QSet<const QObject *> &objects, const Fn &fn)
{
	pls_async_call_mt(reply.request().receiverex(), objects, fn);
}
template<typename IsValid, typename Fn> void pls_sync_call_mt(const pls::http::Reply &reply, const QObject *object, const IsValid &isValid, const Fn &fn)
{
	pls_sync_call_mt(reply.request().receiverex(), object, isValid, fn);
}
template<typename IsValid, typename Fn> void pls_async_call_mt(const pls::http::Reply &reply, const QObject *object, const IsValid &isValid, const Fn &fn)
{
	pls_async_call_mt(reply.request().receiverex(), object, isValid, fn);
}
template<typename IsValid, typename Fn> void pls_sync_call_mt(const pls::http::Reply &reply, const QSet<const QObject *> &objects, const IsValid &isValid, const Fn &fn)
{
	pls_sync_call_mt(reply.request().receiverex(), objects, isValid, fn);
}
template<typename IsValid, typename Fn> void pls_async_call_mt(const pls::http::Reply &reply, const QSet<const QObject *> &objects, const IsValid &isValid, const Fn &fn)
{
	pls_async_call_mt(reply.request().receiverex(), objects, isValid, fn);
}

#endif // _PRISM_COMMON_LIBHTTPCLIENT_LIBHTTPCLIENT_H
