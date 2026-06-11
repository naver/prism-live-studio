#ifndef PLSLIVEINFONAVERSHOPPINGLIVEPRODUCTLIST_H
#define PLSLIVEINFONAVERSHOPPINGLIVEPRODUCTLIST_H

#include <QList>
#include <QPair>
#include <QWidget>
#include <QButtonGroup>
#include <QPushButton>

#include "PLSNaverShoppingLIVEAPI.h"
#include "utils-api.h"
#include "libhttp-client.h"

namespace Ui {
class PLSLiveInfoNaverShoppingLIVEProductList;
}

class QLabel;
class QScrollArea;
class PLSPlatformNaverShoppingLIVE;
class PLSLiveInfoNaverShoppingLIVEProductItemView;
class PLSLoadingView;
class PLSNaverShoppingLIVEProductDialogView;
class PLSLoadNextPage;

class PLSLiveInfoNaverShoppingLIVEProductCountBadge : public QWidget {
	Q_OBJECT

public:
	explicit PLSLiveInfoNaverShoppingLIVEProductCountBadge(QWidget *parent = nullptr);
	~PLSLiveInfoNaverShoppingLIVEProductCountBadge() override = default;

	void setProductCount(int productCount, bool ok = true);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	QString productCount;
};

class PLSProductBtn : public QPushButton {
	Q_OBJECT
public:
	explicit PLSProductBtn(QWidget *parent = nullptr);
	~PLSProductBtn() override = default;

	void setTitle(const QString &title);
	void setSelectedNumber(int count);

private:
	QLabel *titleLabel = nullptr;
	QLabel *countLabel = nullptr;
};

class PLSLiveInfoNaverShoppingLIVEProductList : public QWidget {
	Q_OBJECT
	Q_PROPERTY(int titleLabelY READ titleLabelY WRITE setTitleLabelY)

public:
	using Product = PLSNaverShoppingLIVEAPI::ProductInfo;

	explicit PLSLiveInfoNaverShoppingLIVEProductList(QWidget *parent = nullptr);
	~PLSLiveInfoNaverShoppingLIVEProductList() override;

	PLSPlatformNaverShoppingLIVE *getPlatform();

	int titleLabelY() const;
	void setTitleLabelY(int titleLabelY);

	QList<Product> getFixedOrUnfixedProducts(PLSProductType productType, bool fixed) const;
	QList<Product> getAllProducts() const;
	bool hasUnattachableProduct();
	int getProductCount() const;

	QList<qint64> getFixedOrUnfixedProductNos(PLSProductType productType, bool fixed) const;
	int getProductCount(PLSProductType productType) const;
	PLSProductType getProductType() const;
	QList<Product> getProduct(PLSProductType productType) const;
	QList<qint64> getProductNos(PLSProductType productType) const;
	QList<qint64> getProductNos(const QList<Product> &products, bool introducing) const;
	QMap<PLSProductType, QList<Product>> getProducts() const;
	void setProductRepresent(PLSProductType productType, qint64 productNo, bool represent);
	void setProductType(PLSProductType productType);
	void setProduct(PLSProductType productType, QList<Product> products);
	void removeProduct(PLSProductType productType, qint64 productNo);

	void setOwnerScrollArea(QScrollArea *ownerScrollArea);
	void setProductNos(PLSProductType productType, const QList<qint64> &fixedProductNos, const QList<qint64> &unfixedProductNos, const QList<qint64> &introducingProductNos,
			   std::function<void()> &&updateFinished = nullptr);
	void setAllProducts(const QList<Product> &products, bool update = false);
	void setProducts(PLSProductType productType, QList<Product> fixedProducts, QList<Product> unfixedProducts, bool update = false);
	void setProductReadonly(bool readonly);
	void setIsLiving(bool isLiving);
	void setIsScheduleLive(bool isScheduleLive);
	void setIsScheduleLiveLoading(bool isScheduleLiveLoading);
	void setIsPlanningLive(bool isPlanningLive);
	void expiredClose();
	void resetIconSize() const;
	void updateWhenDpiChanged() const;
	void cancelSearchRequest();

private:
	void searchProduct(int currentPage, PLSProductType productType, const QList<qint64> &fixedProductNos, const QList<qint64> &unfixedProductNos, const QList<qint64> &introducingProductNos);
	void addProduct(const QList<Product> &fixedProducts, const QList<Product> &unfixedProducts);
	QString createSearchProductKey(const QList<qint64> &fixedProducts, const QList<qint64> &unfixedProducts);

	void updateAllProductsInfo(PLSProductType productType, const QList<qint64> &fixedProducts, const QList<qint64> &unfixedProducts, const QList<qint64> &introducingProducts);
	void updateAllProductsInfo_lambda(int currentPage, PLSProductType productType, const QList<qint64> &fixedProductNos, const QList<qint64> &unfixedProductNos, bool ok, bool hasNext,
					  QList<PLSNaverShoppingLIVEAPI::ProductInfo> &fixedProducts, QList<PLSNaverShoppingLIVEAPI::ProductInfo> &unfixedProducts,
					  const std::chrono::steady_clock::time_point &start);
	void updateAllProductsInfo_lambda_ok(int currentPage, QList<PLSNaverShoppingLIVEAPI::ProductInfo> &fixedProducts, QList<PLSNaverShoppingLIVEAPI::ProductInfo> &unfixedProducts);
	void updateProductsInfo(const QList<Product> &fixedProducts, const QList<Product> &unfixedProducts);
	void updateVisibleProductsInfo(PLSProductType productType, QList<Product> &fixedProducts, QList<Product> &unfixedProducts);
	void updateFixProductTip(bool ok = true);
	void updateUIWhenSwitchScheduleList();
	void updateProductCountBadge(bool ok = true);
	void updateProductBtnNumber();
	void setAddButtonEnabled(bool enabled);
	void checkProductEmpty();
	bool checkAgeLimitProducts(QList<Product> &fixedProducts, QList<Product> &unfixedProducts);
	void clearAll();

	void onFixButtonClicked(PLSLiveInfoNaverShoppingLIVEProductItemView *itemView);
	void onRemoveButtonClicked(const PLSLiveInfoNaverShoppingLIVEProductItemView *itemView);
	void onSmartStoreChanged();

	void doProductFixed(PLSLiveInfoNaverShoppingLIVEProductItemView *itemView, bool isFixed);
	void doProductRemoved(const PLSLiveInfoNaverShoppingLIVEProductItemView *itemView);

	bool getProductLoadingViewport(QRect &geometry, const PLSLoadingView *loadingView) const;

	void showPage(bool hasProducts, bool showNoNet = false, bool apiFailed = false, bool showProductList = true);

	void productUpdateFinished();
	void allowScrollAreaShowVerticalScrollBar();
	void OnBtnGroupClicked(PLSProductType productType);
	bool isProductItemViewExisted(qint64 productNo);

	void setNoProductPageShowOrHide(bool show);
signals:
	void productChangedOrUpdated(bool changed);

private slots:
	void on_nppAddButton_clicked();
	void on_ppAddButton_clicked();
	void on_noNetRetryButton_clicked();
	void on_gotoNaverShoppingToolButton_clicked();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	Ui::PLSLiveInfoNaverShoppingLIVEProductList *ui = nullptr;
	QWidget *titleWidget = nullptr;
	PLSLiveInfoNaverShoppingLIVEProductCountBadge *productCountBadge = nullptr;
	QScrollArea *ownerScrollArea = nullptr;
	bool productReadonly = false;
	bool isLiving = false;
	bool isScheduleLive = false;
	bool isScheduleLiveLoading = false;
	bool isPlanningLive = false;
	QList<Product> fixedProducts;
	QList<Product> unfixedProducts;
	QMap<PLSProductType, QList<Product>> allProducts;
	QList<PLSLiveInfoNaverShoppingLIVEProductItemView *> itemViews;
	PLSLoadingView *productLoadingView = nullptr;
	PLSNaverShoppingLIVEProductDialogView *productDialogView = nullptr;
	PLSLoadNextPage *productNextPage = nullptr;
	std::function<void()> setProductNosUpdateFinished = nullptr;
	QButtonGroup *productBtnGroup = nullptr;
	QMap<QString, QPair<pls::http::Request, bool>> cancelRequestMap;
};

#endif // PLSLIVEINFONAVERSHOPPINGLIVEPRODUCTLIST_H
