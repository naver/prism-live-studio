#ifndef PLSLIVEINFONCB2B_H
#define PLSLIVEINFONCB2B_H

#include <QWidget>
#include "../PLSLiveInfoBase.h"
#include "PLSScheduleCombox.h"
#include "PLSScheduleComboxMenu.h"

class PLSPlatformNCB2B;

namespace Ui {
class PLSLiveInfoNCB2B;
}

class PLSLiveInfoNCB2B : public PLSLiveInfoBase {
	Q_OBJECT

public:
	explicit PLSLiveInfoNCB2B(PLSPlatformBase *pPlatformBase, QWidget *parent = nullptr);
	~PLSLiveInfoNCB2B() override;

private:
	Ui::PLSLiveInfoNCB2B *ui;
	void setupFirstUI();
	std::vector<PLSScheComboxItemData> m_vecItemDatas;
	PLSPlatformNCB2B *m_platform;

	void refreshUI();

	void refreshPrivacy();

	void saveDateWhenClickButton();

	QString m_enteredID;
	/**
	* check whether the ui is changed,
	* for example, the user input another title
	*/

	void saveTempNormalDataWhenSwitch() const;
	void doUpdateOkState();

protected:
	/**
	* show the loading UI, then request the api.
	*/
	void showEvent(QShowEvent *event) override;

private slots:

	void okButtonClicked();
	void cancelButtonClicked();
	void titleEdited();

	void scheduleButtonClicked();
	void scheduleItemClick(const QString selectID);
	void reloadScheduleList();

	void descriptionEdited();

	void refreshTitleDescri();
	void refreshSchedulePopButton();
};

#endif // PLSLIVEINFONCB2B_H
