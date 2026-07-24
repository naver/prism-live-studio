#include "PLSNoticeUpdateRepository.hpp"

#include "frontend-api/frontend-api.h"
#include "libhttp-client.h"
#include "liblog.h"
#include "libutils-api.h"
#include "login-user-info.hpp"
#include "pls-common-define.hpp"

#include <QDateTime>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTimer>
#include <QUrlQuery>
#include <algorithm>
#include "prism-version.h"

constexpr int NOTICE_API_TIMEOUT_MS = 15000;
#if defined(PRODUCT_LENS)
#define APPTYPE QStringLiteral("LENS")
#elif defined(PRODUCT_PRISM)
#define APPTYPE QStringLiteral("LIVE_STUDIO")
#endif
namespace {
void applyNoticeRequestHeaders(pls::http::Request &request, const QString &appVersion)
{
	request.rawHeader(common::HTTP_HEAD_CC_TYPE, pls_get_locale().section('-', 1, 1))
		.rawHeader("X-prism-lang", pls_get_locale().section('-', 0, 0))
		.rawHeader("X-prism-apptype", APPTYPE)
		.rawHeader("X-prism-appversion", appVersion.isEmpty() ? PLS_VERSION : appVersion);
}
} // namespace

PLSNoticeUpdateRepository *PLSNoticeUpdateRepository::instance()
{
	static PLSNoticeUpdateRepository *s_instance = nullptr;
	if (!s_instance) {
		s_instance = new PLSNoticeUpdateRepository();
	}
	return s_instance;
}

PLSNoticeUpdateRepository::PLSNoticeUpdateRepository(QObject *parent) : QObject(parent)
{
	m_centerCacheState.inflightItems = std::make_shared<QMap<int, PLSNoticeUpdateItem>>();
	m_centerCacheStateB2B.inflightItems = std::make_shared<QMap<int, PLSNoticeUpdateItem>>();
}

bool PLSNoticeUpdateRepository::isItemValidNow(const PLSNoticeUpdateItem &item)
{
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	if (item.startAtMs > 0 && item.startAtMs > now)
		return false;
	if (item.endAtMs > 0 && item.endAtMs < now)
		return false;
	return true;
}

bool PLSNoticeUpdateRepository::itemSeqDesc(const PLSNoticeUpdateItem &a, const PLSNoticeUpdateItem &b)
{
	return a.id.toInt() > b.id.toInt();
}

QList<PLSNoticeUpdateItem> PLSNoticeUpdateRepository::mapToListDesc(const QMap<int, PLSNoticeUpdateItem> &map)
{
	QList<PLSNoticeUpdateItem> list;
	for (auto it = map.end(); it != map.begin();) {
		--it;
		list.append(it.value());
	}
	return list;
}

bool PLSNoticeUpdateRepository::parseNoticeV2Response(const QByteArray &data, PLSNoticeProvider provider, FetchPageResult *outResult)
{
	static const QString kRequestFailedError = QStringLiteral("Parse failed");
	if (!outResult) {
		return false;
	}
	*outResult = FetchPageResult();

	QJsonParseError parseErr;
	QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
	if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
		outResult->error = kRequestFailedError;
		PLS_ERROR("NoticeUpdate", "Notice API response parse failed. parseError=%d, isObject=%s", static_cast<int>(parseErr.error), doc.isObject() ? "true" : "false");
		return false;
	}

	QJsonObject root = doc.object();
	QJsonArray list;
	auto serviceName = root[QStringLiteral("serviceName")].toString();
	outResult->page = root[QStringLiteral("page")].toInt(0);
	outResult->size = root[QStringLiteral("size")].toInt(PLS_NOTICE_PAGE_SIZE);
	if (!root.contains(QStringLiteral("notice")) || !root[QStringLiteral("notice")].isArray()) {
		outResult->error = kRequestFailedError;
		PLS_ERROR("NoticeUpdate", "Notice API response field parse failed. notice field missing or invalid.");
		return false;
	} else {
		list = root[QStringLiteral("notice")].toArray();
	}
	outResult->count = root[QStringLiteral("count")].toInt(list.size());
	outResult->totalCount = root[QStringLiteral("totalCount")].toInt(outResult->count);

	for (const QJsonValue &val : list) {
		if (!val.isObject()) {
			outResult->error = kRequestFailedError;
			PLS_ERROR("NoticeUpdate", "Notice API response field parse failed. notice item is not an object.");
			return false;
		}
		QJsonObject obj = val.toObject();
		PLSNoticeUpdateItem item;
		item.provider = provider;
		QString noticeType = obj[QStringLiteral("noticeType")].toString().trimmed().toUpper();
		item.category = (provider == PLSNoticeProvider::PRISM && noticeType == QStringLiteral("UPDATE")) ? PLSNoticeCategory::Update : PLSNoticeCategory::Notice;
		item.providerName = (provider == PLSNoticeProvider::B2B) ? serviceName : QStringLiteral("PRISM Live Studio");

		if (obj.contains(QStringLiteral("seq"))) {
			item.id = QString::number(obj[QStringLiteral("seq")].toInt());
		} else if (obj.contains(QStringLiteral("noticeId"))) {
			QString serviceId = PLSLOGINUSERINFO ? PLSLOGINUSERINFO->getNCPPlatformServiceId() : QString();
			item.id = obj[QStringLiteral("noticeId")].toString().remove(serviceId);
		}
		if (item.id.isEmpty()) {
			outResult->error = kRequestFailedError;
			PLS_ERROR("NoticeUpdate", "Notice API response field parse failed. item id is empty.");
			return false;
		}

		item.title = obj[QStringLiteral("title")].toString();

		if (obj.contains(QStringLiteral("contentUrl")) && obj[QStringLiteral("contentUrl")].isObject()) {
			QJsonObject cu = obj[QStringLiteral("contentUrl")].toObject();
			item.contentPopUrl = cu[QStringLiteral("pc")].toString();
			item.contentDetailUrl = cu[QStringLiteral("detail")].toString();
			if (item.contentPopUrl.isEmpty())
				item.contentPopUrl = item.contentDetailUrl;
		}

		auto parseIsoMs = [](const QJsonValue &v) -> qint64 {
			if (v.isNull() || !v.isString())
				return 0;
			QString s = v.toString();
			if (s.isEmpty())
				return 0;
			QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
			return dt.isValid() ? dt.toMSecsSinceEpoch() : 0;
		};

		if (obj.contains(QStringLiteral("startAt"))) {
			item.startAtMs = parseIsoMs(obj[QStringLiteral("startAt")]);
			if (item.publishAtMs == 0)
				item.publishAtMs = item.startAtMs;
		}
		if (obj.contains(QStringLiteral("endAt")))
			item.endAtMs = parseIsoMs(obj[QStringLiteral("endAt")]);

		if (obj.contains(QStringLiteral("publishAt"))) {
			item.publishAtMs = static_cast<qint64>(obj[QStringLiteral("publishAt")].toDouble(0));
		} else if (obj.contains(QStringLiteral("createdTime"))) {
			item.publishAtMs = parseIsoMs(obj[QStringLiteral("createdTime")]);
			if (item.publishAtMs == 0 && item.startAtMs != 0)
				item.publishAtMs = item.startAtMs;
		}

		item.detailType = obj[QStringLiteral("detailType")].toString();
		item.imageIncluded = obj[QStringLiteral("imageIncluded")].toBool(false);

		outResult->items.append(item);
	}
	return true;
}

void PLSNoticeUpdateRepository::fetchNoticePageFromApiAsync(const QObject *receiver, int page, int limit, PLSNoticeProvider provider, const std::function<void(const FetchPageResult &)> &callback)
{
	QString baseUrl = provider == PLSNoticeProvider::B2B ? QString(PLS_NOTICE_B2B_URL).arg(PRISM_SSL) : QString(PLS_NOTICE_V2_URL).arg(PRISM_SSL);
	QUrl url(baseUrl);
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("viewType"), QStringLiteral("list"));
	q.addQueryItem(QStringLiteral("page"), QString::number(page));
	q.addQueryItem(QStringLiteral("size"), QString::number(limit));
	if (provider == PLSNoticeProvider::B2B) {
		const QString serviceId = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceId();
		if (!serviceId.isEmpty()) {
			q.addQueryItem(QStringLiteral("serviceId"), serviceId);
		}
	}
	url.setQuery(q);

	pls::http::Request request;
	request.method(pls::http::Method::Get).hmacUrl(url.toString(), PLS_PC_HMAC_KEY.toUtf8().constData()).jsonContentType().withLog().receiver(receiver).timeout(NOTICE_API_TIMEOUT_MS);
	applyNoticeRequestHeaders(request, QStringLiteral(PLS_VERSION));

	pls::http::request(request.okResult([this, callback, provider](const pls::http::Reply &reply) {
					  FetchPageResult result;
					  if (parseNoticeV2Response(reply.data(), provider, &result)) {
						  pls_async_call_mt(this, [callback, result]() { callback(result); });
					  } else {
						  pls_async_call_mt(this, [callback, result]() { callback(result); });
					  }
				  })
				   .failResult([this, callback, provider](const pls::http::Reply &reply) {
					   FetchPageResult result;
					   const int status = reply.statusCode();
					   if (status == 404) {
						   if (provider == PLSNoticeProvider::B2B) {
							   PLS_INFO("NoticeUpdate", "B2B Notice API 404.");
						   } else {
							   PLS_INFO("NoticeUpdate", "Notice API 404: no notices found for this version.");
						   }
						   pls_async_call_mt(this, [callback, result]() { callback(result); });
						   return;
					   }
					   result.error = reply.errors();
					   if (result.error.isEmpty())
						   result.error = QStringLiteral("Request failed");
					   if (provider == PLSNoticeProvider::B2B) {
						   PLS_ERROR("NoticeUpdate", "B2B Notice API failed. status=%d", status);
					   } else {
						   PLS_ERROR("NoticeUpdate", "Notice API request failed. status=%d, error=%s", status, result.error.toUtf8().constData());
					   }
					   pls_async_call_mt(this, [callback, result]() { callback(result); });
				   }));
}

void PLSNoticeUpdateRepository::fetchNoticeFromApiAsync(const QObject *receiver, int offset, int limit, const ListCallback &callback)
{
	fetchNoticePageFromApiAsync(receiver, offset, limit, PLSNoticeProvider::PRISM, [callback](const FetchPageResult &result) { callback(result.items, result.error); });
}

void PLSNoticeUpdateRepository::fetchB2BNoticeFromApiAsync(const QObject *receiver, int offset, int limit, const ListCallback &callback)
{
	fetchNoticePageFromApiAsync(receiver, offset, limit, PLSNoticeProvider::B2B, [callback](const FetchPageResult &result) { callback(result.items, result.error); });
}

bool PLSNoticeUpdateRepository::fetchNoticeFromApiSync(int offset, int limit, QList<PLSNoticeUpdateItem> *outItems, QString *outError)
{
	if (!outItems || !outError) {
		return false;
	}
	outItems->clear();
	outError->clear();

	QEventLoop loop;
	ListCallback cb = [outItems, outError, &loop](const QList<PLSNoticeUpdateItem> &items, const QString &err) {
		*outItems = items;
		*outError = err;
		loop.quit();
	};
	fetchNoticeFromApiAsync(this, offset, limit, cb);
	loop.exec();
	return outError->isEmpty();
}

void PLSNoticeUpdateRepository::fetchCurrentNoticeDetailFromApiAsync(const QObject *receiver, const QString &appVersion, const std::function<void(const FetchPageResult &)> &callback)
{
	QString baseUrl = QString(PLS_NOTICE_V2_URL).arg(PRISM_SSL);
	QUrl url(baseUrl);
	QUrlQuery q;
	q.addQueryItem(QStringLiteral("viewType"), QStringLiteral("current"));
	q.addQueryItem(QStringLiteral("noticeType"), QStringLiteral("UPDATE"));
	url.setQuery(q);

	pls::http::Request request;
	request.method(pls::http::Method::Get) //
		.hmacUrl(url.toString(), PLS_PC_HMAC_KEY.toUtf8().constData())
		.jsonContentType()
		.withLog()
		.receiver(receiver)
		.timeout(NOTICE_API_TIMEOUT_MS);
	applyNoticeRequestHeaders(request, appVersion);

	pls::http::request(request.okResult([this, callback](const pls::http::Reply &reply) {
					  FetchPageResult result;
					  parseNoticeV2Response(reply.data(), PLSNoticeProvider::PRISM, &result);
					  pls_async_call_mt(this, [callback, result]() { callback(result); });
				  })
				   .failResult([this, callback, appVersion](const pls::http::Reply &reply) {
					   FetchPageResult result;
					   const int status = reply.statusCode();
					   if (status == 404) {
						   PLS_INFO("NoticeUpdate", "Current notice API 404. version=%s", appVersion.isEmpty() ? PLS_VERSION : appVersion.toUtf8().constData());
						   pls_async_call_mt(this, [callback, result]() { callback(result); });
						   return;
					   }
					   result.error = reply.errors();
					   if (result.error.isEmpty())
						   result.error = QStringLiteral("Request failed");
					   PLS_ERROR("NoticeUpdate", "Current notice API request failed. status=%d, error=%s", status, result.error.toUtf8().constData());
					   pls_async_call_mt(this, [callback, result]() { callback(result); });
				   }));
}

void PLSNoticeUpdateRepository::fetchCurrentNoticeFromApiAsync(const QObject *receiver, const QString &appVersion, const ItemCallback &callback)
{
	fetchCurrentNoticeDetailFromApiAsync(receiver, appVersion, [callback](const FetchPageResult &result) {
		const PLSNoticeUpdateItem item = result.items.isEmpty() ? PLSNoticeUpdateItem() : result.items.first();
		callback(item, result.error);
	});
}

bool PLSNoticeUpdateRepository::fetchCurrentNoticeFromApiSync(const QString &appVersion, PLSNoticeUpdateItem *outItem, QString *outError)
{
	if (!outItem || !outError)
		return false;

	*outItem = {};
	outError->clear();

	QEventLoop loop;
	fetchCurrentNoticeFromApiAsync(this, appVersion, [outItem, outError, &loop](const PLSNoticeUpdateItem &item, const QString &err) {
		*outItem = item;
		*outError = err;
		loop.quit();
	});
	loop.exec();
	return outError->isEmpty();
}

void PLSNoticeUpdateRepository::refreshCenterCacheAsync(const QObject *receiver, bool isB2B, const RefreshCallback &callback, bool prefetchPeerCache)
{
	refreshSingleCenterCacheAsync(receiver, isB2B, callback);

	// In B2B mode, warm the sibling PRISM cache in background on first load.
	// Tab switches still use a single-cache refresh path.
	if (prefetchPeerCache && isB2B) {
		refreshSingleCenterCacheAsync(receiver, false, {});
	}
}

void PLSNoticeUpdateRepository::whenCenterCacheIdle(const QObject *receiver, bool isB2B, const RefreshCallback &callback)
{
	if (!callback)
		return;
	CenterCacheState &state = centerCacheState(isB2B);
	if (!state.isFetching) {
		const QString err = state.loadedCount > 0 ? QString() : state.lastError;
		pls_async_call_mt(receiver, [callback, err]() { callback(err); });
		return;
	}
	state.pendingIdleAfterFetch.push_back(PendingCenterCacheIdleWaiter{state.requestId, pls_qobject_ptr<QObject>(receiver), callback});
}

void PLSNoticeUpdateRepository::notifyActiveCenterRefreshSuperseded(CenterCacheState &state)
{
	if (!state.activeRefreshCallback) {
		state.activeRefreshReceiver = nullptr;
		return;
	}
	if (!pls_object_is_valid(state.activeRefreshReceiver)) {
		state.activeRefreshCallback = nullptr;
		state.activeRefreshReceiver = nullptr;
		return;
	}
	QObject *const recv = pls::get_object(state.activeRefreshReceiver);
	auto cb = std::move(state.activeRefreshCallback);
	state.activeRefreshCallback = nullptr;
	state.activeRefreshReceiver = nullptr;
	if (!recv || !cb) {
		return;
	}
	const QString superseded = QStringLiteral("__superseded__");
	pls_async_call_mt(recv, [cb = std::move(cb), superseded]() { cb(superseded); });
}

void PLSNoticeUpdateRepository::completePendingIdleWaiters(bool isB2B, int finishedRequestId, const QString &error)
{
	CenterCacheState &state = centerCacheState(isB2B);
	std::vector<PendingCenterCacheIdleWaiter> toRun;
	for (auto it = state.pendingIdleAfterFetch.begin(); it != state.pendingIdleAfterFetch.end();) {
		if (it->requestId == finishedRequestId) {
			toRun.push_back(std::move(*it));
			it = state.pendingIdleAfterFetch.erase(it);
		} else {
			++it;
		}
	}
	for (auto &w : toRun) {
		if (!w.callback || !pls_object_is_valid(w.receiver)) {
			continue;
		}
		QObject *const recv = pls::get_object(w.receiver);
		if (!recv) {
			continue;
		}
		auto cb = std::move(w.callback);
		pls_async_call_mt(recv, [cb = std::move(cb), error]() { cb(error); });
	}
}

void PLSNoticeUpdateRepository::refreshSingleCenterCacheAsync(const QObject *receiver, bool isB2B, const RefreshCallback &callback)
{
	CenterCacheState &state = centerCacheState(isB2B);
	if (state.isFetching) {
		completePendingIdleWaiters(isB2B, state.requestId, QStringLiteral("__superseded__"));
		notifyActiveCenterRefreshSuperseded(state);
	}
	const int requestId = state.requestId + 1;
	resetCenterCacheState(isB2B);
	state.requestId = requestId;
	state.isFetching = true;
	state.lastError.clear();
	state.activeRefreshReceiver = pls_qobject_ptr<QObject>(receiver);
	state.activeRefreshCallback = callback;

	auto fetchNextPage = std::make_shared<std::function<void(int)>>();
	*fetchNextPage = [this, receiver, isB2B, callback, fetchNextPage, requestId](int page) {
		const auto onPage = [this, receiver, isB2B, callback, fetchNextPage, requestId](const FetchPageResult &result) {
			CenterCacheState &currentState = centerCacheState(isB2B);
			if (currentState.requestId != requestId) {
				return;
			}
			if (!result.error.isEmpty()) {
				applyCenterCacheFetchResult(isB2B, result);
				const QString errOut = currentState.loadedCount > 0 ? QString() : result.error;
				currentState.activeRefreshReceiver = nullptr;
				currentState.activeRefreshCallback = nullptr;
				completePendingIdleWaiters(isB2B, requestId, errOut);
				if (callback) {
					pls_async_call_mt(receiver, [callback, errOut]() { callback(errOut); });
				}
				return;
			}

			applyCenterCachePage(isB2B, result);
			if (currentState.hasMoreRemote) {
				(*fetchNextPage)(currentState.nextPage);
				return;
			}

			currentState.isFetching = false;
			currentState.activeRefreshReceiver = nullptr;
			currentState.activeRefreshCallback = nullptr;
			completePendingIdleWaiters(isB2B, requestId, QString());
			if (callback) {
				pls_async_call_mt(receiver, [callback]() { callback(QString()); });
			}
		};

		fetchNoticePageFromApiAsync(receiver, page, PLS_NOTICE_PAGE_SIZE, isB2B ? PLSNoticeProvider::B2B : PLSNoticeProvider::PRISM, onPage);
	};
	(*fetchNextPage)(0);
}

bool PLSNoticeUpdateRepository::lessByPublishDesc(const PLSNoticeUpdateItem &lhs, const PLSNoticeUpdateItem &rhs)
{
	if (lhs.publishAtMs != rhs.publishAtMs)
		return lhs.publishAtMs > rhs.publishAtMs;
	return lhs.id.toInt() > rhs.id.toInt();
}

void PLSNoticeUpdateRepository::resetCenterCacheState(bool isB2B)
{
	CenterCacheState &state = centerCacheState(isB2B);
	state.inflightItems = std::make_shared<QMap<int, PLSNoticeUpdateItem>>();
	state.loadedCount = 0;
	state.totalCount = 0;
	state.nextPage = 0;
	state.hasMoreRemote = false;
	state.isFetching = false;
	state.activeRefreshReceiver = nullptr;
	state.activeRefreshCallback = nullptr;
}

void PLSNoticeUpdateRepository::applyCenterCacheFetchResult(bool isB2B, const FetchPageResult &result)
{
	CenterCacheState &state = centerCacheState(isB2B);
	if (!result.error.isEmpty()) {
		state.isFetching = false;
		state.hasMoreRemote = false;
		state.lastError = result.error;
		if (!state.firstLoadCompleted) {
			auto emptyMap = std::make_shared<QMap<int, PLSNoticeUpdateItem>>();
			state.inflightItems = emptyMap;
			state.items.setValue(emptyMap, true);
			state.firstLoadCompleted = true;
		}
		return;
	}

	applyCenterCachePage(isB2B, result);
}

void PLSNoticeUpdateRepository::applyCenterCachePage(bool isB2B, const FetchPageResult &result)
{
	CenterCacheState &state = centerCacheState(isB2B);
	auto baseMap = state.inflightItems ? state.inflightItems : std::make_shared<QMap<int, PLSNoticeUpdateItem>>();
	auto mergedMap = std::make_shared<QMap<int, PLSNoticeUpdateItem>>(*baseMap);
	for (const auto &item : result.items) {
		int seq = item.id.toInt();
		if (seq > 0) {
			mergedMap->insert(seq, item);
		}
	}
	state.inflightItems = mergedMap;
	state.items.setValue(mergedMap, true);
	state.firstLoadCompleted = true;
	state.lastError.clear();
	state.loadedCount = mergedMap->size();
	state.totalCount = qMax(result.totalCount, state.loadedCount);
	state.nextPage = result.page + 1;
	const bool fullPageLoaded = result.size <= 0 || result.count >= result.size;
	state.hasMoreRemote = fullPageLoaded && state.loadedCount < state.totalCount;
}

PLSNoticeUpdateRepository::CenterCacheState &PLSNoticeUpdateRepository::centerCacheState(bool isB2B)
{
	return isB2B ? m_centerCacheStateB2B : m_centerCacheState;
}

const PLSNoticeUpdateRepository::CenterCacheState &PLSNoticeUpdateRepository::centerCacheState(bool isB2B) const
{
	return isB2B ? m_centerCacheStateB2B : m_centerCacheState;
}

void PLSNoticeUpdateRepository::setCenterCache(const QList<PLSNoticeUpdateItem> &items, const QString &error)
{
	FetchPageResult result;
	result.items = items;
	result.count = items.size();
	result.size = PLS_NOTICE_PAGE_SIZE;
	result.totalCount = items.size();
	result.error = error;
	applyCenterCacheFetchResult(false, result);
}

std::shared_ptr<const QMap<int, PLSNoticeUpdateItem>> PLSNoticeUpdateRepository::getCenterCacheMap(int *outOffset, bool *outHasMore)
{
	auto &state = centerCacheState(false);
	auto map = state.items.value();
	if (outOffset)
		*outOffset = state.nextPage;
	if (outHasMore)
		*outHasMore = state.hasMoreRemote;
	if (!map)
		return std::make_shared<QMap<int, PLSNoticeUpdateItem>>();
	return map;
}

std::shared_ptr<QMap<int, PLSNoticeUpdateItem>> PLSNoticeUpdateRepository::getCenterCacheMapMutable(int *outOffset, bool *outHasMore)
{
	auto &state = centerCacheState(false);
	auto map = state.items.value();
	if (outOffset)
		*outOffset = state.nextPage;
	if (outHasMore)
		*outHasMore = state.hasMoreRemote;
	if (!map)
		return std::make_shared<QMap<int, PLSNoticeUpdateItem>>();
	return map;
}

void PLSNoticeUpdateRepository::setCenterCacheB2B(const QList<PLSNoticeUpdateItem> &items, const QString &error)
{
	FetchPageResult result;
	result.items = items;
	result.count = items.size();
	result.size = PLS_NOTICE_PAGE_SIZE;
	result.totalCount = items.size();
	result.error = error;
	applyCenterCacheFetchResult(true, result);
}

std::shared_ptr<const QMap<int, PLSNoticeUpdateItem>> PLSNoticeUpdateRepository::getCenterCacheMapB2B(int *outOffset, bool *outHasMore)
{
	auto &state = centerCacheState(true);
	auto map = state.items.value();
	if (outOffset)
		*outOffset = state.nextPage;
	if (outHasMore)
		*outHasMore = state.hasMoreRemote;
	if (!map)
		return std::make_shared<QMap<int, PLSNoticeUpdateItem>>();
	return map;
}

std::shared_ptr<QMap<int, PLSNoticeUpdateItem>> PLSNoticeUpdateRepository::getCenterCacheMapB2BMutable(int *outOffset, bool *outHasMore)
{
	auto &state = centerCacheState(true);
	auto map = state.items.value();
	if (outOffset)
		*outOffset = state.nextPage;
	if (outHasMore)
		*outHasMore = state.hasMoreRemote;
	if (!map)
		return std::make_shared<QMap<int, PLSNoticeUpdateItem>>();
	return map;
}

int PLSNoticeUpdateRepository::centerCacheLoadedCount(bool isB2B) const
{
	return centerCacheState(isB2B).loadedCount;
}

int PLSNoticeUpdateRepository::centerCacheTotalCount(bool isB2B) const
{
	return centerCacheState(isB2B).totalCount;
}

bool PLSNoticeUpdateRepository::centerCacheHasMoreRemote(bool isB2B) const
{
	return centerCacheState(isB2B).hasMoreRemote;
}

bool PLSNoticeUpdateRepository::centerCacheIsFetching(bool isB2B) const
{
	return centerCacheState(isB2B).isFetching;
}

QString PLSNoticeUpdateRepository::centerCacheLastError(bool isB2B) const
{
	return centerCacheState(isB2B).lastError;
}

QString PLSNoticeUpdateRepository::usedSeqsFilePath()
{
	return pls_get_app_user_data_file_path_pn(QStringLiteral("/user/notice_v2.json"));
}

bool PLSNoticeUpdateRepository::loadUsedSeqsFromFile(QSet<int> *outNoticeSeqs, QSet<int> *outUpdateSeqs, QSet<int> *outB2bNoticeSeqs)
{
	if (!outNoticeSeqs || !outUpdateSeqs) {
		return false;
	}
	outNoticeSeqs->clear();
	outUpdateSeqs->clear();
	if (outB2bNoticeSeqs)
		outB2bNoticeSeqs->clear();
	QFile file(usedSeqsFilePath());
	if (!file.open(QFile::ReadOnly)) {
		return true;
	}
	QJsonParseError parseErr;
	QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseErr);
	file.close();
	if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
		return true;
	}
	QJsonObject root = doc.object();
	for (const QJsonValue &v : root[QStringLiteral("notice")].toArray()) {
		if (v.isDouble())
			outNoticeSeqs->insert(static_cast<int>(v.toDouble()));
	}
	for (const QJsonValue &v : root[QStringLiteral("update")].toArray()) {
		if (v.isDouble())
			outUpdateSeqs->insert(static_cast<int>(v.toDouble()));
	}
	if (outB2bNoticeSeqs) {
		for (const QJsonValue &v : root[QStringLiteral("b2b_notice")].toArray()) {
			if (v.isDouble())
				outB2bNoticeSeqs->insert(static_cast<int>(v.toDouble()));
		}
	}
	return true;
}

bool PLSNoticeUpdateRepository::saveUsedSeqsToFile(const QSet<int> &noticeSeqs, const QSet<int> &updateSeqs, const QSet<int> &b2bNoticeSeqs)
{
	QJsonObject root;
	QJsonArray noticeArr;
	for (int seq : noticeSeqs)
		noticeArr.append(seq);
	root.insert(QStringLiteral("notice"), noticeArr);
	QJsonArray updateArr;
	for (int seq : updateSeqs)
		updateArr.append(seq);
	root.insert(QStringLiteral("update"), updateArr);
	QJsonArray b2bArr;
	for (int seq : b2bNoticeSeqs)
		b2bArr.append(seq);
	root.insert(QStringLiteral("b2b_notice"), b2bArr);
	QFile file(usedSeqsFilePath());
	if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
		PLS_ERROR("NoticeUpdate", "Failed to write notice_v2.json: %s", file.errorString().toUtf8().constData());
		return false;
	}
	file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
	file.close();
	return true;
}

QList<PLSNoticeUpdateItem> PLSNoticeUpdateRepository::getNewItemAndPersistSeqs()
{
	QSet<int> noticeSeqs;
	QSet<int> updateSeqs;
	QSet<int> b2bNoticeSeqs;
	loadUsedSeqsFromFile(&noticeSeqs, &updateSeqs, &b2bNoticeSeqs);
	auto map = m_centerCacheState.items.value();
	if (!map) {
		return {};
	}
	QList<PLSNoticeUpdateItem> items;
	for (auto it = map->cbegin(); it != map->cend(); ++it) {
		const auto &item = it.value();
		const int seq = item.id.toInt();
		if (seq <= 0 || item.category != PLSNoticeCategory::Notice)
			continue;
		if (!PLSNoticeUpdateRepository::isItemValidNow(item) || noticeSeqs.contains(seq))
			continue;
		items.append(item);
	}
	std::sort(items.begin(), items.end(), lessByPublishDesc);
	return items;
}

QList<PLSNoticeUpdateItem> PLSNoticeUpdateRepository::getNewB2BNoticeAndPersistSeqs()
{
	QSet<int> noticeSeqs;
	QSet<int> updateSeqs;
	QSet<int> b2bNoticeSeqs;
	loadUsedSeqsFromFile(&noticeSeqs, &updateSeqs, &b2bNoticeSeqs);

	auto map = m_centerCacheStateB2B.items.value();
	if (!map) {
		return {};
	}
	QList<PLSNoticeUpdateItem> items;
	for (auto it = map->cbegin(); it != map->cend(); ++it) {
		const auto &item = it.value();
		const int seq = item.id.toInt();
		if (seq <= 0 || item.category != PLSNoticeCategory::Notice)
			continue;
		if (!PLSNoticeUpdateRepository::isItemValidNow(item) || b2bNoticeSeqs.contains(seq))
			continue;
		items.append(item);
	}
	std::sort(items.begin(), items.end(), lessByPublishDesc);
	return items;
}

QList<PLSNoticeUpdateItem> PLSNoticeUpdateRepository::filterUnreadNoticeItems(const QList<PLSNoticeUpdateItem> &items) const
{
	QSet<int> noticeSeqs;
	QSet<int> updateSeqs;
	QSet<int> b2bNoticeSeqs;
	loadUsedSeqsFromFile(&noticeSeqs, &updateSeqs, &b2bNoticeSeqs);

	QList<PLSNoticeUpdateItem> unreadItems;
	for (const auto &item : items) {
		const int seq = item.id.toInt();
		if (seq <= 0 || item.category != PLSNoticeCategory::Notice || !isItemValidNow(item))
			continue;

		if (item.provider == PLSNoticeProvider::B2B) {
			if (!b2bNoticeSeqs.contains(seq))
				unreadItems.append(item);
			continue;
		}

		if (!noticeSeqs.contains(seq))
			unreadItems.append(item);
	}
	return unreadItems;
}

bool PLSNoticeUpdateRepository::hasUnreadNotice(bool forB2B)
{
	if (forB2B) {
		QSet<int> noticeSeqs, updateSeqs, b2bSeqs;
		loadUsedSeqsFromFile(&noticeSeqs, &updateSeqs, &b2bSeqs);
		auto map = m_centerCacheStateB2B.items.value();
		if (!map) {
			return false;
		}
		for (auto it = map->end(); it != map->begin();) {
			--it;
			int seq = it->id.toInt();
			if (seq > 0 && !b2bSeqs.contains(seq) && isItemValidNow(*it))
				return true;
		}
	}

	QSet<int> noticeSeqs, updateSeqs, b2bSeqs;
	loadUsedSeqsFromFile(&noticeSeqs, &updateSeqs, &b2bSeqs);
	auto map = m_centerCacheState.items.value();
	if (!map) {
		return false;
	}
	for (auto it = map->end(); it != map->begin();) {
		--it;
		int seq = it->id.toInt();
		if (seq <= 0 || !isItemValidNow(*it))
			continue;
		if (it->category == PLSNoticeCategory::Notice && !noticeSeqs.contains(seq))
			return true;
	}
	return false;
}

bool PLSNoticeUpdateRepository::hasUnreadNoticeOrUpdate(bool forB2B)
{
	if (forB2B) {
		QSet<int> noticeSeqs, updateSeqs, b2bSeqs;
		loadUsedSeqsFromFile(&noticeSeqs, &updateSeqs, &b2bSeqs);
		auto map = m_centerCacheStateB2B.items.value();
		if (!map) {
			return false;
		}
		for (auto it = map->end(); it != map->begin();) {
			--it;
			int seq = it->id.toInt();
			if (seq > 0 && !b2bSeqs.contains(seq) && isItemValidNow(*it))
				return true;
		}
	}

	QSet<int> noticeSeqs, updateSeqs, b2bSeqs;
	loadUsedSeqsFromFile(&noticeSeqs, &updateSeqs, &b2bSeqs);
	auto map = m_centerCacheState.items.value();
	if (!map) {
		return false;
	}
	for (auto it = map->end(); it != map->begin();) {
		--it;
		int seq = it->id.toInt();
		if (seq <= 0 || !isItemValidNow(*it))
			continue;
		if (it->category == PLSNoticeCategory::Notice && !noticeSeqs.contains(seq))
			return true;
		if (it->category == PLSNoticeCategory::Update && !updateSeqs.contains(seq))
			return true;
	}
	return false;
}

void PLSNoticeUpdateRepository::markItemAsRead(const PLSNoticeUpdateItem &item)
{
	int seq = item.id.toInt();
	if (seq <= 0)
		return;
	QSet<int> noticeSeqs, updateSeqs, b2bSeqs;
	loadUsedSeqsFromFile(&noticeSeqs, &updateSeqs, &b2bSeqs);
	bool changed = false;
	if (item.provider == PLSNoticeProvider::B2B) {
		if (!b2bSeqs.contains(seq)) {
			b2bSeqs.insert(seq);
			changed = true;
		}
	} else {
		if (item.category == PLSNoticeCategory::Notice) {
			if (!noticeSeqs.contains(seq)) {
				noticeSeqs.insert(seq);
				changed = true;
			}
		} else {
			if (!updateSeqs.contains(seq)) {
				updateSeqs.insert(seq);
				changed = true;
			}
		}
	}
	if (changed)
		saveUsedSeqsToFile(noticeSeqs, updateSeqs, b2bSeqs);
}
