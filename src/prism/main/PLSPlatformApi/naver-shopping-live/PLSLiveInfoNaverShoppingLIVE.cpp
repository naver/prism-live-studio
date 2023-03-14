#include "PLSNetworkMonitor.h"
#include "PLSLiveInfoNaverShoppingLIVE.h"
#include "ui_PLSLiveInfoNaverShoppingLIVE.h"
#include "PLSLiveInfoNaverShoppingLIVEProductItemView.h"
#include "../PLSPlatformApi.h"
#include "../common/PLSDateFormate.h"
#include "alert-view.hpp"
#include "PLSChannelDataAPI.h"
#include <QtGui/qdesktopservices.h>
#include <QClipboard>
#include <QPainter>
#include "PLSDpiHelper.h"
#include "prism/PLSPlatformPrism.h"
#include "log/log.h"
#include "PLSServerStreamHandler.hpp"

#define NAVER_SHOPPING_LIVE_MIN_PHOTO_WIDTH 170
#define NAVER_SHOPPING_LIVE_MIN_PHOTO_HEIGHT 226

#define RELEASE_LEVEL_REAL "REAL"
#define RELEASE_LEVEL_REHEARSAL "TEST"

static const int TitleLengthLimit = 30;
static const int SummaryLengthLimit = 20;

#define FIRST_CATEGORY_DEFAULT_TITLE tr("navershopping.liveinfo.first.category.title")
#define SECOND_CATEGORY_DEFAULT_TITLE tr("navershopping.liveinfo.second.category.title")

namespace {
class GuideButton : public QFrame {
	bool hovered = false;
	bool pressed = false;
	std::function<void()> clicked;

public:
	GuideButton(const QString &buttonText, bool fromLeftToRight, QWidget *parent, std::function<void()> clicked_) : QFrame(parent), clicked(std::move(clicked_))
	{
		setObjectName("guideButton");
		setProperty("lang", pls_get_current_language());
		setMouseTracking(true);

		QLabel *icon = new QLabel(this);
		icon->setObjectName("guideButtonIcon");
		icon->setMouseTracking(true);
		icon->setAlignment(Qt::AlignCenter);

		QLabel *text = new QLabel(this);
		text->setObjectName("guideButtonText");
		text->setMouseTracking(true);
		text->setText(buttonText);
		text->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

		QHBoxLayout *layout = new QHBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(5);
		if (fromLeftToRight) {
			layout->addWidget(icon);
			layout->addWidget(text);
		} else {
			layout->addWidget(text);
			layout->addWidget(icon);
		}
	}
	virtual ~GuideButton() {}

private:
	void setState(const char *name, bool &state, bool value)
	{
		if (state != value) {
			pls_flush_style_recursive(this, name, state = value);
		}
	}

protected:
	virtual bool event(QEvent *event) override
	{
		switch (event->type()) {
		case QEvent::Enter:
			setState("hovered", hovered, true);
			break;
		case QEvent::Leave:
			setState("hovered", hovered, false);
			break;
		case QEvent::MouseButtonPress:
			setState("pressed", pressed, true);
			break;
		case QEvent::MouseButtonRelease:
			setState("pressed", pressed, false);
			if (rect().contains(dynamic_cast<QMouseEvent *>(event)->pos())) {
				clicked();
			}
			break;
		case QEvent::MouseMove: {
			setState("hovered", hovered, rect().contains(dynamic_cast<QMouseEvent *>(event)->pos()));
			break;
		}
		}
		return QFrame::event(event);
	}
};
}

static const char *liveInfoMoudule = "PLSLiveInfoNaverShoppingLIVE";

#define IsLiving PLSCHANNELS_API->isLiving()
#define IsPrepareLivePage PLS_PLATFORM_API->isPrepareLive()
#define ThisIsValid [](QObject *receiver) -> bool { return naverShoppingLIVELiveInfos.contains(receiver); }

static QList<QObject *> naverShoppingLIVELiveInfos;

PLSLiveInfoNaverShoppingLIVE::PLSLiveInfoNaverShoppingLIVE(PLSPlatformBase *pPlatformBase, const QVariantMap &info, QWidget *parent, PLSDpiHelper dpiHelper)
	: PLSLiveInfoBase(pPlatformBase, parent, dpiHelper), ui(new Ui::PLSLiveInfoNaverShoppingLIVE), platform(dynamic_cast<PLSPlatformNaverShoppingLIVE *>(pPlatformBase)), srcInfo(info)
{
	naverShoppingLIVELiveInfos.push_front(this);
	imageScaleThread = new PLSPLSNaverShoppingLIVEImageScaleThread();
	imageScaleThread->start();
	dpiHelper.setCss(this, {PLSCssIndex::PLSLiveInfoNaverShoppingLIVE});
	dpiHelper.updateCssWithParent(this);

	setAttribute(Qt::WA_AlwaysShowToolTips);
	PLS_INFO(liveInfoMoudule, "Naver Shopping LIVE liveinfo Will show");
	platform->setAlertParent(this);
	ui->setupUi(this->content());
	QMetaObject::connectSlotsByName(this);
	dpiHelper.updateCssWithParent2(ui->thumbnaiBg, ui->scheCombox, ui->yearButton, ui->hourButton, ui->minuteButton, ui->apButton, ui->shareFirstObject, ui->shareSecondObject);

	auto lang = pls_get_current_language();
	if (!IS_KR()) {
		lang = "en-US";
	}
	ui->yearButton->setProperty("lang", lang);

	ui->thumbnailButton->setImageSize(QSize(NAVER_SHOPPING_LIVE_MIN_PHOTO_WIDTH, NAVER_SHOPPING_LIVE_MIN_PHOTO_HEIGHT));
	ui->guideLabel->setHidden(true);
	setupData();
	setupGuideButton();
	setupLineEdit();
	setupThumbnailButton();
	setupScheduleComboBox();
	setupCategoryButton();
	setupDateComboBox();
	setupProductList();
	setupEventFilter();
	initSetupUI();
	ui->rehearsalButton->setVisible(PLS_PLATFORM_API->isPrepareLive());
	updateStepTitle(ui->okButton);
	getCategoryListRequest();
	if (PLS_PLATFORM_API->isPrepareLive()) {
		ui->horizontalLayout->addWidget(ui->okButton);
	}
	notifyDpiChangedBefore([this](bool firstShow) {
		if (!firstShow) {
			ui->productWidget->resetIconSize();
		}
	});
	PLSDpiHelper::setDynamicContentsMargins(ui->verticalLayout, true);
	dpiHelper.notifyDpiChanged(this, [this](double dpi) {
		updateContentMargins(dpi);
		adjustToastSize();
		ui->productWidget->updateWhenDpiChanged();
	});

	updateContentMargins(PLSDpiHelper::getDpi(parent));
}

PLSLiveInfoNaverShoppingLIVE::~PLSLiveInfoNaverShoppingLIVE()
{
	naverShoppingLIVELiveInfos.removeOne(this);
	delete ui;
	delete imageScaleThread;
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
	QWidget *widget = new QWidget;
	widget->setObjectName("guideBackView");

	QWidget *manageButton = new GuideButton(tr("NaverShoppingLive.LiveInfo.ScheNoProductGoToNST"), false, nullptr, [this]() { QDesktopServices::openUrl(CHANNEL_NAVER_SHOPPING_LIVE_LOGIN); });
	manageButton->setObjectName("manageButton");

	QWidget *lineView = new QWidget(nullptr);
	lineView->setObjectName("lineView");

	QHBoxLayout *layout = new QHBoxLayout(widget);
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
	ui->summaryLineEdit->setEnabled(!PLS_PLATFORM_API->isLiving());
	ui->liveTitleLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("LiveInfo.base.Title")));
	ui->summaryLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("navershopping.liveinfo.summary.title")));
	ui->categoryLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("navershopping.liveinfo.category.title")));
	ui->dateLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("navershopping.liveinfo.date.title")));
	connect(ui->lineEditTitle, &QLineEdit::textChanged, this, &PLSLiveInfoNaverShoppingLIVE::titleEdited, Qt::QueuedConnection);
	connect(ui->summaryLineEdit, &QLineEdit::textChanged, this, &PLSLiveInfoNaverShoppingLIVE::summaryEdited, Qt::QueuedConnection);
}

void PLSLiveInfoNaverShoppingLIVE::setupThumbnailButton()
{
	connect(ui->thumbnailButton, &PLSSelectImageButton::imageSelected, this, &PLSLiveInfoNaverShoppingLIVE::imageSelected);
}

void PLSLiveInfoNaverShoppingLIVE::setupCategoryButton()
{
	connect(ui->shareFirstObject, &PLSLoadingCombox::pressed, this, [=] { ui->shareFirstObject->showTitlesView(platform->getFirstCategoryTitleList()); });
	connect(ui->shareFirstObject, &PLSLoadingCombox::clickItemIndex, this, [=](int showIndex) {
		PLSNaverShoppingLIVEAPI::LiveCategory firstCategory = platform->getCategoryList().at(showIndex);
		ui->shareFirstObject->setComboBoxTitleData(firstCategory.displayName);
		ui->shareSecondObject->setDisabled(firstCategory.children.isEmpty());
		ui->shareSecondObject->setComboBoxTitleData(SECOND_CATEGORY_DEFAULT_TITLE);
		m_TempPrepareInfo.firstCategoryName = firstCategory.displayName;
		doUpdateOkState();
	});
	connect(ui->shareSecondObject, &PLSLoadingCombox::pressed, this,
		[=] { ui->shareSecondObject->showTitlesView(platform->getSecondCategoryTitleList(ui->shareFirstObject->getComboBoxTitle())); });
	connect(ui->shareSecondObject, &PLSLoadingCombox::clickItemIndex, this, [=](int showIndex) {
		QString firstTitle = ui->shareFirstObject->getComboBoxTitle();
		QString secondTitle = platform->getSecondCategoryTitleList(firstTitle).at(showIndex);
		ui->shareSecondObject->setComboBoxTitleData(secondTitle);
		m_TempPrepareInfo.secondCategoryName = secondTitle;
		doUpdateOkState();
	});
	ui->yearButton->setEnabled(!PLS_PLATFORM_API->isLiving());
	ui->hourButton->setEnabled(!PLS_PLATFORM_API->isLiving());
	ui->minuteButton->setEnabled(!PLS_PLATFORM_API->isLiving());
	ui->apButton->setEnabled(!PLS_PLATFORM_API->isLiving());
	connect(ui->yearButton, &QPushButton::toggled, this, [=](bool checked) {
		if (checked) {
			ui->yearButton->showDateCalender();
		}
	});
	connect(ui->yearButton, &PLSShoppingCalenderCombox::clickDate, this, [=](const QDate &date) {
		m_TempPrepareInfo.ymdDate = date;
		QDateTime dateTime = QDateTime(date);
		QString yearMonthDay;
		PLSNaverShoppingLIVEAPI::getNaverShoppingDateFormat(dateTime.toTime_t(), yearMonthDay);
		ui->yearButton->setDate(date);
		ui->yearButton->setText(yearMonthDay);
	});
	connect(ui->hourButton, &QPushButton::toggled, this, [=](bool checked) {
		if (checked) {
			ui->hourButton->showHour(ui->hourButton->text());
		}
	});
	connect(ui->hourButton, &PLSShoppingCalenderCombox::clickTime, this, [=](QString time) {
		m_TempPrepareInfo.hour = time;
		ui->hourButton->setText(time);
	});
	connect(ui->minuteButton, &QPushButton::toggled, this, [=](bool checked) {
		if (checked) {
			ui->minuteButton->showMinute(ui->minuteButton->text());
		}
	});
	connect(ui->minuteButton, &PLSShoppingCalenderCombox::clickTime, this, [=](QString time) {
		m_TempPrepareInfo.minute = time;
		ui->minuteButton->setText(time);
	});
	connect(ui->apButton, &QPushButton::toggled, this, [=](bool checked) {
		if (checked) {
			ui->apButton->showAp(ui->apButton->text());
		}
	});
	connect(ui->apButton, &PLSShoppingCalenderCombox::clickTime, this, [=](QString time) {
		int index = apIndexForString(time);
		if (index >= 0) {
			m_TempPrepareInfo.ap = index;
			ui->apButton->setText(time);
		}
	});
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
	QSizePolicy sizePolicy = ui->dateItemWidget->sizePolicy();
	sizePolicy.setRetainSizeWhenHidden(false);
	ui->dateItemWidget->setSizePolicy(sizePolicy);
	ui->horizontalLayout_8->setAlignment(Qt::AlignLeft);
	if (!IS_KR()) {
		ui->horizontalLayout_8->addWidget(ui->apButton);
	}
}

void PLSLiveInfoNaverShoppingLIVE::prepareLiving(std::function<void(bool)> callback)
{
	//check first reservation date is valid
	if (!m_TempPrepareInfo.isNowLiving && m_prepareLivingType != PrepareRequestType::UpdateLivingPrepareRequest) {
		QStringList apList = ui->apButton->apList();
		int apIndex = apList.indexOf(ui->apButton->text());
		uint timeStamp = PLSNaverShoppingLIVEAPI::getNaverShoppingLocalTimeStamp(ui->yearButton->date(), ui->hourButton->text(), ui->minuteButton->text(), apIndex);
		QDateTime currTime = QDateTime::currentDateTime();
		currTime = currTime.addDays(-14);
		uint filterTimeStamp = currTime.toTime_t();
		if (timeStamp < filterTimeStamp) {
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.set.invalid.date"));
			callback(false);
			return;
		}
	}

	//click golive button,check term of agree and use term
	if (m_prepareLivingType == PrepareRequestType::GoLivePrepareRequest) {
		if (!platform->checkNaverShoppingTermOfAgree(true)) {
			return;
		}
		platform->checkNaverShoopingNotes(true);
	}

	//now living and live before not upload image
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
	auto requestCallback = [=](PLSAPINaverShoppingType apiType, const QString &imageURL) {
		hideLoading();
		m_TempPrepareInfo.standByImageURL = imageURL;
		m_TempPrepareInfo.standByImagePath = ui->thumbnailButton->getImagePath();
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			startLiving(callback);
			return;
		}
		saveLiveInfo();
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
			apiType = PLSAPINaverShoppingType::PLSNaverShoppingUploadImageFailed;
			platform->handleCommonApiType(apiType);
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
	prepareInfo.broadcastEndUrl = m_TempPrepareInfo.broadcastEndUrl;
	if (m_prepareLivingType == PrepareRequestType::GoLivePrepareRequest) {
		prepareInfo.releaseLevel = RELEASE_LEVEL_REAL;
	} else if (m_prepareLivingType == PrepareRequestType::RehearsalPrepareRequest) {
		prepareInfo.releaseLevel = RELEASE_LEVEL_REHEARSAL;
	} else {
		prepareInfo.releaseLevel = m_TempPrepareInfo.releaseLevel;
	}
	prepareInfo.ymdDate = ui->yearButton->date();
	prepareInfo.yearMonthDay = ui->yearButton->text();
	prepareInfo.hour = ui->hourButton->text();
	prepareInfo.minute = ui->minuteButton->text();
	prepareInfo.ap = apIndexForString(ui->apButton->text());
	prepareInfo.description = ui->summaryLineEdit->text();
	prepareInfo.firstCategoryName = getFirstCategoryTitle();
	prepareInfo.secondCategoryName = getSecondCategoryTitle();
	platform->setPrepareInfo(prepareInfo);
}

void PLSLiveInfoNaverShoppingLIVE::startLiving(std::function<void(bool)> callback)
{
	if (m_prepareLivingType == PrepareRequestType::GoLivePrepareRequest || m_prepareLivingType == PrepareRequestType::RehearsalPrepareRequest) {
		if (!m_TempPrepareInfo.isNowLiving && isModified(m_scheduleTempPrepareInfo)) {
			updateScheduleLiveInfoRequest([=](bool result) {
				if (!result) {
					return;
				}
				createLivingRequest(callback);
			});
			return;
		}
		createLivingRequest(callback);
	} else if (m_prepareLivingType == PrepareRequestType::UpdateLivingPrepareRequest) {
		auto requestCallback = [=](PLSAPINaverShoppingType apiType) {
			hideLoading();
			if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
				callback(true);
				return;
			}
			if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
				apiType = PLSAPINaverShoppingType::PLSNaverShoppingUpdateFailed;
				platform->handleCommonApiType(apiType);
			}
		};
		showLoading(content());
		saveLiveInfo();
		platform->updateLiving(requestCallback, platform->getLivingInfo().id, this, ThisIsValid);
	} else {
		if (!m_TempPrepareInfo.isNowLiving) {
			if (isModified(m_scheduleTempPrepareInfo)) {
				updateScheduleLiveInfoRequest(callback);
			} else {
				saveLiveInfo();
				callback(true);
			}
			platform->setShareLink(m_TempPrepareInfo.broadcastEndUrl);
			return;
		}
		platform->setShareLink(QString());
		saveLiveInfo();
		callback(true);
	}
}

bool PLSLiveInfoNaverShoppingLIVE::isProductListChanged(const QList<PLSNaverShoppingLIVEAPI::ProductInfo> &oldProductList)
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
			if (newKey != oldKey || newPresent != oldPresent) {
				productChanged = true;
				break;
			}
		}
	}
	return productChanged;
}

void PLSLiveInfoNaverShoppingLIVE::getPrepareInfoFromScheduleInfo(PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &prepareInfo, const PLSNaverShoppingLIVEAPI::ScheduleInfo &scheduleInfo)
{
	prepareInfo.title = scheduleInfo.title;
	prepareInfo.standByImagePath = scheduleInfo.standByImagePath;
	prepareInfo.standByImageURL = scheduleInfo.standByImage;
	prepareInfo.shoppingProducts = scheduleInfo.shoppingProducts;
	prepareInfo.isNowLiving = false;
	prepareInfo.scheduleId = scheduleInfo.id;
	prepareInfo.broadcastEndUrl = scheduleInfo.broadcastEndUrl;
	prepareInfo.releaseLevel = scheduleInfo.releaseLevel;
	QString apString;
	PLSNaverShoppingLIVEAPI::getNaverShoppingDateFormat(scheduleInfo.timeStamp, prepareInfo.ymdDate, prepareInfo.yearMonthDay, prepareInfo.hour, prepareInfo.minute, apString);
	prepareInfo.ap = apIndexForString(apString);
	prepareInfo.description = scheduleInfo.description;
	QString firstCategoryName;
	QString secondCategoryName;
	QString displayName = scheduleInfo.displayCategory.displayName;
	QString parentId = scheduleInfo.displayCategory.parentId;
	platform->getCategoryName(firstCategoryName, secondCategoryName, displayName, parentId);
	prepareInfo.firstCategoryName = firstCategoryName;
	prepareInfo.secondCategoryName = secondCategoryName;
}

void PLSLiveInfoNaverShoppingLIVE::updateContentMargins(double dpi)
{
	int _25px = PLSDpiHelper::calculate(dpi, 25);
	if (m_verticalScrollBar->isVisible()) {
		ui->verticalLayout->setContentsMargins(QMargins(_25px, 0, _25px - m_verticalScrollBar->width(), 0));
	} else {
		ui->verticalLayout->setContentsMargins(QMargins(_25px, 0, _25px, 0));
	}
}

bool PLSLiveInfoNaverShoppingLIVE::isTitleTooLong(QString &title, int maxWordCount)
{
	bool isToLong = false;
	QVector<uint> ucs4 = title.toUcs4();
	int length = ucs4.count();
	if (length > maxWordCount) {
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
	isTitleTooLong(title, TitleLengthLimit);
	ui->lineEditTitle->setText(title);
	ui->linkButton->setHidden(m_TempPrepareInfo.isNowLiving);
}

void PLSLiveInfoNaverShoppingLIVE::updateLivePhotoUI()
{
	QString originImagePath = m_TempPrepareInfo.standByImagePath;
	QString resultPath;
	if (platform->getScalePixmapPath(resultPath, originImagePath)) {
		ui->thumbnailButton->setImagePath(resultPath);
	} else {
		ui->thumbnailButton->setImagePath(originImagePath);
	}
}

void PLSLiveInfoNaverShoppingLIVE::updateLiveSummaryUI()
{
	ui->summaryLineEdit->setText(m_TempPrepareInfo.description);
}

void PLSLiveInfoNaverShoppingLIVE::updateLiveCategoryUI()
{
	QStringList firstTitleList = platform->getFirstCategoryTitleList();
	if (firstTitleList.count() > 0) {
		if (firstTitleList.contains(m_TempPrepareInfo.firstCategoryName)) {
			ui->shareFirstObject->setComboBoxTitleData(m_TempPrepareInfo.firstCategoryName);
		} else {
			ui->shareFirstObject->setComboBoxTitleData(FIRST_CATEGORY_DEFAULT_TITLE);
		}
		bool enabled = true;
		if (IsLiving) {
			enabled = false;
		}
		ui->shareFirstObject->setEnabled(enabled);
	} else {
		if (m_TempPrepareInfo.firstCategoryName.isEmpty()) {
			ui->shareFirstObject->setComboBoxTitleData(FIRST_CATEGORY_DEFAULT_TITLE);
		} else {
			ui->shareFirstObject->setComboBoxTitleData(m_TempPrepareInfo.firstCategoryName);
		}
		ui->shareFirstObject->setEnabled(false);
	}

	QString firstTitle = ui->shareFirstObject->getComboBoxTitle();
	QStringList secondTitleList = platform->getSecondCategoryTitleList(firstTitle);
	if (secondTitleList.count() > 0) {
		if (secondTitleList.contains(m_TempPrepareInfo.secondCategoryName)) {
			ui->shareSecondObject->setComboBoxTitleData(m_TempPrepareInfo.secondCategoryName);
		} else {
			ui->shareSecondObject->setComboBoxTitleData(SECOND_CATEGORY_DEFAULT_TITLE);
		}
		bool enabled = true;
		if (IsLiving) {
			enabled = false;
		}
		ui->shareSecondObject->setEnabled(enabled);
	} else {
		if (m_TempPrepareInfo.secondCategoryName.isEmpty()) {
			ui->shareSecondObject->setComboBoxTitleData(SECOND_CATEGORY_DEFAULT_TITLE);
		} else {
			ui->shareSecondObject->setComboBoxTitleData(m_TempPrepareInfo.secondCategoryName);
		}
		ui->shareSecondObject->setEnabled(false);
	}
}

void PLSLiveInfoNaverShoppingLIVE::updateLiveDateUI()
{
	ui->dateItemWidget->setHidden(m_TempPrepareInfo.isNowLiving);
	ui->yearButton->setText(m_TempPrepareInfo.yearMonthDay);
	ui->yearButton->setDate(m_TempPrepareInfo.ymdDate);
	ui->hourButton->setText(m_TempPrepareInfo.hour);
	ui->minuteButton->setText(m_TempPrepareInfo.minute);
	QString apString;
	QStringList apList = ui->apButton->apList();
	if (m_TempPrepareInfo.ap >= 0 && m_TempPrepareInfo.ap < apList.count()) {
		apString = apList.at(m_TempPrepareInfo.ap);
	}
	ui->apButton->setText(apString);

	QString comboboxTitle = m_TempPrepareInfo.title;
	if (m_TempPrepareInfo.isNowLiving) {
		comboboxTitle = tr("New");
	}
	QString startTimeShort;
	if (ui->yearButton->text().length() > 0 && ui->hourButton->text().length() > 0 && ui->minuteButton->text().length() > 0 && ui->apButton->text().length() > 0) {
		uint timeStamp = PLSNaverShoppingLIVEAPI::getNaverShoppingLocalTimeStamp(ui->yearButton->date(), ui->hourButton->text(), ui->minuteButton->text(), m_TempPrepareInfo.ap);
		startTimeShort = PLSDateFormate::timeStampToShortString(timeStamp);
	}
	ui->scheCombox->setupButton(comboboxTitle, startTimeShort);
}

QString PLSLiveInfoNaverShoppingLIVE::getFirstCategoryTitle()
{
	QString firstCategoryName;
	if (ui->shareFirstObject->getComboBoxTitle() != FIRST_CATEGORY_DEFAULT_TITLE) {
		firstCategoryName = ui->shareFirstObject->getComboBoxTitle();
	}
	return firstCategoryName;
}

QString PLSLiveInfoNaverShoppingLIVE::getSecondCategoryTitle()
{
	QString secondCategoryName;
	if (ui->shareSecondObject->getComboBoxTitle() != SECOND_CATEGORY_DEFAULT_TITLE) {
		secondCategoryName = ui->shareSecondObject->getComboBoxTitle();
	}
	return secondCategoryName;
}

void PLSLiveInfoNaverShoppingLIVE::switchNewScheduleItem(PLSScheComboxItemType type, QString id)
{
	//switch from nowLiving to schedule,save the nowLiving data
	if (type == PLSScheComboxItemType::Ty_NormalLive) {
		m_TempPrepareInfo = m_normalTempPrepareInfo;
		m_TempPrepareInfo.isNowLiving = true;
		m_scheduleTempPrepareInfo = PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo();
	} else {
		//switch from nowliving to schdule,need save now living data
		if (m_TempPrepareInfo.isNowLiving) {
			m_normalTempPrepareInfo.title = ui->lineEditTitle->text();
			m_normalTempPrepareInfo.description = ui->summaryLineEdit->text();
			m_normalTempPrepareInfo.standByImagePath = ui->thumbnailButton->getImagePath();
			m_normalTempPrepareInfo.firstCategoryName = getFirstCategoryTitle();
			m_normalTempPrepareInfo.secondCategoryName = getSecondCategoryTitle();
			m_normalTempPrepareInfo.shoppingProducts = ui->productWidget->getAllProducts();
		}
		PLSNaverShoppingLIVEAPI::ScheduleInfo scheduleInfo = platform->getSelectedScheduleInfo(id);
		getPrepareInfoFromScheduleInfo(m_TempPrepareInfo, scheduleInfo);
		m_scheduleTempPrepareInfo = m_TempPrepareInfo;
		if (m_TempPrepareInfo.standByImagePath.isEmpty()) {
			createToastWidget();
			m_pLabelToast->setText(tr("navershopping.liveinfo.load.thumbnail.failed"));
			adjustToastSize();
		}
	}
	updateSetupUI();
}

void PLSLiveInfoNaverShoppingLIVE::updateScheduleLiveInfoRequest(std::function<void(bool)> callback)
{
	auto requestCallback = [=](PLSAPINaverShoppingType apiType) {
		hideLoading();
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			callback(true);
			return;
		}
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
			apiType = PLSAPINaverShoppingType::PLSNaverShoppingUpdateScheduleInfoFailed;
			platform->handleCommonApiType(apiType);
		}
		callback(false);
	};
	showLoading(content());
	saveLiveInfo();
	platform->updateLiving(requestCallback, m_TempPrepareInfo.scheduleId, this, ThisIsValid);
}

void PLSLiveInfoNaverShoppingLIVE::createLivingRequest(std::function<void(bool)> callback)
{
	auto requestCallback = [=](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &) {
		if (apiType != PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
			hideLoading();
			if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
				apiType = PLSAPINaverShoppingType::PLSNaverShoppingCreateLivingFailed;
				platform->handleCommonApiType(apiType);
			}
			return;
		}
		platform->checkPushNotification([=]() {
			hideLoading();
			callback(true);
		});
	};
	showLoading(content());
	saveLiveInfo();
	platform->createLiving(requestCallback, this, ThisIsValid);
}

int PLSLiveInfoNaverShoppingLIVE::apIndexForString(const QString &apString)
{
	QStringList apList = ui->apButton->apList();
	int index = -1;
	if (apList.contains(apString)) {
		index = apList.indexOf(apString);
	}
	return index;
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
	connect(
		platform, &PLSPlatformNaverShoppingLIVE::showLiveinfoLoading, this, [=]() { showLoading(content()); }, Qt::QueuedConnection);
	connect(
		platform, &PLSPlatformNaverShoppingLIVE::hiddenLiveinfoLoading, this, [=]() { hideLoading(); }, Qt::QueuedConnection);
	connect(
		platform, &PLSPlatformNaverShoppingLIVE::closeDialogByExpired, this,
		[=]() {
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

	//LiveInfo category
	updateLiveCategoryUI();

	//LiveInfo product
	ui->productWidget->setIsScheduleLive(!m_TempPrepareInfo.isNowLiving);
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

	//update ok button state
	doUpdateOkState();
}

void PLSLiveInfoNaverShoppingLIVE::updateRequest()
{
	if (IsLiving) {
		auto requestCallback = [=](PLSAPINaverShoppingType apiType, const PLSNaverShoppingLIVEAPI::NaverShoppingLivingInfo &) {
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
		auto RequestCallback = [=](PLSAPINaverShoppingType apiType, const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList) {
			hideLoading();
			if (apiType != PLSAPINaverShoppingType::PLSNaverShoppingSuccess) {
				ui->productWidget->setIsScheduleLiveLoading(false);
				ui->productWidget->setAllProducts(m_TempPrepareInfo.shoppingProducts, false);
				doUpdateOkState();
				return;
			}
			bool contains = false;
			PLSNaverShoppingLIVEAPI::ScheduleInfo tempInfo;
			for (PLSNaverShoppingLIVEAPI::ScheduleInfo info : scheduleList) {
				if (info.id == m_TempPrepareInfo.scheduleId) {
					tempInfo = info;
					contains = true;
					break;
				}
			}
			if (contains) {
				//update liveInfo prepareInfo data
				auto tempPrepareInfo = platform->getPrepareLiveInfo();
				getPrepareInfoFromScheduleInfo(tempPrepareInfo, tempInfo);
				platform->setPrepareInfo(tempPrepareInfo);
			} else {
				platform->clearLiveInfo();
			}
			initSetupUI(true);
		};
		showLoading(content());
		platform->getScheduleList(RequestCallback, this, ThisIsValid);
	}
}

void PLSLiveInfoNaverShoppingLIVE::getCategoryListRequest()
{
	if (!PLSNetworkMonitor::Instance()->IsInternetAvailable()) {
		ui->productWidget->setAllProducts(m_TempPrepareInfo.shoppingProducts, false);
		updateLiveCategoryUI();
		doUpdateOkState();
		return;
	}

	auto categoryCallback = [=](PLSAPINaverShoppingType apiType, const QList<PLSNaverShoppingLIVEAPI::LiveCategory> & /*categoryList*/) {
		if (m_refreshProductList) {
			hideLoading();
		}
		updateLiveCategoryUI();
		doUpdateOkState();
		if (apiType == PLSAPINaverShoppingType::PLSNaverShoppingFailed) {
			apiType = PLSAPINaverShoppingType::PLSNaverShoppingCategoryListFailed;
			platform->handleCommonApiType(apiType);
		}
	};
	if (m_refreshProductList) {
		showLoading(content());
	} else {
		updateRequest();
	}
	platform->getCategoryList(categoryCallback, this, ThisIsValid);
}

void PLSLiveInfoNaverShoppingLIVE::updateSetupUI()
{

	//liveInfo title
	updateLiveTitleUI();

	//LiveInfo Profile
	updateLivePhotoUI();

	//LiveInfo summary
	updateLiveSummaryUI();

	//LiveInfo category
	updateLiveCategoryUI();

	//LiveInfo product
	QList<PLSNaverShoppingLIVEAPI::ProductInfo> list = m_TempPrepareInfo.shoppingProducts;
	ui->productWidget->setIsScheduleLive(!m_TempPrepareInfo.isNowLiving);
	ui->productWidget->setAllProducts(list);

	//LiveInfo date
	updateLiveDateUI();

	//update ok button state
	doUpdateOkState();
}

void PLSLiveInfoNaverShoppingLIVE::doUpdateOkState()
{
	bool enabled = isOkButtonEnabled();
	ui->okButton->setEnabled(enabled);
	if (!m_TempPrepareInfo.isNowLiving) {
		ui->rehearsalButton->setEnabled(false);
	} else {
		ui->rehearsalButton->setEnabled(enabled);
	}
}

bool PLSLiveInfoNaverShoppingLIVE::isModified(const PLSNaverShoppingLIVEAPI::NaverShoppingPrepareLiveInfo &info)
{
	bool imagePathChanged = true;
	QString bigStandByImagePath = info.standByImagePath;
	QString scaleStandByImagePath = platform->getScaleImagePath(bigStandByImagePath);
	if (bigStandByImagePath == ui->thumbnailButton->getImagePath() || scaleStandByImagePath == ui->thumbnailButton->getImagePath()) {
		imagePathChanged = false;
	}

	if (imagePathChanged) {
		return true;
	}

	bool titleChanged = info.title != ui->lineEditTitle->text();
	if (titleChanged) {
		return true;
	}

	bool summaryChanged = info.description != ui->summaryLineEdit->text();
	if (summaryChanged) {
		return true;
	}

	bool categoryNotChanged = m_scheduleTempPrepareInfo.firstCategoryName == getFirstCategoryTitle() && m_scheduleTempPrepareInfo.secondCategoryName == getSecondCategoryTitle();
	if (!categoryNotChanged) {
		return true;
	}

	bool productChanged = isProductListChanged(info.shoppingProducts);
	if (productChanged) {
		return true;
	}

	QStringList apList = ui->apButton->apList();
	int apIndex = apList.indexOf(ui->apButton->text());
	bool dateNotChanged = info.yearMonthDay == ui->yearButton->text() && info.hour == ui->hourButton->text() && info.minute == ui->minuteButton->text() && info.ap == apIndex;
	if (!dateNotChanged) {
		return true;
	}

	return false;
}

bool PLSLiveInfoNaverShoppingLIVE::isOkButtonEnabled()
{
	bool enabled = true;
	if (ui->thumbnailButton->getImagePath().isEmpty()) {
		enabled = false;
	} else if (ui->lineEditTitle->text().trimmed().isEmpty()) {
		enabled = false;
	} else if (ui->summaryLineEdit->text().trimmed().isEmpty()) {
		enabled = false;
	} else if (ui->productWidget->getAllProducts().count() == 0) {
		enabled = false;
	} else {
		QString firstCategoryTitle = ui->shareFirstObject->getComboBoxTitle();
		QString secondCategoryTitle = ui->shareSecondObject->getComboBoxTitle();
		if (platform->getFirstCategoryTitleList().contains(firstCategoryTitle)) {
			QStringList secondTitleList = platform->getSecondCategoryTitleList(firstCategoryTitle);
			if (secondTitleList.count() > 0 && !secondTitleList.contains(secondCategoryTitle)) {
				enabled = false;
			}
		} else {
			enabled = false;
		}
	}
	return enabled;
}

void PLSLiveInfoNaverShoppingLIVE::createToastWidget()
{
	if (m_pLabelToast.isNull()) {
		m_pLabelToast = new QLabel(this, Qt::SubWindow | Qt::FramelessWindowHint);
		m_pLabelToast->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		m_pLabelToast->setObjectName("labelToast");
		m_pLabelToast->setWordWrap(true);
		m_pLabelToast->show();
		m_pLabelToast->raise();

		m_pButtonToastClose = new QPushButton(m_pLabelToast);
		m_pButtonToastClose->setObjectName("pushButtonClear");
		m_pButtonToastClose->show();

		QTimer::singleShot(5000, m_pLabelToast, &QObject::deleteLater);
		connect(m_pButtonToastClose, &QPushButton::clicked, m_pLabelToast, &QObject::deleteLater);
	}
}

void PLSLiveInfoNaverShoppingLIVE::adjustToastSize()
{
	if (!m_pLabelToast.isNull()) {
		int spaceWidth = PLSDpiHelper::calculate(this, 25);
		int closeTop = PLSDpiHelper::calculate(this, 13);
		int closeWidth = PLSDpiHelper::calculate(this, 12);
		int closeRight = PLSDpiHelper::calculate(this, 14);
		m_pLabelToast->setFixedSize(this->width() - 2 * spaceWidth, PLSDpiHelper::calculate(this, 40));
		m_pLabelToast->move(spaceWidth, PLSDpiHelper::calculate(this, 107));
		m_pButtonToastClose->move(m_pLabelToast->width() - closeWidth - closeRight, closeTop);
	}
}

bool PLSLiveInfoNaverShoppingLIVE::isScheduleLive(const PLSScheComboxItemData &itemData)
{
	return itemData.type == PLSScheComboxItemType::Ty_Schedule;
}

bool PLSLiveInfoNaverShoppingLIVE::eventFilter(QObject *i_Object, QEvent *i_Event)
{
	if (m_verticalScrollBar && i_Object == m_verticalScrollBar) {
		switch (i_Event->type()) {
		case QEvent::Show: {
			QMargins margins = ui->verticalLayout->contentsMargins();
			margins.setRight(PLSDpiHelper::calculate(this, 24) - m_verticalScrollBar->width());
			ui->verticalLayout->setContentsMargins(margins);
			break;
		}
		case QEvent::Hide: {
			QMargins margins = ui->verticalLayout->contentsMargins();
			margins.setRight(PLSDpiHelper::calculate(this, 24));
			ui->verticalLayout->setContentsMargins(margins);
			break;
		}
		}
	}
	return PLSLiveInfoBase::eventFilter(i_Object, i_Event);
}

void PLSLiveInfoNaverShoppingLIVE::titleEdited()
{
	QString newText = ui->lineEditTitle->text();
	if (isTitleTooLong(newText, TitleLengthLimit)) {
		QSignalBlocker signalBlocker(ui->lineEditTitle);
		ui->lineEditTitle->setText(newText);
		PLSAlertView::warning(this, tr("Alert.Title"), tr("navershopping.max.title.length"));
	}
	doUpdateOkState();
}

void PLSLiveInfoNaverShoppingLIVE::summaryEdited()
{
	QString newText = ui->summaryLineEdit->text();
	if (isTitleTooLong(newText, SummaryLengthLimit)) {
		QSignalBlocker signalBlocker(ui->summaryLineEdit);
		ui->summaryLineEdit->setText(newText);
		PLSAlertView::warning(this, tr("Alert.Title"), tr("navershopping.liveinfo.max.summary.length"));
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
		if (!PLSServerStreamHandler::instance()->isValidWatermark()) {
			PLSAlertView::warning(nullptr, QTStr("Alert.Title"), QTStr("watermark.resource.is.not.existed.tip"));
			return;
		}
	} else if (IsLiving) {
		PLS_LIVE_UI_STEP(liveInfoMoudule, "NaverShopping liveinfo update living ok button", ACTION_CLICK);
		m_prepareLivingType = PrepareRequestType::UpdateLivingPrepareRequest;
	} else {
		PLS_LIVE_UI_STEP(liveInfoMoudule, "NaverShopping liveinfo ok button", ACTION_CLICK);
		m_prepareLivingType = PrepareRequestType::LiveBeforeOkButtonPrepareRequest;
	}
	prepareLiving([=](bool result) {
		if (result) {
			accept();
		}
	});
}

void PLSLiveInfoNaverShoppingLIVE::on_rehearsalButton_clicked()
{
	PLS_LIVE_UI_STEP(liveInfoMoudule, "NaverShopping liveinfo rehearsal button", ACTION_CLICK);
	m_prepareLivingType = PrepareRequestType::RehearsalPrepareRequest;
	prepareLiving([=](bool result) {
		if (result) {
			accept();
		}
	});
}

void PLSLiveInfoNaverShoppingLIVE::on_linkButton_clicked()
{
	createToastWidget();
	m_pLabelToast->setText(tr("navershopping.copy.url.tooltip"));
	adjustToastSize();
	auto clipboard = QGuiApplication::clipboard();
	clipboard->setText(m_TempPrepareInfo.broadcastEndUrl);
}

void PLSLiveInfoNaverShoppingLIVE::scheduleButtonClicked()
{
	PLS_UI_STEP(liveInfoMoudule, "NaverShopping liveinfo schedule pop button click", ACTION_CLICK);
	if (!ui->scheCombox->getMenuHide()) {
		return;
	}
	auto pPlatformNaverShopping = PLS_PLATFORM_NAVERSHOPPING;

	m_vecItemDatas.clear();
	for (int i = 0; i < 1; i++) {
		PLSScheComboxItemData data = PLSScheComboxItemData();
		data.title = tr("LiveInfo.live.loading.scheduled");
		data.type = PLSScheComboxItemType::Ty_Loading;
		m_vecItemDatas.push_back(data);
	}
	auto RequestCallback = [=](PLSAPINaverShoppingType, const QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> &scheduleList) {
		if (ui->scheCombox == nullptr || ui->scheCombox->isMenuNULL() || ui->scheCombox->getMenuHide()) {
			return;
		}

		m_vecItemDatas.clear();
		QList<PLSNaverShoppingLIVEAPI::ScheduleInfo> list = scheduleList;
		double dpi = PLSDpiHelper::getDpi(this);
		QDateTime currTime = QDateTime::currentDateTime();
		currTime = currTime.addDays(-14);
		uint filterTimeStamp = currTime.toTime_t();
		std::sort(list.begin(), list.end(), [=](PLSNaverShoppingLIVEAPI::ScheduleInfo &lhs, PLSNaverShoppingLIVEAPI::ScheduleInfo &rhs) { return lhs.timeStamp > rhs.timeStamp; });
		for (PLSNaverShoppingLIVEAPI::ScheduleInfo info : list) {
			uint nowTime = PLSDateFormate::getNowTimeStamp();
			uint scheduleTime = info.timeStamp;
			if (scheduleTime < filterTimeStamp || info.releaseLevel == RELEASE_LEVEL_REHEARSAL) {
				continue;
			}
			bool expired = nowTime > scheduleTime;
			PLSScheComboxItemData scheData = PLSScheComboxItemData();
			scheData.title = info.title;
			scheData._id = info.id;
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
			PLSScheComboxItemData nomarlData = PLSScheComboxItemData();
			nomarlData._id = "";
			nomarlData.title = tr("New");
			nomarlData.time = tr("New");
			nomarlData.type = PLSScheComboxItemType::Ty_NormalLive;
			m_vecItemDatas.insert(m_vecItemDatas.begin(), nomarlData);
		}

		if (m_vecItemDatas.size() == 0) {
			PLSScheComboxItemData nomarlData = PLSScheComboxItemData();
			nomarlData._id = "";
			nomarlData.title = tr("New");
			nomarlData.time = tr("LiveInfo.Youtube.no.scheduled");
			nomarlData.type = PLSScheComboxItemType::Ty_Placehoder;
			m_vecItemDatas.insert(m_vecItemDatas.begin(), nomarlData);
		}
		ui->scheCombox->showScheduleMenu(m_vecItemDatas);
	};

	ui->scheCombox->showScheduleMenu(m_vecItemDatas);
	platform->getScheduleList(RequestCallback, this, ThisIsValid);
}

void PLSLiveInfoNaverShoppingLIVE::scheduleItemClick(const QString selectID)
{
	for (auto data : m_vecItemDatas) {
		if ((0 != data._id.compare(selectID))) {
			continue;
		}
		PLSScheComboxItemType type = data.type;
		if (type != PLSScheComboxItemType::Ty_Schedule && type != PLSScheComboxItemType::Ty_NormalLive) {
			break;
		}

		if (m_TempPrepareInfo.scheduleId.compare(selectID) == 0) {
			break;
		}

		if (data._id != selectID) {
			continue;
		}

		//change
		if (m_scheduleTempPrepareInfo.scheduleId.length() > 0 && isModified(m_scheduleTempPrepareInfo)) {
			PLSAlertView::Button button =
				PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.schedule.switch.tip"), PLSAlertView::Button::Yes | PLSAlertView::Button::No);
			if (button == PLSAlertView::Button::Yes) {
				if (!isOkButtonEnabled()) {
					PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("navershopping.liveinfo.schedule.info.empty.tip"));
					return;
				}
				m_prepareLivingType = PrepareRequestType::LiveBeforeUpdateSchedulePrepareRequest;
				prepareLiving([=](bool result) {
					if (result) {
						switchNewScheduleItem(type, data._id);
					}
				});
				return;
			}
		}
		switchNewScheduleItem(type, data._id);
		break;
	}
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

void PLSNaverShoppingImageScaleProcessor::process(PLSPlatformNaverShoppingLIVE *platform, double dpi, const QString &imagePath, const QSize &imageSize)
{
	QString scaleImagePath;
	if (platform->getScalePixmapPath(scaleImagePath, imagePath)) {
		return;
	}

	QPixmap pixmap = QPixmap(imagePath);
	QPixmap scaled = pixmap.scaled(imageSize);
	QPixmap image(scaled.size());
	image.fill(Qt::transparent);

	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	QPainterPath path;
	path.addRoundedRect(image.rect(), PLSDpiHelper::calculate(dpi, 4), PLSDpiHelper::calculate(dpi, 4));
	painter.setClipPath(path);
	painter.drawPixmap(image.rect(), scaled);

	QString savePath = platform->getScaleImagePath(imagePath);
	image.save(savePath);
	platform->setScalePixmapPath(imagePath, savePath);
}

void PLSPLSNaverShoppingLIVEImageScaleThread::process()
{
	while (running) {
		std::tuple<PLSPlatformNaverShoppingLIVE *, double, QString, QSize> resource;
		if (pop(resource) && running) {
			processor->process(std::get<0>(resource), std::get<1>(resource), std::get<2>(resource), std::get<3>(resource));
		}
	}
}
