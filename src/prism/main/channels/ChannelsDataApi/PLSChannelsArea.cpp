#include "PLSChannelsArea.h"
#include "ui_ChannelsArea.h"
#include "PLSChannelDataAPI.h"
#include "ChannelCapsule.h"
#include "ChannelConst.h"
#include "ChannelDefines.h"
#include "ChannelCommonFunctions.h"
#include <QWheelEvent>
#include <QScrollBar>
#include "GoLivePannel.h"
#include <QPushButton>
#include "ChannelsAddWin.h"
#include "DefaultPlatformsAddList.h"
#include "PLSChannelsVirualAPI.h"
#include "PLSAddingFrame.h"
#include "LogPredefine.h"
#include <QTime>
#include <QGuiApplication>
#include <QWindow>
using namespace ChannelData;

PLSChannelsArea::PLSChannelsArea(QWidget *parent) : QFrame(parent), ui(new Ui::ChannelsArea), isHolding(false)
{
	ui->setupUi(this);
	auto refreshBtn = new QPushButton();
	refreshBtn->setObjectName("RefreshButton");
	auto refreshTxtBtn = new QPushButton(CHANNELS_TR(MyChannel));
	refreshTxtBtn->setObjectName("RefreshTxtButton");
	ui->HeaderLayout->addWidget(refreshTxtBtn, 5);
	ui->HeaderLayout->addWidget(refreshBtn, 1);
	ui->HeaderLayout->addStretch(4);

	connect(refreshBtn, &QPushButton::clicked, this, &PLSChannelsArea::refreshChannels, Qt::QueuedConnection);
	connect(refreshTxtBtn, &QPushButton::clicked, this, &PLSChannelsArea::refreshChannels, Qt::QueuedConnection);

	addBtn = new QPushButton(CHANNELS_TR(Add));
	addBtn->setObjectName("ChannelsAdd");
	addBtn->setToolTip(CHANNELS_TR(AddTip));
	this->appendTailWidget(addBtn);
	connect(addBtn, &QPushButton::clicked, this, &PLSChannelsArea::showChannelsAdd, Qt::QueuedConnection);

	auto defaultAddWid = new DefaultPlatformsAddList;
	ui->AddFrame->layout()->addWidget(defaultAddWid);

	goliveWid = new GoLivePannel;
	ui->TailFrame->layout()->addWidget(goliveWid);

	initScollButtons();

	ui->MidFrame->setAttribute(Qt::WA_Hover);
	ui->MidFrame->installEventFilter(this);

	mbusyFrame = new PLSAddingFrame(ui->MidFrame);
	mbusyFrame->setObjectName("LoadingFrame");
	mbusyFrame->setContent(CHANNELS_TR(Loading));
	mbusyFrame->setSourceFirstFile(g_loadingPixPath);
	mbusyFrame->setWindowFlags(Qt::SubWindow | Qt::FramelessWindowHint);
	mbusyFrame->setWindowModality(Qt::NonModal);
	mbusyFrame->hide();

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelAdded, this, QOverload<const QString &>::of(&PLSChannelsArea::addChannel), Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelRemoved, this, &PLSChannelsArea::removeChannel, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::channelModified, this, &PLSChannelsArea::updateChannelUi, Qt::QueuedConnection);
	//connect(PLSCHANNELS_API, &PLSChannelDataAPI::prismTokenExpired, this, &PLSChannelsArea::clearAllRTMP, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::holdOnChannel, this, &PLSChannelsArea::holdOnChannel);
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::holdOnChannelArea, this,
		[=](bool isHold) {
			if (isHold) {
				mbusyFrame->setContent("");
				mbusyFrame->resize(54, 54);
			}

			holdOnChannelArea(isHold);
		},
		Qt::QueuedConnection);
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::addingHold, this,
		[=](bool isHold) {
			if (isHold) {
				mbusyFrame->setContent(CHANNELS_TR(Adding));
				mbusyFrame->resize(200, 54);
			}

			holdOnChannelArea(isHold);
		},
		Qt::QueuedConnection);

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::holdOnGolive, goliveWid, &GoLivePannel::holdOnAll, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::sigAllClear, this, &PLSChannelsArea::clearAll, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::liveStateChanged, this, &PLSChannelsArea::updateUi, Qt::QueuedConnection);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toDoinitialize, this, &PLSChannelsArea::initChannels, Qt::QueuedConnection);
	connect(
		PLSCHANNELS_API, &PLSChannelDataAPI::networkInvalidOcurred, this, []() { showNetworkErrorAlert(); }, Qt::QueuedConnection);

	updateUi();
}

PLSChannelsArea::~PLSChannelsArea()
{
	mChannelsWidget.clear();
	delete ui;
}

void PLSChannelsArea::initChannels()
{
	//qDebug() << " time initialize :" << QTime::currentTime();
	PRE_LOG_UI(InitChannels, PLSChannelsArea);
	HolderReleaser releaser(&PLSChannelsArea::holdOnChannelArea, this);
	auto allChannels = PLSCHANNELS_API->getCurrentSortedChannelsUUID();
	if (!allChannels.isEmpty()) {
		SemaphoreHolder holder(PLSCHANNELS_API->getSourceSemaphore());
		for (auto &info : allChannels) {
			QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			addChannel(info);
		}
	}

	//qDebug() << " time initialize :" << QTime::currentTime();
}

void PLSChannelsArea::appendTailWidget(QWidget *widget)
{
	ui->ScrollTailFrameLayout->insertWidget(-1, widget);
	ui->NormalDisFrameLayout->addStretch(10);
}

void PLSChannelsArea::holdOnChannelArea(bool holdOn)
{
	//qDebug() << "----------------hold " << holdOn;
	if (holdOn) {
		isHolding = true;
		if (!mbusyFrame->isVisible()) {
			auto posParentC = ui->MidFrame->contentsRect().center();
			auto posMC = mbusyFrame->contentsRect().center();
			auto posDiffer = posParentC - posMC;
			mbusyFrame->move(posDiffer);
			mbusyFrame->start(500);
			mCheckTimer.start(10000);
			updateUi();
		}

	} else {
		isHolding = false;
		if (!PLSCHANNELS_API->isEmptyToAcquire()) {
			return;
		}
		updateUi();
		mCheckTimer.stop();
	}
}

int PLSChannelsArea::getRTMPInsertIndex()
{
	auto scrollLay = ui->CapusuleLayout;
	int count = scrollLay->count();
	int ret = -1;
	for (int i = 0; i < count; ++i) {
		auto widget = dynamic_cast<ChannelCapsule *>(scrollLay->itemAt(i)->widget());
		if (widget) {
			const auto &uuid = widget->getChannelID();
			auto &info = PLSCHANNELS_API->getChanelInfoRef(uuid);
			int type = getInfo(info, g_data_type, ChannelType);
			if (type == ChannelType) {
				ret = i;
			}
		}
	}
	return ret + 1;
}

int PLSChannelsArea::getChannelInsertIndex(const QString &platformName)
{
	auto defaultPlatforms = getDefaultPlatforms();
	auto ite = defaultPlatforms.begin();

	while (ite != defaultPlatforms.end()) {
		auto name = *ite;
		if (PLSCHANNELS_API->getChanelInfoRefByPlatformName(name, ChannelType).isEmpty()) {
			ite = defaultPlatforms.erase(ite);
		} else {
			++ite;
		}
	}

	return defaultPlatforms.indexOf(platformName);
}

void PLSChannelsArea::refreshOrder()
{
	auto scrollLay = ui->CapusuleLayout;
	int count = scrollLay->count();
	for (int i = 0; i < count; ++i) {
		auto widget = dynamic_cast<ChannelCapsule *>(scrollLay->itemAt(i)->widget());
		if (widget) {
			const auto &uuid = widget->getChannelID();
			PLSCHANNELS_API->setValueOfChannel(uuid, g_displayOrder, i);
		}
	}
}

void PLSChannelsArea::initScollButtons()
{

	mLeftButon = new QPushButton(ui->MidFrame);
	mLeftButon->setWindowFlags(Qt::SubWindow | Qt::NoDropShadowWindowHint | Qt::FramelessWindowHint);
	mLeftButon->setAttribute(Qt::WA_TranslucentBackground);
	mLeftButon->setObjectName("LeftButton");
	mLeftButon->setAutoRepeat(true);
	mLeftButon->setAutoRepeatDelay(500);
	mLeftButon->setAutoRepeatInterval(500);
	mLeftButon->hide();
	connect(mLeftButon, &QPushButton::clicked, [=]() { scrollNext(true); });

	mRightButton = new QPushButton(ui->MidFrame);
	mRightButton->setWindowFlags(Qt::SubWindow | Qt::NoDropShadowWindowHint | Qt::FramelessWindowHint);
	mRightButton->setAttribute(Qt::WA_TranslucentBackground);
	mRightButton->setObjectName("RightButton");
	mRightButton->setAutoRepeat(true);
	mRightButton->setAutoRepeatDelay(500);
	mRightButton->setAutoRepeatInterval(500);
	mRightButton->hide();
	connect(mRightButton, &QPushButton::clicked, [=]() { scrollNext(false); });

	ui->ChannelScrollArea->verticalScrollBar()->setDisabled(true);
}

void PLSChannelsArea::insertChannelCapsule(QWidget *wid, int index)
{
	auto scrollLay = dynamic_cast<QHBoxLayout *>(ui->CapusuleLayout);
	scrollLay->insertWidget(index, wid);
}

void PLSChannelsArea::scrollNext(bool forwartStep)
{
	auto bar = ui->ChannelScrollArea->horizontalScrollBar();
	bar->setValue(bar->value() + (forwartStep ? -1 : 1) * 210);
}

void PLSChannelsArea::displayScrollButtons(bool isShow)
{
	mLeftButon->setVisible(isShow);
	mRightButton->setVisible(isShow);
	mRightButton->move(ui->MidFrame->rect().topRight() - QPoint(mRightButton->width(), 0));
}

void PLSChannelsArea::holdOnChannel(const QString &uuid, bool holdOn)
{
	auto wid = mChannelsWidget.value(uuid);
	if (wid) {
		wid->holdOn(holdOn);
	}
}

void PLSChannelsArea::addChannel(const QString &channelUUID)
{
	auto channelInfo = PLSCHANNELS_API->getChannelInfo(channelUUID);
	if (channelInfo.isEmpty()) {
		return;
	}
	addChannel(channelInfo);
	refreshOrder();
	checkIsEmptyUi();
}

void PLSChannelsArea::addChannel(const QVariantMap &channelInfo)
{
	ChannelCapsulePtr channelWid(new ChannelCapsule, deleteChannelWidget<ChannelCapsule>);
	channelWid->initialize(channelInfo);
	int type = getInfo(channelInfo, g_data_type, ChannelType);
	int order = 0;
	if (type == ChannelType) {

		auto name = getInfo(channelInfo, g_channelName);
		int index = getChannelInsertIndex(name);
		if (index != -1) {
			order = index;
		}

	} else {
		order = getInfo(channelInfo, g_displayOrder, -1);
		if (order == -1) {
			order = getRTMPInsertIndex();
		}
	}
	this->insertChannelCapsule(channelWid.data(), order);
	mChannelsWidget.insert(getInfo(channelInfo, g_channelUUID, QString()), channelWid);
	ui->ChannelScrollArea->ensureWidgetVisible(channelWid.data(), 10, 10);
}

bool PLSChannelsArea::checkIsEmptyUi()
{
	if (PLSCHANNELS_API->isEmpty()) {
		ui->NormalDisFrame->hide();
		ui->AddFrame->show();
		return true;
	}
	ui->AddFrame->hide();
	ui->NormalDisFrame->show();
	return false;
}

void PLSChannelsArea::updateUi()
{
	checkIsEmptyUi();
	int state = PLSCHANNELS_API->currentBroadcastState();

	switch (state) {
	case StreamEnd:
	case ReadyState: {
		if (isHolding) {
			mbusyFrame->setVisible(true);
			ui->HeaderFrame->setEnabled(false);
			ui->MidFrame->setEnabled(false);
			addBtn->setVisible(false);
		} else {
			mbusyFrame->setVisible(false);
			mbusyFrame->stop();
			ui->HeaderFrame->setEnabled(true);
			ui->MidFrame->setEnabled(true);
			addBtn->setVisible(true);
			switchAllChannelsState(false);
		}

	} break;
	case StopBroadcastGo:
	case BroadcastGo:
	case CanBroadcastState:
	case StreamStarting: {
		addBtn->setVisible(false);
		ui->HeaderFrame->setEnabled(false);
		ui->MidFrame->setEnabled(false);
		mbusyFrame->setVisible(false);
		mbusyFrame->stop();
	} break;
	case StreamStarted: {
		addBtn->setVisible(false);
		ui->HeaderFrame->setEnabled(false);
		switchAllChannelsState(true);
		ui->MidFrame->setEnabled(true);
	} break;
	case StreamStopped: {

	} break;
	default:
		break;
	}
}

void PLSChannelsArea::removeChannel(const QString &channelUUID)
{
	mChannelsWidget.remove(channelUUID);
	this->updateUi();
	refreshOrder();
}

void PLSChannelsArea::updateChannelUi(const QString &channelUUID)
{
	if (PLSCHANNELS_API->isLiving()) {
		std::for_each(mChannelsWidget.begin(), mChannelsWidget.end(), [](ChannelCapsulePtr wid) { wid->updateUi(); });
	} else {
		auto channelWid = mChannelsWidget.value(channelUUID);
		if (channelWid) {
			channelWid->updateUi();
		}
	}
}

void PLSChannelsArea::refreshChannels()
{
	PRE_LOG_UI(My channels Clicked, PLSChannelsArea);
	PLSCHANNELS_API->sigRefreshAllChannels();
}

void PLSChannelsArea::showChannelsAdd()
{
	PRE_LOG_UI(show add channels, PLSChannelsArea);
	auto channelsAdd = new ChannelsAddWin(this);
	channelsAdd->setWindowFlags(Qt::Popup);
	channelsAdd->setAttribute(Qt::WA_DeleteOnClose);
	auto newPos = this->mapToGlobal(this->frameGeometry().center()) + QPoint(-channelsAdd->width() / 2, this->height());
	channelsAdd->move(newPos);

	connect(channelsAdd, &ChannelsAddWin::destroyed, this, [=]() {
		addBtn->setChecked(false);
		addBtn->setCheckable(false);
	});
	addBtn->setCheckable(true);
	addBtn->setChecked(true);
	channelsAdd->show();
}

void PLSChannelsArea::switchAllChannelsState(bool on)
{
	auto ite = mChannelsWidget.begin();
	for (; ite != mChannelsWidget.end(); ++ite) {
		auto info = PLSCHANNELS_API->getChannelInfo(ite.key());
		if (info.isEmpty()) {
			continue;
		}
		auto &wid = *ite;
		if (wid->isActive()) {
			wid->setVisible(true);
		} else {
			wid->setVisible(!on);
		}
		wid->updateUi();
	}
}

void PLSChannelsArea::clearAll()
{
	mChannelsWidget.clear();
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
}

void PLSChannelsArea::changeEvent(QEvent *e)
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

void PLSChannelsArea::wheelEvent(QWheelEvent *event)
{
	QPoint numDegrees = event->angleDelta() / 8;
	scrollNext(numDegrees.y() > 0);
	event->accept();
}

bool PLSChannelsArea::eventFilter(QObject *watched, QEvent *event)
{
	Q_UNUSED(watched)
	if (isScrollButtonsNeeded() && (event->type() == QEvent::HoverEnter || event->type() == QEvent::Enter)) {
		displayScrollButtons(true);
		return true;
	}

	if (event->type() == QEvent::HoverLeave) {
		auto pos = ui->MidFrame->mapFromGlobal(QCursor::pos());
		if (!ui->MidFrame->contentsRect().contains(pos)) {
			displayScrollButtons(false);
		}
		return true;
	}

	return false;
}

bool PLSChannelsArea::isScrollButtonsNeeded()
{
	auto recView = ui->ChannelScrollArea->contentsRect();
	auto scrollGeo = ui->scrollAreaWidgetContents->contentsRect();
	return recView.width() < scrollGeo.width();
}
