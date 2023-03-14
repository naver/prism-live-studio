#include "PLSMotionItemView.h"
#include "ui_PLSMotionItemView.h"

#include "PLSMotionFileManager.h"
#include "PLSMotionNetwork.h"
#include "PLSDpiHelper.h"
#include "PLSWidgetDpiAdapter.hpp"

#include <qapplication.h>
#include <qthread.h>
#include <qpainter.h>

static QList<PLSMotionItemView *> motionItemViewCache;
static QList<QPair<PLSMotionItemView *, PLSMotionItemView *>> motionItemViewBatchs;

static bool isOfBatch(PLSMotionItemView *view)
{
	for (const QPair<PLSMotionItemView *, PLSMotionItemView *> &batch : motionItemViewBatchs) {
		if (view >= batch.first && view < batch.second) {
			return true;
		}
	}
	return false;
}

PLSMotionItemView::PLSMotionItemView(QWidget *parent) : QLabel(parent), ui(new Ui::PLSMotionItemView), m_data(), m_isInUsing(false), m_properties(false), m_selected(false), m_hasMotionIcon(false)
{
	setVisible(false);
	ui->setupUi(this);
	this->installEventFilter(this);
	ui->closeButton->setHidden(true);
	ui->verticalLayout->setContentsMargins(0, 4, 4, 0);
	connect(ui->closeButton, &QPushButton::clicked, this, [=] { emit deleteFileButtonClicked(m_data); });
}

PLSMotionItemView::~PLSMotionItemView()
{
	delete ui;
}

const MotionData &PLSMotionItemView::motionData() const
{
	return m_data;
}

void PLSMotionItemView::setMotionData(const MotionData &motionData, bool properties)
{
	m_properties = properties;
	m_data = motionData;
	if (minimumWidth() > 1 && minimumHeight() > 1) {
		PLSMotionFileManager::instance()->getThumbnailPixmapAsync(this, m_data, size(), getDpi(), m_properties);
	}
}

QSize PLSMotionItemView::imageSize() const
{
	return m_imageSize;
}

void PLSMotionItemView::setImageSize(const QSize &size)
{
	if (size == m_imageSize) {
		return;
	}

	m_imageSize = size;
	setFixedSize(size);

	if ((size.width() > 1) && (size.height() > 1)) {
		PLSMotionFileManager::instance()->getThumbnailPixmapAsync(this, m_data, size, getDpi(), m_properties);
	}
}

void PLSMotionItemView::setSelected(bool selected)
{
	m_selected = selected;
	update();
}

bool PLSMotionItemView::selected() const
{
	return m_selected;
}

void PLSMotionItemView::setIconHidden(bool hidden)
{
	m_hasMotionIcon = !hidden;
	update();
}

void PLSMotionItemView::setCloseButtonHidden(bool hidden)
{
	ui->closeButton->setHidden(hidden);
}

void PLSMotionItemView::setCloseButtonHiddenByCursorPosition(const QPoint &cursorPosition)
{
	ui->closeButton->setVisible(rect().contains(mapFromGlobal(cursorPosition)));
}

bool PLSMotionItemView::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == this) {
		if (event->type() == QEvent::Leave) {
			ui->closeButton->setHidden(true);
		} else if (event->type() == QEvent::Enter) {
			ui->closeButton->setHidden(!m_data.canDelete);
		}
	}
	return QLabel::eventFilter(watched, event);
}

double PLSMotionItemView::getDpi()
{
	if (!m_isInUsing) {
		return 0.0;
	}

	for (QWidget *p = parentWidget(); p; p = p->parentWidget()) {
		if (PLSWidgetDpiAdapter *adapter = dynamic_cast<PLSWidgetDpiAdapter *>(p); adapter) {
			return adapter->getDpi();
		}
	}
	return 0.0;
}

void PLSMotionItemView::mousePressEvent(QMouseEvent *event)
{
	emit clicked(this);
	QLabel::mousePressEvent(event);
}

void PLSMotionItemView::resizeEvent(QResizeEvent *event)
{
	QLabel::resizeEvent(event);

	if (minimumWidth() > 1 && minimumHeight() > 1) {
		PLSMotionFileManager::instance()->getThumbnailPixmapAsync(this, m_data, event->size(), getDpi(), m_properties);
	}
}

void PLSMotionItemView::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::RenderHint::Antialiasing);
	painter.setRenderHint(QPainter::RenderHint::SmoothPixmapTransform);

	QRect rect = this->rect();
	painter.fillRect(rect, Qt::transparent);

	if (!m_selected) {
		painter.drawPixmap(rect, m_normalPixmap);
	} else {
		painter.drawPixmap(rect, m_selectedPixmap);
	}

	if (m_hasMotionIcon) {
		double dpi = PLSDpiHelper::getDpi(this);
		int _7px = PLSDpiHelper::calculate(dpi, 7);
		int _23px = PLSDpiHelper::calculate(dpi, 23);
		PLSMotionFileManager::instance()->getMotionFlagSvg()->render(&painter, QRect(rect.left() + _7px, rect.bottom() - _23px - _7px, _23px, _23px));
	}
}

void PLSMotionItemView::processThumbnailFinished(QThread *thread, const QString &itemId, const QPixmap &normalPixmap, const QPixmap &selectedPixmap)
{
	if (m_isInUsing && (m_data.itemId == itemId)) {
		m_normalPixmap = normalPixmap;
		m_selectedPixmap = selectedPixmap;
		if (thread != this->thread()) {
			QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
		} else {
			update();
		}
	}
}

void PLSMotionItemView::addBatchCache(int batchCount, bool check)
{
	if (batchCount <= 0 || QThread::currentThread() != qApp->thread()) {
		return;
	}

	if (check) {
		batchCount -= motionItemViewCache.count();
	}

	if (batchCount <= 0) {
		return;
	}

	PLSMotionItemView *views = new PLSMotionItemView[batchCount];
	motionItemViewBatchs.append(QPair<PLSMotionItemView *, PLSMotionItemView *>(views, views + batchCount));
	for (int i = 0; i < batchCount; ++i) {
		motionItemViewCache.append(&views[i]);
	}
}

void PLSMotionItemView::cleaupCache()
{
	QObject::connect(qApp, &QObject::destroyed, []() {
		while (!motionItemViewCache.isEmpty()) {
			PLSMotionItemView *view = motionItemViewCache.takeFirst();
			if (!isOfBatch(view)) {
				delete view;
			}
		}

		while (!motionItemViewBatchs.isEmpty()) {
			QPair<PLSMotionItemView *, PLSMotionItemView *> batch = motionItemViewBatchs.takeFirst();
			delete[] batch.first;
		}
	});
}

PLSMotionItemView *PLSMotionItemView::alloc(const MotionData &data, bool properties)
{
	PLSMotionItemView *view = nullptr;
	if (!motionItemViewCache.isEmpty()) {
		view = motionItemViewCache.takeFirst();
	} else {
		view = new PLSMotionItemView();
	}

	view->m_isInUsing = true;
	view->setFixedSize(1, 1);
	view->m_imageSize = QSize(1, 1);
	view->setMotionData(data, properties);
	view->setCloseButtonHidden(true);
	return view;
}

void PLSMotionItemView::dealloc(PLSMotionItemView *view)
{
	view->m_isInUsing = false;
	view->hide();
	view->setParent(nullptr);
	view->setFixedSize(1, 1);

	motionItemViewCache.append(view);
	disconnect(view->clickedConn);
	disconnect(view->delBtnClickedConn);
}
