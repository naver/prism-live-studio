#ifndef PLSNAVERSHOPPINGLIVESEARCHKEYWORDITEMVIEW_H
#define PLSNAVERSHOPPINGLIVESEARCHKEYWORDITEMVIEW_H

#include <QPushButton>
#include <QVBoxLayout>

#include "PLSNaverShoppingLIVEAPI.h"
#include "PLSNaverShoppingLIVEItemViewCache.h"

namespace Ui {
class PLSNaverShoppingLIVESearchKeywordItemView;
}

template<typename ItemView> class PLSNaverShoppingLIVEItemViewCache;

class PLSNaverShoppingLIVESearchKeywordItemView : public QPushButton, public PLSNaverShoppingLIVEItemViewBase {
	Q_OBJECT

public:
	explicit PLSNaverShoppingLIVESearchKeywordItemView(QWidget *parent = nullptr);
	~PLSNaverShoppingLIVESearchKeywordItemView() override;

	QString getSearchKeyword() const;
	void setInfo(const QString &searchKeyword);

signals:
	void removeButtonClicked(PLSNaverShoppingLIVESearchKeywordItemView *itemView);
	void searchButtonClicked(PLSNaverShoppingLIVESearchKeywordItemView *itemView);

public:
	static void addBatchCache(int batchCount, bool check = false);
	static void cleaupCache();

	static PLSNaverShoppingLIVESearchKeywordItemView *alloc(QWidget *parent, const QString &searchKeyword);
	template<typename ClickedReceiver, typename RemoveClickedSlot, typename SearchClickedSlot>
	static PLSNaverShoppingLIVESearchKeywordItemView *alloc(QWidget *parent, ClickedReceiver *receiver, RemoveClickedSlot removeClickedSlot, SearchClickedSlot searchClickedSlot,
								const QString &searchKeyword)
	{
		PLSNaverShoppingLIVESearchKeywordItemView *itemView = alloc(parent, searchKeyword);
		itemView->removeButtonClickedConnection = QObject::connect(itemView, &PLSNaverShoppingLIVESearchKeywordItemView::removeButtonClicked, receiver, removeClickedSlot);
		itemView->searchButtonClickedConnection = QObject::connect(itemView, &PLSNaverShoppingLIVESearchKeywordItemView::searchButtonClicked, receiver, searchClickedSlot);
		return itemView;
	}
	static void dealloc(PLSNaverShoppingLIVESearchKeywordItemView *view);
	static void dealloc(QList<PLSNaverShoppingLIVESearchKeywordItemView *> &views, QVBoxLayout *layout);
	static void updateSearchKeyword(const QList<PLSNaverShoppingLIVESearchKeywordItemView *> &views);

protected:
	bool event(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	Ui::PLSNaverShoppingLIVESearchKeywordItemView *ui = nullptr;
	QMetaObject::Connection removeButtonClickedConnection;
	QMetaObject::Connection searchButtonClickedConnection;
	QString searchKeyword;

	friend class PLSNaverShoppingLIVEItemViewCache<PLSNaverShoppingLIVESearchKeywordItemView>;
};

#endif // PLSNAVERSHOPPINGLIVESEARCHKEYWORDITEMVIEW_H
