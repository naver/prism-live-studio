#ifndef PLSLIVEINFOYOUTUBE_H
#define PLSLIVEINFOYOUTUBE_H

#include "..\PLSLiveInfoBase.h"
#include <vector>

#include <QMenu>
#include <QTimer>
#include <QLabel>
#include <QListWidgetItem>
#include "PLSScheduleMenuItem.h"
#include "PLSScheduleMenu.h"

using namespace std;

namespace Ui {
class PLSLiveInfoYoutube;
}

class PLSLiveInfoYoutube : public PLSLiveInfoBase {
	Q_OBJECT

public:
	explicit PLSLiveInfoYoutube(PLSPlatformBase *pPlatformBase, QWidget *parent = nullptr);
	~PLSLiveInfoYoutube();

protected:
	/**
	* show the loading UI, then request the api.
	*/
	virtual void showEvent(QShowEvent *event) override;

private:
	Ui::PLSLiveInfoYoutube *ui;
	void setupFirstUI();
	vector<ComplexItemData> m_vecItemDatas;

	void refreshUI();

	void refreshPrivacy();

	QString m_enteredID;

	/**
	* check whether the ui is changed,
	* for example, the user input another title
	*/
	bool isModified();
private slots:
	void okButtonClicked();
	void cancelButtonClicked();

	void scheduleButtonClicked();
	void scheduleItemClick(const QString selectID);

	void titleEdited();
	void descriptionEdited();

	void refreshTitleDescri();
	void refreshCategory();
	void refreshSchedulePopButton();
	/**
	* check whether the "ok" button can be enabled.
	* 1. if some api request is failed, "ok" button can't be enabled.
	* 2. if some required content is empty, same as 1.
	* 3. if user didn't modify any content, same as 1.
	*/
	void doUpdateOkState();
};

#endif // PLSLIVEINFOYOUTUBE_H
