#ifndef RESOLUTIONTIPFRAME_H
#define RESOLUTIONTIPFRAME_H

#include <QFrame>
#include <QImage>
#include <QPainterPath>

namespace Ui {
class ResolutionTipFrame;
}

class ResolutionTipFrame : public QFrame {
	Q_OBJECT

public:
	explicit ResolutionTipFrame(QWidget *parent = nullptr);
	~ResolutionTipFrame() override;
	void setText(const QString &txt);
	void setBackgroundColor(const QColor &color);
	void updateBackground();

	void updateUI();
	void calculatePos();
	void delayCalculate();
	void aligin();

	void setAliginWidget(QWidget *aliginWidget);
public slots:

	void on_CloseBtn_clicked();
	void delayShow(bool visible, int time = 200 /*ms*/);

protected:
	void changeEvent(QEvent *e) override;
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *evnt) override;

private:
	Ui::ResolutionTipFrame *ui;
	QImage mBackground;
	QPainterPath mPath;
	QColor mBackgroundColor{"#666666"};
	QWidget *mAliginWidget = nullptr;
	QTimer *mDelayTimer = nullptr;
};

#endif // RESOLUTIONTIPFRAME_H
