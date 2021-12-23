#ifndef RESOLUTIONTIPFRAME_H
#define RESOLUTIONTIPFRAME_H

#include <QFrame>
#include <QImage>
namespace Ui {
class ResolutionTipFrame;
}

class ResolutionTipFrame : public QFrame {
	Q_OBJECT

public:
	ResolutionTipFrame(QWidget *parent = nullptr);
	~ResolutionTipFrame();
	void setText(const QString &txt);
	void updateBackground();
public slots:

	void on_CloseBtn_clicked();

protected:
	void changeEvent(QEvent *e);
	void resizeEvent(QResizeEvent *event);
	void paintEvent(QPaintEvent *evnt) override;

private:
	Ui::ResolutionTipFrame *ui;
	QImage mBackground;
	QPainterPath mPath;
};

#endif // RESOLUTIONTIPFRAME_H
