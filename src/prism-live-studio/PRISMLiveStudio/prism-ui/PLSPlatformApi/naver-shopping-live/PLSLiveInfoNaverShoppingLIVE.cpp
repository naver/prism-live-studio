#include "PLSLiveInfoNaverShoppingLIVE.h"
#include "ui_PLSLiveInfoNaverShoppingLIVE.h"
#include "PLSLiveInfoNaverShoppingLIVEProductItemView.h"
#include "PLSPlatformApi.h"
#include "../common/PLSDateFormate.h"
#include "PLSAlertView.h"
#include "PLSChannelDataAPI.h"
#include <QtGui/qdesktopservices.h>
#include <QClipboard>
#include <QPainter>
#include <QPainterPath>
#include "PLSPlatformPrism.h"
#include "liblog.h"
#include "PLSServerStreamHandler.hpp"
#include "PLSShoppingHostChannel.h"
#include "libutils-api.h"
#include "log/log.h"
#include "PLSGuideButton.h"

using namespace common;
static const int NAVER_SHOPPING_LIVE_MIN_PHOTO_WIDTH = 170;
static const int NAVER_SHOPPING_LIVE_MIN_PHOTO_HEIGHT = 226;
static const int TitleLengthLimit = 40;
static const int SummaryLengthLimit = 15;

extern void loadPixmap(QPixmap &pix, const QString &pixmapPath, const QSize &pixSize);

#define SERACH_NORMAL_PATH QStringLiteral(":resource/images/ic-help-off-normal.svg")
#define SERACH_HOVER_PATH QStringLiteral(":resource/images/ic-help-off-over.svg")

#define FIRST_CATEGORY_DEFAULT_TITLE tr("navershopping.liveinfo.first.category.title")
#define SECOND_CATEGORY_DEFAULT_TITLE tr("navershopping.liveinfo.second.category.title")

constexpr auto liveInfoMoudule = "PLSLiveInfoNaverShoppingLIVE";

#define IsLiving PLSCHANNELS_API->isLiving()
#define IsPrepareLivePage PLS_PLATFORM_API->isPrepareLive()
#define ThisIsValid [](const QObject *obj) { return pls_object_is_valid(obj); }

PLSLiveInfoNaverShoppingLIVE::PLSLiveInfoNaverShoppingLIVE(PLSPlatformBase *pPlatformBase, const QVariantMap &info, QWidget *parent)
	: PLSLiveInfoBase(pPlatformBase, parent), platform(dynamic_cast<PLSPlatformNaverShoppingLIVE *>(pPlatformBase)), srcInfo(info)
{
	ui = pls_new<Ui::PLSLiveInfoNaverShoppingLIVE>();

	imageScaleThread = pls_new<PLSPLSNaverShoppingLIVEImageScaleThread>();
	imageScaleThread->startThread();
	pls_add_css(this, {"PLSLiveInfoNaverShoppingLIVE"});

	setAttribute(Qt::WA_AlwaysShowToolTips);
	PLS_INFO(liveInfoMoudule, "Naver Shopping LIVE liveinfo Will show");
	pls_add_css(this, {"PLSLiveInfoNaverShoppingLIVE"});
	platform->setAlertParent(this);
	setupUi(ui);

	m_closeGuideTimer = pls_new<QTimer>(this);
	m_closeGuideTimer->setSingleShot(true);

	auto lang = pls_get_current_language();
	if (!IS_KR()) {
		lang = "en-US";
	}
	ui->yearButton->setProperty("lang", lang);

	ui->thumbnailButton->setImageSize(QSize(NAVER_SHOPPING_LIVE_MIN_PHOTO_WIDTH, NAVER_SHOPPING_LIVE_MIN_PHOTO_HEIGHT));
	ui->guideLabel->setHidden(false);
	ui->guideLabel->setText(tr("navershopping.liveinfo.choose.test.schedule.guide"));
	setupData();
	setupGuideButton();
	setupLineEdit();
	setupThumbnailButton();
	setupScheduleComboBox();
	setupDateComboBox();
	setupSearchRadio();
	setupProductList();
	setupEventFilter();
	initSetupUI();
	ui->rehearsalButton->setVisible(PLS_PLATFORM_API->isPrepareLive());
	updateStepTitle(ui->okButton);
	getCategoryListRequest();
#if defined(Q_OS_WIN)
	if (!PLS_PLATFORM_API->isPrepareLive()) {
		ui->horizontalLayout->addWidget(ui->cancelButton);
	}
#endif
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStartBroadcastInInfoView, this, &PLSLiveInfoNaverShoppingLIVE::on_okButton_clicked);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStartRehearsal, this, &PLSLiveInfoNaverShoppingLIVE::on_rehearsalButton_clicked);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStopRehearsal, this, &PLSLiveInfoNaverShoppingLIVE::on_cancelButton_clicked);
	connect(ui->scheduleGuideLabel, &QLabel::linkActivated, this, [](const QString &) { QDesktopServices::openUrl(CHANNEL_NAVER_SHOPPING_LIVE_LOGIN); });

	ui->dualWidget->setText(tr("navershopping.liveinfo.title"))->setUUID(platform->getChannelUUID());
}

PLSLiveInfoNaverShoppingLIVE::~PLSLiveInfoNaverShoppingLIVE()
{
	pls_delete(ui);
	pls_delete(imageScaleThread);
	imageScaleThread = nullptr;
}

void PLSLiveInfoNaverShoppingLIVE::setupData()
{
	m_TempPrepareInfo = platform->getPrepareLiveInfo();
	m_refreshProductList = true;
	if (IsLiving) {
		m_refreshProductList = false;
	} else if (!m_TempPrepareInfo.isNowLiving) {
		m_refreshProductList = false;
	}
}

void PLSLiveInfoNaverShoppingLIVE::setupGuideButton()
{
	auto widget = pls_new<QWidget>();
	widget->setObjectName("guideBackView");

	auto manageButton = pls_new<GuideButton>(tr("NaverShoppingLive.LiveInfo.ScheNoProductGoToNST"), false, nullptr, []() { QDesktopServices::openUrl(CHANNEL_NAVER_SHOPPING_LIVE_LOGIN); });
	manageButton->setObjectName("manageButton");
	manageButton->setProperty("showHandCursor", true);

	auto lineView = pls_new<QWidget>(nullptr);
	lineView->setObjectName("lineView");

	auto layout = pls_new<QHBoxLayout>(widget);
	layout->setContentsMargins(0, 0, 25, 0);
	layout->setSpacing(10);
	layout->addWidget(manageButton);
	layout->addWidget(lineView);
	layout->addWidget(createResolutionButtonsFrame());
	ui->horizontalLayout_3->addStretch();
	ui->horizontalLayout_3->addWidget(widget);
}

void PLSLiveInfoNaverShoppingLIVE::setupLineEdit()
{
	ui->liveTitleLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("LiveInfo.base.Title")));
	ui->summaryLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("navershopping.liveinfo.summary.title")));
	ui->dateLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("navershopping.liveinfo.date.title")));
	connect(ui->lineEditTitle, &QLineEdit::textChanged, this, &PLSLiveInfoNaverShoppingLIVE::titleEdited, Qt::QueuedConnection);
	connect(ui->summaryLineEdit, &QLineEdit::textChanged, this, &PLSLiveInfoNaverShoppingLIVE::summaryEdited, Qt::QueuedConnection);
	ui->searchIcon->setToolTip(QString("<qt>%1</qt>").arg(tr("navershopping.liveinfo.search.hover.tip")));
	ui->searchTitleLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("navershopping.liveinfo.search.title")));
	ui->notifyLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("navershopping.liveinfo.notify.title")));
	ui->helpLabel->setToolTip(tr("navershopping.liveinfo.notify.tooltip"));
	ui->searchIcon->setVisible(m_TempPrepareInfo.isNowLiving);
	ui->helpLabel->setVisible(m_TempPrepareInfo.isNowLiving);
}

void PLSLiveInfoNaverShoppingLIVE::setupThumbnailButton() const
{
	connect(ui->thumbnailButton, &PLSSelectImageButton::imageSelected, this, &PLSLiveInfoNaverShoppingLIVE::imageSelected);
}

void PLSLiveInfoNaverShoppingLIVE::setupScheduleComboBox()
{
	ui->scheCombox->setButtonEnable(!PLS_PLATFORM_API->isLiving());
	connect(ui->scheCombox, &PLSScheduleCombox::pressed, this, &PLSLiveInfoNaverShoppingLIVE::scheduleButtonClicked);
	connect(ui->scheCombox, &PLSScheduleCombox::menuItemClicked, this, &PLSLiveInfoNaverShoppingLIVE::scheduleItemClick);
	QSizePolicy policy = ui->guideLabel->sizePolicy();
	policy.setRetainSizeWhenHidden(true);
	ui->guideLabel->setSizePolicy(policy);
}

void PLSLiveInfoNaverShoppingLIVE::setupDateComboBox()
{
	bool isLiving = PLS_PLATFORM_API->isLiving();
	ui->yearButton->setEnabled(!isLiving);
	ui->hourButton->setEnabled(!isLiving);
	ui->minuteButton->setEnabled(!isLiving);
	ui->apButton->setEnabled(!isLiving);

	connect(ui->yearButton, &QPushButton::toggled, this, [this](bool checked) {
		if (checked) {
			ui->yearButton->showDateCalender();
		}
	});

	connect(ui->yearButton, &PLSShoppingCalenderCombox::clickDate, this, [this](const QDate &date) {
		m_TempPrepareInfo.ymdDate = date;
		QTime time(0, 0, 0);
		auto dateTime = QDateTime(date, time);
		QString yearMonthDay;
		PLSNaverShoppingLIVEAPI::getNaverShoppingDateFormat(dateTime.toSecsSinceEpoch(), yearMonthDay);
		ui->yearButton->setDate(date);
		ui->yearButton->setText(yearMonthDay);
		doUpdateOkState();
	});

	connect(ui->hourButton, &QPushButton::toggled, this, [this](bool checked) {
		if (checked) {
			ui->hourButton->showHour(ui->hourButton->text());
		}
	});

	connect(ui->hourButton, &PLSShoppingCalenderCombox::clickTime, this, [this](QString time) {
		m_TempPrepareInfo.hour = time;
		ui->hourButton->setText(time);
		doUpdateOkState();
	});

	connect(ui->minuteButton, &QPushButton::toggled, this, [this](bool checked) {
		if (checked) {
			ui->minuteButton->showMinute(ui->minuteButton->text());
		}
	});

	connect(ui->minuteButton, &PLSShoppingCalenderCombox::clickTime, this, [this](QString time) {
		m_TempPrepareInfo.minute = time;
		ui->minuteButton->setText(time);
		doUpdateOkState();
	});

	connect(ui->apButton, &QPushButton::toggled, this, [this](bool checked) {
		if (checked) {
			ui->apButton->showAp(ui->apButton->text());
		}
	});

	connect(ui->apButton, &PLSShoppingCalenderCombox::clickTime, this, [this](QString time) {
		int index = apIndexForString(time);
		if (index >= 0) {
			m_TempPrepareInfo.ap = index;
			ui->apButton->setText(time);
			doUpdateOkState();
		}
	});

	QSizePolicy sizePolicy = ui->dateItemWidget->sizePolicy();
	sizePolicy.setRetainSizeWhenHidden(false);
	ui->dateItemWidget->setSizePolicy(sizePolicy);
	ui->horizontalLayout_8->setAlignment(Qt::AlignLeft);
	if (!IS_KR()) {
		ui->horizontalLayout_8->addWidget(ui->apButton);
	}
}

void PLSLiveInfoNaverShoppingLIVE::setupSearchRadio()
{
	connect(ui->radioButton_allow, &PLSRadioButton::clicked, this, [this]() {
		PLS_UI_STEP(liveInfoMoudule, "NaverShopping radioButton allow click", ACTION_CLICK);
		doUpdateOkState();
	});
	connect(ui->radioButton_disallow, &PLSRadioButton::clicked, this, [this]() {
		PLS_UI_STEP(liveInfoMoudule, "NaverShopping radioButton disallow click", ACTION_CLICK);
		doUpdateOkState();
	});
}

void PLSLiveInfoNaverShoppingLIVE::prepareLiving(const std::function<void(bool, const QByteArray &)> &callback)
{
	//The 14-day validity period of the appointment time is not checked during the live broadcast
	if (m_prepareLivingType != PrepareRequestType::UpdateLivingPrepareRequest && !m_TempPrepareInfo.isNowLiving) {
		QStringList apList = ui->apButton->apList();
		int apIndex = apList.indexOf(ui->apButton->text());
		qint64 timeStamp = PLSNaverShoppingLIVEAPI::getLocalTimeStamp(ui->yearButton->date(), ui->hourButton->text(), ui->minuteButton->text(), apIndex);
		QDateTime currTime = QDateTime::currentDateTime();
		currTime = currTime.addDays(-14);
		qint64 filterTimeStamp = currTime.toSecsSinceEpoch();
		if (timeStamp < filterTimeStamp) {
			pls_alert_error_message(nullptr, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.set.invalid.date"));
			callback(false, "");
			return;
		}
	}

	//During the live broadcast, if the live product is empty, no update request will be made
	if (m_prepareLivingType == PrepareRequestType::UpdateLivingPrepareRequest && ui->productWidget->getProduct(PLSProductType::MainProduct).isEmpty()) {
		callback(true, "");
		return;
	}

	//click golive button,check term of agree and use term
	if (m_prepareLivingType == PrepareRequestType::GoLivePrepareRequest) {

		//check navershopping user term
		PLS_INFO(liveInfoMoudule, "Live Term Status: PLSLiveInfoNaverShoppingLIVE m_prepareLivingType is GoLivePrepareRequest");
		if (!platform->checkNaverShoppingTermOfAgree(true)) {
			PLS_INFO(liveInfoMoudule, "Live Term Status: PLSLiveInfoNaverShoppingLIVE checkNaverShoppingTermOfAgree() value is false");
			return;
		}
		PLS_INFO(liveInfoMoudule, "Live Term Status: PLSLiveInfoNaverShoppingLIVE checkNaverShoppingTermOfAgree() value is true");
		platform->checkNaverShoopingNotes(true);

		//Check if the navershopping resolution is legal
		if (!platform->checkGoLiveShoppingResolution(m_TempPrepareInfo)) {
			PLS_INFO(liveInfoMoudule, "Live Term Status: PLSLiveInfoNaverShoppingLIVE checkGoLiveShoppingResolution() value is false");
			return;
		}
		PLS_INFO(liveInfoMoudule, "Live Term Status: PLSLiveInfoNaverShoppingLIVE checkGoLiveShoppingResolution() value is true");

		//check Shopping Host & Influencer Channel
		if (m_TempPrepareInfo.externalExposeAgreementStatus == CHANNEL_CONNECTION_NONE) {
			PLSShoppingHostChannel hostChannel(this);
			bool isOk = (hostChannel.exec() == QDialog::Accepted);
			m_TempPrepareInfo.externalExposeAgreementStatus = isOk ? CHANNEL_CONNECTION_AGREE : CHANNEL_CONNECTION_NOAGREE;
		}
	}

	//click rehearsal button,product is other shop
	if (m_prepareLivingType == PrepareRequestType::RehearsalPrepareRequest) {

		//Check if the navershopping resolution is legal
		if (!platform->checkGoLiveShoppingResolution(m_TempPrepareInfo)) {
			return;
		}
	}

	//Click the ok button before the live broadcast, there are two situations, immediately save the information for live broadcast, and upload pictures first when making an appointment for live broadcast
	if (m_TempPrepareInfo.isNowLiving && m_prepareLivingType == PrepareRequestType::LiveBeforeOkButtonPrepareRequest) {
		startLiving(callback);
		return;
	}

	//if image upload success, then ignore the upload image
	QString bigStandByImagePath = m_TempPrepareInfo.standByImagePath;
	QString scaleStandByImagePath = platform->getScaleImagePath(bigStandByImagePath);
	QString thumbnailButtonImagePath = ui->thumbnailButton->getImagePath();
	bool imagePathChanged = true;
	if (bigStandByImagePath == thumbnailButtonImagePath || scaleStandByImagePath == thumbnailButtonImagePath) {
		imagePathChanged = false;
	}
	if (!imagePathChanged && m_TempPrepareInfo.standByImageURL.length() > 0) {
		startLiving(callback);
		return;
	}

	showLoading(content());
	auto requestCallback = [this, callback](PLSAPINaverShoppingType apiType, const QString &imageURL, const QByteArray &data) {
		hideLoading();
		m_TempPrepareInfo.standByImageURL = imageURL;
		m_TempPrepareInfo.standByImagePath = ui->thumbnailButton->getImagePath();
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			startLiving(callback);
			return;
		}

		//Failed to upload pictures before live broadcast of GoLive and Rehearsal, saving current user information
		saveLiveInfo();

		//Request failure apiType equal to PLSNaverShoppingFailed means that all special errors have been handled, and this error identification needs to be handled by yourself
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
			PLSErrorHandler::ExtraData extraData;
			extraData.urlEn = CHANNEL_NAVER_SHOPPING_LIVE_GET_SEESION_KEY;
			PLSNaverShoppingLIVEAPI::showAlertByPrismCodeWithErrorMsg(data, PLSErrorHandler::CHANNEL_NAVER_SHOPPING_LIVE_UPLOAD_IMAGE_FAILED, NAVER_SHOPPING_LIVE,
										  PLSErrCustomKey_UploadImageFailed, extraData);
		}

		//Failed to upload pictures before GoLive and Rehearsal live, add SRE
		if (m_prepareLivingType == PrepareRequestType::GoLivePrepareRequest) {
			printStartLiveFailedLog(data, "navershopping go live upload local image api reuqest failed");
		} else if (m_prepareLivingType == PrepareRequestType::RehearsalPrepareRequest) {
			printStartLiveFailedLog(data, "navershopping rehearsal upload local image api reuqest failed");
		}
	};
	PLSNaverShoppingLIVEAPI::uploadImage(platform, ui->thumbnailButton->getImagePath(), requestCallback, this, ThisIsValid);
}

void PLSLiveInfoNaverShoppingLIVE::saveLiveInfo()
{
	PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo prepareInfo;
	prepareInfo.title = ui->lineEditTitle->text();
	prepareInfo.standByImagePath = ui->thumbnailButton->getImagePath();
	prepareInfo.standByImageURL = m_TempPrepareInfo.standByImageURL;
	prepareInfo.shoppingProducts = ui->productWidget->getAllProducts();
	bool isNowLiving = m_TempPrepareInfo.isNowLiving;
	prepareInfo.isNowLiving = isNowLiving;
	prepareInfo.scheduleId = m_TempPrepareInfo.scheduleId;
	prepareInfo.isPlanLiving = m_TempPrepareInfo.isPlanLiving;
	prepareInfo.broadcastEndUrl = m_TempPrepareInfo.broadcastEndUrl;
	prepareInfo.infoType = m_TempPrepareInfo.infoType;
	prepareInfo.releaseLevel = m_TempPrepareInfo.releaseLevel;
	prepareInfo.externalExposeAgreementStatus = m_TempPrepareInfo.externalExposeAgreementStatus;
	prepareInfo.allowSearch = ui->radioButton_allow->isChecked();
	prepareInfo.productType = ui->productWidget->getProductType();
	prepareInfo.sendNotification = ui->sendRadioButton->isChecked();
	if (m_prepareLivingType == PrepareRequestType::GoLivePrepareRequest) {
		prepareInfo.infoType = PrepareInfoType::GoLivePrepareInfo;
		if (m_TempPrepareInfo.isNowLiving) {
			prepareInfo.releaseLevel = RELEASE_LEVEL_REAL;
		}
	} else if (m_prepareLivingType == PrepareRequestType::RehearsalPrepareRequest) {
		prepareInfo.infoType = PrepareInfoType::RehearsalPrepareInfo;
		if (m_TempPrepareInfo.isNowLiving) {
			prepareInfo.releaseLevel = RELEASE_LEVEL_REHEARSAL;
		}
	}
	prepareInfo.ymdDate = ui->yearButton->date();
	prepareInfo.yearMonthDay = ui->yearButton->text();
	prepareInfo.hour = ui->hourButton->text();
	prepareInfo.minute = ui->minuteButton->text();
	prepareInfo.ap = apIndexForString(ui->apButton->text());
	prepareInfo.description = ui->summaryLineEdit->text();
	platform->setPrepareInfo(prepareInfo);
}

void PLSLiveInfoNaverShoppingLIVE::startLiving(const std::function<void(bool, const QByteArray &)> &callback)
{
	if (m_prepareLivingType == PrepareRequestType::GoLivePrepareRequest || m_prepareLivingType == PrepareRequestType::RehearsalPrepareRequest) {
		if (!m_TempPrepareInfo.isNowLiving && isModified(m_scheduleTempPrepareInfo)) {
			updateScheduleLiveInfoRequest([this, callback](bool result, const QByteArray &data) {
				if (!result) {
					printStartLiveFailedLog(data, "navershopping update schedule api request failed");
					return;
				}
				createLivingRequest(callback);
			});
			return;
		}
		createLivingRequest(callback);
	} else if (m_prepareLivingType == PrepareRequestType::UpdateLivingPrepareRequest && m_TempPrepareInfo.isNowLiving) {
		auto requestCallback = [this, callback](PLSAPINaverShoppingType apiType, const QByteArray &data) {
			hideLoading();
			if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
				callback(true, data);
				return;
			}
			if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
				PLSErrorHandler::ExtraData extraData;
				extraData.urlEn = CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_LIVING;
				PLSNaverShoppingLIVEAPI::showAlertByPrismCodeWithErrorMsg(data, PLSErrorHandler::CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_LIVE_INFO_FAILED, NAVER_SHOPPING_LIVE,
											  PLSErrCustomKey_UpdateLiveInfoFailedNoService, extraData);
			}
		};
		showLoading(content());
		saveLiveInfo();
		platform->updateLivingRequest(requestCallback, platform->getLivingInfo().id, this, ThisIsValid);
	} else if (m_prepareLivingType == PrepareRequestType::LiveBeforeUpdateSchedulePrepareRequest) {
		callback(true, "");
		//Update Appointment Information Request
		//updateScheduleLiveInfoRequest(callback);

	} else {

		//Click the ok button before the live broadcast, if you choose to schedule a live broadcast at this time
		if (!m_TempPrepareInfo.isNowLiving) {
			if (isModified(m_scheduleTempPrepareInfo)) {
				updateScheduleLiveInfoRequest(callback);
			} else {
				saveLiveInfo();
				callback(true, "");
			}
			platform->setShareLink(m_TempPrepareInfo.broadcastEndUrl);
			return;
		}

		//Click the ok button before the live broadcast. If the live broadcast is selected at this time, the information entered by the user will be saved.
		platform->setShareLink(QString());
		saveLiveInfo();
		callback(true, "");
	}
}

bool PLSLiveInfoNaverShoppingLIVE::isProductListChanged(const QList<PLSNaverShoppingLIVEAPI::ProductInfo> &oldProductList) const
{
	auto newProductList = ui->productWidget->getAllProducts();
	bool productChanged = false;
	if (newProductList.count() != oldProductList.count()) {
		productChanged = true;
	} else {
		for (int i = 0; i < newProductList.count(); i++) {
			QString newKey = newProductList.at(i).key;
			QString oldKey = oldProductList.at(i).key;
			bool newPresent = newProductList.at(i).represent;
			bool oldPresent = oldProductList.at(i).represent;
			PLSProductType newType = newProductList.at(i).productType;
			PLSProductType oldType = oldProductList.at(i).productType;
			if (newKey != oldKey || newPresent != oldPresent || newType != oldType) {
				productChanged = true;
				break;
			}
		}
	}
	return productChanged;
}

void PLSLiveInfoNaverShoppingLIVE::getPrepareInfoFromScheduleInfo(PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &prepareInfo, const PLSNaverShoppingLIVEAPI::ScheduleInfo &scheduleInfo) const
{
	prepareInfo.title = scheduleInfo.title;
	prepareInfo.standByImagePath = scheduleInfo.standByImagePath;
	prepareInfo.standByImageURL = scheduleInfo.standByImage;
	prepareInfo.shoppingProducts = scheduleInfo.shoppingProducts;
	prepareInfo.isNowLiving = false;
	prepareInfo.scheduleId = scheduleInfo.id;
	prepareInfo.broadcastEndUrl = scheduleInfo.broadcastEndUrl;
	prepareInfo.releaseLevel = scheduleInfo.releaseLevel;
	prepareInfo.isPlanLiving = scheduleInfo.broadcastType == PLANNING_LIVING;
	prepareInfo.externalExposeAgreementStatus = scheduleInfo.externalExposeAgreementStatus;
	prepareInfo.allowSearch = scheduleInfo.searchable;
	prepareInfo.sendNotification = scheduleInfo.sendNotification;
	prepareInfo.introducing = scheduleInfo.introducing;
	QString apString;
	PLSNaverShoppingLIVEAPI::getNaverShoppingDateFormat(scheduleInfo.timeStamp, prepareInfo.ymdDate, prepareInfo.yearMonthDay, prepareInfo.hour, prepareInfo.minute, apString);
	prepareInfo.ap = apIndexForString(apString);
	prepareInfo.description = scheduleInfo.description;
}

void PLSLiveInfoNaverShoppingLIVE::updateContentMargins(double dpi)
{
	//int _25px = PLSDpiHelper::calculate(dpi, 25);
	int _25px = 25;
	if (m_verticalScrollBar->isVisible()) {
		ui->verticalLayout->setContentsMargins(QMargins(_25px, 0, _25px - m_verticalScrollBar->width(), 0));
	} else {
		ui->verticalLayout->setContentsMargins(QMargins(_25px, 0, _25px, 0));
	}
}

bool PLSLiveInfoNaverShoppingLIVE::isTitleTooLong(QString &title, int maxWordCount) const
{
	bool isToLong = false;
	QVector<uint> ucs4 = title.toUcs4();
	if (int length = ucs4.count(); length > maxWordCount) {
		ucs4.remove(maxWordCount, length - maxWordCount);
		title = QString::fromUcs4((const char32_t *)ucs4.constData(), ucs4.count());
		isToLong = true;
	}
	return isToLong;
}

void PLSLiveInfoNaverShoppingLIVE::updateLiveTitleUI()
{
	QString title = m_TempPrepareInfo.title;
	if (title.isEmpty()) {
		title = tr("navershopping.liveinfo.default.title").arg(platform->getUserInfo().nickname);
	}
	if (m_TempPrepareInfo.isNowLiving) {
		isTitleTooLong(title, TitleLengthLimit);
	}
	ui->lineEditTitle->setText(title);
	ui->lineEditTitle->setEnabled(m_TempPrepareInfo.isNowLiving);
	ui->linkButton->setHidden(m_TempPrepareInfo.isNowLiving);
}

void PLSLiveInfoNaverShoppingLIVE::updateLivePhotoUI()
{
	QString originImagePath = m_TempPrepareInfo.standByImagePath;
	ui->thumbnailButton->setProperty("ignoreHover", !m_TempPrepareInfo.isNowLiving);
	if (QString resultPath; platform->getScalePixmapPath(resultPath, originImagePath)) {
		ui->thumbnailButton->setImagePath(resultPath);
	} else {
		ui->thumbnailButton->setImagePath(originImagePath);
	}
	//click dashboard golive button show liveinfo,when choose test reservation item
	if (IsPrepareLivePage && !m_TempPrepareInfo.isNowLiving && m_TempPrepareInfo.releaseLevel == RELEASE_LEVEL_REHEARSAL) {
		showToast(tr("navershopping.liveinfo.choose.test.schedule.toast"));
	}
}

void PLSLiveInfoNaverShoppingLIVE::updateLiveSummaryUI()
{
	if (m_TempPrepareInfo.isNowLiving) {
		isTitleTooLong(m_TempPrepareInfo.description, SummaryLengthLimit);
	}
	ui->summaryLineEdit->setText(m_TempPrepareInfo.description);
	ui->summaryLineEdit->setEnabled(m_TempPrepareInfo.isNowLiving);
}

void PLSLiveInfoNaverShoppingLIVE::updateScheduleGuideUI()
{
	ui->scheduleGuideLabel->setText(QTStr("NaverShoppingLive.LiveInfo.Schedule.Live.Guide"));
	ui->scheduleGuideLabel->setVisible(!m_TempPrepareInfo.isNowLiving);
	if (m_TempPrepareInfo.isNowLiving) {
		ui->verticalSpacer_6->changeSize(0, 0);
	} else {
		ui->verticalSpacer_6->changeSize(0, 20);
	}
}

void PLSLiveInfoNaverShoppingLIVE::updateLiveDateUI()
{
	ui->dateItemWidget->setHidden(m_TempPrepareInfo.isNowLiving);
	QDate ymdDate;
	QString yearMonthDay;
	QString hourString;
	QString minuteString;
	QString apString;
	if (!m_TempPrepareInfo.isNowLiving) {
		PLSNaverShoppingLIVEAPI::ScheduleInfo scheduleInfo = platform->getSelectedScheduleInfo(m_TempPrepareInfo.scheduleId);
		PLSNaverShoppingLIVEAPI::getNaverShoppingDateFormat(scheduleInfo.timeStamp, ymdDate, yearMonthDay, hourString, minuteString, apString);
	}
	ui->yearButton->setText(yearMonthDay);
	ui->yearButton->setDate(ymdDate);
	ui->hourButton->setText(hourString);
	ui->minuteButton->setText(minuteString);
	ui->apButton->setText(apString);

	QString comboboxTitle = m_TempPrepareInfo.title;
	if (m_TempPrepareInfo.isNowLiving) {
		comboboxTitle = tr("New");
	}
	QString startTimeShort;
	if (ui->yearButton->text().length() > 0 && ui->hourButton->text().length() > 0 && ui->minuteButton->text().length() > 0 && ui->apButton->text().length() > 0) {
		PLSNaverShoppingLIVEAPI::ScheduleInfo scheduleInfo = platform->getSelectedScheduleInfo(m_TempPrepareInfo.scheduleId);
		startTimeShort = PLSDateFormate::timeStampToShortString(scheduleInfo.timeStamp);
	}
	ui->scheCombox->setupButton(comboboxTitle, startTimeShort, (!m_TempPrepareInfo.isNowLiving && m_TempPrepareInfo.releaseLevel == RELEASE_LEVEL_REHEARSAL));

	ui->yearButton->setEnabled(m_TempPrepareInfo.isNowLiving);
	ui->hourButton->setEnabled(m_TempPrepareInfo.isNowLiving);
	ui->minuteButton->setEnabled(m_TempPrepareInfo.isNowLiving);
	ui->apButton->setEnabled(m_TempPrepareInfo.isNowLiving);
}

void PLSLiveInfoNaverShoppingLIVE::updateSearchUI()
{
	if (m_TempPrepareInfo.allowSearch) {
		ui->radioButton_allow->setChecked(true);
	} else {
		ui->radioButton_disallow->setChecked(true);
	}
	ui->radioButton_allow->setEnabled(m_TempPrepareInfo.isNowLiving);
	ui->radioButton_disallow->setEnabled(m_TempPrepareInfo.isNowLiving);
}

void PLSLiveInfoNaverShoppingLIVE::updateNotifyUI()
{
	if (m_TempPrepareInfo.sendNotification) {
		ui->sendRadioButton->setChecked(true);
	} else {
		ui->notSendRadioButton->setChecked(true);
	}

	if (m_TempPrepareInfo.isNowLiving) {
		bool isLiving = PLS_PLATFORM_API->isLiving();
		ui->notifyRadioButtonWidget->setEnabled(!isLiving);
	} else {
		if (m_TempPrepareInfo.releaseLevel == RELEASE_LEVEL_REHEARSAL) {
			ui->sendRadioButton->setChecked(true);
		}
		ui->notifyRadioButtonWidget->setEnabled(false);
	}
}

void PLSLiveInfoNaverShoppingLIVE::switchNewScheduleItem(PLSScheComboxItemType type, QString id)
{
	ui->productWidget->cancelSearchRequest();

	//switch from nowLiving to schedule,save the nowLiving data
	if (type == PLSScheComboxItemType::Ty_NormalLive) {
		m_TempPrepareInfo = m_normalTempPrepareInfo;
		m_TempPrepareInfo.isNowLiving = true;
		m_TempPrepareInfo.isPlanLiving = false;
		m_scheduleTempPrepareInfo = PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo();
		updateSetupUI();

	} else {
		//switch from nowliving to schdule,need save now living data
		if (m_TempPrepareInfo.isNowLiving) {
			m_normalTempPrepareInfo.title = ui->lineEditTitle->text();
			m_normalTempPrepareInfo.description = ui->summaryLineEdit->text();
			m_normalTempPrepareInfo.standByImagePath = ui->thumbnailButton->getImagePath();
			m_normalTempPrepareInfo.shoppingProducts = ui->productWidget->getAllProducts();
			m_normalTempPrepareInfo.allowSearch = ui->radioButton_allow->isChecked();
			m_normalTempPrepareInfo.sendNotification = ui->sendRadioButton->isChecked();
		}

		auto requestCallback = [this, id](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo) {
			if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
				PLSNaverShoppingLIVEAPI::ScheduleInfo scheduleInfo = platform->getSelectedScheduleInfo(id);
				scheduleInfo.shoppingProducts = livingInfo.shoppingProducts;
				getPrepareInfoFromScheduleInfo(m_TempPrepareInfo, scheduleInfo);
				m_scheduleTempPrepareInfo = m_TempPrepareInfo;
				if (m_TempPrepareInfo.standByImagePath.isEmpty()) {
					showToast(tr("navershopping.liveinfo.load.thumbnail.failed"));
				}

			} else {
				PLS_LIVE_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live call living info failed, apiType: %d", apiType);
			}
			hideLoading();
			updateSetupUI();
		};
		showLoading(content());
		PLSNaverShoppingLIVEAPI::getLivingInfo(platform, id, false, requestCallback, this, ThisIsValid);
	}
}

void PLSLiveInfoNaverShoppingLIVE::updateScheduleLiveInfoRequest(const std::function<void(bool, const QByteArray &)> &callback)
{
	auto requestCallback = [this, callback](PLSAPINaverShoppingType apiType, const QByteArray &data) {
		hideLoading();
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			callback(true, data);
			return;
		}
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
			PLSErrorHandler::ExtraData extraData;
			extraData.urlEn = CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_LIVING;
			PLSNaverShoppingLIVEAPI::showAlertByPrismCodeWithErrorMsg(data, PLSErrorHandler::CHANNEL_NAVER_SHOPPING_LIVE_UPDATE_SCHEDULE_INFO_FAILED, NAVER_SHOPPING_LIVE,
										  PLSErrCustomKey_LoadLiveInfoFailed, extraData);
		}
		callback(false, data);
	};
	showLoading(content());
	saveLiveInfo();
	platform->updateScheduleRequest(requestCallback, m_TempPrepareInfo.scheduleId, this, ThisIsValid);
}

void PLSLiveInfoNaverShoppingLIVE::createLivingRequest(const std::function<void(bool, const QByteArray &)> &callback)
{
	auto requestCallback = [this, callback](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &, const QByteArray &data) {
		if (apiType != PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			hideLoading();
			PLSErrorHandler::ExtraData extraData;
			if (m_TempPrepareInfo.isNowLiving) {
				extraData.urlEn = CHANNEL_NAVER_SHOPPING_LIVE_CREATE_NOW_LIVING;
			} else {
				extraData.urlEn = CHANNEL_NAVER_SHOPPING_LIVE_CREATE_SCHEDULE_LIVING;
			}
			if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
				if (ui->productWidget->hasUnattachableProduct()) {
					PLSNaverShoppingLIVEAPI::showAlertByPrismCodeWithErrorMsg(data, PLSErrorHandler::CHANNEL_NAVER_SHOPPING_LIVE_ADD_UNATTACHABLE_PRODUCT_FAILED,
												  NAVER_SHOPPING_LIVE, "FailedToStartLive", extraData);
				} else {
					PLSNaverShoppingLIVEAPI::showAlertByPrismCodeWithErrorMsg(data, PLSErrorHandler::CHANNEL_NAVER_SHOPPING_LIVE_CREATE_SCHEDULE_LIVING_FAILED, NAVER_SHOPPING_LIVE,
												  "FailedToStartLive", extraData);
				}
			}
			printStartLiveFailedLog(data, "navershopping create live api request failed");
			return;
		}
		platform->checkPushNotification([this, callback]() {
			PLS_LOGEX(PLS_LOG_INFO, liveInfoMoudule, {{"platformName", "navershopping"}, {"startLiveStatus", "Success"}}, "navershopping start live success");
			hideLoading();
			callback(true, "");
		});
	};
	showLoading(content());
	saveLiveInfo();
	platform->createLiving(requestCallback, this, ThisIsValid);
}

int PLSLiveInfoNaverShoppingLIVE::apIndexForString(const QString &apString) const
{
	QStringList apList = ui->apButton->apList();
	int index = -1;
	if (apList.contains(apString)) {
		index = apList.indexOf(apString);
	}
	return index;
}

void PLSLiveInfoNaverShoppingLIVE::showToast(const QString &str)
{
	m_closeGuideTimer->start(5000);
	createToastWidget();
	m_pLabelToast->setText(str);
	adjustToastSize();
}

void PLSLiveInfoNaverShoppingLIVE::scheduleListLoadingFinished(PLSAPINaverShoppingType apiType, const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList, const QByteArray &data)
{
	if (apiType != PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
			PLSErrorHandler::ExtraData extraData;
			extraData.urlEn = CHANNEL_NAVER_SHOPPING_LIVE_SCHEDULE_LIST;
			PLSNaverShoppingLIVEAPI::showAlertByPrismCodeWithErrorMsg(data, PLSErrorHandler::CHANNEL_NAVER_SHOPPING_LIVE_REFRESH_LIVEINFO_FAILED, NAVER_SHOPPING_LIVE,
										  PLSErrCustomKey_LoadLiveInfoFailed, extraData);
		}
		return;
	}
	m_vecItemDatas.clear();
	QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> list = scheduleList;
	double dpi = 1;
	QDateTime currTime = QDateTime::currentDateTime();
	currTime = currTime.addDays(-14);
	currTime = currTime.toUTC();
	qint64 filterTimeStamp = currTime.toSecsSinceEpoch();
	std::sort(list.begin(), list.end(), [](const PLSNaverShoppingLIVEAPI::ScheduleInfo &lhs, const PLSNaverShoppingLIVEAPI::ScheduleInfo &rhs) { return lhs.timeStamp > rhs.timeStamp; });
	for (PLSNaverShoppingLIVEAPI::ScheduleInfo info : list) {
		uint nowTime = PLSDateFormate::getNowTimeStamp();
		uint scheduleTime = info.timeStamp;
		if (scheduleTime < filterTimeStamp) {
			continue;
		}
		bool expired = nowTime > scheduleTime;
		auto scheData = PLSScheComboxItemData();
		scheData.title = info.title;
		scheData._id = info.id;
		scheData.showIcon = (info.releaseLevel == RELEASE_LEVEL_REHEARSAL);
		scheData.type = PLSScheComboxItemType::Ty_Schedule;
		scheData.time = info.startTimeUTC;
		scheData.platformName = "navershopping";
		scheData.isExpired = expired;
		if (info.standByImagePath.length() > 0 && !platform->isAddScaleImageThread(info.standByImagePath)) {
			imageScaleThread->push(std::make_tuple(platform, dpi, info.standByImagePath, ui->thumbnailButton->size()));
		}
		if (info.id != m_TempPrepareInfo.scheduleId) {
			if (expired) {
				m_vecItemDatas.push_back(scheData);
			} else {
				m_vecItemDatas.insert(m_vecItemDatas.begin(), scheData);
			}
		}
	}
	if (!m_TempPrepareInfo.isNowLiving) {
		auto normalData = PLSScheComboxItemData();
		normalData._id = "";
		normalData.title = tr("New");
		normalData.time = tr("New");
		normalData.type = PLSScheComboxItemType::Ty_NormalLive;
		m_vecItemDatas.insert(m_vecItemDatas.begin(), normalData);
	}

	if (m_vecItemDatas.empty()) {
		auto normalData = PLSScheComboxItemData();
		normalData._id = "";
		normalData.title = tr("New");
		normalData.time = tr("LiveInfo.Youtube.no.scheduled");
		normalData.type = PLSScheComboxItemType::Ty_Placeholder;
		m_vecItemDatas.insert(m_vecItemDatas.begin(), normalData);
	}
	ui->scheCombox->showScheduleMenu(m_vecItemDatas);
}

void PLSLiveInfoNaverShoppingLIVE::setupProductList()
{
	ui->productWidget->setOwnerScrollArea(ui->scrollArea);
	ui->productWidget->setIsLiving(IsLiving);
	connect(ui->productWidget, &PLSLiveInfoNaverShoppingLIVEProductList::productChangedOrUpdated, this, [this](bool) { doUpdateOkState(); });
}

void PLSLiveInfoNaverShoppingLIVE::setupEventFilter()
{
	m_verticalScrollBar = ui->scrollArea->verticalScrollBar();
	m_verticalScrollBar->installEventFilter(this);
	ui->searchIcon->installEventFilter(this);
	connect(platform, &PLSPlatformNaverShoppingLIVE::showLiveinfoLoading, this, [this]() { showLoading(content()); }, Qt::QueuedConnection);
	connect(platform, &PLSPlatformNaverShoppingLIVE::hiddenLiveinfoLoading, this, [this]() { hideLoading(); }, Qt::QueuedConnection);
	connect(
		platform, &PLSPlatformNaverShoppingLIVE::closeDialogByExpired, this,
		[this]() {
			ui->productWidget->expiredClose();
			reject();
		},
		Qt::QueuedConnection);
}

void PLSLiveInfoNaverShoppingLIVE::initSetupUI(bool requestFinished)
{
	m_TempPrepareInfo = platform->getPrepareLiveInfo();
	if (m_TempPrepareInfo.isNowLiving) {
		m_scheduleTempPrepareInfo = PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo();
		m_normalTempPrepareInfo = m_TempPrepareInfo;
	} else {
		m_normalTempPrepareInfo = PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo();
		m_scheduleTempPrepareInfo = m_TempPrepareInfo;
	}

	//liveInfo title
	updateLiveTitleUI();

	//LiveInfo Profile
	updateLivePhotoUI();

	//LiveInfo summary
	updateLiveSummaryUI();

	updateScheduleGuideUI();

	//LiveInfo product list
	ui->productWidget->setIsScheduleLive(!m_TempPrepareInfo.isNowLiving);
	ui->productWidget->setIsPlanningLive(m_TempPrepareInfo.isPlanLiving);
	ui->productWidget->setProductType(m_TempPrepareInfo.productType);
	if (m_refreshProductList) {
		ui->productWidget->setIsScheduleLiveLoading(false);
		ui->productWidget->setAllProducts(m_TempPrepareInfo.shoppingProducts, true);
	} else {
		ui->productWidget->setIsScheduleLiveLoading(!requestFinished);
		if (requestFinished) {
			ui->productWidget->setAllProducts(m_TempPrepareInfo.shoppingProducts, false);
		}
	}

	//LiveInfo date
	updateLiveDateUI();

	//LiveInfo search
	updateSearchUI();

	//LiveInfo notify
	updateNotifyUI();

	//update ok button state
	doUpdateOkState();
}

void PLSLiveInfoNaverShoppingLIVE::updateRequest()
{
	if (IsLiving) {
		auto requestCallback = [this](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &) {
			hideLoading();
			if (apiType != PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
				ui->productWidget->setIsScheduleLiveLoading(false);
				ui->productWidget->setAllProducts(m_TempPrepareInfo.shoppingProducts, false);
				doUpdateOkState();
				return;
			}
			initSetupUI(true);
		};
		showLoading(content());
		platform->getLivingInfo(false, requestCallback, this, ThisIsValid);
	} else if (!m_TempPrepareInfo.isNowLiving) {
		auto RequestCallback = [this](PLSAPINaverShoppingType apiType, const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList, int, int, const QByteArray &) {
			hideLoading();
			if (apiType != PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
				ui->productWidget->setIsScheduleLiveLoading(false);
				ui->productWidget->setAllProducts(m_TempPrepareInfo.shoppingProducts, false);
				doUpdateOkState();
				return;
			}
			updateSelectedScheduleItenInfo(scheduleList);
		};
		showLoading(content());
		m_requestFlag++;
		platform->getScheduleList(RequestCallback, SCHEDULE_FIRST_PAGE_NUM, m_requestFlag, LIVEINFO_GET_SCHEDULE_LIST, this, ThisIsValid);
	}
}

void PLSLiveInfoNaverShoppingLIVE::updateSelectedScheduleItenInfo(const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList)
{
	bool contains = false;
	for (PLSNaverShoppingLIVEAPI::ScheduleInfo info : scheduleList) {
		if (info.id == m_TempPrepareInfo.scheduleId) {
			auto requestCallback = [this, info, &contains](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &livingInfo) {
				if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
					PLSNaverShoppingLIVEAPI::ScheduleInfo tempInfo = info;
					tempInfo.shoppingProducts = livingInfo.shoppingProducts;

					//update liveInfo prepareInfo data
					auto tempPrepareInfo = platform->getPrepareLiveInfo();
					getPrepareInfoFromScheduleInfo(tempPrepareInfo, tempInfo);
					platform->setPrepareInfo(tempPrepareInfo);
				} else {
					PLS_LIVE_ERROR(MODULE_PLATFORM_NAVER_SHOPPING_LIVE, "Naver Shopping Live call living info failed, apiType: %d", apiType);
				}
				hideLoading();
				initSetupUI(true);
			};
			contains = true;
			showLoading(content());
			PLSNaverShoppingLIVEAPI::getLivingInfo(platform, info.id, false, requestCallback, this, ThisIsValid);
			break;
		}
	}

	if (!contains) {
		platform->clearLiveInfo();
		initSetupUI(true);
	}
}

void PLSLiveInfoNaverShoppingLIVE::printStartLiveFailedLog(const QByteArray &data, const QString &error)
{
	QString errorCode = "null";
	QString errorMessage = "null";
	PLSNaverShoppingLIVEAPI::getErrorCodeOrErrorMessage(data, errorCode, errorMessage);
	PLS_LOGEX(PLS_LOG_ERROR, liveInfoMoudule,
		  {{"platformName", "navershopping"},
		   {"startLiveStatus", "Failed"},
		   {"startLiveFailed", (error + QString(", error code: %1, error message: %2").arg(errorCode).arg(errorMessage)).toUtf8().constData()}},
		  "navershopping start live failed");
}

void PLSLiveInfoNaverShoppingLIVE::getCategoryListRequest()
{
	if (!pls_get_network_state()) {
		ui->productWidget->setAllProducts(m_TempPrepareInfo.shoppingProducts, false);
		doUpdateOkState();
		return;
	}

	auto categoryCallback = [this](PLSAPINaverShoppingType apiType, const QList<PLSNaverShoppingLIVEAPI::LiveCategory> & /*categoryList*/, const QByteArray &data) {
		if (m_refreshProductList) {
			hideLoading();
		}
		doUpdateOkState();
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
			PLSErrorHandler::ExtraData extraData;
			extraData.urlEn = CHANNEL_NAVER_SHOPPING_LIVE_CATEGORY_LIST;
			PLSNaverShoppingLIVEAPI::showAlertByPrismCodeWithErrorMsg(data, PLSErrorHandler::CHANNEL_NAVER_SHOPPING_LIVE_GET_CATEGORY_LIST_FAILED, NAVER_SHOPPING_LIVE, "", extraData);
		}
	};

	updateRequest();
}

void PLSLiveInfoNaverShoppingLIVE::updateSetupUI()
{

	//liveInfo title
	updateLiveTitleUI();

	//LiveInfo Profile
	updateLivePhotoUI();

	//LiveInfo summary
	updateLiveSummaryUI();

	updateScheduleGuideUI();

	ui->searchIcon->setVisible(m_TempPrepareInfo.isNowLiving);
	ui->helpLabel->setVisible(m_TempPrepareInfo.isNowLiving);

	//LiveInfo product
	QList<PLSNaverShoppingLIVEAPI::ProductInfo> list = m_TempPrepareInfo.shoppingProducts;
	ui->productWidget->setIsScheduleLive(!m_TempPrepareInfo.isNowLiving);
	ui->productWidget->setIsPlanningLive(m_TempPrepareInfo.isPlanLiving);
	ui->productWidget->setProductType(m_TempPrepareInfo.productType);
	ui->productWidget->setAllProducts(list, true);

	//LiveInfo date
	updateLiveDateUI();

	//LiveInfo search
	updateSearchUI();

	//Live info notify
	updateNotifyUI();

	//update ok button state
	doUpdateOkState();
}

void PLSLiveInfoNaverShoppingLIVE::doUpdateOkState()
{
	bool enabled = isOkButtonEnabled();
	if (IsPrepareLivePage && !m_TempPrepareInfo.isNowLiving && m_TempPrepareInfo.releaseLevel == RELEASE_LEVEL_REHEARSAL) {
		ui->okButton->setEnabled(false);
	} else {
		ui->okButton->setEnabled(enabled);
	}
	ui->rehearsalButton->setEnabled(enabled);
}

bool PLSLiveInfoNaverShoppingLIVE::isModified(const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &info)
{
	if (!m_TempPrepareInfo.isNowLiving) {
		return false;
	}

	//Whether the uploaded live image has changed
	bool imagePathChanged = true;
	QString bigStandByImagePath = info.standByImagePath;
	QString scaleStandByImagePath = platform->getScaleImagePath(bigStandByImagePath);
	if (bigStandByImagePath == ui->thumbnailButton->getImagePath() || scaleStandByImagePath == ui->thumbnailButton->getImagePath()) {
		imagePathChanged = false;
	}

	if (imagePathChanged) {
		return true;
	}

	//check host channel same
	if (m_TempPrepareInfo.externalExposeAgreementStatus != info.externalExposeAgreementStatus) {
		return true;
	}

	//Whether the title of the uploaded live stream has changed
	if (info.title != ui->lineEditTitle->text()) {
		return true;
	}

	//Whether the description of the upload stream has changed
	if (info.description != ui->summaryLineEdit->text()) {
		return true;
	}

	//Whether the date of uploading the live stream has changed
	if (!m_TempPrepareInfo.isNowLiving) {
		QDate ymdDate;
		QString yearMonthDay;
		QString hourString;
		QString minuteString;
		QString apString;
		PLSNaverShoppingLIVEAPI::ScheduleInfo scheduleInfo = platform->getSelectedScheduleInfo(info.scheduleId);
		PLSNaverShoppingLIVEAPI::getNaverShoppingDateFormat(scheduleInfo.timeStamp, ymdDate, yearMonthDay, hourString, minuteString, apString);
		bool dateNotChanged = yearMonthDay == ui->yearButton->text() && hourString == ui->hourButton->text() && minuteString == ui->minuteButton->text() && apString == ui->apButton->text();
		if (!dateNotChanged) {
			return true;
		}
	}

	//Whether the search for uploading the live stream has changed
	if (ui->radioButton_allow->isChecked() != info.allowSearch) {
		return true;
	}

	if (ui->sendRadioButton->isChecked() != info.sendNotification) {
		return true;
	}

	//Whether the list of products uploaded for live broadcast has changed
	if (isProductListChanged(info.shoppingProducts)) {
		return true;
	}

	return false;
}

bool PLSLiveInfoNaverShoppingLIVE::isOkButtonEnabled()
{
	//Whether the uploaded avatar address is empty
	if (ui->thumbnailButton->getImagePath().isEmpty()) {
		return false;
	}

	//Whether the input title of the live broadcast is empty
	if (ui->lineEditTitle->text().trimmed().isEmpty()) {
		return false;
	}

	//Whether the input introduction of the live broadcast is empty
	if (ui->summaryLineEdit->text().trimmed().isEmpty()) {
		return false;
	}

	//If no item list is selected
	if (ui->productWidget->getProduct(PLSProductType::MainProduct).isEmpty()) {
		if (m_TempPrepareInfo.isPlanLiving && !isModified(m_scheduleTempPrepareInfo)) {
			return true;
		}
		return false;
	}

	return true;
}

void PLSLiveInfoNaverShoppingLIVE::createToastWidget()
{
	if (m_pLabelToast.isNull()) {
		m_pLabelToast = pls_new<QLabel>(this, Qt::SubWindow | Qt::FramelessWindowHint);
#if defined(Q_OS_MACOS)
		m_pLabelToast->setAttribute(Qt::WA_DontCreateNativeAncestors);
		m_pLabelToast->setAttribute(Qt::WA_NativeWindow);
#endif
		m_pLabelToast->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		m_pLabelToast->setObjectName("labelToast");
		m_pLabelToast->setMouseTracking(true);
		m_pLabelToast->setWordWrap(true);
		m_pLabelToast->show();
		m_pLabelToast->raise();

		m_pButtonToastClose = pls_new<QPushButton>(m_pLabelToast);
		m_pButtonToastClose->setObjectName("pushButtonClear");
		m_pButtonToastClose->show();

		connect(m_closeGuideTimer, &QTimer::timeout, this, [this]() { m_pLabelToast->deleteLater(); });
		connect(m_pButtonToastClose, &QPushButton::clicked, m_pLabelToast, &QObject::deleteLater);
	}
}

void PLSLiveInfoNaverShoppingLIVE::adjustToastSize()
{
	if (!m_pLabelToast.isNull()) {
		int spaceWidth = 25;
		int closeTop = 12;
		int closeWidth = 12;
		int closeRight = 14;
		m_pLabelToast->setFixedSize(this->width() - 2 * spaceWidth, 40);
#if defined(Q_OS_MACOS)
		m_pLabelToast->move(spaceWidth, 73);
#else
		m_pLabelToast->move(spaceWidth, 95 + 15);
#endif
		m_pButtonToastClose->move(m_pLabelToast->width() - closeWidth - closeRight, closeTop);
	}
}

bool PLSLiveInfoNaverShoppingLIVE::isScheduleLive(const PLSScheComboxItemData &itemData) const
{
	return itemData.type == PLSScheComboxItemType::Ty_Schedule;
}

bool PLSLiveInfoNaverShoppingLIVE::eventFilter(QObject *i_Object, QEvent *i_Event)
{
	if (m_verticalScrollBar && i_Object == m_verticalScrollBar) {
		if (i_Event->type() == QEvent::Show) {
			QMargins margins = ui->verticalLayout->contentsMargins();
			margins.setRight(24 - m_verticalScrollBar->width());
			ui->verticalLayout->setContentsMargins(margins);
		} else if (i_Event->type() == QEvent::Hide) {
			QMargins margins = ui->verticalLayout->contentsMargins();
			margins.setRight(24);
			ui->verticalLayout->setContentsMargins(margins);
		}
	}

	return PLSLiveInfoBase::eventFilter(i_Object, i_Event);
}

void PLSLiveInfoNaverShoppingLIVE::titleEdited()
{
	if (m_TempPrepareInfo.isNowLiving) {
		if (QString newText = ui->lineEditTitle->text(); isTitleTooLong(newText, TitleLengthLimit)) {
			QSignalBlocker signalBlocker(ui->lineEditTitle);
			ui->lineEditTitle->setText(newText);
			PLSAlertView::warning(this, tr("Alert.Title"), QTStr("LiveInfo.Title.Length.Check.arg").arg(TitleLengthLimit).arg(QTStr("Channels.naver_shopping_live")));
		}
	}
	doUpdateOkState();
}

void PLSLiveInfoNaverShoppingLIVE::summaryEdited()
{
	if (m_TempPrepareInfo.isNowLiving) {
		if (QString newText = ui->summaryLineEdit->text(); isTitleTooLong(newText, SummaryLengthLimit)) {
			QSignalBlocker signalBlocker(ui->summaryLineEdit);
			ui->summaryLineEdit->setText(newText);
			PLSAlertView::warning(this, tr("Alert.Title"), tr("navershopping.liveinfo.max.summary.length"));
		}
	}
	doUpdateOkState();
}

void PLSLiveInfoNaverShoppingLIVE::imageSelected(const QString &)
{
	doUpdateOkState();
}

void PLSLiveInfoNaverShoppingLIVE::on_okButton_clicked()
{
	if (IsPrepareLivePage) {
		PLS_LIVE_UI_STEP(liveInfoMoudule, "NaverShopping liveinfo goLive button", ACTION_CLICK);
		m_prepareLivingType = PrepareRequestType::GoLivePrepareRequest;
	} else if (IsLiving) {
		PLS_LIVE_UI_STEP(liveInfoMoudule, "NaverShopping liveinfo update living ok button", ACTION_CLICK);
		m_prepareLivingType = PrepareRequestType::UpdateLivingPrepareRequest;
	} else {
		PLS_LIVE_UI_STEP(liveInfoMoudule, "NaverShopping liveinfo ok button", ACTION_CLICK);
		m_prepareLivingType = PrepareRequestType::LiveBeforeOkButtonPrepareRequest;
	}

	prepareLiving([this](bool result, const QByteArray &) {
		if (result) {
			accept();
		}
	});
}

void PLSLiveInfoNaverShoppingLIVE::on_rehearsalButton_clicked()
{
	PLS_LIVE_UI_STEP(liveInfoMoudule, "NaverShopping liveinfo rehearsal button", ACTION_CLICK);
	m_prepareLivingType = PrepareRequestType::RehearsalPrepareRequest;
	prepareLiving([this](bool result, const QByteArray &) {
		if (result) {
			if (!m_TempPrepareInfo.isNowLiving) {
				platform->saveCurrentScheduleRehearsalPrepareInfo();
			}
			accept();
		}
	});
}

void PLSLiveInfoNaverShoppingLIVE::on_linkButton_clicked()
{
	showToast(tr("navershopping.copy.url.tooltip"));
	auto clipboard = QGuiApplication::clipboard();
	clipboard->setText(m_TempPrepareInfo.broadcastEndUrl);
}

void PLSLiveInfoNaverShoppingLIVE::scheduleButtonClicked()
{
	PLS_UI_STEP(liveInfoMoudule, "NaverShopping liveinfo schedule pop button click", ACTION_CLICK);
	if (!ui->scheCombox->getMenuHide()) {
		return;
	}
	m_vecItemDatas.clear();

	for (int i = 0; i < 1; i++) {
		auto data = PLSScheComboxItemData();
		data.title = tr("LiveInfo.live.loading.scheduled");
		data.type = PLSScheComboxItemType::Ty_Loading;
		m_vecItemDatas.push_back(data);
	}
	auto requestCallback = [this](PLSAPINaverShoppingType apiType, const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList, int, int, const QByteArray &data) {
		this->scheduleListLoadingFinished(apiType, scheduleList, data);
	};

	ui->scheCombox->showScheduleMenu(m_vecItemDatas);
	m_requestFlag++;
	platform->getScheduleList(requestCallback, SCHEDULE_FIRST_PAGE_NUM, m_requestFlag, LIVEINFO_GET_SCHEDULE_LIST, ui->scheCombox, [](const QObject *receiver) {
		if (auto schedule = static_cast<const PLSScheduleCombox *>(receiver); schedule == nullptr || schedule->isMenuNULL() || schedule->getMenuHide()) {
			return false;
		}
		return true;
	});
}

void PLSLiveInfoNaverShoppingLIVE::scheduleItemClick(const QString selectID)
{
	PLSScheComboxItemData selelctData;
	bool hasFindSelectData = false;
	for (const PLSScheComboxItemData &data : m_vecItemDatas) {
		PLSScheComboxItemType type = data.type;
		if (type != PLSScheComboxItemType::Ty_Schedule && type != PLSScheComboxItemType::Ty_NormalLive) {
			continue;
		}
		if (0 == data._id.compare(selectID) && m_TempPrepareInfo.scheduleId.compare(selectID) != 0) {
			selelctData = data;
			hasFindSelectData = true;
			break;
		}
	}

	//save modified schduleInfo to web tool
	if (hasFindSelectData) {
		if (m_scheduleTempPrepareInfo.scheduleId.length() > 0 && isModified(m_scheduleTempPrepareInfo)) {
			checkSwitchNewScheduleItem(selelctData);
			return;
		}
		switchNewScheduleItem(selelctData.type, selelctData._id);
	}
}

void PLSLiveInfoNaverShoppingLIVE::checkSwitchNewScheduleItem(const PLSScheComboxItemData &selelctData)
{
	//Every time the reservation channel is switched, it is detected whether the reservation information has been changed before switching, and if it is changed, the user will be prompted whether to save it
	PLSAlertView::Button button = pls_alert_error_message(this, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.schedule.switch.tip"), PLSAlertView::Button::Yes | PLSAlertView::Button::No);
	if (button == PLSAlertView::Button::Yes) {

		//If the user clicks to save, first check whether the information to be saved is legal
		if (!isOkButtonEnabled()) {
			pls_alert_error_message(this, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.schedule.info.empty.tip"));
			return;
		}

		//If the information saved by the user is legal, call the update appointment API to update it
		m_prepareLivingType = PrepareRequestType::LiveBeforeUpdateSchedulePrepareRequest;
		prepareLiving([this, selelctData](bool result, const QByteArray &) {
			if (result) {
				switchNewScheduleItem(selelctData.type, selelctData._id);
			}
		});
		return;
	}

	//If the user clicks no need to save, switch directly to the new appointment channel
	switchNewScheduleItem(selelctData.type, selelctData._id);
}

void PLSLiveInfoNaverShoppingLIVE::on_cancelButton_clicked()
{
	PLS_UI_STEP(liveInfoMoudule, "NaverShopping liveinfo cancel button", ACTION_CLICK);
	reject();
}

PLSPlatformNaverShoppingLIVE *PLSLiveInfoNaverShoppingLIVE::getPlatform()
{
	return platform;
}

void PLSNaverShoppingImageScaleProcessor::process(PLSPlatformNaverShoppingLIVE *platform, double dpi, const QString &imagePath, const QSize &imageSize) const
{
	if (QString scaleImagePath; platform->getScalePixmapPath(scaleImagePath, imagePath)) {
		return;
	}

	QPixmap pixmap(imagePath);
	QPixmap scaled = pixmap.scaled(imageSize);
	QPixmap image(scaled.size());
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	QPainterPath path;
	path.addRoundedRect(image.rect(), 4, 4);
	painter.setClipPath(path);
	painter.drawPixmap(image.rect(), scaled);

	QString savePath = platform->getScaleImagePath(imagePath);
	image.save(savePath);
	platform->setScalePixmapPath(imagePath, savePath);
}

void PLSPLSNaverShoppingLIVEImageScaleThread::process()
{
	while (running()) {
		std::tuple<PLSPlatformNaverShoppingLIVE *, double, QString, QSize> resource;
		if (pop(resource) && running()) {
			processor()->process(std::get<0>(resource), std::get<1>(resource), std::get<2>(resource), std::get<3>(resource));
		}
	}
}

#if defined(Q_OS_MACOS)
QList<QWidget *> PLSLiveInfoNaverShoppingLIVE::moveContentExcludeWidgetList()
{
	QList<QWidget *> excludeChildList;
	if (m_pLabelToast) {
		excludeChildList.append(m_pLabelToast);
	}
	return excludeChildList;
}
#endif
