#ifndef PLSNAVERSHOPPINGLIVEITEMVIEWCACHE_H
#define PLSNAVERSHOPPINGLIVEITEMVIEWCACHE_H

#include <QApplication>
#include <QVBoxLayout>
#include <QList>
#include <QThread>

#include "PLSNaverShoppingLIVEDataManager.h"

class PLSNaverShoppingLIVEItemViewBase {
protected:
	PLSNaverShoppingLIVEItemViewBase() {}
	virtual ~PLSNaverShoppingLIVEItemViewBase() {}

public:
	qint64 getItemId() const { return itemId; }
	void setItemId(qint64 itemId) { this->itemId = itemId; }

protected:
	qint64 itemId = -1;
};

template<typename ItemView> class PLSNaverShoppingLIVEItemViewCache {
public:
	PLSNaverShoppingLIVEItemViewCache() {}
	~PLSNaverShoppingLIVEItemViewCache() {}

public:
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

		ItemView *views = new ItemView[batchCount];
		itemViewBatchs.append(QPair<ItemView *, ItemView *>(views, views + batchCount));
		for (int i = 0; i < batchCount; ++i) {
			itemViewCache.append(&views[i]);
		}
	}
	void cleaupCache()
	{
		QObject::connect(qApp, &QObject::destroyed, [this]() {
			while (!itemViewCache.isEmpty()) {
				ItemView *itemView = itemViewCache.takeFirst();
				if (!isOfBatch(itemView)) {
					delete itemView;
				}
			}

			while (!itemViewBatchs.isEmpty()) {
				QPair<ItemView *, ItemView *> batch = itemViewBatchs.takeFirst();
				delete[] batch.first;
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
			itemView = new ItemView(parent);
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
		for (const auto &batch : itemViewBatchs) {
			if (itemView >= batch.first && itemView < batch.second) {
				return true;
			}
		}
		return false;
	}

private:
	QList<ItemView *> itemViewCache;
	QList<QPair<ItemView *, ItemView *>> itemViewBatchs;
};

#endif // PLSNAVERSHOPPINGLIVEITEMVIEWCACHE_H
