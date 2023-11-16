/*
* @file		PLSLiveInfoTwitch.h
* @brief	To show twitch liveinfo dialog
* @author	wu.longyue@navercorp.com
* @date		2020-01-06
*/

#pragma once

#include <QListWidget>

#include "../PLSLiveInfoBase.h"
#include "PLSPlatformTwitch.h"

#include "ui_PLSLiveInfoTwitch.h"
#include "libbrowser.h"
#include <qtimer.h>

namespace Ui {
class PLSLiveInfoTwitch;
}

class PLSLiveInfoTwitchListWidget : public QListWidget {
	Q_OBJECT
public:
	explicit PLSLiveInfoTwitchListWidget(QWidget *parent) : QListWidget(parent) {}
};

class PLSLiveInfoTwitch : public PLSLiveInfoBase {
	Q_OBJECT
public:
	PLSLiveInfoTwitch(PLSPlatformBase *pPlatformBase, QWidget *parent = nullptr);
	~PLSLiveInfoTwitch() override;

protected:
	void showEvent(QShowEvent *event) override;

private:
	void getJs();

private slots:
	void doOk();

private:
	Ui::PLSLiveInfoTwitch *ui;
	QVariantMap m_categorys;
	QPushButton *pushButtonClear;
	QPushButton *pushButtonSearch;
	PLSLiveInfoTwitchListWidget *listCategory;
	pls::browser::BrowserWidget *m_browserWidget;
	QString m_jsCode;
	QTimer m_updateTimer;
	bool m_isUpdateOk = false;
	bool m_inputError = false;
};