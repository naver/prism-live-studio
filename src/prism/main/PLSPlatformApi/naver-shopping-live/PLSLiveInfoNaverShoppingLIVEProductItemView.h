#ifndef PLSLIVEINFONAVERSHOPPINGLIVEPRODUCTITEMVIEW_H
#define PLSLIVEINFONAVERSHOPPINGLIVEPRODUCTITEMVIEW_H

#include <QWidget>
#include <QVBoxLayout>

#include "PLSNaverShoppingLIVEAPI.h"
#include "PLSNaverShoppingLIVEItemViewCache.h"

namespace Ui {
class PLSLiveInfoNaverShoppingLIVEProductItemView;
}

class PLSPlatformNaverShoppingLIVE;
template<typename ItemView> class PLSNaverShoppingLIVEItemViewCache;
class PLSLoadingView;

class PLSLiveInfoNaverShoppingLIVEProductItemView : public QWidget, public PLSNaverShoppingLIVEItemViewBase {
	Q_OBJECT

private:
	explicit PLSLiveInfoNaverShoppingLIVEProductItemView(QWidget *parent = nullptr);
	~PLSLiveInfoNaverShoppingLIVEProductItemView();

public:
	using Product = PLSNaverShoppingLIVEAPI::ProductInfo;

public:
	void setInfo(PLSPlatformNaverShoppingLIVE *platform, const Product &product, bool fixed);
	void setProductReadonly(bool readonly);

	bool isFixed() const;
	void setFixed(bool fixed);

	qint64 getProductNo() const;
	const Product &getProduct() const;

signals:
	void fixButtonClicked(PLSLiveInfoNaverShoppingLIVEProductItemView *itemView);
	void removeButtonClicked(PLSLiveInfoNaverShoppingLIVEProductItemView *itemView);

public:
	static void addBatchCache(int batchCount, bool check = false);
	static void cleaupCache();

	static PLSLiveInfoNaverShoppingLIVEProductItemView *alloc(QWidget *parent, PLSPlatformNaverShoppingLIVE *platform, const Product &product, bool fixed);
	template<typename ClickedReceiver, typename FixClickedSlot, typename RemoveClickedSlot>
	static PLSLiveInfoNaverShoppingLIVEProductItemView *alloc(QWidget *parent, ClickedReceiver *receiver, FixClickedSlot fixClickedSlot, RemoveClickedSlot removeClickedSlot,
								  PLSPlatformNaverShoppingLIVE *platform, const Product &product, bool fixed)
	{
		PLSLiveInfoNaverShoppingLIVEProductItemView *itemView = alloc(parent, platform, product, fixed);
		itemView->fixButtonClickedConnection = QObject::connect(itemView, &PLSLiveInfoNaverShoppingLIVEProductItemView::fixButtonClicked, receiver, fixClickedSlot);
		itemView->removeButtonClickedConnection = QObject::connect(itemView, &PLSLiveInfoNaverShoppingLIVEProductItemView::removeButtonClicked, receiver, removeClickedSlot);
		return itemView;
	}
	static void dealloc(PLSLiveInfoNaverShoppingLIVEProductItemView *view);
	static void dealloc(QList<PLSLiveInfoNaverShoppingLIVEProductItemView *> &views, QVBoxLayout *layout);
	static void resetIconSize(QList<PLSLiveInfoNaverShoppingLIVEProductItemView *> &views);
	static void updateWhenDpiChanged(QList<PLSLiveInfoNaverShoppingLIVEProductItemView *> &views);

protected:
	virtual bool event(QEvent *event) override;
	virtual bool eventFilter(QObject *watched, QEvent *event) override;

private:
	Ui::PLSLiveInfoNaverShoppingLIVEProductItemView *ui;
	PLSLoadingView *imageLoadingView = nullptr;
	bool fixed = false;
	Product product;
	QMetaObject::Connection fixButtonClickedConnection;
	QMetaObject::Connection removeButtonClickedConnection;

	friend class PLSNaverShoppingLIVEItemViewCache<PLSLiveInfoNaverShoppingLIVEProductItemView>;
};

#endif // PLSLIVEINFONAVERSHOPPINGLIVEPRODUCTITEMVIEW_H
