#ifndef PLSLIVEINFOYOUTUBE_H
#define PLSLIVEINFOYOUTUBE_H

#include <vector>
#include "../PLSLiveInfoBase.h"

#include <QLabel>
#include <QListWidgetItem>
#include <QMenu>
#include <QTimer>

#include "PLSScheduleCombox.h"
#include "PLSScheduleComboxMenu.h"
#include "PLSPlatformYoutube.h"

namespace Ui {
class PLSLiveInfoYoutube;
}

class PLSLiveInfoYoutube : public PLSLiveInfoBase {
	Q_OBJECT

public:
	explicit PLSLiveInfoYoutube(PLSPlatformBase *pPlatformBase, QWidget *parent = nullptr);
	~PLSLiveInfoYoutube() override;

protected:
	/**
	* show the loading UI, then request the api.
	*/
	void showEvent(QShowEvent *event) override;

private:
	Ui::PLSLiveInfoYoutube *ui;
	void setupFirstUI();
	std::vector<PLSScheComboxItemData> m_vecItemDatas;

	void refreshUI();

	void refreshPrivacy();
	void refreshLatency();

	void saveDateWhenClickButton();

	QString m_enteredID;
	bool m_isLiveStopped{false};
	/**
	* check whether the ui is changed,
	* for example, the user input another title
	*/

	void saveTempNormalDataWhenSwitch() const;

private slots:
	void okButtonClicked();
	void cancelButtonClicked();
	void rehearsalButtonClicked();

	void scheduleButtonClicked();
	void scheduleItemClick(const QString selectID);
	void reloadScheduleList();
	void selectScheduleCheck();

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

	void refreshRadios();
	void setKidsRadioButtonClick(bool checked = false);
	void setNotKidsRadioButtonClick(bool checked = false);
	void refreshUltraTipLabelVisible();

	void refreshThumbnailButton();
	void onImageSelected(const QString &imageFilePath) const;
	PLSYoutubeLiveinfoData::Latency getUILatency() const;
};

#endif // PLSLIVEINFOYOUTUBE_H
