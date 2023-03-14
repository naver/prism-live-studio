#ifndef PLSSHOPPINGCOMBOXMENU_H
#define PLSSHOPPINGCOMBOXMENU_H

#include <QDialog>
#include <QListWidget>
#include "PLSDpiHelper.h"
#include "PLSShoppingCalenderWidget.h"

enum class DateTimeType { DateHour, DateMinute };

class PLSShoppingComboxMenu : public QDialog {
	Q_OBJECT
public:
	explicit PLSShoppingComboxMenu(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSShoppingComboxMenu();
	PLSShoppingCalenderWidget *calender();
	QListWidget *listWidget();
	void showDateCalender(const QDate &date);
	void showHour(QString hour);
	void showMinute(QString minute);
	void showAp(QString ap);
	const QStringList &apList();
	bool eventFilter(QObject *watched, QEvent *event);

private:
	void setSizePolicy(QWidget *widget);
	void showDateTimeList(DateTimeType type, QString selectedTime);

protected:
	void showEvent(QShowEvent *event) override;
	void hideEvent(QHideEvent *event) override;
signals:
	void isShow(bool isVisiable);

private:
	PLSShoppingCalenderWidget *m_calenderWidget;
	QListWidget *m_listWidget;
	QList<QString> m_timeList;
	QStringList m_apList;
};

#endif // PLSSHOPPINGCOMBOXMENU_H
