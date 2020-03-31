/*
* @file		PLSLiveInfoTwitch.h
* @brief	To show twitch liveinfo dialog
* @author	wu.longyue@navercorp.com
* @date		2020-01-06
*/

#pragma once

#include <QListWidget>

#include "..\PLSLiveInfoBase.h"
#include "PLSPlatformTwitch.h"

#include "ui_PLSLiveInfoTwitch.h"

namespace Ui {
class PLSLiveInfoTwitch;
}

class PLSLiveInfoTwitch : public PLSLiveInfoBase {
	Q_OBJECT
public:
	PLSLiveInfoTwitch(PLSPlatformBase *pPlatformBase, QWidget *parent = nullptr);
	~PLSLiveInfoTwitch();

protected:
	virtual void showEvent(QShowEvent *event) override;

private:
	void refreshChannel(PLSPlatformApiResult);
	void refreshServer(PLSPlatformApiResult);
	void showListCategory();
	bool isModified();
	void titleEdited();
private slots:
	void doOk();
	void doGetCategory(QJsonObject root);
	void doUpdateChannel(PLSPlatformApiResult);
	void doUpdateOkState();

private:
	Ui::PLSLiveInfoTwitch *ui;

	QPushButton *pushButtonClear;

	QListWidget *listCategory;
};
