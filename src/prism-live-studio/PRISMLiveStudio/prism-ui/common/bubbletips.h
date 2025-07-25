#ifndef BUBBLETIPS_H
#define BUBBLETIPS_H

#include <QFrame>
#include <QSet>
#include <QPointer>
#include <QBasicTimer>
#include <QDockWidget>
#include "PLSDock.h"

namespace Ui {
class bubbletips;
}

namespace BubbleTips {
enum TipsType { None, DualOutputAudioTrack };
}

static int bubbleInfiniteLoopDuration = -1;

class bubbletips : public QFrame {
	Q_OBJECT

public:
	explicit bubbletips(QWidget *buddy, const QString &txt, int pos_offset = 50, int duration = 5000, BubbleTips::TipsType tipsType = BubbleTips::None);
	~bubbletips();

	/* the anchor point is at the arrow*/
	void moveTo(const QPoint &pos);
	void addListenDockTitleWidget(PLSDockTitle *dockTitleWidget);

public slots:
	void onLinkActivated(const QString &link);
	void on_bubbleCloseButton_clicked();
	void onTopLevelChanged(bool topLevel);

signals:
	void clickTextLink(const QString &link);

protected:
	void paintEvent(QPaintEvent *event) override;
	void timerEvent(QTimerEvent *event) override;
	void showEvent(QShowEvent *event) override;
	bool eventFilter(QObject *watcher, QEvent *e) override;

private:
	void generateArrow();
	void doMove(const QPoint &pos);
	void initEventFilter();
	void uninitEventFilter();
	void updateSize();

	Ui::bubbletips *ui;
	QPixmap m_arrow;
	QPointer<QWidget> buddy;
	int duration = 5000;
	int m_arrowOffset = 50;
	QBasicTimer timer;
	QPoint displayPos;
	QSet<QObject *> ancestors;
	QPointer<PLSDockTitle> m_titleWidget;
};

/* You can show a bubble tip in the position of the buddy widget.*/
bubbletips *pls_show_bubble_tips(QWidget *buddy, const QPoint &local_pos, const QString &txt, int offset = 50, int durationMs = 5000, BubbleTips::TipsType tipsType = BubbleTips::None);

#endif // BUBBLETIPS_H
