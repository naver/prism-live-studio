#include "PLSLiveInfoYoutube.h"
#include <frontend-api.h>
#include <QDebug>
#include <QHBoxLayout>
#include <QImage>
#include <QPixmap>
#include <QWidgetAction>
#include <qradiobutton.h>
#include <QTextEdit>
#include <vector>
#include "../PLSLiveInfoDialogs.h"
#include "../PLSPlatformApi.h"
#include "ChannelCommonFunctions.h"

#include "PLSAlertView.h"
#include "log/log.h"
#include "ui_PLSLiveInfoYoutube.h"
#include "PLSAPIYoutube.h"
#include "libui.h"
#include "PLSRadioButton.h"
#include <QToolTip>

static const char *const liveInfoModule = "PLSLiveInfoYoutube";
using namespace std;
using namespace common;

PLSLiveInfoYoutube::PLSLiveInfoYoutube(PLSPlatformBase *pPlatformBase, QWidget *parent) : PLSLiveInfoBase(pPlatformBase, parent)
{
	ui = pls_new<Ui::PLSLiveInfoYoutube>();

	pls_add_css(this, {"PLSLiveInfoYoutube"});
	PLS_INFO(liveInfoModule, "Youtube liveinfo Will show");
	setupUi(ui);

	setHasCloseButton(false);

	ui->sPushButton->setFocusPolicy(Qt::NoFocus);
	content()->setFocusPolicy(Qt::StrongFocus);

	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	pPlatformYoutube->liveInfoIsShowing();
	m_enteredID = PLS_PLATFORM_YOUTUBE->getSelectData()._id;

	ui->dualWidget->setText(pPlatformYoutube->getPlatFormName())->setUUID(pPlatformYoutube->getChannelUUID());
	ui->latencyRadioWidget->setEnabled(!PLS_PLATFORM_API->isLiving());
	ui->textEditDescribe->setAcceptRichText(false);

	m_isLiveStopped = PLS_PLATFORM_YOUTUBE->isSelfStopping();
	setupFirstUI();

	connect(ui->okButton, &QPushButton::clicked, this, &PLSLiveInfoYoutube::okButtonClicked);
	connect(ui->cancelButton, &QPushButton::clicked, this, &PLSLiveInfoYoutube::cancelButtonClicked);
	connect(ui->rehearsalButton, &QPushButton::clicked, this, &PLSLiveInfoYoutube::rehearsalButtonClicked);

	pPlatformYoutube->setAlertParent(this);

	updateStepTitle(ui->okButton);

#if defined(Q_OS_WIN)
	if (!PLS_PLATFORM_API->isPrepareLive()) {
		ui->bottomButtonWidget->layout()->addWidget(ui->cancelButton);
	}
#endif
	ui->rehearsalButton->setVisible(PLS_PLATFORM_API->isPrepareLive() && PLSCHANNELS_API->currentSelectedCount() == 1);

	ui->sPushButton->setButtonEnable(!PLS_PLATFORM_API->isLiving());
	ui->thumbnailButton->setIsIgnoreMinSize(true);

	PLSAPICommon::createHelpIconWidget(ui->latencyTitle, QString("<qt>%1</qt>").arg(tr("LiveInfo.latency.title.tip")), ui->formLayout, this, &PLSLiveInfoYoutube::shown);

	doUpdateOkState();
	refreshThumbnailButton();
}

PLSLiveInfoYoutube::~PLSLiveInfoYoutube()
{
	pls_delete(ui, nullptr);
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
	refreshUI();
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	if (nullptr != pPlatformYoutube) {
		connect(pPlatformYoutube, &PLSPlatformYoutube::onGetTitleDescription, this, &PLSLiveInfoYoutube::refreshTitleDescri);
		connect(pPlatformYoutube, &PLSPlatformYoutube::onGetTitleDescription, this, &PLSLiveInfoYoutube::refreshPrivacy);
		connect(pPlatformYoutube, &PLSPlatformYoutube::onGetTitleDescription, this, &PLSLiveInfoYoutube::refreshSchedulePopButton);
	}

	connect(ui->sPushButton, &PLSScheduleCombox::pressed, this, &PLSLiveInfoYoutube::scheduleButtonClicked);
	connect(ui->sPushButton, &PLSScheduleCombox::menuItemClicked, this, &PLSLiveInfoYoutube::scheduleItemClick);
	connect(ui->kidsRadioButton, &PLSRadioButton::clicked, this, &PLSLiveInfoYoutube::setKidsRadioButtonClick);
	connect(ui->notKidsRadioButton, &PLSRadioButton::clicked, this, &PLSLiveInfoYoutube::setNotKidsRadioButtonClick);

	connect(ui->radioButton_normal, &PLSRadioButton::clicked, this, [this]() {
		PLS_UI_STEP(liveInfoModule, "Youtube radioButton normal click", ACTION_CLICK);
		refreshUltraTipLabelVisible();
	});
	connect(ui->radioButton_low, &PLSRadioButton::clicked, this, [this]() {
		PLS_UI_STEP(liveInfoModule, "Youtube radioButton low click", ACTION_CLICK);
		refreshUltraTipLabelVisible();
	});
	connect(ui->radioButton_ultraLow, &PLSRadioButton::clicked, this, [this]() {
		PLS_UI_STEP(liveInfoModule, "Youtube radioButton ultraLow click", ACTION_CLICK);
		refreshUltraTipLabelVisible();
	});

	connect(ui->lineEditTitle, &QLineEdit::textChanged, this, &PLSLiveInfoYoutube::titleEdited, Qt::QueuedConnection);
	connect(ui->textEditDescribe, &QTextEdit::textChanged, this, &PLSLiveInfoYoutube::descriptionEdited, Qt::QueuedConnection);
	connect(ui->comboBoxPublic, SIGNAL(currentIndexChanged(int)), this, SLOT(doUpdateOkState()));
	connect(ui->comboBoxCategory, SIGNAL(currentIndexChanged(int)), this, SLOT(doUpdateOkState()));
	connect(
		pPlatformYoutube, &PLSPlatformYoutube::closeDialogByExpired, this,
		[this]() {
			hideLoading();
			reject();
		},
		Qt::DirectConnection);
	connect(
		pPlatformYoutube, &PLSPlatformYoutube::toShowLoading, this,
		[this](bool isShow) {
			if (isShow) {
				showLoading(this->content());
			} else {

				hideLoading();
			}
		},
		Qt::DirectConnection);

	connect(ui->thumbnailButton, &PLSSelectImageButton::takeButtonClicked, this, []() { PLS_UI_STEP(liveInfoModule, "Youtube LiveInfo's Thumbnail Take Button", ACTION_CLICK); });
	connect(ui->thumbnailButton, &PLSSelectImageButton::selectButtonClicked, this, []() { PLS_UI_STEP(liveInfoModule, "Youtube LiveInfo's Thumbnail Select Button", ACTION_CLICK); });
	connect(ui->thumbnailButton, &PLSSelectImageButton::imageSelected, this, &PLSLiveInfoYoutube::onImageSelected);
	connect(pPlatformYoutube, &PLSPlatformYoutube::receiveLiveStop, this, [this]() { m_isLiveStopped = true; });
	connect(this, &PLSLiveInfoYoutube::loadingFinished, this, &PLSLiveInfoYoutube::selectScheduleCheck, Qt::QueuedConnection);
}

void PLSLiveInfoYoutube::showEvent(QShowEvent *event)
{
	Q_UNUSED(event)
	showLoading(content());
	auto _onNextVideo = [this](bool value) {
		//use the new m_selectData to replace old data in schedule list
		PLS_PLATFORM_YOUTUBE->updateScheduleListAndSort();
		refreshUI();
		hideLoading();

		if (!value && !PLS_PLATFORM_API->isPrepareLive()) {
			reject();
			return;
		}
		emit loadingFinished();
	};
	PLS_PLATFORM_YOUTUBE->requestCategoryList(
		[_onNextVideo, this](bool value) {
			if (!value) {
				_onNextVideo(false);
				return;
			}
			if (PLS_PLATFORM_YOUTUBE->getSelectData()._id.isEmpty() || m_isLiveStopped) {
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
		QString localString = pPlatformYoutube->getPrivacyDatas()[i];
		ui->comboBoxPublic->addItem(localString);

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
	case PLSYoutubeLiveinfoData::Latency::Normal:
		ui->radioButton_normal->setChecked(true);
		break;
	case PLSYoutubeLiveinfoData::Latency::Low:
		ui->radioButton_low->setChecked(true);
		break;
	case PLSYoutubeLiveinfoData::Latency::UltraLow:
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
	if (ui->comboBoxCategory->currentIndex() < 0 && !pPlatformYoutube->getCategoryDatas().empty()) {
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

void PLSLiveInfoYoutube::saveTempNormalDataWhenSwitch() const
{
	PLSYoutubeLiveinfoData &tempData = PLS_PLATFORM_YOUTUBE->getTempNormalData();
	tempData.title = ui->lineEditTitle->text();
	tempData.description = ui->textEditDescribe->toPlainText();
	size_t cateCount = PLS_PLATFORM_YOUTUBE->getCategoryDatas().size();
	if (cateCount > 0 && cateCount > ui->comboBoxCategory->currentIndex()) {
		tempData.categoryID = PLS_PLATFORM_YOUTUBE->getCategoryDatas()[ui->comboBoxCategory->currentIndex()]._id;
	} else {
		PLS_WARN(liveInfoModule, "category size invalid. all count:%d, currentIndex:%d", cateCount, ui->comboBoxCategory->currentIndex());
		tempData.categoryID = kDefaultCategoryID;
	}
	tempData.privacyStatus = PLS_PLATFORM_YOUTUBE->getPrivacyEnglishDatas()[ui->comboBoxPublic->currentIndex()];

	tempData.latency = getUILatency();
	tempData.iskidsUserSelect = ui->kidsRadioButton->isChecked() || ui->notKidsRadioButton->isChecked();
	tempData.isForKids = ui->kidsRadioButton->isChecked();
	tempData.pixMap = ui->thumbnailButton->getOriginalPixmap();
}

void PLSLiveInfoYoutube::okButtonClicked()
{
	PLS_UI_STEP(liveInfoModule, "Youtube liveinfo OK Button Click", ACTION_CLICK);
	if (PLS_PLATFORM_API->isPrepareLive()) {
		PLS_PLATFORM_YOUTUBE->setIsRehearsal(false);
	}
	saveDateWhenClickButton();
}

void PLSLiveInfoYoutube::cancelButtonClicked()
{
	PLS_UI_STEP(liveInfoModule, "Youtube liveinfo Cancel Button Click", ACTION_CLICK);
	reject();
}

void PLSLiveInfoYoutube::rehearsalButtonClicked()
{

	PLS_UI_STEP(liveInfoModule, "Youtube liveinfo Rehearsal Button Click", ACTION_CLICK);
	if (PLS_PLATFORM_API->isPrepareLive()) {
		PLS_PLATFORM_YOUTUBE->setIsRehearsal(true);
	}
	saveDateWhenClickButton();
}

void PLSLiveInfoYoutube::saveDateWhenClickButton()
{
	if (m_isLiveStopped) {
		PLS_INFO(liveInfoModule, "Youtube live stopped, ignore OK Button Click");
		reject();
		return;
	}
	if (getIsRunLoading()) {
		PLS_INFO(liveInfoModule, "Youtube ignore OK Button Click, because is loading");
		return;
	}

	showLoading(content());
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;

	auto _onNext = [this, pPlatformYoutube](bool isSucceed) {
		hideLoading();
		PLS_INFO(liveInfoModule, "Youtube liveinfo Save %s", (isSucceed ? "succeed" : "failed"));
		if (PLS_PLATFORM_API->isPrepareLive()) {
			if (isSucceed) {
				PLS_LOGEX(PLS_LOG_INFO, liveInfoModule,
					  {
						  {"platformName", YOUTUBE},
						  {"startLiveStatus", "Success"},
					  },
					  "YouTube start live success");
			} else {
				PLS_LOGEX(PLS_LOG_ERROR, liveInfoModule,
					  {{"platformName", YOUTUBE}, {"startLiveStatus", "Failed"}, {"startLiveFailed", pPlatformYoutube->getFailedErr().toUtf8().constData()}},
					  "YouTube start live failed");
			}
		}
		if (isSucceed)
			accept();
	};

	PLSYoutubeLiveinfoData uiData = pPlatformYoutube->getTempSelectData();
	uiData.title = ui->lineEditTitle->text();
	uiData.description = ui->textEditDescribe->toPlainText();
	uiData.latency = getUILatency();
	uiData.pixMap = ui->thumbnailButton->getOriginalPixmap();
	bool isNeedUpdate = PLS_PLATFORM_YOUTUBE->isModifiedWithNewData(ui->comboBoxCategory->currentIndex(), ui->comboBoxPublic->currentIndex(), ui->kidsRadioButton->isChecked(),
									ui->notKidsRadioButton->isChecked(), &uiData);
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
	pPlatformYoutube->setFailedErr("");
	pPlatformYoutube->saveSettings(_onNext, isNeedUpdate, uiData, this);
}

void PLSLiveInfoYoutube::scheduleButtonClicked()
{
	PLS_UI_STEP(liveInfoModule, "Youtube liveinfo schedule pop button click", ACTION_CLICK);
	if (!ui->sPushButton->getMenuHide()) {
		return;
	}
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;

	m_vecItemDatas.clear();
	for (int i = 0; i < 1; i++) {
		PLSScheComboxItemData data;
		data.title = tr("LiveInfo.live.loading.scheduled");
		data.type = PLSScheComboxItemType::Ty_Loading;
		m_vecItemDatas.push_back(data);
	}

	auto _onNext = [this](bool) {
		if (ui->sPushButton == nullptr || ui->sPushButton->isMenuNULL() || ui->sPushButton->getMenuHide()) {
			return;
		}

		reloadScheduleList();
	};

	if (nullptr != pPlatformYoutube) {
		pPlatformYoutube->requestScheduleList(_onNext, ui->sPushButton);
	}

	ui->sPushButton->showScheduleMenu(m_vecItemDatas);
}

void PLSLiveInfoYoutube::scheduleItemClick(const QString selectID)
{

	for (const auto &data : m_vecItemDatas) {
		if (data._id != selectID) {
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

		if (pPlatformYoutube->getTempSelectData().isNormalLive && type == PLSScheComboxItemType::Ty_Schedule) {
			//normal to schedule, temp saved.
			saveTempNormalDataWhenSwitch();
		}
		pPlatformYoutube->setTempSelectID(selectID);
		refreshThumbnailButton();
		if (selectID.isEmpty()) {
			refreshUI();
			break;
		}
		showLoading(content());
		pPlatformYoutube->requestCategoryID(
			[this](bool) {
				refreshUI();
				hideLoading();
			},
			selectID, this);
		break;
	}
}
void PLSLiveInfoYoutube::selectScheduleCheck()
{
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	auto lastData = pPlatformYoutube->getScheduleDatas();
	if (lastData.empty()) {
		return;
	}
	auto info = pPlatformYoutube->getInitData();
	auto json = getInfo(info, ChannelData::g_selectedSchedule, QJsonObject());
	if (json.isEmpty()) {
		return;
	}
	pPlatformYoutube->setTempSelectID(lastData.front()._id);
	refreshUI();
}

void PLSLiveInfoYoutube::reloadScheduleList()
{
	m_vecItemDatas.clear();
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	for (const auto &data : pPlatformYoutube->getScheduleDatas()) {
		PLSScheComboxItemData scheData;
		scheData.title = data.title;
		scheData._id = data._id;
		scheData.type = PLSScheComboxItemType::Ty_Schedule;
		scheData.time = data.startTimeUTC;
		if (data._id != pPlatformYoutube->getTempSelectID()) {
			m_vecItemDatas.push_back(scheData);
		}
	}

	if (!pPlatformYoutube->getTempSelectData().isNormalLive) {
		PLSScheComboxItemData normalData;
		normalData._id = "";
		normalData.title = tr("New");
		normalData.time = tr("New");
		normalData.type = PLSScheComboxItemType::Ty_NormalLive;
		m_vecItemDatas.insert(m_vecItemDatas.begin(), normalData);
	}

	if (m_vecItemDatas.empty()) {
		PLSScheComboxItemData normalData;
		normalData._id = "";
		normalData.title = tr("New");
		normalData.time = tr("LiveInfo.Youtube.no.scheduled");
		normalData.type = PLSScheComboxItemType::Ty_Placeholder;
		m_vecItemDatas.insert(m_vecItemDatas.begin(), normalData);
	}

	ui->sPushButton->showScheduleMenu(m_vecItemDatas);
}

void PLSLiveInfoYoutube::doUpdateOkState()
{
	if (ui->lineEditTitle->text().trimmed().isEmpty() || PLS_PLATFORM_YOUTUBE->getCategoryDatas().empty()) {
		ui->rehearsalButton->setEnabled(false);
		ui->okButton->setEnabled(false);
		return;
	}
	if (!ui->kidsRadioButton->isChecked() && !ui->notKidsRadioButton->isChecked()) {
		ui->rehearsalButton->setEnabled(false);
		ui->okButton->setEnabled(false);
		return;
	}
	ui->rehearsalButton->setEnabled(true);
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
	PLS_UI_STEP(liveInfoModule, __FUNCTION__, ACTION_CLICK);
	doUpdateOkState();
}

void PLSLiveInfoYoutube::setNotKidsRadioButtonClick(bool)
{
	PLS_UI_STEP(liveInfoModule, __FUNCTION__, ACTION_CLICK);
	doUpdateOkState();
}

void PLSLiveInfoYoutube::refreshUltraTipLabelVisible()
{

	ui->ultraLowTipLabel->setHidden(!ui->radioButton_ultraLow->isChecked());
}

void PLSLiveInfoYoutube::refreshThumbnailButton()
{
	ui->thumDetailLabel->setHidden(true);
	const auto &data = PLS_PLATFORM_YOUTUBE->getTempSelectData();
	ui->thumbnailButton->setPixmap(data.pixMap);

	PLS_PLATFORM_YOUTUBE->downloadThumImage([this]() { ui->thumbnailButton->setPixmap(PLS_PLATFORM_YOUTUBE->getTempSelectData().pixMap); }, data.thumbnailUrl, ui->thumbnailButton);
}

void PLSLiveInfoYoutube::onImageSelected(const QString &imageFilePath) const
{
	if (QFile::exists(imageFilePath)) {
		QFile::remove(imageFilePath);
	}
}

PLSYoutubeLiveinfoData::Latency PLSLiveInfoYoutube::getUILatency() const
{
	auto _latency = PLSYoutubeLiveinfoData::Latency::Low;
	if (ui->radioButton_normal->isChecked()) {
		_latency = PLSYoutubeLiveinfoData::Latency::Normal;
	} else if (ui->radioButton_low->isChecked()) {
		_latency = PLSYoutubeLiveinfoData::Latency::Low;
	} else {
		_latency = PLSYoutubeLiveinfoData::Latency::UltraLow;
	}
	return _latency;
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
	const auto channelName = pPlatformYoutube->getInitData().value(ChannelData::g_channelName).toString();

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
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("LiveInfo.Youtube.Description.Length.Check.arg").arg(PLS_PLATFORM_YOUTUBE->getChannelName()));
	}

	doUpdateOkState();
}
