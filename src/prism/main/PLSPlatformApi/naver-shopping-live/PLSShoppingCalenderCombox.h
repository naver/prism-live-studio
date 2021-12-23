#ifndef PLSSHOPPINGCALENDERCOMBOX_H
#define PLSSHOPPINGCALENDERCOMBOX_H

#include <QPushButton>
#include "PLSDpiHelper.h"
#include "PLSShoppingComboxMenu.h"

class PLSShoppingCalenderCombox : public QPushButton {

	Q_OBJECT
public:
	explicit PLSShoppingCalenderCombox(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSShoppingCalenderCombox();
	void showDateCalender();
	void showHour(QString hour);
	void showMinute(QString minute);
	void showAp(QString ap);
	const QStringList &apList();
	void setDate(const QDate &date);
	const QDate &date();

signals:
	void clickDate(const QDate &date);
	void clickTime(QString time);

private:
	void showComboListView(bool show);

private:
	PLSShoppingComboxMenu *m_menu;
	QDate m_date;
};

#endif // PLSSHOPPINGCALENDERCOMBOX_H
