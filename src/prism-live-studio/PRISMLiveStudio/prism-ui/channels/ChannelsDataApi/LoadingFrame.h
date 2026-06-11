#ifndef LOADINGFRAME_H
#define LOADINGFRAME_H

#include <QFrame>
#include <QImage>
#include <QPixmap>
#include <QTimer>
#include <memory>
#include "ui_LoadingFrame.h"

/* this is UI for busy view while requesting */
namespace Ui {
class LoadingFrame;
}

class LoadingFrame : public QFrame {
	Q_OBJECT

public:
	explicit LoadingFrame(QWidget *parent = nullptr);
	~LoadingFrame() override;
	void initialize(const QString &file, int tick = 300);
public slots:

	void setImage(const QString &file);
	void setTick(int mssecond = 300);
	void next();
	void start();

	void setTitleVisible(bool isVisible = true) const;

protected:
	void changeEvent(QEvent *e) override;
	void paintEvent(QPaintEvent *event) override;

private:
	std::unique_ptr<Ui::LoadingFrame> ui = std::make_unique<Ui::LoadingFrame>();

	QTimer mTimer;
	int mTick = 300;
	QImage mPic;
	int mStepCount = 0;
};

#endif // LOADINGFRAME_H
