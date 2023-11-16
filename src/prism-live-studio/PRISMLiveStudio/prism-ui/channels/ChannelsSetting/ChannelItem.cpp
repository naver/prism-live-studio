#include "ChannelItem.h"
#include "ChannelCommonFunctions.h"
#include "LogPredefine.h"
#include "pls-channel-const.h"
#include "ui_ChannelItem.h"

using namespace ChannelData;

ChannelItem::ChannelItem(QWidget *parent) : QPushButton(parent), ui(new Ui::ChannelItem)
{
	ui->setupUi(this);
	this->setCheckable(true);
	connect(this, &QPushButton::toggled, this, &ChannelItem::onSelectStateChanged);
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
	bool toSharp = true;
	if (channelState == Valid) {
		getComplexImageOfChannel(uuid, userIcon, platformIcon);
	} else {
		auto platformName = getInfo(mLastData, g_platformName);
		userIcon = getPlatformImageFromName(platformName);
		toSharp = false;
	}
	ui->IconLabel->setMainPixmap(userIcon, QSize(34, 34), toSharp);
	ui->IconLabel->setPlatformPixmap(platformIcon, QSize(18, 18));

	updateTextLabel();

	QSignalBlocker bloker(this);
	bool isSelected = getInfo(mLastData, ChannelData::g_displayState, true);
	ui->checkBox->setChecked(isSelected);
	this->setChecked(isSelected);
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
	QString msg = getInfo(mLastData, g_displayPlatformName) + QString(" select state changed:") + (isChecked ? "true" : "false");
	PRE_LOG_UI_MSG(msg.toStdString().c_str(), ChannelItem)
	emit sigSelectionChanged(mLastUUid, isChecked);
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
