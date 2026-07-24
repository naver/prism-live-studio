#ifndef PLS_NOTICE_UPDATE_REPOSITORY_HPP
#define PLS_NOTICE_UPDATE_REPOSITORY_HPP

#include "PLSNoticeUpdateTypes.hpp"

#include <QObject>
#include <QMap>
#include <functional>
#include <memory>
#include <utility>
#include <vector>
#include "libutils-api.h"

#define PLSNOTICEUPDATEREPOSITORY PLSNoticeUpdateRepository::instance()
constexpr int PLS_NOTICE_PAGE_SIZE = 50;

class PLSNoticeUpdateRepository : public QObject {
	Q_OBJECT

public:
	using ListCallback = std::function<void(const QList<PLSNoticeUpdateItem> &, const QString &)>;
	using ItemCallback = std::function<void(const PLSNoticeUpdateItem &, const QString &)>;
	struct FetchPageResult {
		QList<PLSNoticeUpdateItem> items;
		int page{0};
		int size{0};
		int count{0};
		int totalCount{0};
		QString error;
	};
	using RefreshCallback = std::function<void(const QString &)>;

	static PLSNoticeUpdateRepository *instance();

	static bool isItemValidNow(const PLSNoticeUpdateItem &item);
	static bool itemSeqDesc(const PLSNoticeUpdateItem &a, const PLSNoticeUpdateItem &b);
	static QList<PLSNoticeUpdateItem> mapToListDesc(const QMap<int, PLSNoticeUpdateItem> &map);

	void fetchNoticeFromApiAsync(const QObject *receiver, int offset, int limit, const ListCallback &callback);
	bool fetchNoticeFromApiSync(int offset, int limit, QList<PLSNoticeUpdateItem> *outItems, QString *outError);
	void fetchCurrentNoticeFromApiAsync(const QObject *receiver, const QString &appVersion, const ItemCallback &callback);
	bool fetchCurrentNoticeFromApiSync(const QString &appVersion, PLSNoticeUpdateItem *outItem, QString *outError);
	void refreshCenterCacheAsync(const QObject *receiver, bool isB2B, const RefreshCallback &callback, bool prefetchPeerCache = false);
	/** If that cache is idle, runs callback on receiver thread; else queues for when the current in-flight fetch for that cache ends (no extra HTTP). */
	void whenCenterCacheIdle(const QObject *receiver, bool isB2B, const RefreshCallback &callback);

	void setCenterCache(const QList<PLSNoticeUpdateItem> &items, const QString &error = {});
	/** Returns the center cache as a read-only map (blocking until ready). Optional offset/hasMore out-params. */
	std::shared_ptr<const QMap<int, PLSNoticeUpdateItem>> getCenterCacheMap(int *outOffset = nullptr, bool *outHasMore = nullptr);
	/** Returns the center cache map; caller may modify it (blocking until ready). Optional offset/hasMore out-params. */
	std::shared_ptr<QMap<int, PLSNoticeUpdateItem>> getCenterCacheMapMutable(int *outOffset = nullptr, bool *outHasMore = nullptr);

	void setCenterCacheB2B(const QList<PLSNoticeUpdateItem> &items, const QString &error = {});
	/** Returns the B2B center cache as a read-only map (blocking until ready). Optional offset/hasMore out-params. */
	std::shared_ptr<const QMap<int, PLSNoticeUpdateItem>> getCenterCacheMapB2B(int *outOffset = nullptr, bool *outHasMore = nullptr);
	/** Returns the B2B center cache map; caller may modify it (blocking until ready). Optional offset/hasMore out-params. */
	std::shared_ptr<QMap<int, PLSNoticeUpdateItem>> getCenterCacheMapB2BMutable(int *outOffset = nullptr, bool *outHasMore = nullptr);

	int centerCacheLoadedCount(bool isB2B) const;
	int centerCacheTotalCount(bool isB2B) const;
	bool centerCacheHasMoreRemote(bool isB2B) const;
	bool centerCacheIsFetching(bool isB2B) const;
	QString centerCacheLastError(bool isB2B) const;

	QList<PLSNoticeUpdateItem> getNewItemAndPersistSeqs();
	QList<PLSNoticeUpdateItem> getNewB2BNoticeAndPersistSeqs();
	QList<PLSNoticeUpdateItem> filterUnreadNoticeItems(const QList<PLSNoticeUpdateItem> &items) const;

	void fetchB2BNoticeFromApiAsync(const QObject *receiver, int offset, int limit, const ListCallback &callback);

	bool hasUnreadNotice(bool forB2B);
	bool hasUnreadNoticeOrUpdate(bool forB2B);
	void markItemAsRead(const PLSNoticeUpdateItem &item);

private:
	static QString usedSeqsFilePath();
	static bool loadUsedSeqsFromFile(QSet<int> *outNoticeSeqs, QSet<int> *outUpdateSeqs, QSet<int> *outB2bNoticeSeqs = nullptr);
	static bool saveUsedSeqsToFile(const QSet<int> &noticeSeqs, const QSet<int> &updateSeqs, const QSet<int> &b2bNoticeSeqs = {});

private:
	struct PendingCenterCacheIdleWaiter {
		int requestId{0};
		pls::QObjectPtr<QObject> receiver;
		RefreshCallback callback;
	};

	struct CenterCacheState {
		pls::AsyncResult<std::shared_ptr<QMap<int, PLSNoticeUpdateItem>>> items;
		std::shared_ptr<QMap<int, PLSNoticeUpdateItem>> inflightItems;
		QString lastError;
		int loadedCount{0};
		int totalCount{0};
		int nextPage{0};
		bool hasMoreRemote{false};
		bool isFetching{false};
		bool firstLoadCompleted{false};
		int requestId{0};
		pls::QObjectPtr<QObject> activeRefreshReceiver;
		RefreshCallback activeRefreshCallback;
		std::vector<PendingCenterCacheIdleWaiter> pendingIdleAfterFetch;
	};

	explicit PLSNoticeUpdateRepository(QObject *parent = nullptr);

	static bool lessByPublishDesc(const PLSNoticeUpdateItem &lhs, const PLSNoticeUpdateItem &rhs);
	static bool parseNoticeV2Response(const QByteArray &data, PLSNoticeProvider provider, FetchPageResult *outResult);
	void fetchNoticePageFromApiAsync(const QObject *receiver, int page, int limit, PLSNoticeProvider provider, const std::function<void(const FetchPageResult &)> &callback);
	void fetchCurrentNoticeDetailFromApiAsync(const QObject *receiver, const QString &appVersion, const std::function<void(const FetchPageResult &)> &callback);
	void refreshSingleCenterCacheAsync(const QObject *receiver, bool isB2B, const RefreshCallback &callback);
	void completePendingIdleWaiters(bool isB2B, int finishedRequestId, const QString &error);
	void notifyActiveCenterRefreshSuperseded(CenterCacheState &state);
	void resetCenterCacheState(bool isB2B);
	void applyCenterCacheFetchResult(bool isB2B, const FetchPageResult &result);
	void applyCenterCachePage(bool isB2B, const FetchPageResult &result);


	CenterCacheState &centerCacheState(bool isB2B);
	const CenterCacheState &centerCacheState(bool isB2B) const;

private:
	CenterCacheState m_centerCacheState;
	CenterCacheState m_centerCacheStateB2B;
};

#endif // PLS_NOTICE_UPDATE_REPOSITORY_HPP
