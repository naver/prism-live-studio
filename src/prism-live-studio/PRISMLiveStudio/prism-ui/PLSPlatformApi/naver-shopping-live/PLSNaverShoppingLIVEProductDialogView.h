#ifndef PLSNAVERSHOPPINGLIVEPRODUCTDIALOGVIEW_H
#define PLSNAVERSHOPPINGLIVEPRODUCTDIALOGVIEW_H

#include "PLSDialogView.h"
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

constexpr const char *MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER = "PLSNaverShoppingLIVE/ProductManager";

class PLSNaverShoppingLIVEProductDialogView : public PLSDialogView {
	Q_OBJECT
	Q_PROPERTY(QString lang READ getLang)

public:
	using Product = PLSNaverShoppingLIVEAPI::ProductInfo;

	struct SmartStoreInfo {
		QString storeId;
		QString storeName;
	};

	explicit PLSNaverShoppingLIVEProductDialogView(PLSPlatformNaverShoppingLIVE *platform, PLSProductType productType, const QList<Product> &selectedProducts, const QList<Product> &otherProducts,
						       bool isLiving, bool isPlanningLive, QWidget *parent = nullptr);
	~PLSNaverShoppingLIVEProductDialogView() override;

	QString getLang() const;

	QList<qint64> getSelectedProductNos() const;
	QList<Product> getSelectedProducts() const;
	QList<qint64> getNewRecentProductNos() const;

	QString getCurrentSmartStoreName() const;
	bool isMyStoreProduct(const Product &product) const;

private:
	void initRecentProducts(int currentPage = 0);
	void initSmartStores();
	void searchSmartStores();
	void storeSearch(const QString &text = QString(), int currentPage = 0);
	void searchSearch(const QString &text = QString(), int currentPage = 0);
	void updateSearchKeywords(const QString &searchKeyword = QString(), bool showSearchKeywords = true);

	bool isSelectedProductNo(qint64 productNo) const;
	bool isOtherProductNo(qint64 productNo);
	bool findItemView(PLSNaverShoppingLIVEProductItemView *&itemView, const QList<PLSNaverShoppingLIVEProductItemView *> &itemViews, qint64 productNo) const;
	void syncItemViewSelectedState(const QList<PLSNaverShoppingLIVEProductItemView *> *current, const QList<PLSNaverShoppingLIVEProductItemView *> &itemViews, qint64 productNo,
				       bool selected) const;
	void syncItemViewSelectedState(const QList<PLSNaverShoppingLIVEProductItemView *> *current, qint64 productNo, bool selected) const;
	void updateRecentItems();
	void updateCountTip();
	void updateSmartStoreSearch();
	void updateSmartStores(bool apiFailed = false);
	qint64 getSearchIndex(qint64 &searchIndex) const;
	bool asyncProcessAndCheckDone() const;
	QLayout *vscrollBarToLayout(const QObject *vscrollBar) const;

	void processProductSearchByProductNos(bool ok, const QList<PLSNaverShoppingLIVEAPI::ProductInfo> &products, bool isFirstPage, int currentPage, bool next);
	void processStoreChannelProductSearch(bool ok, const QList<Product> &products, bool next, int page, bool isFirstPage, const QString &text, int currentPage, qint64 searchIndex);
	void processSearchSearch(bool ok, bool hasProduct, const Product &product, qint64 searchIndex);
	void processSearchSearch(bool ok, const QList<Product> &products, bool next, int page, bool isFirstPage, const QString &text, int currentPage, qint64 searchIndex);

	void onProductItemAddRemoveButtonClicked(const QList<PLSNaverShoppingLIVEProductItemView *> *itemViews, PLSNaverShoppingLIVEProductItemView *itemView, qint64 productNo,
						 bool forceRemove = false);
	void onRemoveButtonClicked(const PLSNaverShoppingLIVESearchKeywordItemView *itemView);
	void onSearchButtonClicked(const PLSNaverShoppingLIVESearchKeywordItemView *itemView);
	void onAddRecentProduct(const PLSNaverShoppingLIVEProductItemView *srcItemView, qint64 productNo);
	void onLineEditSetFocus();
	void onFlushLineEditStyle();
	void clearSearchTabResult();
	void doSearchTabSearch();

	void eventFilter_storeNameWidget(const QEvent *event);

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
	bool event(QEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;
	void done(int result) override;

private:
	Ui::PLSNaverShoppingLIVEProductDialogView *ui = nullptr;
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
	PLSPlatformNaverShoppingLIVE *platform = nullptr;
	QList<SmartStoreInfo> smartStores;
	SmartStoreInfo currentSmartStore;
	QList<Product> recentProducts;
	QList<PLSNaverShoppingLIVEProductItemView *> recentProductItemViews;
	QList<PLSNaverShoppingLIVEProductItemView *> storeProductItemViews;
	QList<PLSNaverShoppingLIVEProductItemView *> searchProductItemViews;
	QList<PLSNaverShoppingLIVESearchKeywordItemView *> searchKeywordItemViews;
	QList<Product> selectedProducts;
	QList<Product> otherProducts;
	QList<Product> originProducts;
	QList<qint64> newRecentProductNos;
	qint64 storeSearchIndex = 0;
	qint64 searchSearchIndex = 0;
	bool recentNeedUpdateItems = false;
	bool isLiving = false;
	bool isPlanningLive = false;
	bool doneFlag = false;
	PLSProductType productType;
	QMap<QString, pls::http::Request> cancelSearchRequestMap;

	friend class PLSNaverShoppingLIVEStoreNameList;
};

#endif // PLSNAVERSHOPPINGLIVEPRODUCTDIALOGVIEW_H
