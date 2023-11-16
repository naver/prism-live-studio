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

public:
	explicit PLSNaverShoppingLIVEProductItemView(QWidget *parent = nullptr);
	~PLSNaverShoppingLIVEProductItemView() override;

	using Details = PLSNaverShoppingLIVEAPI::ProductInfo;

	qint64 getProductNo() const;
	QString getUrl() const;
	QString getImageUrl() const;
	QString getName() const;
	double getPrice() const;
	QString getStoreName() const;
	Details getDetails() const;

	void setInfo(const PLSPlatformNaverShoppingLIVE *platform, const Details &details);
	void setInfo(const PLSPlatformNaverShoppingLIVE *platform, const PLSNaverShoppingLIVEProductItemView *srcItemView);
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
	static void resetIconSize(const QList<PLSNaverShoppingLIVEProductItemView *> &views);
	static void updateWhenDpiChanged(const QList<PLSNaverShoppingLIVEProductItemView *> &views);

protected:
	bool event(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	Ui::PLSNaverShoppingLIVEProductItemView *ui = nullptr;
	PLSLoadingView *imageLoadingView = nullptr;
	QMetaObject::Connection addRemoveButtonClickedConnection;
	Details details;

	friend class PLSNaverShoppingLIVEItemViewCache<PLSNaverShoppingLIVEProductItemView>;
};

#endif // PLSNAVERSHOPPINGLIVEPRODUCTITEMVIEW_H
