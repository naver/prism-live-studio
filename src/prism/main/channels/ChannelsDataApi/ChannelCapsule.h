#ifndef CHANNELCAPSULE_H
#define CHANNELCAPSULE_H

#include <QFrame>
#include <QVariantMap>
#include <QStateMachine>
#include <QTimer>
class QLabel;
class ChannelConfigPannel;

namespace Ui {
class ChannelCapsule;
}

class ChannelCapsule : public QFrame {
	Q_OBJECT
public:
	explicit ChannelCapsule(QWidget *parent = nullptr);
	~ChannelCapsule();
	/*init ui and state */
	void initialize(const QVariantMap &srcData);
	/*update ui */
	void updateUi();
	/* update viewers and likes */
	void updateInfo();
	/*start update viewers and likes */
	void switchUpdateInfo(bool on = true);
	/*get if is active by user */
	bool isActive();
	/*switch busy state*/
	void holdOn(bool hold);

	const QString &getChannelID() { return mInfoID; }

protected:
	void changeEvent(QEvent *e);
	/* to show config pannel*/
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void shiftState(const QVariantMap &info);

	void RTMPTypeState(const QVariantMap &info);
	void channelTypeState(const QVariantMap &info);
	void normalState(const QVariantMap &info);

	void errorState(const QVariantMap &info);
	void unInitializeState(const QVariantMap &info);
	void waitingState(const QVariantMap &info);

	void nextPixmap();

	bool isPannelOutOfView();

private slots:
	void showConfigPannel();
	void hideConfigPannel();
	void checkConfigVisible();

private:
	Ui::ChannelCapsule *ui;
	QStateMachine mMachine;
	QString mInfoID;
	ChannelConfigPannel *mConfigPannel;
	QTimer mUpdateTimer;
	QLabel *mLoadingFrame;
	QTimer mbusyTimer;
	int mBusyCount;
	QTimer mConfigTimer;
};

#endif // CHANNELCAPSULE_H
