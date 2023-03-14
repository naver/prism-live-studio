#include "PLSNetworkMonitor.h"
#include "PLSLiveInfoNaverShoppingLIVEProductList.h"
#include "ui_PLSLiveInfoNaverShoppingLIVEProductList.h"
#include "PLSLiveInfoNaverShoppingLIVEProductItemView.h"
#include "PLSNaverShoppingLIVEProductDialogView.h"
#include "PLSPlatformNaverShoppingLIVE.h"
#include "PLSLiveInfoNaverShoppingLIVE.h"
#include "PLSNaverShoppingLIVEDataManager.h"
#include "PLSLoadingView.h"

#include "alert-view.hpp"
#include "pls-common-define.hpp"

#include <QPainter>
#include <QDesktopServices>
#include <chrono>

static QList<QObject *> liveInfoNaverShoppingLIVEProductList;

static bool isEqual(const QList<qint64> &a, const QList<qint64> &b)
{
	if (a.count() != b.count()) {
		return false;
	}

	for (int i = 0, count = a.count(); i < count; ++i) {
		if (a[i] != b[i]) {
			return false;
		}
	}
	return true;
}

static QList<qint64> toProductNos(const QList<PLSLiveInfoNaverShoppingLIVEProductList::Product> &products)
{
	QList<qint64> productNos;
	for (const auto &product : products)
		productNos.append(product.productNo);
	return productNos;
}

static bool removeByProductNo(QList<PLSLiveInfoNaverShoppingLIVEProductList::Product> &products, qint64 productNo)
{
	for (int i = 0, count = products.count(); i < count; ++i) {
		if (products[i].productNo == productNo) {
			products.removeAt(i);
			return true;
		}
	}
	return false;
}

PLSLiveInfoNaverShoppingLIVEProductCountBadge::PLSLiveInfoNaverShoppingLIVEProductCountBadge(QWidget *parent) : QWidget(parent) {}

PLSLiveInfoNaverShoppingLIVEProductCountBadge::~PLSLiveInfoNaverShoppingLIVEProductCountBadge() {}

void PLSLiveInfoNaverShoppingLIVEProductCountBadge::setProductCount(int productCount, bool ok)
{
	this->productCount = QString::number(productCount);
	setProperty("textLength", this->productCount.length());
	pls_flush_style(this);
	setVisible(ok && (productCount > 0) && PLSNetworkMonitor::Instance()->IsInternetAvailable());
	update();
}

void PLSLiveInfoNaverShoppingLIVEProductCountBadge::paintEvent(QPaintEvent * /*event*/)
{
	QPainter painter(this);

	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	QRect rect = this->rect();
	painter.fillRect(rect, Qt::transparent);

	QPainterPath path;
	path.addRoundedRect(rect, rect.height() / 2, rect.height() / 2);
	painter.setClipPath(path);

	painter.fillRect(rect, QColor(239, 252, 53));
	painter.drawText(rect.adjusted(0, PLSDpiHelper::calculate(this, -2), 0, 0), productCount, QTextOption(Qt::AlignCenter));
}

PLSLiveInfoNaverShoppingLIVEProductList::PLSLiveInfoNaverShoppingLIVEProductList(QWidget *parent) : QWidget(parent), ui(new Ui::PLSLiveInfoNaverShoppingLIVEProductList)
{
	liveInfoNaverShoppingLIVEProductList.append(this);

	ui->setupUi(this);
	updateFixProductTip();
	showPage(false);

	auto lang = pls_get_current_language();
	ui->ppAddButton->setProperty("lang", lang);
	ui->nppAddButton->setProperty("lang", lang);
	ui->noNetRetryButton->setProperty("lang", lang);
	ui->placeholder->hide();

	titleWidget = new QWidget(this);
	titleWidget->setObjectName("titleWidget");

	QHBoxLayout *layout = new QHBoxLayout(titleWidget);
	layout->setMargin(0);
	layout->setSpacing(8);

	QHBoxLayout *layout1 = new QHBoxLayout();
	layout1->setContentsMargins(0, 0, 0, 1);
	layout1->setSpacing(0);

	QHBoxLayout *layout2 = new QHBoxLayout();
	layout2->setContentsMargins(0, 2, 0, 0);
	layout2->setSpacing(0);

	layout->addLayout(layout1);
	layout->addLayout(layout2);
	layout->addStretch(1);

	QLabel *titleLabel = new QLabel(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("navershopping.product.title")), this);
	titleLabel->setObjectName("titleLabel");
	titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
	layout1->addWidget(titleLabel);

	productCountBadge = new PLSLiveInfoNaverShoppingLIVEProductCountBadge(this);
	productCountBadge->setObjectName("productCountBadge");
	productCountBadge->hide();
	layout2->addWidget(productCountBadge);

	ui->hline2->installEventFilter(this);
}

PLSLiveInfoNaverShoppingLIVEProductList::~PLSLiveInfoNaverShoppingLIVEProductList()
{
	liveInfoNaverShoppingLIVEProductList.removeOne(this);
	PLSLiveInfoNaverShoppingLIVEProductItemView::dealloc(itemViews, ui->productsLayout);
	PLSLoadingView::deleteLoadingView(productLoadingView);
	delete ui;

	if (productDialogView) {
		delete productDialogView;
		productDialogView = nullptr;
	}
}

PLSPlatformNaverShoppingLIVE *PLSLiveInfoNaverShoppingLIVEProductList::getPlatform()
{
	for (QWidget *parent = parentWidget(); parent; parent = parent->parentWidget()) {
		if (PLSLiveInfoNaverShoppingLIVE *liveInfo = dynamic_cast<PLSLiveInfoNaverShoppingLIVE *>(parent); liveInfo) {
			return liveInfo->getPlatform();
		}
	}
	return nullptr;
}

int PLSLiveInfoNaverShoppingLIVEProductList::titleLabelY() const
{
	return titleWidget->y();
}

void PLSLiveInfoNaverShoppingLIVEProductList::setTitleLabelY(int titleLabelY)
{
	titleWidget->move(0, titleLabelY);
}

QList<qint64> PLSLiveInfoNaverShoppingLIVEProductList::getFixedProductNos() const
{
	return fixedProductNos;
}

QList<qint64> PLSLiveInfoNaverShoppingLIVEProductList::getUnfixedProductNos() const
{
	return unfixedProductNos;
}

QList<qint64> PLSLiveInfoNaverShoppingLIVEProductList::getAllProductNos() const
{
	QList<qint64> allProductNos;
	for (auto productNo : this->fixedProductNos)
		allProductNos.append(productNo);
	for (auto productNo : this->unfixedProductNos)
		allProductNos.append(productNo);
	return allProductNos;
}

QList<PLSLiveInfoNaverShoppingLIVEProductList::Product> PLSLiveInfoNaverShoppingLIVEProductList::getFixedProducts() const
{
	QList<Product> fixedProducts;
	for (auto product : this->fixedProducts) {
		product.represent = true;
		fixedProducts.append(product);
	}
	return fixedProducts;
}

QList<PLSLiveInfoNaverShoppingLIVEProductList::Product> PLSLiveInfoNaverShoppingLIVEProductList::getUnfixedProducts() const
{
	QList<Product> unfixedProducts;
	for (auto product : this->unfixedProducts) {
		product.represent = false;
		unfixedProducts.append(product);
	}
	return unfixedProducts;
}

QList<PLSLiveInfoNaverShoppingLIVEProductList::Product> PLSLiveInfoNaverShoppingLIVEProductList::getAllProducts() const
{
	QList<Product> allProducts;
	for (auto product : this->fixedProducts) {
		product.represent = true;
		allProducts.append(product);
	}
	for (auto product : this->unfixedProducts) {
		product.represent = false;
		allProducts.append(product);
	}
	return allProducts;
}

int PLSLiveInfoNaverShoppingLIVEProductList::getProductNoCount() const
{
	return fixedProductNos.count() + unfixedProductNos.count();
}

int PLSLiveInfoNaverShoppingLIVEProductList::getProductCount() const
{
	return fixedProducts.count() + unfixedProducts.count();
}

void PLSLiveInfoNaverShoppingLIVEProductList::setOwnerScrollArea(QScrollArea *ownerScrollArea)
{
	this->ownerScrollArea = ownerScrollArea;
}

void PLSLiveInfoNaverShoppingLIVEProductList::setProductNos(const QList<qint64> &fixedProductNos, const QList<qint64> &unfixedProductNos, std::function<void()> &&updateFinished)
{
	setIsScheduleLiveLoading(false);

	setProductNosUpdateFinished = std::move(updateFinished);

	if (isEqual(this->fixedProductNos, fixedProductNos) && isEqual(this->unfixedProductNos, unfixedProductNos)) {
		productUpdateFinished();
		return;
	}

	this->fixedProductNos = fixedProductNos;
	this->unfixedProductNos = unfixedProductNos;
	this->fixedProducts.clear();
	this->unfixedProducts.clear();
	PLSLiveInfoNaverShoppingLIVEProductItemView::dealloc(itemViews, ui->productsLayout);

	updateProductCountBadge();
	updateFixProductTip();
	updateAllProductsInfo();
}

void PLSLiveInfoNaverShoppingLIVEProductList::setAllProducts(const QList<Product> &products, bool update)
{
	QList<Product> fixedProducts;
	QList<Product> unfixedProducts;
	for (const auto &product : products) {
		if (product.represent) {
			fixedProducts.append(product);
		} else {
			unfixedProducts.append(product);
		}
	}

	setProducts(fixedProducts, unfixedProducts, update);
}

void PLSLiveInfoNaverShoppingLIVEProductList::setProducts(QList<Product> fixedProducts, QList<Product> unfixedProducts, bool update)
{
	if (update) {
		setProductNos(toProductNos(fixedProducts), toProductNos(unfixedProducts));
		return;
	}

	setIsScheduleLiveLoading(false);

	setProductNosUpdateFinished = nullptr;

	this->fixedProductNos = toProductNos(fixedProducts);
	this->unfixedProductNos = toProductNos(unfixedProducts);
	this->fixedProducts.clear();
	this->unfixedProducts.clear();

	if (!fixedProducts.isEmpty() || !unfixedProducts.isEmpty()) {
		showPage(true);
		updateProductsInfo(fixedProducts, unfixedProducts);
		if (checkAgeLimitProducts(fixedProducts, unfixedProducts)) {
			if (!fixedProducts.isEmpty() || !unfixedProducts.isEmpty()) {
				updateProductsInfo(fixedProducts, unfixedProducts);
			} else {
				clearAll();
			}
		}
	} else {
		PLSLiveInfoNaverShoppingLIVEProductItemView::dealloc(itemViews, ui->productsLayout);
		showPage(false);
		updateProductCountBadge();
		updateFixProductTip();
	}

	setAddButtonEnabled(true);
}

void PLSLiveInfoNaverShoppingLIVEProductList::setProductReadonly(bool readonly)
{
	productReadonly = readonly;

	ui->ppAddButton->setEnabled(!readonly && (getProductNoCount() < PLSNaverShoppingLIVEDataManager::MAX_LIVEINFO_PRODUCT_COUNT));
	ui->nppAddButton->setEnabled(!readonly && (getProductNoCount() < PLSNaverShoppingLIVEDataManager::MAX_LIVEINFO_PRODUCT_COUNT));

	ui->guideWidget->setVisible(!readonly);
	ui->hline2->setVisible(!readonly);

	checkProductEmpty();

	for (auto itemView : itemViews) {
		itemView->setProductReadonly(readonly);
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::setIsLiving(bool isLiving)
{
	this->isLiving = isLiving;
}

void PLSLiveInfoNaverShoppingLIVEProductList::setIsScheduleLive(bool isScheduleLive)
{
	this->isScheduleLive = isScheduleLive;
	checkProductEmpty();
}

void PLSLiveInfoNaverShoppingLIVEProductList::setIsScheduleLiveLoading(bool isScheduleLiveLoading)
{
	this->isScheduleLiveLoading = isScheduleLiveLoading;
	checkProductEmpty();
}

void PLSLiveInfoNaverShoppingLIVEProductList::expiredClose()
{
	if (productDialogView) {
		productDialogView->reject();
		delete productDialogView;
		productDialogView = nullptr;
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::resetIconSize()
{
	PLSLiveInfoNaverShoppingLIVEProductItemView::resetIconSize(itemViews);
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateWhenDpiChanged()
{
	PLSLiveInfoNaverShoppingLIVEProductItemView::updateWhenDpiChanged(itemViews);
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateAllProductsInfo()
{
	PLSLiveInfoNaverShoppingLIVEProductItemView::dealloc(itemViews, ui->productsLayout);

	if (!fixedProductNos.isEmpty() || !unfixedProductNos.isEmpty()) {
		showPage(true);
		setAddButtonEnabled(false);
		ui->placeholder->show();

		PLSLoadingView::newLoadingView(productLoadingView, !setProductNosUpdateFinished, this, productReadonly ? 96 : 65,
					       [this](QRect &geometry, PLSLoadingView *loadingView) { return getProductLoadingViewport(geometry, loadingView); });
		titleWidget->raise();

		PLSNaverShoppingLIVEAPI::productSearchByProductNos(
			getPlatform(), fixedProductNos, unfixedProductNos,
			[this, start = std::chrono::steady_clock::now()](bool ok, QList<PLSNaverShoppingLIVEAPI::ProductInfo> fixedProducts,
									 QList<PLSNaverShoppingLIVEAPI::ProductInfo> unfixedProducts) {
				if (ok) {
					PLSLoadingView::deleteLoadingView(productLoadingView);
					if (!fixedProducts.isEmpty() || !unfixedProducts.isEmpty()) {
						showPage(true);
						updateProductsInfo(fixedProducts, unfixedProducts);
						if (checkAgeLimitProducts(fixedProducts, unfixedProducts)) {
							if (!fixedProducts.isEmpty() || !unfixedProducts.isEmpty()) {
								updateProductsInfo(fixedProducts, unfixedProducts);
							} else {
								clearAll();
							}
						}
					} else {
						showPage(false);
					}
					setAddButtonEnabled(true);
				} else if ((std::chrono::steady_clock::now() - start) > 300ms) {
					PLSLoadingView::deleteLoadingView(productLoadingView);
					showPage(false, true, true);
					updateProductCountBadge(false);
					updateFixProductTip();
					setAddButtonEnabled(true);
				} else {
					updateProductCountBadge(false);
					updateFixProductTip();
					QTimer::singleShot(300, this, [this]() {
						PLSLoadingView::deleteLoadingView(productLoadingView);
						showPage(false, true, true);
						setAddButtonEnabled(true);
					});
				}

				emit productChangedOrUpdated(false);
				productUpdateFinished();
			},
			this, [](QObject *object) { return liveInfoNaverShoppingLIVEProductList.contains(object); });
	} else {
		showPage(false);
		setAddButtonEnabled(true);
		productUpdateFinished();
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateProductsInfo(const QList<Product> &fixedProducts, const QList<Product> &unfixedProducts)
{
	int fixedProductCount = fixedProducts.count();
	int unfixedProductCount = unfixedProducts.count();
	int productCount = fixedProductCount + unfixedProductCount;

	this->fixedProducts = fixedProducts;
	this->unfixedProducts = unfixedProducts;
	updateProductCountBadge();
	updateFixProductTip();

	allowScrollAreaShowVerticalScrollBar();
	PLSLiveInfoNaverShoppingLIVEProductItemView::addBatchCache(productCount, true);

	if (productCount <= itemViews.count()) {
		for (int i = 0, j = 0; i < fixedProductCount; ++i, ++j) {
			auto view = itemViews[j];
			const auto &product = fixedProducts[i];
			view->setInfo(getPlatform(), product, true);
			view->setProductReadonly(productReadonly);
		}

		for (int i = 0, j = fixedProductCount; i < unfixedProductCount; ++i, ++j) {
			auto view = itemViews[j];
			const auto &product = unfixedProducts[i];
			view->setInfo(getPlatform(), product, false);
			view->setProductReadonly(productReadonly);
		}

		if (itemViews.count() > productCount) {
			while (itemViews.count() > productCount) {
				auto view = itemViews.takeLast();
				ui->productsLayout->removeWidget(view);
				PLSLiveInfoNaverShoppingLIVEProductItemView::dealloc(view);
			}

			ui->verticalLayout_2->invalidate();
		}
	} else {
		for (int i = 0, count = itemViews.count(); i < count; ++i) {
			auto view = itemViews[i];
			const auto &product = (i < fixedProductCount) ? fixedProducts[i] : unfixedProducts[i - fixedProductCount];
			view->setInfo(getPlatform(), product, this->fixedProductNos.contains(product.productNo));
			view->setProductReadonly(productReadonly);
		}

		if (productCount > itemViews.count()) {
			for (int i = itemViews.count(); i < productCount; ++i) {
				const auto &product = (i < fixedProductCount) ? fixedProducts[i] : unfixedProducts[i - fixedProductCount];
				auto view = PLSLiveInfoNaverShoppingLIVEProductItemView::alloc(ui->products, this, &PLSLiveInfoNaverShoppingLIVEProductList::onFixButtonClicked,
											       &PLSLiveInfoNaverShoppingLIVEProductList::onRemoveButtonClicked, getPlatform(), product,
											       this->fixedProductNos.contains(product.productNo));
				view->setProductReadonly(productReadonly);
				ui->productsLayout->addWidget(view);
				pls_flush_style_recursive(view);
				PLSDpiHelper::updateLayout(view);
				this->itemViews.append(view);
				view->show();
			}

			ui->verticalLayout_2->invalidate();
		}
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateFixProductTip(bool ok)
{
	ui->fixGuideText->setText(tr("NaverShoppingLive.LiveInfo.Product.FixProductTip").arg(ok ? fixedProducts.count() : 0));
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateProductCountBadge(bool ok)
{
	productCountBadge->setProductCount(getProductCount(), ok);
}

void PLSLiveInfoNaverShoppingLIVEProductList::setAddButtonEnabled(bool enabled)
{
	bool addButtonEnabled = enabled && !productReadonly && (getProductNoCount() < PLSNaverShoppingLIVEDataManager::MAX_LIVEINFO_PRODUCT_COUNT);

	ui->ppAddButton->setEnabled(addButtonEnabled);
	ui->nppAddButton->setEnabled(addButtonEnabled);
}

void PLSLiveInfoNaverShoppingLIVEProductList::checkProductEmpty()
{
	if (fixedProductNos.isEmpty() && unfixedProductNos.isEmpty()) {
		showPage(false);
	}
}

bool PLSLiveInfoNaverShoppingLIVEProductList::checkAgeLimitProducts(QList<Product> &fixedProducts, QList<Product> &unfixedProducts)
{
	QList<Product> ageLimitProducts;
	for (const auto &product : fixedProducts) {
		if (!product.isMinorPurchasable) {
			ageLimitProducts.append(product);
		}
	}
	for (const auto &product : unfixedProducts) {
		if (!product.isMinorPurchasable) {
			ageLimitProducts.append(product);
		}
	}

	if (ageLimitProducts.isEmpty()) {
		return false;
	} else if (PLSAlertView::question(pls_get_toplevel_view(this), tr("Alert.Title"), tr("NaverShoppingLive.LiveInfo.RemoveAgeLimitProducts"),
					  {{PLSAlertView::Button::Close, tr("Close")}, {PLSAlertView::Button::Apply, tr("Delete")}}) != PLSAlertView::Button::Apply) {
		return false;
	}

	for (const auto &product : ageLimitProducts) {
		fixedProductNos.removeOne(product.productNo);
		unfixedProductNos.removeOne(product.productNo);
		removeByProductNo(fixedProducts, product.productNo);
		removeByProductNo(unfixedProducts, product.productNo);
	}
	return true;
}

void PLSLiveInfoNaverShoppingLIVEProductList::clearAll()
{
	PLSLiveInfoNaverShoppingLIVEProductItemView::dealloc(itemViews, ui->productsLayout);
	this->fixedProductNos.clear();
	this->unfixedProductNos.clear();
	this->fixedProducts.clear();
	this->unfixedProducts.clear();
	showPage(false);
	setAddButtonEnabled(true);
	updateFixProductTip();
	updateProductCountBadge();
}

void PLSLiveInfoNaverShoppingLIVEProductList::onFixButtonClicked(PLSLiveInfoNaverShoppingLIVEProductItemView *itemView)
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_LIVEINFO, "Fix Product Button", ACTION_CLICK);

	qint64 productNo = itemView->getProductNo();
	if (!itemView->isFixed()) {
		if (fixedProductNos.count() >= PLSNaverShoppingLIVEDataManager::MAX_FIXED_PRODUCT_COUNT) {
			PLSAlertView::information(this, tr("Alert.Title"), tr("NaverShoppingLive.LiveInfo.FixProduct.Limit"), PLSAlertView::Button::Ok, PLSAlertView::Button::Ok);
		} else {
			doProductFixed(itemView, true);
		}
	} else if (fixedProductNos.contains(productNo)) {
		doProductFixed(itemView, false);
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::onRemoveButtonClicked(PLSLiveInfoNaverShoppingLIVEProductItemView *itemView)
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_LIVEINFO, "Remove Product Button", ACTION_CLICK);

	if (isLiving && (getProductNoCount() == PLSNaverShoppingLIVEDataManager::MIN_LIVEINFO_PRODUCT_COUNT)) {
		PLSAlertView::information(this, tr("Alert.Title"), tr("NaverShoppingLive.LiveInfo.Product.AtLeastOne"), PLSAlertView::Button::Ok, PLSAlertView::Button::Ok);
	} else {
		doProductRemoved(itemView);
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::onSmartStoreChanged()
{
	if (!fixedProductNos.isEmpty() || !unfixedProductNos.isEmpty()) {
		clearAll();
		PLSLoadingView::newLoadingView(productLoadingView, this, 65, [this](QRect &geometry, PLSLoadingView *loadingView) { return getProductLoadingViewport(geometry, loadingView); });
		titleWidget->raise();
		setAddButtonEnabled(false);

		emit productChangedOrUpdated(true);
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::doProductFixed(PLSLiveInfoNaverShoppingLIVEProductItemView *itemView, bool isFixed)
{
	qint64 productNo = itemView->getProductNo();
	if (isFixed) {
		if (!fixedProductNos.contains(productNo)) {
			fixedProductNos.prepend(productNo);
			fixedProducts.prepend(itemView->getProduct());
			unfixedProductNos.removeOne(productNo);
			removeByProductNo(unfixedProducts, productNo);
		} else if (fixedProductNos.first() != productNo) {
			fixedProductNos.removeOne(productNo);
			removeByProductNo(fixedProducts, productNo);
			fixedProductNos.prepend(productNo);
			fixedProducts.prepend(itemView->getProduct());
		} else {
			return;
		}

		updateFixProductTip();
		itemView->setFixed(true);
		ui->productsLayout->removeWidget(itemView);
		ui->productsLayout->insertWidget(0, itemView);
		itemViews.removeOne(itemView);
		itemViews.insert(0, itemView);
	} else {
		unfixedProductNos.prepend(productNo);
		unfixedProducts.prepend(itemView->getProduct());
		fixedProductNos.removeOne(productNo);
		removeByProductNo(fixedProducts, productNo);
		updateFixProductTip();

		itemView->setFixed(false);
		ui->productsLayout->removeWidget(itemView);
		ui->productsLayout->insertWidget(fixedProductNos.count(), itemView);
		itemViews.removeOne(itemView);
		itemViews.insert(fixedProductNos.count(), itemView);
	}

	emit productChangedOrUpdated(true);
}

void PLSLiveInfoNaverShoppingLIVEProductList::doProductRemoved(PLSLiveInfoNaverShoppingLIVEProductItemView *itemView)
{
	qint64 productNo = itemView->getProductNo();
	fixedProductNos.removeOne(productNo);
	unfixedProductNos.removeOne(productNo);
	removeByProductNo(fixedProducts, productNo);
	removeByProductNo(unfixedProducts, productNo);

	updateProductsInfo(fixedProducts, unfixedProducts);
	setAddButtonEnabled(true);

	if (getProductNoCount() == 0) {
		showPage(false);
	}

	emit productChangedOrUpdated(true);
}

bool PLSLiveInfoNaverShoppingLIVEProductList::getProductLoadingViewport(QRect &geometry, PLSLoadingView * /*loadingView*/) const
{
	if (!ownerScrollArea) {
		return false;
	}

	QPoint pos{0, mapFromGlobal(ui->hline2->mapToGlobal(QPoint{0, ui->hline2->height()})).y()};
	QWidget *viewport = ownerScrollArea->viewport();
	QRect viewportRect{viewport->mapToGlobal(QPoint{0, 0}), viewport->size()};
	QRect parentRect{this->mapToGlobal(pos), this->size()};
	geometry = viewportRect & parentRect;
	geometry.moveTopLeft(pos);
	return true;
}

void PLSLiveInfoNaverShoppingLIVEProductList::showPage(bool hasProducts, bool showNoNet, bool apiFailed, bool showProductList)
{
	ui->placeholder->hide();
	if (showNoNet || !PLSNetworkMonitor::Instance()->IsInternetAvailable()) {
		ui->noNetTipLabel->setText(apiFailed && PLSNetworkMonitor::Instance()->IsInternetAvailable() ? QObject::tr("NaverShoppingLive.LiveInfo.ProductInfoUpdateApiFailedTip")
													     : QObject::tr("NaverShoppingLive.LiveInfo.Product.NoNetTip"));
		ui->noProductPage->hide();
		ui->scheNoProductWidget->hide();
		ui->productPage->hide();
		ui->noNetPage->show();
	} else if (!hasProducts) {
		ui->productPage->hide();
		ui->noNetPage->hide();
		if (!isScheduleLive && !isScheduleLiveLoading) {
			ui->scheNoProductWidget->hide();
			ui->noProductPage->show();
		} else {
			ui->noProductPage->hide();
			ui->scheNoProductWidget->setVisible(!isScheduleLiveLoading);
		}
	} else {
		ui->noProductPage->hide();
		ui->scheNoProductWidget->hide();
		ui->noNetPage->hide();
		ui->products->setVisible(showProductList);
		ui->productPage->show();
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::productUpdateFinished()
{
	if (setProductNosUpdateFinished) {
		setProductNosUpdateFinished();
		setProductNosUpdateFinished = nullptr;
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::allowScrollAreaShowVerticalScrollBar()
{
	if (ownerScrollArea) {
		ownerScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::on_nppAddButton_clicked()
{
	on_ppAddButton_clicked();
}

void PLSLiveInfoNaverShoppingLIVEProductList::on_ppAddButton_clicked()
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_LIVEINFO, "Add Product Button", ACTION_CLICK);

	if (productDialogView) {
		delete productDialogView;
		productDialogView = nullptr;
	}

	productDialogView = new PLSNaverShoppingLIVEProductDialogView(getPlatform(), getAllProducts(), isLiving, pls_get_toplevel_view(this));
	connect(productDialogView, &PLSNaverShoppingLIVEProductDialogView::smartStoreChanged, this, &PLSLiveInfoNaverShoppingLIVEProductList::onSmartStoreChanged, Qt::DirectConnection);
	if (productDialogView->exec() != PLSNaverShoppingLIVEProductDialogView::Accepted) {
		return;
	}

	QList<qint64> selectedProductNos = productDialogView->getSelectedProductNos();
	qSort(selectedProductNos);

	QList<qint64> currentProductNos = getAllProductNos();
	qSort(currentProductNos);

	if (isEqual(currentProductNos, selectedProductNos)) {
		return;
	}

	QList<Product> fixedProducts;
	QList<Product> unfixedProducts;
	for (const auto &selectedProduct : productDialogView->getSelectedProducts()) {
		if (this->fixedProductNos.contains(selectedProduct.productNo)) {
			fixedProducts.append(selectedProduct);
		} else {
			unfixedProducts.append(selectedProduct);
		}
	}

	if ((getProductCount() <= 0) && fixedProducts.isEmpty() && !unfixedProducts.isEmpty()) {
		fixedProducts.append(unfixedProducts.takeLast());
	}

	if (!fixedProducts.isEmpty() || !unfixedProducts.isEmpty()) {
		showPage(true, false, false, false);
		setAddButtonEnabled(false);

		showPage(true);
		PLSNaverShoppingLIVEDataManager::instance()->addRecentProductNos(productDialogView->getNewRecentProductNos());
		this->fixedProductNos = toProductNos(fixedProducts);
		this->unfixedProductNos = toProductNos(unfixedProducts);
		updateProductsInfo(fixedProducts, unfixedProducts);
		setAddButtonEnabled(true);
		emit productChangedOrUpdated(true);
	} else {
		clearAll();
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::on_noNetRetryButton_clicked()
{
	if (PLSNetworkMonitor::Instance()->IsInternetAvailable()) {
		updateAllProductsInfo();
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::on_gotoNaverShoppingToolButton_clicked()
{
	//QDesktopServices::openUrl(CHANNEL_NAVER_SHOPPING_LIVE_LOGIN);
	on_ppAddButton_clicked();
}

bool PLSLiveInfoNaverShoppingLIVEProductList::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->hline2) {
		switch (event->type()) {
		case QEvent::Resize:
		case QEvent::Move:
			if (productLoadingView) {
				productLoadingView->updateGeometry();
			}
			break;
		default:
			break;
		}
	}

	return QWidget::eventFilter(watched, event);
}
