#include "PLSNaverShoppingLIVEProductItemView.h"
#include "ui_PLSNaverShoppingLIVEProductItemView.h"
#include "PLSNaverShoppingLIVEAPI.h"
#include "PLSNaverShoppingLIVEItemViewCache.h"
#include "PLSNaverShoppingLIVEDataManager.h"
#include "PLSLoadingView.h"
#include "frontend-api.h"
#include "utils-api.h"
#include "libui.h"
#include <QMouseEvent>
#include <QDesktopServices>

using ItemViewCache = PLSNaverShoppingLIVEItemViewCache<PLSNaverShoppingLIVEProductItemView>;

void naverShoppingLIVEProductItemView_init(QWidget *itemView, QLabel *title);
void naverShoppingLIVEProductItemView_event(const QWidget *itemView, const QWidget *widget, QEvent *event, const QString &url, bool isInLiveinfo);
void naverShoppingLIVEProductItemView_title_mouseEnter(QWidget *widget);
void naverShoppingLIVEProductItemView_title_mouseLeave(QWidget *widget);

PLSNaverShoppingLIVEProductItemView::PLSNaverShoppingLIVEProductItemView(QWidget *parent) : QFrame(parent)
{
	ui = pls_new<Ui::PLSNaverShoppingLIVEProductItemView>();
	setAttribute(Qt::WA_Hover);
	setMouseTracking(true);
	ui->setupUi(this);
	naverShoppingLIVEProductItemView_init(this, ui->nameLabel);
	ui->nameSpacer->installEventFilter(this);
	connect(ui->addRemoveButton, &QPushButton::clicked, this, [this]() { PLSNaverShoppingLIVEProductItemView::addRemoveButtonClicked(this, details.productNo); });
}

PLSNaverShoppingLIVEProductItemView::~PLSNaverShoppingLIVEProductItemView()
{
	PLSLoadingView::deleteLoadingView(imageLoadingView);
	pls_delete(ui, nullptr);
}

qint64 PLSNaverShoppingLIVEProductItemView::getProductNo() const
{
	return details.productNo;
}

QString PLSNaverShoppingLIVEProductItemView::getUrl() const
{
	return details.linkUrl;
}

QString PLSNaverShoppingLIVEProductItemView::getImageUrl() const
{
	return details.imageUrl;
}

QString PLSNaverShoppingLIVEProductItemView::getName() const
{
	return details.name;
}

double PLSNaverShoppingLIVEProductItemView::getPrice() const
{
	return details.price;
}

QString PLSNaverShoppingLIVEProductItemView::getStoreName() const
{
	return details.mallName;
}

PLSNaverShoppingLIVEProductItemView::Details PLSNaverShoppingLIVEProductItemView::getDetails() const
{
	return details;
}

void PLSNaverShoppingLIVEProductItemView::setInfo(const PLSPlatformNaverShoppingLIVE *platform, const Details &details_)
{
	this->details = details_;

	ui->nameLabel->setText(details.name);
	ui->storeLabel->setText(details.mallName);
	if (details.productStatus == PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_OUTOFSTOCK) {
		ui->statusLabel->setText(tr("NaverShoppingLive.LiveInfo.Product.Status.Outofstock"));
		ui->discountLabel->hide();
		ui->priceLabel->hide();
		ui->discountPriceLabel->hide();
		ui->statusLabel->show();
	} else if (details.productStatus != PLSNaverShoppingLIVEDataManager::PRODUCT_STATUS_SALE) {
		ui->statusLabel->setText(tr("NaverShoppingLive.LiveInfo.Product.Status.Other"));
		ui->discountLabel->hide();
		ui->priceLabel->hide();
		ui->discountPriceLabel->hide();
		ui->statusLabel->show();
	} else if (details.specialPriceIsValid()) { // discount type 2
		ui->discountLabel->setText(PLSNaverShoppingLIVEDataManager::convertRate(details.discountRate));
		ui->priceLabel->setText(tr("NaverShoppingLive.ProductPrice").arg(PLSNaverShoppingLIVEDataManager::convertPrice(details.price)));
		ui->discountPriceLabel->setText(tr("NaverShoppingLive.ProductDiscountPrice").arg(PLSNaverShoppingLIVEDataManager::convertPrice(details.specialPrice)));
		ui->statusLabel->hide();
		ui->discountLabel->setVisible(details.discountRateIsValid());
		ui->priceLabel->show();
		ui->discountPriceLabel->show();
	} else if (details.discountRateIsValid()) { // discount type 1
		ui->discountLabel->setText(PLSNaverShoppingLIVEDataManager::convertRate(details.discountRate));
		ui->priceLabel->setText(tr("NaverShoppingLive.ProductPrice").arg(PLSNaverShoppingLIVEDataManager::convertPrice(details.price)));
		ui->statusLabel->hide();
		ui->discountPriceLabel->hide();
		ui->discountLabel->show();
		ui->priceLabel->show();
	} else {
		ui->priceLabel->setText(tr("NaverShoppingLive.ProductPrice").arg(PLSNaverShoppingLIVEDataManager::convertPrice(details.price)));
		ui->statusLabel->hide();
		ui->discountLabel->hide();
		ui->discountPriceLabel->hide();
		ui->priceLabel->show();
	}

	PLSNaverShoppingLIVEDataManager::instance()->downloadImage(
		platform, details.imageUrl,
		[this, _itemId = this->getItemId()]() {
			if (_itemId != this->getItemId()) {
				return;
			}

			PLSLoadingView::newLoadingView(imageLoadingView, ui->iconLabel);
		},
		[this, specialPriceIsValid = details.specialPriceIsValid(), _itemId = this->getItemId()](bool ok, const QString &imagePath) {
			if (_itemId != this->getItemId()) {
				return;
			}

			PLSLoadingView::deleteLoadingView(imageLoadingView);

			if (ok) {
				ui->iconLabel->setImage(this->details.linkUrl, this->details.imageUrl, imagePath, specialPriceIsValid, false);
			} else {
				ui->iconLabel->setImage(this->details.linkUrl, QPixmap(), QPixmap(), specialPriceIsValid, false);
			}
		},
		this, nullptr, -1);
}

void PLSNaverShoppingLIVEProductItemView::setInfo(const PLSPlatformNaverShoppingLIVE *platform, const PLSNaverShoppingLIVEProductItemView *srcItemView)
{
	setInfo(platform, srcItemView->getDetails());
}

void PLSNaverShoppingLIVEProductItemView::setSelected(bool selected)
{
	pls_flush_style(ui->addRemoveButton, "added", selected);
}

void PLSNaverShoppingLIVEProductItemView::setStoreNameVisible(bool visible)
{
	ui->storeLabel->setVisible(visible);
}

void PLSNaverShoppingLIVEProductItemView::addBatchCache(int batchCount, bool check)
{
	ItemViewCache::instance()->addBatchCache(batchCount, check);
}

void PLSNaverShoppingLIVEProductItemView::cleaupCache()
{
	ItemViewCache::instance()->cleaupCache();
}

PLSNaverShoppingLIVEProductItemView *PLSNaverShoppingLIVEProductItemView::alloc(QWidget *parent, PLSPlatformNaverShoppingLIVE *platform, const Details &details)
{
	auto view = ItemViewCache::instance()->alloc(parent, platform, details);
	view->ui->iconLabel->setFixedSize(1, 1);
	return view;
}

void PLSNaverShoppingLIVEProductItemView::dealloc(PLSNaverShoppingLIVEProductItemView *view)
{
	QObject::disconnect(view->addRemoveButtonClickedConnection);
	ItemViewCache::instance()->dealloc(view);
	view->ui->iconLabel->setFixedSize(1, 1);
}

void PLSNaverShoppingLIVEProductItemView::dealloc(QList<PLSNaverShoppingLIVEProductItemView *> &views, QVBoxLayout *layout)
{
	ItemViewCache::instance()->dealloc(views, layout, [](PLSNaverShoppingLIVEProductItemView *view) {
		QObject::disconnect(view->addRemoveButtonClickedConnection);
		view->ui->iconLabel->setFixedSize(1, 1);
	});
}

void PLSNaverShoppingLIVEProductItemView::resetIconSize(const QList<PLSNaverShoppingLIVEProductItemView *> &views)
{
	for (auto view : views) {
		view->ui->iconLabel->setFixedSize(1, 1);
	}
}

void PLSNaverShoppingLIVEProductItemView::updateWhenDpiChanged(const QList<PLSNaverShoppingLIVEProductItemView *> &views)
{
	for (auto view : views) {
		view->ui->nameLabel->setText(view->details.name);
	}
}

bool PLSNaverShoppingLIVEProductItemView::event(QEvent *event)
{
	bool result = QFrame::event(event);
	switch (event->type()) {
	case QEvent::HoverEnter:
		setProperty("state", "hover");
		naverShoppingLIVEProductItemView_title_mouseEnter(ui->nameLabel);
		ui->iconLabel->mouseEnter();
		break;
	case QEvent::HoverLeave:
		setProperty("state", "none");
		naverShoppingLIVEProductItemView_title_mouseLeave(ui->nameLabel);
		ui->iconLabel->mouseLeave();
		break;
	default:
		break;
	}
	return result;
}

bool PLSNaverShoppingLIVEProductItemView::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->nameLabel) {
		naverShoppingLIVEProductItemView_event(this, static_cast<QWidget *>(watched), event, details.linkUrl, false);
	} else if (watched == ui->nameSpacer) {
		switch (event->type()) {
		case QEvent::Resize:
			if (static_cast<QResizeEvent *>(event)->size().width() == 0) {
				QString text = ui->nameLabel->fontMetrics().elidedText(details.name, Qt::ElideRight, ui->nameWidget->width());
				ui->nameLabel->setText(text);
			}
			break;
		default:
			break;
		}
	}

	return QFrame::eventFilter(watched, event);
}
