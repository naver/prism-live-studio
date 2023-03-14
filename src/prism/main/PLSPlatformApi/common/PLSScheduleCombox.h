#pragma once

#include <QLabel>
#include <QPushButton>
#include <vector>
#include "PLSDpiHelper.h"
#include "PLSScheduleComboxMenu.h"

using namespace std;

class PLSScheduleCombox : public QPushButton {
	Q_OBJECT
public:
	enum class UesForType {
		Ty_V_Normal,
		Ty_H_Normal,
	};

	explicit PLSScheduleCombox(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSScheduleCombox();
	void showScheduleMenu(const vector<PLSScheComboxItemData> &datas);

	bool isMenuNULL();
	bool getMenuHide();
	void setMenuHideIfNeed();

	void setButtonEnable(bool enable);

	void setupButton(const QString title, const QString time);
	void setupButton(const PLSScheComboxItemData &data);
	void updateTitle(const QString title);

signals:
	void menuItemClicked(const QString selectData);
	void menuItemExpired(vector<QString> &ids);

private slots:
	void updateDetailLabel();

private:
	enum class PLSScheduleComboxType {
		Ty_Normal,
		Ty_Hover,
		Ty_On,
		Ty_Disabled,
	};

	void updateStyle(PLSScheduleComboxType type);
	PLSScheduleComboxMenu *m_scheduleMenu;
	QLabel *m_detailLabel;
	QLabel *m_titleLabel;
	QLabel *m_dropLabel;
	QString m_titleString = {};

	PLSScheComboxItemData m_showData;
	QTimer *m_pLeftTimer;
	void setupTimer();

protected:
	void enterEvent(QEvent *event);
	void leaveEvent(QEvent *event);
	virtual void resizeEvent(QResizeEvent *event) override;
};
