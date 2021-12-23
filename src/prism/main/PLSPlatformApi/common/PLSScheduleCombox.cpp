#include "PLSScheduleCombox.h"
#include <frontend-api.h>
#include <QHBoxLayout>
#include <QStyle>
#include "ChannelCommonFunctions.h"
#include "PLSPlatformApi.h"

PLSScheduleCombox::PLSScheduleCombox(QWidget *parent, PLSDpiHelper dpiHelper) : QPushButton(parent)
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSScheduleCombox});
	QHBoxLayout *buttonLayout = new QHBoxLayout(this);
	buttonLayout->setMargin(0);
	this->m_titleLabel = new QLabel("", this);
	this->m_titleLabel->setObjectName("scheduletTitleLabel");
	this->m_titleLabel->setTextFormat(Qt::PlainText);
	this->m_titleLabel->setOpenExternalLinks(false);
	this->m_titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
	buttonLayout->addWidget(m_titleLabel);
	buttonLayout->addStretch();

	this->m_detailLabel = new QLabel("");
	buttonLayout->addWidget(m_detailLabel);

	m_dropLabel = new QLabel("");
	m_dropLabel->setObjectName("dropLabel");
	buttonLayout->addWidget(m_dropLabel);

	m_scheduleMenu = new PLSScheduleComboxMenu(this);

	setupTimer();

	updateStyle(PLSScheduleComboxType::Ty_Normal);

	connect(m_scheduleMenu, &PLSScheduleComboxMenu::scheduleItemClicked, [=](const QString id) {
		emit menuItemClicked(id);
		this->m_scheduleMenu->setHidden(true);
	});
	connect(m_scheduleMenu, &PLSScheduleComboxMenu::scheduleItemExpired, [=](auto ids) { menuItemExpired(ids); });

	connect(m_scheduleMenu, &PLSScheduleComboxMenu::aboutToHide, [=]() { updateStyle(PLSScheduleComboxType::Ty_Normal); });
	connect(m_scheduleMenu, &PLSScheduleComboxMenu::aboutToShow, [=]() { updateStyle(PLSScheduleComboxType::Ty_On); });
}

void PLSScheduleCombox::setupTimer()
{
	m_pLeftTimer = new QTimer();
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

void PLSScheduleCombox::enterEvent(QEvent *event)
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

	__super::resizeEvent(event);

	QMetaObject::invokeMethod(
		this, [=]() { updateTitle(m_titleString); }, Qt::QueuedConnection);
}

PLSScheduleCombox::~PLSScheduleCombox() {}

void PLSScheduleCombox::showScheduleMenu(const vector<PLSScheComboxItemData> &datas)
{
	m_scheduleMenu->setupDatas(datas, this->width());
	if (m_scheduleMenu->isHidden()) {
		QPoint p = this->mapToGlobal(QPoint(0, this->height()));
		updateStyle(PLSScheduleComboxType::Ty_On);
		m_scheduleMenu->move(p);
		m_scheduleMenu->show();
	}
}
bool PLSScheduleCombox::isMenuNULL()
{
	return m_scheduleMenu == nullptr;
}
bool PLSScheduleCombox::getMenuHide()
{
	return m_scheduleMenu->isHidden();
}

void PLSScheduleCombox::setMenuHideIfNeed()
{
	if (!getMenuHide()) {
		this->m_scheduleMenu->setHidden(true);
	}
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
	int spaceWidth = PLSDpiHelper::calculate(PLSDpiHelper::getDpi(this), 55);
	int labelWidth = this->width() - spaceWidth;
	if (!m_detailLabel->text().isEmpty()) {
		QFontMetrics fmDetail(m_detailLabel->font());
		labelWidth -= fmDetail.width(m_detailLabel->text());
	}

	QFontMetrics fontWidth(m_titleLabel->font());
	QString elidedText = fontWidth.elidedText(title, Qt::ElideRight, labelWidth);
	m_titleLabel->setText(elidedText);
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
