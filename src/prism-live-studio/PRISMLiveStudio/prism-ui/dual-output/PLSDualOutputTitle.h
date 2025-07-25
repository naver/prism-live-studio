#pragma once

#include <QFrame>
#include <QPushButton>
#include <QRadioButton>
#include <QLabel>
#include <QHBoxLayout>

#include "pls-channel-const.h"

class PLSDualOutputTitle : public QFrame {
	Q_OBJECT

public:
	PLSDualOutputTitle(QWidget *parent = nullptr);

public slots:
	void onPlatformChanged(const QString &uuid, ChannelData::ChannelDualOutput outputType);

	void addPlatformIcon(const QString &uuid, bool bMain);
	bool removePlatformIcon(QHBoxLayout *layout, const QString &uuid);
	void removePlatformIcon(const QString &uuid);

	void initPlatformIcon();

	void showHorizontalDisplay(bool);
	void showVerticalDisplay(bool);

	void checkSettingButtonWidth();

	void checkHPlatformTooLarge();
	void checkHPlatformIsEnough();

	void checkVPlatformTooLarge();
	void checkVPlatformIsEnough();

protected:
	bool isIconExists(QHBoxLayout *layout, const QString &uuid);

	void resizePaltformParent();

	virtual void resizeEvent(QResizeEvent *event) override;
	virtual bool eventFilter(QObject *watched, QEvent *event) override;

private:
	QPushButton *m_buttonHPreview = nullptr;
	QPushButton *m_buttonVPreview = nullptr;

	QWidget *m_widgetLeft = nullptr;
	QWidget *m_widgetHPlatform = nullptr;
	QLabel *m_labelHPlatformOutput = nullptr;
	QLabel *m_labelHPlatformCircle = nullptr;

	QWidget *m_widgetRight = nullptr;
	QWidget *m_widgetVPlatform = nullptr;
	QLabel *m_labelVPlatformOutput = nullptr;
	QLabel *m_labelVPlatformCircle = nullptr;

	QHBoxLayout *m_layoutHPlatform = nullptr;
	QHBoxLayout *m_layoutVPlatform = nullptr;

	QRadioButton *m_buttonSetting = nullptr;
	int m_ibuttonSettingWidth = 0;

	bool m_bAllowChecking = true;

	bool m_bShowHPlatformOutput = true;
	int m_iHPlatformOutputWidth = 0;

	bool m_bShowVPlatformOutput = true;
	int m_iVPlatformOutputWidth = 0;

	bool m_bShowHPlatforms = true;
	int m_iHPlatformsWidth = 0;

	bool m_bShowVPlatforms = true;
	int m_iVPlatformsWidth = 0;
};
