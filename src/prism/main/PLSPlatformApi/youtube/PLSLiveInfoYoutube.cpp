#include "PLSLiveInfoYoutube.h"
#include "ui_PLSLiveInfoYoutube.h"
#include "../PLSPlatformApi.h"
#include <frontend-api.h>
#include "CommonDefine.h"
#include <QDebug>
#include <Vector>
#include "PLSScheduleMenuItem.h"
#include <QWidgetAction>
#include <QImage>
#include <QPixmap>
#include <QHBoxLayout>
#include "PLSScheduleButton.h"
#include "../PLSLiveInfoDialogs.h"
#include "log/log.h"
#include "alert-view.hpp"

static const char *liveInfoMoudule = "PLSLiveInfoYoutube";

PLSLiveInfoYoutube::PLSLiveInfoYoutube(PLSPlatformBase *pPlatformBase, QWidget *parent) : PLSLiveInfoBase(pPlatformBase, parent), ui(new Ui::PLSLiveInfoYoutube)
{
	PLS_INFO(liveInfoMoudule, "Youtube liveinfo Will show");
	ui->setupUi(this->content());

	sizeToContent({720, 550});
	setHasCloseButton(false);

	ui->sPushButton->setFocusPolicy(Qt::NoFocus);
	content()->setFocusPolicy(Qt::StrongFocus);

	setupFirstUI();
	m_enteredID = PLS_PLATFORM_YOUTUBE->getSelectDatas()._id;

	connect(ui->okButton, &QPushButton::clicked, this, &PLSLiveInfoYoutube::okButtonClicked);
	connect(ui->cancelButton, &QPushButton::clicked, this, &PLSLiveInfoYoutube::cancelButtonClicked);

	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	pPlatformYoutube->setAlertParent(this);

	updateStepTitle(ui->okButton);

	if (PLS_PLATFORM_API->isPrepareLive()) {
		ui->horizontalLayout_3->addWidget(ui->okButton);
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
	/*auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	if (nullptr == pPlatformYoutube) {
		return;
	}*/
	ui->lineEditTitle->setText("");
	ui->textEditDescribe->setText("");

	ui->textEditDescribe->setPlaceholderText(tr("LiveInfo.youtube.description.guide"));
	ui->comboBoxPublic->clear();
	ui->comboBoxCategory->clear();

	refreshUI();

	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	if (nullptr != pPlatformYoutube) {
		connect(pPlatformYoutube, &PLSPlatformYoutube::onGetTitleDescription, this, &PLSLiveInfoYoutube::refreshTitleDescri);
		connect(pPlatformYoutube, &PLSPlatformYoutube::onGetTitleDescription, this, &PLSLiveInfoYoutube::refreshPrivacy);
		connect(pPlatformYoutube, &PLSPlatformYoutube::onGetTitleDescription, this, &PLSLiveInfoYoutube::refreshSchedulePopButton);
		connect(pPlatformYoutube, &PLSPlatformYoutube::onGetCategorys, this, &PLSLiveInfoYoutube::refreshCategory);
	}

	connect(ui->sPushButton, &PLSScheduleButton::pressed, this, &PLSLiveInfoYoutube::scheduleButtonClicked);
	connect(ui->sPushButton, &PLSScheduleButton::menuItemClicked, this, &PLSLiveInfoYoutube::scheduleItemClick);

	connect(ui->lineEditTitle, &QLineEdit::textChanged, this, &PLSLiveInfoYoutube::titleEdited);
	connect(ui->textEditDescribe, &QTextEdit::textChanged, this, &PLSLiveInfoYoutube::descriptionEdited);
	connect(ui->comboBoxPublic, SIGNAL(currentIndexChanged(int)), this, SLOT(doUpdateOkState()));
	connect(ui->comboBoxCategory, SIGNAL(currentIndexChanged(int)), this, SLOT(doUpdateOkState()));
	connect(
		pPlatformYoutube, &PLSPlatformYoutube::closeDialogByExpired, this, [=]() { reject(); }, Qt::DirectConnection);
}

void PLSLiveInfoYoutube::showEvent(QShowEvent *event)
{
	Q_UNUSED(event)
	showLoading(content());

	auto _onNext = [=](bool value) {
		hideLoading();

		if (!value && !PLS_PLATFORM_API->isPrepareLive()) {
			reject();
		}
	};

	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	pPlatformYoutube->requestUserInfo(_onNext, false);
}

void PLSLiveInfoYoutube::refreshUI()
{
	refreshTitleDescri();
	refreshPrivacy();
	refreshSchedulePopButton();
	refreshCategory();
}

void PLSLiveInfoYoutube::refreshTitleDescri()
{
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	auto data = pPlatformYoutube->getTempSelectDatas();

	ui->lineEditTitle->setText(data.title);
	QString des = data.description;
	if (PLS_PLATFORM_API->isPrepareLive()) {
		QString add_str = tr("LiveInfo.Youtube.description.default.add");
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
	auto data = pPlatformYoutube->getTempSelectDatas();
	ui->comboBoxPublic->clear();
	for (int i = 0; i < pPlatformYoutube->getPrivacyDatas().size(); i++) {
		auto lcoalString = pPlatformYoutube->getPrivacyDatas()[i];
		ui->comboBoxPublic->addItem(lcoalString);

		QString enString = pPlatformYoutube->getPrivacyEnglishDatas()[i];
		if (0 == enString.compare(data.privacyStatus, Qt::CaseInsensitive)) {
			ui->comboBoxPublic->setCurrentIndex(i);
		}
	}
}

void PLSLiveInfoYoutube::refreshCategory()
{
	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	auto data = pPlatformYoutube->getTempSelectDatas();

	ui->comboBoxCategory->clear();
	for (int i = 0; i < pPlatformYoutube->getCategoryDatas().size(); i++) {
		auto categoryData = pPlatformYoutube->getCategoryDatas()[i];
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

	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	auto data = pPlatformYoutube->getTempSelectDatas();

	QString title(data.title);
	if (data.isNormalLive) {
		title = tr("LiveInfo.Schedule.PopUp.New");
	}
	ui->sPushButton->setupButton(title, data.startTimeShort);
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

	PLSScheduleData uiData = pPlatformYoutube->getTempSelectDatas();
	bool isNeedUpdate = PLS_PLATFORM_YOUTUBE->isModifiedWithNewData(ui->lineEditTitle->text(), ui->textEditDescribe->toPlainText(), ui->comboBoxCategory->currentIndex(),
									ui->comboBoxPublic->currentIndex(), &uiData);

	if (PLS_PLATFORM_API->isPrepareLive()) {
		QString add_str = tr("LiveInfo.Youtube.description.default.add");
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
		ComplexItemData data = ComplexItemData();
		data.title = "";
		data.time = tr("LiveInfo.Youtube.loading.scheduled");
		data._id = "";
		data.type = PLSScheduleItemType::Ty_Loading;
		m_vecItemDatas.push_back(data);
	}

	auto _onNext = [=](bool value) {
		Q_UNUSED(value)
		if (ui->sPushButton == nullptr || ui->sPushButton->isMenuNULL() || ui->sPushButton->getMenuHide()) {
			return;
		}

		m_vecItemDatas.clear();

		for (auto data : pPlatformYoutube->getScheduleDatas()) {
			ComplexItemData scheData = ComplexItemData();
			scheData.title = data.title;
			scheData._id = data._id;
			scheData.type = PLSScheduleItemType::Ty_Schedule;
			scheData.time = data.startTimeUTC;
			if ((0 != data._id.compare(pPlatformYoutube->getTempSelectID()))) {
				m_vecItemDatas.push_back(scheData);
			}
		}

		if (pPlatformYoutube->getIsTempSchedule()) {
			ComplexItemData nomarlData = ComplexItemData();
			nomarlData._id = "";
			nomarlData.title = tr("LiveInfo.Schedule.PopUp.New");
			nomarlData.time = tr("LiveInfo.Schedule.PopUp.New");
			nomarlData.type = PLSScheduleItemType::Ty_NormalLive;
			m_vecItemDatas.insert(m_vecItemDatas.begin(), nomarlData);
		}

		if (m_vecItemDatas.size() == 0) {
			ComplexItemData nomarlData = ComplexItemData();
			nomarlData._id = "";
			nomarlData.title = pPlatformYoutube->getNomalLiveDatas().title;
			nomarlData.time = tr("LiveInfo.Youtube.no.scheduled");
			nomarlData.type = PLSScheduleItemType::Ty_Placehoder;
			m_vecItemDatas.insert(m_vecItemDatas.begin(), nomarlData);
		}

		ui->sPushButton->showScheduleMenu(m_vecItemDatas);
	};

	if (nullptr != pPlatformYoutube) {
		pPlatformYoutube->requestSchedule(_onNext, ui->sPushButton);
	}

	ui->sPushButton->showScheduleMenu(m_vecItemDatas);
}

void PLSLiveInfoYoutube::scheduleItemClick(const QString selectID)
{

	for (auto data : m_vecItemDatas) {
		if ((0 != data._id.compare(selectID))) {
			continue;
		}

		PLSScheduleItemType type = data.type;
		if (type != PLSScheduleItemType::Ty_Schedule && type != PLSScheduleItemType::Ty_NormalLive) {
			break;
		}

		auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;

		if (pPlatformYoutube->getTempSelectID().compare(selectID) == 0) {
			break;
		}

		bool isSchedule = type == PLSScheduleItemType::Ty_Schedule;

		pPlatformYoutube->setTempSchedule(isSchedule);
		pPlatformYoutube->setTempSelectID(selectID);

		refreshUI();
		break;
	}
}

void PLSLiveInfoYoutube::doUpdateOkState()
{
	if (ui->lineEditTitle->text().isEmpty()) {
		ui->okButton->setEnabled(false);
		return;
	}

	ui->okButton->setEnabled(PLS_PLATFORM_API->isPrepareLive() || isModified());
}

bool PLSLiveInfoYoutube::isModified()
{
	bool isModified = PLS_PLATFORM_YOUTUBE->isModifiedWithNewData(ui->lineEditTitle->text(), ui->textEditDescribe->toPlainText(), ui->comboBoxCategory->currentIndex(),
								      ui->comboBoxPublic->currentIndex(), nullptr);
	if (!isModified) {
		if (m_enteredID.isEmpty() && PLS_PLATFORM_YOUTUBE->getTempSelectDatas().isNormalLive) {
			isModified = false;
		} else if (m_enteredID.compare(PLS_PLATFORM_YOUTUBE->getTempSelectDatas()._id, Qt::CaseInsensitive) != 0) {
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
		ui->lineEditTitle->setText(newText);
	}
	doUpdateOkState();

	auto pPlatformYoutube = PLS_PLATFORM_YOUTUBE;
	const auto channelName = pPlatformYoutube->getInitData().value(ChannelData::g_channelName).toString();

	if (isLargeToMax) {
		PLSAlertView::warning(this, QTStr("Live.Check.Alert.Title"), QTStr("LiveInfo.Youtube.Title.Length.Check.arg").arg(YoutubeTitleLengthLimit).arg(channelName));
	}

	if (isContainSpecial) {
		PLSAlertView::warning(this, QTStr("Live.Check.Alert.Title"), QTStr("LiveInfo.Youtube.Title.Contain.Special.Text"));
	}
}

void PLSLiveInfoYoutube::descriptionEdited()
{
	static const int YoutubeDescribeLengthLimit = 5000;

	if (ui->textEditDescribe->toPlainText().length() > YoutubeDescribeLengthLimit) {
		ui->textEditDescribe->setText(ui->textEditDescribe->toPlainText().left(YoutubeDescribeLengthLimit));
		PLSAlertView::warning(this, QTStr("Live.Check.Alert.Title"), QTStr("LiveInfo.Youtube.Description.Length.Check.arg"));
	}

	doUpdateOkState();
}
