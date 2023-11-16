#ifndef PLSSHOPPINGCALENDERWIDGET_H
#define PLSSHOPPINGCALENDERWIDGET_H

#include <QCalendarWidget>
#include <QTableView>

class QPushButton;
class QLabel;
class PLSShoppingCalenderWidget : public QCalendarWidget {
	Q_OBJECT
public:
	explicit PLSShoppingCalenderWidget(QWidget *parent = nullptr);
	~PLSShoppingCalenderWidget() override = default;
	bool eventFilter(QObject *watched, QEvent *event) override;
	void showShoppingSelectedDate(const QDate &date);

private:
	void initControl();
	void initTopWidget();
	QDate dateForCell(int row, int column) const;
	QDate referenceDate() const;
	int columnForDayOfWeek(int day) const;
	int columnForFirstOfMonth(QDate date) const;
	void updateDateCell();
	bool isValidDate(QDate date) const;
	int getValueForDpi(int value);

protected:
	void showEvent(QShowEvent *event) override;
	void paintCell(QPainter *painter, const QRect &rect, QDate date) const override;

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
	static double g_dpi;
};

#endif // PLSSHOPPINGCALENDERWIDGET_H
