#include "PLSLoadingView.h"

#include "PLSDpiHelper.h"
#include "pls-common-define.hpp"

#include <QPainter>
#include <QResizeEvent>
#include <QTimerEvent>

PLSLoadingView::PLSLoadingView(QWidget *parent) : QFrame(parent)
{
	for (int i = 0; i < 8; ++i) {
		svgRenderers[i].load(QString(":/images/loading-%1.svg").arg(i + 1));
	}

	dpi = PLSDpiHelper::getDpi(this);
	timer = this->startTimer(LOADING_TIMER_TIMEROUT);
	setFocusPolicy(Qt::NoFocus);
}

PLSLoadingView::~PLSLoadingView()
{
	setAutoResizeFrom(nullptr);
	killTimer(timer);
}

PLSLoadingView *PLSLoadingView::newLoadingView(QWidget *parent, PfnGetViewRect &&getViewRect)
{
	return newLoadingView(parent, -1, std::forward<PfnGetViewRect>(getViewRect));
}

PLSLoadingView *PLSLoadingView::newLoadingView(PLSLoadingView *&loadingView, QWidget *parent, PfnGetViewRect &&getViewRect)
{
	return newLoadingView(loadingView, parent, -1, std::forward<PfnGetViewRect>(getViewRect));
}

PLSLoadingView *PLSLoadingView::newLoadingView(QWidget *parent, int absoluteTop, PfnGetViewRect &&getViewRect)
{
	PLSLoadingView *loadingView = new PLSLoadingView(parent);
	loadingView->absoluteTop = absoluteTop;
	loadingView->getViewRect = std::move(getViewRect);
	loadingView->raise();
	loadingView->setAutoResizeFrom(parent);
	loadingView->show();
	return loadingView;
}

PLSLoadingView *PLSLoadingView::newLoadingView(PLSLoadingView *&loadingView, QWidget *parent, int absoluteTop, PfnGetViewRect &&getViewRect)
{
	deleteLoadingView(loadingView);
	loadingView = newLoadingView(parent, absoluteTop, std::forward<PfnGetViewRect>(getViewRect));
	return loadingView;
}

PLSLoadingView *PLSLoadingView::newLoadingView(PLSLoadingView *&loadingView, bool condition, QWidget *parent, int absoluteTop, PfnGetViewRect &&getViewRect)
{
	return condition ? newLoadingView(loadingView, parent, absoluteTop, std::forward<PfnGetViewRect>(getViewRect)) : nullptr;
}

void PLSLoadingView::deleteLoadingView(PLSLoadingView *&loadingView)
{
	if (loadingView) {
		PLSLoadingView *_loadingView = loadingView;
		loadingView = nullptr;

		_loadingView->hide();
		delete _loadingView;
	}
}

void PLSLoadingView::setAutoResizeFrom(QWidget *autoResizeFrom)
{
	if (this->autoResizeFrom == autoResizeFrom) {
		return;
	}

	if (this->autoResizeFrom) {
		this->autoResizeFrom->removeEventFilter(this);
	}

	this->autoResizeFrom = autoResizeFrom;

	if (this->autoResizeFrom) {
		this->autoResizeFrom->installEventFilter(this);
		setSuggestedGeometry(autoResizeFrom->rect());
	}
}

void PLSLoadingView::setAbsoluteTop(int absoluteTop)
{
	this->absoluteTop = absoluteTop;
}

void PLSLoadingView::updateGeometry()
{
	if (this->autoResizeFrom) {
		setSuggestedGeometry(autoResizeFrom->rect());
	}
}

void PLSLoadingView::setSuggestedGeometry(const QRect &suggested)
{
	QRect geometry;
	if (getViewRect && getViewRect(geometry, this)) {
		setGeometry(QRect(geometry.topLeft(), calcSuggestedSize(geometry.size())));
	} else {
		setGeometry(QRect(suggested.topLeft(), calcSuggestedSize(suggested.size())));
	}
}

void PLSLoadingView::setSuggestedGeometry(const QPoint &position, const QSize &size)
{
	setSuggestedGeometry(QRect(position, size));
}

QSize PLSLoadingView::calcSuggestedSize(const QSize &size) const
{
	int minWidth = dpi * 24;
	int minHeight = dpi * (24 + absoluteTop + 1);
	return QSize(size.width() > minWidth ? size.width() : minWidth, size.height() > minHeight ? size.height() : minHeight);
}

void PLSLoadingView::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == timer) {
		curPixmap = (curPixmap + 1) % 8;
		update();
	}
}

void PLSLoadingView::resizeEvent(QResizeEvent *event)
{
	dpi = PLSDpiHelper::getDpi(this);
	QFrame::resizeEvent(event);
}

void PLSLoadingView::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	drawFrame(&painter);

	painter.setRenderHint(QPainter::RenderHint::Antialiasing);
	painter.setRenderHint(QPainter::RenderHint::SmoothPixmapTransform);

	QRect rect = this->rect();
	painter.fillRect(rect, Qt::transparent);

	QRectF svgRect(0, 0, 24 * dpi, 24 * dpi);
	svgRect.moveCenter(rect.center());

	if (absoluteTop >= 0) {
		svgRect.moveTop(dpi * absoluteTop);
	}

	svgRenderers[curPixmap].render(&painter, svgRect);
}

bool PLSLoadingView::eventFilter(QObject *watched, QEvent *event)
{
	if ((watched == autoResizeFrom) && (event->type() == QEvent::Resize)) {
		setSuggestedGeometry(QPoint(0, 0), static_cast<QResizeEvent *>(event)->size());
	}

	return QFrame::eventFilter(watched, event);
}
