#include "PLSLiveInfoNCB2B.h"
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
#include "ui_PLSLiveInfoNCB2B.h"
#include "libui.h"
#include "PLSRadioButton.h"
#include <QToolTip>

using namespace std;
using namespace common;

PLSLiveInfoNCB2B::PLSLiveInfoNCB2B(PLSPlatformBase *pPlatformBase, QWidget *parent) : PLSLiveInfoBase(pPlatformBase, parent), m_platform(dynamic_cast<PLSPlatformNCB2B *>(pPlatformBase))
{
	ui = pls_new<Ui::PLSLiveInfoNCB2B>();

	pls_add_css(this, {"PLSLiveInfoNCB2B"});
	PLS_INFO(MODULE_PLATFORM_NCB2B, "NCB2B liveinfo Will show");
	setupUi(ui);

	setHasCloseButton(false);
	this->setWindowTitle(tr("LiveInfo.liveinformation"));

	ui->sPushButton->setFocusPolicy(Qt::NoFocus);
	content()->setFocusPolicy(Qt::StrongFocus);
	ui->textEditDescribe->setAcceptRichText(false);

	m_platform->liveInfoIsShowing();
	m_enteredID = m_platform->getSelectData()._id;

	setupFirstUI();

	connect(ui->okButton, &QPushButton::clicked, this, &PLSLiveInfoNCB2B::okButtonClicked);
	connect(ui->cancelButton, &QPushButton::clicked, this, &PLSLiveInfoNCB2B::cancelButtonClicked);

	m_platform->setAlertParent(this);

	updateStepTitle(ui->okButton);

#if defined(Q_OS_WIN)
	if (!PLS_PLATFORM_API->isPrepareLive()) {
		ui->bottomButtonWidget->layout()->addWidget(ui->cancelButton);
	}
#endif

	ui->comboBoxPrivacy->setEnabled(!PLS_PLATFORM_API->isLiving());
	ui->sPushButton->setButtonEnable(!PLS_PLATFORM_API->isLiving());
	doUpdateOkState();
}

PLSLiveInfoNCB2B::~PLSLiveInfoNCB2B()
{
	pls_delete(ui, nullptr);
}

void PLSLiveInfoNCB2B::setupFirstUI()
{
	auto channelName = m_platform->getChannelName();
	ui->dualWidget->setText(channelNameConvertMultiLang(channelName))->setUUID(m_platform->getChannelUUID());
	ui->titleLabel_2->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("LiveInfo.base.Title")));

	ui->titleWidget->layout()->addWidget(createResolutionButtonsFrame(true));

	ui->lineEditTitle->setText("");
	ui->textEditDescribe->setText("");
	ui->textEditDescribe->setPlaceholderText(tr("Live.info.Description"));

	ui->comboBoxPrivacy->clear();

	for (const auto &t : PLSPlatformNCB2B::getPrivacyList()) {
		ui->comboBoxPrivacy->addItem(t.second);
	}

	refreshUI();

	connect(ui->sPushButton, &PLSScheduleCombox::pressed, this, &PLSLiveInfoNCB2B::scheduleButtonClicked);
	connect(ui->sPushButton, &PLSScheduleCombox::menuItemClicked, this, &PLSLiveInfoNCB2B::scheduleItemClick);

	connect(ui->lineEditTitle, &QLineEdit::textChanged, this, &PLSLiveInfoNCB2B::titleEdited, Qt::QueuedConnection);
	connect(ui->textEditDescribe, &QTextEdit::textChanged, this, &PLSLiveInfoNCB2B::descriptionEdited, Qt::QueuedConnection);
	connect(
		m_platform, &PLSPlatformNCB2B::closeDialogByExpired, this,
		[this]() {
			hideLoading();
			reject();
		},
		Qt::DirectConnection);
	connect(
		m_platform, &PLSPlatformNCB2B::toShowLoading, this,
		[this](bool isShow) {
			if (isShow) {
				showLoading(this->content());
			} else {
				hideLoading();
			}
		},
		Qt::DirectConnection);
}

void PLSLiveInfoNCB2B::showEvent(QShowEvent *event)
{
	Q_UNUSED(event)
	showLoading(content());
	auto _onNextVideo = [this](bool value) {
		refreshUI();
		hideLoading();

		if (!value && !PLS_PLATFORM_API->isPrepareLive()) {
			reject();
			return;
		}
	};
	if (m_platform->getSelectData()._id.isEmpty()) {
		hideLoading();
		PLSLiveInfoBase::showEvent(event);
		return;
	}
	m_platform->requestCurrentSelectData(_onNextVideo, this);
	PLSLiveInfoBase::showEvent(event);
}

void PLSLiveInfoNCB2B::refreshUI()
{
	refreshTitleDescri();
	refreshSchedulePopButton();

	const auto &data = m_platform->getTempSelectData();
	ui->comboBoxPrivacy->setCurrentText(PLSAPICommon::getPairedString(PLSPlatformNCB2B::getPrivacyList(), data.scope, true));
}

void PLSLiveInfoNCB2B::refreshTitleDescri()
{
	const auto &data = m_platform->getTempSelectData();
	ui->lineEditTitle->setText(data.title);
	ui->lineEditTitle->setCursorPosition(0);
	ui->textEditDescribe->setText(data.description);
}

void PLSLiveInfoNCB2B::refreshSchedulePopButton()
{
	const auto &data = m_platform->getTempSelectData();

	QString title(data.title);
	if (data.isNormalLive) {
		title = tr("New");
	}
	ui->sPushButton->setupButton(title, data.startTimeShort);
}

void PLSLiveInfoNCB2B::saveTempNormalDataWhenSwitch() const
{
	PLSNCB2BLiveinfoData &tempData = m_platform->getTempNormalData();
	tempData.title = ui->lineEditTitle->text();
	tempData.description = ui->textEditDescribe->toPlainText();
	tempData.scope = PLSPlatformNCB2B::getPrivacyList()[ui->comboBoxPrivacy->currentIndex()].first;
}

void PLSLiveInfoNCB2B::okButtonClicked()
{
	PLS_UI_STEP(MODULE_PLATFORM_NCB2B, "NCB2B liveinfo OK Button Click", ACTION_CLICK);
	saveDateWhenClickButton();
}

void PLSLiveInfoNCB2B::cancelButtonClicked()
{
	PLS_UI_STEP(MODULE_PLATFORM_NCB2B, "NCB2B liveinfo Cancel Button Click", ACTION_CLICK);
	reject();
}

void PLSLiveInfoNCB2B::saveDateWhenClickButton()
{
	if (getIsRunLoading()) {
		PLS_INFO(MODULE_PLATFORM_NCB2B, "NCB2B ignore OK Button Click, because is loading");
		return;
	}

	showLoading(content());
	auto _onNext = [this](bool isSucceed) {
		hideLoading();
		PLS_INFO(MODULE_PLATFORM_NCB2B, "NCB2B liveinfo Save %s", (isSucceed ? "succeed" : "failed"));
		if (PLS_PLATFORM_API->isPrepareLive()) {
			if (isSucceed) {
				PLS_LOGEX(PLS_LOG_INFO, MODULE_PLATFORM_NCB2B,
					  {{"platformName", NCB2B}, //
					   {"startLiveStatus", "Success"},
					   {"channelID", m_platform->subChannelID().toUtf8().constData()}},
					  "NCB2B start live success");
			} else {
				PLS_LOGEX(PLS_LOG_ERROR, MODULE_PLATFORM_NCB2B,
					  {{"platformName", NCB2B},
					   {"startLiveStatus", "Failed"},
					   {"startLiveFailed", m_platform->getFailedErr().toUtf8().constData()},
					   {"channelID", m_platform->subChannelID().toUtf8().constData()}},
					  "NCB2B start live failed");
			}
		}
		if (isSucceed)
			accept();
	};

	PLSNCB2BLiveinfoData uiData = m_platform->getTempSelectData();
	uiData.title = ui->lineEditTitle->text();
	uiData.description = ui->textEditDescribe->toPlainText();
	uiData.scope = PLSPlatformNCB2B::getPrivacyList()[ui->comboBoxPrivacy->currentIndex()].first;
	m_platform->setFailedErr("");
	m_platform->saveSettings(_onNext, uiData, this);
}

void PLSLiveInfoNCB2B::scheduleButtonClicked()
{
	PLS_UI_STEP(MODULE_PLATFORM_NCB2B, "NCB2B liveinfo schedule pop button click", ACTION_CLICK);
	if (!ui->sPushButton->getMenuHide()) {
		return;
	}
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

	if (nullptr != m_platform) {
		m_platform->requestScheduleList(_onNext, ui->sPushButton);
	}

	ui->sPushButton->showScheduleMenu(m_vecItemDatas);
}

void PLSLiveInfoNCB2B::scheduleItemClick(const QString selectID)
{

	for (const auto &data : m_vecItemDatas) {
		if (data._id != selectID) {
			continue;
		}

		PLSScheComboxItemType type = data.type;
		if (type != PLSScheComboxItemType::Ty_Schedule && type != PLSScheComboxItemType::Ty_NormalLive) {
			break;
		}
		if (m_platform->getTempSelectID().compare(selectID) == 0) {
			break;
		}

		if (m_platform->getTempSelectData().isNormalLive && type == PLSScheComboxItemType::Ty_Schedule) {
			//normal to schedule, temp saved.
			saveTempNormalDataWhenSwitch();
		}
		m_platform->setTempSelectID(selectID);
		refreshUI();
	}
}

void PLSLiveInfoNCB2B::reloadScheduleList()
{
	m_vecItemDatas.clear();
	for (const auto &data : m_platform->getScheduleDatas()) {
		PLSScheComboxItemData scheData;
		scheData.title = data.title;
		scheData._id = data._id;
		scheData.type = PLSScheComboxItemType::Ty_Schedule;
		scheData.time = data.startTimeUTC;
		if (data._id != m_platform->getTempSelectID()) {
			m_vecItemDatas.push_back(scheData);
		}
	}

	if (!m_platform->getTempSelectData().isNormalLive) {
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

void PLSLiveInfoNCB2B::doUpdateOkState()
{
	if (ui->lineEditTitle->text().trimmed().isEmpty()) {
		ui->okButton->setEnabled(false);
		return;
	}
	ui->okButton->setEnabled(true);
}

void PLSLiveInfoNCB2B::titleEdited()
{
	static const int titleLengthLimit = 100;
	QString newText = ui->lineEditTitle->text();

	bool isLargeToMax = false;
	if (newText.length() > titleLengthLimit) {
		isLargeToMax = true;
		newText = newText.left(titleLengthLimit);
	}

	if (newText.compare(ui->lineEditTitle->text()) != 0) {
		QSignalBlocker signalBlocker(ui->lineEditTitle);
		ui->lineEditTitle->setText(newText);
	}
	doUpdateOkState();

	const auto channelName = m_platform->getInitData().value(ChannelData::g_channelName).toString();

	if (isLargeToMax) {
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("LiveInfo.Title.Length.Check.arg").arg(titleLengthLimit).arg(channelName));
	}
}

void PLSLiveInfoNCB2B::descriptionEdited()
{
	static const int describeLengthLimit = 5000;

	if (ui->textEditDescribe->toPlainText().length() > describeLengthLimit) {
		QSignalBlocker signalBlocker(ui->textEditDescribe);
		ui->textEditDescribe->setText(ui->textEditDescribe->toPlainText().left(describeLengthLimit));
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("LiveInfo.Youtube.Description.Length.Check.arg").arg(m_platform->getChannelName()));
	}

	doUpdateOkState();
}
