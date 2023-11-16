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

#include "ui_PLSScheduleComboxItem.h"
#include <frontend-api.h>
#include "common/PLSAPICommon.h"
#include <pls-common-define.hpp>
#include <libutils-api.h>
#include "libui.h"

const int s_itemHeight_30 = 30;
const int s_itemHeight_40 = 40;
const int s_itemHeight_53 = 53;
const int s_itemHeight_70 = 70;
using namespace std;
using namespace common;
PLSScheduleComboxMenu::PLSScheduleComboxMenu(QWidget *parent) : QMenu(parent)
{
	m_listWidget = pls_new<QListWidget>(this);

	connect(m_listWidget, &QListWidget::itemClicked, this, &PLSScheduleComboxMenu::itemDidSelect);

	m_listWidget->setObjectName("scheduleListWidget");

	auto layout = pls_new<QHBoxLayout>(m_listWidget);
	layout->setContentsMargins(0, 0, 0, 0);
	m_listWidget->setLayout(layout);
	m_listWidget->setViewMode(QListView::ListMode);
	auto action = pls_new<QWidgetAction>(this);
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

	int topBorderHeight = 1;
	int bottomBorderHeight = 1;
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
		int itemHeight = itemData.itemHeight;
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
			int itemHeight = itemData.itemHeight;
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
void PLSScheduleComboxMenu::setHeightAfterShow(int btnWidth)
{
	m_listWidget->setFixedWidth(btnWidth);
	this->setFixedWidth(btnWidth);
}

void PLSScheduleComboxMenu::addAItem(const QString &showStr, const PLSScheComboxItemData &itemData, int itemSize)
{

	auto item = pls_new<QListWidgetItem>();
	item->setData(Qt::UserRole, QVariant::fromValue<PLSScheComboxItemData>(itemData));
	auto widget = pls_new<PLSScheduleComboxItem>(itemData, 1, m_listWidget);
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
	if (!ids.empty()) {
		emit scheduleItemExpired(ids);
	}
}

void PLSScheduleComboxMenu::setupTimer()
{
	m_pLeftTimer = pls_new<QTimer>(this);
	m_pLeftTimer->setInterval(1000);
	connect(m_pLeftTimer, SIGNAL(timeout()), this, SLOT(updateDetailLabel()));
}

void PLSScheduleComboxMenu::itemDidSelect(const QListWidgetItem *item)
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
		auto key = static_cast<QKeyEvent *>(i_Event);

		if ((key->key() != Qt::Key_Enter) && (key->key() != Qt::Key_Return)) {
			return true;
		}

		const QPoint localPoint = m_listWidget->viewport()->mapFromGlobal(QCursor::pos());
		auto itemLocal = m_listWidget->itemAt(localPoint);
		if (itemLocal != nullptr) {
			itemDidSelect(itemLocal);
		} else {
			this->setHidden(true);
		}
		return true;
	} else if (i_Object == this && (i_Event->type() == QEvent::MouseButtonPress || i_Event->type() == QEvent::MouseButtonDblClick)) {
		auto mouseEvent = static_cast<QMouseEvent *>(i_Event);
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
	QMenu::hideEvent(event);
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

const QListWidget *PLSScheduleComboxMenu::getListWidget() const
{
	return m_listWidget;
}

QString PLSScheduleComboxMenu::formateTheLeftTime(long leftTimeStamp)
{

	return QString::asprintf("%02ld:%02ld", leftTimeStamp / 60, leftTimeStamp % 60);
}

static const QString kTitleLabelObjectName = "titleLabel";

PLSScheduleComboxItem::PLSScheduleComboxItem(const PLSScheComboxItemData &data, double dpi, QWidget *parent) : QWidget(parent), _data(data), m_Dpi(dpi)
{
	ui = pls_new<Ui::PLSScheduleComboxItem>();
	ui->setupUi(this);
	ui->bgWidget->layout()->setContentsMargins({12, 0, 6, 0});
	ui->leftWidget->layout()->setContentsMargins({0, 0, 9, 0});

	if (data.type == PLSScheComboxItemType::Ty_Header) {
		pls_flush_style_recursive(ui->bgWidget, "status", "header");
	}
	ui->imgLabel->setDefaultIconPath("");
	QSizePolicy policy = ui->iconLabel->sizePolicy();
	policy.setRetainSizeWhenHidden(false);
	ui->iconLabel->setSizePolicy(policy);

	ui->iconLabel->setHidden(true);
	if (data.showIcon) {
		ui->iconLabel->setHidden(false);
	}

	if (data.type == PLSScheComboxItemType::Ty_Loading || !data.imgUrl.isEmpty()) {
		int imageWidth = data.type == PLSScheComboxItemType::Ty_Loading ? 24 : 34;
		int dpi_widthe = imageWidth;
		ui->imgLabel->setProperty("loading", data.type == PLSScheComboxItemType::Ty_Loading);
		pls_flush_style(ui->imgLabel);

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

void PLSScheduleComboxItem::downloadThumImage(const QLabel *reciver, const QString &url)
{
	auto _callBack = [this, reciver, url](bool ok, const QString &imagePath) {
		if (reciver == nullptr || !pls_object_is_valid(reciver)) {
			return;
		}
		if (!ok) {
			ui->imgLabel->setMainPixmap(":/images/img-vlive-profile.svg", QSize(34, 34), true, m_Dpi);
			ui->imgLabel->update();
			return;
		}

		ui->imgLabel->setMainPixmap(imagePath, QSize(34, 34), true, m_Dpi);
		ui->imgLabel->update();
		PLSImageStatic::instance()->profileUrlMap[url] = imagePath;
	};

	if (PLSImageStatic::instance()->profileUrlMap.contains(url) && !PLSImageStatic::instance()->profileUrlMap[url].isEmpty()) {
		_callBack(true, PLSImageStatic::instance()->profileUrlMap[url]);
		return;
	}
	PLSAPICommon::downloadImageAsync(reciver, url, _callBack);
}

PLSScheduleComboxItem::~PLSScheduleComboxItem()
{
	pls_delete(ui, nullptr);
}

void PLSScheduleComboxItem::setDetailLabelStr(const QString &str)
{
	if (ui->detailLabel != nullptr && !ui->detailLabel->isHidden()) {
		ui->detailLabel->setText(str);
	}
}

void PLSScheduleComboxItem::setupTimer()
{
	m_pTimer = pls_new<QTimer>(this);
	m_pTimer->setInterval(300);
	connect(m_pTimer, SIGNAL(timeout()), this, SLOT(updateProgress()));
}

void PLSScheduleComboxItem::updateProgress()
{
	static int showIndex = 0;
	showIndex = showIndex > 7 ? 1 : ++showIndex;
	ui->imgLabel->setStyleSheet(QString("image: url(\":/resource/images/loading/loading-%1.svg\")").arg(showIndex));
	pls_flush_style(ui->imgLabel);
}

void PLSScheduleComboxItem::resizeEvent(QResizeEvent *event)
{
	ui->titleLabel->setText(GetNameElideString());
	QWidget::resizeEvent(event);
}

QString PLSScheduleComboxItem::GetNameElideString()
{

	QFontMetrics fontWidth(ui->titleLabel->font());
	QString elidedText = fontWidth.elidedText(_data.title, Qt::ElideRight, ui->rightWidget->width() - 10);
	return elidedText;
}
