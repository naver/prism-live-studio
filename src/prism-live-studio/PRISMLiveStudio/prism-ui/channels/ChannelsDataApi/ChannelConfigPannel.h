#ifndef CHANNELCONFIGPANNEL_H
#define CHANNELCONFIGPANNEL_H
#include <qmenu.h>
#include <QActionGroup>
#include <QFrame>
#include <QMetaEnum>

#include "libbrowser.h"

class QPropertyAnimation;
namespace Ui {
class ChannelConfigPannel;
}

class ChannelConfigPannel : public QFrame {
	Q_OBJECT

public:
	enum OUTPUTDIRECTION {
		horizontal = 0,
		vertical = 1,
		none = 2,
	};
	Q_ENUM(OUTPUTDIRECTION)

	explicit ChannelConfigPannel(QWidget *parent = nullptr);
	~ChannelConfigPannel() override;
	void setChannelID(const QString &channelID);
	const QString &getChannelID() const { return mChannelID; }

	void updateUI();
	bool GetMeunShow() { return m_bMenuShow; }

	void setDualOutput(bool bOpen);
	void updateUISpacing(bool isDualOutput);

protected:
	void changeEvent(QEvent *e) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
	void on_showInfoBtn_clicked();

	void on_ShareBtn_clicked();

	void on_ConfigBtn_clicked();

	void on_EnableSwitch_toggled(bool checked);

	void askDeleteChannel();

	void onShowDualoutputMenu();

private:
	void shiftState(const QVariantMap &info);

	void toRTMPTypeState(bool isLiving);

	void toChannelTypeState(int dataState, const QVariantMap &info);

	void doChildrenExclusive(bool &retflag) const;

	void checkExclusiveChannel(bool &retflag);

	void setHorizontalOutputUI();
	void setVerticalOutputUI();
	void setNoSetDirectionUI();
	void closeDualOutputUI();
	void onClickVerticalOutput();
	void onClickHorizontalOutput();
	void onClickNoSetOutput();
	void resetActionsState();

	//private:
	Ui::ChannelConfigPannel *ui;
	QString mChannelID;
	bool mIsAsking = false;
	bool m_isChannelSwithed{false};
	bool m_bMenuShow = false;
	bool m_bDualoutputMenuShow = false;
	OUTPUTDIRECTION m_currentState{none};
	QMenu m_dualMenu;
};

#endif // CHANNELCONFIGPANNEL_H
