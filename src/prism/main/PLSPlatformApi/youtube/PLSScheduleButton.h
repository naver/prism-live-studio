#ifndef PLSSCHEDULEBUTTON_H
#define PLSSCHEDULEBUTTON_H

#include <QPushButton>
#include <QLabel>
#include "PLSScheduleMenu.h"
#include <vector>

using namespace std;

class PLSScheduleButton : public QPushButton {
	Q_OBJECT
public:
	explicit PLSScheduleButton(QWidget *parent = nullptr);
	~PLSScheduleButton();
	void showScheduleMenu(const vector<ComplexItemData> &datas);

	bool isMenuNULL();
	bool getMenuHide();
	void setMenuHideIfNeed();

	void setButtonEnable(bool enable);

	void setupButton(QString title, QString time);
	void updateTitle(QString title);

signals:
	void menuItemClicked(const QString selectData);

private:
	enum class PLSScheduleButtonType {
		Ty_Normal,
		Ty_Hover,
		Ty_On,
		Ty_Disable,
	};

	void updateStyle(PLSScheduleButtonType type);
	PLSScheduleMenu *m_scheduleMenu;
	QLabel *m_schedultTimeLabel;
	QLabel *m_scheduletTitleLabel;
	QLabel *m_dropLabel;

protected:
	void enterEvent(QEvent *event);
	void leaveEvent(QEvent *event);
};

#endif // PLSSCHEDULEBUTTON_H
