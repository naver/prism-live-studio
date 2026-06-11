#ifndef PLSNAVERSHOPPINGLIVEITEMVIEWCACHE_H
#define PLSNAVERSHOPPINGLIVEITEMVIEWCACHE_H

#include <QApplication>
#include <QVBoxLayout>
#include <QList>
#include <QThread>

#include "PLSNaverShoppingLIVEDataManager.h"
#include "utils-api.h"

class PLSNaverShoppingLIVEItemViewBase {
protected:
	PLSNaverShoppingLIVEItemViewBase() = default;
	virtual ~PLSNaverShoppingLIVEItemViewBase() = default;

public:
	qint64 getItemId() const { return itemId; }
	void setItemId(qint64 itemId_) { this->itemId = itemId_; }

private:
	qint64 itemId = -1;
};

template<typename ItemView> class PLSNaverShoppingLIVEItemViewCache {
	using My = PLSNaverShoppingLIVEItemViewCache<ItemView>;

	PLSNaverShoppingLIVEItemViewCache() = default;
	~PLSNaverShoppingLIVEItemViewCache() = default;

public:
	static My *instance()
	{
		static My s_instance;
		return &s_instance;
	}

	void addBatchCache(int batchCount, bool check = false)
	{
		if (batchCount <= 0 || QThread::currentThread() != qApp->thread()) {
			return;
		}

		if (check) {
			batchCount -= itemViewCache.count();
		}

		if (batchCount <= 0) {
			return;
		}

		ItemView *views = pls_new_array<ItemView>(batchCount);
		itemViewBatchs.append(QPair<ItemView *, ItemView *>(views, views + batchCount));
		for (int i = 0; i < batchCount; ++i) {
			itemViewCache.append(&views[i]);
		}
	}
	void cleaupCache()
	{
		pls_qapp_deconstruct_add_cb([this]() {
			while (!itemViewCache.isEmpty()) {
				ItemView *itemView = itemViewCache.takeFirst();
				if (!isOfBatch(itemView)) {
					pls_delete(itemView);
				}
			}

			while (!itemViewBatchs.isEmpty()) {
				QPair<ItemView *, ItemView *> batch = itemViewBatchs.takeFirst();
				pls_delete_array(batch.first);
			}
		});
	}

	template<typename... Args> ItemView *alloc(QWidget *parent, Args &&...args)
	{
		ItemView *itemView = nullptr;
		if (!itemViewCache.isEmpty()) {
			itemView = itemViewCache.takeFirst();
			itemView->setParent(parent);
		} else {
			itemView = pls_new<ItemView>(parent);
		}

		itemView->setItemId(PLSNaverShoppingLIVEDataManager::getItemId());
		itemView->setInfo(std::forward<Args>(args)...);
		return itemView;
	}
	void dealloc(ItemView *itemView)
	{
		itemView->hide();
		itemView->setParent(nullptr);
		itemView->setItemId(-1);
		itemViewCache.append(itemView);
	}
	void dealloc(QList<ItemView *> &views, QVBoxLayout *layout)
	{
		while (!views.isEmpty()) {
			ItemView *view = views.takeFirst();
			layout->removeWidget(view);
			dealloc(view);
		}
	}
	template<typename BeforeDealloc> void dealloc(QList<ItemView *> &views, QVBoxLayout *layout, BeforeDealloc beforeDealloc)
	{
		while (!views.isEmpty()) {
			ItemView *view = views.takeFirst();
			layout->removeWidget(view);
			beforeDealloc(view);
			dealloc(view);
		}
	}

private:
	bool isOfBatch(ItemView *itemView) const
	{
		return std::any_of(itemViewBatchs.begin(), itemViewBatchs.end(), [itemView](const auto &batch) { return itemView >= batch.first && itemView < batch.second; });
	}

	QList<ItemView *> itemViewCache;
	QList<QPair<ItemView *, ItemView *>> itemViewBatchs;
};

#endif // PLSNAVERSHOPPINGLIVEITEMVIEWCACHE_H
