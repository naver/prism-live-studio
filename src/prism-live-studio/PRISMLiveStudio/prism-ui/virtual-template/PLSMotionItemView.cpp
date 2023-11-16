#include "PLSMotionItemView.h"
#include "ui_PLSMotionItemView.h"

#include "PLSMotionFileManager.h"
#include "PLSMotionNetwork.h"
#include "utils-api.h"
#include "libui.h"

#include <qapplication.h>
#include <qthread.h>
#include <qpainter.h>

namespace {
struct LocalGlobalVars {
	static QList<PLSMotionItemView *> motionItemViewCache;
	static QList<QPair<PLSMotionItemView *, PLSMotionItemView *>> motionItemViewBatchs;
};

QList<PLSMotionItemView *> LocalGlobalVars::motionItemViewCache;
QList<QPair<PLSMotionItemView *, PLSMotionItemView *>> LocalGlobalVars::motionItemViewBatchs;
}

static bool isOfBatch(const PLSMotionItemView *view)
{
	return pls_contains(LocalGlobalVars::motionItemViewBatchs.begin(), LocalGlobalVars::motionItemViewBatchs.end(),
			    [view](const QPair<PLSMotionItemView *, PLSMotionItemView *> &batch, int) { return view >= batch.first && view < batch.second; });
}

PLSMotionItemView::PLSMotionItemView(QWidget *parent) : QLabel(parent), m_data()
{
	ui = pls_new<Ui::PLSMotionItemView>();
	PLSMotionItemView::setVisible(false);
	ui->setupUi(this);
	this->installEventFilter(this);
	ui->closeButton->setHidden(true);
	ui->verticalLayout->setContentsMargins(0, 4, 4, 0);
	setProperty("showHandCursor", true);

	connect(ui->closeButton, &QPushButton::clicked, this, [this] { emit deleteFileButtonClicked(m_data); });
}

PLSMotionItemView::~PLSMotionItemView()
{
	pls_delete(ui);
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

double PLSMotionItemView::getDpi() const
{
	if (!m_isInUsing) {
		return 0.0;
	}

	//for (QWidget *p = parentWidget(); p; p = p->parentWidget()) {
	//	if (const PLSWidgetDpiAdapter *adapter = dynamic_cast<PLSWidgetDpiAdapter *>(p); adapter) {
	//		return adapter->getDpi();
	//	}
	//}
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
		int _5px = 5;
		int _23px = 23;
		PLSMotionFileManager::instance()->getMotionFlagSvg()->render(&painter, QRect(rect.left() + _5px, rect.height() - _23px - _5px, _23px, _23px));
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
		batchCount -= LocalGlobalVars::motionItemViewCache.count();
	}

	if (batchCount <= 0) {
		return;
	}

	PLSMotionItemView *views = pls_new_array<PLSMotionItemView>(batchCount);
	LocalGlobalVars::motionItemViewBatchs.append(QPair<PLSMotionItemView *, PLSMotionItemView *>(views, views + batchCount));
	for (int i = 0; i < batchCount; ++i) {
		LocalGlobalVars::motionItemViewCache.append(&views[i]);
	}
}

void PLSMotionItemView::cleaupCache()
{
	pls_qapp_deconstruct_add_cb([]() {
		while (!LocalGlobalVars::motionItemViewCache.isEmpty()) {
			PLSMotionItemView *view = LocalGlobalVars::motionItemViewCache.takeFirst();
			if (!isOfBatch(view)) {
				pls_delete(view);
			}
		}

		while (!LocalGlobalVars::motionItemViewBatchs.isEmpty()) {
			QPair<PLSMotionItemView *, PLSMotionItemView *> batch = LocalGlobalVars::motionItemViewBatchs.takeFirst();
			pls_delete_array(batch.first);
		}
	});
}

PLSMotionItemView *PLSMotionItemView::alloc(const MotionData &data, bool properties)
{
	PLSMotionItemView *view = nullptr;
	if (!LocalGlobalVars::motionItemViewCache.isEmpty()) {
		view = LocalGlobalVars::motionItemViewCache.takeFirst();
	} else {
		view = pls_new<PLSMotionItemView>();
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

	LocalGlobalVars::motionItemViewCache.append(view);
	disconnect(view->clickedConn);
	disconnect(view->delBtnClickedConn);
}
