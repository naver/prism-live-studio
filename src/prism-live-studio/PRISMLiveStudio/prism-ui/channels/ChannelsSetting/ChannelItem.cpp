#include "ChannelItem.h"
#include "ChannelCommonFunctions.h"
#include "LogPredefine.h"
#include "pls-channel-const.h"
#include "ui_ChannelItem.h"

using namespace ChannelData;

ChannelItem::ChannelItem(QWidget *parent) : QPushButton(parent), ui(new Ui::ChannelItem)
{
	PLS_DISABLE_UISTEP_V2(this);
	ui->setupUi(this);
	this->setCheckable(true);
	connect(this, &QPushButton::toggled, this, &ChannelItem::onSelectStateChanged);
	pls_uistep_v2_enable(ui->checkBox, false);
}

ChannelItem::~ChannelItem()
{
	delete ui;
}

void ChannelItem::setData(const QVariantMap &data)
{
	mLastData = data;
	auto uuid = getInfo(mLastData, g_channelUUID);
	mLastUUid = uuid;

	QString userIcon;
	QString platformIcon;
	int channelState = getInfo(mLastData, g_channelStatus, Error);
	auto platformName = getInfo(mLastData, g_channelName);
	if (channelState == Valid) {
		getComplexImageOfChannel(uuid, ImageType::tagIcon, userIcon, platformIcon);
	} else {
		userIcon = getPlatformImageFromName(platformName, ImageType::tagIcon);
	}
	ui->IconLabel->setMainPixmap(userIcon, QSize(34, 34));
	ui->IconLabel->setPlatformPixmap(platformIcon, QSize(18, 18));

	updateTextLabel();

	QSignalBlocker bloker(this);
	bool isSelected = getInfo(mLastData, ChannelData::g_displayState, true);
	ui->checkBox->setChecked(isSelected);
	this->setChecked(isSelected);
	pls_uistep_v2_enable(this, PLS_UI_STEPS_V2_SIGNAL_CLICKED, false);
	pls_uistep_v2_custom(this, PLS_UI_STEPS_V2_SIGNAL_TOGGLED, QStringLiteral("Choose"), platformName, [this]() -> QString { return this->isChecked() ? "On" : "Off"; });
}

void ChannelItem::changeEvent(QEvent *e)
{
	QPushButton::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}

void ChannelItem::resizeEvent(QResizeEvent *event)
{
	QPushButton::resizeEvent(event);
	updateTextLabel();
}

void ChannelItem::onSelectStateChanged(bool isChecked)
{
	ui->checkBox->blockSignals(true);
	ui->checkBox->setChecked(isChecked);
	emit sigSelectionChanged(mLastUUid, isChecked);
	PLS_UI_ACTION(!isChecked ? "Uncheck Channel Item Done" : "Check Channel Item Done");
}

void ChannelItem::updateTextLabel()
{
	int channelState = getInfo(mLastData, g_channelStatus, Error);
	QString displayText;
	if (channelState == Valid) {
		displayText = getInfo(mLastData, ChannelData::g_nickName);
		displayText = getElidedText(ui->NameLabel, displayText, ui->NameLabel->contentsRect().width());
	} else {
		displayText = getInfo(mLastData, ChannelData::g_errorString);
		ui->NameLabel->setWordWrap(true);
	}

	ui->NameLabel->setText(displayText);
}
