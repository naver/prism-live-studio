#include "PLSLiveInfoNaverShoppingLIVEProductItemView.h"
#include "ui_PLSLiveInfoNaverShoppingLIVEProductItemView.h"
#include "PLSNaverShoppingLIVEItemViewCache.h"
#include "PLSNaverShoppingLIVEDataManager.h"
#include "PLSLiveInfoNaverShoppingLIVE.h"
#include "PLSNaverShoppingLIVEProductDialogView.h"
#include "PLSLoadingView.h"
#include "frontend-api.h"

#include <QDesktopServices>

static PLSNaverShoppingLIVEItemViewCache<PLSLiveInfoNaverShoppingLIVEProductItemView> itemViewCache;

void naverShoppingLIVEProductItemView_init(QWidget *itemView, QLabel *title)
{
	title->setMouseTracking(itemView);
	title->installEventFilter(itemView);
}
void naverShoppingLIVEProductItemView_event(QWidget *, QWidget *, QEvent *event, const QString &url, bool isInLiveinfo)
{
	switch (event->type()) {
	case QEvent::MouseButtonRelease: {
		if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
			PLS_UI_STEP(isInLiveinfo ? MODULE_NAVER_SHOPPING_LIVE_LIVEINFO : MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Product Title", ACTION_CLICK);
			QDesktopServices::openUrl(url);
		}
		break;
	}
	}
}
void naverShoppingLIVEProductItemView_title_mouseEnter(QWidget *widget)
{
	widget->setCursor(Qt::PointingHandCursor);
	pls_flush_style(widget);
}
void naverShoppingLIVEProductItemView_title_mouseLeave(QWidget *widget)
{
	widget->setCursor(Qt::ArrowCursor);
	pls_flush_style(widget);
}

PLSLiveInfoNaverShoppingLIVEProductItemView::PLSLiveInfoNaverShoppingLIVEProductItemView(QWidget *parent) : QWidget(parent), ui(new Ui::PLSLiveInfoNaverShoppingLIVEProductItemView)
{
	setAttribute(Qt::WA_Hover);
	setMouseTracking(true);
	ui->setupUi(this);
	naverShoppingLIVEProductItemView_init(this, ui->titleLabel);
	ui->titleSpacer->installEventFilter(this);
	ui->fixButton->setToolTip(tr("NaverShoppingLive.LiveInfo.FixButton.Tooltip"));
	ui->removeButton->setToolTip(tr("Delete"));
	connect(ui->fixButton, &QPushButton::clicked, this, [this]() { fixButtonClicked(this); });
	connect(ui->removeButton, &QPushButton::clicked, this, [this]() { removeButtonClicked(this); });
}

PLSLiveInfoNaverShoppingLIVEProductItemView::~PLSLiveInfoNaverShoppingLIVEProductItemView()
{
	PLSLoadingView::deleteLoadingView(imageLoadingView);
	delete ui;
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::setInfo(PLSPlatformNaverShoppingLIVE *platform, const Product &product, bool fixed)
{
	this->product = product;
	setFixed(fixed);

	ui->titleLabel->setText(product.name);
	ui->storeLabel->setText(product.mallName);
	if (product.productStatus == PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_OUTOFSTOCK) {
		ui->statusLabel->setText(tr("NaverShoppingLive.LiveInfo.Product.Status.Outofstock"));
		ui->discountLabel->hide();
		ui->priceLabel->hide();
		ui->discountPriceLabel->hide();
		ui->discountIconLabel->hide();
		ui->statusLabel->show();
	} else if (product.productStatus != PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_SALE) {
		ui->statusLabel->setText(tr("NaverShoppingLive.LiveInfo.Product.Status.Other"));
		ui->discountLabel->hide();
		ui->priceLabel->hide();
		ui->discountPriceLabel->hide();
		ui->discountIconLabel->hide();
		ui->statusLabel->show();
	} else if (product.specialPriceIsValid()) { // discount type 2
		ui->discountLabel->setText(PLSNaverShoppingLIVEDataManager::convertRate(product.discountRate));
		ui->priceLabel->setText(tr("NaverShoppingLive.ProductPrice").arg(PLSNaverShoppingLIVEDataManager::convertPrice(product.price)));
		ui->discountPriceLabel->setText(tr("NaverShoppingLive.ProductDiscountPrice").arg(PLSNaverShoppingLIVEDataManager::convertPrice(product.specialPrice)));
		ui->statusLabel->hide();
		ui->discountLabel->setVisible(product.discountRateIsValid());
		ui->priceLabel->show();
		ui->discountIconLabel->show();
		ui->discountPriceLabel->show();
	} else if (product.discountRateIsValid()) { // discount type 1
		ui->discountLabel->setText(PLSNaverShoppingLIVEDataManager::convertRate(product.discountRate));
		ui->priceLabel->setText(tr("NaverShoppingLive.ProductPrice").arg(PLSNaverShoppingLIVEDataManager::convertPrice(product.price)));
		ui->statusLabel->hide();
		ui->discountPriceLabel->hide();
		ui->discountLabel->show();
		ui->priceLabel->show();
	} else {
		ui->priceLabel->setText(tr("NaverShoppingLive.ProductPrice").arg(PLSNaverShoppingLIVEDataManager::convertPrice(product.price)));
		ui->statusLabel->hide();
		ui->discountLabel->hide();
		ui->discountPriceLabel->hide();
		ui->discountIconLabel->show();
		ui->priceLabel->show();
	}

	PLSNaverShoppingLIVEDataManager::instance()->downloadImage(
		platform, product.imageUrl,
		[this, _itemId = this->itemId]() {
			if (_itemId != this->itemId) {
				return;
			}

			PLSLoadingView::newLoadingView(imageLoadingView, ui->iconLabel);
		},
		[this, specialPriceIsValid = product.specialPriceIsValid(), _itemId = this->itemId](bool ok, const QString &imagePath) {
			if (_itemId != this->itemId) {
				return;
			}

			PLSLoadingView::deleteLoadingView(imageLoadingView);

			if (ok) {
				ui->iconLabel->setImage(this->product.linkUrl, this->product.imageUrl, imagePath, specialPriceIsValid, true);
			} else {
				ui->iconLabel->setImage(this->product.linkUrl, QPixmap(), QPixmap(), specialPriceIsValid, true);
			}
		},
		this, nullptr, -1);
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::setProductReadonly(bool readonly)
{
	ui->fixButton->setEnabled(!readonly);
	ui->removeButton->setEnabled(!readonly);

	ui->fixButton->setVisible(!readonly);
	ui->removeButton->setVisible(!readonly);
}

bool PLSLiveInfoNaverShoppingLIVEProductItemView::isFixed() const
{
	return fixed;
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::setFixed(bool fixed)
{
	this->fixed = fixed;
	pls_flush_style(this->ui->fixButton, "fixed", fixed);
}

qint64 PLSLiveInfoNaverShoppingLIVEProductItemView::getProductNo() const
{
	return product.productNo;
}

const PLSLiveInfoNaverShoppingLIVEProductItemView::Product &PLSLiveInfoNaverShoppingLIVEProductItemView::getProduct() const
{
	return product;
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::addBatchCache(int batchCount, bool check)
{
	itemViewCache.addBatchCache(batchCount, check);
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::cleaupCache()
{
	itemViewCache.cleaupCache();
}

PLSLiveInfoNaverShoppingLIVEProductItemView *PLSLiveInfoNaverShoppingLIVEProductItemView::alloc(QWidget *parent, PLSPlatformNaverShoppingLIVE *platform, const Product &product, bool fixed)
{
	auto view = itemViewCache.alloc(parent, platform, product, fixed);
	view->ui->iconLabel->setFixedSize(1, 1);
	return view;
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::dealloc(PLSLiveInfoNaverShoppingLIVEProductItemView *view)
{
	QObject::disconnect(view->fixButtonClickedConnection);
	QObject::disconnect(view->removeButtonClickedConnection);
	itemViewCache.dealloc(view);
	view->ui->iconLabel->setFixedSize(1, 1);
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::dealloc(QList<PLSLiveInfoNaverShoppingLIVEProductItemView *> &views, QVBoxLayout *layout)
{
	itemViewCache.dealloc(views, layout, [](PLSLiveInfoNaverShoppingLIVEProductItemView *view) {
		QObject::disconnect(view->fixButtonClickedConnection);
		QObject::disconnect(view->removeButtonClickedConnection);
		view->ui->iconLabel->setFixedSize(1, 1);
	});
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::resetIconSize(QList<PLSLiveInfoNaverShoppingLIVEProductItemView *> &views)
{
	for (auto view : views) {
		view->ui->iconLabel->setFixedSize(1, 1);
	}
}

void PLSLiveInfoNaverShoppingLIVEProductItemView::updateWhenDpiChanged(QList<PLSLiveInfoNaverShoppingLIVEProductItemView *> &views)
{
	for (auto view : views) {
		view->ui->titleLabel->setText(view->product.name);
	}
}

bool PLSLiveInfoNaverShoppingLIVEProductItemView::event(QEvent *event)
{
	bool result = QWidget::event(event);
	switch (event->type()) {
	case QEvent::HoverEnter: {
		setProperty("state", "hover");
		naverShoppingLIVEProductItemView_title_mouseEnter(ui->titleLabel);
		ui->iconLabel->mouseEnter();
		break;
	}
	case QEvent::HoverLeave: {
		setProperty("state", "none");
		naverShoppingLIVEProductItemView_title_mouseLeave(ui->titleLabel);
		ui->iconLabel->mouseLeave();
		break;
	}
	default:
		break;
	}
	return result;
}

bool PLSLiveInfoNaverShoppingLIVEProductItemView::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->titleLabel) {
		naverShoppingLIVEProductItemView_event(this, static_cast<QWidget *>(watched), event, product.linkUrl, true);
	} else if (watched == ui->titleSpacer) {
		switch (event->type()) {
		case QEvent::Resize: {
			if (static_cast<QResizeEvent *>(event)->size().width() == 0) {
				QString text = ui->titleLabel->fontMetrics().elidedText(product.name, Qt::ElideRight, ui->titleWidget->width());
				ui->titleLabel->setText(text);
			}
			break;
		}
		}
	}

	return QWidget::eventFilter(watched, event);
}
