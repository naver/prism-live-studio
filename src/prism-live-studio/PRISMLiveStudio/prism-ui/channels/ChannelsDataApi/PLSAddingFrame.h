#ifndef PLSADDINGFRAME_H
#define PLSADDINGFRAME_H

#include <QFrame>
#include <QTimer>
#include <memory>
#include "ui_PLSAddingFrame.h"
namespace Ui {
class PLSAddingFrame;
}

class PLSAddingFrame : public QFrame {
	Q_OBJECT

public:
	explicit PLSAddingFrame(QWidget *parent = nullptr);
	~PLSAddingFrame() override;

	void setSourceFirstFile(const QString &sfile);
	void setContent(const QString &txt) const;
	void start(int time = 300);
	void stop();

protected:
	void changeEvent(QEvent *e) override;

private:
	void nextFrame();

	//private:
	std::unique_ptr<Ui::PLSAddingFrame> ui = std::make_unique<Ui::PLSAddingFrame>();
	QTimer mUpdateTimer;
	QString mSourceFile;
	int mStep = 0;
};

#endif // PLSADDINGFRAME_H
