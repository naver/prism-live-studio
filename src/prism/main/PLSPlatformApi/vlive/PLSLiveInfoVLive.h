#pragma once

#include <QCheckBox>
#include <QWidget>
#include <vector>
#include "..\PLSLiveInfoBase.h"
#include "PLSScheduleCombox.h"

namespace Ui {
class PLSLiveInfoVLive;
}

class PLSPlatformVLive;

class PLSLiveInfoVLive : public PLSLiveInfoBase {
	Q_OBJECT

public:
	explicit PLSLiveInfoVLive(PLSPlatformBase *pPlatformBase, QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSLiveInfoVLive();

protected:
	/**
	* show the loading UI, then request the api.
	*/
	virtual void showEvent(QShowEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;

private:
	Ui::PLSLiveInfoVLive *ui;
	PLSPlatformVLive *m_platform;

	void setupFirstUI();
	vector<PLSScheComboxItemData> m_vecItemDatas;
	void refreshUI();
	void refreshTitleEdit();
	void refreshThumbnailButton();

	QString m_enteredID;
	QPixmap m_thumPix;
	/**
	* check whether the ui is changed,
	* for example, the user input another title
	*/
	bool isModified();
	QSize getNeedDialogSize(double dpi);

private slots:

	void okButtonClicked();
	void cancelButtonClicked();
	void rehearsalButtonClicked();

	void saveDateWhenClickButton();

	void scheduleButtonClicked();
	void scheduleItemClick(const QString selectID);
	void scheduleItemExpired(vector<QString> &ids);

	void profileComboxClicked();
	void profileMenuItemClick(const QString selectID);
	void updateProfileComboxItems();

	void boardComboxClicked();
	void boardMenuItemClick(const QString selectID);
	void updateBoardComboxItems();

	void titleEdited();

	void refreshSchedulePopButton();

	void updateScheduleComboxItems();
	/**
	* check whether the "ok" button can be enabled.
	* 1. if some api request is failed, "ok" button can't be enabled.
	* 2. if some required content is empty, same as 1.
	* 3. if user didn't modify any content, same as 1.
	*/
	void doUpdateOkState();

	void onImageSelected(const QString &imageFilePath);

	void saveTempNormalDataWhenSwitch();

private:
	void updateUIEnable();
	void updateProfileAndBoardUI();
	void updateBoardUserLabelElid();
	void updateCountryLabels();
	void getBoardDetailData();
};
