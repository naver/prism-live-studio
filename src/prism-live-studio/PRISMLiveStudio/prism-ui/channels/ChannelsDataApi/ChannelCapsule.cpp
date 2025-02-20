#include "ChannelCapsule.h"
#include <QHelpEvent>
#include <QLabel>
#include <QTimer>
#include <QToolTip>
#include "ChannelCommonFunctions.h"
#include "ChannelConfigPannel.h"
#include "LogPredefine.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelsVirualAPI.h"
#include "PLSPlatformNCB2B.h"
#include "frontend-api.h"
#include "libui.h"
#include "pls-channel-const.h"
#include "pls/pls-dual-output.h"
#include "ui_ChannelCapsule.h"

using namespace ChannelData;
constexpr int MaxNameLabelWidth = 86;
constexpr int MaxChannelNameLabelWidth = 130;
const char CheckedStr[] = "Checked";

ChannelCapsule::ChannelCapsule(QWidget *parent) : QFrame(parent), ui(new Ui::ChannelCapsule)
{
	ui->setupUi(this);
	this->setAttribute(Qt::WA_Hover, true);
	this->installEventFilter(this);
	pls_add_css(this, {"ChannelCapsule"});
	initializeConfigPannel();

	connect(&mUpdateTimer, &QTimer::timeout, this, &ChannelCapsule::updateStatisticInfo);
	connect(qApp, &QCoreApplication::aboutToQuit, &mUpdateTimer, &QTimer::stop);

	delayUpdateText();
	ui->UserIcon->setFocusPolicy(Qt::NoFocus);
	closeDualOutputUI();
}

void ChannelCapsule::normalState(const QVariantMap &info)
{

	int dataType = getInfo(info, g_data_type, NoType);
	if (dataType == ChannelType) {
		channelTypeState(info);
	} else if (dataType >= CustomType) {
		RTMPTypeState(info);
	}
}

void ChannelCapsule::shiftState(const QVariantMap &srcData)
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

void ChannelCapsule::RTMPTypeState(const QVariantMap &info)
{
	bool isLiving = PLSCHANNELS_API->isLiving();
	int userState = getInfo(info, g_channelUserStatus, NotExist);

	bool isEnabled = (userState == Enabled);
	bool isAct = (isLiving && isEnabled);

	ui->HeaderPart->setEnabled(isEnabled);
	ui->InfoPart->setEnabled(isEnabled);

	ui->UserIcon->setActive(isAct);
	switchUpdateStatisticInfo(false);
	ui->ChannelInfo->hide();

	ui->TextFrame->setVisible(true);
	ui->StateActive->setVisible(!isLiving);
	if (!isLiving) {
		setOnLine(isEnabled);
	}

	ui->CatogeryFrame->hide();

	ui->ErrorFrame->hide();
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
	bool isInfoShow = switchUpdateStatisticInfo(isAct);
	ui->ChannelInfo->setVisible(isLiving && isInfoShow);

	ui->TextFrame->setVisible(true);
	ui->StateActive->setVisible(!isLiving);
	if (!isLiving) {
		setOnLine(isEnabled);
	}

	bool hasText = !getInfo(info, g_displayLine2).isEmpty() || !getInfo(info, g_catogry).isEmpty();
	bool isCatogeryVisible = !isLiving && hasText;
	ui->CatogeryFrame->setVisible(isCatogeryVisible);

	ui->ErrorFrame->hide();
}

void ChannelCapsule::errorState(const QVariantMap &info)
{
	ui->HeaderPart->setEnabled(false);
	ui->InfoPart->setEnabled(false);

	int dataType = getInfo(info, g_data_type, NoType);
	bool isLiving = PLSCHANNELS_API->isLiving();
	int userState = getInfo(info, g_channelUserStatus, NotExist);

	ui->UserIcon->setActive(false);
	bool isInfoOk = switchUpdateStatisticInfo(false);
	ui->TextFrame->setVisible(true);

	ui->StateActive->setVisible(false);
	if (!isLiving) {
		setOnLine(userState == Enabled);
	}

	if (dataType >= CustomType) {
		ui->CatogeryFrame->setVisible(false);
		ui->ChannelInfo->setVisible(false);
	} else {
		ui->CatogeryFrame->setVisible(!isLiving);
		ui->ChannelInfo->setVisible(isLiving && isInfoOk);
	}
	ui->ErrorFrame->setVisible(false);
}

void ChannelCapsule::unInitializeState(const QVariantMap &info)
{
	waitingState(info);
}

void ChannelCapsule::waitingState(const QVariantMap &info)
{
	ui->HeaderPart->setEnabled(false);
	ui->InfoPart->setEnabled(false);

	ui->UserIcon->setVisible(true);
	ui->UserIcon->setActive(false);

	updateErrorLabel(info);
	ui->ErrorFrame->setVisible(true);
	ui->TextFrame->setVisible(false);

	switchUpdateStatisticInfo(false);
	ui->ChannelInfo->setVisible(false);
	setOnLine(false);
}

void ChannelCapsule::initializeConfigPannel()
{
	mConfigPannel = new ChannelConfigPannel(this);
	mConfigPannel->setAttribute(Qt::WA_Hover, true);
	mConfigPannel->setWindowFlags(Qt::SubWindow | Qt::NoDropShadowWindowHint | Qt::FramelessWindowHint);
	mConfigPannel->installEventFilter(this);
	mConfigPannel->hide();
}

bool ChannelCapsule::isPannelOutOfView() const
{
	if (nullptr == m_pTopWiget) {
		return false;
	}
	auto parentWid = m_pTopWiget;
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

void ChannelCapsule::setChannelID(const QString &uuid)
{
	mInfoID = uuid;
	mConfigPannel->setChannelID(mInfoID);
}

QString ChannelCapsule::translatePublicString(const QString &platform, const QString &src)
{
	if (src.isEmpty()) {
		return src;
	}
	QString transSrc;
	if (platform.contains(YOUTUBE)) {
		QStringList trans{QObject::tr("youtube.privacy.public"), QObject::tr("youtube.privacy.unlisted"), QObject::tr("youtube.privacy.private")};
		QStringList indexs{"youtube.privacy.public", "youtube.privacy.unlisted", "youtube.privacy.private"};
		QRegularExpression rule(".+" + src);
		rule.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

		if (auto index = indexs.indexOf(rule); index != -1) {
			transSrc = trans[index];
		}
	}
	if (platform.contains(NCB2B)) {
		transSrc = PLSAPICommon::getPairedString(PLSPlatformNCB2B::getPrivacyList(), src, true);
	}
	return !transSrc.isEmpty() ? transSrc : src;
}

void ChannelCapsule::updateUi(bool bPostedEvents)
{
	const auto srcData = PLSCHANNELS_API->getChannelInfo(mInfoID);
	if (srcData.isEmpty()) {
		hide();
		PLS_WARN("ChannelCapsule", "srcData is empty, hide this channel");
		return;
	}

	auto bOpen = pls_is_dual_output_on();
	setDualOutput(bOpen);

	mLastMap = srcData;
	updateIcons(mLastMap);
	shiftState(mLastMap);
	updateTextFrames(mLastMap);
	delayUpdateText();
	mConfigPannel->setDualOutput(bOpen);
	mConfigPannel->updateUI();
	if (bPostedEvents)
		QApplication::sendPostedEvents();
}
void ChannelCapsule::updateIcons(const QVariantMap &srcData)
{
	int state = getInfo(srcData, g_channelStatus, Error);

	QString userIcon;
	QString platformIcon;
	if (state == Valid || state == Error || state == Expired || state == UnAuthorized) {
		getComplexImageOfChannel(mInfoID, ImageType::tagIcon, userIcon, platformIcon);
	} else {
		QString channelName = getInfo(srcData, g_channelName);
		userIcon = getPlatformImageFromName(channelName, ImageType::tagIcon);
	}

	ui->UserIcon->setMainPixmap(userIcon, QSize(30, 30));
	ui->UserIcon->setPlatformPixmap(platformIcon, QSize(16, 16));
}
void ChannelCapsule::updateErrorLabel(const QVariantMap &info)
{
	QString errorStr = getInfo(info, g_errorString);
	ui->ErrorLabel->setText(errorStr);
	ui->ErrorLabel->setToolTip(errorStr);
}

void ChannelCapsule::updateTextFrames(const QVariantMap &srcData)
{
	int OutputDirection = ui->OutputDirection->isVisible() ? ui->StateActive->width() : 0;
	auto TextFrameSize = ui->TextFrame->width() - (ui->StateActive->isVisible() ? ui->StateActive->width() : 0) - OutputDirection;
	ui->UserName->resize(TextFrameSize, ui->UserName->height());
	ui->CatogeryName->resize(ui->TextFrame->width(), ui->CatogeryName->height());

	QString line1 = getInfo(srcData, g_displayLine1, QString("No Name"));
	line1 = getElidedText(ui->UserName, line1, TextFrameSize);
	ui->UserName->setText(line1);

	QString line2 = getInfo(srcData, g_displayLine2, QString());

	QString platformName = getInfo(srcData, g_fixPlatformName, QString());
	line2 = translatePublicString(platformName, line2);

	int widthElid = ui->CatogeryName->width();
	if ((platformName == VLIVE || platformName == NAVER_SHOPPING_LIVE) && !line2.isEmpty()) {
		QString split(3, QChar(0x00B7));
		auto values = line2.split(split);
		if (values.size() == 2) {
			auto leftWidth = int(widthElid * 0.45);
			auto leftString = getElidedText(ui->CatogeryName, values[0], leftWidth);
			auto rightWidth = int(widthElid * 0.45);
			auto rightString = getElidedText(ui->CatogeryName, values[1], rightWidth);
			line2 = leftString + " " + QChar(0x00b7) + " " + rightString;
		} else {
			line2 = getElidedText(ui->CatogeryName, line2, widthElid);
		}
	} else {
		line2 = getElidedText(ui->CatogeryName, line2, widthElid);
	}

	//catogry = QString("platfo... ") + QString::fromStdWString(std::wstring{0x25CF})+QString(" chann...")
	//catogry = QString("platfo... ") + QString::fromStdWString(std::wstring{0x00b7}) + QString(" chann...")
	ui->CatogeryName->setText(line2);
	ui->TextFrame->repaint();
}

void ChannelCapsule::delayUpdateText()
{
	if (mLastMap.isEmpty()) {
		return;
	}
	QTimer::singleShot(100, this, [this]() {
		PLS_INFO("ChannelCapsule", "singleShot delayUpdateText");
		updateTextFrames(mLastMap);
	});
}

QString &formatNumber(QString &number)
{
	number = number.simplified();
	if (number.isEmpty()) {
		number = "0";
		return number;
	}
	bool ok;
	auto num = number.toLongLong(&ok);
	if (ok) {
		if (num < 1000) {
			return number;
		}
		if (num >= 1000000) {
			number = QString::number(num / 1000000.0, 'f', 2) + "M";
			return number;
		}
		if (num >= 1000) {
			number = QString::number(num / 1000.0, 'f', 2) + "K";
			return number;
		}
	}
	return number;
}

QString getStatisticsImage(const QString &src, bool isEnabled)
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

QString createStatisticsCss(const QString &srcImage)
{

	QString ret = "QLabel{image: url(%1);color:transparent;} QLabel:disabled{image: url(%2);color:transparent;} ";
	QString disbleImage = getStatisticsImage(srcImage, false);
	return ret.arg(srcImage, disbleImage);
}

bool ChannelCapsule::updateStatisticInfo()
{
	auto info = PLSCHANNELS_API->getChannelInfo(mInfoID);
	QString viewers;

	if (info.contains(g_viewers)) {
		viewers = getInfo(info, g_viewers, QString("0"));
		formatNumber(viewers);
		ui->ViewerNumberLabel->setText(viewers);
		auto css = createStatisticsCss(getInfo(info, g_viewersPix, g_defaultViewerIcon));
		ui->ViewerPicLabel->setStyleSheet(css);
		ui->ViewerFrame->setVisible(true);
	} else {
		ui->ViewerFrame->setVisible(false);
	}
	QString likes;
	if (info.contains(g_likes)) {
		likes = getInfo(info, g_likes, QString("0"));
		formatNumber(likes);
		ui->LikeNumberLabel->setText(likes);

		auto css = createStatisticsCss(getInfo(info, g_likesPix, g_defaultLikeIcon));
		ui->LikePicLabel->setStyleSheet(css);
		ui->LikeInfo->setVisible(true);
	} else {
		ui->LikeInfo->setVisible(false);
	}

	return !(viewers.isEmpty() && likes.isEmpty());
}
bool ChannelCapsule::isYoutube()
{
	int dataType = getInfo(mLastMap, g_data_type, NoType);
	auto name = getInfo(mLastMap, g_channelName, QString());
	if (dataType == ChannelType && name.contains(YOUTUBE, Qt::CaseInsensitive)) {
		return true;
	}
	return false;
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
	if (watched == mConfigPannel) {
		switch (event->type()) {
		case QEvent::Wheel:
			hideConfigPannel();
			break;
#ifdef DEBUG
		case QEvent::MouseButtonDblClick:
			auto mouseEvn = dynamic_cast<QMouseEvent *>(event);
			if (mouseEvn && mouseEvn->button() == Qt::LeftButton && mouseEvn->modifiers().testFlag(Qt::ControlModifier)) {
				ViewMapData(PLSCHANNELS_API->getChannelInfo(mInfoID));
			}
			break;
#endif // DEBUG
		default:
			break;
		}

	} else {
		switch (event->type()) {
		case QEvent::HoverMove:
			if (!isVisible())
				return false;
			if (!PLSCHANNELS_API->isEmptyToAcquire() || isPannelOutOfView() || mConfigPannel->isVisible() || PLSCHANNELS_API->isShifting()) {
				return false;
			}
			showConfigPannel();
			return true;
		case QEvent::HoverLeave:
			if (auto hLeav = dynamic_cast<QHoverEvent *>(event); hLeav) {
				auto pos = hLeav->position().toPoint();
				auto contentRec = this->rect();
				if (!contentRec.contains(pos) && !mConfigPannel->GetMeunShow()) {
					hideConfigPannel();
					return true;
				}
			}
			break;
		default:
			break;
		}
	}
	return false;
}

void ChannelCapsule::showConfigPannel()
{
	if (mConfigPannel && !mConfigPannel->isVisible()) {
		mConfigPannel->show();
	}
}

void ChannelCapsule::hideConfigPannel()
{
	if (mConfigPannel && mConfigPannel->isVisible()) {
		mConfigPannel->hide();
	}
}

bool ChannelCapsule::switchUpdateStatisticInfo(bool on)
{
	bool ret = updateStatisticInfo();

	if (on && ret) {
		mUpdateTimer.start(5000);

	} else {
		mUpdateTimer.stop();
	}
	return ret;
}

bool ChannelCapsule::isOnLine() const
{
	return ui->StateActive->property(CheckedStr).toBool();
}

void ChannelCapsule::setOnLine(bool isActive)
{
	ui->StateActive->setProperty(CheckedStr, isActive);
	ui->StateActive->setDisabled(true);
	refreshStyle(ui->StateActive);
}
bool ChannelCapsule::isSelectedDisplay() const
{
	return PLSCHANNELS_API->isChannelSelectedDisplay(mInfoID);
}

void ChannelCapsule::setDualOutput(bool bOpen)
{
	mConfigPannel->setDualOutput(bOpen);
	mConfigPannel->updateUISpacing(bOpen);
	if (bOpen) {
		auto dualOutput = PLSCHANNELS_API->getValueOfChannel(mInfoID, g_channelDualOutput, NoSet);
		switch (dualOutput) {
		case channel_data::NoSet:
			closeDualOutputUI();
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

void ChannelCapsule::setHorizontalOutputUI()
{
	ui->OutputDirection->setVisible(true);
	ui->OutputDirection->setProperty("VerticalOutput", false);
	refreshStyle(ui->OutputDirection);
}

void ChannelCapsule::setVerticalOutputUI()
{
	ui->OutputDirection->setVisible(true);
	ui->OutputDirection->setProperty("VerticalOutput", true);
	refreshStyle(ui->OutputDirection);
}

void ChannelCapsule::closeDualOutputUI()
{
	ui->OutputDirection->setVisible(false);
	ui->OutputDirection->setText("");
}
