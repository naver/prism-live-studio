#include "PLSLiveInfoYoutube.h"
#include <frontend-api.h>
#include <QDebug>
#include <QHBoxLayout>
#include <QImage>
#include <QPixmap>
#include <QWidgetAction>
#include <qradiobutton.h>
#include <QTextEdit>
#include <Vector>
#include "../PLSLiveInfoDialogs.h"
#include "../PLSPlatformApi.h"
#include "ChannelCommonFunctions.h"

#include "alert-view.hpp"
#include "log/log.h"
#include "ui_PLSLiveInfoYoutube.h"

static const char *liveInfoMoudule = "PLSLiveInfoYoutube";

PLSLiveInfoYoutube::PLSLiveInfoYoutube(PLSPlatformBase *pPlatformBase, QWidget *parent, PLSDpiHelper dpiHelper) : PLSLiveInfoBase(pPlatformBase, parent, dpiHelper), ui(new Ui::PLSLiveInfoYoutube)
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSLiveInfoYoutube});

	PLS_INFO(liveInfoMoudule, "Youtube liveinfo Will show");
	ui->setupUi(this->content());

	setHasCloseButton(false);
	setHasBorder(true);

	ui->sPushButton->setFocusPolicy(Qt::NoFocus);
	content()->setFocusPolicy(Qt::StrongFocus);

	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	pPlatformYoutube->liveInfoisShowing();
	m_enteredID = PLS_PLATFORM_YOUTUBE->getSelectData()._id;

	ui->latencyRadioWidget->setEnabled(!PLS_PLATFORM_API->isLiving());

	setupFirstUI();

	connect(ui->okButton, &QPushButton::clicked, this, &PLSLiveInfoYoutube::okButtonClicked);
	connect(ui->cancelButton, &QPushButton::clicked, this, &PLSLiveInfoYoutube::cancelButtonClicked);

	pPlatformYoutube->setAlertParent(this);

	updateStepTitle(ui->okButton);

	if (PLS_PLATFORM_API->isPrepareLive()) {
		ui->bottomButtonWidget->layout()->addWidget(ui->okButton);
	}

	ui->sPushButton->setButtonEnable(!PLS_PLATFORM_API->isLiving());

	doUpdateOkState();
}

PLSLiveInfoYoutube::~PLSLiveInfoYoutube()
{
	delete ui;
}

void PLSLiveInfoYoutube::setupFirstUI()
{
	ui->kidsLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("LiveInfo.Kids.Title")));
	ui->liveTitleLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("LiveInfo.base.Title")));

	ui->titleWidget->layout()->addWidget(createResolutionButtonsFrame());

	ui->hiddenRadioButton->setHidden(true);

	ui->lineEditTitle->setText("");
	ui->textEditDescribe->setText("");

	ui->textEditDescribe->setPlaceholderText(tr("Live.info.Description"));
	ui->comboBoxPublic->clear();
	ui->comboBoxCategory->clear();
	ui->latencyIcon->setToolTip(QString("<qt>%1</qt>").arg(tr("LiveInfo.latency.title.tip")));

	refreshUI();
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	if (nullptr != pPlatformYoutube) {
		connect(pPlatformYoutube, &PLSPlatformYoutube::onGetTitleDescription, this, &PLSLiveInfoYoutube::refreshTitleDescri);
		connect(pPlatformYoutube, &PLSPlatformYoutube::onGetTitleDescription, this, &PLSLiveInfoYoutube::refreshPrivacy);
		connect(pPlatformYoutube, &PLSPlatformYoutube::onGetTitleDescription, this, &PLSLiveInfoYoutube::refreshSchedulePopButton);
		connect(pPlatformYoutube, &PLSPlatformYoutube::onGetCategorys, this, &PLSLiveInfoYoutube::refreshCategory);
	}

	connect(ui->sPushButton, &PLSScheduleCombox::pressed, this, &PLSLiveInfoYoutube::scheduleButtonClicked);
	connect(ui->sPushButton, &PLSScheduleCombox::menuItemClicked, this, &PLSLiveInfoYoutube::scheduleItemClick);
	connect(ui->kidsRadioButton, &QRadioButton::clicked, this, &PLSLiveInfoYoutube::setKidsRadioButtonClick);
	connect(ui->notKidsRadioButton, &QRadioButton::clicked, this, &PLSLiveInfoYoutube::setNotKidsRadioButtonClick);

	connect(ui->radioButton_normal, &QRadioButton::clicked, this, [=]() {
		PLS_UI_STEP(liveInfoMoudule, "Youtube radioButton normal click", ACTION_CLICK);
		refreshUltraTipLabelVisible();
	});
	connect(ui->radioButton_low, &QRadioButton::clicked, this, [=]() {
		PLS_UI_STEP(liveInfoMoudule, "Youtube radioButton low click", ACTION_CLICK);
		refreshUltraTipLabelVisible();
	});
	connect(ui->radioButton_ultraLow, &QRadioButton::clicked, this, [=]() {
		PLS_UI_STEP(liveInfoMoudule, "Youtube radioButton ultraLow click", ACTION_CLICK);
		refreshUltraTipLabelVisible();
	});

	connect(ui->lineEditTitle, &QLineEdit::textChanged, this, &PLSLiveInfoYoutube::titleEdited, Qt::QueuedConnection);
	connect(ui->textEditDescribe, &QTextEdit::textChanged, this, &PLSLiveInfoYoutube::descriptionEdited);
	connect(ui->comboBoxPublic, SIGNAL(currentIndexChanged(int)), this, SLOT(doUpdateOkState()));
	connect(ui->comboBoxCategory, SIGNAL(currentIndexChanged(int)), this, SLOT(doUpdateOkState()));
	connect(
		pPlatformYoutube, &PLSPlatformYoutube::closeDialogByExpired, this,
		[=]() {
			hideLoading();
			reject();
		},
		Qt::DirectConnection);
	connect(
		pPlatformYoutube, &PLSPlatformYoutube::toShowLoading, this,
		[=](bool isShow) {
			if (isShow) {
				showLoading(this->content());
			} else {

				hideLoading();
			}
		},
		Qt::DirectConnection);
}

void PLSLiveInfoYoutube::showEvent(QShowEvent *event)
{
	Q_UNUSED(event)
	showLoading(content());
	PLS_PLATFORM_YOUTUBE->setIsShownAlert(false);
	auto _onNextVideo = [=](bool value) {
		refreshUI();
		if (!value && !PLS_PLATFORM_YOUTUBE->isShownAlert()) {
			PLS_PLATFORM_YOUTUBE->setIsShownAlert(false);
			PLS_PLATFORM_YOUTUBE->setupApiFailedWithCode(PLSPlatformApiResult::PAR_API_FAILED);
		}
		hideLoading();

		if (!value && !PLS_PLATFORM_API->isPrepareLive()) {
			reject();
		}
	};
	PLS_PLATFORM_YOUTUBE->requestCategoryList(
		[=](bool value) {
			if (!value) {
				_onNextVideo(false);
				return;
			}
			if (PLS_PLATFORM_YOUTUBE->getSelectData()._id.isEmpty()) {
				_onNextVideo(true);
				return;
			}
			PLS_PLATFORM_YOUTUBE->requestCurrentSelectData(_onNextVideo, this);
		},
		this);
	PLSLiveInfoBase::showEvent(event);
}

void PLSLiveInfoYoutube::refreshUI()
{
	refreshRadios();
	refreshTitleDescri();
	refreshPrivacy();
	refreshSchedulePopButton();
	refreshCategory();
	refreshLatency();
}

void PLSLiveInfoYoutube::refreshTitleDescri()
{
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	auto &data = pPlatformYoutube->getTempSelectData();

	ui->lineEditTitle->setText(data.title);
	ui->lineEditTitle->setCursorPosition(0);
	QString des = data.description;
	if (PLS_PLATFORM_API->isPrepareLive()) {
		QString add_str = s_description_default_add;
		if (!des.contains(add_str)) {
			if (!des.isEmpty()) {
				des.append("\n");
			}
			des.append(add_str);
		}
	}
	ui->textEditDescribe->setText(des);
}

void PLSLiveInfoYoutube::refreshPrivacy()
{
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	const auto &data = pPlatformYoutube->getTempSelectData();
	ui->comboBoxPublic->clear();
	for (int i = 0; i < pPlatformYoutube->getPrivacyDatas().size(); i++) {
		QString lcoalString = pPlatformYoutube->getPrivacyDatas()[i];
		ui->comboBoxPublic->addItem(lcoalString);

		QString enString = pPlatformYoutube->getPrivacyEnglishDatas()[i];
		if (0 == enString.compare(data.privacyStatus, Qt::CaseInsensitive)) {
			ui->comboBoxPublic->setCurrentIndex(i);
		}
	}
}

void PLSLiveInfoYoutube::refreshLatency()
{
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	const auto &data = pPlatformYoutube->getTempSelectData();
	switch (data.latency) {
	case PLSYoutubeLatency::Normal:
		ui->radioButton_normal->setChecked(true);
		break;
	case PLSYoutubeLatency::Low:
		ui->radioButton_low->setChecked(true);
		break;
	case PLSYoutubeLatency::UltraLow:
		ui->radioButton_ultraLow->setChecked(true);
		break;
	default:
		break;
	}
	ui->radioButton_ultraLow->setEnabled(!data.isCaptions);
	refreshUltraTipLabelVisible();
}

void PLSLiveInfoYoutube::refreshCategory()
{
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	const auto &data = pPlatformYoutube->getTempSelectData();

	ui->comboBoxCategory->clear();
	for (int i = 0; i < pPlatformYoutube->getCategoryDatas().size(); i++) {
		const auto &categoryData = pPlatformYoutube->getCategoryDatas()[i];
		ui->comboBoxCategory->addItem(categoryData.title);
		if (0 == categoryData._id.compare(data.categoryID, Qt::CaseInsensitive)) {
			ui->comboBoxCategory->setCurrentIndex(i);
		}
	}
	if (ui->comboBoxCategory->currentIndex() < 0 && pPlatformYoutube->getCategoryDatas().size() > 0) {
		ui->comboBoxCategory->setCurrentIndex(0);
	}
}

void PLSLiveInfoYoutube::refreshSchedulePopButton()
{
	const auto &data = PLS_PLATFORM_YOUTUBE->getTempSelectData();

	QString title(data.title);
	if (data.isNormalLive) {
		title = tr("New");
	}
	ui->sPushButton->setupButton(title, data.startTimeShort);
}

void PLSLiveInfoYoutube::saveTempNormalDataWhenSwitch()
{
	PLSYoutubeLiveinfoData &tempData = PLS_PLATFORM_YOUTUBE->getTempNormalData();
	tempData.title = ui->lineEditTitle->text();
	tempData.description = ui->textEditDescribe->toPlainText();
	size_t cateCount = PLS_PLATFORM_YOUTUBE->getCategoryDatas().size();
	if (cateCount > 0 && cateCount > ui->comboBoxCategory->currentIndex()) {
		tempData.categoryID = PLS_PLATFORM_YOUTUBE->getCategoryDatas()[ui->comboBoxCategory->currentIndex()]._id;
	} else {
		PLS_WARN(liveInfoMoudule, "category size invalid. all count:%d, currentIndex:%d", cateCount, ui->comboBoxCategory->currentIndex());
		tempData.categoryID = kDefaultCategoryID;
	}
	tempData.privacyStatus = PLS_PLATFORM_YOUTUBE->getPrivacyEnglishDatas()[ui->comboBoxPublic->currentIndex()];

	tempData.latency = getUILatency();
	tempData.iskidsUserSelect = ui->kidsRadioButton->isChecked() || ui->notKidsRadioButton->isChecked();
	tempData.isForKids = ui->kidsRadioButton->isChecked();
}

void PLSLiveInfoYoutube::okButtonClicked()
{
	PLS_UI_STEP(liveInfoMoudule, "Youtube liveinfo OK Button Click", ACTION_CLICK);
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;

	auto _onNext = [=](bool isSucceed) {
		hideLoading();
		PLS_INFO(liveInfoMoudule, "Youtube liveinfo Save %s", (isSucceed ? "succeed" : "failed"));
		if (isSucceed) {
			accept();
		}
	};

	showLoading(content());

	PLSYoutubeLiveinfoData uiData = pPlatformYoutube->getTempSelectData();
	bool isNeedUpdate = PLS_PLATFORM_YOUTUBE->isModifiedWithNewData(ui->lineEditTitle->text(), ui->textEditDescribe->toPlainText(), ui->comboBoxCategory->currentIndex(),
									ui->comboBoxPublic->currentIndex(), ui->kidsRadioButton->isChecked(), ui->notKidsRadioButton->isChecked(), getUILatency(),
									&uiData);

	if (PLS_PLATFORM_API->isPrepareLive()) {
		QString add_str = s_description_default_add;
		if (!uiData.description.contains(add_str)) {
			if (!uiData.description.isEmpty()) {
				uiData.description.append("\n");
			}
			uiData.description.append(add_str);
			isNeedUpdate = true;
		}
	}
	pPlatformYoutube->saveSettings(_onNext, isNeedUpdate, uiData);
}

void PLSLiveInfoYoutube::cancelButtonClicked()
{
	PLS_UI_STEP(liveInfoMoudule, "Youtube liveinfo Cancel Button Click", ACTION_CLICK);
	reject();
}

void PLSLiveInfoYoutube::scheduleButtonClicked()
{
	PLS_UI_STEP(liveInfoMoudule, "Youtube liveinfo schedule pop button click", ACTION_CLICK);
	if (!ui->sPushButton->getMenuHide()) {
		return;
	}
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;

	m_vecItemDatas.clear();
	for (int i = 0; i < 1; i++) {
		PLSScheComboxItemData data = PLSScheComboxItemData();
		data.title = tr("LiveInfo.live.loading.scheduled");
		data.type = PLSScheComboxItemType::Ty_Loading;
		m_vecItemDatas.push_back(data);
	}

	auto _onNext = [=](bool value) {
		Q_UNUSED(value)
		if (ui->sPushButton == nullptr || ui->sPushButton->isMenuNULL() || ui->sPushButton->getMenuHide()) {
			return;
		}

		m_vecItemDatas.clear();

		for (const auto &data : pPlatformYoutube->getScheduleDatas()) {
			PLSScheComboxItemData scheData = PLSScheComboxItemData();
			scheData.title = data.title;
			scheData._id = data._id;
			scheData.type = PLSScheComboxItemType::Ty_Schedule;
			scheData.time = data.startTimeUTC;
			if (data._id != pPlatformYoutube->getTempSelectID()) {
				m_vecItemDatas.push_back(scheData);
			}
		}

		if (!pPlatformYoutube->getTempSelectData().isNormalLive) {
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

		ui->sPushButton->showScheduleMenu(m_vecItemDatas);
	};

	if (nullptr != pPlatformYoutube) {
		pPlatformYoutube->requestScheduleList(_onNext, ui->sPushButton);
	}

	ui->sPushButton->showScheduleMenu(m_vecItemDatas);
}

void PLSLiveInfoYoutube::scheduleItemClick(const QString selectID)
{

	for (const auto &data : m_vecItemDatas) {
		if ((0 != data._id.compare(selectID))) {
			continue;
		}

		PLSScheComboxItemType type = data.type;
		if (type != PLSScheComboxItemType::Ty_Schedule && type != PLSScheComboxItemType::Ty_NormalLive) {
			break;
		}

		auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;

		if (pPlatformYoutube->getTempSelectID().compare(selectID) == 0) {
			break;
		}

		if (data._id != selectID) {
			continue;
		}

		if (pPlatformYoutube->getTempSelectData().isNormalLive && type == PLSScheComboxItemType::Ty_Schedule) {
			//normal to schedule, temp saved.
			saveTempNormalDataWhenSwitch();
		}
		pPlatformYoutube->setTempSelectID(selectID);
		refreshUI();
		break;
	}
}

void PLSLiveInfoYoutube::doUpdateOkState()
{
	if (ui->lineEditTitle->text().trimmed().isEmpty() || PLS_PLATFORM_YOUTUBE->getCategoryDatas().size() == 0) {
		ui->okButton->setEnabled(false);
		return;
	}
	if (!ui->kidsRadioButton->isChecked() && !ui->notKidsRadioButton->isChecked()) {
		ui->okButton->setEnabled(false);
		return;
	}
	ui->okButton->setEnabled(true);
}

void PLSLiveInfoYoutube::refreshRadios()
{
	const auto &data = PLS_PLATFORM_YOUTUBE->getTempSelectData();

	if (!data.iskidsUserSelect) {
		ui->hiddenRadioButton->setChecked(true);
	} else {
		if (data.isForKids) {
			ui->kidsRadioButton->setChecked(true);
		} else {
			ui->notKidsRadioButton->setChecked(true);
		}
	}
}

void PLSLiveInfoYoutube::setKidsRadioButtonClick(bool)
{
	PLS_UI_STEP(liveInfoMoudule, __FUNCTION__, ACTION_CLICK);
	doUpdateOkState();
}

void PLSLiveInfoYoutube::setNotKidsRadioButtonClick(bool)
{
	PLS_UI_STEP(liveInfoMoudule, __FUNCTION__, ACTION_CLICK);
	doUpdateOkState();
}

void PLSLiveInfoYoutube::refreshUltraTipLabelVisible()
{

	ui->ultraLowTipLabel->setHidden(!ui->radioButton_ultraLow->isChecked());
}

PLSYoutubeLatency PLSLiveInfoYoutube::getUILatency()
{
	PLSYoutubeLatency _latency = PLSYoutubeLatency::Low;
	if (ui->radioButton_normal->isChecked()) {
		_latency = PLSYoutubeLatency::Normal;
	} else if (ui->radioButton_low->isChecked()) {
		_latency = PLSYoutubeLatency::Low;
	} else {
		_latency = PLSYoutubeLatency::UltraLow;
	}
	return _latency;
}

bool PLSLiveInfoYoutube::isModified()
{
	bool isModified = PLS_PLATFORM_YOUTUBE->isModifiedWithNewData(ui->lineEditTitle->text(), ui->textEditDescribe->toPlainText(), ui->comboBoxCategory->currentIndex(),
								      ui->comboBoxPublic->currentIndex(), ui->kidsRadioButton->isChecked(), ui->notKidsRadioButton->isChecked(), getUILatency(),
								      nullptr);
	if (!isModified) {
		if (m_enteredID.isEmpty() && PLS_PLATFORM_YOUTUBE->getTempSelectData().isNormalLive) {
			isModified = false;
		} else if (m_enteredID.compare(PLS_PLATFORM_YOUTUBE->getTempSelectData()._id, Qt::CaseInsensitive) != 0) {
			isModified = true;
		}
	}
	return isModified;
}

void PLSLiveInfoYoutube::titleEdited()
{
	static const int YoutubeTitleLengthLimit = 100;
	QString newText = ui->lineEditTitle->text();

	bool isLargeToMax = false;
	if (newText.length() > YoutubeTitleLengthLimit) {
		isLargeToMax = true;
		newText = newText.left(YoutubeTitleLengthLimit);
	}

	bool isContainSpecial = false;
	//this must use origin text to check special string
	if (ui->lineEditTitle->text().contains("<") || ui->lineEditTitle->text().contains(">")) {
		isContainSpecial = true;
		newText = newText.replace("<", "");
		newText = newText.replace(">", "");
	}
	if (newText.compare(ui->lineEditTitle->text()) != 0) {
		QSignalBlocker signalBlocker(ui->lineEditTitle);
		ui->lineEditTitle->setText(newText);
	}
	doUpdateOkState();

	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	const auto channelName = pPlatformYoutube->getInitData().value(ChannelData::g_platformName).toString();

	if (isLargeToMax) {
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("LiveInfo.Title.Length.Check.arg").arg(YoutubeTitleLengthLimit).arg(channelName));
	}

	if (isContainSpecial) {
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("LiveInfo.Youtube.Title.Contain.Special.Text"));
	}
}

void PLSLiveInfoYoutube::descriptionEdited()
{
	static const int YoutubeDescribeLengthLimit = 5000;

	if (ui->textEditDescribe->toPlainText().length() > YoutubeDescribeLengthLimit) {
		QSignalBlocker signalBlocker(ui->textEditDescribe);
		ui->textEditDescribe->setText(ui->textEditDescribe->toPlainText().left(YoutubeDescribeLengthLimit));
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("LiveInfo.Youtube.Description.Length.Check.arg"));
	}

	doUpdateOkState();
}
