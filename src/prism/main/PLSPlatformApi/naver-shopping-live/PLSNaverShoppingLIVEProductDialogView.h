#ifndef PLSNAVERSHOPPINGLIVEPRODUCTDIALOGVIEW_H
#define PLSNAVERSHOPPINGLIVEPRODUCTDIALOGVIEW_H

#include "dialog-view.hpp"
#include "PLSNaverShoppingLIVEAPI.h"

namespace Ui {
class PLSNaverShoppingLIVEProductDialogView;
}

class QScrollBar;
class PLSPlatformNaverShoppingLIVE;
class PLSNaverShoppingLIVEProductItemView;
class PLSNaverShoppingLIVESearchKeywordItemView;
class PLSLoadingView;
class PLSLoadNextPage;
class PLSNaverShoppingLIVEStoreNameList;

#define MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER "PLSNaverShoppingLIVE/ProductManager"

class PLSNaverShoppingLIVEProductDialogView : public PLSDialogView {
	Q_OBJECT
	Q_PROPERTY(QString lang READ getLang)

public:
	using Product = PLSNaverShoppingLIVEAPI::ProductInfo;

	struct SmartStoreInfo {
		QString storeId;
		QString storeName;
	};

public:
	explicit PLSNaverShoppingLIVEProductDialogView(PLSPlatformNaverShoppingLIVE *platform, const QList<Product> &selectedProducts, bool isLiving, QWidget *parent = nullptr,
						       PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSNaverShoppingLIVEProductDialogView();

public:
	QString getLang() const;

public:
	QList<qint64> getSelectedProductNos() const;
	QList<Product> getSelectedProducts() const;
	QList<qint64> getNewRecentProductNos() const;

	QString getCurrentSmartStoreName() const;

private:
	void initRecentProducts(int currentPage = 0);
	void initSmartStores();
	void searchSmartStores();
	void storeSearch(const QString &text = QString(), int currentPage = 0);
	void searchSearch(const QString &text = QString(), int currentPage = 0);
	void updateSearchKeywords(const QString &searchKeyword = QString(), bool showSearchKeywords = true);

	bool isSelectedProductNo(qint64 productNo) const;
	bool findItemView(PLSNaverShoppingLIVEProductItemView *&itemView, const QList<PLSNaverShoppingLIVEProductItemView *> &itemViews, qint64 productNo);
	void syncItemViewSelectedState(QList<PLSNaverShoppingLIVEProductItemView *> *current, const QList<PLSNaverShoppingLIVEProductItemView *> &itemViews, qint64 productNo, bool selected);
	void syncItemViewSelectedState(QList<PLSNaverShoppingLIVEProductItemView *> *current, qint64 productNo, bool selected);
	void updateRecentItems();
	void updateCountTip();
	void updateSmartStores(bool apiFailed = false);
	qint64 getSearchIndex(qint64 &searchIndex) const;
	bool asyncProcessAndCheckDone();
	QLayout *vscrollBarToLayout(QObject *vscrollBar) const;

	void onProductItemAddRemoveButtonClicked(QList<PLSNaverShoppingLIVEProductItemView *> *itemViews, PLSNaverShoppingLIVEProductItemView *itemView, qint64 productNo);
	void onRemoveButtonClicked(PLSNaverShoppingLIVESearchKeywordItemView *itemView);
	void onSearchButtonClicked(PLSNaverShoppingLIVESearchKeywordItemView *itemView);
	void onAddRecentProduct(PLSNaverShoppingLIVEProductItemView *srcItemView, qint64 productNo);
	void onLineEditSetFocus();
	void onFlushLineEditStyle();
	void clearSearchTabResult();
	void doSearchTabSearch();

signals:
	void smartStoreChanged();
	void lineEditSetFocus();
	void flushLineEditStyle();

private slots:
	void on_storeSearchBarClearButton_clicked();
	void on_searchSearchBarClearButton_clicked();
	void on_storeSearchBarSearchButton_clicked();
	void on_storeSearchBarLineEdit_textChanged(const QString &text);
	void on_searchSearchBarSearchButton_clicked();
	void on_searchSearchBarLineEdit_textChanged(const QString &text);
	void on_searchSearchBarLineEdit_returnPressed();
	void on_okButton_clicked();
	void on_cancelButton_clicked();
	void on_recentNoNetRetryButton_clicked();
	void on_storeChangeStoreButton_clicked();
	void on_noSmartStoreButton_clicked();
	void on_storeNoNetButton_clicked();
	void on_storeSearchNoNetButton_clicked();
	void on_searchNoNetButton_clicked();
	void on_storeApiFailedRetryButton_clicked();

protected:
	virtual bool event(QEvent *event) override;
	virtual bool eventFilter(QObject *watched, QEvent *event) override;
	virtual void done(int result);

private:
	Ui::PLSNaverShoppingLIVEProductDialogView *ui;
	QScrollBar *recentProductVScrollBar = nullptr;
	QScrollBar *storeProductVScrollBar = nullptr;
	QScrollBar *searchProductVScrollBar = nullptr;
	QScrollBar *searchKeywordVScrollBar = nullptr;
	PLSLoadingView *recentProductsLoadingView = nullptr;
	PLSLoadingView *storeProductsLoadingView = nullptr;
	PLSLoadingView *searchProductsLoadingView = nullptr;
	PLSLoadNextPage *recentProductNextPage = nullptr;
	PLSLoadNextPage *storeProductNextPage = nullptr;
	PLSLoadNextPage *searchProductNextPage = nullptr;
	PLSPlatformNaverShoppingLIVE *platform;
	QList<SmartStoreInfo> smartStores;
	SmartStoreInfo currentSmartStore;
	QList<Product> recentProducts;
	QList<PLSNaverShoppingLIVEProductItemView *> recentProductItemViews;
	QList<PLSNaverShoppingLIVEProductItemView *> storeProductItemViews;
	QList<PLSNaverShoppingLIVEProductItemView *> searchProductItemViews;
	QList<PLSNaverShoppingLIVESearchKeywordItemView *> searchKeywordItemViews;
	QList<Product> selectedProducts;
	QList<qint64> newRecentProductNos;
	qint64 storeSearchIndex = 0;
	qint64 searchSearchIndex = 0;
	bool recentNeedUpdateItems = false;
	bool isLiving = false;
	bool doneFlag = false;

	friend class PLSNaverShoppingLIVEStoreNameList;
};

#endif // PLSNAVERSHOPPINGLIVEPRODUCTDIALOGVIEW_H
