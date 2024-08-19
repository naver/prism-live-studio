#pragma once

#include <QListWidget>

class QDragMoveEvent;

class FocusList : public QListWidget {
	Q_OBJECT

public:
	FocusList(QWidget *parent);

protected:
	void focusInEvent(QFocusEvent *event) override;
	virtual void dragMoveEvent(QDragMoveEvent *event) override;
	virtual void dragLeaveEvent(QDragLeaveEvent* event) override;
	virtual void dragEnterEvent(QDragEnterEvent* event) override;
	virtual void dropEvent(QDropEvent* event) override;
	void paintEvent(QPaintEvent* event) override;
signals:
	void GotFocus();

private:
	void SetPaintLinePos(const int& startPosX, const int& startPosY, const int& endPosX, const int& endPosY);

private:
	bool isDraging = false;
	QPoint lineStart{};
	QPoint lineEnd{};
};
