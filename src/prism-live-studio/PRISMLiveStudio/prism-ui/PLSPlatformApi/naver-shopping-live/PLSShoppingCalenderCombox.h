#ifndef PLSSHOPPINGCALENDERCOMBOX_H
#define PLSSHOPPINGCALENDERCOMBOX_H

#include <QPushButton>
#include "PLSShoppingComboxMenu.h"

class PLSShoppingCalenderCombox : public QPushButton {

	Q_OBJECT
public:
	explicit PLSShoppingCalenderCombox(QWidget *parent = nullptr);
	~PLSShoppingCalenderCombox() override = default;
	void showDateCalender();
	void showHour(QString hour);
	void showMinute(QString minute);
	void showAp(QString ap);
	const QStringList &apList() const;
	void setDate(const QDate &date);
	const QDate &date() const;

signals:
	void clickDate(const QDate &date);
	void clickTime(QString time);

private:
	void showComboListView(bool show);

	PLSShoppingComboxMenu *m_menu;
	QDate m_date;
};

#endif // PLSSHOPPINGCALENDERCOMBOX_H
