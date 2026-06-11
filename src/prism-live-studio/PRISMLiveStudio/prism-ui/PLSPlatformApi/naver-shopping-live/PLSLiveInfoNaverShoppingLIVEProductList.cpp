#include "PLSLiveInfoNaverShoppingLIVEProductList.h"
#include "ui_PLSLiveInfoNaverShoppingLIVEProductList.h"
#include "PLSLiveInfoNaverShoppingLIVEProductItemView.h"
#include "PLSNaverShoppingLIVEProductDialogView.h"
#include "PLSPlatformNaverShoppingLIVE.h"
#include "PLSLiveInfoNaverShoppingLIVE.h"
#include "PLSNaverShoppingLIVEDataManager.h"
#include "PLSLoadingView.h"
#include "PLSAlertView.h"
#include "pls-common-define.hpp"
#include "PLSLoadNextPage.h"

#include <QPainter>
#include <QDesktopServices>
#include <chrono>
#include <QPainterPath>
#include <QHBoxLayout>
#include "liblog.h"

using namespace common;
using namespace std;
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

void PLSLiveInfoNaverShoppingLIVEProductCountBadge::setProductCount(int productCount_, bool ok)
{
	this->productCount = QString::number(productCount_);
	setProperty("textLength", this->productCount.length());
	pls_flush_style(this);
	setVisible(ok && (productCount_ > 0) && pls_get_network_state());
	update();
}

void PLSLiveInfoNaverShoppingLIVEProductCountBadge::paintEvent(QPaintEvent * /*event*/)
{
	QPainter painter(this);

	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	QRect rect = this->rect();
	painter.fillRect(rect, Qt::transparent);
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(39, 39, 39));
	painter.drawRect(rect);

	painter.setBrush(QColor(239, 252, 53));
	painter.drawRoundedRect(rect, 10, 10);

	painter.setPen(Qt::black);
	int yPos = 0;
#if defined(Q_OS_WIN)
	yPos = -2;
#endif
	painter.drawText(rect.adjusted(0, yPos, 0, 0), productCount, QTextOption(Qt::AlignCenter));
}

PLSLiveInfoNaverShoppingLIVEProductList::PLSLiveInfoNaverShoppingLIVEProductList(QWidget *parent) : QWidget(parent)
{
	ui = pls_new<Ui::PLSLiveInfoNaverShoppingLIVEProductList>();

	ui->setupUi(this);
	updateFixProductTip();
	updateProductBtnNumber();
	showPage(false);

	auto lang = pls_get_current_language();
	ui->ppAddButton->setProperty("lang", lang);
	updateUIWhenSwitchScheduleList();
	connect(ui->gotoNaverShoppingToolTipLabel1, &QLabel::linkActivated, this, [](const QString &) { QDesktopServices::openUrl(CHANNEL_NAVER_SHOPPING_LIVE_LOGIN); });

	ui->nppAddButton->setProperty("lang", lang);
	ui->noNetRetryButton->setProperty("lang", lang);
	ui->placeholder->hide();

	titleWidget = pls_new<QWidget>(this);
	titleWidget->setObjectName("titleWidget");

	QHBoxLayout *layout = pls_new<QHBoxLayout>(titleWidget);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	QHBoxLayout *layout1 = pls_new<QHBoxLayout>();
	layout1->setContentsMargins(0, 0, 0, 1);
	layout1->setSpacing(0);

	QHBoxLayout *layout2 = pls_new<QHBoxLayout>();
	layout2->setContentsMargins(0, 2, 0, 0);
	layout2->setSpacing(0);

	layout->addLayout(layout1);
	layout->addLayout(layout2);
	layout->addStretch(1);

	QLabel *titleLabel = pls_new<QLabel>(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("navershopping.product.title")), this);
	titleLabel->setObjectName("titleLabel");
	titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
	layout1->addWidget(titleLabel);

	productCountBadge = pls_new<PLSLiveInfoNaverShoppingLIVEProductCountBadge>(this);
	productCountBadge->setObjectName("productCountBadge");
	productCountBadge->hide();
	layout2->addWidget(productCountBadge);

	productBtnGroup = pls_new<QButtonGroup>(this);
	connect(productBtnGroup, &QButtonGroup::idClicked, this, [this](int index) { OnBtnGroupClicked(static_cast<PLSProductType>(index)); });
	productBtnGroup->addButton(ui->mainProductBtn, static_cast<int>(PLSProductType::MainProduct));
	productBtnGroup->addButton(ui->subProductBtn, static_cast<int>(PLSProductType::SubProduct));
	ui->mainProductBtn->setChecked(true);

	ui->hline2->installEventFilter(this);
}

PLSLiveInfoNaverShoppingLIVEProductList::~PLSLiveInfoNaverShoppingLIVEProductList()
{
	PLSLiveInfoNaverShoppingLIVEProductItemView::dealloc(itemViews, ui->productsLayout);
	PLSLoadingView::deleteLoadingView(productLoadingView);
	PLSLoadNextPage::deleteLoadNextPage(productNextPage);

	pls_delete(ui, nullptr);
	if (productDialogView) {
		pls_delete(productDialogView, nullptr);
	}
}

PLSPlatformNaverShoppingLIVE *PLSLiveInfoNaverShoppingLIVEProductList::getPlatform()
{
	for (QWidget *parent = parentWidget(); parent; parent = parent->parentWidget()) {
		if (auto liveInfo = dynamic_cast<PLSLiveInfoNaverShoppingLIVE *>(parent); liveInfo) {
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

QList<PLSLiveInfoNaverShoppingLIVEProductList::Product> PLSLiveInfoNaverShoppingLIVEProductList::getFixedOrUnfixedProducts(PLSProductType productType, bool fixed) const
{
	auto products = getProduct(productType);
	if (products.isEmpty()) {
		return {};
	}

	QList<Product> fixedProds;
	for (auto product : products) {
		if (product.represent == fixed) {
			fixedProds.append(product);
		}
	}
	return fixedProds;
}

QList<PLSLiveInfoNaverShoppingLIVEProductList::Product> PLSLiveInfoNaverShoppingLIVEProductList::getAllProducts() const
{
	QList<Product> allTempProducts;

	for (auto key : allProducts.keys()) {
		auto product = allProducts.value(key);
		allTempProducts.append(product);
	}
	return allTempProducts;
}

bool PLSLiveInfoNaverShoppingLIVEProductList::hasUnattachableProduct()
{
	for (const auto &value : allProducts.values()) {
		for (const auto &product : value) {
			if (!product.attachable) {
				return true;
			}
		}
	}
	return false;
}

int PLSLiveInfoNaverShoppingLIVEProductList::getProductCount() const
{
	int count = 0;
	for (auto key : allProducts.keys()) {
		count += allProducts.value(key).count();
	}
	return count;
}

QList<qint64> PLSLiveInfoNaverShoppingLIVEProductList::getFixedOrUnfixedProductNos(PLSProductType productType, bool fixed) const
{
	auto product = allProducts[productType];
	if (product.isEmpty()) {
		return {};
	}

	QList<qint64> productNos;
	for (auto pro : product) {
		if (pro.represent == fixed) {
			productNos.append(pro.productNo);
		}
	}
	return productNos;
}

int PLSLiveInfoNaverShoppingLIVEProductList::getProductCount(PLSProductType productType) const
{
	return getProduct(productType).count();
}

PLSProductType PLSLiveInfoNaverShoppingLIVEProductList::getProductType() const
{
	bool mainProductChecked = ui->mainProductBtn->isChecked();
	return mainProductChecked ? PLSProductType::MainProduct : PLSProductType::SubProduct;
}

QList<PLSNaverShoppingLIVEAPI::ProductInfo> PLSLiveInfoNaverShoppingLIVEProductList::getProduct(PLSProductType productType) const
{
	auto iter = allProducts.find(productType);
	if (iter != allProducts.end()) {
		return iter.value();
	}
	return QList<PLSNaverShoppingLIVEAPI::ProductInfo>();
}

QList<qint64> PLSLiveInfoNaverShoppingLIVEProductList::getProductNos(PLSProductType productType) const
{
	auto product = getProduct(productType);
	if (product.isEmpty()) {
		return QList<qint64>();
	}
	return toProductNos(product);
}

QList<qint64> PLSLiveInfoNaverShoppingLIVEProductList::getProductNos(const QList<Product> &products, bool introducing) const
{
	QList<qint64> productNos;
	for (auto pro : products) {
		if (pro.introducing == introducing) {
			productNos.append(pro.productNo);
		}
	}
	return productNos;
}

QMap<PLSProductType, QList<PLSNaverShoppingLIVEAPI::ProductInfo>> PLSLiveInfoNaverShoppingLIVEProductList::getProducts() const
{
	return allProducts;
}

void PLSLiveInfoNaverShoppingLIVEProductList::setProductRepresent(PLSProductType productType, qint64 productNo, bool represent)
{
	auto product = allProducts[productType];
	auto iter = std::find_if(product.begin(), product.end(), [productNo](Product &pro) { return pro.productNo == productNo; });
	if (iter == product.end()) {
		return;
	}
	auto count = getFixedOrUnfixedProductNos(productType, true).count();
	Product newProduct = *iter;
	newProduct.represent = represent;

	int index = iter - product.begin();
	product.takeAt(index);

	if (represent) {
		product.emplaceFront(newProduct);
	} else {
		product.insert(count - 1, newProduct);
	}
	setProduct(productType, product);
}

void PLSLiveInfoNaverShoppingLIVEProductList::setProductType(PLSProductType productType)
{
	ui->mainProductBtn->setChecked(productType == PLSProductType::MainProduct);
	ui->subProductBtn->setChecked(productType == PLSProductType::SubProduct);
}

void PLSLiveInfoNaverShoppingLIVEProductList::setProduct(PLSProductType productType, QList<Product> products_)
{
	auto iter = allProducts.find(productType);
	if (iter == allProducts.end()) {
		allProducts.insert(productType, products_);
	} else {
		allProducts[productType] = products_;
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::removeProduct(PLSProductType productType, qint64 productNo)
{
	auto product = allProducts[productType];
	auto iter = std::find_if(product.begin(), product.end(), [productNo](Product &pro) { return pro.productNo == productNo; });
	if (iter == product.end()) {
		return;
	}
	product.erase(iter);
	setProduct(productType, product);
}

void PLSLiveInfoNaverShoppingLIVEProductList::setOwnerScrollArea(QScrollArea *ownerScrollArea_)
{
	this->ownerScrollArea = ownerScrollArea_;
}

void PLSLiveInfoNaverShoppingLIVEProductList::setProductNos(PLSProductType productType, const QList<qint64> &fixedProductNos_, const QList<qint64> &unfixedProductNos_,
							    const QList<qint64> &introducingProductNos, std::function<void()> &&updateFinished)
{
	setIsScheduleLiveLoading(false);

	setProductNosUpdateFinished = std::move(updateFinished);

	PLSLoadNextPage::deleteLoadNextPage(productNextPage);
	for (auto item : itemViews) {
		item->setVisible(false);
	}

	updateProductCountBadge();
	updateProductBtnNumber();
	updateFixProductTip();
	updateAllProductsInfo(productType, fixedProductNos_, unfixedProductNos_, introducingProductNos);
}

void PLSLiveInfoNaverShoppingLIVEProductList::setAllProducts(const QList<Product> &products, bool update)
{
	QList<Product> mainProducts;
	QList<Product> subProducts;

	QList<Product> mainFixedProds;
	QList<Product> mainUnfixedProds;
	QList<Product> subUnfixedProds;

	for (const auto &product : products) {
		if (product.productType == PLSProductType::MainProduct) {
			mainProducts.append(product);
			if (product.represent) {
				mainFixedProds.append(product);
			} else {
				mainUnfixedProds.append(product);
			}
		} else if (product.productType == PLSProductType::SubProduct) {
			subProducts.append(product);
			subUnfixedProds.append(product);
		}
	}

	setProduct(PLSProductType::MainProduct, mainProducts);
	setProduct(PLSProductType::SubProduct, subProducts);

	auto productType = getProductType();
	if (productType == PLSProductType::MainProduct) {
		setProducts(productType, mainFixedProds, mainUnfixedProds, update);
	} else {
		setProducts(productType, {}, subUnfixedProds, update);
	}
	updateProductBtnNumber();
	updateUIWhenSwitchScheduleList();
}

void PLSLiveInfoNaverShoppingLIVEProductList::setProducts(PLSProductType productType, QList<Product> fixedProducts_, QList<Product> unfixedProducts_, bool update)
{
	this->fixedProducts.clear();
	this->unfixedProducts.clear();

	if (update) {
		auto fixedProductsNos = toProductNos(fixedProducts_);
		auto unfixedProductsNos = toProductNos(unfixedProducts_);
		auto introducingNos = getProductNos(fixedProducts_, true);
		introducingNos.append(getProductNos(unfixedProducts_, true));

		setProductNos(productType, fixedProductsNos, unfixedProductsNos, introducingNos);
		return;
	}

	setIsScheduleLiveLoading(false);

	setProductNosUpdateFinished = nullptr;

	if (!fixedProducts_.isEmpty() || !unfixedProducts_.isEmpty()) {
		updateVisibleProductsInfo(productType, fixedProducts_, unfixedProducts_);

		showPage(true);
		updateProductsInfo(fixedProducts_, unfixedProducts_);
		// spec-out 2024/07/31
#if 0
		if (checkAgeLimitProducts(fixedProducts_, unfixedProducts_)) {
			if (!fixedProducts_.isEmpty() || !unfixedProducts_.isEmpty()) {
				updateProductsInfo(fixedProducts_, unfixedProducts_);
			} else {
				clearAll();
			}
		}
#endif
	} else {
		PLSLiveInfoNaverShoppingLIVEProductItemView::dealloc(itemViews, ui->productsLayout);
		showPage(false);
		updateProductCountBadge();
		updateProductBtnNumber();
		updateFixProductTip();
	}

	setAddButtonEnabled(true);
}

void PLSLiveInfoNaverShoppingLIVEProductList::setProductReadonly(bool readonly)
{
	productReadonly = readonly;

	ui->ppAddButton->setEnabled(!readonly && (getProductCount() < PLSNaverShoppingLIVEDataManager::MAX_LIVEINFO_PRODUCT_COUNT));
	ui->nppAddButton->setEnabled(!readonly && (getProductCount() < PLSNaverShoppingLIVEDataManager::MAX_LIVEINFO_PRODUCT_COUNT));

	ui->guideWidget->setVisible(!readonly);
	ui->hline2->setVisible(!readonly);

	checkProductEmpty();

	for (auto itemView : itemViews) {
		itemView->setProductReadonly(readonly);
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::setIsLiving(bool isLiving_)
{
	this->isLiving = isLiving_;
}

void PLSLiveInfoNaverShoppingLIVEProductList::setIsScheduleLive(bool isScheduleLive_)
{
	this->isScheduleLive = isScheduleLive_;
	checkProductEmpty();
}

void PLSLiveInfoNaverShoppingLIVEProductList::setIsScheduleLiveLoading(bool isScheduleLiveLoading_)
{
	this->isScheduleLiveLoading = isScheduleLiveLoading_;
	checkProductEmpty();
}

void PLSLiveInfoNaverShoppingLIVEProductList::setIsPlanningLive(bool isPlanningLive_)
{
	this->isPlanningLive = isPlanningLive_;
}

void PLSLiveInfoNaverShoppingLIVEProductList::expiredClose()
{
	if (productDialogView) {
		productDialogView->reject();
		pls_delete(productDialogView, nullptr);
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::resetIconSize() const
{
	PLSLiveInfoNaverShoppingLIVEProductItemView::resetIconSize(itemViews);
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateWhenDpiChanged() const
{
	PLSLiveInfoNaverShoppingLIVEProductItemView::updateWhenDpiChanged(itemViews);
}

void PLSLiveInfoNaverShoppingLIVEProductList::cancelSearchRequest()
{
	for (auto key : cancelRequestMap.keys()) {
		pls::http::Request request = cancelRequestMap.value(key).first;
		cancelRequestMap[key].second = true;
		request.abort();
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::searchProduct(int currentPage, PLSProductType productType, const QList<qint64> &fixedProductNos, const QList<qint64> &unfixedProductNos,
							    const QList<qint64> &introducingProductNos)
{
	QString key = createSearchProductKey(fixedProductNos, unfixedProductNos);
	if (auto iter = cancelRequestMap.find(key); iter != cancelRequestMap.end()) {
		if (!iter.value().second) {
			return;
		}
	}
	cancelRequestMap.insert(key, QPair<pls::http::Request, bool>(pls::http::Request(), false));
	for (auto key_ : cancelRequestMap.keys()) {
		if (key_ == key) {
			continue;
		}
		pls::http::Request request = cancelRequestMap.value(key_).first;
		cancelRequestMap[key_].second = true;
		request.abort();
	}
	pls::http::Request request = PLSNaverShoppingLIVEAPI::productSearchByProductNos(
		getPlatform(), currentPage, PLSNaverShoppingLIVEDataManager::LIVE_INFO_PRODUCT_PAGE_SIZE, productType, fixedProductNos, unfixedProductNos, introducingProductNos,
		[this, currentPage, fixedProductNos, unfixedProductNos,
		 start = std::chrono::steady_clock::now()](bool ok, bool hasNext, PLSProductType productType, QList<PLSNaverShoppingLIVEAPI::ProductInfo> fixedProducts_,
							   QList<PLSNaverShoppingLIVEAPI::ProductInfo> unfixedProducts_, const QString &searchKey) {
			auto iter = cancelRequestMap.find(searchKey);
			if (iter != cancelRequestMap.end()) {
				if (iter.value().second) {
					cancelRequestMap.remove(searchKey);
					if (cancelRequestMap.isEmpty()) {
						PLSLoadingView::deleteLoadingView(productLoadingView);
					}
					return;
				}
				cancelRequestMap.remove(searchKey);
			}

			updateAllProductsInfo_lambda(currentPage, productType, fixedProductNos, unfixedProductNos, ok, hasNext, fixedProducts_, unfixedProducts_, start);
		},
		this, [](const QObject *obj) { return pls_object_is_valid(obj); });
	cancelRequestMap[key] = QPair<pls::http::Request, bool>(request, false);
}

void PLSLiveInfoNaverShoppingLIVEProductList::addProduct(const QList<Product> &fixedProducts, const QList<Product> &unfixedProducts)
{
	auto fixedProductCount = fixedProducts.count();
	auto productCount = fixedProducts.count() + unfixedProducts.count();
	PLSLiveInfoNaverShoppingLIVEProductItemView::addBatchCache(productCount, true);

	for (int i = 0; i < productCount; ++i) {
		bool fixed = i < fixedProductCount;
		auto product = fixed ? fixedProducts[i] : unfixedProducts[i - fixedProductCount];
		if (isProductItemViewExisted(product.productNo)) {
			continue;
		}

		product.represent = fixed;
		product.setAttachmentType(getProductType());
		if (fixed) {
			this->fixedProducts.append(product);
		} else {
			this->unfixedProducts.append(product);
		}
		auto view = PLSLiveInfoNaverShoppingLIVEProductItemView::alloc(ui->products, this, &PLSLiveInfoNaverShoppingLIVEProductList::onFixButtonClicked,
									       &PLSLiveInfoNaverShoppingLIVEProductList::onRemoveButtonClicked, getPlatform(), product);
		view->setProductReadonly(isScheduleLive);
		ui->productsLayout->addWidget(view);
		pls_flush_style_recursive(view);
		this->itemViews.append(view);
		view->show();
	}

	ui->verticalLayout_2->invalidate();
}

QString PLSLiveInfoNaverShoppingLIVEProductList::createSearchProductKey(const QList<qint64> &fixedProducts, const QList<qint64> &unfixedProducts)
{
	QString key;
	for (auto no : fixedProducts) {
		key += QString::number(no).append("-");
	}
	for (auto no : unfixedProducts) {
		key += QString::number(no).append("-");
	}
	return key;
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateAllProductsInfo(PLSProductType productType, const QList<qint64> &fixedProducts, const QList<qint64> &unfixedProducts,
								    const QList<qint64> &introducingProducts)
{
	if (!fixedProducts.isEmpty() || !unfixedProducts.isEmpty()) {
		showPage(true);
		setAddButtonEnabled(false);
		ui->placeholder->show();

		if (!pls_get_network_state()) {
			return;
		}
		PLSLoadingView::newLoadingView(productLoadingView, !setProductNosUpdateFinished, this, productReadonly ? 96 : 65,
					       [this](QRect &geometry, const PLSLoadingView *loadingView) { return getProductLoadingViewport(geometry, loadingView); });
		titleWidget->raise();
		searchProduct(0, productType, fixedProducts, unfixedProducts, introducingProducts);
	} else {
		cancelSearchRequest();
		showPage(false);
		setAddButtonEnabled(true);
		productUpdateFinished();
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateAllProductsInfo_lambda(int currentPage, PLSProductType productType, const QList<qint64> &fixedProductNos, const QList<qint64> &unfixedProductNos,
									   bool ok, bool hasNext, QList<PLSNaverShoppingLIVEAPI::ProductInfo> &fixedProducts_,
									   QList<PLSNaverShoppingLIVEAPI::ProductInfo> &unfixedProducts_, const std::chrono::steady_clock::time_point &start)
{
	if (ok) {
		updateAllProductsInfo_lambda_ok(currentPage, fixedProducts_, unfixedProducts_);

		if (hasNext) {
			PLSLoadNextPage::newLoadNextPage(productNextPage, ownerScrollArea, [this, currentPage, productType, fixedProductNos, unfixedProductNos, fixedProducts_, unfixedProducts_]() {
				auto introducingNos = getProductNos(fixedProducts_, true);
				introducingNos.append(getProductNos(unfixedProducts_, true));
				searchProduct(currentPage + 1, productType, fixedProductNos, unfixedProductNos, introducingNos);
			});
		} else {
			PLSLoadNextPage::deleteLoadNextPage(productNextPage);
		}

	} else if ((std::chrono::steady_clock::now() - start) > 300ms) {
		if (cancelRequestMap.isEmpty()) {
			PLSLoadingView::deleteLoadingView(productLoadingView);
			showPage(false, true, true);
			updateProductCountBadge(false);
			updateProductBtnNumber();
			updateFixProductTip();
			setAddButtonEnabled(true);
		}
	} else {
		if (cancelRequestMap.isEmpty()) {
			updateProductCountBadge(false);
			updateProductBtnNumber();
			updateFixProductTip();
			QTimer::singleShot(300, this, [this]() {
				PLS_INFO("PLSLiveInfoNaverShoppingLIVEProductList", "single shot timer triggered for update product list UI.");
				PLSLoadingView::deleteLoadingView(productLoadingView);
				showPage(false, true, true);
				setAddButtonEnabled(true);
			});
		}
	}

	emit productChangedOrUpdated(false);
	productUpdateFinished();
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateAllProductsInfo_lambda_ok(int currentPage, QList<PLSNaverShoppingLIVEAPI::ProductInfo> &fixedProducts_,
									      QList<PLSNaverShoppingLIVEAPI::ProductInfo> &unfixedProducts_)
{
	PLSLoadingView::deleteLoadingView(productLoadingView);
	if (!fixedProducts_.isEmpty() || !unfixedProducts_.isEmpty()) {
		showPage(true);

		if (currentPage == 0) {
			updateProductsInfo(fixedProducts_, unfixedProducts_);
		} else {
			addProduct(fixedProducts_, unfixedProducts_);
		}
		// spec-out 2024/07/31
#if 0
		if (checkAgeLimitProducts(fixedProducts_, unfixedProducts_)) {
			if (!fixedProducts_.isEmpty() || !unfixedProducts_.isEmpty()) {
				updateProductsInfo(fixedProducts_, unfixedProducts_);
			} else {
				clearAll();
			}
		}
#endif
	} else {
		showPage(false);
	}
	setAddButtonEnabled(true);
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateProductsInfo(const QList<Product> &fixedProducts_, const QList<Product> &unfixedProducts_)
{
	int fixedProductCount = fixedProducts_.count();
	int unfixedProductCount = unfixedProducts_.count();
	int productCount = fixedProductCount + unfixedProductCount;

	this->fixedProducts = fixedProducts_;
	this->unfixedProducts = unfixedProducts_;

	updateProductCountBadge();
	updateProductBtnNumber();
	updateFixProductTip();

	allowScrollAreaShowVerticalScrollBar();
	PLSLiveInfoNaverShoppingLIVEProductItemView::addBatchCache(productCount, true);

	if (productCount <= itemViews.count()) {
		for (int i = 0, j = 0; i < fixedProductCount; ++i, ++j) {
			auto view = itemViews[j];
			const auto &product = fixedProducts_[i];
			view->setInfo(getPlatform(), product, true, isScheduleLive);
		}

		for (int i = 0, j = fixedProductCount; i < unfixedProductCount; ++i, ++j) {
			auto view = itemViews[j];
			const auto &product = unfixedProducts_[i];
			view->setInfo(getPlatform(), product, false, isScheduleLive);
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
			const auto &product = (i < fixedProductCount) ? fixedProducts_[i] : unfixedProducts_[i - fixedProductCount];
			view->setInfo(getPlatform(), product, false, isScheduleLive);
		}

		if (productCount > itemViews.count()) {
			for (int i = itemViews.count(); i < productCount; ++i) {
				const auto &product = (i < fixedProductCount) ? fixedProducts_[i] : unfixedProducts_[i - fixedProductCount];
				auto view = PLSLiveInfoNaverShoppingLIVEProductItemView::alloc(ui->products, this, &PLSLiveInfoNaverShoppingLIVEProductList::onFixButtonClicked,
											       &PLSLiveInfoNaverShoppingLIVEProductList::onRemoveButtonClicked, getPlatform(), product);
				view->setProductReadonly(isScheduleLive);
				ui->productsLayout->addWidget(view);
				pls_flush_style_recursive(view);
				this->itemViews.append(view);
				view->show();
			}

			ui->verticalLayout_2->invalidate();
		}
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateVisibleProductsInfo(PLSProductType productType, QList<Product> &fixedProducts, QList<Product> &unfixedProducts)
{
	auto products = fixedProducts + unfixedProducts;
	if (products.count() <= PLSNaverShoppingLIVEDataManager::LIVE_INFO_PRODUCT_PAGE_SIZE) {
		return;
	}

	PLSLoadNextPage::deleteLoadNextPage(productNextPage);
	PLSLoadNextPage::newLoadNextPage(productNextPage, ownerScrollArea, [this, productType, fixedProducts, unfixedProducts]() {
		auto fixedProductsNos = toProductNos(fixedProducts);
		auto unfixedProductsNos = toProductNos(unfixedProducts);
		auto introducingNos = getProductNos(fixedProducts, true);
		introducingNos.append(getProductNos(unfixedProducts, true));
		searchProduct(1, productType, fixedProductsNos, unfixedProductsNos, introducingNos);
	});
	fixedProducts.clear();
	unfixedProducts.clear();
	for (int i = 0; i < PLSNaverShoppingLIVEDataManager::LIVE_INFO_PRODUCT_PAGE_SIZE; i++) {
		auto product = products[i];
		if (product.represent) {
			fixedProducts.append(product);
		} else {
			unfixedProducts.append(product);
		}
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateFixProductTip(bool ok)
{
	if (isScheduleLive) {
		ui->fixGuideText->setText(QTStr("NaverShoppingLive.LiveInfo.Schedule.Fixed.Product.Guide").arg(ok ? getFixedOrUnfixedProducts(PLSProductType::MainProduct, true).count() : 0));
	} else {
		ui->fixGuideText->setText(tr("NaverShoppingLive.LiveInfo.Product.FixProductTip").arg(ok ? getFixedOrUnfixedProducts(PLSProductType::MainProduct, true).count() : 0));
	}
	ui->fixGuideText->setVisible(ui->mainProductBtn->isChecked());
	ui->fixGuideIcon->setVisible(ui->mainProductBtn->isChecked());
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateUIWhenSwitchScheduleList()
{
	ui->ppAddButton->setVisible(!isScheduleLive);
	ui->nppAddButton->setVisible(!isScheduleLive);
	ui->gotoNaverShoppingToolButton->setVisible(!isScheduleLive);
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateProductCountBadge(bool ok)
{
	productCountBadge->setProductCount(getProductCount(), ok);
}

void PLSLiveInfoNaverShoppingLIVEProductList::updateProductBtnNumber()
{
	int mainProdutCount = getProductCount(PLSProductType::MainProduct);
	int subProdutCount = getProductCount(PLSProductType::SubProduct);

	ui->mainProductBtn->setTitle(QTStr("NaverShoppingLive.LiveInfo.Live.Products"));
	ui->subProductBtn->setTitle(QTStr("NaverShoppingLive.LiveInfo.Other.Products"));
	ui->mainProductBtn->setSelectedNumber(mainProdutCount);
	ui->subProductBtn->setSelectedNumber(subProdutCount);
}

void PLSLiveInfoNaverShoppingLIVEProductList::setAddButtonEnabled(bool enabled)
{
	bool addButtonEnabled = enabled && !productReadonly && (getProductCount(getProductType()) < PLSNaverShoppingLIVEDataManager::MAX_LIVEINFO_PRODUCT_COUNT);

	ui->ppAddButton->setEnabled(addButtonEnabled);
	ui->nppAddButton->setEnabled(addButtonEnabled);
}

void PLSLiveInfoNaverShoppingLIVEProductList::checkProductEmpty()
{
	auto count = getProductCount(getProductType());
	if (0 == count) {
		showPage(false);
	}
}

bool PLSLiveInfoNaverShoppingLIVEProductList::checkAgeLimitProducts(QList<Product> &fixedProducts_, QList<Product> &unfixedProducts_)
{
	QList<Product> ageLimitProducts;
	for (const auto &product : fixedProducts_) {
		if (!product.isMinorPurchasable) {
			ageLimitProducts.append(product);
		}
	}
	for (const auto &product : unfixedProducts_) {
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
		removeByProductNo(fixedProducts_, product.productNo);
		removeByProductNo(unfixedProducts_, product.productNo);
		removeProduct(getProductType(), product.productNo);
	}
	return true;
}

void PLSLiveInfoNaverShoppingLIVEProductList::clearAll()
{
	PLSLiveInfoNaverShoppingLIVEProductItemView::dealloc(itemViews, ui->productsLayout);
	this->fixedProducts.clear();
	this->unfixedProducts.clear();
	showPage(false);
	setAddButtonEnabled(true);
	updateFixProductTip();
	updateProductCountBadge();
	updateProductBtnNumber();
}

void PLSLiveInfoNaverShoppingLIVEProductList::onFixButtonClicked(PLSLiveInfoNaverShoppingLIVEProductItemView *itemView)
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_LIVEINFO, "Fix Product Button", ACTION_CLICK);

	qint64 productNo = itemView->getProductNo();
	auto fixedProductNos = getFixedOrUnfixedProductNos(getProductType(), true);
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

void PLSLiveInfoNaverShoppingLIVEProductList::onRemoveButtonClicked(const PLSLiveInfoNaverShoppingLIVEProductItemView *itemView)
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_LIVEINFO, "Remove Product Button", ACTION_CLICK);
	if (getProductType() != PLSProductType::MainProduct) {
		doProductRemoved(itemView);
		return;
	}

	if (isLiving && (getProductCount(PLSProductType::MainProduct) == PLSNaverShoppingLIVEDataManager::MIN_LIVEINFO_PRODUCT_COUNT)) {
		pls_alert_error_message(this, tr("Alert.Title"), tr("NaverShoppingLive.LiveInfo.Product.AtLeastOne"), PLSAlertView::Button::Ok, PLSAlertView::Button::Ok);
	} else {
		doProductRemoved(itemView);
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::onSmartStoreChanged()
{
	auto count = getProductCount(getProductType());
	if (0 == count) {
		clearAll();
		PLSLoadingView::newLoadingView(productLoadingView, this, 65, [this](QRect &geometry, const PLSLoadingView *loadingView) { return getProductLoadingViewport(geometry, loadingView); });
		titleWidget->raise();
		setAddButtonEnabled(false);

		emit productChangedOrUpdated(true);
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::doProductFixed(PLSLiveInfoNaverShoppingLIVEProductItemView *itemView, bool isFixed)
{
	qint64 productNo = itemView->getProductNo();
	auto productType = getProductType();

	auto fixedProductNos = getFixedOrUnfixedProductNos(productType, true);
	setProductRepresent(getProductType(), productNo, isFixed);

	if (isFixed) {
		if (!fixedProductNos.contains(productNo)) {
			removeByProductNo(unfixedProducts, productNo);
		} else if (fixedProductNos.first() != productNo) {
			removeByProductNo(fixedProducts, productNo);
		} else {
			return;
		}

		updateFixProductTip();
		itemView->setFixed(true);
		fixedProducts.prepend(itemView->getProduct());
		ui->productsLayout->removeWidget(itemView);
		ui->productsLayout->insertWidget(0, itemView);
		itemViews.removeOne(itemView);
		itemViews.insert(0, itemView);
	} else {
		removeByProductNo(fixedProducts, productNo);
		updateFixProductTip();

		itemView->setFixed(false);
		unfixedProducts.prepend(itemView->getProduct());
		ui->productsLayout->removeWidget(itemView);
		auto fixedProductCount = getFixedOrUnfixedProductNos(productType, true).count();

		ui->productsLayout->insertWidget(fixedProductCount, itemView);
		itemViews.removeOne(itemView);
		itemViews.insert(fixedProductCount, itemView);
	}

	emit productChangedOrUpdated(true);
}

void PLSLiveInfoNaverShoppingLIVEProductList::doProductRemoved(const PLSLiveInfoNaverShoppingLIVEProductItemView *itemView)
{
	qint64 productNo = itemView->getProductNo();

	removeByProductNo(fixedProducts, productNo);
	removeByProductNo(unfixedProducts, productNo);
	auto productType = getProductType();
	removeProduct(productType, productNo);

	updateProductsInfo(fixedProducts, unfixedProducts);
	setAddButtonEnabled(true);

	if (getProductCount(getProductType()) == 0) {
		showPage(false);
	}

	emit productChangedOrUpdated(true);
}

bool PLSLiveInfoNaverShoppingLIVEProductList::getProductLoadingViewport(QRect &geometry, const PLSLoadingView * /*loadingView*/) const
{
	if (!ownerScrollArea) {
		return false;
	}

	QPoint pos{0, mapFromGlobal(ui->hline2->mapToGlobal(QPoint{0, ui->hline2->height()})).y()};
	const QWidget *viewport = ownerScrollArea->viewport();
	QRect viewportRect{viewport->mapToGlobal(QPoint{0, 0}), viewport->size()};
	QRect parentRect{this->mapToGlobal(pos), this->size()};

	geometry = viewportRect & parentRect;
	if (0 == geometry.width() || 0 == geometry.height()) {
		geometry.setWidth(parentRect.width());
		geometry.setHeight(parentRect.height());
	}
	geometry.moveTopLeft(pos);
	return true;
}

void PLSLiveInfoNaverShoppingLIVEProductList::showPage(bool hasProducts, bool showNoNet, bool apiFailed, bool showProductList)
{
	ui->placeholder->hide();
	if (showNoNet || !pls_get_network_state()) {
		ui->noNetTipLabel->setText(apiFailed && pls_get_network_state() ? QObject::tr("NaverShoppingLive.LiveInfo.ProductInfoUpdateApiFailedTip")
										: QObject::tr("NaverShoppingLive.LiveInfo.Product.NoNetTip"));
		ui->noProductPage->hide();
		setNoProductPageShowOrHide(false);
		ui->productPage->hide();
		ui->noNetPage->show();
	} else if (!hasProducts) {
		ui->productPage->hide();
		ui->noNetPage->hide();
		if (!isScheduleLive && !isScheduleLiveLoading) {
			setNoProductPageShowOrHide(false);
			ui->noProductTipLabel->setText(ui->mainProductBtn->isChecked() ? QTStr("NaverShoppingLive.LiveInfo.Main.Product.NoProductTip")
										       : QTStr("NaverShoppingLive.LiveInfo.Sub.Product.NoProductTip"));
			ui->noProductPage->show();
		} else {
			ui->noProductPage->hide();
			ui->gotoNaverShoppingToolTipLabel1->setText(ui->mainProductBtn->isChecked() ? QTStr("NaverShoppingLive.LiveInfo.Main.Product.NoProductTip")
												    : QTStr("NaverShoppingLive.LiveInfo.Sub.Product.NoProductTip"));
			setNoProductPageShowOrHide(!isScheduleLiveLoading);
		}
	} else {
		ui->noProductPage->hide();
		setNoProductPageShowOrHide(false);
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

void PLSLiveInfoNaverShoppingLIVEProductList::OnBtnGroupClicked(PLSProductType productType)
{
	PLSLoadNextPage::deleteLoadNextPage(productNextPage);
	auto products = getProduct(productType);

	QList<Product> fixedProducts;
	QList<Product> unfixedProducts;
	for (auto product : products) {
		if (product.represent) {
			fixedProducts.append(product);
		} else {
			unfixedProducts.append(product);
		}
	}

	setProducts(productType, fixedProducts, unfixedProducts, true);

	setAddButtonEnabled(products.count() <= PLSNaverShoppingLIVEDataManager::MAX_LIVEINFO_PRODUCT_COUNT);
}

bool PLSLiveInfoNaverShoppingLIVEProductList::isProductItemViewExisted(qint64 productNo)
{
	auto iter = std::find_if(itemViews.begin(), itemViews.end(), [productNo](PLSLiveInfoNaverShoppingLIVEProductItemView *item) { return item->getProductNo() == productNo; });
	return iter != itemViews.end();
}

void PLSLiveInfoNaverShoppingLIVEProductList::setNoProductPageShowOrHide(bool show)
{
	if (isScheduleLive) {
		ui->gotoNaverShoppingToolTipLabel1->setText(ui->mainProductBtn->isChecked() ? QTStr("NaverShoppingLive.LiveInfo.Schedule.Main.Product.Guide")
											    : QTStr("NaverShoppingLive.LiveInfo.Schedule.Sub.Product.Guide"));
		ui->verticalSpacer_7->changeSize(0, 0);
	} else {
		ui->gotoNaverShoppingToolTipLabel1->setText(ui->mainProductBtn->isChecked() ? QTStr("NaverShoppingLive.LiveInfo.Main.Product.NoProductTip")
											    : QTStr("NaverShoppingLive.LiveInfo.Sub.Product.NoProductTip"));
		ui->verticalSpacer_7->changeSize(0, 16);
	}
	ui->scheNoProductWidget->setVisible(show);
}

void PLSLiveInfoNaverShoppingLIVEProductList::on_nppAddButton_clicked()
{
	on_ppAddButton_clicked();
}

void PLSLiveInfoNaverShoppingLIVEProductList::on_ppAddButton_clicked()
{
	PLS_UI_STEP(MODULE_NAVER_SHOPPING_LIVE_LIVEINFO, "Add Product Button", ACTION_CLICK);

	pls_delete(productDialogView, nullptr);

	PLSProductType productType = getProductType();
	bool isMainProduct = productType == PLSProductType::MainProduct;
	PLSProductType otherType = isMainProduct ? PLSProductType::SubProduct : PLSProductType::MainProduct;
	productDialogView =
		pls_new<PLSNaverShoppingLIVEProductDialogView>(getPlatform(), productType, getProduct(productType), getProduct(otherType), isLiving, isPlanningLive, pls_get_toplevel_view(this));
	QString title = isMainProduct ? QTStr("NaverShoppingLive.LiveInfo.Live.Products") : QTStr("NaverShoppingLive.LiveInfo.Other.Products");
	productDialogView->setWindowTitle(title);
	connect(productDialogView, &PLSNaverShoppingLIVEProductDialogView::smartStoreChanged, this, &PLSLiveInfoNaverShoppingLIVEProductList::onSmartStoreChanged, Qt::DirectConnection);
	if (productDialogView->exec() != PLSNaverShoppingLIVEProductDialogView::Accepted) {
		return;
	}

	QList<qint64> selectedProductNos = productDialogView->getSelectedProductNos();
	std::sort(selectedProductNos.begin(), selectedProductNos.end());

	QList<qint64> currentProductNos = getProductNos(productType);
	std::sort(currentProductNos.begin(), currentProductNos.end());

	if (isEqual(currentProductNos, selectedProductNos)) {
		return;
	}

	QList<Product> fixedProds;
	QList<Product> unfixedProds;
	for (auto &selectedProduct : productDialogView->getSelectedProducts()) {
		selectedProduct.setAttachmentType(productType);
		auto fixedProductNos = getFixedOrUnfixedProductNos(productType, true);
		if (fixedProductNos.contains(selectedProduct.productNo)) {
			fixedProds.append(selectedProduct);
		} else {
			unfixedProds.append(selectedProduct);
		}
	}

	if (productType == PLSProductType::MainProduct && (getProductCount(productType) <= 0) && fixedProds.isEmpty() && !unfixedProds.isEmpty()) {
		auto product = unfixedProds.takeLast();
		product.represent = true;
		fixedProds.append(product);
	}

	// update
	setProduct(productType, fixedProds + unfixedProds);

	if (!fixedProds.isEmpty() || !unfixedProds.isEmpty()) {
		showPage(true, false, false, false);
		setAddButtonEnabled(false);

		showPage(true);
		PLSNaverShoppingLIVEDataManager::instance()->addRecentProductNos(productDialogView->getNewRecentProductNos());

		setProducts(productType, fixedProds, unfixedProds, false);
		setAddButtonEnabled(true);
		emit productChangedOrUpdated(true);
	} else {
		clearAll();
		emit productChangedOrUpdated(true);
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::on_noNetRetryButton_clicked()
{
	if (pls_get_network_state()) {
		auto productType = getProductType();
		auto fixedProducts = getFixedOrUnfixedProductNos(productType, true);
		auto unfixedProducts = getFixedOrUnfixedProductNos(productType, false);
		auto introducingNos = getProductNos(getProduct(productType), true);
		updateAllProductsInfo(productType, fixedProducts, unfixedProducts, introducingNos);
	}
}

void PLSLiveInfoNaverShoppingLIVEProductList::on_gotoNaverShoppingToolButton_clicked()
{
#if 0
	QDesktopServices::openUrl(CHANNEL_NAVER_SHOPPING_LIVE_LOGIN);
#endif

	on_ppAddButton_clicked();
}

bool PLSLiveInfoNaverShoppingLIVEProductList::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->hline2) {
		switch (event->type()) {
		case QEvent::Resize:
		case QEvent::Move:
			if (productLoadingView) {
				productLoadingView->updateViewGeometry();
			}
			break;
		default:
			break;
		}
	}

	return QWidget::eventFilter(watched, event);
}

PLSProductBtn::PLSProductBtn(QWidget *parent)
{
	QHBoxLayout *hLayout = pls_new<QHBoxLayout>(this);
	hLayout->setContentsMargins(0, 0, 0, 2);
	hLayout->setSpacing(6);
	hLayout->setAlignment(Qt::AlignCenter);

	titleLabel = pls_new<QLabel>(QTStr("NaverShoppingLive.LiveInfo.Live.Products"), this);
	titleLabel->setObjectName("proTitleLabel");

	countLabel = pls_new<QLabel>(QTStr("0/100"), this);
	countLabel->setObjectName("proCountLabel");

	hLayout->addWidget(titleLabel);
	hLayout->addWidget(countLabel);

	connect(this, &QPushButton::toggled, this, [this](bool checked) {
		pls_flush_style(titleLabel, "checked", checked);
		pls_flush_style(countLabel, "checked", checked);
	});
}

void PLSProductBtn::setTitle(const QString &title)
{
	titleLabel->setText(title);
}

void PLSProductBtn::setSelectedNumber(int count)
{
	countLabel->setText(QString("%1/100").arg(count));
}
