#ifndef GOLIVEPANNEL_H
#define GOLIVEPANNEL_H

#include <QFrame>
#include <QMenu>
#include <QStateMachine>

class PLSAddingFrame;

namespace Ui {
class GoLivePannel;
}

class GoLivePannel : public QFrame {
	Q_OBJECT

public:
	explicit GoLivePannel(QWidget *parent = nullptr);
	~GoLivePannel();
	void holdOnAll(bool holdOn);

protected:
	void changeEvent(QEvent *e);

private slots:

	void on_Record_toggled(bool isCheck);

	void toggleRecord(bool isStart = true);

	void updateRecordButton(int state);

	void on_GoLiveShift_toggled(bool isCheck);

	void toggleBroadcast(bool toStart = true);

	void updateGoliveButton(int state);

private:
	void setEnteredGolive(bool isEntered) { isEnteredGolive = isEntered; }
	void setEnteredRecord(bool isEntered) { isEnteredRecord = isEntered; }

private:
	Ui::GoLivePannel *ui;

	PLSAddingFrame *mBusyFrame;

	const QString goliveText;
	const QString finishLiveText;
	const QString finisheRehearsalText;

	volatile bool isEnteredGolive;
	volatile bool isEnteredRecord;
};

#endif // GOLIVEPANNEL_H
