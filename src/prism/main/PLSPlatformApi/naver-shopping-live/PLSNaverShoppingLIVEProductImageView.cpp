#include "PLSNaverShoppingLIVEProductImageView.h"
#include "PLSNaverShoppingLIVEDataManager.h"
#include "PLSLiveInfoNaverShoppingLIVE.h"
#include "PLSNaverShoppingLIVEProductDialogView.h"

#include "PLSDpiHelper.h"
#include "log.h"

#include <QResizeEvent>
#include <QPainter>
#include <QDesktopServices>

PLSNaverShoppingLIVEProductImageView::PLSNaverShoppingLIVEProductImageView(QWidget *parent) : QLabel(parent)
{
	setMouseTracking(true);
}

PLSNaverShoppingLIVEProductImageView::~PLSNaverShoppingLIVEProductImageView() {}

void PLSNaverShoppingLIVEProductImageView::setImage(const QString &url, const QString &imageUrl, const QString &imagePath, bool hasDiscountIcon, bool isInLiveinfo)
{
	this->hasDiscountIcon = hasDiscountIcon;
	this->isInLiveinfo = isInLiveinfo;
	this->url = url;
	this->imageUrl = imageUrl;
	this->imagePath = imagePath;
	if (minimumWidth() > 1 && minimumHeight() > 1) {
		PLSNaverShoppingLIVEDataManager::instance()->getThumbnailPixmapAsync(this, imageUrl, imagePath, size(), PLSDpiHelper::getDpi(this));
	}
}

void PLSNaverShoppingLIVEProductImageView::setImage(const QString &url, const QPixmap &normalPixmap, const QPixmap &hoveredPixmap, bool hasDiscountIcon, bool isInLiveinfo)
{
	this->hasDiscountIcon = hasDiscountIcon;
	this->isInLiveinfo = isInLiveinfo;
	this->url = url;
	this->imageUrl.clear();
	this->imagePath.clear();
	this->normalPixmap = normalPixmap;
	this->hoveredPixmap = hoveredPixmap;
	this->update();
}

void PLSNaverShoppingLIVEProductImageView::mouseEnter()
{
	hovered = true;
	setCursor(Qt::PointingHandCursor);
	update();
}

void PLSNaverShoppingLIVEProductImageView::mouseLeave()
{
	hovered = false;
	setCursor(Qt::ArrowCursor);
	update();
}

bool PLSNaverShoppingLIVEProductImageView::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::MouseButtonRelease: {
		if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
			PLS_UI_STEP(isInLiveinfo ? MODULE_NAVER_SHOPPING_LIVE_LIVEINFO : MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Product Thumbnail", ACTION_CLICK);
			QDesktopServices::openUrl(url);
		}
		break;
	}
	}

	return QLabel::event(event);
}

void PLSNaverShoppingLIVEProductImageView::resizeEvent(QResizeEvent *event)
{
	QLabel::resizeEvent(event);

	if (minimumWidth() > 1 && minimumHeight() > 1) {
		PLSNaverShoppingLIVEDataManager::instance()->getThumbnailPixmapAsync(this, imageUrl, imagePath, event->size(), PLSDpiHelper::getDpi(this));
	}
}

void PLSNaverShoppingLIVEProductImageView::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::RenderHint::Antialiasing);
	painter.setRenderHint(QPainter::RenderHint::SmoothPixmapTransform);

	QRect rect = this->rect();
	painter.fillRect(rect, Qt::transparent);

	if (!hasDiscountIcon) {
		if (!normalPixmap.isNull() && !hoveredPixmap.isNull()) {
			if (!hovered) {
				painter.drawPixmap(rect, normalPixmap);
			} else {
				painter.drawPixmap(rect, hoveredPixmap);
			}
		} else {
			double dpi = PLSDpiHelper::getDpi(this);
			if (hovered) {
				double radius = dpi * 3;

				painter.setPen(Qt::NoPen);
				painter.setBrush(Qt::yellow);
				painter.drawRoundedRect(rect, radius, radius);

				rect.adjust(2, 2, -2, -2);
				painter.setBrush(QColor(30, 30, 30));
				painter.drawRoundedRect(rect, radius, radius);
			}

			QRect imageRect{QPoint(0, 0), PLSDpiHelper::calculate(dpi, QSize(24, 24))};
			imageRect.moveCenter(rect.center());
			PLSNaverShoppingLIVEDataManager::instance()->getDefaultImage().render(&painter, imageRect);
		}
	} else {
		if (!livePixmap.isNull() && !liveHoveredPixmap.isNull()) {
			if (!hovered) {
				painter.drawPixmap(rect, livePixmap);
			} else {
				painter.drawPixmap(rect, liveHoveredPixmap);
			}
		} else {
			double dpi = PLSDpiHelper::getDpi(this);
			if (hovered) {
				double radius = dpi * 3;

				painter.setPen(Qt::NoPen);
				painter.setBrush(Qt::yellow);
				painter.drawRoundedRect(rect, radius, radius);

				rect.adjust(2, 2, -2, -2);
				painter.setBrush(QColor(30, 30, 30));
				painter.drawRoundedRect(rect, radius, radius);
			}

			QRect imageRect{QPoint(0, 0), PLSDpiHelper::calculate(dpi, QSize(24, 24))};
			imageRect.moveCenter(rect.center());
			PLSNaverShoppingLIVEDataManager::instance()->getDefaultImage().render(&painter, imageRect);
		}
	}
}

void PLSNaverShoppingLIVEProductImageView::processFinished(bool, QThread *thread, const QString &imageUrl, const QPixmap &normalPixmap, const QPixmap &hoveredPixmap, const QPixmap &livePixmap,
							   const QPixmap &liveHoveredPixmap)
{
	if (this->imageUrl == imageUrl) {
		this->normalPixmap = normalPixmap;
		this->hoveredPixmap = hoveredPixmap;
		this->livePixmap = livePixmap;
		this->liveHoveredPixmap = liveHoveredPixmap;
		if (thread != this->thread()) {
			QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
		} else {
			update();
		}
	}
}
