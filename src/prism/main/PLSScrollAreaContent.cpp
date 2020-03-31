#include "PLSScrollAreaContent.h"
#include "PLSSceneDataMgr.h"

#include <QPainter>
#include <QPen>
#include <QDropEvent>
#include <QMimeData>
#include <QDebug>

#include <util/util.hpp>
#include "pls-app.hpp"
#include "pls-common-define.hpp"

PLSScrollAreaContent::PLSScrollAreaContent(QWidget *parent) : QWidget(parent)
{
	this->setAcceptDrops(true);
	SetDefaultRenderNum();
}

PLSScrollAreaContent::~PLSScrollAreaContent() {}

void PLSScrollAreaContent::Refresh()
{
	//Calculate how many columns you can put in this view
	int minWidth = SCENE_ITEM_FIX_WIDTH + SCENE_ITEM_HSPACING;
	int columnCount = (this->width() - SCENE_LEFT_SPACING) / minWidth;
	if (columnCount <= 0)
		return;
	int sum = 0, i = 0;
	SceneDisplayVector vec = PLSSceneDataMgr::Instance()->GetDisplayVector();
	for (auto iter = vec.begin(); iter != vec.end(); ++iter, i++) {
		int rows = i / columnCount;
		int column = i % columnCount;
		int x = SCENE_LEFT_SPACING + column * minWidth;
		int y = rows * (SCENE_ITEM_FIX_HEIGHT + SCENE_ITEM_VSPACING);
		PLSSceneItemView *item = iter->second;
		if (item) {
			item->setGeometry(x, y, SCENE_ITEM_FIX_WIDTH, SCENE_ITEM_FIX_HEIGHT);
			item->show();
			if (i >= renderNumber) {
				item->SetRenderFlag(false);
			} else {
				item->SetRenderFlag(true);
			}
		}

		sum = y + SCENE_ITEM_FIX_HEIGHT;
	}
	setMinimumHeight(sum - SCENE_SCROLL_AREA_SPACING_HEIGHT);
}

void PLSScrollAreaContent::SetRenderNum(const int &renderNumber)
{
	this->renderNumber = renderNumber;
}

int PLSScrollAreaContent::GetRenderNum()
{
	return renderNumber;
}

void PLSScrollAreaContent::SetIsDraging(bool state)
{
	isDrag = state;
}

void PLSScrollAreaContent::paintEvent(QPaintEvent *event)
{
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

void PLSScrollAreaContent::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat(SCENE_DRAG_MIME_TYPE)) {
		event->accept();
	} else {
		event->ignore();
	}
}

void PLSScrollAreaContent::dragMoveEvent(QDragMoveEvent *event)
{
	if (event->mimeData()->hasFormat(SCENE_DRAG_MIME_TYPE)) {
		event->accept();
		isDrag = true;
		QString data = event->mimeData()->data(SCENE_DRAG_MIME_TYPE).toStdString().c_str();
		int width = data.split(":")[0].toInt();
		int height = data.split(":")[1].toInt();
		int row = 0, col = 0;

		//qDebug() << "before direction: row = " << row << ", col = " << col;
		GetRowColByPos(event->pos().x(), event->pos().y(), width, height, row, col);
		GetDirectionByPos(event->pos().x(), event->pos().y(), width, height, row, col);
		//qDebug() << "PLSScrollAreaContent::dropEvent pos: " << event->pos().x() << ", " << event->pos().y();
		//qDebug() << "after  direction: row = " << row << ", col = " << col;

		this->update();
	} else {
		event->ignore();
		isDrag = false;
	}
}

void PLSScrollAreaContent::dropEvent(QDropEvent *event)
{
	if (event->mimeData()->hasFormat(SCENE_DRAG_MIME_TYPE)) {
		event->setDropAction(Qt::MoveAction);
		event->accept();
		QString data = event->mimeData()->data(SCENE_DRAG_MIME_TYPE).toStdString().c_str();
		int width = data.split(":")[0].toInt();
		int height = data.split(":")[1].toInt();
		int x = data.split(":")[2].toInt();
		int y = data.split(":")[3].toInt();
		//qDebug() << "PLSScrollAreaContent::dropEvent width:height: " << width << ", " << height;
		//qDebug() << "PLSScrollAreaContent::dropEvent start pos: " << x << ", " << y;

		int appendRow = 0, appendCol = 0, removeRow = 0, removeCol = 0;
		GetRowColByPos(x, y, width, height, removeRow, removeCol);

		GetRowColByPos(event->pos().x(), event->pos().y(), width, height, appendRow, appendCol);
		GetDirectionByPos(event->pos().x(), event->pos().y(), width, height, appendRow, appendCol);
		//qDebug() << "PLSScrollAreaContent::dropEvent pos: " << event->pos().x() << ", " << event->pos().y();
		//qDebug() << "appendRow = " << appendRow << ", appendCol = " << appendCol;
		//qDebug() << "removeRow = " << removeRow << ", removeCol = " << removeCol;

		int minWidth = SCENE_ITEM_FIX_WIDTH + SCENE_ITEM_HSPACING;
		int columnCount = (this->width() - SCENE_LEFT_SPACING) / minWidth;

		this->update();
		PLSSceneDataMgr::Instance()->SwapData(removeRow, removeCol, appendRow, appendCol, columnCount);
		emit DragFinished();
		isDrag = false;
	} else {
		event->ignore();
	}
}

void PLSScrollAreaContent::dragLeaveEvent(QDragLeaveEvent *event)
{
	isDrag = false;
	update();

	QWidget::dragLeaveEvent(event);
}

void PLSScrollAreaContent::resizeEvent(QResizeEvent *event)
{
	this->Refresh();
	QWidget::resizeEvent(event);
}

void PLSScrollAreaContent::GetRowColByPos(const int &x, const int &y, const int &width, const int &height, int &row, int &col)
{
	row = y / SCENE_ITEM_FIX_HEIGHT;

	int minWidth = SCENE_ITEM_FIX_WIDTH + SCENE_ITEM_HSPACING;
	int columnCount = (this->width() - SCENE_LEFT_SPACING) / minWidth;
	if (columnCount <= 0)
		return;
	col = (x - SCENE_LEFT_SPACING) / minWidth;

	int size = PLSSceneDataMgr::Instance()->GetSceneSize();
	if (row * columnCount + col >= size) {
		row = size / columnCount;
		col = size % columnCount;
		if (0 == col) {
			row = row - 1 < 0 ? 0 : row - 1;
			col = columnCount;
		}
	}
}

void PLSScrollAreaContent::GetDirectionByPos(const int &x, const int &y, const int &width, const int &height, const int &row, int &col)
{
	DragDirection direction = DragDirection::Unknown;

	int minWidth = SCENE_ITEM_FIX_WIDTH + SCENE_ITEM_HSPACING;
	int columnCount = (this->width() - SCENE_LEFT_SPACING) / minWidth;
	if (columnCount <= 0)
		return;
	int leftTopyPos = row * height + row * SCENE_ITEM_VSPACING; //vspacing
	int leftTopxPos = SCENE_LEFT_SPACING + col * (SCENE_ITEM_HSPACING + width);

	//qDebug() << "leftTopxPos = " << leftTopxPos << ", leftTopyPos = " << leftTopyPos;
	if (x - leftTopxPos > width / columnCount) {
		//qDebug() << "right";
		direction = DragDirection::Right;
	} else {
		//qDebug() << "left";
		direction = DragDirection::Left;
	}

	if (direction == DragDirection::Left) {
		if (col == 0) {
			leftTopxPos = SCENE_LEFT_SPACING - 5; //hspacing
		} else {
			leftTopxPos = SCENE_LEFT_SPACING + width * col + (col - 0.5) * SCENE_ITEM_HSPACING; //hspacing
		}
	}

	if (direction == DragDirection::Right) {
		if (col == 0) {
			leftTopxPos = SCENE_LEFT_SPACING - 5; //hspacing
		} else {
			leftTopxPos = SCENE_LEFT_SPACING + width * col + (col - 0.5) * SCENE_ITEM_HSPACING; //hspacing
		}
	}

	SetLinePos(leftTopxPos, leftTopyPos, leftTopxPos, leftTopyPos + SCENE_ITEM_FIX_WIDTH);

	//qDebug() << "lineStartX = " << lineStart.x() << ", lineStartY = " << lineStart.y();
	//qDebug() << "lineEndX = " << lineEnd.x() << ", lineEndY = " << lineEnd.y();
}

void PLSScrollAreaContent::SetLinePos(const int &startX, const int &startY, const int &endX, const int &endY)
{
	lineStart.setX(startX);
	lineStart.setY(startY);
	lineEnd.setX(endX);
	lineEnd.setY(endY);
}

void PLSScrollAreaContent::SetDefaultRenderNum()
{
	//MultiviewLayout index = static_cast<MultiviewLayout>(config_get_int(GetGlobalConfig(), "BasicWindow", "MultiviewLayout"));
	//renderNumber = PLSSceneDataMgr::Instance()->ConvertMultiviewLayoutToInt(index);
	renderNumber = SCENE_RENDER_NUMBER;
}
