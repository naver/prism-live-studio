#include "PLSScheduleComboxMenu.h"
#include <qdebug.h>
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QListWidget>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QPainter>
#include <qscrollbar.h>
#include "./common/PLSDateFormate.h"
#include "PLSDpiHelper.h"
#include "ui_PLSScheduleComboxItem.h"
#include <frontend-api.h>
#include "vlive/PLSAPIVLive.h"
#include <pls-common-define.hpp>
#include "../vlive/PLSPlatformVLive.h"

const int s_itemHeight_30 = 30;
const int s_itemHeight_40 = 40;
const int s_itemHeight_53 = 53;
const int s_itemHeight_70 = 70;

PLSScheduleComboxMenu::PLSScheduleComboxMenu(QWidget *parent) : QMenu(parent)
{
	m_listWidget = new QListWidget(this);

	connect(m_listWidget, &QListWidget::itemClicked, this, &PLSScheduleComboxMenu::itemDidSelect);

	m_listWidget->setObjectName("scheduleListWidget");

	QHBoxLayout *layout = new QHBoxLayout(m_listWidget);
	layout->setContentsMargins(0, 0, 0, 0);
	m_listWidget->setLayout(layout);
	m_listWidget->setViewMode(QListView::ListMode);
	QWidgetAction *action = new QWidgetAction(this);
	action->setDefaultWidget(m_listWidget);
	this->addAction(action);
	this->installEventFilter(this);
	this->setWindowFlag(Qt::NoDropShadowWindowHint);
	setupTimer();
}

void PLSScheduleComboxMenu::setupDatas(const vector<PLSScheComboxItemData> &datas, int btnWidth)
{
	m_pLeftTimer->stop();
	m_listWidget->clear();
	m_vecItems.clear();

	int realShowCount = 0;
	bool isNeedStartTimer = false;

	const static int maxShowCount = 5;
	double dpi = PLSDpiHelper::getDpi(this);

	int topBorderHeight = PLSDpiHelper::calculate(dpi, 1);
	int bottomBorderHeight = PLSDpiHelper::calculate(dpi, 1);
	int widgetHeigth = topBorderHeight + bottomBorderHeight;
	for (auto &itemData : datas) {
		bool isItemIgnore = false;
		QString showStr = PLSScheduleComboxMenu::getDetailTime(itemData, isItemIgnore);
		if (isItemIgnore || itemData.type == PLSScheComboxItemType::Ty_Placehoder) {
			continue;
		}
		if (itemData.needShowTimeLeftTimer) {
			isNeedStartTimer = true;
		}
		int itemHeight = PLSDpiHelper::calculate(dpi, itemData.itemHeight);
		if (realShowCount < maxShowCount) {
			widgetHeigth += itemHeight;
		}
		realShowCount++;
		addAItem(showStr, itemData, itemHeight);
	}

	if (realShowCount == 0) {
		for (auto &itemData : datas) {
			if (itemData.type != PLSScheComboxItemType::Ty_Placehoder) {
				continue;
			}
			int itemHeight = PLSDpiHelper::calculate(dpi, itemData.itemHeight);
			widgetHeigth += itemHeight;
			addAItem(itemData.time, itemData, itemHeight);
			realShowCount++;
			break;
		}
	}

	m_listWidget->setFixedSize(btnWidth, widgetHeigth);
	this->setFixedSize(btnWidth, widgetHeigth);
	if (realShowCount > maxShowCount) {
		m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		m_listWidget->verticalScrollBar()->setEnabled(true);
	} else {
		m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		m_listWidget->verticalScrollBar()->setEnabled(false);
	}
	m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	if (isNeedStartTimer && m_pLeftTimer && !m_pLeftTimer->isActive()) {
		m_pLeftTimer->start();
	}
}

void PLSScheduleComboxMenu::addAItem(const QString &showStr, const PLSScheComboxItemData &itemData, int itemSize)
{
	QListWidgetItem *item = new QListWidgetItem();
	item->setData(Qt::UserRole, QVariant::fromValue<PLSScheComboxItemData>(itemData));
	PLSScheduleComboxItem *widget = new PLSScheduleComboxItem(itemData, PLSDpiHelper::getDpi(this), m_listWidget);
	widget->setDetailLabelStr(showStr);
	item->setSizeHint(QSize(0, itemSize));
	m_listWidget->addItem(item);
	m_listWidget->setItemWidget(item, widget);
	m_vecItems.push_back(widget);
}

void PLSScheduleComboxMenu::updateDetailLabel()
{
	vector<QString> ids;
	for (PLSScheduleComboxItem *menuItem : m_vecItems) {
		PLSScheComboxItemData data = menuItem->getData();
		if (!data.needShowTimeLeftTimer) {
			continue;
		}
		bool willDelete = false;
		QString showStr = PLSScheduleComboxMenu::getDetailTime(data, willDelete);
		menuItem->setDetailLabelStr(showStr);
		if (willDelete && !data._id.isEmpty()) {
			ids.push_back(data._id);
		}
	}
	if (ids.size() > 0) {
		emit scheduleItemExpired(ids);
	}
}

void PLSScheduleComboxMenu::setupTimer()
{
	m_pLeftTimer = new QTimer();
	m_pLeftTimer->setInterval(1000);
	connect(m_pLeftTimer, SIGNAL(timeout()), this, SLOT(updateDetailLabel()));
}

void PLSScheduleComboxMenu::itemDidSelect(QListWidgetItem *item)
{
	PLSScheComboxItemData data = item->data(Qt::UserRole).value<PLSScheComboxItemData>();
	if (data.type == PLSScheComboxItemType::Ty_Header) {
		return;
	}
	emit scheduleItemClicked(data._id);
}

bool PLSScheduleComboxMenu::eventFilter(QObject *i_Object, QEvent *i_Event)
{
	if (i_Object == this && i_Event->type() == QEvent::KeyPress) {
		QKeyEvent *key = static_cast<QKeyEvent *>(i_Event);

		if ((key->key() != Qt::Key_Enter) && (key->key() != Qt::Key_Return)) {
			return true;
		}

		const QPoint localPoint = m_listWidget->viewport()->mapFromGlobal(QCursor::pos());
		QListWidgetItem *itemLocal = m_listWidget->itemAt(localPoint);
		if (itemLocal != nullptr) {
			itemDidSelect(itemLocal);
		} else {
			this->setHidden(true);
		}
		return true;
	} else if (i_Object == this && (i_Event->type() == QEvent::MouseButtonPress || i_Event->type() == QEvent::MouseButtonDblClick)) {
		QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(i_Event);
		QPoint superPoint = parentWidget()->mapToGlobal(QPoint(0, 0));
		QRect superRect(superPoint.x(), superPoint.y(), parentWidget()->width(), parentWidget()->height());
		if (superRect.contains(mouseEvent->globalPos())) {
			QTimer::singleShot(1, [this] { this->setHidden(true); });
			return true;
		} else {
			return QWidget::eventFilter(i_Object, i_Event);
		}
	} else {
		return QWidget::eventFilter(i_Object, i_Event);
	}
}

void PLSScheduleComboxMenu::hideEvent(QHideEvent *event)
{
	m_vecItems.clear();
	m_listWidget->clear();
	__super::hideEvent(event);
}

QString PLSScheduleComboxMenu::getDetailTime(const PLSScheComboxItemData &data, bool &willDelete)
{
	willDelete = false;
	if (data.needShowTimeLeftTimer == false) {
		return data.time;
	}

	QString timeString;
	long startTime = data.isNewLive ? data.timeStamp : data.endTimeStamp;
	long nowTime = PLSDateFormate::getNowTimeStamp();

	if (data.isNewLive) {
		//not have broadcasted
		if (startTime - nowTime > 0 && startTime - nowTime < 10 * 60) {
			//Within 10 minutes, start timer.
			long leftTimeStamp = startTime - nowTime;
			QString leftTime = formateTheLeftTime(leftTimeStamp);
			timeString = leftTime.append(" left");
		} else {
			//the normal time
			timeString = data.time;
		}
	} else {
		//already broadcasted, the startTime is the last broadcast stopped time
		if (nowTime - startTime < 10 * 60) {
			//when broadcast stopped, start 10min stopped.
			long leftTimeStamp = 10 * 60 - (nowTime - startTime);
			QString leftTime = formateTheLeftTime(leftTimeStamp);
			timeString = tr("LiveInfo.vlive.schedule.Recently.streamed").arg(leftTime);
		} else {
			//when 10 min later, delete it.
			willDelete = true;
			QString leftTime = formateTheLeftTime(0);
			timeString = tr("LiveInfo.vlive.schedule.Recently.streamed").arg(leftTime);
		}
	}
	return timeString;
}

QString PLSScheduleComboxMenu::formateTheLeftTime(long leftTimeStamp)
{
	char szText[56] = {0};
	_snprintf(szText, sizeof(szText), "%02ld:%02ld", leftTimeStamp / 60, leftTimeStamp % 60);
	return QString(szText);
}

static const QString kTitleLabelObjectName = "titleLabel";

PLSScheduleComboxMenu::~PLSScheduleComboxMenu() {}

PLSScheduleComboxItem::PLSScheduleComboxItem(const PLSScheComboxItemData data, double dpi, QWidget *parent) : QWidget(parent), ui(new Ui::PLSScheduleComboxItem), _data(data), m_Dpi(dpi)
{
	ui->setupUi(this);
	ui->bgWidget->layout()->setContentsMargins({PLSDpiHelper::calculate(dpi, 12), 0, PLSDpiHelper::calculate(dpi, 6), 0});
	ui->leftWidget->layout()->setContentsMargins({0, 0, PLSDpiHelper::calculate(dpi, 9), 0});

	if (data.type == PLSScheComboxItemType::Ty_Header) {
		pls_flush_style_recursive(ui->bgWidget, "status", "header");
	}

	if (data.type == PLSScheComboxItemType::Ty_Loading || !data.imgUrl.isEmpty()) {
		int imageWidth = data.type == PLSScheComboxItemType::Ty_Loading ? 24 : 34;
		int dpi_widthe = PLSDpiHelper::calculate(dpi, imageWidth);
		ui->imgLabel->setFixedSize(QSize(dpi_widthe, dpi_widthe));

		if (data.type == PLSScheComboxItemType::Ty_Loading) {
			updateProgress();
			setupTimer();
			m_pTimer->start();
		} else {
			ui->imgLabel->setDefaultIconPath(":/images/img-vlive-profile.svg");
			ui->imgLabel->setPadding(0);
			ui->imgLabel->setUseContentsRect(true);
			downloadThumImage(ui->imgLabel, data.imgUrl);
		}
	} else {
		ui->leftWidget->setHidden(true);
	}

	ui->rightImageLabel->setHidden(data.isShowRightIcon == false);

	ui->titleLabel->setAlignment(Qt::AlignBottom | Qt::AlignLeading | Qt::AlignLeft);
	ui->titleLabel->setText(GetNameElideString());

	ui->detailLabel->setText(_data.time);
	ui->detailLabel->setAlignment(Qt::AlignTop | Qt::AlignLeading | Qt::AlignLeft);

	ui->detailLabel->setHidden(data.type == PLSScheComboxItemType::Ty_NormalLive || ui->detailLabel->text().isEmpty());
	if (ui->detailLabel->isHidden()) {
		ui->titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeading | Qt::AlignLeft);
	}

	pls_flush_style(ui->titleLabel, STATUS_SELECTED, data.isSelect ? STATUS_TRUE : STATUS_FALSE);
	pls_flush_style(ui->detailLabel, STATUS_SELECTED, data.isSelect ? STATUS_TRUE : STATUS_FALSE);
	if (data.platformName == "navershopping") {
		pls_flush_style(ui->detailLabel, "expired", data.isExpired ? STATUS_TRUE : STATUS_FALSE);
	}
}

void PLSScheduleComboxItem::downloadThumImage(QLabel *reciver, const QString &url)
{
	auto _callBack = [=](bool ok, const QString &imagePath, void *const) {
		auto label = static_cast<QLabel *>(reciver);
		if (label == nullptr) {
			return;
		}
		if (!ok) {
			ui->imgLabel->setPixmap(":/images/img-vlive-profile.svg", QSize(34, 34), true, m_Dpi);
			ui->imgLabel->update();
			return;
		}

		ui->imgLabel->setPixmap(imagePath, QSize(34, 34), true, m_Dpi);
		ui->imgLabel->update();
		PLSImageStatic::instance()->profileUrlMap[url] = imagePath;
	};

	if (PLSImageStatic::instance()->profileUrlMap.contains(url) && !PLSImageStatic::instance()->profileUrlMap[url].isEmpty()) {
		_callBack(true, PLSImageStatic::instance()->profileUrlMap[url], nullptr);
		return;
	}
	PLSAPIVLive::downloadImageAsync(reciver, url, _callBack, reciver);
}

PLSScheduleComboxItem::~PLSScheduleComboxItem()
{
	delete ui;
}

void PLSScheduleComboxItem::setDetailLabelStr(const QString &str)
{
	if (ui->detailLabel != nullptr && !ui->detailLabel->isHidden()) {
		ui->detailLabel->setText(str);
	}
}

void PLSScheduleComboxItem::enterEvent(QEvent *event)
{
	QWidget::enterEvent(event);
}

void PLSScheduleComboxItem::leaveEvent(QEvent *event)
{
	QWidget::leaveEvent(event);
}

void PLSScheduleComboxItem::setupTimer()
{
	m_pTimer = new QTimer(this);
	m_pTimer->setInterval(300);
	connect(m_pTimer, SIGNAL(timeout()), this, SLOT(updateProgress()));
}

void PLSScheduleComboxItem::updateProgress()
{
	static int showIndex = 0;
	showIndex = showIndex > 7 ? 1 : ++showIndex;
	ui->imgLabel->setStyleSheet(QString("image: url(\":/images/loading-%1.svg\")").arg(showIndex));
	pls_flush_style(ui->imgLabel);
}

void PLSScheduleComboxItem::resizeEvent(QResizeEvent *event)
{
	ui->titleLabel->setText(GetNameElideString());
	__super::resizeEvent(event);
}

QString PLSScheduleComboxItem::GetNameElideString()
{
	double dpi = PLSDpiHelper::getDpi(this);
	QFontMetrics fontWidth(ui->titleLabel->font());
	QString elidedText = fontWidth.elidedText(_data.title, Qt::ElideRight, ui->rightWidget->width() - PLSDpiHelper::calculate(dpi, 10));
	return elidedText;
}
