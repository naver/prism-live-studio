#include "moc_focus-list.cpp"
#include <QDragMoveEvent>
#include <QPainter>
#include <QDrag>
#include <QTimer>
#include <QVBoxLayout>
#include "utils-api.h"
#include "libui.h"

static const int FIX_ITEM_HEIGHT = 40;

FocusList::FocusList(QWidget *parent) : QListWidget(parent) {}

void FocusList::SetEmptyWidget(QWidget *label)
{
	if (!label)
		return;

	if (count() > 0)
		label->hide();
	else
		label->show();

	assert(!emptyLabel);
	emptyLabel = label;

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addStretch();
	layout->addWidget(label);
	layout->addStretch();
}

void FocusList::focusInEvent(QFocusEvent *event)
{
	QListWidget::focusInEvent(event);

	emit GotFocus();
}

void FocusList::dragLeaveEvent(QDragLeaveEvent *event)
{
	isDraging = false;
	QListWidget::dragLeaveEvent(event);
}

void FocusList::dragEnterEvent(QDragEnterEvent *event)
{
	isDraging = true;
	QListWidget::dragEnterEvent(event);
}

// PRISM/wangshaohui/20251203/PRISM_PC-4257/fix cursor issue
void FocusList::startDrag(Qt::DropActions supportedActions)
{
#ifdef Q_OS_WIN
	auto mdl = model();
	if (mdl) {
		QMimeData *mimeData = mdl->mimeData(selectedIndexes());
		if (mimeData) {
			QDrag *drag = new QDrag(this);
			drag->setMimeData(mimeData);

			drag->setDragCursor(pls_get_win_custom_drag_pixmap(this), Qt::MoveAction);
			drag->exec(supportedActions);
		}
	}
#endif //  Q_OS_WIN
	QListView::startDrag(supportedActions);
}

void FocusList::dragMoveEvent(QDragMoveEvent *event)
{
	QRect currentRect;
	int count = this->count();
	int rowCount = this->indexAt(event->position().toPoint()).row();
	if (-1 == rowCount || rowCount > count) {
		rowCount = count - 1;
	}
	currentRect = visualRect(model()->index(rowCount, 0));

	if (QPoint topleft = currentRect.topLeft();
	    event->position().toPoint().y() - topleft.y() > FIX_ITEM_HEIGHT / 2) {
		SetPaintLinePos(currentRect.bottomLeft().x(), currentRect.bottomLeft().y(),
				currentRect.bottomRight().x(), currentRect.bottomRight().y());
	} else {
		SetPaintLinePos(currentRect.topLeft().x(), currentRect.topLeft().y(), currentRect.topRight().x(),
				currentRect.topRight().y());
	}

	this->viewport()->update();
	event->setDropAction(Qt::MoveAction);
	QListWidget::dragMoveEvent(event);
	event->accept();

	//// -----obs code-------
	////if ((itemRow == currentRow() + 1) ||
	////    (currentRow() == count() - 1 && itemRow == -1))
	////	event->ignore();
	////else
	//QListWidget::dragMoveEvent(event);
}

void FocusList::paintEvent(QPaintEvent *event)
{
	CheckCountChanged(count());

	QPainter painter(this->viewport());
	if (isDraging) {
		painter.setPen(QPen(QColor("#effc35"), 1));
	} else {
		painter.setPen(QPen(QColor("#272727"), 1));
	}

	QLine l;
	l.setP1(lineStart);
	l.setP2(lineEnd);
	painter.drawLine(l);
	QListWidget::paintEvent(event);
}

void FocusList::mousePressEvent(QMouseEvent *event)
{
	if (QApplication::keyboardModifiers() & Qt::ControlModifier) { // Ctrl or Command button is pressed
		if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
			QModelIndex idx = indexAt(event->pos());
			if (idx.isValid() && selectionModel()->isSelected(idx)) {
				return; // Cancel the inverted selection function when pressing ctrl
			}
		}
	}

	QListWidget::mousePressEvent(event);
}

void FocusList::rowsInserted(const QModelIndex &parent, int start, int end)
{
	QListWidget::rowsInserted(parent, start, end);
	CheckCountChanged(count());
}

void FocusList::rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
	QListWidget::rowsAboutToBeRemoved(parent, start, end);

	// Since here item is not removed immediately, we have to check it in timer
	QPointer<FocusList> obj = this;
	QTimer::singleShot(200, this, [this, obj]() {
		if (obj)
			CheckCountChanged(count());
	});
}

void FocusList::dropEvent(QDropEvent *event)
{
	isDraging = false;
	QListWidget::dropEvent(event);
}

void FocusList ::SetPaintLinePos(const int &startPosX, const int &startPosY, const int &endPosX, const int &endPosY)
{
	lineStart.setX(startPosX);
	lineStart.setY(startPosY);
	lineEnd.setX(endPosX);
	lineEnd.setY(endPosY);
}

void FocusList ::CheckCountChanged(size_t count)
{
	if (!emptyLabel)
		return;

	if (count > 0)
		emptyLabel->hide();
	else
		emptyLabel->show();
}