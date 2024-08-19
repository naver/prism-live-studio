#include "PLSScheduleCombox.h"
#include <frontend-api.h>
#include <QHBoxLayout>
#include <QStyle>
#include "ChannelCommonFunctions.h"
#include "PLSPlatformApi.h"
#include "utils-api.h"
#include "libui.h"
using namespace std;

PLSScheduleCombox::PLSScheduleCombox(QWidget *parent) : QPushButton(parent)
{
	pls_add_css(this, {"PLSScheduleCombox"});
	auto buttonLayout = pls_new<QHBoxLayout>(this);
	buttonLayout->setContentsMargins(0, 0, 0, 0);

	auto label1 = pls_new<QLabel>("", this);
	label1->setObjectName("scheduleLabel1");
	buttonLayout->addWidget(label1);

	this->m_titleLabel = pls_new<QLabel>("", this);
	this->m_titleLabel->setObjectName("scheduletTitleLabel");
	this->m_titleLabel->setTextFormat(Qt::PlainText);
	this->m_titleLabel->setOpenExternalLinks(false);
	this->m_titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
	buttonLayout->setSpacing(0);
	buttonLayout->addWidget(m_titleLabel);

	m_iconLabel = pls_new<QLabel>("");
	m_iconLabel->setHidden(true);
	QSizePolicy sizePolicy = m_iconLabel->sizePolicy();
	sizePolicy.setRetainSizeWhenHidden(false);
	m_iconLabel->setSizePolicy(sizePolicy);
	m_iconLabel->setObjectName("scheduleIconLabel");
	buttonLayout->addWidget(m_iconLabel);

	buttonLayout->addStretch();

	this->m_detailLabel = pls_new<QLabel>("");
	this->m_detailLabel->setObjectName("scheduletDetailLabel");
	buttonLayout->addWidget(m_detailLabel);

	m_dropLabel = pls_new<QLabel>("");
	m_dropLabel->setObjectName("dropLabel");
	buttonLayout->addWidget(m_dropLabel);

	m_scheduleMenu = pls_new<PLSScheduleComboxMenu>(this);

	setupTimer();

	updateStyle(PLSScheduleComboxType::Ty_Normal);

	connect(m_scheduleMenu, &PLSScheduleComboxMenu::scheduleItemClicked, [this](const QString id) {
		emit menuItemClicked(id);
		this->m_scheduleMenu->setHidden(true);
	});
	connect(m_scheduleMenu, &PLSScheduleComboxMenu::scheduleItemExpired, [this](auto ids) { menuItemExpired(ids); });

	connect(m_scheduleMenu, &PLSScheduleComboxMenu::aboutToHide, [this]() { updateStyle(PLSScheduleComboxType::Ty_Normal); });
	connect(m_scheduleMenu, &PLSScheduleComboxMenu::aboutToShow, [this]() { updateStyle(PLSScheduleComboxType::Ty_On); });
}

void PLSScheduleCombox::setupTimer()
{
	m_pLeftTimer = pls_new<QTimer>();
	m_pLeftTimer->setInterval(1000);
	connect(m_pLeftTimer, SIGNAL(timeout()), this, SLOT(updateDetailLabel()));
}

void PLSScheduleCombox::updateDetailLabel()
{
	if (!m_showData.needShowTimeLeftTimer) {
		m_detailLabel->setText(m_showData.time);
		m_pLeftTimer->stop();
		return;
	}
	bool willDelete = false;

	QString time = PLSScheduleComboxMenu::getDetailTime(m_showData, willDelete);
	m_detailLabel->setText(time);
	if (willDelete) {
		m_pLeftTimer->stop();
	}
}

void PLSScheduleCombox::enterEvent(QEnterEvent *event)
{
	if (m_scheduleMenu->isHidden()) {
		updateStyle(PLSScheduleComboxType::Ty_Hover);
	}
	QWidget::enterEvent(event);
}

void PLSScheduleCombox::leaveEvent(QEvent *event)
{
	if (m_scheduleMenu->isHidden()) {
		updateStyle(PLSScheduleComboxType::Ty_Normal);
	}
	QWidget::leaveEvent(event);
}

void PLSScheduleCombox::resizeEvent(QResizeEvent *event)
{

	QPushButton::resizeEvent(event);

	QMetaObject::invokeMethod(
		this, [this]() { updateTitle(m_titleString); }, Qt::QueuedConnection);
}

void PLSScheduleCombox::showScheduleMenu(const vector<PLSScheComboxItemData> &datas)
{
	m_scheduleMenu->setupDatas(datas, this->width());
	if (m_scheduleMenu->isHidden()) {
		updateStyle(PLSScheduleComboxType::Ty_On);
		m_scheduleMenu->show();
		m_scheduleMenu->setHeightAfterShow(this->width());
		QPoint p = this->mapToGlobal(QPoint(0, this->height()));
		m_scheduleMenu->move(p);
	}
}
bool PLSScheduleCombox::isMenuNULL() const
{
	return !pls_object_is_valid(m_scheduleMenu) || m_scheduleMenu == nullptr;
}
bool PLSScheduleCombox::getMenuHide() const
{
	return m_scheduleMenu->isHidden();
}

void PLSScheduleCombox::setMenuHideIfNeed()
{
	if (!getMenuHide()) {
		this->m_scheduleMenu->setHidden(true);
	}
}

void PLSScheduleCombox::setupButton(const QString title, const QString time, bool showIcon)
{
	m_detailLabel->setText(time);
	m_showIcon = showIcon;
	updateTitle(title);
}

void PLSScheduleCombox::setupButton(const QString title, const QString time)
{
	m_detailLabel->setText(time);
	updateTitle(title);
}

void PLSScheduleCombox::setupButton(const PLSScheComboxItemData &data)
{
	m_pLeftTimer->stop();
	m_showData = data;
	if (data.type != PLSScheComboxItemType::Ty_Schedule || !data.needShowTimeLeftTimer) {
		setupButton(data.title, data.time);
		return;
	}

	bool willDelete = false;
	QString time = PLSScheduleComboxMenu::getDetailTime(data, willDelete);
	setupButton(data.title, time);
	if (data.needShowTimeLeftTimer && m_pLeftTimer && !m_pLeftTimer->isActive()) {
		m_pLeftTimer->start();
	}
}

void PLSScheduleCombox::updateTitle(const QString title)
{
	m_titleString = title;
	int spaceWidth = 55;
	if (this->property("platform").toString() == "NaverShopping") {
		int total = 12 + 15 + 33;
		m_iconLabel->setHidden(!m_showIcon);
		pls_flush_style(m_iconLabel);
		if (m_showIcon) {
			total += 38;
		}
		spaceWidth = total;
	}
	int labelWidth = this->width() - spaceWidth;
	if (!m_detailLabel->text().isEmpty()) {
		QFontMetrics fmDetail(m_detailLabel->font());
		labelWidth -= fmDetail.horizontalAdvance(m_detailLabel->text());
	}

	QFontMetrics fontWidth(m_titleLabel->font());
	QString elidedText = fontWidth.elidedText(title, Qt::ElideRight, labelWidth);
	m_titleLabel->setText(elidedText);
}

const QListWidget *PLSScheduleCombox::listWidget() const
{
	return m_scheduleMenu->getListWidget();
}

void PLSScheduleCombox::setButtonEnable(bool enable)
{
	if (this->isEnabled() == enable) {
		return;
	}
	this->setEnabled(enable);
	updateStyle(enable ? PLSScheduleComboxType::Ty_Normal : PLSScheduleComboxType::Ty_Disabled);
}

void PLSScheduleCombox::updateStyle(PLSScheduleComboxType type)
{
	if (!this->isEnabled()) {
		type = PLSScheduleComboxType::Ty_Disabled;
	}
	switch (type) {
	case PLSScheduleCombox::PLSScheduleComboxType::Ty_Normal:
		pls_flush_style_recursive(this, "type", "Ty_Normal", 1);
		break;
	case PLSScheduleCombox::PLSScheduleComboxType::Ty_Hover:
		pls_flush_style_recursive(this, "type", "Ty_Hover", 1);
		break;
	case PLSScheduleCombox::PLSScheduleComboxType::Ty_On:
		pls_flush_style_recursive(this, "type", "Ty_On", 1);
		break;
	case PLSScheduleCombox::PLSScheduleComboxType::Ty_Disabled:
		pls_flush_style_recursive(this, "type", "Ty_Disabled", 1);
		break;
	default:
		break;
	}
}
