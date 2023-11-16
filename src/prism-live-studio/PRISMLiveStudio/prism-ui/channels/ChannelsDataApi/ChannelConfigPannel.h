#ifndef CHANNELCONFIGPANNEL_H
#define CHANNELCONFIGPANNEL_H
#include <QFrame>
#include "libbrowser.h"
class QPropertyAnimation;
namespace Ui {
class ChannelConfigPannel;
}

class ChannelConfigPannel : public QFrame {
	Q_OBJECT

public:
	explicit ChannelConfigPannel(QWidget *parent = nullptr);
	~ChannelConfigPannel() override;
	void setChannelID(const QString &channelID);
	const QString &getChannelID() const { return mChannelID; }

	void updateUI();
	bool GetMeunShow() { return m_bMenuShow; }

protected:
	void changeEvent(QEvent *e) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
	void on_showInfoBtn_clicked();

	void on_ShareBtn_clicked();

	void on_ConfigBtn_clicked();

	void on_EnableSwitch_toggled(bool checked);

	void askDeleteChannel();

	void closeBowser();

private:
	void shiftState(const QVariantMap &info);

	void toRTMPTypeState(bool isLiving);

	void toChannelTypeState(int dataState, const QVariantMap &info);

	void doChildrenExclusive(bool &retflag) const;

	void checkExclusiveChannel(bool &retflag);

	//private:
	Ui::ChannelConfigPannel *ui;
	QString mChannelID;
	bool mIsAsking = false;
	QPointer<pls::browser::BrowserDialog> m_Browser = nullptr;
	bool m_isChannelSwithed{false};
	bool m_bMenuShow = false;
};

#endif // CHANNELCONFIGPANNEL_H
