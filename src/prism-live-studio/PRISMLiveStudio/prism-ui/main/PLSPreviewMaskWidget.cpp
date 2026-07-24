#include "PLSPreviewMaskWidget.h"
#include "liblog.h"

#include <QPainter>
#include <QResizeEvent>
#include <QVBoxLayout>
#include "PLSMainView.hpp"

PLSPreviewMaskWidget::PLSPreviewMaskWidget(OBSQTDisplay *display, QWidget *parent) : QWidget(parent), m_display(display)
{
	initUi();
}

PLSPreviewMaskWidget::PLSPreviewMaskWidget(QWidget *parent) : QWidget(parent)
{
	initUi();
}

void PLSPreviewMaskWidget::setUpdatePreviewRectCallback(std::function<void()> &&callback)
{
	m_updatePreviewRectCallback = std::move(callback);
}

void PLSPreviewMaskWidget::setPreviewRect(qreal topLeftX, qreal topLeftY, qreal width, qreal height)
{
	setPreviewRect({topLeftX, topLeftY, width, height});
}

void PLSPreviewMaskWidget::setPreviewRect(const QRectF &rect)
{
	m_rect = rect;
}

void PLSPreviewMaskWidget::setBgColor(const QColor &color)
{
	m_bgColor = color;
	update();
}

void PLSPreviewMaskWidget::setRenderColor(const QColor &color)
{
	m_renderColor = color;
	update();
}

void PLSPreviewMaskWidget::paintEvent(QPaintEvent *event)
{
	pls_invoke_safe(m_updatePreviewRectCallback);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.fillRect(rect(), m_bgColor);
	painter.fillRect(m_rect, m_renderColor);
}

void PLSPreviewMaskWidget::resizeEvent(QResizeEvent *event)
{
	if (m_display && !m_display->isVisible()) {
		m_display->resize(event->size());
		m_display->ResizeDisplay();
	}

	QWidget::resizeEvent(event);
}

void PLSPreviewMaskWidget::initUi()
{
	setAttribute(Qt::WA_NativeWindow);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	if (m_display) {
		auto layout = new QVBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		layout->addWidget(m_display);
#ifdef Q_OS_WIN
		if (!PLSMainView::instance()->isVisible()) {
			m_display->hide();
			connect(
				PLSMainView::instance(), &PLSMainView::isshowSignal, this,
				[this](bool show) {
					pls_check_app_exiting();
					if (show) {
						QPointer<OBSQTDisplay> display = m_display;
						QTimer::singleShot(50, [display]() {
							pls_check_app_exiting();
							if (display)
								display->show();
						});
					}
				},
				Qt::ConnectionType(Qt::SingleShotConnection | Qt::QueuedConnection));
		}
#endif // Q_OS_WIN
	}
}