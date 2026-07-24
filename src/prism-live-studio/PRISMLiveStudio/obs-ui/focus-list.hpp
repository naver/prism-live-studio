#pragma once

#include <QListWidget>
#include <QMouseEvent>
#include <QApplication>

class QDragMoveEvent;

class FocusList : public QListWidget {
	Q_OBJECT

public:
	FocusList(QWidget *parent);

	void SetEmptyWidget(QWidget *label);

protected:
	void focusInEvent(QFocusEvent *event) override;
	void startDrag(Qt::DropActions supportedActions) override;
	virtual void dragMoveEvent(QDragMoveEvent *event) override;
	virtual void dragLeaveEvent(QDragLeaveEvent *event) override;
	virtual void dragEnterEvent(QDragEnterEvent *event) override;
	virtual void dropEvent(QDropEvent *event) override;
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;

	void rowsInserted(const QModelIndex &parent, int start, int end) override;
	void rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end) override;

signals:
	void GotFocus();

private:
	void SetPaintLinePos(const int &startPosX, const int &startPosY, const int &endPosX, const int &endPosY);
	void CheckCountChanged(size_t count);

private:
	bool isDraging = false;
	QPoint lineStart{};
	QPoint lineEnd{};
	QWidget *emptyLabel = nullptr;
};
