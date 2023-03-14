#include "PLSScrollAreaContent.h"
#include "PLSSceneDataMgr.h"

#include <QPainter>
#include <QPen>
#include <QDropEvent>
#include <QMimeData>
#include <QDebug>
#include <math.h>

#include <util/util.hpp>
#include "pls-app.hpp"
#include "pls-common-define.hpp"
#include "PLSDpiHelper.h"

PLSScrollAreaContent::PLSScrollAreaContent(QWidget *parent) : QWidget(parent)
{
	this->setAcceptDrops(true);
}

PLSScrollAreaContent::~PLSScrollAreaContent() {}

int PLSScrollAreaContent::Refresh(DisplayMethod displayMethod)
{
	double dpi = PLSDpiHelper::getDpi(this);
	bool gridMode = (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView);
	//Calculate how many columns you can put in this view
	int leftSpacing = gridMode ? SCENE_LEFT_SPACING : SCENE_LIST_MODE_LEFT_SPACING;
	int minWidth = PLSDpiHelper::calculate(dpi, SCENE_ITEM_FIX_WIDTH + SCENE_ITEM_HSPACING);
	int columnCount = (this->width() - PLSDpiHelper::calculate(dpi, leftSpacing)) / minWidth;
	if (columnCount <= 0 || !gridMode)
		columnCount = 1;

	int sum = 0, i = 0, currentY = 0;
	SceneDisplayVector vec = PLSSceneDataMgr::Instance()->GetDisplayVector();
	for (auto iter = vec.begin(); iter != vec.end(); ++iter, i++) {
		int rows = i / columnCount;
		int column = i % columnCount;
		int x = PLSDpiHelper::calculate(dpi, leftSpacing) + column * minWidth;
		PLSSceneItemView *item = iter->second;
		int realHeight = gridMode ? PLSDpiHelper::calculate(dpi, SCENE_ITEM_FIX_HEIGHT) : PLSDpiHelper::calculate(dpi, SCENE_ITEM_LIST_MODE_FIX_HEIGHT);
		int realWidth = gridMode ? PLSDpiHelper::calculate(dpi, SCENE_ITEM_FIX_WIDTH) : width();
		int y = rows * (PLSDpiHelper::calculate(dpi, SCENE_ITEM_VSPACING) + realHeight);
		if (item) {
			item->setGeometry(x, y, realWidth, realHeight);
			item->setFixedHeight(realHeight);
			item->show();
			if (i >= SCENE_RENDER_NUMBER && displayMethod == DisplayMethod::DynamicRealtimeView) {
				item->SetRenderFlag(false);
			} else {
				item->SetRenderFlag(gridMode);
			}

			if (item->GetCurrentFlag()) {
				currentY = rows * realHeight;
				item->OnCurrentItemChanged(true);
				QRect rect(x, y, realWidth, realHeight);
				if (this->visibleRegion().contains(rect)) {
					currentY = SCENE_ITEM_DO_NOT_NEED_AUTO_SCROLL;
				}
			}
		}

		sum = y + realHeight;
	}
	setMinimumHeight(sum + 1);
	return currentY;
}

void PLSScrollAreaContent::SetIsDraging(bool state)
{
	isDrag = state;
}

void PLSScrollAreaContent::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	double dpi = PLSDpiHelper::getDpi(this);
	if (isDrag) {
		painter.setPen(QPen(QColor(SCENE_SCROLLCONTENT_LINE_COLOR), PLSDpiHelper::calculate(dpi, 1)));
	} else {
		painter.setPen(QPen(QColor(SCENE_SCROLLCONTENT_DEFAULT_COLOR), PLSDpiHelper::calculate(dpi, 1)));
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
		DisplayMethod displayMethod = static_cast<DisplayMethod>(atoi(event->mimeData()->data(SCENE_DRAG_GRID_MODE).toStdString().c_str()));
		bool gridMode = (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView);
		int width = data.split(":")[0].toInt();
		int height = data.split(":")[1].toInt();
		int row = 0, col = 0;

		//qDebug() << "before direction: row = " << row << ", col = " << col;
		GetRowColByPos(gridMode, event->pos().x(), event->pos().y(), width, height, row, col);
		SetDrawLineByPos(gridMode, event->pos().x(), event->pos().y(), width, height, row, col);

		//qDebug() << "after  direction: row = " << row << ", col = " << col;

		// auto scroll when go to invisible region
		QRect rect = visibleRegion().boundingRect();
		int y = mapToParent(event->pos()).y();
		double dpi = PLSDpiHelper::getDpi(this);
		if (y < PLSDpiHelper::calculate(dpi, SCENE_ITEM_AUTO_SCROLL_MARGIN)) {
			emit DragMoving(event->pos().x(), -height / 8);
		} else if (y > rect.height() - PLSDpiHelper::calculate(dpi, SCENE_ITEM_AUTO_SCROLL_MARGIN)) {
			emit DragMoving(event->pos().x(), height / 8);
		}

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
		DisplayMethod displayMethod = static_cast<DisplayMethod>(atoi(event->mimeData()->data(SCENE_DRAG_GRID_MODE).toStdString().c_str()));
		bool gridMode = (displayMethod == DisplayMethod::DynamicRealtimeView || displayMethod == DisplayMethod::ThumbnailView);
		int width = data.split(":")[0].toInt();
		int height = data.split(":")[1].toInt();
		int x = data.split(":")[2].toInt();
		int y = data.split(":")[3].toInt();
		int appendRow = 0, appendCol = 0, removeRow = 0, removeCol = 0;
		GetRowColByPos(gridMode, x, y, width, height, removeRow, removeCol);
		GetRowColByPos(gridMode, event->pos().x(), event->pos().y(), width, height, appendRow, appendCol, true, removeRow);
		SetDrawLineByPos(gridMode, event->pos().x(), event->pos().y(), width, height, appendRow, appendCol);

		int leftSpacing = gridMode ? SCENE_LEFT_SPACING : SCENE_LIST_MODE_LEFT_SPACING;
		int hSpacing = gridMode ? SCENE_ITEM_HSPACING : SCENE_ITEM_LIST_MODE_HSPACING;
		double dpi = PLSDpiHelper::getDpi(this);
		int realWidth = gridMode ? PLSDpiHelper::calculate(dpi, SCENE_ITEM_FIX_WIDTH) : width;
		int minWidth = realWidth + PLSDpiHelper::calculate(dpi, hSpacing);
		int columnCount = (this->width() - PLSDpiHelper::calculate(dpi, leftSpacing)) / minWidth;
		if (columnCount <= 0 || !gridMode) {
			columnCount = 1;
		}

		this->update();
		gridMode ? PLSSceneDataMgr::Instance()->SwapData(removeRow, removeCol, appendRow, appendCol, columnCount) : PLSSceneDataMgr::Instance()->SwapDataInListMode(removeRow, appendRow);
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
	emit resizeEventChanged(true);
	QWidget::resizeEvent(event);
}

void PLSScrollAreaContent::GetRowColByPos(bool gridMode, const int &x, const int &y, const int &width, const int &height, int &row, int &col, bool append, int romoveRow)
{
	if (!gridMode && append) {
		GetRowColByPosInListMode(x, y, width, height, row, col, romoveRow);
		return;
	}

	row = y / height;
	int leftSpacing = gridMode ? SCENE_LEFT_SPACING : SCENE_LIST_MODE_LEFT_SPACING;
	int hSpacing = gridMode ? SCENE_ITEM_HSPACING : SCENE_ITEM_LIST_MODE_HSPACING;

	double dpi = PLSDpiHelper::getDpi(this);
	int minWidth = width + PLSDpiHelper::calculate(dpi, hSpacing);
	int columnCount = (this->width() - PLSDpiHelper::calculate(dpi, leftSpacing)) / minWidth;
	if (columnCount <= 0 || !gridMode)
		columnCount = 1;
	col = (x - PLSDpiHelper::calculate(dpi, leftSpacing)) / minWidth;

	int size = PLSSceneDataMgr::Instance()->GetSceneSize();
	if (row * columnCount + col >= size) {
		row = size / columnCount;
		col = size % columnCount;
		if (0 == col) {
			row = row - 1 < 0 ? 0 : row - 1;
			col = gridMode ? columnCount : 0;
		}
	}
}

void PLSScrollAreaContent::GetRowColByPosInListMode(const int &x, const int &y, const int &width, const int &height, int &row, int &col, int romoveRow)
{
	row = y / height;
	col = 0;
	if (romoveRow == row) {
		return;
	}

	DragDirection direction = GetDirectionByPos(false, x, y, width, height, row, col);
	if (direction == DragDirection::Top) {
		if (romoveRow > row) {
			row = std::max(0, y / height);
		} else {
			row = std::max(0, y / height - 1);
		}
	} else {
		if (romoveRow > row) {
			row = std::max(0, y / height + 1);
		} else {
			row = std::max(0, y / height);
		}
	}
}

DragDirection PLSScrollAreaContent::GetDirectionByPos(bool gridMode, const int &x, const int &y, const int &width, const int &height, const int &row, int &col)
{
	double dpi = PLSDpiHelper::getDpi(this);
	DragDirection direction = DragDirection::Unknown;

	int hSpacing = gridMode ? SCENE_ITEM_HSPACING : SCENE_ITEM_LIST_MODE_HSPACING;
	int minWidth = width + PLSDpiHelper::calculate(dpi, hSpacing);
	int leftSpacing = gridMode ? SCENE_LEFT_SPACING : SCENE_LIST_MODE_LEFT_SPACING;

	int columnCount = (this->width() - PLSDpiHelper::calculate(dpi, leftSpacing)) / minWidth;
	if (columnCount <= 0 || !gridMode)
		columnCount = 1;
	int leftTopyPos = row * height + row * PLSDpiHelper::calculate(dpi, SCENE_ITEM_VSPACING); //vspacing
	int leftTopxPos = PLSDpiHelper::calculate(dpi, leftSpacing) + col * (PLSDpiHelper::calculate(dpi, hSpacing) + width);

	if (gridMode) {
		if (x - leftTopxPos > width / columnCount) {
			direction = DragDirection::Right;
		} else {
			direction = DragDirection::Left;
		}
	} else {
		if (y - leftTopyPos > height / 2) {
			direction = DragDirection::Bottom;
		} else {
			direction = DragDirection::Top;
		}
	}
	return direction;
}

void PLSScrollAreaContent::SetDrawLineByPos(bool gridMode, const int &x, const int &y, const int &width, const int &height, const int &row, int &col)
{
	double dpi = PLSDpiHelper::getDpi(this);
	DragDirection direction = GetDirectionByPos(gridMode, x, y, width, height, row, col);

	int hSpacing = gridMode ? SCENE_ITEM_HSPACING : SCENE_ITEM_LIST_MODE_HSPACING;
	int leftSpacing = gridMode ? SCENE_LEFT_SPACING : SCENE_LIST_MODE_LEFT_SPACING;

	int leftTopyPos = row * height + row * PLSDpiHelper::calculate(dpi, SCENE_ITEM_VSPACING); //vspacing
	int leftTopxPos = PLSDpiHelper::calculate(dpi, leftSpacing) + col * (PLSDpiHelper::calculate(dpi, hSpacing) + width);

	if (direction == DragDirection::Left) {
		if (col == 0) {
			leftTopxPos = PLSDpiHelper::calculate(dpi, SCENE_LEFT_SPACING - 5); //hspacing
		} else {
			leftTopxPos = PLSDpiHelper::calculate(dpi, SCENE_LEFT_SPACING) + width * col + (col - 0.5) * PLSDpiHelper::calculate(dpi, SCENE_ITEM_HSPACING); //hspacing
		}
	}

	else if (direction == DragDirection::Right) {
		if (col == 0) {
			leftTopxPos = PLSDpiHelper::calculate(dpi, SCENE_LEFT_SPACING - 5); //hspacing
		} else {
			leftTopxPos = PLSDpiHelper::calculate(dpi, SCENE_LEFT_SPACING) + width * col + (col - 0.5) * PLSDpiHelper::calculate(dpi, SCENE_ITEM_HSPACING); //hspacing
		}
	}

	else if (direction == DragDirection::Bottom) {
		leftTopyPos = (row + 1) * height + row * PLSDpiHelper::calculate(dpi, SCENE_ITEM_VSPACING);
	}

	if (direction == DragDirection::Bottom || direction == DragDirection::Top) {
		SetLinePos(leftTopxPos, leftTopyPos, leftTopxPos + width, leftTopyPos);

	} else {
		SetLinePos(leftTopxPos, leftTopyPos, leftTopxPos, leftTopyPos + width);
	}
}

void PLSScrollAreaContent::SetLinePos(const int &startX, const int &startY, const int &endX, const int &endY)
{
	lineStart.setX(startX);
	lineStart.setY(startY);
	lineEnd.setX(endX);
	lineEnd.setY(endY);
}
