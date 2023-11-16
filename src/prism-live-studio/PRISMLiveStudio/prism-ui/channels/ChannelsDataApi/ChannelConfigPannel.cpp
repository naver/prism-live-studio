#include "ChannelConfigPannel.h"
#include <QMenu>
#include <QUrl>
#include <QWidgetAction>
#include "ChannelCommonFunctions.h"
#include "LogPredefine.h"
#include "PLSAlertView.h"
#include "PLSBasic.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelsVirualAPI.h"
#include "frontend-api.h"
#include "pls-channel-const.h"
#include "pls-net-url.hpp"
#include "ui_ChannelConfigPannel.h"
#include "window-basic-main.hpp"
using namespace ChannelData;

ChannelConfigPannel::ChannelConfigPannel(QWidget *parent) : QFrame(parent), ui(new Ui::ChannelConfigPannel)
{
	ui->setupUi(this);
	pls_add_css(this, {"ChannelConfigPannel"});
	auto btns = this->findChildren<QPushButton *>();
	std::for_each(btns.begin(), btns.end(), [](QPushButton *btn) {
		if (!btn->toolTip().isEmpty()) {
			btn->setAttribute(Qt::WA_AlwaysShowToolTips);
		}
	});

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::liveStateChanged, this, &ChannelConfigPannel::closeBowser, Qt::QueuedConnection);
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

bool ChannelConfigPannel::eventFilter(QObject *, QEvent *event)
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
	PRE_LOG_UI_MSG_STRING("Show Live Info", "Clicked")
	this->hide();
	showChannelInfo(mChannelID);
}

void ChannelConfigPannel::on_ShareBtn_clicked()
{
	this->hide();
	PRE_LOG_UI_MSG_STRING(" Share Button ", "Clicked")
	showShareView(mChannelID);
}

void ChannelConfigPannel::askDeleteChannel()
{

	if (mIsAsking) {
		PRE_LOG_UI_MSG_STRING(" Remove Channel again ", "Clicked")
		return;
	}
	PRE_LOG_UI_MSG_STRING(" Remove Channel ", "Clicked")
	mIsAsking = true;
	QSignalBlocker blocker(this);
	int myType = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_data_type, NoType);
	QString typeStr;
	if (myType == ChannelType) {
		typeStr = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_platformName, QString("RTMP"));
	} else {
		typeStr = "RTMP";
	}
	typeStr = translatePlatformName(typeStr);

	if (auto ret = PLSAlertView::question(nullptr, CHANNELS_TR(Confirm), CHANNELS_TR(DisconnectWarning).arg(typeStr),
					      {{PLSAlertView::Button::Yes, QObject::tr("OK")}, {PLSAlertView::Button::Cancel, QObject::tr("Cancel")}}, PLSAlertView::Button::Cancel);
	    ret != PLSAlertView::Button::Yes) {
		mIsAsking = false;
		return;
	}

	PLSCHANNELS_API->sigTryRemoveChannel(mChannelID, true, true);
	mIsAsking = false;
}

void ChannelConfigPannel::closeBowser()
{
	int state = PLSCHANNELS_API->currentBroadcastState();
	auto platform = PLSCHANNELS_API->getValueOfChannel(mChannelID, ChannelData::g_platformName, QString());
	auto channelT = PLSCHANNELS_API->getValueOfChannel(mChannelID, ChannelData::g_data_type, ChannelData::NoType);
	if (platform.contains(YOUTUBE, Qt::CaseInsensitive) && (channelT == ChannelData::ChannelType) && (state == StreamEnd || state == StreamStopped)) {
		if (m_Browser) {
			m_Browser->close();
		}
	}
}

void ChannelConfigPannel::on_ConfigBtn_clicked()
{
	QSignalBlocker blocker(this);

	auto gotoYoutube = []() { QDesktopServices::openUrl(QUrl(g_yoububeLivePage)); };

	auto menu = createWidgetWidthDeleter<QMenu>(this);
	menu->setWindowFlag(Qt::NoDropShadowWindowHint);
	auto deleteAction = menu->addAction(CHANNELS_TR(Disconnect));
	if (PLSCHANNELS_API->isLiving()) {
		deleteAction->setDisabled(true);
	}
	connect(deleteAction, &QAction::triggered, this, &ChannelConfigPannel::askDeleteChannel, Qt::QueuedConnection);
	auto platform = PLSCHANNELS_API->getValueOfChannel(mChannelID, ChannelData::g_platformName, QString());
	if (PLSCHANNELS_API->getChannelStatus(mChannelID) == UnInitialized) {
		menu->addAction(CHANNELS_TR(GoTo), this, gotoYoutube);
	}
	auto channelT = PLSCHANNELS_API->getValueOfChannel(mChannelID, ChannelData::g_data_type, ChannelData::NoType);
	if (platform.contains(YOUTUBE, Qt::CaseInsensitive) && channelT == ChannelData::ChannelType) {

		auto subChannelID = PLSCHANNELS_API->getValueOfChannel(mChannelID, ChannelData::g_subChannelId, QString());
		auto gotoYoutubeControlPage = [subChannelID]() { QDesktopServices::openUrl(QUrl(g_yoububeStudioManagePage.arg(subChannelID))); };
		menu->addAction(CHANNELS_TR(GotoYoutubeOk), this, gotoYoutubeControlPage);

		auto broadcastID = PLSCHANNELS_API->getValueOfChannel(mChannelID, ChannelData::g_broadcastID, QString());
		auto gotoYoutubeDashBord = [broadcastID, this]() {
			if (m_Browser.isNull()) {
				m_Browser = pls::browser::newBrowserDialog(
					pls::browser::Params()
						.url(g_youtubeDashbordPage.arg(broadcastID))
						.showAtLoadEnded(true)
						.cookieStoragePath(PLSBasic::cookiePath(YOUTUBE))
						.allowPopups(false)
						.title(CHANNELS_TR(config.gotoyoutube.dashbord))
						.size(QSize(600, 700))
						.autoSetTitle(false)
						.parent(App()->getMainView())
						.urlChanged([this, broadcastID](pls::browser::Browser *browser, const QString &urlStr, const QList<pls::browser::Cookie> &cookies) {
							if (urlStr.startsWith("https://accounts.google.com/ServiceLogin?service=youtube") && m_isChannelSwithed == false) {
								m_isChannelSwithed = true;
								auto nextUrl = g_youtubeDashbordPage.arg(broadcastID);
								QString switherUrl = QString("%1?app=desktop&next=%2").arg(g_plsYoutubeChannelSwithcerUrl).arg(nextUrl);
								QString encodeUrl = QUrl::toPercentEncoding(switherUrl);

								int index = urlStr.indexOf("&continue=");
								if (index != -1) {
									auto tmp = urlStr.left(index) + "&continue=" + encodeUrl + "&followup=" + encodeUrl;
									browser->url(tmp);
								}
							}
						}));
				m_Browser->setAttribute(Qt::WA_DeleteOnClose);
			} else {
				m_Browser->url(g_youtubeDashbordPage.arg(broadcastID));
			}
			m_Browser->show();
		};
		auto gotoDashbord = menu->addAction(CHANNELS_TR(config.gotoyoutube.dashbord), this, gotoYoutubeDashBord);
		if (!PLSCHANNELS_API->isLiving()) {
			gotoDashbord->setDisabled(true);
		}
	}
	if (platform.contains(TWITCH, Qt::CaseInsensitive) && channelT == ChannelData::ChannelType) {
		auto nickName = PLSCHANNELS_API->getValueOfChannel(mChannelID, ChannelData::g_userName, QString());
		auto gotoTwitch = [nickName]() { QDesktopServices::openUrl(QUrl(g_twitchHomePage.arg(nickName))); };
		menu->addAction(CHANNELS_TR(GotoTwitchOk), this, gotoTwitch);
	}
	m_bMenuShow = true;
	menu->exec(QCursor::pos());
	m_bMenuShow = false;
	this->hide();
}

void ChannelConfigPannel::doChildrenExclusive(bool &retflag) const
{
	retflag = true;

	if (auto myPlatform = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_platformName, QString()); !PLSCHANNELS_API->getCurrentSelectedPlatformChannels(myPlatform, ChannelType).isEmpty()) {

		//do not want to disable other child
		if (auto typeStr = translatePlatformName(myPlatform); PLSAlertView::question(nullptr, CHANNELS_TR(Confirm), CHANNELS_TR(ChildDisableWaring).arg(typeStr),
											     {{PLSAlertView::Button::Yes, QObject::tr("OK")}, {PLSAlertView::Button::Cancel, QObject::tr("Cancel")}},
											     PLSAlertView::Button::Cancel) != PLSAlertView::Button::Yes) {
			QSignalBlocker blocker(ui->EnableSwitch);
			ui->EnableSwitch->setChecked(false);
			PLSCHANNELS_API->setChannelUserStatus(mChannelID, Disabled);
			return;
		}

		childExclusive(mChannelID);
		PLSCHANNELS_API->setChannelUserStatus(mChannelID, Enabled);
		return;
	}
	retflag = false;
}

void ChannelConfigPannel::checkExclusiveChannel(bool &retflag)
{
	retflag = true;

	//checked now exists
	if (auto exclusiveID = findExistEnabledExclusiveChannel(); !exclusiveID.isEmpty()) {

		QString typeStr = PLSCHANNELS_API->getValueOfChannel(exclusiveID, g_platformName, QString(""));
		int myType = PLSCHANNELS_API->getValueOfChannel(exclusiveID, g_data_type, NoType);
		if (myType == RTMPType && !typeStr.contains("RTMP", Qt::CaseInsensitive)) {
			typeStr = typeStr + " RTMP";
		}

		typeStr = translatePlatformName(typeStr);

		//do not want to disable now
		if (auto ret = PLSAlertView::question(nullptr, CHANNELS_TR(Confirm), CHANNELS_TR(NowDisableWarning).arg(typeStr),
						      {{PLSAlertView::Button::Yes, QObject::tr("OK")}, {PLSAlertView::Button::Cancel, QObject::tr("Cancel")}}, PLSAlertView::Button::Cancel);
		    ret != PLSAlertView::Button::Yes) {
			QSignalBlocker blocker(ui->EnableSwitch);
			ui->EnableSwitch->setChecked(false);
			PLSCHANNELS_API->setChannelUserStatus(mChannelID, Disabled);
			return;
		}
		//disable now
		PLSCHANNELS_API->setChannelUserStatus(exclusiveID, Disabled);
		PLSCHANNELS_API->setChannelUserStatus(mChannelID, Enabled);
		return;
	}
	retflag = false;
}

void ChannelConfigPannel::on_EnableSwitch_toggled(bool checked)
{
	PRE_LOG_UI_MSG_STRING(" Switch Channel " + QString(checked ? " ON" : "OFF"), "Clicked")
	int myType = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_data_type, NoType);
	bool isLiving = PLSCHANNELS_API->isLiving();
	//to uncheck and isliving
	if (!checked && isLiving) {
		QString typeStr = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_platformName, QString(""));
		typeStr = translatePlatformName(typeStr);

		//reject disable
		if (auto ret = PLSAlertView::question(nullptr, CHANNELS_TR(Confirm), CHANNELS_TR(DisableWarnig).arg(typeStr),
						      {{PLSAlertView::Button::Yes, QObject::tr("OK")}, {PLSAlertView::Button::Cancel, QObject::tr("Cancel")}}, PLSAlertView::Button::Cancel);
		    ret != PLSAlertView::Button::Yes) {
			QSignalBlocker blocker(ui->EnableSwitch);
			ui->EnableSwitch->setChecked(true);
			PLSCHANNELS_API->setChannelUserStatus(mChannelID, Enabled);
			return;
		}
		//agree disable
		PLSCHANNELS_API->setChannelUserStatus(mChannelID, Disabled);
		return;
	}

	//to uncheck and not living
	if (!checked && !isLiving) {
		PLSCHANNELS_API->setChannelUserStatus(mChannelID, Disabled);
		return;
	}
	/************************to  checked******************************/

	/* not exclusive channel */
	if (myType == ChannelType) {
		bool retflag;
		//close other if is multi children platform
		doChildrenExclusive(retflag);
		if (retflag) {
			return;
		}
	}

	bool hasExclusive;
	checkExclusiveChannel(hasExclusive);
	if (hasExclusive) {
		return;
	}
	//to checked ,is now channel
	if (isExclusiveChannel(mChannelID)) {
		//to checked not now
		disableAll();
		PLSCHANNELS_API->setChannelUserStatus(mChannelID, Enabled);
		return;
	}

	// checked now is not exist and to checked and max limitted
	if (PLSCHANNELS_API->currentSelectedCount() >= 6) {
		pls_alert_error_message(nullptr, QObject::tr("Alert.Title"), CHANNELS_TR(info.selectedLimited));
		QSignalBlocker blocker(ui->EnableSwitch);
		ui->EnableSwitch->setChecked(false);
		PLSCHANNELS_API->setChannelUserStatus(mChannelID, Disabled);
		return;
	}

	// checked now is not exist and to checked and in limitted
	PLSCHANNELS_API->setChannelUserStatus(mChannelID, Enabled);
	/*****************to checked *****************************************/
	return;
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

	auto checkIsEnabled = [&]() {
		if (isLiving && currentSelected <= 1) {
			return false;
		}

		if (isLiving && userState != Enabled) {
			return false;
		}

		if (dataState == Expired && userState == Enabled) {
			return true;
		}

		if (dataState == UnAuthorized && userState == Enabled) {
			return true;
		}

		if (dataState != Valid) {
			return false;
		}

		return true;
	};

	bool isEnabled = checkIsEnabled();

	ui->EnableSwitch->setEnabled(isEnabled);
	if (dataType >= CustomType) {
		ui->showInfoBtn->setProperty("shape", "pen");
	}
	refreshStyle(ui->showInfoBtn);
	if (dataType == ChannelType) {
		toChannelTypeState(dataState, info);
	} else if (dataType >= CustomType) {
		toRTMPTypeState(isLiving);
	}

#if 0
	ui->ConfigBtn->setVisible(!isLiving);
	if (isLiving) {
		ui->horizontalLayout->removeItem(ui->configSpacer);
		ui->horizontalLayout->removeWidget(ui->ConfigBtn);
	} else if (ui->horizontalLayout->indexOf(ui->configSpacer) < 0) {
		ui->horizontalLayout->addItem(ui->configSpacer);
		ui->horizontalLayout->addWidget(ui->ConfigBtn);
	}
#endif
}

void ChannelConfigPannel::toRTMPTypeState(bool isLiving)
{

	ui->showInfoBtn->setToolTip(CHANNELS_TR(EditRTMPTips));
	ui->showInfoBtn->setEnabled(!isLiving);
	ui->ShareBtn->setEnabled(false);
}

void ChannelConfigPannel::toChannelTypeState(int dataState, const QVariantMap &info)
{

	ui->showInfoBtn->setToolTip(QObject::tr("LiveInfo.liveinformation"));
	ui->showInfoBtn->setEnabled(dataState == Valid);
	bool isNeedShare = (info.contains(g_shareUrl) || info.contains(g_shareUrlTemp));
	if (isNeedShare) {
		ui->ShareBtn->setEnabled(dataState == Valid || dataState == Expired);
	} else {
		ui->ShareBtn->setEnabled(false);
	}
}

void ChannelConfigPannel::updateUI()
{
	const auto &info = PLSCHANNELS_API->getChanelInfoRef(mChannelID);
	if (info.isEmpty()) {
		return;
	}
	shiftState(info);

	auto platform = getInfo(info, g_platformName);
	bool enabled = !PLSCHANNELS_API->isRehearsaling() || g_rehearsalingConfigEnabledList.contains(platform);
	this->setEnabled(enabled);

	return;
}
