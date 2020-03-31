#ifndef PLSTOPLEVELVIEW_HPP
#define PLSTOPLEVELVIEW_HPP

#include <QWidget>
#include <QTimer>
#include <QDateTime>
#include <QApplication>
#include <QMouseEvent>
#include <QComboBox>
#include <QLineEdit>
#include <QAbstractButton>
#include <QTextEdit>
#include <QListWidget>
#include <QAbstractSpinBox>

#include "frontend-api.h"

template<typename ParentWidget> class PLSToplevelView : public ParentWidget {
	using Widget = ParentWidget;
	using ToplevelView = PLSToplevelView<ParentWidget>;

public:
	PLSToplevelView(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags()) : ParentWidget(parent, f)
	{
		resizePausedCheckTimer.setSingleShot(true);
		resizePausedCheckTimer.setInterval(200);

		connect(&resizePausedCheckTimer, &QTimer::timeout, this, [this]() {
			if (isResizing) {
				isResizing = false;
				resizePausedCheckTimer.stop();
				emit endResizeSignal();
			}
		});
	}
	virtual ~PLSToplevelView() {}

public:
	enum class CursorPosition { None, Top, Bottom, Left, Right, TopLeft, TopRight, BottomLeft, BottomRight, TitleBar, Content };
	static const int BORDER_WIDTH = 1;
	static const int RESIZE_BORDER_WIDTH = 5;
	static const int CORNER_WIDTH = 5;

	class InActiveHelper {
		ToplevelView *tlv;

	public:
		InActiveHelper(ToplevelView *tlv_) : tlv(tlv_) { tlv->isActive = false; }
		~InActiveHelper() { tlv->isActive = true; }
	};

public:
	void checkCursorPosition(const QPoint &globalPos)
	{
		if (!isActive) {
			cursorPosition = CursorPosition::None;
			return;
		}

		QPoint pos = mapFromGlobal(globalPos);
		if (!isResizeEnabled || isMaxState || isFullScreenState) {
			if (titleBarRect().contains(pos)) {
				cursorPosition = CursorPosition::TitleBar;
			} else if (rect().contains(pos)) {
				cursorPosition = CursorPosition::Content;
			} else {
				cursorPosition = CursorPosition::None;
			}
			return;
		}

		QRect centralRect(RESIZE_BORDER_WIDTH, RESIZE_BORDER_WIDTH, width() - RESIZE_BORDER_WIDTH * 2, height() - RESIZE_BORDER_WIDTH * 2);
		if (pos.x() <= centralRect.left() && pos.y() <= centralRect.top()) {
			cursorPosition = CursorPosition::TopLeft;
		} else if (pos.x() >= centralRect.right() && pos.y() >= centralRect.bottom()) {
			cursorPosition = CursorPosition::BottomRight;
		} else if (pos.x() >= centralRect.right() && pos.y() <= centralRect.top()) {
			cursorPosition = CursorPosition::TopRight;
		} else if (pos.x() <= centralRect.left() && pos.y() >= centralRect.bottom()) {
			cursorPosition = CursorPosition::BottomLeft;
		} else if (pos.x() <= centralRect.left()) {
			cursorPosition = CursorPosition::Left;
		} else if (pos.x() >= centralRect.right()) {
			cursorPosition = CursorPosition::Right;
		} else if (pos.y() <= centralRect.top()) {
			cursorPosition = CursorPosition::Top;
		} else if (pos.y() >= centralRect.bottom()) {
			cursorPosition = CursorPosition::Bottom;
		} else if (titleBarRect().contains(pos)) {
			cursorPosition = CursorPosition::TitleBar;
		} else if (centralRect.contains(pos)) {
			cursorPosition = CursorPosition::Content;
		} else {
			cursorPosition = CursorPosition::None;
		}
	}
	void updateCursor(void)
	{
		switch (cursorPosition) {
		case CursorPosition::Top:
		case CursorPosition::Bottom:
			setCursor(Qt::SizeVerCursor);
			break;
		case CursorPosition::Left:
		case CursorPosition::Right:
			setCursor(Qt::SizeHorCursor);
			break;
		case CursorPosition::TopLeft:
		case CursorPosition::BottomRight:
			setCursor(Qt::SizeFDiagCursor);
			break;
		case CursorPosition::TopRight:
		case CursorPosition::BottomLeft:
			setCursor(Qt::SizeBDiagCursor);
			break;
		default:
			setCursor(Qt::ArrowCursor);
			break;
		}
	}
	void mousePress(QMouseEvent *event)
	{
		QPoint globalPos = event->globalPos();
		QPoint pos = mapFromGlobal(globalPos);
		if (isActive && rect().contains(pos) && (event->button() == Qt::LeftButton) && !isMouseButtonDown) {
			checkCursorPosition(globalPos);
			updateCursor();

			if (cursorPosition == CursorPosition::None) {
				return;
			} else if (!isMoveInContent && cursorPosition == CursorPosition::Content) {
				return;
			} else if (cursorPosition == CursorPosition::Content && isInControl(childAt(pos))) {
				return;
			}

			grabMouse(cursor());
			isMouseButtonDown = true;
			isResizing = false;

			globalPosMousePress = globalPos;
			geometryOfMousePress = geometry();
			globalPosMouseMoveForResize = globalPos;
		}
	}
	void mouseRelease(QMouseEvent *event)
	{
		if (isActive && (event->button() == Qt::LeftButton) && isMouseButtonDown) {
			if (isResizing) {
				isResizing = false;
				resizePausedCheckTimer.stop();
				emit endResizeSignal();
			}

			isMouseButtonDown = false;
			releaseMouse();

			cursorPosition = CursorPosition::None;
			updateCursor();
		}
	}
	void mouseDbClick(QMouseEvent *event)
	{
		if (!isActive || event->button() != Qt::LeftButton) {
			return;
		} else if (!canMaximized()) {
			return;
		}

		checkCursorPosition(event->globalPos());
		updateCursor();

		if (cursorPosition != CursorPosition::TitleBar) {
			return;
		} else if (!isMaxState && !isFullScreenState) {
			showMaximized();
		} else {
			showNormal();
		}
	}
	void mouseMove(QMouseEvent *event)
	{
		QPoint globalPos = event->globalPos();

		isMouseButtonDown = isMouseButtonDown && (event->buttons() & Qt::LeftButton);

		if (isMouseButtonDown && globalPosMousePress == globalPos) {
			return;
		}

		if (isActive && !isMouseButtonDown) {
			checkCursorPosition(globalPos);
			updateCursor();
		}

		if (isForMove()) {
			if (isMaxState || isFullScreenState) {
				isMaxState = false;
				isFullScreenState = false;
				flushMaxFullScreenStateStyle();

				QRect g = geometry();

				QRect ng = geometryOfNormal;
				QPoint pos = mapFromGlobal(globalPos);
				ng.moveTo(globalPos.x() - pos.x() * ng.width() / g.width(), g.y());
				setGeometry(ng);

				globalPosMousePress = globalPos;
				geometryOfMousePress = ng;

				onMaxFullScreenStateChanged();
			} else {
				QRect g = geometryOfMousePress;
				g.moveTo(geometryOfMousePress.topLeft() + globalPos - globalPosMousePress);
				if (g != geometry()) {
					setGeometry(g);
				}
			}
		} else if (isForResize()) {
			if (globalPosMouseMoveForResize != globalPos) {
				globalPosMouseMoveForResize = globalPos;

				if (!isResizing) {
					isResizing = true;
					resizePausedCheckTimer.start();
					emit beginResizeSignal();
				} else {
					resizePausedCheckTimer.stop();
					resizePausedCheckTimer.start();
				}
			}

			QSize maxSize = maximumSize(), minSize = minimumSize();
			int mpw = geometryOfMousePress.width(), mph = geometryOfMousePress.height(), ox = globalPos.x() - globalPosMousePress.x(), oy = globalPos.y() - globalPosMousePress.y();
			QRect g = geometry();
			switch (cursorPosition) {
			case CursorPosition::Top:
				g = fromBottomLeftSize(geometryOfMousePress.bottomLeft(), QSize(mpw, mph - oy).expandedTo(minSize).boundedTo(maxSize));
				break;
			case CursorPosition::Bottom:
				g = fromTopLeftSize(geometryOfMousePress.topLeft(), QSize(mpw, mph + oy).expandedTo(minSize).boundedTo(maxSize));
				break;
			case CursorPosition::Left:
				g = fromTopRightSize(geometryOfMousePress.topRight(), QSize(mpw - ox, mph).expandedTo(minSize).boundedTo(maxSize));
				break;
			case CursorPosition::Right:
				g = fromTopLeftSize(geometryOfMousePress.topLeft(), QSize(mpw + ox, mph).expandedTo(minSize).boundedTo(maxSize));
				break;
			case CursorPosition::TopLeft:
				g = fromBottomRightSize(geometryOfMousePress.bottomRight(), QSize(mpw - ox, mph - oy).expandedTo(minSize).boundedTo(maxSize));
				break;
			case CursorPosition::TopRight:
				g = fromBottomLeftSize(geometryOfMousePress.bottomLeft(), QSize(mpw + ox, mph - oy).expandedTo(minSize).boundedTo(maxSize));
				break;
			case CursorPosition::BottomLeft:
				g = fromTopRightSize(geometryOfMousePress.topRight(), QSize(mpw - ox, mph + oy).expandedTo(minSize).boundedTo(maxSize));
				break;
			case CursorPosition::BottomRight:
				g = fromTopLeftSize(geometryOfMousePress.topLeft(), QSize(mpw + ox, mph + oy).expandedTo(minSize).boundedTo(maxSize));
				break;
			default:
				break;
			}

			if (g != geometry()) {
				setGeometry(g);
			}
		}
	}

public:
	bool getIsMoveInContent() const { return isMoveInContent; }
	void setIsMoveInContent(bool isMoveInContent) { this->isMoveInContent = isMoveInContent; }

	bool getResizeEnabled() const { return isResizeEnabled; }
	void setResizeEnabled(bool isResizeEnabled) { this->isResizeEnabled = isResizeEnabled; }

	bool getMaxState() const { return isMaxState; }
	bool getFullScreenState() const { return isFullScreenState; }

protected:
	bool isCursorPosition(CursorPosition cursorPosition) const { return this->cursorPosition == cursorPosition; }
	bool isForMove() const
	{
		if (!isActive || !isMouseButtonDown) {
			return false;
		} else if (isCursorPosition(CursorPosition::TitleBar)) {
			return true;
		} else if (!isMoveInContent) {
			return false;
		} else if (isCursorPosition(CursorPosition::Content)) {
			return true;
		}
		return false;
	}
	bool isForResize() const
	{
		if (!isActive || !isMouseButtonDown) {
			return false;
		} else if (isCursorPosition(CursorPosition::Left)) {
			return true;
		} else if (isCursorPosition(CursorPosition::Top)) {
			return true;
		} else if (isCursorPosition(CursorPosition::Right)) {
			return true;
		} else if (isCursorPosition(CursorPosition::Bottom)) {
			return true;
		} else if (isCursorPosition(CursorPosition::TopLeft)) {
			return true;
		} else if (isCursorPosition(CursorPosition::TopRight)) {
			return true;
		} else if (isCursorPosition(CursorPosition::BottomLeft)) {
			return true;
		} else if (isCursorPosition(CursorPosition::BottomRight)) {
			return true;
		}
		return false;
	}
	template<typename Control> bool isControl(QWidget *child)
	{
		if (this == child) {
			return false;
		} else if (dynamic_cast<Control *>(child)) {
			return true;
		} else {
			return isControl<Control>(child->parentWidget());
		}
	}
	bool isInControl(QWidget *child)
	{
		if (!child) {
			return false;
		} else if (this == child) {
			return false;
		} else if (isControl<QAbstractButton>(child)) {
			return true;
		} else if (isControl<QComboBox>(child)) {
			return true;
		} else if (isControl<QLineEdit>(child)) {
			return true;
		} else if (isControl<QTextEdit>(child)) {
			return true;
		} else if (isControl<QAbstractSpinBox>(child)) {
			return true;
		} else if (isControl<QListWidget>(child)) {
			return true;
		} else if (isInCustomControl(child)) {
			return true;
		}
		return false;
	}
	QRect fromTopLeftSize(const QPoint &topLeft, const QSize &size) { return QRect(topLeft.x(), topLeft.y(), size.width(), size.height()); }
	QRect fromTopRightSize(const QPoint &topRight, const QSize &size) { return QRect(topRight.x() - size.width(), topRight.y(), size.width(), size.height()); }
	QRect fromBottomLeftSize(const QPoint &bottomLeft, const QSize &size) { return QRect(bottomLeft.x(), bottomLeft.y() - size.height(), size.width(), size.height()); }
	QRect fromBottomRightSize(const QPoint &bottomRight, const QSize &size) { return QRect(bottomRight.x() - size.width(), bottomRight.y() - size.height(), size.width(), size.height()); }

public:
	void showMaximized()
	{
		if (!isMaxState) {
			InActiveHelper helper(this);

			isMaxState = true;
			flushMaxFullScreenStateStyle();
			onMaxFullScreenStateChanged();

			geometryOfNormal = geometry();
			onSaveNormalGeometry();

			extern QRect getScreenAvailableRect(QWidget * widget);
			QRect rcsa = getScreenAvailableRect(this);
			setGeometry(rcsa.x(), rcsa.y(), rcsa.width(), rcsa.height());
			activateWindow();
		}
	}
	void showFullScreen()
	{
		if (!isFullScreenState) {
			InActiveHelper helper(this);

			isFullScreenState = true;
			flushMaxFullScreenStateStyle();
			onMaxFullScreenStateChanged();

			if (!isMaxState) {
				geometryOfNormal = geometry();
				onSaveNormalGeometry();
			}

			extern QRect getScreenRect(QWidget * widget);
			QRect rcsa = getScreenRect(this);
			setGeometry(rcsa.x(), rcsa.y(), rcsa.width(), rcsa.height());
			activateWindow();
		}
	}
	void showNormal()
	{
		if (isMaxState && isFullScreenState) {
			InActiveHelper helper(this);

			isFullScreenState = false;
			flushMaxFullScreenStateStyle();
			onMaxFullScreenStateChanged();

			extern QRect getScreenAvailableRect(QWidget * widget);
			QRect rcsa = getScreenAvailableRect(this);
			setGeometry(rcsa.x(), rcsa.y(), rcsa.width(), rcsa.height());
			activateWindow();
		} else if (isMaxState || isFullScreenState) {
			InActiveHelper helper(this);

			isMaxState = false;
			isFullScreenState = false;
			onMaxFullScreenStateChanged();
			flushMaxFullScreenStateStyle();

			setGeometry(geometryOfNormal);
			activateWindow();
		}
	}

protected:
	virtual QRect titleBarRect() const = 0;
	virtual bool canMaximized() const = 0;
	virtual bool canFullScreen() const = 0;
	virtual bool isInCustomControl(QWidget *child) const { return false; }

	virtual void beginResizeSignal() = 0;
	virtual void endResizeSignal() = 0;
	virtual void flushMaxFullScreenStateStyle() = 0;
	virtual void onMaxFullScreenStateChanged() {}
	virtual void onSaveNormalGeometry() {}

protected:
	bool isActive = true;
	bool isMoveInContent = false;
	bool isResizeEnabled = true;
	bool isMaxState = false;
	bool isFullScreenState = false;
	bool isMouseButtonDown = false;
	bool isResizing = false;
	CursorPosition cursorPosition = CursorPosition::None;
	QPoint globalPosMousePress;
	QRect geometryOfMousePress;
	QPoint globalPosMouseMoveForResize;
	QTimer resizePausedCheckTimer;
	QRect geometryOfNormal;
};

#endif // PLSTOPLEVELVIEW_HPP
