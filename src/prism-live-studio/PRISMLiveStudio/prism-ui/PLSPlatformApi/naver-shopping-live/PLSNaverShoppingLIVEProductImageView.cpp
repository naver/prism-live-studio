#include "PLSNaverShoppingLIVEProductImageView.h"
#include "PLSNaverShoppingLIVEDataManager.h"
#include "PLSLiveInfoNaverShoppingLIVE.h"
#include "PLSNaverShoppingLIVEProductDialogView.h"
#include <QResizeEvent>
#include <QPainter>
#include <QDesktopServices>
#include "liblog.h"
#include "pls-shared-functions.h"

PLSNaverShoppingLIVEProductImageView::PLSNaverShoppingLIVEProductImageView(QWidget *parent) : QLabel(parent)
{
	QHBoxLayout *layoutInner = pls_new<QHBoxLayout>(this);
	layoutInner->setContentsMargins(0, 0, 0, 0);
	borderLabel = pls_new<QLabel>(this);
	borderLabel->setObjectName("borderLabel");
	layoutInner->addWidget(borderLabel);

	liveDiscountPixmap = pls_shared_paint_svg(":/resource/images/navershopping/badge_nshoppinglive.svg", QSize(27, 17) * 4);
	setMouseTracking(true);
}

void PLSNaverShoppingLIVEProductImageView::setImage(const QString &url_, const QString &imageUrl_, const QString &imagePath_, bool hasDiscountIcon_, bool isInLiveinfo_, double opacity)
{
	this->hasDiscountIcon = hasDiscountIcon_;
	this->isInLiveinfo = isInLiveinfo_;
	this->url = url_;
	this->imageUrl = imageUrl_;
	this->imagePath = imagePath_;
	this->opacity = opacity;
	if (minimumWidth() > 1 && minimumHeight() > 1) {
		PLSNaverShoppingLIVEDataManager::instance()->getThumbnailPixmapAsync(this, imageUrl, imagePath, size(), 1);
	}
}

void PLSNaverShoppingLIVEProductImageView::setImage(const QString &url_, const QPixmap &normalPixmap_, const QPixmap &hoveredPixmap_, bool hasDiscountIcon_, bool isInLiveinfo_, double opacity)
{
	this->hasDiscountIcon = hasDiscountIcon_;
	this->isInLiveinfo = isInLiveinfo_;
	this->url = url_;
	this->imageUrl.clear();
	this->imagePath.clear();
	this->normalPixmap = normalPixmap_;
	this->hoveredPixmap = hoveredPixmap_;
	this->opacity = opacity;
	this->update();
}

void PLSNaverShoppingLIVEProductImageView::mouseEnter()
{
	hovered = true;
	setCursor(Qt::PointingHandCursor);
	update();
	pls_flush_style(borderLabel, "enter", hasDiscountIcon);
}

void PLSNaverShoppingLIVEProductImageView::mouseLeave()
{
	hovered = false;
	setCursor(Qt::ArrowCursor);
	update();
	pls_flush_style(borderLabel, "enter", false);
}

bool PLSNaverShoppingLIVEProductImageView::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::MouseButtonRelease:
		if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
			PLS_UI_STEP(isInLiveinfo ? MODULE_NAVER_SHOPPING_LIVE_LIVEINFO : MODULE_NAVER_SHOPPING_LIVE_PRODUCT_MANAGER, "Product Thumbnail", ACTION_CLICK);
			QDesktopServices::openUrl(url);
		}
		break;
	default:
		break;
	}

	return QLabel::event(event);
}

void PLSNaverShoppingLIVEProductImageView::resizeEvent(QResizeEvent *event)
{
	QLabel::resizeEvent(event);

	if (minimumWidth() > 1 && minimumHeight() > 1) {
		PLSNaverShoppingLIVEDataManager::instance()->getThumbnailPixmapAsync(this, imageUrl, imagePath, event->size(), 1);
	}
}

void PLSNaverShoppingLIVEProductImageView::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::RenderHint::Antialiasing);
	painter.setRenderHint(QPainter::RenderHint::SmoothPixmapTransform);

	QRect rect = this->rect();
	painter.fillRect(rect, Qt::transparent);
	painter.setOpacity(opacity);
	if (!hasDiscountIcon) {
		if (!normalPixmap.isNull() && !hoveredPixmap.isNull()) {
			if (!hovered) {
				painter.drawPixmap(rect, normalPixmap);
			} else {
				painter.drawPixmap(rect, hoveredPixmap);
			}
		} else {
			double dpi = 1;
			if (hovered) {
				double radius = dpi * 3;

				painter.setPen(Qt::NoPen);
				painter.setBrush(Qt::yellow);
				painter.drawRoundedRect(rect, radius, radius);

				rect.adjust(2, 2, -2, -2);
				painter.setBrush(QColor(30, 30, 30));
				painter.drawRoundedRect(rect, radius, radius);
			}

			QRect imageRect{QPoint(0, 0), QSize(24, 24)};
			imageRect.moveCenter(rect.center());
			PLSNaverShoppingLIVEDataManager::instance()->getDefaultImage().render(&painter, imageRect);
		}
	} else {
		if (!normalPixmap.isNull()) {
			QPixmap p = liveDiscountPixmap.scaled(QSize(27, 17) * devicePixelRatioF(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			p.setDevicePixelRatio(devicePixelRatioF());
			painter.drawPixmap(rect, normalPixmap);
			painter.drawPixmap(QRect(0, 0, 27, 17), p);
		} else {
			double dpi = 1;
			if (hovered) {
				double radius = dpi * 3;

				painter.setPen(Qt::NoPen);
				painter.setBrush(Qt::yellow);
				painter.drawRoundedRect(rect, radius, radius);

				rect.adjust(2, 2, -2, -2);
				painter.setBrush(QColor(30, 30, 30));
				painter.drawRoundedRect(rect, radius, radius);
			}

			QRect imageRect{QPoint(0, 0), QSize(24, 24)};
			imageRect.moveCenter(rect.center());
			PLSNaverShoppingLIVEDataManager::instance()->getDefaultImage().render(&painter, imageRect);
		}
	}
}

void PLSNaverShoppingLIVEProductImageView::processFinished(bool, QThread *thread, const QString &imageUrl_, const QPixmap &normalPixmap_, const QPixmap &hoveredPixmap_, const QPixmap &livePixmap_,
							   const QPixmap &liveHoveredPixmap_)
{
	if (this->imageUrl == imageUrl_) {
		this->normalPixmap = normalPixmap_;
		this->hoveredPixmap = hoveredPixmap_;
		this->livePixmap = livePixmap_;
		this->liveHoveredPixmap = liveHoveredPixmap_;
		if (thread != this->thread()) {
			QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
		} else {
			update();
		}
	}
}
