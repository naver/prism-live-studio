#ifndef BUBBLETIPS_H
#define BUBBLETIPS_H

#include <QFrame>
#include <QSet>
#include <QPointer>
#include <QBasicTimer>
#include <QDockWidget>

namespace Ui {
class bubbletips;
}

namespace BubbleTips {
enum ArrowPosition { Top, Right, Bottom, Left };
}

class bubbletips : public QFrame {
	Q_OBJECT

public:
	explicit bubbletips(QWidget *buddy, BubbleTips::ArrowPosition arrowPos, const QString &txt, int pos_offset = 50, int duration = 5000);
	~bubbletips();

	/* pos_offfset: the poition offset(0:left ~ 50:center ~ 100:right) from left/top to right/bottom */
	void setArrowPosition(BubbleTips::ArrowPosition pos, int pos_offset = 50);
	/* the anchor point is at the arrow*/
	void moveTo(const QPoint &pos);

public slots:
	void onTopLevelChanged(bool topLevel);

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void timerEvent(QTimerEvent *event) override;
	void moveEvent(QMoveEvent *event) override;
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *watcher, QEvent *e) override;

private:
	void generateArrow(BubbleTips::ArrowPosition pos);
	void doMove(const QPoint &pos);
	void initEventFilter();
	void uninitEventFilter();

	Ui::bubbletips *ui;
	QImage mArrow;
	QPointer<QWidget> buddy;
	BubbleTips::ArrowPosition mPos = BubbleTips::Right;
	int dpi = 100;
	int duration = 5000;
	int offset = 50;
	QBasicTimer timer;
	QPoint displayPos;
	QPointer<QWidget> toplevelWidget;
	QPointer<QDockWidget> dockWidget;
	QSet<QObject *> ancestors;
};

/* You can show a bubble tip in the position of the buddy widget.*/
bool pls_show_bubble_tips(QWidget *buddy, BubbleTips::ArrowPosition arrowPos, const QPoint &local_pos, const QString &txt, int offset = 50, int durationMs = 5000);

#endif // BUBBLETIPS_H
