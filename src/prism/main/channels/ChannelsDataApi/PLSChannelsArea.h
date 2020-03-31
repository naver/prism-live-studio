#ifndef CHANNELSAREA_H
#define CHANNELSAREA_H

#include <QFrame>
#include <QVariantMap>
#include "ChannelDefines.h"

class QPushButton;
class PLSAddingFrame;
class GoLivePannel;

namespace Ui {
class ChannelsArea;
}

class PLSChannelsArea : public QFrame {
	Q_OBJECT

public:
	explicit PLSChannelsArea(QWidget *parent = nullptr);
	~PLSChannelsArea();

	void initChannels();

	void appendTailWidget(QWidget *widget);

	void holdOnChannelArea(bool holdOn);

private slots:

	void addChannel(const QString &channelUUID);

	void removeChannel(const QString &channelUUID);

	void updateChannelUi(const QString &channelUUID);

	void refreshChannels();

	void showChannelsAdd();

	void switchAllChannelsState(bool on = true);

	void clearAll();

	void clearAllRTMP();

protected:
	void changeEvent(QEvent *e);

	void wheelEvent(QWheelEvent *event) override;

	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	void addChannel(const QVariantMap &channelInfo);

	bool checkIsEmptyUi();
	void updateUi();

	void insertChannelCapsule(QWidget *wid, int index = -1);

	void scrollNext(bool forwartStep = true);

	void displayScrollButtons(bool isShow = true);

	void holdOnChannel(const QString &uuid, bool holdOn);

	int getRTMPInsertIndex();

	int getChannelInsertIndex(const QString &platformName);

	void refreshOrder();

	void initScollButtons();

	bool isScrollButtonsNeeded();

private:
	Ui::ChannelsArea *ui;

	QMap<QString, ChannelData::ChannelCapsulePtr> mChannelsWidget;

	QPushButton *mLeftButon;

	QPushButton *mRightButton;

	QPushButton *addBtn;

	PLSAddingFrame *mbusyFrame;

	GoLivePannel *goliveWid;

	bool isHolding;
	QTimer mCheckTimer;
};

#endif // CHANNELSAREA_H
