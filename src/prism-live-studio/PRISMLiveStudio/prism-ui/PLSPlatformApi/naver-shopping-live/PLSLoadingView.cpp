#include "PLSLoadingView.h"
#include "pls-common-define.hpp"
#include "utils-api.h"
#include <QPainter>
#include <QResizeEvent>
#include <QTimerEvent>

using namespace common;
PLSLoadingView::PLSLoadingView(QWidget *parent, QString pathImage) : QFrame(parent)
{
	if (pathImage.isEmpty()) {
		pathImage = QString(":resource/images/loading/loading-%1.svg");
	}

	for (int i = 0; i < 8; ++i) {
		m_svgRenderers[i].load(pathImage.arg(i + 1));
	}

	m_dpi = 1;
	m_timer = this->startTimer(LOADING_TIMER_TIMEROUT);
	setFocusPolicy(Qt::NoFocus);
}

PLSLoadingView::~PLSLoadingView()
{
	setAutoResizeFrom(nullptr);
	killTimer(m_timer);
}

PLSLoadingView *PLSLoadingView::newLoadingView(QWidget *parent, const PfnGetViewRect &getViewRect)
{
	return newLoadingView(parent, -1, getViewRect);
}

PLSLoadingView *PLSLoadingView::newLoadingView(PLSLoadingView *&loadingView, QWidget *parent, const PfnGetViewRect &getViewRect)
{
	return newLoadingView(loadingView, parent, -1, getViewRect);
}

PLSLoadingView *PLSLoadingView::newLoadingView(QWidget *parent, int absoluteTop, const PfnGetViewRect &getViewRect, const QString &pathImage, std::optional<QColor> colorBackground)
{
	PLSLoadingView *loadingView = pls_new<PLSLoadingView>(parent, pathImage);
	loadingView->m_colorBackground = colorBackground;
	loadingView->m_absoluteTop = absoluteTop;
	loadingView->m_getViewRect = getViewRect;
	loadingView->raise();
	loadingView->setAutoResizeFrom(parent);
	loadingView->show();
	return loadingView;
}

PLSLoadingView *PLSLoadingView::newLoadingView(PLSLoadingView *&loadingView, QWidget *parent, int absoluteTop, const PfnGetViewRect &getViewRect)
{
	deleteLoadingView(loadingView);
	loadingView = newLoadingView(parent, absoluteTop, getViewRect);
	return loadingView;
}

PLSLoadingView *PLSLoadingView::newLoadingView(PLSLoadingView *&loadingView, bool condition, QWidget *parent, int absoluteTop, const PfnGetViewRect &getViewRect)
{
	return condition ? newLoadingView(loadingView, parent, absoluteTop, getViewRect) : nullptr;
}

void PLSLoadingView::deleteLoadingView(PLSLoadingView *&loadingView)
{
	if (loadingView) {
		PLSLoadingView *_loadingView = loadingView;
		loadingView = nullptr;

		_loadingView->hide();
		pls_delete(_loadingView);
	}
}

void PLSLoadingView::setAutoResizeFrom(QWidget *autoResizeFrom)
{
	if (m_autoResizeFrom == autoResizeFrom) {
		return;
	}

	if (m_autoResizeFrom) {
		m_autoResizeFrom->removeEventFilter(this);
	}

	m_autoResizeFrom = autoResizeFrom;

	if (m_autoResizeFrom) {
		m_autoResizeFrom->installEventFilter(this);
		setSuggestedGeometry(m_autoResizeFrom->rect());
	}
}

void PLSLoadingView::setAbsoluteTop(int absoluteTop)
{
	m_absoluteTop = absoluteTop;
}

void PLSLoadingView::updateViewGeometry()
{
	if (m_autoResizeFrom) {
		setSuggestedGeometry(m_autoResizeFrom->rect());
	}
}

void PLSLoadingView::setSuggestedGeometry(const QRect &suggested)
{
	QRect geometry;
	if (m_getViewRect && m_getViewRect(geometry, this)) {
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
	auto minWidth = static_cast<int>(m_dpi * 24);
	auto minHeight = static_cast<int>(m_dpi * (24 + m_absoluteTop + 1));
	return QSize(size.width() > minWidth ? size.width() : minWidth, size.height() > minHeight ? size.height() : minHeight);
}

void PLSLoadingView::timerEvent(QTimerEvent *event)
{
	if (event->timerId() == m_timer) {
		m_curPixmap = (m_curPixmap + 1) % 8;
		update();
	}
}

void PLSLoadingView::resizeEvent(QResizeEvent *event)
{
	m_dpi = 1;
	QFrame::resizeEvent(event);
}

void PLSLoadingView::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	drawFrame(&painter);

	painter.setRenderHint(QPainter::RenderHint::Antialiasing);
	painter.setRenderHint(QPainter::RenderHint::SmoothPixmapTransform);

	QRect rect = this->rect();
	if (!m_colorBackground.has_value()) {
		painter.fillRect(rect, Qt::transparent);
	} else {
		painter.fillRect(rect, m_colorBackground.value());
	}

	QRectF svgRect(0, 0, 24 * m_dpi, 24 * m_dpi);
	svgRect.moveCenter(rect.center());

	if (m_absoluteTop >= 0) {
		svgRect.moveTop(m_dpi * m_absoluteTop);
	}

	m_svgRenderers[m_curPixmap].render(&painter, svgRect);
}

bool PLSLoadingView::eventFilter(QObject *watched, QEvent *event)
{
	if ((watched == m_autoResizeFrom) && (event->type() == QEvent::Resize)) {
		setSuggestedGeometry(QPoint(0, 0), static_cast<QResizeEvent *>(event)->size());
	}

	return QFrame::eventFilter(watched, event);
}
