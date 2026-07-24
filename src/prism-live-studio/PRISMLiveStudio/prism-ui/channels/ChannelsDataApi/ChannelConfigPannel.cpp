#include "ChannelConfigPannel.h"
#include <QMenu>
#include <QUrl>
#include <QWidgetAction>
#include "ChannelCommonFunctions.h"
#include "LogPredefine.h"
#include "PLSBasic.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelsVirualAPI.h"
#include "frontend-api.h"
#include "pls-channel-const.h"
#include "pls-net-url.hpp"
#include "pls-performance.h"
#include "pls/pls-dual-output.h"
#include "ui_ChannelConfigPannel.h"
#include "window-basic-main.hpp"
using namespace ChannelData;
constexpr auto resourcePath = ":/channels/resource/images/ChannelsSource/btn_dualoutput_%1_%2.svg";

class CustomStyle : public QProxyStyle {
public:
	int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override
	{
		if (metric == PM_SmallIconSize) {
			return 28;
		}
		return QProxyStyle::pixelMetric(metric, option, widget);
	}
};
ChannelConfigPannel::ChannelConfigPannel(QWidget *parent) : QFrame(parent), ui(new Ui::ChannelConfigPannel)
{
	PLS_DISABLE_UISTEP_V2(this);
	ui->setupUi(this);
	pls_add_css(this, {"ChannelConfigPannel"});
	auto btns = this->findChildren<QPushButton *>();
	setNoSetDirectionUI();

	m_dualMenu.setObjectName("dualMenu");
	m_dualMenu.setWindowFlags(Qt::Popup | Qt::NoDropShadowWindowHint);
	m_dualMenu.setStyle(new CustomStyle);
	m_dualMenu.setStyleSheet("QMenu::item {padding-left: 15px; font-size:14px;padding-top:0px;padding-bottom:0px;padding-right:-24px;} QMenu::icon{left:8px;}");
	QMetaEnum metaEnum = QMetaEnum::fromType<OUTPUTDIRECTION>();
	auto menuSize = metaEnum.keyCount();
	for (int i = 0; i < menuSize; ++i) {
		auto name = metaEnum.valueToKey(i);
		auto lickedSlot = [i, this](bool isChecked) {
			switch (static_cast<OUTPUTDIRECTION>(i)) {
			case horizontal:
				onClickHorizontalOutput();
				break;
			case vertical:
				onClickVerticalOutput();
				break;
			case none:
				onClickNoSetOutput();

				break;
			default:
				break;
			}
		};
		auto action = m_dualMenu.addAction(QIcon(pls_load_svg(QString(resourcePath).arg(name).arg("off"), 4 * QSize(28, 28))),
						   tr(QString("Channels.dualoutput.%1").arg(name).toUtf8().constData()), lickedSlot);
		action->setCheckable(true);
		action->setParent(this);
	}
	connect(ui->dualOutputBtn, &QPushButton::clicked, this, &ChannelConfigPannel::onShowDualoutputMenu);
	ui->dualOutputBtn->installEventFilter(this);
	pls_uistep_v2_set_title(this, QStringLiteral("Channel Config Pannel"));
}
ChannelConfigPannel::~ChannelConfigPannel()
{
	delete ui;
}

void ChannelConfigPannel::setChannelID(const QString &channelID)
{
	mChannelID = channelID;
	updateUI();
	auto channelName = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_channelName, QString(""));
	int myType = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_data_type, NoType);
	if (myType >= CustomType) {
		channelName.append("(R)");
	}
	pls_uistep_v2_custom(ui->EnableSwitch, PLS_UI_STEPS_V2_SIGNAL_CLICKED, PLS_UI_STEPS_V2_ACTION_SWITCH, channelName,
			     [this]() -> QString { return ui->EnableSwitch->isChecked() ? "On" : "Off"; });
	pls_uistep_v2_set_info(ui->ShareBtn, channelName, QStringLiteral("Share Channel Button"));
	pls_uistep_v2_set_info(ui->showInfoBtn, channelName, QStringLiteral("Show Live Info Button"));
	pls_uistep_v2_set_info(ui->ConfigBtn, channelName, QStringLiteral("More Config Button"));
	pls_uistep_v2_set_info(ui->dualOutputBtn, channelName, QStringLiteral("Dual Output Button"));
	pls_uistep_v2_set_custom_show_hide_name(this, QString("%1 Config Pannel").arg(channelName).toUtf8());
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

bool ChannelConfigPannel::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == ui->dualOutputBtn) {
		switch (event->type()) {
		case QEvent::HoverEnter:
			ui->arrowIconLabel->setProperty("state", "hover");
			pls_flush_style(ui->arrowIconLabel);
			break;
		case QEvent::HoverLeave:
			ui->arrowIconLabel->setProperty("state", "normal");
			pls_flush_style(ui->arrowIconLabel);
			break;
		case QEvent::MouseButtonPress:
			ui->arrowIconLabel->setProperty("state", "pressed");
			pls_flush_style(ui->arrowIconLabel);
			break;
		default:
			break;
		}
	}
	return QFrame::eventFilter(obj, event);
}

void ChannelConfigPannel::on_showInfoBtn_clicked()
{
#if defined(PLS_PERFORMANCE_STATS)
	auto channelName = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_channelName, QString(""));
	auto id = channelName.append("_ShowLiveInfoAllTime").toUtf8();
	PLS_PERFORMANCE_GLOBAL_START(id.constData());
	PLS_PERFORMANCE_GLOBAL_START("LiveInfoShow_Before", id.constData());
#endif
	this->hide();
	showChannelInfo(mChannelID);
}

void ChannelConfigPannel::on_ShareBtn_clicked()
{
	this->hide();
	showShareView(mChannelID);
}

void ChannelConfigPannel::askDeleteChannel()
{

	if (mIsAsking) {
		return;
	}
	mIsAsking = true;
	QSignalBlocker blocker(this);
	int myType = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_data_type, NoType);
	QString typeStr;
	if (myType == ChannelType) {
		typeStr = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_channelName, QString("RTMP"));
	} else {
		typeStr = "RTMP";
	}
	typeStr = translatePlatformName(typeStr);

	PLSErrorHandler::ExtraData extraData("Click Disconnect Button");
	extraData.defaultArg = {typeStr};
	PLSErrorHandler::RetData retData = PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_DISCONNECT_WARNING, PLSErrKeyAllAlert, {}, extraData);
	if (retData.clickedBtn != QDialogButtonBox::Yes) {
		mIsAsking = false;
		return;
	}

	PLSCHANNELS_API->sigTryRemoveChannel(mChannelID, true, true);
	mIsAsking = false;
}

void ChannelConfigPannel::onShowDualoutputMenu()
{
	QMetaEnum metaEnum = QMetaEnum::fromType<OUTPUTDIRECTION>();

	pls_check_app_exiting();
	resetActionsState();

	if (!m_bDualoutputMenuShow) {
		m_bDualoutputMenuShow = true;

		QPushButton *menuButton = dynamic_cast<QPushButton *>(sender());
		auto pos = ui->dualOutputBtn->mapToGlobal(ui->dualOutputBtn->rect().bottomLeft() + QPoint(-20, 0));
		pls_push_modal_view(&m_dualMenu);
		auto checkAction = m_dualMenu.actions().at(m_currentState);

		checkAction->setChecked(true);
		checkAction->setIcon(QIcon(pls_load_svg(QString(resourcePath).arg(metaEnum.valueToKey(m_currentState)).arg("on"), 4 * QSize(22, 22))));
		m_dualMenu.exec(pos);
		pls_pop_modal_view(&m_dualMenu);
		hide();
		ui->EnableSwitch->setChecked(m_currentState != none);
		m_bDualoutputMenuShow = false;

	} else {
		m_bDualoutputMenuShow = false;
	}
}

void ChannelConfigPannel::on_ConfigBtn_clicked()
{
	QSignalBlocker blocker(this);

	auto gotoYoutube = []() { QDesktopServices::openUrl(QUrl(g_yoububeLivePage)); };

	auto menu = createWidgetWidthDeleter<QMenu>(this);
	menu->setWindowFlag(Qt::NoDropShadowWindowHint);
	auto deleteAction = menu->addAction(CHANNELS_TR(Disconnect));
	deleteAction->setParent(this);
	auto platform = PLSCHANNELS_API->getValueOfChannel(mChannelID, ChannelData::g_channelName, QString());
	pls_uistep_v2_set_info(deleteAction, platform, QStringLiteral("Disconnect Menu"));
	if (PLSCHANNELS_API->isLiving()) {
		deleteAction->setDisabled(true);
	}
	connect(deleteAction, &QAction::triggered, this, &ChannelConfigPannel::askDeleteChannel, Qt::QueuedConnection);
	pls_uistep_v2_set_custom_show_hide_name(menu.data(), QString("%1 More Menu").arg(platform).toUtf8());
	if (PLSCHANNELS_API->getChannelStatus(mChannelID) == UnInitialized) {
		auto goYoutube = menu->addAction(CHANNELS_TR(GoTo), this, gotoYoutube);
		goYoutube->setParent(this);
	}
	auto channelT = PLSCHANNELS_API->getValueOfChannel(mChannelID, ChannelData::g_data_type, ChannelData::NoType);
	if (platform.contains(YOUTUBE, Qt::CaseInsensitive) && channelT == ChannelData::ChannelType) {
		auto subChannelID = PLSCHANNELS_API->getValueOfChannel(mChannelID, ChannelData::g_subChannelId, QString());
		auto gotoYoutubeControlPage = [subChannelID]() { QDesktopServices::openUrl(QUrl(g_yoububeStudioManagePage.arg(subChannelID))); };
		auto goYoutubeControlPage = menu->addAction(CHANNELS_TR(GotoYoutubeOk), this, gotoYoutubeControlPage);
		goYoutubeControlPage->setParent(this);
	}

	if (platform.contains(TWITCH, Qt::CaseInsensitive) && channelT == ChannelData::ChannelType) {
		auto nickName = PLSCHANNELS_API->getValueOfChannel(mChannelID, ChannelData::g_userName, QString());
		auto gotoTwitch = [nickName]() { QDesktopServices::openUrl(QUrl(g_twitchHomePage.arg(nickName))); };
		auto goTwitchAction = menu->addAction(CHANNELS_TR(GotoTwitchOk), this, gotoTwitch);
		goTwitchAction->setParent(this);
	}
	m_bMenuShow = true;
	QPointer<ChannelConfigPannel> tmp(this);
	menu->exec(QCursor::pos());
	if (tmp) {
		m_bMenuShow = false;
		this->hide();
	}
}

void ChannelConfigPannel::doChildrenExclusive(bool &retflag)
{
	retflag = true;

	if (auto myPlatform = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_channelName, QString()); !PLSCHANNELS_API->getCurrentSelectedPlatformChannels(myPlatform, ChannelType).isEmpty()) {

		//do not want to disable other child
		auto typeStr = translatePlatformName(myPlatform);
		PLSErrorHandler::ExtraData extraData("Channel enable child exclusive confirm");
		extraData.defaultArg = {typeStr};
		if (PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_CHANNEL_CHILD_EXCLUSIVE_CONFIRM, PLSErrKeyAllAlert, {}, extraData).clickedBtn != QDialogButtonBox::Yes) {
			QSignalBlocker blocker(ui->EnableSwitch);
			ui->EnableSwitch->setChecked(false);
			PLSCHANNELS_API->setChannelUserStatus(mChannelID, Disabled);
			onClickNoSetOutput();
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

		QString typeStr = PLSCHANNELS_API->getValueOfChannel(exclusiveID, g_channelName, QString(""));
		int myType = PLSCHANNELS_API->getValueOfChannel(exclusiveID, g_data_type, NoType);
		typeStr = translatePlatformName(typeStr);

		if (myType == RTMPType && !typeStr.contains("RTMP", Qt::CaseInsensitive)) {
			typeStr = typeStr + " RTMP";
		}

		//do not want to disable now
		PLSErrorHandler::ExtraData extraData("Channel exclusive disable other confirm");
		extraData.defaultArg = {typeStr};
		if (PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_CHANNEL_EXCLUSIVE_DISABLE_OTHER_CONFIRM, PLSErrKeyAllAlert, {}, extraData).clickedBtn != QDialogButtonBox::Yes) {
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
	QString typeStr = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_channelName, QString(""));
	typeStr = translatePlatformName(typeStr);
	if (!checked && isLiving) {

		//reject disable
		PLSErrorHandler::ExtraData extraData("Channel disable while living confirm");
		extraData.defaultArg = {typeStr};
		if (PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_CHANNEL_DISABLE_WHILE_LIVING_CONFIRM, PLSErrKeyAllAlert, {}, extraData).clickedBtn != QDialogButtonBox::Yes) {
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
	if (pls_is_dual_output_on()) {
		if (isExclusiveChannel(mChannelID)) {
			PLSErrorHandler::ExtraData extraData("Channel dual output exclusive warning");
			extraData.defaultArg = {typeStr};
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_CHANNEL_DUAL_OUTPUT_EXCLUSIVE_WARNING, PLSErrKeyAllAlert, {}, extraData);
			QSignalBlocker blocker(ui->EnableSwitch);
			ui->EnableSwitch->setChecked(false);
			return;
		}
	}

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

	if (showSelectedLimitedAlert("EnableSwitch")) {
		QSignalBlocker blocker(ui->EnableSwitch);
		ui->EnableSwitch->setChecked(false);
		PLSCHANNELS_API->setChannelUserStatus(mChannelID, Disabled);
		return;
	}

	// checked now is not exist and to checked and in limitted
	PLSCHANNELS_API->setChannelUserStatus(mChannelID, Enabled);
	/*****************to checked *****************************************/
	if (pls_is_dual_output_on() && checked && !m_bDualoutputMenuShow) {
		onShowDualoutputMenu();
	}
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
#if defined(Q_OS_MACOS)
	ui->EnableSwitch->setAttribute(Qt::WA_TransparentForMouseEvents, !isEnabled);
#endif
	auto platform = getInfo(info, g_channelName);
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
#if defined(Q_OS_MACOS)
	ui->showInfoBtn->setAttribute(Qt::WA_TransparentForMouseEvents, isLiving);
#endif
	ui->ShareBtn->setEnabled(false);
#if defined(Q_OS_MACOS)
	ui->ShareBtn->setAttribute(Qt::WA_TransparentForMouseEvents, true);
#endif
}

void ChannelConfigPannel::toChannelTypeState(int dataState, const QVariantMap &info)
{
	bool showInfoEnabled = (dataState == Valid);
	ui->showInfoBtn->setToolTip(QObject::tr("LiveInfo.liveinformation"));
	ui->showInfoBtn->setEnabled(showInfoEnabled);
#if defined(Q_OS_MACOS)
	ui->showInfoBtn->setAttribute(Qt::WA_TransparentForMouseEvents, !showInfoEnabled);
#endif
	bool isNeedShare = (info.contains(g_shareUrl) || info.contains(g_shareUrlTemp));
	bool shareEnabled = false;
	if (isNeedShare) {
		shareEnabled = (dataState == Valid || dataState == Expired);
	}
	ui->ShareBtn->setEnabled(shareEnabled);
#if defined(Q_OS_MACOS)
	ui->ShareBtn->setAttribute(Qt::WA_TransparentForMouseEvents, !shareEnabled);
#endif
}

void ChannelConfigPannel::updateUI()
{
	const auto info = PLSCHANNELS_API->getChannelInfo(mChannelID);
	if (info.isEmpty()) {
		return;
	}
	auto bOpen = pls_is_dual_output_on();
	updateUISpacing(bOpen);
	setDualOutput(bOpen);
	shiftState(info);

	auto platform = getInfo(info, g_channelName);
	bool enabled = !PLSCHANNELS_API->isRehearsaling() || g_rehearsalingConfigEnabledList.contains(platform);
	this->setEnabled(enabled);
	ui->dualOutputBtn->setEnabled(ui->EnableSwitch->isEnabled());

	return;
}

void ChannelConfigPannel::setDualOutput(bool bOpen)
{
	if (/*PLSCHANNELS_API->getChannelUserStatus(mChannelID) != Enabled || */ PLSCHANNELS_API->isLiving()) {
		bOpen = false;
	}
	if (bOpen) {
		auto dualOutput = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_channelDualOutput, NoSet);
		switch (dualOutput) {
		case channel_data::NoSet:
			setNoSetDirectionUI();
			break;
		case channel_data::HorizontalOutput:
			setHorizontalOutputUI();
			break;
		case channel_data::VerticalOutput:
			setVerticalOutputUI();
			break;
		default:
			break;
		}
	} else {
		closeDualOutputUI();
	}
}

void ChannelConfigPannel::setHorizontalOutputUI()
{

	m_currentState = horizontal;

	ui->dualOutputBtn->setVisible(true);
	ui->dualOutputIconLabel->setProperty("orientation", "H");
	refreshStyle(ui->dualOutputIconLabel);
}

void ChannelConfigPannel::setVerticalOutputUI()
{

	m_currentState = vertical;
	ui->dualOutputBtn->setVisible(true);
	ui->dualOutputIconLabel->setProperty("orientation", "V");
	refreshStyle(ui->dualOutputIconLabel);
}

void ChannelConfigPannel::setNoSetDirectionUI()
{
	m_currentState = none;
	ui->dualOutputBtn->setVisible(pls_is_dual_output_on());
	ui->dualOutputIconLabel->setProperty("orientation", "None");
	refreshStyle(ui->dualOutputIconLabel);
}

void ChannelConfigPannel::closeDualOutputUI()
{
	ui->dualOutputBtn->setVisible(false);
	updateUISpacing(false);
}

void ChannelConfigPannel::onClickVerticalOutput()
{
	auto dualOutput = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_channelDualOutput, NoSet);
	if (dualOutput == VerticalOutput) {
		return;
	}
	if (showSelectedLimitedAlert("Voutput")) {
		return;
	}
	setVerticalOutputUI();

	PLSCHANNELS_API->setValueOfChannel(mChannelID, g_channelDualOutput, VerticalOutput);
	PLSCHANNELS_API->channelModified(mChannelID);
	PLSCHANNELS_API->sigSetChannelDualOutput(mChannelID, VerticalOutput);
}

void ChannelConfigPannel::onClickHorizontalOutput()
{
	auto dualOutput = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_channelDualOutput, NoSet);
	if (dualOutput == HorizontalOutput) {
		//ui->Houtput->setChecked(true);
		return;
	}
	if (showSelectedLimitedAlert("Houtput")) {
		return;
	}

	setHorizontalOutputUI();
	PLSCHANNELS_API->setValueOfChannel(mChannelID, g_channelDualOutput, HorizontalOutput);
	PLSCHANNELS_API->channelModified(mChannelID);
	PLSCHANNELS_API->sigSetChannelDualOutput(mChannelID, HorizontalOutput);
}

void ChannelConfigPannel::onClickNoSetOutput()
{
	auto dualOutput = PLSCHANNELS_API->getValueOfChannel(mChannelID, g_channelDualOutput, NoSet);
	if (dualOutput == NoSet) {
		return;
	}
	setNoSetDirectionUI();

	PLSCHANNELS_API->setValueOfChannel(mChannelID, g_channelDualOutput, NoSet);
	PLSCHANNELS_API->channelModified(mChannelID);
	PLSCHANNELS_API->sigSetChannelDualOutput(mChannelID, NoSet);
}
void ChannelConfigPannel::resetActionsState()
{
	QMetaEnum metaEnum = QMetaEnum::fromType<OUTPUTDIRECTION>();
	auto actionSize = m_dualMenu.actions().size();

	for (auto index = 0; index < actionSize; ++index) {
		auto actionTmp = m_dualMenu.actions()[index];
		if (actionTmp) {
			QSignalBlocker blocker(m_dualMenu);
			if (isExclusiveChannel(mChannelID)) {
				actionTmp->setDisabled(true);
			} else {
				auto name = metaEnum.valueToKey(index);
				actionTmp->setIcon(QIcon(pls_load_svg(QString(resourcePath).arg(name).arg("off"), 4 * QSize(28, 28))));
				actionTmp->setChecked(false);
			}
		}
	}
}

void ChannelConfigPannel::updateUISpacing(bool isDualOutput)
{
	if (isDualOutput) {
		ui->horizontalSpacer->changeSize(13, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
		ui->horizontalSpacer_2->changeSize(5, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
		ui->horizontalSpacer_3->changeSize(9, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
		ui->horizontalSpacer_4->changeSize(10, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
		ui->horizontalSpacer_5->changeSize(9, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
		ui->horizontalSpacer_6->changeSize(9, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
		ui->horizontalSpacer_7->changeSize(9, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
	} else {
		ui->horizontalSpacer->changeSize(13, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
		ui->horizontalSpacer_2->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
		ui->horizontalSpacer_3->changeSize(13, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
		ui->horizontalSpacer_4->changeSize(22, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
		ui->horizontalSpacer_5->changeSize(20, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
		ui->horizontalSpacer_6->changeSize(20, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
		ui->horizontalSpacer_7->changeSize(17, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
	}
}

bool ChannelConfigPannel::showSelectedLimitedAlert(const QString &objectName)
{
	auto allowCount = PLSCHANNELS_API->getUserAllowedEnabledChannelsCount();
	bool bShowAlert = false;

	if (objectName == "EnableSwitch" && PLSCHANNELS_API->currentSelectedCount() >= allowCount) {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_CHANNEL_SELECTED_LIMITED, PLSErrKeyAllAlert, {},
						      PLSErrorHandler::ExtraData("Channel selected limited EnableSwitch"));
		bShowAlert = true;
		return bShowAlert;
	}

	if (objectName == "Houtput" || objectName == "Voutput") {
		QStringList horOutputList, verOutputList;
		PLSCHANNELS_API->getChannelCountOfOutputDirection(horOutputList, verOutputList);
		int horOutputCount = horOutputList.count();
		int verOutputCount = verOutputList.count();

		if (horOutputCount + verOutputCount >= allowCount && PLSCHANNELS_API->getValueOfChannel(mChannelID, g_channelDualOutput, NoSet) == NoSet) {
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_CHANNEL_SELECTED_LIMITED, PLSErrKeyAllAlert, {},
							      PLSErrorHandler::ExtraData("Channel selected limited dual output direction"));
			bShowAlert = true;
		}
	}

	return bShowAlert;
}
