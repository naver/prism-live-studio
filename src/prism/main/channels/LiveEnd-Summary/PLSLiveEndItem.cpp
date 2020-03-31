#include "PLSLiveEndItem.h"
#include "ui_PLSLiveEndItem.h"
#include "QPainter"
#include "ChannelConst.h"
#include "ChannelCommonFunctions.h"
#include "ChannelConst.h"
#include "PLSChannelDataAPI.h"

#include <iostream>
#include <string>
#include <sstream>
#include <regex>

PLSLiveEndItem::PLSLiveEndItem(const QString &uuid, QWidget *parent) : QFrame(parent), ui(new Ui::PLSLiveEndItem), mSourceData(PLSCHANNELS_API->getChanelInfoRef(uuid))
{
	ui->setupUi(this);
	setupData();
}

PLSLiveEndItem::~PLSLiveEndItem()
{
	delete ui;
}

void PLSLiveEndItem::setupData()
{

	QString platformName = mSourceData.value(ChannelData::g_channelName, "--").toString();
	QString userNname = mSourceData.value(ChannelData::g_nickName, "--").toString();
	ui->channelLabel->setText(platformName);
	ui->dotLabel->setText("");
	ui->liveTitleLabel->setText(userNname);

	QPixmap pix(":/Images/skin/img-youtube-profile.png");
	QPixmap fixMap = pix.scaled(34, 34, Qt::KeepAspectRatio, Qt::SmoothTransformation);

	QImage *errImage = new QImage(":/Images/skin/icon-error-3.png");
	ui->errorIcon->setPixmap(QPixmap::fromImage(*errImage));
	ui->errorIcon->setHidden(true);

	combineTwoImage();
	setupStatusWidget();
}

void PLSLiveEndItem::combineTwoImage()
{
	using namespace ChannelData;
	QString userIcon;
	QString platformIcon;
	bool needSharp = false;
	int type = getInfo(mSourceData, g_data_type, NoType);
	if (type == ChannelType) {
		needSharp = true;
	}
	getComplexImageOfChannel(getInfo(mSourceData, g_channelUUID), userIcon, platformIcon);
	ui->channelIcon->setPlatformPixmap(platformIcon, false);
	ui->channelIcon->setPixmap(userIcon, needSharp);
}

void PLSLiveEndItem::setupStatusWidget()
{
	ui->statusWidget->setHidden(true);

	auto dataType = getInfo(mSourceData, ChannelData::g_data_type, ChannelData::RTMPType);
	QString platformName = mSourceData.value(ChannelData::g_channelName, "").toString();

	if (dataType != ChannelData::ChannelType) {
		return;
	}

	if (platformName != YOUTUBE) {
		return;
	}

	QString viewers = mSourceData.value(ChannelData::g_viewers, "").toString();
	QString likes = mSourceData.value(ChannelData::g_likes, "").toString();

	ui->statusWidget->setHidden(false);
	QPixmap pix(":/liveend/end/ic-liveend-view.png");
	ui->statusImage1->setScaledContents(true);
	ui->statusImage1->setPixmap(pix);
	ui->statusLabel1->setText(toThousandsNum(viewers));

	pix.load(":/liveend/end/ic-liveend-like.png");
	ui->statusImage2->setPixmap(pix);
	ui->statusImage1->setScaledContents(true);
	ui->statusLabel2->setText(toThousandsNum(likes));
	ui->statusImage2->setHidden(platformName == TWITCH);
	ui->statusLabel2->setHidden(platformName == TWITCH);

	pix.load(":/liveend/end/ic-liveend-like.png");
	ui->statusImage3->setPixmap(pix);
	ui->statusImage1->setScaledContents(true);
	ui->statusLabel3->setText("0");

	ui->statusImage3->setHidden(true);
	ui->statusLabel3->setHidden(true);
}

QString PLSLiveEndItem::toThousandsNum(QString numString)
{
	long long num = numString.toLongLong();
	std::stringstream ss;
	ss << num;

	std::string str = ss.str();
	std::regex pattern("^(-?\\d+)(\\d{3})");

	while (true) {
		str = std::regex_replace(str, pattern, "$1,$2");
		if (!std::regex_search(str, pattern))
			break;
	}

	return QString::fromStdString(str);
}
