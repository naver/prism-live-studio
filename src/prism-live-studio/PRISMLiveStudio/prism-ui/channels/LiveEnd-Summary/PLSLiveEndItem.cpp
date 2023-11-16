#include "PLSLiveEndItem.h"
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataAPI.h"
#include "QPainter"
#include "pls-channel-const.h"
#include "ui_PLSLiveEndItem.h"

#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include "PLSImageTextButton.h"

PLSLiveEndItem::PLSLiveEndItem(const QString &uuid, QWidget *parent) : QFrame(parent), mSourceData(PLSCHANNELS_API->getChanelInfoRef(uuid))
{
	ui = pls_new<Ui::PLSLiveEndItem>();
	ui->setupUi(this);
	ui->channelIcon->setPadding(0);
	ui->channelIcon->setUseContentsRect(true);
	setupData();
}

PLSLiveEndItem::~PLSLiveEndItem()
{
	pls_delete(ui, nullptr);
}

void PLSLiveEndItem::setupData()
{
	QString platformName = mSourceData.value(ChannelData::g_platformName, "--").toString();
	m_shareUrl = mSourceData.value(ChannelData::g_shareUrlTemp, "").toString();

	ui->tipWidget->setHidden(!needShowTipView());
	ui->channelLabel->adjustSize();
	ui->channelLabel->setText(translatePlatformName(platformName));
	ui->dotLabel->setText("");
	ui->pushButton_tip->setLabelText(tr("nshopping.end.item.button.text"));

	ui->pushButton_share->setHidden(true);

	setNameElideString();

	combineTwoImage();
	setupStatusWidget();
	connect(ui->pushButton_tip, &QPushButton::clicked, [this]() { emit tipButtonClick(); });

	connect(ui->pushButton_share, &QPushButton::clicked, this, &PLSLiveEndItem::shareButtonClicked);
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
	ui->channelIcon->setMainPixmap(userIcon, QSize(34, 34), needSharp);
}

void PLSLiveEndItem::setupStatusWidget()
{
	ui->statusWidget->setHidden(true);

	auto dataType = getInfo(mSourceData, ChannelData::g_data_type, ChannelData::RTMPType);
	QString platformName = mSourceData.value(ChannelData::g_platformName, "").toString();

	if (dataType != ChannelData::ChannelType) {
		return;
	}
	auto hasCount = PLSCHANNELS_API->isPlatformHasCountForEndView(platformName);
	if (!hasCount && platformName != YOUTUBE && platformName != VLIVE && platformName != NAVER_TV && platformName != AFREECATV && platformName != FACEBOOK && platformName != NAVER_SHOPPING_LIVE) {
		return;
	}

	ui->statusWidget->setHidden(false);

	ui->statusImage1->setScaledContents(true);
	ui->statusImage1->setScaledContents(true);
	ui->statusImage1->setScaledContents(true);

	this->refreshUIWhenDPIChanged(1, platformName);
}

void PLSLiveEndItem::refreshUIWhenDPIChanged(double dpi, const QString &platformName)
{
	QString count1 = mSourceData.value(ChannelData::g_viewers, "").toString();
	QString count2 = mSourceData.value(ChannelData::g_likes, "").toString();
	QString count3 = mSourceData.value(ChannelData::g_comments, "").toString();
	QString count4 = mSourceData.value(ChannelData::g_totalViewers, "").toString();

	int originSize = 15;
	int scale = 3;
	int scaleSize = originSize * scale;

	QPixmap pix1(pls_shared_paint_svg(getInfo(mSourceData, ChannelData::g_viewersPix, QString(":/channels/resource/images/ChannelsSource/statistics/ic-liveend-view.svg")),
					  QSize(scaleSize, scaleSize)));
	QPixmap pix2(
		pls_shared_paint_svg(getInfo(mSourceData, ChannelData::g_likesPix, QString(":/channels/resource/images/ChannelsSource/statistics/ic-liveend-like.svg")), QSize(scaleSize, scaleSize)));
	QPixmap pix3(
		pls_shared_paint_svg(getInfo(mSourceData, ChannelData::g_commentsPix, QString(":/channels/resource/images/ChannelsSource/statistics/reply-icon.svg")), QSize(scaleSize, scaleSize)));
	QPixmap pix4(pls_shared_paint_svg(getInfo(mSourceData, ChannelData::g_commentsPix, QString(":/channels/resource/images/ChannelsSource/statistics/ic-liveend-chat-fb.svg")),
					  QSize(scaleSize, scaleSize)));
	if (platformName == YOUTUBE || platformName == NAVER_SHOPPING_LIVE) {
		ui->statusImage3->setHidden(true);
		ui->statusLabel3->setHidden(true);
	} else if (platformName == VLIVE) {
		assert(false);
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
	} else {
		auto viewList = PLSCHANNELS_API->getEndLiveList(mSourceData);
		assert(viewList.size() >= 3);
		for (int i = 0; i < viewList.size() && i * 2 + 1 < ui->counLayout->count(); ++i) {
			const auto &p = viewList[i];
			auto imageLb = dynamic_cast<QLabel *>(ui->counLayout->itemAt(i * 2)->widget());
			auto txtLb = dynamic_cast<QLabel *>(ui->counLayout->itemAt(i * 2 + 1)->widget());
			if (imageLb == nullptr || txtLb == nullptr) {
				continue;
			}
			if (p.first.isEmpty()) {
				imageLb->hide();
				txtLb->hide();
				continue;
			}
			imageLb->setPixmap(p.second);
			txtLb->setText(toThousandsNum(p.first));
		}
		return;
	}

	ui->statusImage1->setPixmap(pix1);
	ui->statusImage2->setPixmap(pix2);
	ui->statusImage3->setPixmap(pix3);

	ui->statusLabel1->setText(toThousandsNum(count1));
	ui->statusLabel2->setText(toThousandsNum(count2));
	ui->statusLabel3->setText(toThousandsNum(count3));

	if (platformName == YOUTUBE) {
		ui->pushButton_share->seIsLeftAlign(true);
		ui->pushButton_share->setLabelText(m_shareUrl);
		ui->pushButton_share->setHidden(false);
	}
}

bool PLSLiveEndItem::needShowTipView() const
{
	auto dataType = getInfo(mSourceData, ChannelData::g_data_type, ChannelData::RTMPType);
	QString platformName = mSourceData.value(ChannelData::g_platformName, "").toString();
	return dataType == ChannelData::ChannelType && platformName == NAVER_SHOPPING_LIVE;
}

QString PLSLiveEndItem::toThousandsNum(QString numString) const
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

	int tipWidgetWidth = needShowTipView() ? 185 : 0;

	int channelIconAndSpace = 34 + 16 + 13;
	int centerTextWidth = ui->contentwidget->width() - channelIconAndSpace - tipWidgetWidth;

	QString userNname = mSourceData.value(ChannelData::g_displayLine1, "--").toString();

	QFontMetrics fontMetrics(ui->channelLabel->font());
	int platformWidth = fontMetrics.horizontalAdvance(ui->channelLabel->text());

	int labelPadding = 5;
	int dotWidth = 3;
	int titleMaxWidth = centerTextWidth - platformWidth - labelPadding - dotWidth - labelPadding - 10;

	QFontMetrics titleFont(ui->liveTitleLabel->font());
	QString elidedText = titleFont.elidedText(userNname, Qt::ElideRight, titleMaxWidth);
	ui->liveTitleLabel->setText(elidedText);
}

void PLSLiveEndItem::shareButtonClicked() const
{
	PLS_UI_STEP(END_MODULE, "PLSEnd Dialog share Button Click", ACTION_CLICK);
	if (m_shareUrl.isEmpty()) {

		PLS_INFO(END_MODULE, "PLSEnd Dialog shared url is empty");
		return;
	}
	QDesktopServices::openUrl(QUrl(m_shareUrl));
}
