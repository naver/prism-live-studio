#include "PLSLiveEndItem.h"
#include "ChannelCommonFunctions.h"
#include "ChannelConst.h"
#include "PLSChannelDataAPI.h"
#include "PLSPlatformApi/vlive/PLSAPIVLive.h"
#include "QPainter"
#include "ui_PLSLiveEndItem.h"

#include <iostream>
#include <regex>
#include <sstream>
#include <string>

PLSLiveEndItem::PLSLiveEndItem(const QString &uuid, QWidget *parent) : QFrame(parent), ui(new Ui::PLSLiveEndItem), mSourceData(PLSCHANNELS_API->getChanelInfoRef(uuid))
{
	ui->setupUi(this);
	ui->channelIcon->setPadding(0);
	ui->channelIcon->setUseContentsRect(true);
	setupData();
}

PLSLiveEndItem::~PLSLiveEndItem()
{
	delete ui;
}

void PLSLiveEndItem::setupData()
{
	QString platformName = mSourceData.value(ChannelData::g_platformName, "--").toString();

	ui->channelLabel->adjustSize();
	ui->channelLabel->setText(translatePlatformName(platformName));
	ui->dotLabel->setText("");

	setNameElideString();

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
	ui->channelIcon->setPlatformPixmap(platformIcon, QSize(18, 18));
	ui->channelIcon->setPixmap(userIcon, QSize(34, 34), needSharp);
}

void PLSLiveEndItem::setupStatusWidget()
{
	ui->statusWidget->setHidden(true);

	auto dataType = getInfo(mSourceData, ChannelData::g_data_type, ChannelData::RTMPType);
	QString platformName = mSourceData.value(ChannelData::g_platformName, "").toString();

	if (dataType != ChannelData::ChannelType) {
		return;
	}

	if (platformName != YOUTUBE && platformName != VLIVE && platformName != NAVER_TV && platformName != AFREECATV && platformName != FACEBOOK && platformName != NAVER_SHOPPING_LIVE) {
		return;
	}

	ui->statusWidget->setHidden(false);

	ui->statusImage1->setScaledContents(true);
	ui->statusImage1->setScaledContents(true);
	ui->statusImage1->setScaledContents(true);

	PLSDpiHelper dpiHelper;
	dpiHelper.notifyDpiChanged(this, [=](double dpi) {
		extern QPixmap paintSvg(const QString &pixmapPath, const QSize &pixSize);

		QString count1 = mSourceData.value(ChannelData::g_viewers, "").toString();
		QString count2 = mSourceData.value(ChannelData::g_likes, "").toString();
		QString count3 = mSourceData.value(ChannelData::g_comments, "").toString();
		QString count4 = mSourceData.value(ChannelData::g_totalViewers, "").toString();

		QPixmap pix1(paintSvg(getInfo(mSourceData, ChannelData::g_viewersPix, QString(":/images/ic-liveend-view.svg")), PLSDpiHelper::calculate(dpi, QSize(15, 15))));
		QPixmap pix2(paintSvg(getInfo(mSourceData, ChannelData::g_likesPix, QString(":/images/ic-liveend-like.svg")), PLSDpiHelper::calculate(dpi, QSize(15, 15))));
		QPixmap pix3(paintSvg(getInfo(mSourceData, ChannelData::g_commentsPix, QString(":/images/reply-icon.svg")), PLSDpiHelper::calculate(dpi, QSize(15, 15))));
		QPixmap pix4(paintSvg(getInfo(mSourceData, ChannelData::g_commentsPix, QString(":/images/ic-liveend-chat-fb.svg")), PLSDpiHelper::calculate(dpi, QSize(15, 15))));
		if (platformName == YOUTUBE || platformName == NAVER_SHOPPING_LIVE) {
			ui->statusImage3->setHidden(true);
			ui->statusLabel3->setHidden(true);
		} else if (platformName == VLIVE) {
			swap(pix2, pix3);
			swap(count2, count3);
			if (PLSAPIVLive::isVliveFanship()) {
				ui->statusImage1->setHidden(true);
				ui->statusLabel1->setHidden(true);
			}
		} else if (platformName == NAVER_TV || platformName == AFREECATV) {
			if (platformName == AFREECATV) {
				swap(count1, count4);
			}
			ui->statusImage2->setHidden(true);
			ui->statusLabel2->setHidden(true);
			ui->statusImage3->setHidden(true);
			ui->statusLabel3->setHidden(true);
		} else if (platformName == FACEBOOK) {
			swap(pix3, pix4);
			ui->statusImage1->setHidden(true);
			ui->statusLabel1->setHidden(true);
		}

		ui->statusImage1->setPixmap(pix1);
		ui->statusImage2->setPixmap(pix2);
		ui->statusImage3->setPixmap(pix3);

		ui->statusLabel1->setText(toThousandsNum(count1));
		ui->statusLabel2->setText(toThousandsNum(count2));
		ui->statusLabel3->setText(toThousandsNum(count3));
	});
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

void PLSLiveEndItem::setNameElideString()
{
	QString userNname = mSourceData.value(ChannelData::g_displayLine1, "--").toString();

	QFontMetrics fontMetrics(ui->channelLabel->font());
	int platformWidth = fontMetrics.width(ui->channelLabel->text());

	int superWidgetWidth = ui->nameWidget->width();
	double dpi = PLSDpiHelper::getDpi(this);

	int labelPadding = PLSDpiHelper::calculate(dpi, 5);
	int dotWidth = PLSDpiHelper::calculate(dpi, 3);
	int titleMaxWidth = superWidgetWidth - platformWidth - labelPadding - dotWidth - labelPadding - PLSDpiHelper::calculate(dpi, 10);

	QFontMetrics titleFont(ui->liveTitleLabel->font());
	QString elidedText = titleFont.elidedText(userNname, Qt::ElideRight, titleMaxWidth);
	ui->liveTitleLabel->setText(elidedText);
}
