#include <QWidget>
#include <QTimer>
#include "pls-common-define.hpp"
#include "PLSSceneItemView.h"

enum class DragDirection { Left, Right, Top, Bottom, Unknown };

class PLSScrollAreaContent : public QWidget {
	Q_OBJECT
public:
	explicit PLSScrollAreaContent(QWidget *parent = nullptr);
	~PLSScrollAreaContent() final;
	int Refresh(DisplayMethod displayMethod, bool scrollBarVisible);
	void SetIsDraging(bool state);

protected:
	void paintEvent(QPaintEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void dragLeaveEvent(QDragLeaveEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	void GetRowColByPos(bool gridMode, const int &x, const int &y, const int &width, const int &height, int &row, int &col, bool append = false, int romoveRow = 0);
	void GetRowColByPosInListMode(const int &x, const int &y, const int &width, const int &height, int &row, int &col, int romoveRow);
	DragDirection GetDirectionByPos(bool gridMode, const int &x, const int &y, const int &width, const int &height, const int &row, const int &col);
	void SetDrawLineByPos(bool gridMode, const int &x, const int &y, const int &width, const int &height, const int &row, const int &col);
	void SetLinePos(const int &startX, const int &startY, const int &endX, const int &endY);
signals:
	void DragFinished();
	void DragMoving(int xPos, int yPos);
	void resizeEventChanged(bool);

private:
	QPoint lineStart;
	QPoint lineEnd;
	bool isDrag{false};
	QTimer timerAutoScroll;
	int autoScrollStepValue = 0;
};
