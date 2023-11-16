#pragma once

#include <QLabel>
#include <QPushButton>
#include <vector>

#include "PLSScheduleComboxMenu.h"

class PLSScheduleCombox : public QPushButton {
	Q_OBJECT
public:
	enum class UesForType {
		Ty_V_Normal,
		Ty_H_Normal,
	};

	explicit PLSScheduleCombox(QWidget *parent = nullptr);
	void showScheduleMenu(const std::vector<PLSScheComboxItemData> &datas);

	bool isMenuNULL() const;
	bool getMenuHide() const;
	void setMenuHideIfNeed();

	void setButtonEnable(bool enable);

	void setupButton(const QString title, const QString time, bool showIcon);
	void setupButton(const QString title, const QString time);
	void setupButton(const PLSScheComboxItemData &data);
	void updateTitle(const QString title);
	const QListWidget *listWidget() const;

signals:
	void menuItemClicked(const QString selectData);
	void menuItemExpired(std::vector<QString> &ids);

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
	QLabel *m_iconLabel;
	QLabel *m_titleLabel;
	QLabel *m_dropLabel;
	QString m_titleString = {};
	bool m_showIcon = false;
	PLSScheComboxItemData m_showData;
	QTimer *m_pLeftTimer;
	void setupTimer();

protected:
	void enterEvent(QEnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
};
