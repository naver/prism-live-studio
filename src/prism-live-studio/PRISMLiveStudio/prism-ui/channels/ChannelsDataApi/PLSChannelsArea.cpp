#include "PLSChannelsArea.h"
#include <QGuiApplication>
#include <QPushButton>
#include <QScrollBar>
#include <QTime>
#include <QToolButton>
#include <QWheelEvent>
#include <QWindow>
#include "ChannelCapsule.h"
#include "ChannelCommonFunctions.h"
#include "ChannelDefines.h"
#include "ChannelsAddWin.h"
#include "DefaultPlatformsAddList.h"
#include "GoLivePannel.h"
#include "LogPredefine.h"
#include "PLSAddingFrame.h"
#include "PLSBasic.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelsVirualAPI.h"
#include "libui.h"
#include "pls-channel-const.h"
#include "window-basic-main.hpp"
using namespace ChannelData;

PLSChannelsArea::PLSChannelsArea(QWidget *parent) : QFrame(parent)
{

	ui->setupUi(this);
	initializeMychannels();
	pls_add_css(this, {"PLSChannelsArea"});
	auto defaultAddWid = new DefaultPlatformsAddList;
	ui->AddFrame->layout()->addWidget(defaultAddWid);
	createFoldButton();
	initScollButtons();

	ui->MidFrame->setAttribute(Qt::WA_Hover);
	ui->MidFrame->installEventFilter(this);
	ui->MidFrame->setProperty("showRightBorder", "false");
	ui->AddFrameInvisible->setVisible(false);
	connect(
		ui->GotoAddWinButton, &QAbstractButton::clicked, this, []() { showChannelsSetting(); }, Qt::QueuedConnection);

	mbusyFrame = new PLSAddingFrame(ui->MidFrame);
	mbusyFrame->setObjectName("LoadingFrame");
	mbusyFrame->setContent(CHANNELS_TR(Loading));
	mbusyFrame->setSourceFirstFile(g_loadingPixPath);
	mbusyFrame->hide();
	mbusyFrame->installEventFilter(this);

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelAdded, this, QOverload<const QString &>::of(&PLSChannelsArea::addChannel), Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelRemoved, this, &PLSChannelsArea::removeChannel, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelModified, this, &PLSChannelsArea::updateChannelUi, Qt::QueuedConnection);
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::addChannelForDashBord, this,
		[this](QString uuid) {
			auto channelInfo = PLSCHANNELS_API->getChannelInfo(uuid);
			if (channelInfo.isEmpty()) {
				return;
			}

			bool isToShow = getInfo(channelInfo, g_displayState, true);
			if (isToShow) {
				addChannel(uuid);
			} else {
				removeChannelWithoutYoutubeDock(uuid);
			}
		},
		Qt::QueuedConnection);

	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::holdOnChannelArea, this,
		[this](bool isHold) {
			if (isHold) {
				mbusyFrame->setContent("");
				mbusyFrame->resize(48, 48);
			}

			holdOnChannelArea(isHold);
		},
		Qt::QueuedConnection);
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::addingHold, this,
		[this](bool isHold) {
			if (isHold) {
				mbusyFrame->setContent(CHANNELS_TR(Adding));
				mbusyFrame->resize(QSize(200, 48));
			}

			holdOnChannelArea(isHold);
		},
		Qt::QueuedConnection);

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::sigAllClear, this, &PLSChannelsArea::clearAll, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::liveStateChanged, this, &PLSChannelsArea::delayUpdateUi, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::liveTypeChanged, this, &PLSChannelsArea::delayUpdateUi, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toDoinitialize, this, &PLSChannelsArea::beginInitChannels, Qt::QueuedConnection);
	connect(this, &PLSChannelsArea::sigNextInitialize, this, &PLSChannelsArea::initializeNextStep, Qt::QueuedConnection);

	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::networkInvalidOcurred, this,
		[]() {
			pls_check_app_exiting();
			showNetworkErrorAlert();
		},
		Qt::QueuedConnection);

	pls_connect(PLSBasic::instance(), &PLSBasic::sigOpenDualOutput, this, &PLSChannelsArea::updateAllChannelsByDualOutput);

	this->setDisabled(true);
}

PLSChannelsArea::~PLSChannelsArea()
{
	mChannelsWidget.clear();
	m_FoldChannelsWidget.clear();
}

void PLSChannelsArea::beginInitChannels()
{

	isUiInitialized = false;
	checkIsEmptyUi();
	this->holdOnChannelArea(true);
	mInitializeInfos = PLSCHANNELS_API->sortAllChannels();
	PLSCHANNELS_API->release();
	emit sigNextInitialize();
}
void PLSChannelsArea::endInitialize()
{

	this->setEnabled(true);
	PLSCHANNELS_API->sigChannelAreaInialized();
	if (PLSCHANNELS_API->hasError()) {
		PLSCHANNELS_API->networkInvalidOcurred();
	}
	this->holdOnChannelArea(false);
	isUiInitialized = true;
	PLSCHANNELS_API->clearOldVersionImages();
}

void PLSChannelsArea::initializeNextStep()
{
	if (PLSCHANNELS_API->isExit()) {
		PLSCHANNELS_API->acquire();
		return;
	}

	if (!mInitializeInfos.isEmpty()) {
		auto channelInfo = mInitializeInfos.takeFirst();
		auto uuid = getInfo(channelInfo, g_channelUUID);
		channelInfo = PLSCHANNELS_API->getChannelInfo(uuid);

		if (!channelInfo.isEmpty()) {
			bool isToShow = getInfo(channelInfo, g_displayState, true);
			auto type = getInfo(channelInfo, g_data_type, NoType);
			auto platform = getInfo(channelInfo, g_channelName);
			if (type == ChannelType && platform.contains(YOUTUBE, Qt::CaseInsensitive)) {
				isToShow = true;
			}
			if (isToShow) {

				addChannel(channelInfo, true);
				addFoldChannel(channelInfo);
			}
		} else {
			PLS_WARN("PLSChannelsArea", "initialize channel,channelInfo is empty");
		}
	}

	if (!mInitializeInfos.isEmpty()) {
		emit sigNextInitialize();
		return;
	}

	if (mInitializeInfos.isEmpty()) {
		PLSCHANNELS_API->acquire();
		endInitialize();
		return;
	}
}

void PLSChannelsArea::holdOnChannelArea(bool holdOn)
{
	//hold
	isHolding = holdOn;
	if (holdOn && mbusyFrame->isVisible()) {
		return;
	}
	if (holdOn) {
		auto posParentC = ui->MidFrame->contentsRect().center();
		auto posMC = mbusyFrame->contentsRect().center();
		auto posDiffer = posParentC - posMC;
		mbusyFrame->move(posDiffer);
		mbusyFrame->start(200);
		mbusyFrame->setVisible(true);

		updateUi();

		return;
	}

	// unhold
	if (PLSCHANNELS_API->isEmptyToAcquire()) {
		delayUpdateUi();
		delayTask(&PLSChannelsArea::hideLoading, 300);
	}
}

void PLSChannelsArea::refreshOrder() const
{
	PLSCHANNELS_API->sortAllChannels();
}

void PLSChannelsArea::initScollButtons()
{

	mLeftButon = new QPushButton(ui->MidFrame);
	mLeftButon->setObjectName("LeftButton");
	mLeftButon->setAutoRepeat(true);
	mLeftButon->setAutoRepeatDelay(500);
	mLeftButon->setAutoRepeatInterval(500);
	ui->MidFrameLayout->insertWidget(0, mLeftButon);
	connect(mLeftButon, &QPushButton::clicked, [this]() { scrollNext(true); });
	mLeftButon->setVisible(false);

	mRightButton = new QPushButton(ui->MidFrame);
	mRightButton->setObjectName("RightButton");
	mRightButton->setAutoRepeat(true);
	mRightButton->setAutoRepeatDelay(500);
	mRightButton->setAutoRepeatInterval(500);
	ui->MidFrameLayout->addWidget(mRightButton);
	connect(mRightButton, &QPushButton::clicked, [this]() { scrollNext(false); });
	mRightButton->setVisible(false);

	ui->ChannelScrollArea->verticalScrollBar()->setDisabled(true);
	ui->NormalDisFrameLayout->addStretch(10);
	ui->FoldLayout->addStretch(10);
}

void PLSChannelsArea::checkScrollButtonsState(ScrollDirection direction)
{
	bool isNeeded = isScrollButtonsNeeded();
	enabledScrollButtons(isNeeded);
	displayScrollButtons(isNeeded);
	if (isNeeded) {
		switch (direction) {
		case PLSChannelsArea::ScrollDirection::NOScroll:
			break;
		case PLSChannelsArea::ScrollDirection::ForwardScroll:
			ensureCornerChannelVisible(true);
			break;
		case PLSChannelsArea::ScrollDirection::BackScroll:
			ensureCornerChannelVisible(false);
			break;
		default:
			break;
		}
		ui->MidFrame->setProperty("showRightBorder", "true");
		pls_flush_style(ui->MidFrame);
		buttonLimitCheck();
		if (!mLeftButon->isEnabled()) {
			ui->NormalDisFrameLayout->setContentsMargins(0, 0, 0, 0);
			ui->AddFrame->setContentsMargins(0, 0, 0, 0);
		}
	} else {
		ui->MidFrame->setProperty("showRightBorder", "false");
		pls_flush_style(ui->MidFrame);
		ui->NormalDisFrameLayout->setContentsMargins(20, 0, 0, 0);
		ui->AddFrame->setContentsMargins(20, 0, 0, 0);
	}
}

void PLSChannelsArea::insertFoldChannelCapsule(QWidget *wid, int index) const
{
	int layoutIndex = getLayoutOrder(true, ui->FoldCapslesFrameLayout, index);
	ui->FoldCapslesFrameLayout->insertWidget(layoutIndex, wid);
}

int PLSChannelsArea::getLayoutOrder(bool bFold, QHBoxLayout *layout, int channelOrder) const
{
	int count = layout->count();
	int layoutIndex = -1;
	for (int i = 0; i < count; i++) {
		auto item = layout->itemAt(i);
		if (item) {
			QVariantMap channelInfo;
			if (bFold) {
				auto foldCapsule = dynamic_cast<ChannelFoldCapsule *>(item->widget());
				channelInfo = PLSCHANNELS_API->getChannelInfo(foldCapsule->getChannelID());
			} else {
				auto capsule = dynamic_cast<ChannelCapsule *>(item->widget());
				channelInfo = PLSCHANNELS_API->getChannelInfo(capsule->getChannelID());
			}
			int order = getInfo(channelInfo, g_displayOrder, -1);

			if (order > channelOrder) {
				layoutIndex = i;
				break;
			}
		}
	}
	if (layoutIndex >= count) {
		layoutIndex = -1;
	}
	return layoutIndex;
}

void PLSChannelsArea::insertChannelCapsule(QWidget *wid, int index) const
{
	int layoutIndex = getLayoutOrder(false, ui->CapusuleLayout, index);
	ui->CapusuleLayout->insertWidget(layoutIndex, wid);
}

void PLSChannelsArea::scrollNext(bool forwartStep)
{
	ensureCornerChannelVisible(forwartStep);
	auto bar = ui->ChannelScrollArea->horizontalScrollBar();
	int itemWidth = checkIsEmptyUi() ? 90 : 200;
	int width = itemWidth;
	int currentV = bar->value();
	int lastV = currentV + (forwartStep ? -1 : 1) * width;
	if (lastV < 0) {
		lastV = 0;
	}
	bar->setValue(lastV);

	buttonLimitCheck();
	PLS_UI_STEP("PLSChannelsArea", "Scroll", QString::number(lastV).toUtf8().constData());
}

void PLSChannelsArea::ensureCornerChannelVisible(bool forwartStep) const
{
	auto recView = ui->ChannelScrollArea->contentsRect();
	auto targetPos = forwartStep ? (recView.topLeft() + QPoint(10, 10)) : (recView.topRight() + QPoint(-10, 10));
	auto child = ui->ChannelScrollArea->childAt(targetPos);

	if (child) {

		if (m_bFold) {
			auto capusle = findParent<ChannelFoldCapsule *>(child);
			if (capusle) {
				ui->ChannelScrollArea->ensureWidgetVisible(capusle, 1, 1);
			}
		} else {
			auto capusle = findParent<ChannelCapsule *>(child);
			if (capusle) {
				ui->ChannelScrollArea->ensureWidgetVisible(capusle, 1, 1);
			}
		}
	}
}

void PLSChannelsArea::buttonLimitCheck()
{
	auto bar = ui->ChannelScrollArea->horizontalScrollBar();
	mRightButton->setDisabled(bar->value() == bar->maximum());
	mLeftButon->setDisabled(bar->value() == bar->minimum());
}

void PLSChannelsArea::displayScrollButtons(bool isShow)
{
	mLeftButon->setVisible(isShow);
	mRightButton->setVisible(isShow);
}

void PLSChannelsArea::enabledScrollButtons(bool isEnabled)
{
	mLeftButon->setEnabled(isEnabled);
	mRightButton->setEnabled(isEnabled);
}

void PLSChannelsArea::addChannel(const QString &channelUUID)
{
	auto channelInfo = PLSCHANNELS_API->getChannelInfo(channelUUID);
	if (channelInfo.isEmpty()) {
		return;
	}

	bool isToShow = getInfo(channelInfo, g_displayState, true);
	if (!isToShow) {
		return;
	}

	auto channelWid = addChannel(channelInfo);
	addFoldChannel(channelInfo);
	if (!isUiInitialized || !channelWid->isVisible()) {
		return;
	}

	bool isLeader = getInfo(channelInfo, g_isLeader, true);
	if (isLeader) {
		checkScrollButtonsState(ScrollDirection::BackScroll);
		ui->ChannelScrollArea->ensureWidgetVisible(channelWid.data(), 10, 10);
	}
}

ChannelFoldCapsulePtr PLSChannelsArea::addFoldChannel(const QVariantMap &channelInfo)
{
	auto foldChannelWid = createWidgetWidthDeleter<ChannelFoldCapsule>(this);
	auto uuid = getInfo(channelInfo, g_channelUUID);
	foldChannelWid->setChannelID(uuid);
	int order = getInfo(channelInfo, g_displayOrder, -1);
	this->insertFoldChannelCapsule(foldChannelWid.data(), order);
	m_FoldChannelsWidget.insert(uuid, foldChannelWid);

	foldChannelWid->updateUi();
	foldChannelWid->setVisible(false);

	return foldChannelWid;
}

ChannelCapsulePtr PLSChannelsArea::addChannel(const QVariantMap &channelInfo, bool bInit)
{
	auto channelWid = createWidgetWidthDeleter<ChannelCapsule>(this);
	auto uuid = getInfo(channelInfo, g_channelUUID);
	channelWid->setChannelID(uuid);
	channelWid->setTopFrame(ui->ChannelScrollArea);
	int order = getInfo(channelInfo, g_displayOrder, -1);
	this->insertChannelCapsule(channelWid.data(), order);
	mChannelsWidget.insert(uuid, channelWid);
	bool isToShow = getInfo(channelInfo, g_displayState, true);
	if (isToShow) {
		channelWid->updateUi();
	}
	channelWid->setVisible(isToShow);
	auto subID = getInfo(channelInfo, g_subChannelId);
	auto platform = getInfo(channelInfo, g_channelName);

	if (isUiInitialized && PLSCHANNELS_API->currentTransactionCMDType() == (int)ChannelTransactionsKeys::CMDTypeValue::AddChannelCMD) {
		QString log = QString("Channel UI Added, ID:%1, sub ID:%2, order: %3, platform :%4 ").arg(uuid).arg(subID).arg(order).arg(platform);
		PRE_LOG_MSG_STEP(log, g_addChannelStep, INFO)
	}

	auto type = getInfo(channelInfo, g_data_type, NoType);
	if (cef_js_avail && type == ChannelType && platform.contains(YOUTUBE, Qt::CaseInsensitive)) {
		auto basic = PLSBasic::instance();
		if (!basic->GetYouTubeAppDock()) {
			basic->NewYouTubeAppDock();
		}
		basic->GetYouTubeAppDock()->AccountConnected();
		if (bInit) {
			const char *dockStateStr = config_get_string(App()->GlobalConfig(), "BasicWindow", "DockState");
			QByteArray dockState = QByteArray::fromBase64(QByteArray(dockStateStr));
			basic->restoreState(dockState);
		} else {
			basic->GetYouTubeAppDock()->SettingsUpdated(false);
		}
	}

	return channelWid;
}

bool PLSChannelsArea::checkIsEmptyUi()
{
	//empty channels
	if (PLSCHANNELS_API->isEmpty()) {
		ui->AddFrameInvisible->setVisible(false);
		ui->NormalDisFrame->hide();
		ui->FoldDisPlay->hide();
		displayScrollButtons(false);
		if (isScrollButtonsNeeded()) {
			displayScrollButtons(true);
		}
		ui->AddFrame->show();
		return true;
	}

	// no display channels

	if (int visibleWidgets = visibleCount(); isUiInitialized && visibleWidgets < 1) {
		ui->NormalDisFrame->setVisible(false);
		displayScrollButtons(false);
		if (isScrollButtonsNeeded()) {
			displayScrollButtons(true);
		}
		ui->AddFrame->hide();
		ui->FoldDisPlay->hide();
		ui->AddFrameInvisible->setVisible(true);
		return false;
	}

	//normal
	ui->AddFrame->hide();
	ui->AddFrameInvisible->setVisible(false);
	if (m_bFold) {
		ui->NormalDisFrame->hide();
		ui->FoldDisPlay->show();
	} else {
		ui->FoldDisPlay->hide();
		ui->NormalDisFrame->show();
	}

	displayScrollButtons(false);
	if (isScrollButtonsNeeded()) {
		displayScrollButtons(true);
	}

	return false;
}

void PLSChannelsArea::updateUi()
{
	int state = PLSCHANNELS_API->currentBroadcastState();

	switch (state) {
	case ReadyState:
		toReadyState();
		break;
	case BroadcastGo:
		myChannelsIconBtn->setEnabled(false);
		ui->MidFrame->setEnabled(false);
		break;
	case StreamStarted:
		myChannelsIconBtn->setEnabled(false);
		delayUpdateAllChannelsUi();
		ui->MidFrame->setEnabled(true);
		break;
	case StopBroadcastGo:
		ui->MidFrame->setEnabled(false);
		break;
	default:
		break;
	}
}

void PLSChannelsArea::toReadyState()
{
	//hold
	static QVariant locker;
	if (isHolding) {
		myChannelsIconBtn->setEnabled(false);
		ui->MidFrame->setEnabled(false);
		ui->TailFrame->setEnabled(false);
		//locker.setValue(HotKeyLocker::createHotkeyLocker());
		return;
	}
	//unhold

	delayUpdateAllChannelsUi();
	ui->MidFrame->setEnabled(true);
	myChannelsIconBtn->setEnabled(true);
	ui->TailFrame->setEnabled(true);
	locker.clear();
}

void PLSChannelsArea::delayUpdateUi()
{
	delayTask(&PLSChannelsArea::updateUi);
}

void PLSChannelsArea::updateAllChannelsUi()
{
	if (m_bFold) {
		auto check = [&](ChannelFoldCapsulePtr wid) { wid->updateUi(); };
		std::for_each(m_FoldChannelsWidget.begin(), m_FoldChannelsWidget.end(), check);
		return;
	}

	bool isLiving = PLSCHANNELS_API->isLiving();
	auto check = [&](ChannelCapsulePtr wid) {
		if ((!wid->isSelectedDisplay()) || (isLiving && !wid->isOnLine())) {
			wid->hide();
		} else {
			wid->show();
			wid->updateUi();
		}
	};
	std::for_each(mChannelsWidget.begin(), mChannelsWidget.end(), check);
}

void PLSChannelsArea::delayUpdateAllChannelsUi()
{
	static QTimer *delayTimer = nullptr;
	if (delayTimer == nullptr) {
		delayTimer = new QTimer(this);
		delayTimer->setSingleShot(true);
		connect(delayTimer, &QTimer::timeout, this, [this]() {
			updateAllChannelsUi();
			checkIsEmptyUi();
			checkScrollButtonsState(isUiInitialized ? ScrollDirection::NOScroll : ScrollDirection::ForwardScroll);
		});
	}
	delayTimer->start(200);
}

void PLSChannelsArea::hideLoading()
{
	mbusyFrame->setVisible(false);
	mbusyFrame->stop();
}

void PLSChannelsArea::updateAllChannelsByDualOutput(bool bOpen)
{
	if (bOpen)
		PLSCHANNELS_API->setChannelDefaultOutputDirection();
	auto check = [bOpen](ChannelCapsulePtr wid) { wid->updateUi(true); };
	std::for_each(mChannelsWidget.begin(), mChannelsWidget.end(), check);
}

void PLSChannelsArea::removeChannel(const QString &channelUUID)
{
	auto ite = mChannelsWidget.find(channelUUID);
	if (ite != mChannelsWidget.end()) {
		auto wid = ite.value();
		bool bYoutube = wid->isYoutube();
		wid->hide();
		mChannelsWidget.erase(ite);
		delayTask(&PLSChannelsArea::refreshOrder);

		if (cef_js_avail && bYoutube) {
			auto basic = PLSBasic::instance();
			if (!basic->GetYouTubeAppDock()) {
				basic->NewYouTubeAppDock();
			}
			basic->GetYouTubeAppDock()->AccountDisconnected();
			basic->GetYouTubeAppDock()->Update();
			basic->DeleteYouTubeAppDock();
		}
	}

	auto it = m_FoldChannelsWidget.find(channelUUID);
	if (it != m_FoldChannelsWidget.end()) {
		auto wid = it.value();
		wid->hide();
		m_FoldChannelsWidget.erase(it);
	}
}

void PLSChannelsArea::updateChannelUi(const QString &channelUUID)
{
	if (PLSCHANNELS_API->isLiving()) {
		delayUpdateAllChannelsUi();
		return;
	}

	auto channelWid = mChannelsWidget.value(channelUUID);
	if (channelWid == nullptr) {
		return;
	}
	if (channelWid->isSelectedDisplay()) {
		channelWid->setVisible(true);
		channelWid->updateUi();
		return;
	}

	channelWid->setVisible(false);
}

void PLSChannelsArea::refreshChannels()
{
	PRE_LOG_UI_MSG_STRING("Refresh [menu] ", "Clicked")

	auto matchedPlaftorms = PLSCHANNELS_API->getAllChannelInfo();

	auto isMatched = [&](const QVariantMap &info) {
		auto platform = getInfo(info, g_fixPlatformName);
		return g_platformsToClearData.contains(platform, Qt::CaseInsensitive) && getInfo(info, g_data_type, NoType) == ChannelType;
	};

	if (std::find_if(matchedPlaftorms.constBegin(), matchedPlaftorms.constEnd(), isMatched) != matchedPlaftorms.constEnd()) {

		auto ret = PLSAlertView::question(this, tr("Alert.Title"), tr("RefreshChannel.DeleteLiveInfo.Alert.Message"),
						  {{PLSAlertView::Button::Yes, tr("RefreshChannel.DeleteLiveInfo.Alert.Refresh")}, {PLSAlertView::Button::Cancel, QObject::tr("Cancel")}},
						  PLSAlertView::Button::Cancel);
		if (ret != PLSAlertView::Button::Yes) {
			return;
		}
	}
	PLSCHANNELS_API->setResetNeed(true);
	PLSCHANNELS_API->sigRefreshAllChannels();
}

void PLSChannelsArea::showChannelsAdd()
{
	PRE_LOG_UI_MSG_STRING("add channels [Menu]", "Clicked")
	auto channelsAdd = new ChannelsAddWin(this);
	channelsAdd->setAttribute(Qt::WA_DeleteOnClose);
	channelsAdd->show();
}

void PLSChannelsArea::clearAll()
{
	m_FoldChannelsWidget.clear();
}

void PLSChannelsArea::clearAllRTMP()
{
	auto ite = mChannelsWidget.begin();
	while (ite != mChannelsWidget.end()) {
		if (!PLSCHANNELS_API->isChannelInfoExists(ite.key())) {
			ite = mChannelsWidget.erase(ite);
		} else {
			++ite;
		}
	}

	auto it = m_FoldChannelsWidget.begin();
	while (it != m_FoldChannelsWidget.end()) {
		if (!PLSCHANNELS_API->isChannelInfoExists(it.key())) {
			it = m_FoldChannelsWidget.erase(it);
		} else {
			++it;
		}
	}
}

void PLSChannelsArea::changeEvent(QEvent *e)
{
	QFrame::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	case QEvent::Show:
		break;
	default:
		break;
	}
}

void PLSChannelsArea::wheelEvent(QWheelEvent *event)
{
	QPoint numDegrees = event->angleDelta() / 50;
	if (numDegrees.y() == 0) {
		return;
	}
	scrollNext(numDegrees.y() > 0);
	event->accept();
}

bool PLSChannelsArea::eventFilter(QObject *watched, QEvent *event)
{
	switch (event->type()) {
	case QEvent::Resize:
		if (watched == ui->MidFrame || watched == mbusyFrame) {
			auto sizeToPoint = [](const QSize &size) { return QPoint(size.width(), size.height()); };
			mbusyFrame->move(sizeToPoint((ui->MidFrame->size() - mbusyFrame->size()) / 2));
			if (watched == ui->MidFrame && isUiInitialized) {
				checkScrollButtonsState(ScrollDirection::ForwardScroll);
			}
			return true;
		}
		break;
	case QEvent::Show:
		if (isUiInitialized) {
			checkScrollButtonsState(ScrollDirection::ForwardScroll);
		}
		break;
	default:

		break;
	}
	return false;
}

void PLSChannelsArea::showEvent(QShowEvent *event)
{
	QFrame::showEvent(event);
	QTimer::singleShot(200, this, [this]() {
		PLS_INFO("PLSChannelsArea", "singleShot showEvent");
		ui->scrollAreaWidgetContents->adjustSize();
	});
}

bool PLSChannelsArea::isScrollButtonsNeeded() const
{
	ui->scrollAreaWidgetContents->adjustSize();
	auto recView = ui->ChannelScrollArea->contentsRect();
	auto scrollGeo = ui->scrollAreaWidgetContents->contentsRect();
	return recView.width() < scrollGeo.width();
}

void PLSChannelsArea::initializeMychannels()
{
	myChannelsIconBtn = new QToolButton();
	myChannelsIconBtn->setObjectName("MyChannelsIconBtn");
	myChannelsTxtBtn = new QPushButton(CHANNELS_TR(MyChannel));
	myChannelsTxtBtn->setObjectName("MyChannelsTxtBtn");
	myChannelsTxtBtn->setProperty("notShowHandCursor", true);
	myChannelsTxtBtn->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
	ui->HeaderLayout->addWidget(myChannelsTxtBtn, 25);
	ui->HeaderLayout->addWidget(myChannelsIconBtn, 1, Qt::AlignLeft | Qt::AlignHCenter);

	auto menu = new QMenu(myChannelsIconBtn);
	menu->setWindowFlag(Qt::NoDropShadowWindowHint);
	menu->setObjectName("MyChannelsMenu");
	auto settingAction = menu->addAction(CHANNELS_TR(SettingChannels), this, []() {
		PRE_LOG_UI_MSG_STRING("My Channels Setting [menu] ", "Clicked")
		showChannelsSetting();
	});
	menu->addAction(CHANNELS_TR(AddChannels), this, &PLSChannelsArea::showChannelsAdd);
	menu->addAction(CHANNELS_TR(RefreshChannels), this, &PLSChannelsArea::refreshChannels);

	connect(
		myChannelsIconBtn, &QToolButton::clicked, this,
		[this, settingAction, menu]() {
			if (pls_is_app_exiting()) { //issue 3989
				return;
			}
			settingAction->setDisabled(PLSCHANNELS_API->isEmpty());
			auto pos = QPoint(myChannelsIconBtn->width() / 2, myChannelsIconBtn->height());
			menu->exec(myChannelsIconBtn->mapToGlobal(pos));
		},
		Qt::QueuedConnection);
}

int PLSChannelsArea::visibleCount()
{
	auto isWidVisible = [](ChannelCapsulePtr wid) { return wid->isSelectedDisplay(); };
	return int(std::count_if(mChannelsWidget.begin(), mChannelsWidget.end(), isWidVisible));
}

void PLSChannelsArea::onFoldUpButtonClick()
{
	m_bFold = true;
	m_FoldUpButton->setVisible(false);
	m_FoldDownButton->setEnabled(true);
	m_FoldDownButton->setVisible(true);
	ui->HeaderFrame->hide();
	ui->NormalDisFrame->hide();
	ui->AddFrame->hide();
	ui->AddFrameInvisible->hide();

	updateAllChannelsUi();
	ui->FoldDisPlay->show();

	int showNumber = 0;
	int rtmpNumber = 0;
	int bandNumber = 0;
	auto it = m_FoldChannelsWidget.begin();
	while (it != m_FoldChannelsWidget.end()) {

		auto channelInfo = PLSCHANNELS_API->getChannelInfo(it.key());
		int dataType = getInfo(channelInfo, g_data_type, NoType);
		int userState = getInfo(channelInfo, g_channelUserStatus, NotExist);
		QString platformName = getInfo(channelInfo, g_channelName, QString());

		if (userState != Enabled) {
			it.value()->hide();
			++it;
			continue;
		}
		if (dataType >= CustomType) {
			rtmpNumber++;
			it.value()->hide();
			++it;
			continue;
		}
		if (platformName == BAND) {
			bandNumber++;
			it.value()->hide();
			++it;
			continue;
		}
		it.value()->show();
		showNumber++;
		++it;
	}

	if (showNumber == 0) {
		ui->FoldCapslesFrame->hide();
		if (rtmpNumber > 0) {
			ui->ErrorText->setText(tr("Channels.dashbrod.viewerCount.nosupport"));
		} else {
			if (bandNumber > 0) {
				ui->ErrorText->setText(tr("Channels.dashbrod.viewerCount.nosupport"));
			} else {
				ui->ErrorText->setText(tr("Channels.dashbrod.noconnect"));
			}
		}
		ui->ErrorText->show();
	} else {
		ui->FoldCapslesFrame->show();
		ui->ErrorText->hide();
	}
}
void PLSChannelsArea::onFoldDownButtonClick()
{
	m_bFold = false;
	m_FoldDownButton->setVisible(false);
	m_FoldUpButton->setEnabled(true);
	m_FoldUpButton->setVisible(true);
	ui->HeaderFrame->show();
	updateAllChannelsUi();
	checkIsEmptyUi();
}

void PLSChannelsArea::createFoldButton()
{

	m_FoldUpButton = pls_new<QPushButton>(ui->TailFrame);
	m_FoldUpButton->setObjectName("FoldUpButton");
	ui->TailLayout->insertWidget(0, m_FoldUpButton);
	connect(m_FoldUpButton, &QPushButton::clicked, this, &PLSChannelsArea::onFoldUpButtonClick);
	connect(
		m_FoldUpButton, &QPushButton::clicked, PLSMainView::instance(), []() { PLSMainView::instance()->setChannelsAreaHeight(30); }, Qt::QueuedConnection);
	m_FoldUpButton->setEnabled(true);
	m_FoldUpButton->setVisible(true);
	m_FoldUpButton->setToolTip(tr("Channels.dashbrod.tooltip"));

	m_FoldDownButton = pls_new<QPushButton>(ui->TailFrame);
	m_FoldDownButton->setObjectName("FoldDownButton");
	ui->TailLayout->insertWidget(1, m_FoldDownButton);
	connect(m_FoldDownButton, &QPushButton::clicked, this, &PLSChannelsArea::onFoldDownButtonClick);
	connect(
		m_FoldDownButton, &QPushButton::clicked, PLSMainView::instance(),
		[this]() {
			PLSMainView::instance()->setChannelsAreaHeight(60);

			repaint();
			pls_async_call(this, [this] { repaint(); });
		},
		Qt::QueuedConnection);
	m_FoldDownButton->setEnabled(false);
	m_FoldDownButton->setVisible(false);
}

void PLSChannelsArea::removeChannelWithoutYoutubeDock(const QString &channelUUID)
{
	auto ite = mChannelsWidget.find(channelUUID);
	if (ite != mChannelsWidget.end()) {
		auto wid = ite.value();
		wid->hide();
		mChannelsWidget.erase(ite);
		delayTask(&PLSChannelsArea::refreshOrder);
	}

	auto it = m_FoldChannelsWidget.find(channelUUID);
	if (it != m_FoldChannelsWidget.end()) {
		auto wid = it.value();
		wid->hide();
		m_FoldChannelsWidget.erase(it);
	}
}
