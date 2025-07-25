#include "moc_focus-list.cpp"
#include <QDragMoveEvent>
#include <QPainter>
static const int FIX_ITEM_HEIGHT = 40;

FocusList::FocusList(QWidget *parent) : QListWidget(parent) {}

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