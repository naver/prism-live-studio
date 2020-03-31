#include "ChannelCapsule.h"
#include "ui_ChannelCapsule.h"
#include "ChannelCommonFunctions.h"
#include "ChannelConst.h"
#include "frontend-api.h"
#include "LogPredefine.h"
#include "ChannelConfigPannel.h"
#include "PLSChannelsVirualAPI.h"
#include <QTimer>
#include <QLabel>
#include <QToolTip>
#include <QHelpEvent>
#include "pls-app.hpp"

using namespace ChannelData;
constexpr int MaxNameLabelWidth = 86;
constexpr int MaxChannelNameLabelWidth = 130;

ChannelCapsule::ChannelCapsule(QWidget *parent) : QFrame(parent), ui(new Ui::ChannelCapsule), mBusyCount(0)
{
	ui->setupUi(this);
	this->setAttribute(Qt::WA_Hover, true);
	this->installEventFilter(this);

	mConfigPannel = new ChannelConfigPannel(this);
	mConfigPannel->setAttribute(Qt::WA_Hover, true);
	mConfigPannel->setWindowFlags(Qt::SubWindow | Qt::NoDropShadowWindowHint | Qt::FramelessWindowHint);
	mConfigPannel->installEventFilter(this);
	mConfigPannel->hide();

	mLoadingFrame = new QLabel(ui->UserIcon);
	mLoadingFrame->setObjectName("busyFrame");
	mLoadingFrame->setAlignment(Qt::AlignCenter);
	mLoadingFrame->hide();

	connect(&mUpdateTimer, &QTimer::timeout, this, &ChannelCapsule::updateInfo);
	connect(&mbusyTimer, &QTimer::timeout, this, &ChannelCapsule::nextPixmap);
	connect(&mConfigTimer, &QTimer::timeout, this, &ChannelCapsule::checkConfigVisible);
	PRE_LOG(new channel capsule, INFO);
}

void ChannelCapsule::normalState(const QVariantMap &info)
{

	int dataType = getInfo(info, g_data_type, NoType);
	switch (dataType) {
	case ChannelType:
		channelTypeState(info);
		break;
	case RTMPType:
		RTMPTypeState(info);
		break;
	default:
		break;
	}
}

void ChannelCapsule::shiftState(const QVariantMap &info)
{
	int dataState = getInfo(info, g_channelStatus, Error);

	switch (dataState) {
	case Error:
		errorState(info);
		break;
	case UnInitialized:
		unInitializeState(info);
		break;
	case Waiting:
		waitingState(info);
		break;
	case Valid:
	case Expired:
		normalState(info);
		break;
	default:
		errorState(info);
		break;
	}
}

void ChannelCapsule::RTMPTypeState(const QVariantMap &info)
{
	bool isLiving = PLSCHANNELS_API->isLiving();
	int userState = getInfo(info, g_channelUserStatus, NotExist);

	bool isEnabled = userState == Enabled;
	bool isAct = (isLiving && isEnabled);

	ui->HeaderPart->setEnabled(isEnabled);
	ui->InfoPart->setEnabled(isEnabled);

	ui->UserIcon->setActive(isAct);
	switchUpdateInfo(false);
	ui->NameLineFrame->setVisible(true);
	ui->ChannelName->hide();
	ui->StateActive->setVisible(!isLiving);
	ui->StateActive->setChecked(isEnabled);
	ui->StateActive->setDisabled(true);
	ui->ChannelInfo->hide();

	ui->ErrorLabel->hide();
	this->setEnabled(true);
}

void ChannelCapsule::channelTypeState(const QVariantMap &info)
{

	bool isLiving = PLSCHANNELS_API->isLiving();
	int userState = getInfo(info, g_channelUserStatus, NotExist);

	bool isEnabled = userState == Enabled;
	bool isAct = (isLiving && isEnabled);

	ui->HeaderPart->setEnabled(isEnabled);
	ui->InfoPart->setEnabled(isEnabled);

	ui->UserIcon->setActive(isAct);
	switchUpdateInfo(isAct);

	ui->NameLineFrame->setVisible(true);
	ui->ChannelName->setVisible(!isLiving && !ui->ChannelName->text().isEmpty());
	ui->StateActive->setVisible(!isLiving);
	ui->StateActive->setChecked(isEnabled);
	ui->StateActive->setDisabled(true);
	ui->ChannelInfo->setVisible(isLiving);

	ui->ErrorLabel->hide();
}

void ChannelCapsule::errorState(const QVariantMap &info)
{
	ui->HeaderPart->setEnabled(false);
	ui->InfoPart->setEnabled(false);

	int dataType = getInfo(info, g_data_type, NoType);
	bool isLiving = PLSCHANNELS_API->isLiving();
	int userState = getInfo(info, g_channelUserStatus, NotExist);

	ui->UserIcon->setActive(false);
	switchUpdateInfo(false);
	ui->NameLineFrame->setVisible(true);
	ui->StateActive->setVisible(false);
	ui->StateActive->setChecked(userState == Enabled);
	ui->StateActive->setDisabled(true);

	if (dataType == RTMPType) {
		ui->ChannelName->setVisible(false);
		ui->ChannelInfo->setVisible(false);
	} else {
		ui->ChannelName->setVisible(!isLiving);
		ui->ChannelInfo->setVisible(isLiving);
	}
	ui->ErrorLabel->setVisible(false);
}

void ChannelCapsule::unInitializeState(const QVariantMap &info)
{
	waitingState(info);
}

void ChannelCapsule::waitingState(const QVariantMap &info)
{
	ui->HeaderPart->setEnabled(false);
	ui->InfoPart->setEnabled(false);

	ui->ErrorLabel->setVisible(true);

	ui->NameLineFrame->setVisible(false);
	ui->ChannelName->setVisible(false);
	ui->UserIcon->setActive(false);
	switchUpdateInfo(false);
	ui->ChannelInfo->setVisible(false);

	ui->StateActive->setVisible(false);
	ui->StateActive->setChecked(false);
	ui->StateActive->setDisabled(true);

	QString channelName = getInfo(info, g_channelName);
	QString defaultIcon = getPlatformImageFromName(channelName);

	ui->UserIcon->setPixmap(defaultIcon);
	ui->UserIcon->setPlatformPixmap("");
}

void ChannelCapsule::nextPixmap()
{
	QCoreApplication::processEvents();
	++mBusyCount;
	if (mBusyCount >= 9) {
		mBusyCount = 1;
	}
	mLoadingFrame->setPixmap(g_loadingPixPath.arg(mBusyCount));
}

bool ChannelCapsule::isPannelOutOfView()
{
	auto parentWid = this->parentWidget()->parentWidget()->parentWidget()->parentWidget();
	auto parentGeometry = parentWid->contentsRect();
	auto center = this->contentsRect().center();
	center = this->mapTo(parentWid, center);

	auto right = center + QPoint(this->width() / 2 - 20, 0);
	auto left = center - QPoint(this->width() / 2 - 20, 0);
	return (!parentGeometry.contains(center) || !parentGeometry.contains(left) || !parentGeometry.contains(right));
}

ChannelCapsule::~ChannelCapsule()
{
	mConfigPannel->deleteLater();
	delete ui;
}

void ChannelCapsule::initialize(const QVariantMap &srcData)
{
	mInfoID = getInfo(srcData, g_channelUUID, QString());
	mConfigPannel->setChannelID(mInfoID);
	this->updateUi();
}

QString &translateForYoutube(QString &src)
{
	QStringList trans{QObject::tr("youtube.privacy.public"), QObject::tr("youtube.privacy.unlisted"), QObject::tr("youtube.privacy.private")};
	QStringList indexs{("youtube.privacy.public"), ("youtube.privacy.unlisted"), ("youtube.privacy.private")};
	QRegularExpression rule(".+" + src);
	rule.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
	int index = indexs.indexOf(rule);
	if (index != -1) {
		src = trans[index];
	}
	return src;
}

void ChannelCapsule::updateUi()
{
	const auto &srcData = PLSCHANNELS_API->getChanelInfoRef(mInfoID);
	if (srcData.isEmpty()) {
		return;
	}

	QString userName = getInfo(srcData, g_nickName, QString("empty"));
	QString catogry;
	QString catogryTemp = getInfo(srcData, g_catogryTemp, QString());
	if (!catogryTemp.isEmpty()) {
		catogry = catogryTemp;
	} else {
		catogry = getInfo(srcData, g_catogry, QString());
	}

	QString platformName = getInfo(srcData, g_channelName, QString());
	if (platformName.contains(YOUTUBE) && !catogry.isEmpty()) {
		translateForYoutube(catogry);
	}

	ui->UserName->setText(getElidedText(ui->UserName, userName, MaxNameLabelWidth));
	ui->ChannelName->setText(getElidedText(ui->ChannelName, catogry, MaxChannelNameLabelWidth));

	auto userIcon = g_defualtPlatformIcon;
	auto platformIcon = g_defualtPlatformSmallIcon;
	bool needSharp = false;

	int type = getInfo(srcData, g_data_type, NoType);
	if (type == ChannelType) {
		needSharp = true;
		updateInfo();
	}

	getComplexImageOfChannel(mInfoID, userIcon, platformIcon);

	ui->UserIcon->setPixmap(userIcon, needSharp);
	ui->UserIcon->setPlatformPixmap(platformIcon, false);
	int state = getInfo(srcData, g_channelStatus, Error);
	if (state != Valid) {
		QString errorStr = getInfo(srcData, g_errorString);
		ui->ErrorLabel->setText(errorStr);
		ui->ErrorLabel->setToolTip(errorStr);
	}

	shiftState(srcData);
	mConfigPannel->updateUI();
}

QString &formatNumber(QString &number, bool isEng = true)
{
	number = number.simplified();
	if (number.isEmpty()) {
		number = "0";
		return number;
	}
	int N = number.count();
	if (N <= 3) {
		return number;
	}
	if (N <= 6) {
		return number.insert(N - 3, ",");
	}

	QString head;
	QString suffix;

	switch (N) {
	case 7: {
		if (isEng) {
			head = number.chopped(5);
			head.insert(N - 6, '.');
			suffix = CHANNELS_TR(Mill);
		} else {
			head = number.chopped(4);
			suffix = CHANNELS_TR(Wan);
		}
	} break;
	case 8: {
		if (isEng) {
			head = number.chopped(5);
			head.insert(N - 6, '.');
			suffix = CHANNELS_TR(Mill);
		} else {
			head = number.chopped(4);
			head.insert(1, ',');
			suffix = CHANNELS_TR(Wan);
		}

	} break;
	case 9: {
		if (isEng) {
			head = number.chopped(5);
			head.insert(N - 6, '.');
			suffix = CHANNELS_TR(Mill);
		} else {
			head = number.chopped(7);
			head.insert(1, '.');
			suffix = CHANNELS_TR(Yi);
		}

	} break;
	case 10: {
		if (isEng) {
			head = number.chopped(8);
			head.insert(N - 9, '.');
			suffix = CHANNELS_TR(Bill);
		} else {
			head = number.chopped(8);
			suffix = CHANNELS_TR(Yi);
		}

	} break;

	default: {
		if (isEng) {
			head = number.chopped(8);
			head.insert(N - 9, '.');
			suffix = CHANNELS_TR(Bill);
		} else {
			head = number.chopped(8);
			suffix = CHANNELS_TR(Yi);
		}

	} break;
	}
	if (head.endsWith(".0")) {
		head.remove(".0");
	}
	number = head + suffix;
	return number;
}

void ChannelCapsule::updateInfo()
{
	bool isEng = false;
	auto lang = QString(App()->GetLocale());
	if (lang.contains("en", Qt::CaseInsensitive)) {
		isEng = true;
	}
	auto info = PLSCHANNELS_API->getChannelInfo(mInfoID);
	if (info.contains(g_viewers)) {
		auto viewers = getInfo(info, g_viewers, QString("0"));
		formatNumber(viewers, isEng);
		ui->ViewerNumberLabel->setText(viewers);
		ui->ViewerFrame->setVisible(true);
	} else {
		ui->ViewerFrame->setVisible(false);
	}

	if (info.contains(g_likes)) {
		auto likes = getInfo(info, g_likes, QString("0"));
		formatNumber(likes, isEng);
		ui->LikeNumberLabel->setText(likes);
		ui->LikeInfo->setVisible(true);
	} else {
		ui->LikeInfo->setVisible(false);
	}
}
void ChannelCapsule::changeEvent(QEvent *e)
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

bool ChannelCapsule::eventFilter(QObject *watched, QEvent *event)
{
	switch (event->type()) {
	case QEvent::Enter: {
		//PRE_LOG("hover enter");
		if (!PLSCHANNELS_API->isEmptyToAcquire() || watched == mConfigPannel || isPannelOutOfView() || mConfigPannel->isVisible() || PLSCHANNELS_API->isShifting()) {
			return false;
		}
		auto enter = dynamic_cast<QEnterEvent *>(event);
		auto contentRect = this->contentsRect();
		auto pos = enter->pos();
		if (contentRect.contains(pos)) {
			showConfigPannel();
			return true;
		}

	} break;
	case QEvent::HoverLeave:
		if (mConfigPannel == watched) {
			hideConfigPannel();
			return true;
		}
		auto hLeav = dynamic_cast<QHoverEvent *>(event);
		if (hLeav) {
			auto pos = hLeav->pos();
			auto contentRec = this->contentsRect().adjusted(-1, -1, 1, 1);
			if (!contentRec.contains(pos)) {
				hideConfigPannel();
				return true;
			}
		}

		break;
	} //end switch

	return false;
}

void ChannelCapsule::showConfigPannel()
{
	mConfigPannel->show();
	mConfigTimer.start(500);
}

void ChannelCapsule::hideConfigPannel()
{
	mConfigPannel->hide();
	mConfigTimer.stop();
}

void ChannelCapsule::checkConfigVisible()
{
	if (mConfigPannel->isVisible() && !mConfigPannel->signalsBlocked()) {
		auto pos = QCursor::pos();
		auto thisPos = this->mapFromGlobal(pos);
		auto thisCont = this->contentsRect();
		if (!thisCont.contains(thisPos)) {
			hideConfigPannel();
		}
	}
}

void ChannelCapsule::switchUpdateInfo(bool on)
{
	if (on) {
		updateInfo();
		mUpdateTimer.start(5000);
	} else {
		mUpdateTimer.stop();
	}
}

bool ChannelCapsule::isActive()
{
	return ui->StateActive->isChecked();
}

void ChannelCapsule::holdOn(bool hold)
{
	this->setDisabled(hold);
}
