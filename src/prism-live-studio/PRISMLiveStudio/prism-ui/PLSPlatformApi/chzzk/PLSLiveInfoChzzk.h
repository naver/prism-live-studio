#ifndef PLSLIVEINFOCHZZK_H
#define PLSLIVEINFOCHZZK_H

#include "../PLSLiveInfoBase.h"
#include <qevent.h>
#include "PLSSearchCombobox.h"
#include "PLSRadioButton.h"

class QShowEvent;
class QPushButton;

namespace Ui {
class PLSLiveInfoChzzk;
}
class PLSPlatformChzzk;
class PLSLiveInfoChzzk : public PLSLiveInfoBase {
	Q_OBJECT

public:
	explicit PLSLiveInfoChzzk(PLSPlatformBase *pPlatformBase, QWidget *parent = nullptr);
	~PLSLiveInfoChzzk() override;

protected:
	/**
	* show the loading UI, then request the api.
	*/
	void showEvent(QShowEvent *event) override;

private slots:
	void okButtonClicked();
	void cancelButtonClicked();
	void titleEdited();

private:
	void setupFirstUI();
	void refreshUI();
	void refreshThumButton();
	void setupGuideButton();
	void doUpdateOkState();
	template<typename T> void createWidget(QLabel *oldLabel, const QString &tooltip, QFormLayout *formLayout, QObject *sender, T singal);

private:
	Ui::PLSLiveInfoChzzk *ui;
	PLSPlatformChzzk *m_platform = nullptr;
	PLSRadioButtonGroup *m_chatGroups = nullptr;
	bool m_isClickedDeleteBtn = false;
};

#endif // PLSLIVEINFOCHZZK_H
