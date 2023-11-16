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
#include "frontend-api.h"
#include "libui.h"
#include "pls-channel-const.h"
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

QString &translateForYoutube(QString &src)
{
	if (src.isEmpty()) {
		return src;
	}
	QStringList trans{QObject::tr("youtube.privacy.public"), QObject::tr("youtube.privacy.unlisted"), QObject::tr("youtube.privacy.private")};
	QStringList indexs{"youtube.privacy.public", "youtube.privacy.unlisted", "youtube.privacy.private"};
	QRegularExpression rule(".+" + src);
	rule.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

	if (auto index = indexs.indexOf(rule); index != -1) {
		src = trans[index];
	}
	return src;
}

void ChannelCapsule::updateUi()
{
	const auto srcData = PLSCHANNELS_API->getChannelInfo(mInfoID);
	if (srcData.isEmpty()) {
		hide();
		PLS_WARN("ChannelCapsule", "srcData is empty, hide this channel");
		return;
	}

	mLastMap = srcData;
	updateIcons(mLastMap);
	shiftState(mLastMap);
	updateTextFrames(mLastMap);
	delayUpdateText();
	mConfigPannel->updateUI();
}
void ChannelCapsule::updateIcons(const QVariantMap &srcData)
{
	int state = getInfo(srcData, g_channelStatus, Error);
	bool needSharp = false;

	QString userIcon;
	QString platformIcon;
	if (state == Valid || state == Error || state == Expired || state == UnAuthorized) {

		if (int type = getInfo(srcData, g_data_type, NoType); type == ChannelType) {
			needSharp = true;
		}
		getComplexImageOfChannel(mInfoID, userIcon, platformIcon);
	} else {
		QString channelName = getInfo(srcData, g_platformName);
		userIcon = getPlatformImageFromName(channelName);
	}

	ui->UserIcon->setMainPixmap(userIcon, QSize(30, 30), needSharp);
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
	auto TextFrameSize = ui->TextFrame->width() - (ui->StateActive->isVisible() ? ui->StateActive->width() : 0);
	ui->UserName->resize(TextFrameSize, ui->UserName->height());
	ui->CatogeryName->resize(ui->TextFrame->width(), ui->CatogeryName->height());

	QString line1 = getInfo(srcData, g_displayLine1, QString("No Name"));
	line1 = getElidedText(ui->UserName, line1, TextFrameSize);
	ui->UserName->setText(line1);

	QString line2 = getInfo(srcData, g_displayLine2, QString());

	QString platformName = getInfo(srcData, g_platformName, QString());
	if (platformName.contains(YOUTUBE) && !line2.isEmpty()) {
		translateForYoutube(line2);
	}

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
}

void ChannelCapsule::delayUpdateText()
{
	if (mLastMap.isEmpty()) {
		return;
	}
	QTimer::singleShot(100, this, [this]() { updateTextFrames(mLastMap); });
}

QString &formatNumber(QString &number, bool isEng = true)
{
	number = number.simplified();
	if (number.isEmpty()) {
		number = "0";
		return number;
	}
	auto N = number.count();
	if (N <= 3) {
		return number;
	}
	if (N <= 6) {
		return number.insert(N - 3, ",");
	}

	QString head;
	QString suffix;

	switch (N) {
	case 7:
		if (isEng) {
			head = number.chopped(5);
			head.insert(N - 6, '.');
			suffix = CHANNELS_TR(Mill);
		} else {
			head = number.chopped(4);
			suffix = CHANNELS_TR(Wan);
		}
		break;
	case 8:
		if (isEng) {
			head = number.chopped(5);
			head.insert(N - 6, '.');
			suffix = CHANNELS_TR(Mill);
		} else {
			head = number.chopped(4);
			head.insert(1, ',');
			suffix = CHANNELS_TR(Wan);
		}

		break;
	case 9:
		if (isEng) {
			head = number.chopped(5);
			head.insert(N - 6, '.');
			suffix = CHANNELS_TR(Mill);
		} else {
			head = number.chopped(7);
			head.insert(1, '.');
			suffix = CHANNELS_TR(Yi);
		}

		break;
	case 10:
		if (isEng) {
			head = number.chopped(8);
			head.insert(N - 9, '.');
			suffix = CHANNELS_TR(Bill);
		} else {
			head = number.chopped(8);
			suffix = CHANNELS_TR(Yi);
		}

		break;

	default:
		if (isEng) {
			head = number.chopped(8);
			head.insert(N - 9, '.');
			suffix = CHANNELS_TR(Bill);
		} else {
			head = number.chopped(8);
			suffix = CHANNELS_TR(Yi);
		}

		break;
	}
	if (head.endsWith(".0")) {
		head.remove(".0");
	}
	number = head + suffix;
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
	bool isEng = IS_ENGLISH();
	auto info = PLSCHANNELS_API->getChannelInfo(mInfoID);
	QString viewers;

	if (info.contains(g_viewers)) {
		viewers = getInfo(info, g_viewers, QString("0"));
		formatNumber(viewers, isEng);
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
		formatNumber(likes, isEng);
		ui->LikeNumberLabel->setText(likes);

		auto css = createStatisticsCss(getInfo(info, g_likesPix, g_defaultLikeIcon));
		ui->LikePicLabel->setStyleSheet(css);
		ui->LikeInfo->setVisible(true);
	} else {
		ui->LikeInfo->setVisible(false);
	}

	return !(viewers.isEmpty() && likes.isEmpty());
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
	case QEvent::HoverMove:
		if (!isVisible())
			return false;
		if (!PLSCHANNELS_API->isEmptyToAcquire() || watched == mConfigPannel || isPannelOutOfView() || mConfigPannel->isVisible() || PLSCHANNELS_API->isShifting()) {
			return false;
		}
		showConfigPannel();
		return true;

	case QEvent::HoverLeave:
		if (mConfigPannel == watched && !mConfigPannel->GetMeunShow()) {
			hideConfigPannel();
			return true;
		} else if (mConfigPannel == watched && mConfigPannel->GetMeunShow()) {
			showConfigPannel();
			return true;
		}

		if (auto hLeav = dynamic_cast<QHoverEvent *>(event); hLeav) {
			auto pos = hLeav->position().toPoint();
			auto contentRec = this->rect();
			if (!contentRec.contains(pos)) {
				hideConfigPannel();
				return true;
			}
		}
		break;
	case QEvent::Wheel:
		if (mConfigPannel == watched) {
			hideConfigPannel();
		}
		break;
#ifdef DEBUG
	case QEvent::MouseButtonDblClick:
		if (mConfigPannel == watched) {
			auto mouseEvn = dynamic_cast<QMouseEvent *>(event);
			if (mouseEvn && mouseEvn->button() == Qt::LeftButton && mouseEvn->modifiers().testFlag(Qt::ControlModifier)) {
				ViewMapData(PLSCHANNELS_API->getChannelInfo(mInfoID));
			}
		}
		break;
#endif // DEBUG

	default:
		break;
	} //end switch

	return false;
}

void ChannelCapsule::showConfigPannel()
{
	mConfigPannel->show();
}

void ChannelCapsule::hideConfigPannel()
{
	mConfigPannel->hide();
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
