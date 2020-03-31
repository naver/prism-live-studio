#include "PLSScheduleButton.h"
#include <QHBoxLayout>
#include "../PLSPlatformApi.h"
#include <frontend-api.h>
#include <QStyle>

PLSScheduleButton::PLSScheduleButton(QWidget *parent) : QPushButton(parent)
{
	QHBoxLayout *buttonLayout = new QHBoxLayout(this);
	buttonLayout->setMargin(0);
	this->m_scheduletTitleLabel = new QLabel("", this);
	this->m_scheduletTitleLabel->setObjectName("scheduletTitleLabel");
	buttonLayout->addWidget(m_scheduletTitleLabel);
	buttonLayout->addStretch();

	this->m_schedultTimeLabel = new QLabel("");
	this->m_schedultTimeLabel->setObjectName("schedultTimeLabel");
	buttonLayout->addWidget(m_schedultTimeLabel);

	m_dropLabel = new QLabel("");
	m_dropLabel->setObjectName("dropLabel");
	buttonLayout->addWidget(m_dropLabel);

	m_scheduleMenu = new PLSScheduleMenu(this);

	updateStyle(PLSScheduleButtonType::Ty_Normal);

	connect(m_scheduleMenu, &PLSScheduleMenu::scheduleItemClicked, [=](const QString id) {
		emit menuItemClicked(id);
		this->m_scheduleMenu->setHidden(true);
	});

	connect(m_scheduleMenu, &PLSScheduleMenu::aboutToHide, [=]() { updateStyle(PLSScheduleButtonType::Ty_Normal); });
	connect(m_scheduleMenu, &PLSScheduleMenu::aboutToShow, [=]() { updateStyle(PLSScheduleButtonType::Ty_On); });
}

void PLSScheduleButton::enterEvent(QEvent *event)
{
	if (m_scheduleMenu->isHidden()) {
		updateStyle(PLSScheduleButtonType::Ty_Hover);
	}
	QWidget::enterEvent(event);
}

void PLSScheduleButton::leaveEvent(QEvent *event)
{
	if (m_scheduleMenu->isHidden()) {
		updateStyle(PLSScheduleButtonType::Ty_Normal);
	}
	QWidget::leaveEvent(event);
}

PLSScheduleButton::~PLSScheduleButton() {}

void PLSScheduleButton::showScheduleMenu(const vector<ComplexItemData> &datas)
{
	m_scheduleMenu->setupDatas(datas, this->width());
	if (m_scheduleMenu->isHidden()) {
		QPoint p = this->mapToGlobal(QPoint(0, this->height()));
		m_scheduleMenu->exec(p);
	}
}
bool PLSScheduleButton::isMenuNULL()
{
	return m_scheduleMenu == nullptr;
}
bool PLSScheduleButton::getMenuHide()
{
	return m_scheduleMenu->isHidden();
}

void PLSScheduleButton::setMenuHideIfNeed()
{
	if (!getMenuHide()) {
		this->m_scheduleMenu->setHidden(true);
	}
}

void PLSScheduleButton::setupButton(QString title, QString time)
{
	m_schedultTimeLabel->setText(time);
	updateTitle(title);
}

void PLSScheduleButton::updateTitle(QString title)
{
	int p = this->width();
	int buttonWidth = this->width() < 300 ? 550 : this->width(); //when the ui init, the width maybe very small = 100
	int labelWidth = (m_schedultTimeLabel->text().isEmpty() ? (buttonWidth - 60) : (buttonWidth - 160));
	QFontMetrics fontWidth(m_scheduletTitleLabel->font());
	QString elidedText = fontWidth.elidedText(title, Qt::ElideRight, labelWidth);
	m_scheduletTitleLabel->setText(elidedText);
}

void PLSScheduleButton::setButtonEnable(bool enable)
{
	if (this->isEnabled() == enable) {
		return;
	}
	updateStyle(enable ? PLSScheduleButtonType::Ty_Normal : PLSScheduleButtonType::Ty_Disable);
	this->setEnabled(enable);
}

void PLSScheduleButton::updateStyle(PLSScheduleButtonType type)
{
	if (!this->isEnabled()) {
		type = PLSScheduleButtonType::Ty_Disable;
	}
	switch (type) {
	case PLSScheduleButton::PLSScheduleButtonType::Ty_Normal:
		this->setStyleSheet(
			"PLSScheduleButton{background-color: #444444;} PLSScheduleButton #dropLabel {image: url(./data/prism-studio/themes/Dark/txt-dropbox-open-normal.png);} PLSScheduleButton > .QLabel{ color: #ffffff;}");
		break;
	case PLSScheduleButton::PLSScheduleButtonType::Ty_Hover:
		this->setStyleSheet(
			"PLSScheduleButton{background-color: #555555;} PLSScheduleButton #dropLabel {image: url(./data/prism-studio/themes/Dark/txt-dropbox-open-over.png);} PLSScheduleButton > .QLabel{ color: #ffffff;}");
		break;
	case PLSScheduleButton::PLSScheduleButtonType::Ty_On:
		this->setStyleSheet(
			"PLSScheduleButton{background-color: #222222;} PLSScheduleButton #dropLabel {image: url(./data/prism-studio/themes/Dark/txt-dropbox-open-click.png);} PLSScheduleButton > .QLabel{ color: #ffffff;}");
		break;
	case PLSScheduleButton::PLSScheduleButtonType::Ty_Disable:
		this->setStyleSheet(
			"PLSScheduleButton{background-color: #373737;} PLSScheduleButton #dropLabel {image: url(./data/prism-studio/themes/Dark/txt-dropbox-open-disable.png);} PLSScheduleButton > .QLabel{ color: #666666;}");
		break;
	default:
		break;
	}
}
