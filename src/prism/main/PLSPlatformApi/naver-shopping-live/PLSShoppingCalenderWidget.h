#ifndef PLSSHOPPINGCALENDERWIDGET_H
#define PLSSHOPPINGCALENDERWIDGET_H

#include <QCalendarWidget>
#include <QTableView>
#include "PLSDpiHelper.h"

class QPushButton;
class QLabel;
class PLSShoppingCalenderWidget : public QCalendarWidget {
	Q_OBJECT
public:
	explicit PLSShoppingCalenderWidget(QWidget *parent = nullptr, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSShoppingCalenderWidget();
	bool eventFilter(QObject *watched, QEvent *event) override;
	void showSelectedDate(const QDate &date);

private:
	void initControl();
	void initTopWidget();
	QDate dateForCell(int row, int column);
	QDate referenceDate();
	int columnForDayOfWeek(int day);
	int columnForFirstOfMonth(QDate date);
	void updateDateCell();
	bool isValidDate(QDate date);
	int getValueForDpi(int value);

protected:
	void paintCell(QPainter *painter, const QRect &rect, const QDate &date) const;

signals:
	void sizeChanged(QSize size);

private slots:
	void onbtnClicked();

private:
	QTableView *table_view;
	QMap<QString, QDate> m_dateMap;
	QPushButton *m_leftMonthBtn;
	QPushButton *m_rightMonthBtn;
	QWidget *m_topWidget;
	QLabel *m_dataLabel;
	QDate m_currentDate;
	QDate m_hoverDate;
	QDate m_showSelectedDate;
};

#endif // PLSSHOPPINGCALENDERWIDGET_H
