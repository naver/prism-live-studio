#ifndef GOLIVEPANNEL_H
#define GOLIVEPANNEL_H

#include <QFrame>
#include <QMenu>
#include "ChannelCommonFunctions.h"
class PLSAddingFrame;

namespace Ui {
class GoLivePannel;
}

class GoLivePannel : public QFrame {
	Q_OBJECT

public:
	explicit GoLivePannel(QWidget *parent = nullptr);
	~GoLivePannel() override;
	void holdOnAll(bool holdOn);

protected:
	void changeEvent(QEvent *e) override;

private slots:

	void on_Record_toggled(bool isCheck);

	void toggleRecord(bool isStart = true);

	void updateRecordButton(int state);

	void on_GoLiveShift_toggled(bool isCheck);

	void toggleBroadcast(bool toStart = true);

	void updateGoliveButton(int state);

private:
	bool confirmToContinue() const;

	void setEnteredGolive(bool isEntered) { isEnteredGolive = isEntered; }
	void setEnteredRecord(bool isEntered) { isEnteredRecord = isEntered; }

	//private:
	Ui::GoLivePannel *ui;

	PLSAddingFrame *mBusyFrame;

	const QString goliveText{CHANNELS_TR(GoLive)};
	const QString finishLiveText{CHANNELS_TR(FinishLive)};
	const QString finisheRehearsalText{CHANNELS_TR(FinishRehearsal)};

	bool isEnteredGolive = false;
	bool isEnteredRecord = false;
};

#endif // GOLIVEPANNEL_H
