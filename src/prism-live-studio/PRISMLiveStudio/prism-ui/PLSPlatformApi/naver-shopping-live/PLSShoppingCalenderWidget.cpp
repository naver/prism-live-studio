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
#include <QMouseEvent>
#include <qglobal.h>
#include "frontend-api.h"
#include "libui.h"

const int CalenderBottomPadding = 11;
const int CalenderLeftPadding = 9;
const int CalenderHeaderHeight = 43;
const int CalenderCellHeight = 26;
const int CalenderCellWeight = 31;
const int SelectDateCellWidth = 23;
const int CALENDER_TEXT_FONT_SIZE = 11;

const QString g_ontFamilyEn = "Segoe UI";
const QString g_tipFontFamilyKr = "Malgun Gothic";

PLSShoppingCalenderWidget::PLSShoppingCalenderWidget(QWidget *parent) : QCalendarWidget(parent)
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

	int cellWidth = getValueForDpi(CalenderCellWeight);
	table_view->horizontalHeader()->setDefaultSectionSize(cellWidth);
	table_view->horizontalHeader()->setMinimumSectionSize(cellWidth);
	table_view->horizontalHeader()->setMaximumSectionSize(cellWidth);
	int cellHeight = getValueForDpi(CalenderCellHeight);
	table_view->verticalHeader()->setDefaultSectionSize(cellHeight);
	table_view->verticalHeader()->setMinimumSectionSize(cellHeight);
	table_view->verticalHeader()->setMaximumSectionSize(cellHeight);

	initControl();
	pls_add_css(this, {"PLSShoppingCalenderWidget"});
	QDate currentDate = QDate::currentDate();
	m_currentDate = currentDate.addDays(-14);

	updateDateCell();
}

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

void PLSShoppingCalenderWidget::showShoppingSelectedDate(const QDate &date)
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
	connect(this, &QCalendarWidget::currentPageChanged, this, [this](int /*year*/, int /*month*/) { updateDateCell(); });
	updateDateCell();
}

void PLSShoppingCalenderWidget::initTopWidget()
{
	m_topWidget = pls_new<QWidget>(this);
	m_topWidget->setObjectName("CalendarTopWidget");
	m_topWidget->setFixedHeight(CalenderHeaderHeight);
	m_topWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	auto hboxLayout = pls_new<QHBoxLayout>();
	hboxLayout->setContentsMargins(0, 0, 0, 0);
	hboxLayout->setSpacing(0);

	m_leftMonthBtn = pls_new<QPushButton>(this);
	m_leftMonthBtn->setObjectName("CalendarLeftMonthBtn");

	m_rightMonthBtn = pls_new<QPushButton>(this);
	m_rightMonthBtn->setObjectName("CalendarRightMonthBtn");

	m_dataLabel = pls_new<QLabel>(this);
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

QDate PLSShoppingCalenderWidget::dateForCell(int row, int column) const
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
	if (int columnCount = 7; row < firstRow || row > (firstRow + rowCount - 1) || column < firstColumn || column > (firstColumn + columnCount - 1)) {
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

QDate PLSShoppingCalenderWidget::referenceDate() const
{
	int refDay = 1;
	while (refDay <= 31) {
		if (auto refDate = QDate(yearShown(), monthShown(), refDay); refDate.isValid()) {
			return refDate;
		}
		refDay += 1;
	}
	return QDate();
}

int PLSShoppingCalenderWidget::columnForDayOfWeek(int day) const
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

int PLSShoppingCalenderWidget::columnForFirstOfMonth(QDate date) const
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

	auto start = QDate(yearShown(), monthShown(), 1);
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
	auto end = QDate(yearShown(), monthShown(), start.daysInMonth());
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

bool PLSShoppingCalenderWidget::isValidDate(QDate date) const
{
	auto start = QDate(yearShown(), monthShown(), 1);
	if (auto end = QDate(yearShown(), monthShown(), start.daysInMonth()); date >= start && date <= end) {
		return true;
	}
	return false;
}

int PLSShoppingCalenderWidget::getValueForDpi(int value)
{
	return value;
}

void PLSShoppingCalenderWidget::showEvent(QShowEvent *event)
{
	updateDateCell();
	QCalendarWidget::showEvent(event);
}

void PLSShoppingCalenderWidget::paintCell(QPainter *painter, const QRect &rect, QDate date) const
{
	if (date == m_showSelectedDate) {
		auto start = QDate(yearShown(), monthShown(), 1);
		if (auto end = QDate(yearShown(), monthShown(), start.daysInMonth()); date < start || date > end) {
			return;
		}
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(Qt::NoPen);
		painter->setBrush(QColor("#effc35"));
		QFont font = painter->font();
		font.setBold(true);
		painter->setFont(font);
		int width = SelectDateCellWidth;
		QRectF rectF = QRectF(ceil((float)rect.x() + ((float)rect.width() - (float)width) * 0.5f), ceil((float)rect.y() + ((float)rect.height() - (float)width) * 0.5f), width, width);
		painter->drawEllipse(rectF);
		painter->setPen(QColor("#2d2d2d"));
		painter->drawText(rect, Qt::AlignCenter, QString::number(date.day()));
		painter->restore();
	} else if (date == m_hoverDate) {
		painter->save();
		painter->setRenderHint(QPainter::Antialiasing);
		painter->setPen(Qt::NoPen);
		painter->setBrush(QColor("#444444"));
		int width = SelectDateCellWidth;
		painter->drawEllipse(QRectF(ceil((float)rect.x() + ((float)rect.width() - (float)width) * 0.5f), ceil((float)rect.y() + ((float)rect.height() - (float)width) * 0.5f), width, width));
		if (date.dayOfWeek() == Qt::Sunday) {
			painter->setPen(QColor("#c34151"));
		} else {
			painter->setPen(QColor("#ffffff"));
		}
		painter->drawText(rect, Qt::AlignCenter, QString::number(date.day()));
		painter->restore();
	} else if (date < minimumDate() || date > maximumDate()) {
		auto start = QDate(yearShown(), monthShown(), 1);
		if (auto end = QDate(yearShown(), monthShown(), start.daysInMonth()); date < start || date > end) {
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
		auto start = QDate(yearShown(), monthShown(), 1);
		if (auto end = QDate(yearShown(), monthShown(), start.daysInMonth()); date < start || date > end) {
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
	if (auto senderBtn = qobject_cast<QPushButton *>(sender()); senderBtn == m_leftMonthBtn) {
		showPreviousMonth();
	} else if (senderBtn == m_rightMonthBtn) {
		showNextMonth();
	}
}
