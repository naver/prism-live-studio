#ifndef PLSNAVERSHOPPINGLIVEPRODUCTITEMVIEW_H
#define PLSNAVERSHOPPINGLIVEPRODUCTITEMVIEW_H

#include <QFrame>
#include <QVBoxLayout>

#include "PLSNaverShoppingLIVEAPI.h"
#include "PLSNaverShoppingLIVEItemViewCache.h"

namespace Ui {
class PLSNaverShoppingLIVEProductItemView;
}

template<typename ItemView> class PLSNaverShoppingLIVEItemViewCache;
class PLSLoadingView;

class PLSNaverShoppingLIVEProductItemView : public QFrame, public PLSNaverShoppingLIVEItemViewBase {
	Q_OBJECT

private:
	explicit PLSNaverShoppingLIVEProductItemView(QWidget *parent = nullptr);
	~PLSNaverShoppingLIVEProductItemView();

public:
	using Details = PLSNaverShoppingLIVEAPI::ProductInfo;

public:
	qint64 getProductNo() const;
	QString getUrl() const;
	QString getImageUrl() const;
	QString getName() const;
	double getPrice() const;
	QString getStoreName() const;
	Details getDetails() const;

	void setInfo(PLSPlatformNaverShoppingLIVE *platform, const Details &details);
	void setInfo(PLSPlatformNaverShoppingLIVE *platform, PLSNaverShoppingLIVEProductItemView *srcItemView);
	void setSelected(bool selected);

	void setStoreNameVisible(bool visible);

signals:
	void addRemoveButtonClicked(PLSNaverShoppingLIVEProductItemView *view, qint64 productNo);

public:
	static void addBatchCache(int batchCount, bool check = false);
	static void cleaupCache();

	static PLSNaverShoppingLIVEProductItemView *alloc(QWidget *parent, PLSPlatformNaverShoppingLIVE *platform, const Details &details);
	template<typename ClickedReceiver, typename ClickedSlot>
	static PLSNaverShoppingLIVEProductItemView *alloc(QWidget *parent, ClickedReceiver *receiver, ClickedSlot clickedSlot, PLSPlatformNaverShoppingLIVE *platform, const Details &details)
	{
		PLSNaverShoppingLIVEProductItemView *itemView = alloc(parent, platform, details);
		itemView->addRemoveButtonClickedConnection = QObject::connect(itemView, &PLSNaverShoppingLIVEProductItemView::addRemoveButtonClicked, receiver, clickedSlot);
		return itemView;
	}
	static void dealloc(PLSNaverShoppingLIVEProductItemView *view);
	static void dealloc(QList<PLSNaverShoppingLIVEProductItemView *> &views, QVBoxLayout *layout);
	static void resetIconSize(QList<PLSNaverShoppingLIVEProductItemView *> &views);
	static void updateWhenDpiChanged(QList<PLSNaverShoppingLIVEProductItemView *> &views);

protected:
	virtual bool event(QEvent *event) override;
	virtual bool eventFilter(QObject *watched, QEvent *event) override;

private:
	Ui::PLSNaverShoppingLIVEProductItemView *ui;
	PLSLoadingView *imageLoadingView = nullptr;
	QMetaObject::Connection addRemoveButtonClickedConnection;
	Details details;

	friend class PLSNaverShoppingLIVEItemViewCache<PLSNaverShoppingLIVEProductItemView>;
};

#endif // PLSNAVERSHOPPINGLIVEPRODUCTITEMVIEW_H
