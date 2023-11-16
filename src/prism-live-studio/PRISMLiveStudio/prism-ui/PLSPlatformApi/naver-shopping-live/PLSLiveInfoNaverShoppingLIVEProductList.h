#ifndef PLSLIVEINFONAVERSHOPPINGLIVEPRODUCTLIST_H
#define PLSLIVEINFONAVERSHOPPINGLIVEPRODUCTLIST_H

#include <QList>
#include <QPair>
#include <QWidget>

#include "PLSNaverShoppingLIVEAPI.h"
#include "utils-api.h"

namespace Ui {
class PLSLiveInfoNaverShoppingLIVEProductList;
}

class QLabel;
class QScrollArea;
class PLSPlatformNaverShoppingLIVE;
class PLSLiveInfoNaverShoppingLIVEProductItemView;
class PLSLoadingView;
class PLSNaverShoppingLIVEProductDialogView;

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

	QList<qint64> getFixedProductNos() const;
	QList<qint64> getUnfixedProductNos() const;
	QList<qint64> getAllProductNos() const;
	QList<Product> getFixedProducts() const;
	QList<Product> getUnfixedProducts() const;
	QList<Product> getAllProducts() const;
	int getProductNoCount() const;
	int getProductCount() const;

	void setOwnerScrollArea(QScrollArea *ownerScrollArea);
	void setProductNos(const QList<qint64> &fixedProductNos, const QList<qint64> &unfixedProductNos, std::function<void()> &&updateFinished = nullptr);
	void setAllProducts(const QList<Product> &products, bool update = false);
	void setProducts(QList<Product> fixedProducts, QList<Product> unfixedProducts, bool update = false);
	void setProductReadonly(bool readonly);
	void setIsLiving(bool isLiving);
	void setIsScheduleLive(bool isScheduleLive);
	void setIsScheduleLiveLoading(bool isScheduleLiveLoading);
	void setIsPlanningLive(bool isPlanningLive);
	void expiredClose();
	void resetIconSize() const;
	void updateWhenDpiChanged() const;

private:
	void updateAllProductsInfo();
	void updateAllProductsInfo_lambda(bool ok, QList<PLSNaverShoppingLIVEAPI::ProductInfo> &fixedProducts, QList<PLSNaverShoppingLIVEAPI::ProductInfo> &unfixedProducts,
					  const std::chrono::steady_clock::time_point &start);
	void updateAllProductsInfo_lambda_ok(QList<PLSNaverShoppingLIVEAPI::ProductInfo> &fixedProducts, QList<PLSNaverShoppingLIVEAPI::ProductInfo> &unfixedProducts);
	void updateProductsInfo(const QList<Product> &fixedProducts, const QList<Product> &unfixedProducts);
	void updateFixProductTip(bool ok = true);
	void updateProductCountBadge(bool ok = true);
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
	QList<qint64> fixedProductNos;
	QList<qint64> unfixedProductNos;
	QList<Product> fixedProducts;
	QList<Product> unfixedProducts;
	QList<PLSLiveInfoNaverShoppingLIVEProductItemView *> itemViews;
	PLSLoadingView *productLoadingView = nullptr;
	PLSNaverShoppingLIVEProductDialogView *productDialogView = nullptr;
	std::function<void()> setProductNosUpdateFinished = nullptr;
};

#endif // PLSLIVEINFONAVERSHOPPINGLIVEPRODUCTLIST_H
