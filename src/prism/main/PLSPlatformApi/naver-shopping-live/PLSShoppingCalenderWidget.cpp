#include "PLSShoppingCalenderWidget.h"
#include <QLayout>
#include <QTextCharFormat>
#include <QPainter>
#include <QLocale>
#include <QDebug>
#include <QAbstractItemModel>
#include <QHeaderView>
#include <QTimer>
#include <QEvent>
#include <QMap>
#include <QPushButton>
#include <QLabel>
#include <qglobal.h>
#include "PLSDpiHelper.h"
#include "frontend-api.h"

const int CalenderBottomPadding = 11;
const int CalenderLeftPadding = 9;
const int CalenderHeaderHeight = 43;
const int CalenderCellHeight = 26;
const int CalenderCellWeight = 31;
const int SelectDateCellWidth = 23;
#define CALENDER_TEXT_FONT_SIZE 11
static int g_dpi = 0;
const QString g_ontFamilyEn = "Segoe UI";
const QString g_tipFontFamilyKr = "Malgun Gothic";

PLSShoppingCalenderWidget::PLSShoppingCalenderWidget(QWidget *parent, PLSDpiHelper dpiHelper) : QCalendarWidget(parent)
{
	layout()->setSizeConstraint(QLayout::SetFixedSize);
	setNavigationBarVisible(false);
	setDateEditEnabled(false);
	initTopWidget();
	table_view = findChild<QTableView *>("qt_calendar_calendarview");
	table_view->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Fixed);
	table_view->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::Fixed);
	table_view->setMouseTracking(true);
	table_view->itemDelegateForRow(0);
	table_view->installEventFilter(this);
	dpiHelper.notifyDpiChanged(this, [this](double dpi) {
		g_dpi = dpi;
		int cellWidth = getValueForDpi(CalenderCellWeight);
		table_view->horizontalHeader()->setDefaultSectionSize(cellWidth);
		table_view->horizontalHeader()->setMinimumSectionSize(cellWidth);
		table_view->horizontalHeader()->setMaximumSectionSize(cellWidth);
		int cellHeight = getValueForDpi(CalenderCellHeight);
		table_view->verticalHeader()->setDefaultSectionSize(cellHeight);
		table_view->verticalHeader()->setMinimumSectionSize(cellHeight);
		table_view->verticalHeader()->setMaximumSectionSize(cellHeight);
		updateDateCell();
	});
	initControl();
	dpiHelper.setCss(this, {PLSCssIndex::PLSShoppingCalenderWidget});
	QDate currentDate = QDate::currentDate();
	m_currentDate = currentDate.addDays(-14);
}

PLSShoppingCalenderWidget::~PLSShoppingCalenderWidget() {}

bool PLSShoppingCalenderWidget::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == table_view) {
		if (event->type() == QEvent::MouseMove) {
			QPoint pos = dynamic_cast<QMouseEvent *>(event)->pos();
			QModelIndex index = table_view->indexAt(pos);
			m_hoverDate = dateForCell(index.row(), index.column());
			if (m_hoverDate < minimumDate() || m_hoverDate > maximumDate()) {
				m_hoverDate = QDate();
			}
			update();
		} else if (event->type() == QEvent::Leave) {
			m_hoverDate = QDate();
			update();
		}
	}
	return QCalendarWidget::eventFilter(watched, event);
}

void PLSShoppingCalenderWidget::showSelectedDate(const QDate &date)
{
	m_showSelectedDate = date;
}

void PLSShoppingCalenderWidget::initControl()
{
	QTextCharFormat sundayWeekdayTextFormat;
	QColor sundayColor(195, 65, 81);
	sundayWeekdayTextFormat.setForeground(sundayColor);

	QTextCharFormat saturdayWeekdayTextFormat;
	QColor saturdayColor(255, 255, 255);
	saturdayWeekdayTextFormat.setForeground(saturdayColor);

	//weekday show first letter
	setWeekdayTextFormat(Qt::Saturday, saturdayWeekdayTextFormat);
	setWeekdayTextFormat(Qt::Sunday, sundayWeekdayTextFormat);
	setHorizontalHeaderFormat(QCalendarWidget::SingleLetterDayNames);
	setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
	setFirstDayOfWeek(Qt::Sunday);

	setLocale(QLocale::English);
	if (IS_KR()) {
		setLocale(QLocale::Korean);
	}
	connect(this, &QCalendarWidget::currentPageChanged, this, [=](int /*year*/, int /*month*/) { updateDateCell(); });
	updateDateCell();
}

void PLSShoppingCalenderWidget::initTopWidget()
{
	m_topWidget = new QWidget(this);
	m_topWidget->setObjectName("CalendarTopWidget");
	m_topWidget->setFixedHeight(CalenderHeaderHeight);
	m_topWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	QHBoxLayout *hboxLayout = new QHBoxLayout;
	hboxLayout->setContentsMargins(0, 0, 0, 0);
	hboxLayout->setSpacing(0);

	m_leftMonthBtn = new QPushButton(this);
	m_leftMonthBtn->setObjectName("CalendarLeftMonthBtn");

	m_rightMonthBtn = new QPushButton(this);
	m_rightMonthBtn->setObjectName("CalendarRightMonthBtn");

	m_dataLabel = new QLabel(this);
	m_dataLabel->setObjectName("CalendarDataLabel");
	m_dataLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

	hboxLayout->addWidget(m_dataLabel);
	hboxLayout->addStretch();
	hboxLayout->addWidget(m_leftMonthBtn);
	hboxLayout->addSpacing(getValueForDpi(20));
	hboxLayout->addWidget(m_rightMonthBtn);
	hboxLayout->addSpacing(getValueForDpi(18));
	m_topWidget->setLayout(hboxLayout);

	QVBoxLayout *vBodyLayout = qobject_cast<QVBoxLayout *>(layout());
	vBodyLayout->insertWidget(0, m_topWidget);
	connect(m_leftMonthBtn, SIGNAL(clicked()), this, SLOT(onbtnClicked()));
	connect(m_rightMonthBtn, SIGNAL(clicked()), this, SLOT(onbtnClicked()));
}

QDate PLSShoppingCalenderWidget::dateForCell(int row, int column)
{
	int firstRow = 1;
	if (horizontalHeaderFormat() == HorizontalHeaderFormat::NoHorizontalHeader) {
		firstRow = 0;
	}
	int firstColumn = 1;
	if (verticalHeaderFormat() == VerticalHeaderFormat::NoVerticalHeader) {
		firstColumn = 0;
	}
	int rowCount = 6;
	int columnCount = 7;
	if (row < firstRow || row > (firstRow + rowCount - 1) || column < firstColumn || column > (firstColumn + columnCount - 1)) {
		return QDate();
	}
	QDate refDate = referenceDate();
	if (!refDate.isValid()) {
		return QDate();
	}
	int columnForFirstOfShownMonth = columnForFirstOfMonth(refDate);
	if (columnForFirstOfShownMonth - firstColumn < 1) {
		row -= 1;
	}
	int requestedDay = 7 * (row - firstRow) + column - columnForFirstOfShownMonth - refDate.day() + 1;
	return refDate.addDays(requestedDay);
}

QDate PLSShoppingCalenderWidget::referenceDate()
{
	int refDay = 1;
	while (refDay <= 31) {
		QDate refDate = QDate(yearShown(), monthShown(), refDay);
		if (refDate.isValid()) {
			return refDate;
		}
		refDay += 1;
	}
	return QDate();
}

int PLSShoppingCalenderWidget::columnForDayOfWeek(int day)
{
	int firstColumn = 1;
	if (verticalHeaderFormat() == VerticalHeaderFormat::NoVerticalHeader) {
		firstColumn = 0;
	}
	if (day < 1 || day > 7) {
		return -1;
	}
	int column = day - int(firstDayOfWeek());
	if (column < 0) {
		column += 7;
	}
	return column + firstColumn;
}

int PLSShoppingCalenderWidget::columnForFirstOfMonth(QDate date)
{
	return (columnForDayOfWeek(date.dayOfWeek()) - (date.day() % 7) + 8) % 7;
}

void PLSShoppingCalenderWidget::updateDateCell()
{
	int showRow = 1;
	for (int row = table_view->model()->rowCount() - 1; row > 0; row--) {
		QDate startDate = dateForCell(row, 0);
		QDate endDate = dateForCell(row, table_view->model()->columnCount() - 1);
		if (!isValidDate(startDate) && !isValidDate(endDate)) {
			table_view->setRowHidden(row, true);
		} else {
			showRow += 1;
			table_view->setRowHidden(row, false);
		}
	}

	int width = getValueForDpi(CalenderCellWeight) * table_view->model()->columnCount() + 2 * getValueForDpi(CalenderLeftPadding);
	int height = getValueForDpi(CalenderCellHeight) * showRow + getValueForDpi(CalenderBottomPadding);
	table_view->setFixedSize(width, height);
	int view_height = height + getValueForDpi(CalenderHeaderHeight);
	setFixedSize(width, view_height);
	m_topWidget->setFixedHeight(getValueForDpi(CalenderHeaderHeight));
	emit sizeChanged(QSize(width, view_height));

	QDate start = QDate(yearShown(), monthShown(), 1);
	QLocale locale = QLocale::English;
	QString dateText = locale.toString(start, "MMMM yyyy");
	if (IS_KR()) {
		locale = QLocale::Korean;
		QString format = locale.dateFormat(QLocale::LongFormat);
		QString formatText = locale.toString(start, format);
		QStringList list = formatText.split(" ");
		if (list.size() > 2 && list.at(0).length() >= 5 && list.at(1).length() >= 2) {
			QString first = list.at(0).mid(0, list.at(0).length() - 1);
			QString second = list.at(0).right(1);
			QString third = list.at(1).mid(0, list.at(1).length() - 1);
			QString four = list.at(1).right(1);
			dateText = QString("%1<span style='font-weight:normal;'>%2</span> %3<span style='font-weight:normal;'>%4</span>").arg(first).arg(second).arg(third).arg(four);
		}
	}

	m_dataLabel->setText(dateText);
	if (start < m_currentDate) {
		start = m_currentDate;
	}
	setMinimumDate(start);
	QDate end = QDate(yearShown(), monthShown(), start.daysInMonth());
	setMaximumDate(end);
	if (m_showSelectedDate >= start && m_showSelectedDate <= end) {
		setSelectedDate(m_showSelectedDate);
	}

	if (m_currentDate.month() == monthShown() && m_currentDate.year() == yearShown()) {
		m_leftMonthBtn->setEnabled(false);
	} else {
		m_leftMonthBtn->setEnabled(true);
	}
}

bool PLSShoppingCalenderWidget::isValidDate(QDate date)
{
	QDate start = QDate(yearShown(), monthShown(), 1);
	QDate end = QDate(yearShown(), monthShown(), start.daysInMonth());
	if (date >= start && date <= end) {
		return true;
	}
	return false;
}

int PLSShoppingCalenderWidget::getValueForDpi(int value)
{
	double dpi = PLSDpiHelper::getDpi(this);
	int result = PLSDpiHelper::calculate(dpi, value);
	return result;
}

void PLSShoppingCalenderWidget::paintCell(QPainter *painter, const QRect &rect, const QDate &date) const
{
	if (date == m_showSelectedDate) {
		QDate start = QDate(yearShown(), monthShown(), 1);
		QDate end = QDate(yearShown(), monthShown(), start.daysInMonth());
		if (date < start || date > end) {
			return;
		}
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(Qt::NoPen);
		painter->setBrush(QColor("#effc35"));
		QFont font;
		font.setPixelSize(PLSDpiHelper::calculate(g_dpi, CALENDER_TEXT_FONT_SIZE));
		font.setBold(true);
		painter->setFont(font);
		int width = g_dpi * SelectDateCellWidth;
		painter->drawEllipse(QRectF(ceil(rect.x() + (rect.width() - width) * 0.5f), ceil(rect.y() + (rect.height() - width) * 0.5f), width, width));
		painter->setPen(QColor("#2d2d2d"));
		painter->drawText(rect, Qt::AlignCenter, QString::number(date.day()));
		painter->restore();
	} else if (date == m_hoverDate) {
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(Qt::NoPen);
		painter->setBrush(QColor("#444444"));
		int width = g_dpi * SelectDateCellWidth;
		painter->drawEllipse(QRectF(ceil(rect.x() + (rect.width() - width) * 0.5f), ceil(rect.y() + (rect.height() - width) * 0.5f), width, width));
		if (date.dayOfWeek() == Qt::Sunday) {
			painter->setPen(QColor("#c34151"));
		} else {
			painter->setPen(QColor("#ffffff"));
		}
		painter->drawText(rect, Qt::AlignCenter, QString::number(date.day()));
		painter->restore();
	} else if (date < minimumDate() || date > maximumDate()) {
		QDate start = QDate(yearShown(), monthShown(), 1);
		QDate end = QDate(yearShown(), monthShown(), start.daysInMonth());
		if (date < start || date > end) {
			return;
		}
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(Qt::NoPen);
		painter->setBrush(Qt::NoBrush);
		if (date.dayOfWeek() == Qt::Sunday) {
			painter->setPen(QColor("#78373f"));
		} else {
			painter->setPen(QColor("#666666"));
		}
		painter->drawText(rect, Qt::AlignCenter, QString::number(date.day()));
		painter->restore();
	} else {
		QDate start = QDate(yearShown(), monthShown(), 1);
		QDate end = QDate(yearShown(), monthShown(), start.daysInMonth());
		if (date < start || date > end) {
			return;
		}
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(Qt::NoPen);
		painter->setBrush(Qt::NoBrush);
		if (date.dayOfWeek() == Qt::Sunday) {
			painter->setPen(QColor("#c34151"));
		} else {
			painter->setPen(QColor("#ffffff"));
		}
		painter->drawText(rect, Qt::AlignCenter, QString::number(date.day()));
		painter->restore();
	}
}

void PLSShoppingCalenderWidget::onbtnClicked()
{
	QPushButton *senderBtn = qobject_cast<QPushButton *>(sender());
	if (senderBtn == m_leftMonthBtn) {
		showPreviousMonth();
	} else if (senderBtn == m_rightMonthBtn) {
		showNextMonth();
	}
}
