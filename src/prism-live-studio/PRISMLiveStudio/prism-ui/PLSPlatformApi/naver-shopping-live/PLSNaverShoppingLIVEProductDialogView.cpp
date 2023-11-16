
#include "PLSNaverShoppingLIVEProductDialogView.h"
#include "ui_PLSNaverShoppingLIVEProductDialogView.h"
#include "PLSPlatformNaverShoppingLIVE.h"
#include "PLSNaverShoppingLIVEProductItemView.h"
#include "PLSNaverShoppingLIVEAPI.h"
#include "PLSNaverShoppingLIVEDataManager.h"
#include "PLSNaverShoppingLIVESearchKeywordItemView.h"
#include "PLSNaverShoppingLIVEStoreNameList.h"
#include "PLSLoadNextPage.h"
#include "PLSLoadingView.h"
#include "PLSAlertView.h"
#include "frontend-api.h"
#include "PLSShoppingRhythmicity.h"
#include <QApplication>
#include <QJsonObject>
#include <QMenu>
#include <QWidgetAction>
#include <QListWidget>
#include <array>
#include "utils-api.h"
#include "libui.h"
#include "liblog.h"
#include "login-info.hpp"
#include "libutils-api.h"
#include "PLSSyncServerManager.hpp"

#define ThisIsValid [](const QObject *obj) { return pls_object_is_valid(obj); }

const int MAX_SEARCH_KEYWORDS_COUNT = 15;
const int MAX_SELECTED_PRODUCT_COUNT = 30;
const int TAB_MIN = 0;
const int TAB_MAX = 3;
const int RECENT_TAB = 0;
const int STORE_TAB = 1;
const int SEARCH_TAB = 2;

static void clearList(QVBoxLayout *layout, QList<PLSNaverShoppingLIVEProductItemView *> &products, int &currentPage)
{
	PLSNaverShoppingLIVEProductItemView::dealloc(products, layout);
	currentPage = 0;
}

static inline QPoint operator+(const QPoint &pt, const QSize &sz)
{
	return QPoint(pt.x() + sz.width(), pt.y() + sz.height());
}

static void clearDefault(const QList<QPushButton *> &buttons)
{
	for (QPushButton *button : buttons) {
		button->setAutoDefault(false);
		button->setDefault(false);
	}
}
template<typename... PushButton> static void setCurrentButton(QPushButton *current, PushButton &&...others)
{
	const size_t OthersCount = sizeof...(PushButton);
	std::array<QPushButton *, OthersCount> buttons = {others...};
	for (size_t i = 0; i < OthersCount; ++i) {
		pls_flush_style(buttons[i], "state", "none");
	}

	pls_flush_style(current, "state", "current");
}

static void updateUI_recentEmpty(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->recentEmptyTipsLabel->setText(QObject::tr("NaverShoppingLive.ProductDialog.Recent.NoProduct"));
	ui->recentScrollAreaWidget->hide();
	ui->recentNoNet->hide();
	ui->recentEmpty->show();
}
static void updateUI_recentNotEmpty(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->recentNoNet->hide();
	ui->recentEmpty->hide();
	ui->recentScrollAreaWidget->show();
}
static void updateUI_recentFailed(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->recentNoNetTipsLabel->setText(QObject::tr("NaverShoppingLive.LiveInfo.ProductInfoUpdateApiFailedTip"));
	ui->recentNoNet->setProperty("usingType", "GetRecentApiFailed");
	ui->recentScrollAreaWidget->hide();
	ui->recentEmpty->hide();
	ui->recentNoNet->show();
}
static void updateUI_recentNoNet(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->recentNoNetTipsLabel->setText(QObject::tr("NaverShoppingLive.LiveInfo.Product.NoNetTip"));
	ui->recentNoNet->setProperty("usingType", "NoNet");
	ui->recentScrollAreaWidget->hide();
	ui->recentEmpty->hide();
	ui->recentNoNet->show();
}

static void updateUI_storeNoStore(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->noSmartStoreTipLabel->setText(QObject::tr("NaverShoppingLive.ProductDialog.Store.NoStoreTip"));
	ui->noSmartStoreButton->setProperty("usingType", "NoStore");
	ui->storeSearchBarWidget->hide();
	ui->storeScrollAreaWidget->hide();
	ui->storeNoNetWidget->hide();
	ui->storeSearchNoNetWidget->hide();
	ui->noSmartStoreButton->hide();
	ui->noStoreProductsWidget->hide();
	ui->noStoreSearchResultWidget->hide();
	ui->storeApiFailedWidget->hide();
	ui->storeWidget->hide();
	ui->noSmartStoreWidget->show();
}
static void updateUI_storeStoreFailed(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->noSmartStoreTipLabel->setText(QObject::tr("NaverShoppingLive.ProductDialog.Store.SearchStoreApiFailedTip"));
	ui->noSmartStoreButton->setText(QObject::tr("Retry"));
	ui->noSmartStoreButton->setProperty("usingType", "GetStoreApiFailed");
	ui->storeSearchBarWidget->hide();
	ui->storeScrollAreaWidget->hide();
	ui->storeNoNetWidget->hide();
	ui->storeSearchNoNetWidget->hide();
	ui->noStoreProductsWidget->hide();
	ui->noStoreSearchResultWidget->hide();
	ui->storeApiFailedWidget->hide();
	ui->storeWidget->hide();
	ui->noSmartStoreButton->show();
	ui->noSmartStoreWidget->show();
}
static void updateUI_storeEmpty(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->noSmartStoreWidget->hide();
	ui->storeScrollAreaWidget->hide();
	ui->storeNoNetWidget->hide();
	ui->storeSearchNoNetWidget->hide();
	ui->storeApiFailedWidget->hide();
	ui->storeSearchBarWidget->hide();
	ui->noStoreSearchResultWidget->hide();
	ui->storeWidget->show();
	ui->noStoreProductsWidget->show();
}
static void updateUI_storeBeginSearch(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->noSmartStoreWidget->hide();
	ui->storeNoNetWidget->hide();
	ui->storeSearchNoNetWidget->hide();
	ui->noStoreProductsWidget->hide();
	ui->noStoreSearchResultWidget->hide();
	ui->storeApiFailedWidget->hide();
	ui->storeWidget->show();
	ui->storeSearchBarWidget->show();
	ui->storeScrollAreaWidget->show();
}
static void updateUI_storeNoNet(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->noSmartStoreWidget->hide();
	ui->noStoreProductsWidget->hide();
	ui->storeScrollAreaWidget->hide();
	ui->storeApiFailedWidget->hide();
	ui->noStoreSearchResultWidget->hide();
	ui->storeSearchNoNetWidget->hide();
	ui->storeWidget->hide();
	ui->storeSearchBarWidget->hide();
	ui->storeNoNetWidget->show();
}
static void updateUI_storeSearchNoNet(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->noSmartStoreWidget->hide();
	ui->noStoreProductsWidget->hide();
	ui->storeScrollAreaWidget->hide();
	ui->storeApiFailedWidget->hide();
	ui->noStoreSearchResultWidget->hide();
	ui->storeNoNetWidget->hide();
	ui->storeWidget->show();
	ui->storeSearchBarWidget->show();
	ui->storeSearchNoNetWidget->show();
}
static void updateUI_storeSearchResultEmpty(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->noSmartStoreWidget->hide();
	ui->storeScrollAreaWidget->hide();
	ui->storeNoNetWidget->hide();
	ui->storeSearchNoNetWidget->hide();
	ui->storeApiFailedWidget->hide();
	ui->noStoreProductsWidget->hide();
	ui->storeWidget->show();
	ui->storeSearchBarWidget->show();
	ui->noStoreSearchResultWidget->show();
}
static void updateUI_storeSearchResultNotEmpty(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->noSmartStoreWidget->hide();
	ui->storeNoNetWidget->hide();
	ui->storeSearchNoNetWidget->hide();
	ui->noStoreProductsWidget->hide();
	ui->storeApiFailedWidget->hide();
	ui->noStoreSearchResultWidget->hide();
	ui->storeWidget->show();
	ui->storeSearchBarWidget->show();
	ui->storeScrollAreaWidget->show();
}
static void updateUI_storeSearchFailed(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->storeSearchBarWidget->hide();
	ui->noSmartStoreWidget->hide();
	ui->storeScrollAreaWidget->hide();
	ui->noStoreProductsWidget->hide();
	ui->noStoreSearchResultWidget->hide();
	ui->storeNoNetWidget->hide();
	ui->storeSearchNoNetWidget->hide();
	ui->storeWidget->show();
	ui->storeSearchBarWidget->show();
	ui->storeApiFailedWidget->show();
}

static void updateUI_searchKeywords(Ui::PLSNaverShoppingLIVEProductDialogView *ui, bool hasKeywords)
{
	ui->searchTitleLabel->setText(QObject::tr("NaverShoppingLive.ProductDialog.SearchKeywords.Title"));
	ui->searchTitleLabel->setVisible(hasKeywords);
	ui->searchScrollArea->hide();
	ui->searchNoNetWidget->hide();
	if (hasKeywords) {
		ui->emptySearchResultWidget->hide();
		ui->searchKeywordsScrollArea->show();
		ui->contentsWidget->show();
	} else {
		ui->emptySearchResultLabel->setText(QObject::tr("NaverShoppingLive.ProductDialog.NoSearchKeywords"));
		ui->searchKeywordsScrollArea->hide();
		ui->contentsWidget->hide();
		ui->emptySearchResultWidget->show();
	}
}
static void updateUI_searchResultEmpty(Ui::PLSNaverShoppingLIVEProductDialogView *ui, bool byUrl)
{
	ui->emptySearchResultLabel->setText(byUrl ? QObject::tr("NaverShoppingLive.ProductDialog.UrlSearchApiFailed") : QObject::tr("NaverShoppingLive.ProductDialog.NoSearchResult"));
	ui->searchScrollArea->hide();
	ui->searchKeywordsScrollArea->hide();
	ui->contentsWidget->hide();
	ui->searchTitleLabel->hide();
	ui->searchNoNetWidget->hide();
	ui->emptySearchResultWidget->show();
}
static void updateUI_searchResultNotEmpty(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->searchTitleLabel->setText(QObject::tr("NaverShoppingLive.ProductDialog.SearchResult.Title"));
	ui->searchKeywordsScrollArea->hide();
	ui->emptySearchResultWidget->hide();
	ui->searchNoNetWidget->hide();
	ui->searchTitleLabel->show();
	ui->searchScrollArea->show();
	ui->contentsWidget->show();
}
static void updateUI_searchNoNet(Ui::PLSNaverShoppingLIVEProductDialogView *ui)
{
	ui->searchTitleLabel->setText(QObject::tr("NaverShoppingLive.ProductDialog.SearchResult.Title"));
	ui->searchNoNetLabel->setText(QObject::tr("NaverShoppingLive.LiveInfo.Product.NoNetTip"));
	ui->searchNoNetButton->setProperty("usingType", "NoNet");
	ui->searchNoNetButton->show();
	ui->searchScrollArea->hide();
	ui->searchKeywordsScrollArea->hide();
	ui->contentsWidget->hide();
	ui->emptySearchResultWidget->hide();
	ui->searchTitleLabel->show();
	ui->searchNoNetWidget->show();
}
static void updateUI_searchResultFailed(Ui::PLSNaverShoppingLIVEProductDialogView *ui, bool byUrl)
{
	ui->searchNoNetLabel->setText(byUrl ? QObject::tr("NaverShoppingLive.ProductDialog.UrlSearchApiFailed") : QObject::tr("NaverShoppingLive.ProductDialog.SearchApiFailed"));
	ui->searchNoNetButton->setProperty("usingType", "SearchApiFailed");
	ui->searchNoNetButton->setVisible(!byUrl);
	ui->emptySearchResultWidget->hide();
	ui->searchScrollArea->hide();
	ui->searchKeywordsScrollArea->hide();
	ui->contentsWidget->hide();
	ui->searchTitleLabel->hide();
	ui->searchNoNetWidget->show();
}

static QList<PLSNaverShoppingLIVEProductDialogView::SmartStoreInfo> toSmartStores(const QList<PLSNaverShoppingLIVEAPI::GetSelectiveAccountStore> &stores)
{
	QList<PLSNaverShoppingLIVEProductDialogView::SmartStoreInfo> smartStores;
	for (const PLSNaverShoppingLIVEAPI::GetSelectiveAccountStore &store : stores) {
		smartStores.append(PLSNaverShoppingLIVEProductDialogView::SmartStoreInfo{store.channelNo, store.channelName});
	}
	return smartStores;
}

template<typename T> static bool removeAtByProductNo(QList<T> &list, qint64 productNo)
{
	if constexpr (std::is_pointer_v<T>) {
		for (int i = 0, count = list.count(); i < count; ++i) {
			if (list[i]->productNo == productNo) {
				list.removeAt(i);
				return true;
			}
		}
	} else {
		for (int i = 0, count = list.count(); i < count; ++i) {
			if (list[i].productNo == productNo) {
				list.removeAt(i);
				return true;
			}
		}
	}
	return false;
}

template<typename T> static bool findByProductNo(T &rItem, QList<T> &list, qint64 productNo)
{
	if constexpr (std::is_pointer_v<T>) {
		for (int i = 0, count = list.count(); i < count; ++i) {
			auto item = list[i];
			if (item->productNo == productNo) {
				rItem = item;
				return true;
			}
		}
	} else {
		for (int i = 0, count = list.count(); i < count; ++i) {
			auto &item = list[i];
			if (item.productNo == productNo) {
				rItem = item;
				return true;
			}
		}
	}
	return false;
}

template<typename T, typename IsProductNo> static bool findByProductNo(const QList<T> &list, IsProductNo isProductNo)
{
	for (int i = 0, count = list.count(); i < count; ++i) {
		if (isProductNo(list[i])) {
			return true;
		}
	}
	return false;
}

template<typename T, typename IsProductNo> static bool findByProductNo(T &ritem, QList<T> &list, IsProductNo isProductNo)
{
	for (int i = 0, count = list.count(); i < count; ++i) {
		T &item = list[i];
		if (isProductNo(item)) {
			ritem = item;
			return true;
		}
	}
	return false;
}

static bool findSmartStore(PLSNaverShoppingLIVEProductDialogView::SmartStoreInfo &smartStore, QList<PLSNaverShoppingLIVEProductDialogView::SmartStoreInfo> &smartStores, const QString &storeId)
{
	if (smartStores.isEmpty() || storeId.isEmpty()) {
		return false;
	}

	for (int i = 0, count = smartStores.count(); i < count; ++i) {
		const auto &item = smartStores[i];
		if (item.storeId == storeId) {
			smartStore = item;
			return true;
		}
	}
	return false;
}

template<typename ProductType> static QStringList toProductNoList(const QList<ProductType> &products)
{
	QStringList productNoList;
	for (const auto &product : products) {
		productNoList.append(QString::number(product.productNo));
	}
	return productNoList;
}

static void listenVerticalScrollBar(QScrollBar *&scrollBar, const QScrollArea *scrollArea, QLayout *layout, QObject *listener)
{
	if (scrollBar = scrollArea->verticalScrollBar(); scrollBar) {
		scrollBar->installEventFilter(listener);
	}
}

static void processScrollContentMarginRight(const QScrollBar *scrollBar, QLayout *layout, const QMargins &margins, bool show)
{
	if (show) {
		layout->setContentsMargins(margins.left(), margins.top(), margins.right() - scrollBar->width(), margins.bottom());
	} else {
		layout->setContentsMargins(margins);
	}
}

static void processScrollContentMarginRight(PLSNaverShoppingLIVEProductDialogView *dlg, const QScrollBar *scrollBar, QLayout *layout, bool show, bool isProduct)
{
	processScrollContentMarginRight(scrollBar, layout, isProduct ? QMargins(24, 0, 29, 20) : QMargins(29, 0, 29, 20), show);
}

static void processScrollContentMarginRight(PLSNaverShoppingLIVEProductDialogView *dlg, const QScrollBar *scrollBar, QLayout *layout, bool isProduct)
{
	if (scrollBar) {
		processScrollContentMarginRight(scrollBar, layout, isProduct ? QMargins(24, 0, 29, 20) : QMargins(29, 0, 29, 20), scrollBar->isVisible());
	}
}

void pls_login_async(const std::function<void(bool ok, const QJsonObject &result)> &callback, const QString &platformName, QWidget *parent, PLSLoginInfo::UseFor useFor);

PLSNaverShoppingLIVEProductDialogView::PLSNaverShoppingLIVEProductDialogView(PLSPlatformNaverShoppingLIVE *platform_, const QList<Product> &selectedProducts_, bool isLiving_, bool isPlanningLive_,
									     QWidget *parent)
	: PLSDialogView(parent), platform(platform_), selectedProducts(selectedProducts_), originProducts(selectedProducts_), isLiving(isLiving_), isPlanningLive(isPlanningLive_)
{
	ui = pls_new<Ui::PLSNaverShoppingLIVEProductDialogView>();
	setResizeEnabled(false);
	pls_set_css(this, {"PLSNaverShoppingLIVEProductDialogView"});
	setupUi(ui);
	setFixedSize(580, 650);
	clearDefault(findChildren<QPushButton *>());
	ui->storeWidget->installEventFilter(this);
	ui->storeNameWidget->installEventFilter(this);
	ui->storeNameWidgetSpacer->installEventFilter(this);
	ui->storeChangeStoreButtonContainer->hide();

	auto lang = pls_get_current_language();
	ui->recentNoNetRetryButton->setProperty("lang", lang);
	ui->storeNoNetButton->setProperty("lang", lang);
	ui->searchNoNetButton->setProperty("lang", lang);
	ui->storeApiFailedRetryButton->setProperty("lang", lang);
	ui->storeSearchNoNetButton->setProperty("lang", lang);
	ui->storeChangeStoreButton->setProperty("lang", lang);
	ui->noSmartStoreButton->setProperty("lang", lang);
	pls_flush_style(ui->recentNoNetRetryButton);

	listenVerticalScrollBar(recentProductVScrollBar, ui->recentScrollArea, ui->recentContentLayout, this);
	listenVerticalScrollBar(storeProductVScrollBar, ui->storeScrollArea, ui->storeContentLayout, this);
	listenVerticalScrollBar(searchProductVScrollBar, ui->searchScrollArea, ui->searchContentLayout, this);
	listenVerticalScrollBar(searchKeywordVScrollBar, ui->searchKeywordsScrollArea, ui->searchKeywordsContentLayout, this);
	processScrollContentMarginRight(this, recentProductVScrollBar, ui->recentContentLayout, true);
	processScrollContentMarginRight(this, storeProductVScrollBar, ui->storeContentLayout, true);
	processScrollContentMarginRight(this, searchProductVScrollBar, ui->searchContentLayout, true);

	connect(this, &PLSNaverShoppingLIVEProductDialogView::lineEditSetFocus, this, &PLSNaverShoppingLIVEProductDialogView::onLineEditSetFocus, Qt::QueuedConnection);
	connect(this, &PLSNaverShoppingLIVEProductDialogView::flushLineEditStyle, this, &PLSNaverShoppingLIVEProductDialogView::onFlushLineEditStyle, Qt::QueuedConnection);

	auto manager = PLSNaverShoppingLIVEDataManager::instance();
	connect(ui->recentButton, &QPushButton::clicked, this, [this, manager]() {
		PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Recent Button", ACTION_CLICK);

		if (recentNeedUpdateItems) {
			recentNeedUpdateItems = false;
			updateRecentItems();
		}

		setCurrentButton(ui->recentButton, ui->storeButton, ui->searchButton);
		ui->stackedWidget->setCurrentIndex(RECENT_TAB);
		manager->setLatestUseTab(RECENT_TAB);
	});
	connect(ui->storeButton, &QPushButton::clicked, this, [this, manager]() {
		PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "My Store Button", ACTION_CLICK);

		setCurrentButton(ui->storeButton, ui->recentButton, ui->searchButton);
		ui->stackedWidget->setCurrentIndex(STORE_TAB);
		manager->setLatestUseTab(STORE_TAB);
		lineEditSetFocus();
	});
	connect(ui->searchButton, &QPushButton::clicked, this, [this, manager]() {
		PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Search Button", ACTION_CLICK);

		setCurrentButton(ui->searchButton, ui->recentButton, ui->storeButton);
		ui->stackedWidget->setCurrentIndex(SEARCH_TAB);
		manager->setLatestUseTab(SEARCH_TAB);
		lineEditSetFocus();
	});

	ui->storeSearchBarLineEdit->installEventFilter(this);
	ui->searchSearchBarLineEdit->installEventFilter(this);

	ui->storeSearchBarClearButton->hide();
	ui->searchSearchBarClearButton->hide();

	updateUI_storeSearchResultNotEmpty(ui);

	initRecentProducts();
	initSmartStores();
	updateSmartStoreSearch();
	updateSearchKeywords();
	updateCountTip();

	if (int latestUseTab = manager->getLatestUseTab(); (latestUseTab >= TAB_MIN) && (latestUseTab < TAB_MAX)) {
		if (ui->searchButton->isHidden() && latestUseTab == SEARCH_TAB) {
			latestUseTab = STORE_TAB;
		}
		switch (latestUseTab) {
		case RECENT_TAB:
			ui->recentButton->click();
			break;
		case STORE_TAB:
			ui->storeButton->click();
			break;
		case SEARCH_TAB:
			ui->searchButton->click();
			break;
		default:
			break;
		}
	} else if (manager->hasRecentProductNos()) {
		ui->recentButton->click();
	} else {
		ui->storeButton->click();
	}

	/*dpiHelper.notifyDpiChanged(this, [this, parent](double, double, bool firstShow) {
		processScrollContentMarginRight(this, recentProductVScrollBar, ui->recentContentLayout, true);
		processScrollContentMarginRight(this, storeProductVScrollBar, ui->storeContentLayout, true);
		processScrollContentMarginRight(this, searchProductVScrollBar, ui->searchContentLayout, true);
		processScrollContentMarginRight(this, searchKeywordVScrollBar, ui->searchKeywordsContentLayout, false);

		if (!firstShow) {
			QMetaObject::invokeMethod(ui->storeNameLabel, "setText", Qt::QueuedConnection, Q_ARG(QString, getCurrentSmartStoreName()));

			QMetaObject::invokeMethod(
				this, [this]() { pls_flush_style(ui->storeSearchBarLineEdit); }, Qt::QueuedConnection);
			QMetaObject::invokeMethod(
				this, [this]() { pls_flush_style(ui->searchSearchBarLineEdit); }, Qt::QueuedConnection);

			PLSNaverShoppingLIVEProductItemView::updateWhenDpiChanged(recentProductItemViews);
			PLSNaverShoppingLIVEProductItemView::updateWhenDpiChanged(storeProductItemViews);
			PLSNaverShoppingLIVEProductItemView::updateWhenDpiChanged(searchProductItemViews);
			return;
		}

		move(parent->mapToGlobal(QPoint(0, 0)) + (parent->size() - this->size()) / 2);
		activateWindow();
	});*/
}

PLSNaverShoppingLIVEProductDialogView::~PLSNaverShoppingLIVEProductDialogView()
{
	PLSNaverShoppingLIVEProductItemView::dealloc(recentProductItemViews, ui->recentContentLayout);
	PLSNaverShoppingLIVEProductItemView::dealloc(storeProductItemViews, ui->storeContentLayout);
	PLSNaverShoppingLIVEProductItemView::dealloc(searchProductItemViews, ui->searchContentLayout);
	PLSNaverShoppingLIVESearchKeywordItemView::dealloc(searchKeywordItemViews, ui->searchKeywordsContentLayout);
	PLSLoadingView::deleteLoadingView(recentProductsLoadingView);
	PLSLoadingView::deleteLoadingView(storeProductsLoadingView);
	PLSLoadingView::deleteLoadingView(searchProductsLoadingView);
	PLSLoadNextPage::deleteLoadNextPage(recentProductNextPage);
	PLSLoadNextPage::deleteLoadNextPage(storeProductNextPage);
	PLSLoadNextPage::deleteLoadNextPage(searchProductNextPage);
	pls_delete(ui, nullptr);
}

QString PLSNaverShoppingLIVEProductDialogView::getLang() const
{
	return pls_get_current_language();
}

QList<qint64> PLSNaverShoppingLIVEProductDialogView::getSelectedProductNos() const
{
	QList<qint64> selectedProductNos;
	for (const auto &selectedProduct : selectedProducts) {
		selectedProductNos.append(selectedProduct.productNo);
	}
	return selectedProductNos;
}

QList<PLSNaverShoppingLIVEProductDialogView::Product> PLSNaverShoppingLIVEProductDialogView::getSelectedProducts() const
{
	return selectedProducts;
}

QList<qint64> PLSNaverShoppingLIVEProductDialogView::getNewRecentProductNos() const
{
	return newRecentProductNos;
}

QString PLSNaverShoppingLIVEProductDialogView::getCurrentSmartStoreName() const
{
	return currentSmartStore.storeName;
}

void PLSNaverShoppingLIVEProductDialogView::initRecentProducts(int currentPage)
{
	bool isFirstPage = currentPage == 0;

	PLSLoadNextPage::deleteLoadNextPage(recentProductNextPage);

	if (isFirstPage && !pls_get_network_state()) {
		updateUI_recentNoNet(ui);
		return;
	}

	bool next = false;
	QList<qint64> recentProductNos = PLSNaverShoppingLIVEDataManager::instance()->getRecentProductNos(next, currentPage, PLSNaverShoppingLIVEDataManager::PRODUCT_PAGE_SIZE);
	if (!isFirstPage || !recentProductNos.isEmpty()) {
		updateUI_recentNotEmpty(ui);
	} else if (pls_get_network_state()) {
		updateUI_recentEmpty(ui);
		return;
	} else {
		updateUI_recentNoNet(ui);
		return;
	}

	if (isFirstPage) {
		PLSLoadingView::newLoadingViewEx(recentProductsLoadingView, ui->recentPage);
		clearList(ui->recentContentLayout, recentProductItemViews, currentPage);
	}

	PLSNaverShoppingLIVEAPI::productSearchByProductNos(
		platform, recentProductNos,
		[this, isFirstPage, currentPage, next](bool ok, const QList<PLSNaverShoppingLIVEAPI::ProductInfo> &products) {
			processProductSearchByProductNos(ok, products, isFirstPage, currentPage, next);
		},
		this, ThisIsValid);
}

void PLSNaverShoppingLIVEProductDialogView::initSmartStores()
{
	auto manager = PLSNaverShoppingLIVEDataManager::instance();

	connect(manager, &PLSNaverShoppingLIVEDataManager::smartStoreInfoUpdated, this, [this](bool ok, const QString &storeId, const QString &storeName) {
		if (ok) {
			smartStores = QList{SmartStoreInfo{storeId, storeName}};
		} else {
			smartStores.clear();
		}

		updateSmartStores(!ok);
	});

	if (platform->getAccountType() == NaverShoppingAccountType::NaverShoppingSmartStore) {
		smartStores.append(SmartStoreInfo{platform->getUserInfo().broadcasterId, platform->getUserInfo().nickname});
	} else if (manager->hasSmartStoreInfo()) {
		smartStores.append(SmartStoreInfo{manager->getSmartStoreId(), manager->getSmartStoreName()});
	} else if (pls_get_network_state()) {
		searchSmartStores();
	}

	updateSmartStores();
}

void PLSNaverShoppingLIVEProductDialogView::searchSmartStores()
{
	PLSLoadingView::newLoadingViewEx(storeProductsLoadingView, ui->storePage);
	PLSNaverShoppingLIVEAPI::getSelectiveAccountStores(
		platform,
		[this](bool ok, const QList<PLSNaverShoppingLIVEAPI::GetSelectiveAccountStore> &stores) {
			PLSLoadingView::deleteLoadingView(storeProductsLoadingView);

			if (ok) {
				smartStores = toSmartStores(stores);
			} else {
				smartStores.clear();
			}

			updateSmartStores(!ok);
		},
		this, ThisIsValid);
}

#define CHECK_SEARCH_INDEX(index)   \
	if (searchIndex != index) { \
		return;             \
	}

void PLSNaverShoppingLIVEProductDialogView::storeSearch(const QString &text, int currentPage)
{
	bool isFirstPage = currentPage == 0;

	PLSLoadNextPage::deleteLoadNextPage(storeProductNextPage);

	if (isFirstPage) { // first page
		PLSLoadingView::newLoadingViewEx(storeProductsLoadingView, ui->storeScrollAreaWidget);
		clearList(ui->storeContentLayout, storeProductItemViews, currentPage);
		updateUI_storeBeginSearch(ui);
	}

	if (isFirstPage && !pls_get_network_state()) {
		PLSLoadingView::deleteLoadingView(storeProductsLoadingView);
		updateUI_storeSearchNoNet(ui);
		return;
	}

	PLSNaverShoppingLIVEAPI::storeChannelProductSearch(
		platform, currentSmartStore.storeId, text, currentPage, 20,
		[this, isFirstPage, text, currentPage, searchIndex = getSearchIndex(storeSearchIndex)](bool ok, const QList<Product> &products, bool next, int page) {
			processStoreChannelProductSearch(ok, products, next, page, isFirstPage, text, currentPage, searchIndex);
		},
		this, ThisIsValid);
}

void PLSNaverShoppingLIVEProductDialogView::searchSearch(const QString &text, int currentPage)
{
	bool isFirstPage = currentPage == 0;

	PLSLoadNextPage::deleteLoadNextPage(searchProductNextPage);

	if (isFirstPage) {
		PLSLoadingView::newLoadingViewEx(searchProductsLoadingView, ui->contentsWidget);
		clearList(ui->searchContentLayout, searchProductItemViews, currentPage);
		updateUI_searchResultNotEmpty(ui);
	}

	if (isFirstPage && !pls_get_network_state()) {
		PLSLoadingView::deleteLoadingView(searchProductsLoadingView);
		updateUI_searchNoNet(ui);
		return;
	}

	if (text.startsWith("http://") || text.startsWith("https://")) {
		PLSNaverShoppingLIVEAPI::productSearchByUrl(
			platform, text,
			[this, searchIndex = getSearchIndex(searchSearchIndex)](bool ok, bool hasProduct, const Product &product) { processSearchSearch(ok, hasProduct, product, searchIndex); }, this,
			ThisIsValid);
	} else {
		PLSNaverShoppingLIVEAPI::productSearchByTag(
			platform, text, currentPage, 20,
			[this, isFirstPage, text, currentPage, searchIndex = getSearchIndex(searchSearchIndex)](bool ok, const QList<Product> &products, bool next, int page) {
				processSearchSearch(ok, products, next, page, isFirstPage, text, currentPage, searchIndex);
			},
			this, ThisIsValid);
	}
}

void PLSNaverShoppingLIVEProductDialogView::updateSearchKeywords(const QString &searchKeyword, bool showSearchKeywords)
{
	if (ui->searchButton->isHidden()) {
		return;
	}
	PLSNaverShoppingLIVEDataManager *manager = PLSNaverShoppingLIVEDataManager::instance();
	QStringList searchKeywords = searchKeyword.isEmpty() ? manager->getSearchKeywords() : manager->addSearchKeyword(searchKeyword);
	if (searchKeywords.count() <= searchKeywordItemViews.count()) {
		for (int i = 0, count = searchKeywords.count(); i < count; ++i) {
			searchKeywordItemViews[i]->setInfo(searchKeywords[i]);
		}

		while (searchKeywordItemViews.count() > searchKeywords.count()) {
			auto view = searchKeywordItemViews.takeLast();
			ui->searchKeywordsContentLayout->removeWidget(view);
			PLSNaverShoppingLIVESearchKeywordItemView::dealloc(view);
		}
	} else {
		for (int i = 0, count = searchKeywordItemViews.count(); i < count; ++i) {
			searchKeywordItemViews[i]->setInfo(searchKeywords[i]);
		}

		for (int i = searchKeywordItemViews.count(), count = searchKeywords.count(); i < count; ++i) {
			auto view = PLSNaverShoppingLIVESearchKeywordItemView::alloc(ui->searchKeywordsContent, this, &PLSNaverShoppingLIVEProductDialogView::onRemoveButtonClicked,
										     &PLSNaverShoppingLIVEProductDialogView::onSearchButtonClicked, searchKeywords[i]);
			ui->searchKeywordsContentLayout->addWidget(view);
			pls_flush_style_recursive(view);
			this->searchKeywordItemViews.append(view);
			view->show();
		}
	}

	if (showSearchKeywords) {
		updateUI_searchKeywords(ui, !searchKeywordItemViews.isEmpty());
		PLSNaverShoppingLIVESearchKeywordItemView::updateSearchKeyword(searchKeywordItemViews);
	}
}

bool PLSNaverShoppingLIVEProductDialogView::isSelectedProductNo(qint64 productNo) const
{
	return std::any_of(selectedProducts.begin(), selectedProducts.end(), [productNo](const auto &selectedProduct) { return selectedProduct.productNo == productNo; });
}

bool PLSNaverShoppingLIVEProductDialogView::findItemView(PLSNaverShoppingLIVEProductItemView *&itemView, const QList<PLSNaverShoppingLIVEProductItemView *> &itemViews, qint64 productNo) const
{
	for (int i = 0, count = itemViews.count(); i < count; ++i) {
		itemView = itemViews[i];
		if (itemView->getProductNo() == productNo) {
			return true;
		}
	}
	return false;
}

void PLSNaverShoppingLIVEProductDialogView::syncItemViewSelectedState(const QList<PLSNaverShoppingLIVEProductItemView *> *current, const QList<PLSNaverShoppingLIVEProductItemView *> &itemViews,
								      qint64 productNo, bool selected) const
{
	if (current == &itemViews) {
		return;
	}

	PLSNaverShoppingLIVEProductItemView *itemView = nullptr;
	if (findItemView(itemView, itemViews, productNo)) {
		itemView->setSelected(selected);
	}
}

void PLSNaverShoppingLIVEProductDialogView::syncItemViewSelectedState(const QList<PLSNaverShoppingLIVEProductItemView *> *current, qint64 productNo, bool selected) const
{
	syncItemViewSelectedState(current, recentProductItemViews, productNo, selected);
	syncItemViewSelectedState(current, storeProductItemViews, productNo, selected);
	syncItemViewSelectedState(current, searchProductItemViews, productNo, selected);
}

void PLSNaverShoppingLIVEProductDialogView::updateRecentItems()
{
	if (recentProducts.count() <= recentProductItemViews.count()) {
		for (int i = 0, count = recentProducts.count(); i < count; ++i) {
			const auto &product = recentProducts[i];
			auto view = recentProductItemViews[i];
			view->setInfo(platform, product);
			view->setSelected(isSelectedProductNo(product.productNo));
			view->setStoreNameVisible(true);
		}

		if (recentProductItemViews.count() > recentProducts.count()) {
			while (recentProductItemViews.count() > recentProducts.count()) {
				auto view = recentProductItemViews.takeLast();
				ui->recentContentLayout->removeWidget(view);
				PLSNaverShoppingLIVEProductItemView::dealloc(view);
			}

			ui->recentContent->adjustSize();
		}
	} else {
		for (int i = 0, count = recentProductItemViews.count(); i < count; ++i) {
			const auto &product = recentProducts[i];
			auto view = recentProductItemViews[i];
			view->setInfo(platform, product);
			view->setSelected(isSelectedProductNo(product.productNo));
			view->setStoreNameVisible(true);
		}

		if (recentProducts.count() > recentProductItemViews.count()) {
			PLSNaverShoppingLIVEProductItemView::addBatchCache(recentProducts.count() - recentProductItemViews.count(), true);

			for (int i = recentProductItemViews.count(), count = recentProducts.count(); i < count; ++i) {
				const auto &product = recentProducts[i];
				auto view = PLSNaverShoppingLIVEProductItemView::alloc(
					ui->recentContent, this,
					[this](PLSNaverShoppingLIVEProductItemView *itemView, qint64 productNo) { onProductItemAddRemoveButtonClicked(&recentProductItemViews, itemView, productNo); },
					platform, product);
				view->setSelected(isSelectedProductNo(product.productNo));
				view->setStoreNameVisible(true);
				ui->recentContentLayout->addWidget(view);
				pls_flush_style_recursive(view);
				this->recentProductItemViews.append(view);
				view->show();
			}

			ui->recentContent->adjustSize();
		}
	}

	if (!recentProductItemViews.isEmpty()) {
		updateUI_recentNotEmpty(ui);
	} else {
		updateUI_recentEmpty(ui);
	}
}

void PLSNaverShoppingLIVEProductDialogView::updateCountTip()
{
	ui->countTipLabel->setVisible(!selectedProducts.isEmpty());
	if (selectedProducts.count() > 1) {
		ui->countTipLabel->setText(tr("NaverShoppingLive.ProductDialog.AddProduct.Counts").arg(selectedProducts.count()));
	} else {
		ui->countTipLabel->setText(tr("NaverShoppingLive.ProductDialog.AddProduct.Count").arg(selectedProducts.count()));
	}
}

void PLSNaverShoppingLIVEProductDialogView::updateSmartStoreSearch()
{
	if (platform->getAccountType() == NaverShoppingAccountType::NaverShoppingSmartStore) {
		ui->searchButton->setHidden(true);
	}
}

void PLSNaverShoppingLIVEProductDialogView::updateSmartStores(bool apiFailed)
{
	ui->storeNameIconLabel->setVisible(smartStores.count() > 1);

	if (!smartStores.isEmpty()) {
		if (!findSmartStore(currentSmartStore, smartStores, PLSNaverShoppingLIVEDataManager::instance()->getLatestStoreId())) {
			currentSmartStore = smartStores.first();
		}

		ui->storeNameLabel->setText(getCurrentSmartStoreName());
		updateUI_storeSearchResultNotEmpty(ui);
		storeSearch(ui->storeSearchBarLineEdit->text());
	} else if (pls_get_network_state()) {
		if (apiFailed) {
			updateUI_storeStoreFailed(ui);
		} else {
			updateUI_storeNoStore(ui);
		}
	} else {
		updateUI_storeNoNet(ui);
	}
}

qint64 PLSNaverShoppingLIVEProductDialogView::getSearchIndex(qint64 &searchIndex) const
{
	if ((searchIndex <= 0) || (searchIndex >= INT64_MAX)) {
		searchIndex = 0;
	}

	return ++searchIndex;
}

bool PLSNaverShoppingLIVEProductDialogView::asyncProcessAndCheckDone() const
{
#if 0
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
#endif
	return doneFlag;
}

QLayout *PLSNaverShoppingLIVEProductDialogView::vscrollBarToLayout(const QObject *vscrollBar) const
{
	if (vscrollBar == recentProductVScrollBar)
		return ui->recentContentLayout;
	else if (vscrollBar == storeProductVScrollBar)
		return ui->storeContentLayout;
	else if (vscrollBar == searchProductVScrollBar)
		return ui->searchContentLayout;
	else if (vscrollBar == searchKeywordVScrollBar)
		return ui->searchKeywordsContentLayout;
	return nullptr;
}

bool PLSNaverShoppingLIVEProductDialogView::isMyStoreProduct(const Product &product) const
{
	return std::any_of(smartStores.begin(), smartStores.end(), [&product](const SmartStoreInfo &smartStore) { return smartStore.storeId == product.channelNo; });
}

void PLSNaverShoppingLIVEProductDialogView::processProductSearchByProductNos(bool ok, const QList<PLSNaverShoppingLIVEAPI::ProductInfo> &products, bool isFirstPage, int currentPage, bool next)
{
	PLSLoadingView::deleteLoadingView(recentProductsLoadingView);
	if (asyncProcessAndCheckDone()) {
		return;
	}

	if (ok) {
		if (isFirstPage) {
			recentProducts = products;
		} else {
			recentProducts.append(products);
		}

		updateRecentItems();

		if (next) {
			PLSLoadNextPage::newLoadNextPage(recentProductNextPage, ui->recentScrollArea, [this, currentPage]() { initRecentProducts(currentPage + 1); });
		}
	} else if (isFirstPage) {
		updateUI_recentFailed(ui);
	} else {
		PLSLoadNextPage::newLoadNextPage(recentProductNextPage, ui->recentScrollArea, [this, currentPage]() { initRecentProducts(currentPage); });
	}
}

void PLSNaverShoppingLIVEProductDialogView::processStoreChannelProductSearch(bool ok, const QList<Product> &products, bool next, int page, bool isFirstPage, const QString &text, int currentPage,
									     qint64 searchIndex)
{
	CHECK_SEARCH_INDEX(storeSearchIndex)

	if (ok) {
		if (!products.isEmpty()) {
			CHECK_SEARCH_INDEX(storeSearchIndex)
			PLSLoadingView::deleteLoadingView(storeProductsLoadingView);
			CHECK_SEARCH_INDEX(storeSearchIndex)

			updateUI_storeSearchResultNotEmpty(ui);

			PLSNaverShoppingLIVEProductItemView::addBatchCache(products.count(), true);
			if (asyncProcessAndCheckDone()) {
				return;
			}

			for (const auto &product : products) {
				PLSNaverShoppingLIVEProductItemView *view = PLSNaverShoppingLIVEProductItemView::alloc(
					ui->storeContent, this,
					[this](PLSNaverShoppingLIVEProductItemView *itemView, qint64 productNo) { onProductItemAddRemoveButtonClicked(&storeProductItemViews, itemView, productNo); },
					platform, product);
				view->setSelected(isSelectedProductNo(product.productNo));
				view->setStoreNameVisible(false);
				ui->storeContentLayout->addWidget(view);
				pls_flush_style_recursive(view);
				this->storeProductItemViews.append(view);
				view->show();
			}

			ui->storeContent->adjustSize();

			if (next) {
				PLSLoadNextPage::newLoadNextPage(storeProductNextPage, ui->storeScrollArea, [this, text, page]() { storeSearch(text, page); });
			}
		} else {
			PLSLoadingView::deleteLoadingView(storeProductsLoadingView);
			CHECK_SEARCH_INDEX(storeSearchIndex)

			if (isFirstPage && text.isEmpty()) {
				updateUI_storeEmpty(ui);
			} else if (isFirstPage) {
				updateUI_storeSearchResultEmpty(ui);
			}
		}
	} else {
		PLSLoadingView::deleteLoadingView(storeProductsLoadingView);
		CHECK_SEARCH_INDEX(storeSearchIndex)

		if (isFirstPage) {
			updateUI_storeSearchFailed(ui);
		} else {
			PLSLoadNextPage::newLoadNextPage(storeProductNextPage, ui->storeScrollArea, [this, text, currentPage]() { storeSearch(text, currentPage); });
		}
	}
}

void PLSNaverShoppingLIVEProductDialogView::processSearchSearch(bool ok, bool hasProduct, const Product &product, qint64 searchIndex)
{
	CHECK_SEARCH_INDEX(searchSearchIndex)

	if (ok) {
		if (hasProduct) {
			CHECK_SEARCH_INDEX(searchSearchIndex)
			PLSLoadingView::deleteLoadingView(searchProductsLoadingView);
			CHECK_SEARCH_INDEX(searchSearchIndex)

			if (asyncProcessAndCheckDone()) {
				return;
			}

			PLSNaverShoppingLIVEProductItemView *view = nullptr;
			view = PLSNaverShoppingLIVEProductItemView::alloc(
				ui->searchContent, this,
				[this](PLSNaverShoppingLIVEProductItemView *itemView, qint64 productNo) { onProductItemAddRemoveButtonClicked(&searchProductItemViews, itemView, productNo); },
				platform, product);

			view->setSelected(isSelectedProductNo(product.productNo));
			view->setStoreNameVisible(true);
			ui->searchContentLayout->addWidget(view);
			pls_flush_style_recursive(view);
			this->searchProductItemViews.append(view);
			view->show();

			ui->searchContent->adjustSize();

			updateUI_searchResultNotEmpty(ui);
		} else {
			PLSLoadingView::deleteLoadingView(searchProductsLoadingView);
			CHECK_SEARCH_INDEX(searchSearchIndex)

			updateUI_searchResultEmpty(ui, true);
		}
	} else {
		PLSLoadingView::deleteLoadingView(searchProductsLoadingView);
		CHECK_SEARCH_INDEX(searchSearchIndex)

		updateUI_searchResultFailed(ui, true);
	}
}

void PLSNaverShoppingLIVEProductDialogView::processSearchSearch(bool ok, const QList<Product> &products, bool next, int page, bool isFirstPage, const QString &text, int currentPage,
								qint64 searchIndex)
{
	CHECK_SEARCH_INDEX(searchSearchIndex)

	if (ok) {
		if (!products.isEmpty()) {
			CHECK_SEARCH_INDEX(searchSearchIndex)
			PLSLoadingView::deleteLoadingView(searchProductsLoadingView);
			CHECK_SEARCH_INDEX(searchSearchIndex)

			updateUI_searchResultNotEmpty(ui);

			PLSNaverShoppingLIVEProductItemView::addBatchCache(products.count(), true);
			if (asyncProcessAndCheckDone()) {
				return;
			}

			for (const auto &product : products) {
				PLSNaverShoppingLIVEProductItemView *view = PLSNaverShoppingLIVEProductItemView::alloc(
					ui->searchContent, this,
					[this](PLSNaverShoppingLIVEProductItemView *itemView, qint64 productNo) { onProductItemAddRemoveButtonClicked(&searchProductItemViews, itemView, productNo); },
					platform, product);
				view->setSelected(isSelectedProductNo(product.productNo));
				view->setStoreNameVisible(true);
				ui->searchContentLayout->addWidget(view);
				pls_flush_style_recursive(view);
				this->searchProductItemViews.append(view);
				view->show();
			}

			ui->searchContent->adjustSize();

			if (next) {
				PLSLoadNextPage::newLoadNextPage(searchProductNextPage, ui->searchScrollArea, [this, text, page]() { searchSearch(text, page); });
			}
		} else {
			PLSLoadingView::deleteLoadingView(searchProductsLoadingView);
			CHECK_SEARCH_INDEX(searchSearchIndex)

			if (isFirstPage) {
				updateUI_searchResultEmpty(ui, false);
			}
		}
	} else {
		PLSLoadingView::deleteLoadingView(searchProductsLoadingView);
		CHECK_SEARCH_INDEX(searchSearchIndex)

		if (isFirstPage) {
			updateUI_searchResultFailed(ui, false);
		} else {
			PLSLoadNextPage::newLoadNextPage(searchProductNextPage, ui->searchScrollArea, [this, text, currentPage]() { searchSearch(text, currentPage); });
		}
	}
}

void PLSNaverShoppingLIVEProductDialogView::onProductItemAddRemoveButtonClicked(const QList<PLSNaverShoppingLIVEProductItemView *> *itemViews, PLSNaverShoppingLIVEProductItemView *itemView,
										qint64 productNo, bool forceRemove)
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Add Or Remove Product Button", ACTION_CLICK);

	if (isSelectedProductNo(productNo) || forceRemove) {
		removeAtByProductNo(selectedProducts, productNo);
		newRecentProductNos.removeOne(productNo);
		itemView->setSelected(false);
		syncItemViewSelectedState(itemViews, productNo, false);
	} else if (selectedProducts.count() < MAX_SELECTED_PRODUCT_COUNT) {
		auto details = itemView->getDetails();
		if (!details.isMinorPurchasable) {
			pls_alert_error_message(this, tr("Alert.Title"), tr("navershopping.liveinfo.age.restrict.product"), PLSAlertView::Button::Ok, PLSAlertView::Button::Ok);
			return;
		}

		selectedProducts.prepend(details);
		itemView->setSelected(true);
		syncItemViewSelectedState(itemViews, productNo, true);
		newRecentProductNos.removeOne(productNo);
		newRecentProductNos.append(productNo);
	} else {
		itemView->setSelected(false);
		syncItemViewSelectedState(itemViews, productNo, false);
		PLSAlertView::information(this, tr("Alert.Title"), tr("NaverShoppingLive.ProductDialog.AddProduct.Limit"), PLSAlertView::Button::Ok, PLSAlertView::Button::Ok);
	}

	updateCountTip();
}

void PLSNaverShoppingLIVEProductDialogView::onRemoveButtonClicked(const PLSNaverShoppingLIVESearchKeywordItemView *itemView)
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Remove Search Keyword Button", ACTION_CLICK);

	PLSNaverShoppingLIVEDataManager::instance()->removeSearchKeyword(itemView->getSearchKeyword());
	updateSearchKeywords();
}

void PLSNaverShoppingLIVEProductDialogView::onSearchButtonClicked(const PLSNaverShoppingLIVESearchKeywordItemView *itemView)
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Search Keyword", ACTION_CLICK);

	ui->searchSearchBarLineEdit->setText(itemView->getSearchKeyword());
	on_searchSearchBarSearchButton_clicked();
}

void PLSNaverShoppingLIVEProductDialogView::onAddRecentProduct(const PLSNaverShoppingLIVEProductItemView *itemView, qint64)
{
	recentProducts.prepend(itemView->getDetails());

	QList<Product> newRecentProducts;
	for (qint64 productNo : PLSNaverShoppingLIVEDataManager::instance()->getRecentProductNos()) {
		Product product;
		if (findByProductNo(product, recentProducts, productNo)) {
			newRecentProducts.append(product);
		}
	}

	recentProducts = newRecentProducts;
	recentNeedUpdateItems = true;
}

void PLSNaverShoppingLIVEProductDialogView::onLineEditSetFocus()
{
	switch (ui->stackedWidget->currentIndex()) {
	case STORE_TAB:
		ui->storeSearchBarLineEdit->setFocus();
		break;
	case SEARCH_TAB:
		ui->searchSearchBarLineEdit->setFocus();
		break;
	default:
		break;
	}
}

void PLSNaverShoppingLIVEProductDialogView::onFlushLineEditStyle()
{
	QMetaObject::invokeMethod(
		this, [this]() { pls_flush_style(ui->storeSearchBarLineEdit); }, Qt::QueuedConnection);
	QMetaObject::invokeMethod(
		this, [this]() { pls_flush_style(ui->searchSearchBarLineEdit); }, Qt::QueuedConnection);
}

void PLSNaverShoppingLIVEProductDialogView::doSearchTabSearch()
{
	ui->titleWidget->setFocus();

	QString searchKeyword = ui->searchSearchBarLineEdit->text();
	if (!searchKeyword.isEmpty()) {
		updateSearchKeywords(searchKeyword, false);
		searchSearch(searchKeyword);
	}
}

void PLSNaverShoppingLIVEProductDialogView::eventFilter_storeNameWidget(const QEvent *event)
{
	switch (event->type()) {
	case QEvent::MouseButtonRelease:
		if (smartStores.count() > 1) {
			PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Store Name Button", ACTION_CLICK);

			PLSNaverShoppingLIVEStoreNameList::popup(ui->storeNameWidget, this, [this](const QString &storeId, const QString &storeName) {
				PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Store Name Drop List Button", ACTION_CLICK);

				currentSmartStore.storeId = storeId;
				currentSmartStore.storeName = storeName;

				ui->storeNameLabel->setText(storeName);
				PLSNaverShoppingLIVEDataManager::instance()->setLatestStoreId(storeId);
				ui->storeSearchBarLineEdit->setText(QString());
				storeSearch(ui->storeSearchBarLineEdit->text());
			});
		}
		break;
	default:
		break;
	}
}

void PLSNaverShoppingLIVEProductDialogView::clearSearchTabResult()
{
	getSearchIndex(searchSearchIndex);
	ui->searchSearchBarLineEdit->setText(QString());
	PLSLoadingView::deleteLoadingView(searchProductsLoadingView);
	PLSLoadNextPage::deleteLoadNextPage(searchProductNextPage);
	updateUI_searchKeywords(ui, !searchKeywordItemViews.isEmpty());
	ui->searchSearchBarSearchButton->setProperty("inputing", false);
	pls_flush_style(ui->searchSearchBarSearchButton);
	PLSNaverShoppingLIVESearchKeywordItemView::updateSearchKeyword(searchKeywordItemViews);
}

void PLSNaverShoppingLIVEProductDialogView::on_storeSearchBarClearButton_clicked()
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "My Store Search-Bar Clear Button", ACTION_CLICK);

	ui->storeSearchBarLineEdit->setText(QString());
	storeSearch();
}

void PLSNaverShoppingLIVEProductDialogView::on_searchSearchBarClearButton_clicked()
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Search Search-Bar Clear Button", ACTION_CLICK);
	clearSearchTabResult();
}

void PLSNaverShoppingLIVEProductDialogView::on_storeSearchBarSearchButton_clicked()
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "My Store Search-Bar Search Button", ACTION_CLICK);

	ui->titleWidget->setFocus();

	storeSearch(ui->storeSearchBarLineEdit->text());
}

void PLSNaverShoppingLIVEProductDialogView::on_storeSearchBarLineEdit_textChanged(const QString &text)
{
	ui->storeSearchBarClearButton->setVisible(!text.isEmpty());
	ui->storeSearchBarSearchButton->setProperty("inputing", !text.isEmpty() && ui->searchSearchBarLineEdit->hasFocus());
	pls_flush_style(ui->storeSearchBarSearchButton);
	storeSearch(text);
}

void PLSNaverShoppingLIVEProductDialogView::on_searchSearchBarSearchButton_clicked()
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Search Search-Bar Search Button", ACTION_CLICK);
	doSearchTabSearch();
}

void PLSNaverShoppingLIVEProductDialogView::on_searchSearchBarLineEdit_textChanged(const QString &text)
{
	ui->searchSearchBarClearButton->setVisible(!text.isEmpty());
	ui->searchSearchBarSearchButton->setProperty("inputing", !text.isEmpty() && ui->searchSearchBarLineEdit->hasFocus());
	pls_flush_style(ui->searchSearchBarSearchButton);

	if (text.isEmpty()) {
		clearSearchTabResult();
	}
}

void PLSNaverShoppingLIVEProductDialogView::on_searchSearchBarLineEdit_returnPressed()
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "My Store Search-Bar Input Edit Enter-Key", ACTION_CLICK);
	doSearchTabSearch();
}

void PLSNaverShoppingLIVEProductDialogView::on_okButton_clicked()
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Product Manager Ok Button", ACTION_CLICK);

	QList<Product> rhythmicityProducts;
	QList<PLSNaverShoppingLIVEProductItemView *> categoryProductItemViews;

	//Check for self-disciplined products
	for (int i = 0; i < selectedProducts.count(); i++) {
		const Product &product = selectedProducts[i];

		// check is my store product, use store id to check
		if (!isMyStoreProduct(product)) {
			continue;
		}

		if (!PLSNaverShoppingLIVEAPI::isRhythmicityProduct(product.wholeCategoryId, product.wholeCategoryName)) {
			continue;
		}

		rhythmicityProducts.append(product);

		PLSNaverShoppingLIVEProductItemView *itemView = nullptr;
		if (findByProductNo(itemView, storeProductItemViews, [&product](const PLSNaverShoppingLIVEProductItemView *iv) { return iv->getProductNo() == product.productNo; })) {
			categoryProductItemViews.append(itemView);
		}
		if (findByProductNo(itemView, recentProductItemViews, [&product](const PLSNaverShoppingLIVEProductItemView *iv) { return iv->getProductNo() == product.productNo; })) {
			categoryProductItemViews.append(itemView);
		}
		if (findByProductNo(itemView, searchProductItemViews, [&product](const PLSNaverShoppingLIVEProductItemView *iv) { return iv->getProductNo() == product.productNo; })) {
			categoryProductItemViews.append(itemView);
		}
	}

	if (!rhythmicityProducts.isEmpty()) {
		PLSShoppingRhythmicity view(this);
		view.setURL(PLSSyncServerManager::instance()->getVoluntaryReviewProducts());
		if (view.exec() != QDialog::Accepted) {
			for (auto itemView : categoryProductItemViews) {
				removeAtByProductNo(selectedProducts, itemView->getProductNo());
				itemView->setSelected(false);
			}

			for (const Product &product : rhythmicityProducts) {
				removeAtByProductNo(selectedProducts, product.productNo);
			}
			updateCountTip();
		}
	}

	if (isLiving && selectedProducts.isEmpty()) {
		pls_alert_error_message(this, tr("Alert.Title"), tr("NaverShoppingLive.LiveInfo.Product.AtLeastOne"), PLSAlertView::Button::Ok, PLSAlertView::Button::Ok);
		return;
	}
	accept();
}

void PLSNaverShoppingLIVEProductDialogView::on_cancelButton_clicked()
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Product Manager Cancel Button", ACTION_CLICK);

	reject();
}

bool PLSNaverShoppingLIVEProductDialogView::event(QEvent *event)
{
	bool result = PLSDialogView::event(event);
	switch (event->type()) {
	case QEvent::WindowActivate:
		flushLineEditStyle();
		break;
	default:
		break;
	}
	return result;
}

bool PLSNaverShoppingLIVEProductDialogView::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->storeSearchBarLineEdit || watched == ui->searchSearchBarLineEdit) {
		switch (event->type()) {
		case QEvent::FocusIn:
			pls_flush_style_recursive(watched == ui->storeSearchBarLineEdit ? ui->storeSearchBar : ui->searchSearchBar, "hasFocus", true);
			break;
		case QEvent::FocusOut:
			pls_flush_style_recursive(watched == ui->storeSearchBarLineEdit ? ui->storeSearchBar : ui->searchSearchBar, "hasFocus", false);
			break;
		default:
			break;
		}
	} else if (watched == ui->storeWidget) {
		switch (event->type()) {
		case QEvent::Resize:
			ui->storeNameLabel->setText(getCurrentSmartStoreName());
			break;
		default:
			break;
		}
	} else if (watched == ui->storeNameWidget) {
		eventFilter_storeNameWidget(event);
	} else if (watched == ui->storeNameWidgetSpacer) {
		switch (event->type()) {
		case QEvent::Resize:
			if (static_cast<QResizeEvent *>(event)->size().width() == 0) {
				int width = ui->storeNameWidgetContainer->width() - ui->storeNameIconLabel->width() - ui->horizontalLayout_9->spacing();
				QString text = ui->storeNameLabel->fontMetrics().elidedText(getCurrentSmartStoreName(), Qt::ElideRight, width);
				ui->storeNameLabel->setText(text);
			}
			break;
		default:
			break;
		}
	} else if (QLayout *contentLayout = vscrollBarToLayout(watched); contentLayout) {
		switch (event->type()) {
		case QEvent::Show:
			processScrollContentMarginRight(this, static_cast<QScrollBar *>(watched), contentLayout, true, watched != searchKeywordVScrollBar);
			break;
		case QEvent::Hide:
			processScrollContentMarginRight(this, static_cast<QScrollBar *>(watched), contentLayout, false, watched != searchKeywordVScrollBar);
			break;
		default:
			break;
		}
	}

	return PLSDialogView::eventFilter(watched, event);
}

void PLSNaverShoppingLIVEProductDialogView::done(int result)
{
	doneFlag = true;
	PLSDialogView::done(result);
}

void PLSNaverShoppingLIVEProductDialogView::on_recentNoNetRetryButton_clicked()
{
	QString usingType = ui->recentNoNet->property("usingType").toString();
	if (usingType == "GetRecentApiFailed") {
		PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Recent Api Failed Retry Button", ACTION_CLICK);
	} else if (usingType == "NoNet") {
		PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Recent No Network Retry Button", ACTION_CLICK);
	} else {
		PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Recent Retry Button", ACTION_CLICK);
	}

	initRecentProducts();
}

void PLSNaverShoppingLIVEProductDialogView::on_storeChangeStoreButton_clicked()
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Change Store Button", ACTION_CLICK);

	if (PLSAlertView::question(this, tr("Alert.Title"), tr("NaverShoppingLive.ProductDialog.Store.ChangeStoreAlert"), PLSAlertView::Button::Ok | PLSAlertView::Button::Cancel,
				   PLSAlertView::Button::Ok) != PLSAlertView::Button::Ok) {
		return;
	}

	smartStores.clear();

	recentProducts.clear();
	recentNeedUpdateItems = true;

	auto manager = PLSNaverShoppingLIVEDataManager::instance();
	manager->clearSmartStoreInfo();

	emit smartStoreChanged();

	pls_login_async(
		[this, manager](bool ok, const QJsonObject &result) {
			if (ok) {
				manager->setSmartStoreAccessToken(platform, result[ChannelData::g_channelToken].toString());
			} else {
				smartStores.clear();
				updateSmartStores(!ok);
			}
		},
		platform->getChannelName(), this, PLSLoginInfo::UseFor::Store);
}

void PLSNaverShoppingLIVEProductDialogView::on_noSmartStoreButton_clicked()
{
	QString usingType = ui->noSmartStoreButton->property("usingType").toString();
	if (usingType == "NoStore") {
		PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "No Store Retry Button", ACTION_CLICK);

		pls_login_async(
			[this](bool ok, const QJsonObject &result) {
				if (ok) {
					PLSNaverShoppingLIVEDataManager::instance()->setSmartStoreAccessToken(platform, result[ChannelData::g_channelToken].toString());
				}
			},
			platform->getChannelName(), this, PLSLoginInfo::UseFor::Store);
	} else if (usingType == "GetStoreApiFailed") {
		PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Get Store Api Failed Retry Button", ACTION_CLICK);

		searchSmartStores();
	}
}

void PLSNaverShoppingLIVEProductDialogView::on_storeNoNetButton_clicked()
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Store No Network Retry Button", ACTION_CLICK);

	if (pls_get_network_state()) {
		searchSmartStores();
	}
}

void PLSNaverShoppingLIVEProductDialogView::on_storeSearchNoNetButton_clicked()
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Store Search No Network Retry Button", ACTION_CLICK);

	if (pls_get_network_state()) {
		storeSearch(ui->storeSearchBarLineEdit->text());
	}
}

void PLSNaverShoppingLIVEProductDialogView::on_searchNoNetButton_clicked()
{
	QString usingType = ui->searchNoNetButton->property("usingType").toString();
	if (usingType == "NoNet") {
		PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Search No Network Retry Button", ACTION_CLICK);
	} else if (usingType == "SearchApiFailed") {
		PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Search Api Failed Retry Button", ACTION_CLICK);
	}

	if (pls_get_network_state()) {
		if (!ui->searchSearchBarLineEdit->text().isEmpty()) {
			on_searchSearchBarSearchButton_clicked();
		} else {
			on_searchSearchBarClearButton_clicked();
		}
	}
}

void PLSNaverShoppingLIVEProductDialogView::on_storeApiFailedRetryButton_clicked()
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Store Search Api Failed Retry Button", ACTION_CLICK);

	storeSearch(ui->storeSearchBarLineEdit->text());
}
