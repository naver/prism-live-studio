#include "ChannelFoldCapsule.h"
#include <QHelpEvent>
#include <QLabel>
#include <QTimer>
#include <QToolTip>
#include "ChannelCommonFunctions.h"
#include "ChannelConfigPannel.h"
#include "LogPredefine.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelsVirualAPI.h"
#include "frontend-api.h"
#include "libui.h"
#include "pls-channel-const.h"
#include "ui_ChannelFoldCapsule.h"

using namespace ChannelData;
constexpr int MaxNameLabelWidth = 84;
constexpr int MaxChannelNameLabelWidth = 130;
const char CheckedStr[] = "Checked";

ChannelFoldCapsule::ChannelFoldCapsule(QWidget *parent) : QFrame(parent), ui(new Ui::ChannelFoldCapsule)
{
	ui->setupUi(this);
	this->setAttribute(Qt::WA_Hover, true);
	this->installEventFilter(this);
	pls_add_css(this, {"ChannelFoldCapsule"});

	connect(&mUpdateTimer, &QTimer::timeout, this, &ChannelFoldCapsule::updateStatisticInfo);
	connect(qApp, &QCoreApplication::aboutToQuit, &mUpdateTimer, &QTimer::stop);

	delayUpdateTextAndIcons();
}

void ChannelFoldCapsule::normalState(const QVariantMap &info)
{

	int dataType = getInfo(info, g_data_type, NoType);
	if (dataType == ChannelType) {
		channelTypeState(info);
	}
}

void ChannelFoldCapsule::shiftState(const QVariantMap &srcData)
{
	int dataState = getInfo(srcData, g_channelStatus, Error);

	switch (dataState) {
	case Error:
	case UnAuthorized:
		errorState(srcData);
		break;
	case UnInitialized:
		unInitializeState(srcData);
		break;
	case EmptyChannel:
	case WaitingActive:
		waitingState(srcData);
		break;
	case Valid:
	case Expired:
		normalState(srcData);
		break;
	default:
		errorState(srcData);
		break;
	}
}

void ChannelFoldCapsule::channelTypeState(const QVariantMap &info)
{

	bool isLiving = PLSCHANNELS_API->isLiving();
	int userState = getInfo(info, g_channelUserStatus, NotExist);

	bool isEnabled = userState == Enabled;
	bool isAct = (isLiving && isEnabled);

	ui->HeaderPart->setEnabled(isEnabled);
	ui->InfoPart->setEnabled(isEnabled);
	bool isInfoShow = switchUpdateStatisticInfo(isAct);
	ui->ChannelInfo->setVisible(isInfoShow);
	bool hasText = !getInfo(info, g_displayLine2).isEmpty() || !getInfo(info, g_catogry).isEmpty();

	ui->ErrorFrame->hide();
}

void ChannelFoldCapsule::errorState(const QVariantMap &info)
{
	ui->HeaderPart->setEnabled(false);
	ui->InfoPart->setEnabled(false);

	int dataType = getInfo(info, g_data_type, NoType);
	bool isLiving = PLSCHANNELS_API->isLiving();
	int userState = getInfo(info, g_channelUserStatus, NotExist);

	bool isInfoOk = switchUpdateStatisticInfo(false);

	if (dataType >= CustomType) {
		ui->ChannelInfo->setVisible(false);
	} else {
		ui->ChannelInfo->setVisible(isLiving && isInfoOk);
	}
	ui->ErrorFrame->setVisible(false);
}

void ChannelFoldCapsule::unInitializeState(const QVariantMap &info)
{
	waitingState(info);
}

void ChannelFoldCapsule::waitingState(const QVariantMap &info)
{
	ui->HeaderPart->setEnabled(false);
	ui->InfoPart->setEnabled(false);

	ui->UserIcon->setVisible(true);

	updateErrorLabel(info);
	ui->ErrorFrame->setVisible(true);

	switchUpdateStatisticInfo(false);
	ui->ChannelInfo->setVisible(false);
}

ChannelFoldCapsule::~ChannelFoldCapsule()
{
	delete ui;
}

void ChannelFoldCapsule::setChannelID(const QString &uuid)
{
	mInfoID = uuid;
}

void ChannelFoldCapsule::updateUi()
{
	const auto srcData = PLSCHANNELS_API->getChannelInfo(mInfoID);
	if (srcData.isEmpty()) {
		return;
	}

	mLastMap = srcData;
	updateIcons(mLastMap);
	shiftState(mLastMap);
	updateTextFrames(mLastMap);
	delayUpdateTextAndIcons();
}
void ChannelFoldCapsule::updateIcons(const QVariantMap &srcData)
{
	int state = getInfo(srcData, g_channelStatus, Error);
	bool needSharp = false;

	QString userIcon;
	QString platformIcon;
	if (state == Valid || state == Error || state == Expired || state == UnAuthorized) {

		if (int type = getInfo(srcData, g_data_type, NoType); type == ChannelType) {
			needSharp = true;
		}
		getComplexImageOfChannel(mInfoID, ImageType::tagIcon, userIcon, platformIcon);
	} else {
		QString channelName = getInfo(srcData, g_channelName);
		userIcon = getPlatformImageFromName(channelName, ImageType::tagIcon);
	}

	auto pixMap = PLSCHANNELS_API->updateImage(platformIcon, QSize(14, 14) * 4);
	auto size = QSize(14, 14) * devicePixelRatio();
	if (!pixMap.isNull() && pixMap.size() != size) {
		pixMap = pixMap.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		pixMap.setDevicePixelRatio(devicePixelRatio());
	}
	pls_shared_circle_mask_image(pixMap);
	ui->UserIcon->setPixmap(pixMap);
}
void ChannelFoldCapsule::updateErrorLabel(const QVariantMap &info)
{
	QString errorStr = getInfo(info, g_errorString);
	ui->ErrorLabel->setText(errorStr);
	ui->ErrorLabel->setToolTip(errorStr);
}

void ChannelFoldCapsule::updateTextFrames(const QVariantMap &srcData)
{
	QString line1 = getInfo(srcData, g_displayLine1, QString("No Name"));
	line1 = getElidedText(ui->UserName, line1, MaxNameLabelWidth);
	ui->UserName->setText(line1);
}

void ChannelFoldCapsule::delayUpdateTextAndIcons()
{
	if (mLastMap.isEmpty()) {
		return;
	}
	QTimer::singleShot(100, this, [this]() {
		PLS_INFO("ChannelFoldCapsule", "singleShot delayUpdateTextAndIcons");
		updateTextFrames(mLastMap);
		updateIcons(mLastMap);
	});
}

QString ChannelFoldCapsule::getStatisticsImage(const QString &src, bool isEnabled)
{
	if (isEnabled) {
		return src;
	}
	QFileInfo info(src);
	auto disabledPath = ":/images/ChannelsSource/statistics/" + info.baseName() + "-disable." + info.suffix();
	if (QFile::exists(disabledPath)) {
		return disabledPath;
	}

	return src;
}

QString ChannelFoldCapsule::createStatisticsCss(const QString &srcImage)
{

	QString ret = "QLabel{image: url(%1);color:transparent;} QLabel:disabled{image: url(%2);color:transparent;} ";
	QString disbleImage = getStatisticsImage(srcImage, false);
	return ret.arg(srcImage, disbleImage);
}

bool ChannelFoldCapsule::updateStatisticInfo()
{
	auto info = PLSCHANNELS_API->getChannelInfo(mInfoID);
	bool isLiving = PLSCHANNELS_API->isLiving();
	bool bReRehearsal = PLSCHANNELS_API->isRehearsaling();
	QString viewers;

	if (info.contains(g_viewers)) {
		viewers = getInfo(info, g_viewers, QString("0"));
		formatNumber(viewers);
		if (!isLiving) {
			viewers = "0";
		}
	} else {
		viewers = "0";
	}
	ui->ViewerNumberLabel->setText(viewers);
	auto css = createStatisticsCss(getInfo(info, g_viewersPix, g_defaultViewerIcon));
	ui->ViewerPicLabel->setStyleSheet(css);
	ui->ViewerFrame->setVisible(true);

	QString likes;
	if (info.contains(g_likes)) {
		likes = getInfo(info, g_likes, QString("0"));
		formatNumber(likes);
		if (!isLiving) {
			likes = "0";
		}
		ui->LikeNumberLabel->setText(likes);
		auto likeCss = createStatisticsCss(getInfo(info, g_likesPix, g_defaultLikeIcon));
		ui->LikePicLabel->setStyleSheet(likeCss);
		ui->LikeInfo->setVisible(true);
	} else {
		auto platform = getInfo(info, g_channelName);
		if (platform == TWITCH || platform == NAVER_TV || platform == AFREECATV || platform == CHZZK) {
			ui->LikeInfo->setVisible(false);
		} else {
			likes = "0";
			ui->LikeNumberLabel->setText(likes);
			auto likeCss = createStatisticsCss(getInfo(info, g_likesPix, g_defaultLikeIcon));
			ui->LikePicLabel->setStyleSheet(likeCss);
			ui->LikeInfo->setVisible(true);
		}
	}

	return !(viewers.isEmpty() && likes.isEmpty());
}
void ChannelFoldCapsule::changeEvent(QEvent *e)
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

bool ChannelFoldCapsule::eventFilter(QObject *watched, QEvent *event)
{
	switch (event->type()) {
	default:
		break;
	} //end switch

	return false;
}

bool ChannelFoldCapsule::switchUpdateStatisticInfo(bool on)
{
	bool ret = updateStatisticInfo();

	if (on && ret) {
		mUpdateTimer.start(5000);

	} else {
		mUpdateTimer.stop();
	}
	return ret;
}

bool ChannelFoldCapsule::isSelectedDisplay() const
{
	return PLSCHANNELS_API->isChannelSelectedDisplay(mInfoID);
}
