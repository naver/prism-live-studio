#include "ChannelConfigPannel.h"
#include "ui_ChannelConfigPannel.h"
#include "ChannelCommonFunctions.h"
#include "PLSChannelsVirualAPI.h"
#include "ChannelConst.h"
#include <QMenu>
#include <QWidgetAction>
#include "alert-view.hpp"
#include <QUrl>
#include "LogPredefine.h"
#include "pls-net-url.hpp"

using namespace ChannelData;

ChannelConfigPannel::ChannelConfigPannel(QWidget *parent) : QFrame(parent), ui(new Ui::ChannelConfigPannel)
{
	ui->setupUi(this);
	auto btns = this->findChildren<QPushButton *>();
	std::for_each(btns.begin(), btns.end(), [](QPushButton *btn) {
		if (!btn->toolTip().isEmpty()) {
			btn->setAttribute(Qt::WA_AlwaysShowToolTips);
		}
	});
}

ChannelConfigPannel::~ChannelConfigPannel()
{
	delete ui;
}

void ChannelConfigPannel::setChannelID(const QString &channelID)
{
	mChannelID = channelID;
	updateUI();
}

void ChannelConfigPannel::changeEvent(QEvent *e)
{
	QFrame::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}

bool ChannelConfigPannel::eventFilter(QObject *watched, QEvent *event)
{
	switch (event->type()) {
	case QEvent::HoverEnter:

		break;
	case QEvent::HoverLeave:

		break;
	default:
		break;
	}
	return false;
}

void ChannelConfigPannel::on_showInfoBtn_clicked()
{
	PRE_LOG_UI(showInfoBtn, ChannelConfigPannel);
	this->hide();
	showChannelInfo(mChannelID);
}

void ChannelConfigPannel::on_ShareBtn_clicked()
{
	this->hide();
	PRE_LOG_UI(ShareBtn, ChannelConfigPannel);
	showShareView(mChannelID);
}

void ChannelConfigPannel::on_ConfigBtn_clicked()
{
	QSignalBlocker blocker(this);
	auto removeChannel = [=]() {
		auto ret = PLSAlertView::question(nullptr, CHANNELS_TR(Confirm), CHANNELS_TR(DisconnectWarning).arg(PLSCHANNELS_API->getValueOfChannel(mChannelID, g_channelName, QString("RTMP"))),
						  {{PLSAlertView::Button::Yes, CHANNELS_TR(Yes)}, {PLSAlertView::Button::Cancel, CHANNELS_TR(Cancel)}}, PLSAlertView::Button::Cancel);
		if (ret != PLSAlertView::Button::Yes) {
			return;
		}
		PRE_LOG_UI(RemoveChannel, ChannelConfigPannel);
		PLSCHANNELS_API->sigTryRemoveChannel(mChannelID, true, true);
	};

	auto gotoYoutube = [=]() { QDesktopServices::openUrl(QUrl(g_yoububeLivePage)); };

	QMenu menu(this);
	menu.addAction(CHANNELS_TR(Disconnect), this, removeChannel);
	int state = PLSCHANNELS_API->getChannelStatus(mChannelID);
	if (state == UnInitialized) {
		menu.addAction(CHANNELS_TR(GoTo), this, gotoYoutube);
	}

	menu.exec(QCursor::pos());
	this->hide();
}

void ChannelConfigPannel::on_EnableSwitch_toggled(bool checked)
{
	PRE_LOG_UI(SwitchChannel, ChannelConfigPannel);
	bool isLiving = PLSCHANNELS_API->isLiving();
	//to uncheck and isliving
	if (!checked && isLiving) {
		auto ret = PLSAlertView::question(nullptr, CHANNELS_TR(Confirm), CHANNELS_TR(DisableWarnig).arg(PLSCHANNELS_API->getValueOfChannel(mChannelID, g_channelName, QString("RTMP"))),
						  {{PLSAlertView::Button::Yes, CHANNELS_TR(Yes)}, {PLSAlertView::Button::Cancel, CHANNELS_TR(Cancel)}}, PLSAlertView::Button::Cancel);
		//reject disable
		if (ret != PLSAlertView::Button::Yes) {
			QSignalBlocker blocker(ui->EnableSwitch);
			ui->EnableSwitch->setChecked(true);
			PLSCHANNELS_API->setChannelUserStatus(mChannelID, Enabled);
			return;
		}
		//agree disable
		PLSCHANNELS_API->setChannelUserStatus(mChannelID, Disabled);
		PLSCHANNELS_API->setChannelStatus(mChannelID, Error);
		return;
	}

	//to uncheck and not living
	if (!checked && !isLiving) {
		PLSCHANNELS_API->setChannelUserStatus(mChannelID, Disabled);
		return;
	}
	//to checked ,is now channel
	bool isNow = isNowChannel(mChannelID);
	if (isNow && checked) {
		disableAll();
		PLSCHANNELS_API->setChannelUserStatus(mChannelID, Enabled);
		return;
	}
	//to checked not now
	if (!isNow && checked) {
		auto nowId = findExistEnabledNow();
		//checked now exists
		if (!nowId.isEmpty()) {
			auto ret = PLSAlertView::question(nullptr, CHANNELS_TR(Confirm), CHANNELS_TR(NowDisableWarning),
							  {{PLSAlertView::Button::Yes, CHANNELS_TR(Yes)}, {PLSAlertView::Button::Cancel, CHANNELS_TR(Cancel)}}, PLSAlertView::Button::Cancel);
			//do not want to disable now
			if (ret != PLSAlertView::Button::Yes) {
				QSignalBlocker blocker(ui->EnableSwitch);
				ui->EnableSwitch->setChecked(false);
				PLSCHANNELS_API->setChannelUserStatus(mChannelID, Disabled);
				return;
			}
			//disable now
			PLSCHANNELS_API->setChannelUserStatus(nowId, Disabled);
			PLSCHANNELS_API->setChannelUserStatus(mChannelID, Enabled);
			return;
		}
		// checked now is not exist and to checked and max limitted
		if (PLSCHANNELS_API->currentSelectedCount() >= 6) {
			PLSAlertView::warning(nullptr, CHANNELS_TR(warning.title), CHANNELS_TR(info.selectedLimited));
			QSignalBlocker blocker(ui->EnableSwitch);
			ui->EnableSwitch->setChecked(false);
			PLSCHANNELS_API->setChannelUserStatus(mChannelID, Disabled);
			return;
		}

		// checked now is not exist and to checked and in limitted
		PLSCHANNELS_API->setChannelUserStatus(mChannelID, Enabled);
		return;
	}
}

void ChannelConfigPannel::shiftState(const QVariantMap &info)
{
	QSignalBlocker blocker(ui->EnableSwitch);
	int userState = getInfo(info, g_channelUserStatus, NotExist);
	int dataState = getInfo(info, g_channelStatus, Error);
	int dataType = getInfo(info, g_data_type, NoType);
	int currentSelected = 0;
	bool isLiving = PLSCHANNELS_API->isLiving();
	if (isLiving) {
		currentSelected = PLSCHANNELS_API->currentSelectedValidCount();
	} else {
		currentSelected = PLSCHANNELS_API->currentSelectedCount();
	}

	ui->EnableSwitch->setChecked(userState == Enabled);
	ui->EnableSwitch->setToolTip((userState == Enabled ? CHANNELS_TR(EnableTip) : CHANNELS_TR(DisableTip)));
	bool isEnabeld = true;
	if (dataState != Valid && dataState != Expired) {
		isEnabeld = false;
	}

	if (isLiving && currentSelected <= 1) {
		isEnabeld = false;
	}
	ui->EnableSwitch->setEnabled(isEnabeld);

	ui->showInfoBtn->setProperty(g_data_type.toStdString().c_str(), dataType);
	refreshStyle(ui->showInfoBtn);
	switch (dataType) {
	case RTMPType:
		ui->showInfoBtn->setToolTip(CHANNELS_TR(EditRTMPTips));
		ui->showInfoBtn->setEnabled(!isLiving);
		ui->ShareBtn->setEnabled(false);
		break;
	case ChannelType:
		ui->showInfoBtn->setToolTip(CHANNELS_TR(InfoTip));
		ui->showInfoBtn->setEnabled(dataState == Valid);
		ui->ShareBtn->setEnabled(isLiving || dataState == Valid || dataState == Expired);
		break;
	default:
		break;
	}
	ui->ConfigBtn->setVisible(!isLiving);
	ui->configSpacer->changeSize(isLiving ? 0 : 40, 20, QSizePolicy::Expanding, QSizePolicy::Expanding);
	ui->horizontalLayout->invalidate();
}

void ChannelConfigPannel::updateUI()
{
	auto &info = PLSCHANNELS_API->getChanelInfoRef(mChannelID);
	if (info.isEmpty()) {
		return;
	}
	shiftState(info);
	return;
}
