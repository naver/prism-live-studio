#ifndef CHANNELFOLDCAPSULE_H
#define CHANNELFOLDCAPSULE_H

#include <QFrame>
#include <QTimer>
#include <QVariantMap>
class QLabel;
class ChannelConfigPannel;

namespace Ui {
class ChannelFoldCapsule;
}

class ChannelFoldCapsule : public QFrame {
	Q_OBJECT
public:
	explicit ChannelFoldCapsule(QWidget *parent = nullptr);
	~ChannelFoldCapsule() override;
	/*init ui and state */
	void setChannelID(const QString &uuid);
	/*update ui */
	void updateUi();

	bool isSelectedDisplay() const;

	const QString &getChannelID() const { return mInfoID; }

protected:
	void changeEvent(QEvent *e) override;
	/* to show config pannel*/
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	/* update viewers and likes */
	bool updateStatisticInfo();
	/*start update viewers and likes */
	bool switchUpdateStatisticInfo(bool on = true);

	void updateTextFrames(const QVariantMap &srcData);
	void delayUpdateTextAndIcons();
	void updateIcons(const QVariantMap &srcData);

	void shiftState(const QVariantMap &srcData);

	void channelTypeState(const QVariantMap &info);
	void normalState(const QVariantMap &info);

	void errorState(const QVariantMap &info);
	void unInitializeState(const QVariantMap &info);
	void waitingState(const QVariantMap &info);
	void updateErrorLabel(const QVariantMap &info);

	QString getStatisticsImage(const QString &src, bool isEnabled = true);
	QString &formatNumber(QString &number, bool isEng = true);
	QString createStatisticsCss(const QString &srcImage);

private:
	Ui::ChannelFoldCapsule *ui;

	QString mInfoID;
	ChannelConfigPannel *mConfigPannel;
	QTimer mUpdateTimer;

	QVariantMap mLastMap;
};

#endif // ChannelFoldCapsule_H
