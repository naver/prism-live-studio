#include "PLSMixerContent.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QDropEvent>
#include <QMimeData>
#include <QScrollArea>
#include <qscrollbar.h>
#include <qdrag.h>
#include "liblog.h"

constexpr auto SCENE_SCROLLCONTENT_LINE_COLOR = "#effc35";
constexpr auto SCENE_SCROLLCONTENT_DEFAULT_COLOR = "#272727";
constexpr auto AUTO_SCROLL_MARGIN = 16;
constexpr auto H_VOLUME_CONTROL_HEIGHT = 80;
constexpr auto V_VOLUME_CONTROL_WIDTH = 120;

PLSMixerContent::PLSMixerContent(Qt::Orientation _orientation, QWidget *parent) : orientation(_orientation)
{
	this->setAcceptDrops(true);
	this->setAutoFillBackground(true);
	if (Qt::Orientation::Horizontal == orientation) {
		main_layout = new QVBoxLayout(this);
		main_layout->setSpacing(0);
		main_layout->setObjectName("hVolControlLayout");
		main_layout->setContentsMargins(3, 1, 5, 1);
	} else {
		main_layout = new QHBoxLayout(this);
		main_layout->setSpacing(5);
		main_layout->setObjectName("vVolControlLayout");
		main_layout->setContentsMargins(16, 0, 20, 20);
	}

	assert(main_layout);
}

void PLSMixerContent::AddWidget(QWidget *widget)
{
	widget->setCursor(Qt::PointingHandCursor);
	main_layout->addWidget(widget);
}

void PLSMixerContent::paintEvent(QPaintEvent *event)
{
	if (Qt::Orientation::Horizontal == orientation) {
		QWidget::paintEvent(event);
		return;
	}

	QPainter painter(this);
	if (isDrag) {
		painter.setPen(QPen(QColor(SCENE_SCROLLCONTENT_LINE_COLOR), 1));
	} else {
		painter.setPen(QPen(QColor(SCENE_SCROLLCONTENT_DEFAULT_COLOR), 1));
	}

	QLine l;
	l.setP1(lineStart);
	l.setP2(lineEnd);
	painter.drawLine(l);

	QWidget::paintEvent(event);
}

void PLSMixerContent::timerEvent(QTimerEvent *event)
{
	do {
		if (event->timerId() == delayedAutoScroll.timerId()) {

			if (!scrollarea)
				break;

			if (Qt::Horizontal == orientation) {
				auto verticalScrollbar = scrollarea->verticalScrollBar();
				verticalScrollbar->setValue(verticalScrollbar->value() + autoScrollStepValue);
			} else {
				auto horizonScrollbar = scrollarea->horizontalScrollBar();
				horizonScrollbar->setValue(horizonScrollbar->value() + autoScrollStepValue);
			}
		}
	} while (false);

	QWidget::timerEvent(event);
}

void PLSMixerContent::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::ParentChange) {
		QWidget *parent = parentWidget();
		while (parent) {
			if (auto obj = qobject_cast<QScrollArea *>(parent); obj) {
				scrollarea = obj;
				break;
			}
			parent = parent->parentWidget();
		}
	}

	QWidget::changeEvent(event);
}

void PLSMixerContent::mousePressEvent(QMouseEvent *event)
{
	startDragPoint = event->position().toPoint();

	VolControl *vol = qobject_cast<VolControl *>(childAt(startDragPoint));
	if (!vol) {
		QWidget::mousePressEvent(event);
		return;
	}

	if (vol == lastClickedVol)
		return;

	vol->setClickState(true);
	if (lastClickedVol) {
		lastClickedVol->setClickState(false);
	}

	lastClickedVol = vol;

	QWidget::mousePressEvent(event);
}

void PLSMixerContent::mouseMoveEvent(QMouseEvent *event)
{
	if (event->buttons() & Qt::LeftButton) {
		QPoint distance = event->pos() - startDragPoint;
		if (distance.manhattanLength() > QApplication::startDragDistance()) {

			VolControl *child = qobject_cast<VolControl *>(childAt(startDragPoint));
			if (!child)
				return;

			QByteArray itemData;
			QDataStream dataStream(&itemData, QIODevice::WriteOnly);
			dataStream << main_layout->indexOf(child);

			QMimeData *mimeData = pls_new<QMimeData>();
			mimeData->setData("application/x-dnditemdata", itemData);

			auto drag = pls_new<QDrag>(this);
			drag->setMimeData(mimeData);
			drag->setHotSpot(startDragPoint - child->pos());
#if defined(__APPLE__)
			/* PRISM_PC-1050: In order to remove the dotted rectangle when dragging on MacOs,
             we use the trick of setting a small and transparent pixmap for the drag. */
			QPixmap pix(1, 1);
			pix.fill(Qt::transparent);
			drag->setPixmap(pix);
#endif
			drag->exec(Qt::MoveAction, Qt::MoveAction);
		}
	}
	QWidget::mouseMoveEvent(event);
}

void PLSMixerContent::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat("application/x-dnditemdata")) {
		event->setDropAction(Qt::MoveAction);
		event->accept();
		isDrag = true;
	} else {
		event->ignore();
	}
}

void PLSMixerContent::dragMoveEvent(QDragMoveEvent *event)
{
	if (event->mimeData()->hasFormat("application/x-dnditemdata")) {

		isDrag = true;
		OnDragMoveEvent(event);
		AutoScroll(event);
		update();
	} else {
		event->ignore();
	}
}

void PLSMixerContent::dropEvent(QDropEvent *event)
{
	if (event->mimeData()->hasFormat("application/x-dnditemdata")) {
		QByteArray itemData = event->mimeData()->data("application/x-dnditemdata");
		QDataStream dataStream(&itemData, QIODevice::ReadOnly);

		int startIndex;
		dataStream >> startIndex;

		if (destIndex != -1 && destIndex != startIndex && nullptr != main_layout->itemAt(startIndex)) {
			main_layout->insertWidget(destIndex, main_layout->itemAt(startIndex)->widget());
			emit mixerReorderd();
		}

		isDrag = false;
		ClearItemBorder();

		delayedAutoScroll.stop();
		event->setDropAction(Qt::MoveAction);
		event->accept();
		update();
	} else {
		event->ignore();
	}
}

void PLSMixerContent::dragLeaveEvent(QDragLeaveEvent *event)
{
	isDrag = false;
	ClearItemBorder();
	delayedAutoScroll.stop();
	update();

	QWidget::dragLeaveEvent(event);
}

void PLSMixerContent::SetLinePos(const int &startX, const int &startY, const int &endX, const int &endY)
{
	lineStart.setX(startX);
	lineStart.setY(startY);
	lineEnd.setX(endX);
	lineEnd.setY(endY);
}

void PLSMixerContent::DisplayItemBorder(VolControl *current, const char *borderType)
{
	if (!current || !borderType)
		return;

	ClearItemBorder();
	current->displayBorder(true, borderType);
	lastDisplayedVol = current;
}

void PLSMixerContent::ClearItemBorder()
{
	if (lastDisplayedVol) {
		lastDisplayedVol->displayBorder(false);
	}
}

void PLSMixerContent::OnDragMoveEvent(QDragMoveEvent *event)
{
	QByteArray itemData = event->mimeData()->data("application/x-dnditemdata");
	QDataStream dataStream(&itemData, QIODevice::ReadOnly);

	int startIndex;
	dataStream >> startIndex;

	VolControl *child = qobject_cast<VolControl *>(childAt(event->position().toPoint()));
	if (!child)
		return;

	QRect currentRect = child->geometry();
	int currentIndex = main_layout->indexOf(child);
	destIndex = currentIndex;
	if (Qt::Horizontal == orientation) {
		QPoint topleft = currentRect.topLeft();
		if (event->position().toPoint().y() - topleft.y() > currentRect.height() / 2) {
			DisplayItemBorder(child, "bottom");
			if (startIndex > currentIndex) {
				destIndex = std::min(currentIndex + 1, main_layout->count() - 1);
			}
			// PLS_INFO("PLSMixerContent", "=========== startIndex: %d, currentIndex: %d bottom, destIndex: %d", startIndex, currentIndex, destIndex);
		} else {
			if (startIndex < currentIndex) {
				destIndex = std::max(currentIndex - 1, 0);
			}
			DisplayItemBorder(child, "top");
			// PLS_INFO("PLSMixerContent", "=========== startIndex: %d, currentIndex: %d top, destIndex: %d", startIndex, currentIndex, destIndex);
		}

	} else {
		QPoint bottomleft = currentRect.bottomLeft();
		if (event->position().toPoint().x() - bottomleft.x() > currentRect.width() / 2) {
			SetLinePos(currentRect.topRight().x() + 1, currentRect.topRight().y(), currentRect.bottomRight().x() + 1, currentRect.bottomRight().y()); //right
			if (startIndex > currentIndex) {
				destIndex = std::min(currentIndex + 1, main_layout->count() - 1);
			}
		} else {
			if (startIndex < currentIndex) {
				destIndex = std::max(currentIndex - 1, 0);
			}
			SetLinePos(currentRect.topLeft().x() - 1, currentRect.topLeft().y(), currentRect.bottomLeft().x() - 1, currentRect.bottomLeft().y()); //left
		}
	}
}

void PLSMixerContent::AutoScroll(QDragMoveEvent *event)
{
	if (Qt::Horizontal == orientation) {
		// up-down auto scroll
		// auto scroll when go to invisible region
		QRect rect = visibleRegion().boundingRect();
		int y = mapToParent(event->position()).y();
		if (y < AUTO_SCROLL_MARGIN) {
			autoScrollStepValue = -H_VOLUME_CONTROL_HEIGHT / 8;
			delayedAutoScroll.start(50, this);
		} else if (y > rect.height() - AUTO_SCROLL_MARGIN) {
			autoScrollStepValue = H_VOLUME_CONTROL_HEIGHT / 8;
			delayedAutoScroll.start(50, this);
		} else {
			autoScrollStepValue = 0;
			delayedAutoScroll.stop();
		}
	} else {
		// left-right auto scroll
		// auto scroll when go to invisible region
		QRect rect = visibleRegion().boundingRect();
		int x = mapToParent(event->position()).x();
		if (x < AUTO_SCROLL_MARGIN) {
			autoScrollStepValue = -V_VOLUME_CONTROL_WIDTH / 8;
			delayedAutoScroll.start(50, this);
		} else if (x > rect.width() - AUTO_SCROLL_MARGIN) {
			autoScrollStepValue = V_VOLUME_CONTROL_WIDTH / 8;
			delayedAutoScroll.start(50, this);
		} else {
			autoScrollStepValue = 0;
			delayedAutoScroll.stop();
		}
	}
}
