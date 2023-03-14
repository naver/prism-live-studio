#include "PLSLiveInfoVLive.h"
#include <frontend-api.h>
#include <QDebug>
#include <QDir>
#include <QHBoxLayout>
#include <QImage>
#include <QPixmap>
#include <QWidgetAction>
#include <Vector>

#include <QTextLayout>

#include "../PLSLiveInfoDialogs.h"
#include "../PLSPlatformApi.h"
#include "ChannelCommonFunctions.h"

#include "PLSAPIVLive.h"
#include "PLSChannelDataAPI.h"
#include "alert-view.hpp"
#include "log/log.h"
#include "ui_PLSLiveInfoVLive.h"

static const char *liveInfoMoudule = "PLSLiveInfoVLive";

PLSLiveInfoVLive::PLSLiveInfoVLive(PLSPlatformBase *pPlatformBase, QWidget *parent, PLSDpiHelper dpiHelper)
	: PLSLiveInfoBase(pPlatformBase, parent, dpiHelper), ui(new Ui::PLSLiveInfoVLive), m_platform(dynamic_cast<PLSPlatformVLive *>(pPlatformBase))
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSLiveInfoVLive});

	dpiHelper.notifyDpiChanged(this, [this](double, double, bool firstShow) {
		if (firstShow) {
			QMetaObject::invokeMethod(
				this, [this] { pls_flush_style(ui->lineEditTitle); }, Qt::QueuedConnection);
		}
	});
	PLS_INFO(liveInfoMoudule, "VLive liveinfo Will show");
	ui->setupUi(this->content());

	setHasCloseButton(false);
	setHasBorder(true);
	this->setWindowTitle(tr("LiveInfo.liveinformation"));

	ui->scheCombox->setFocusPolicy(Qt::NoFocus);
	ui->profileCombox->setFocusPolicy(Qt::NoFocus);
	ui->boardCombox->setFocusPolicy(Qt::NoFocus);
	content()->setFocusPolicy(Qt::StrongFocus);

	m_platform->liveInfoisShowing();
	m_enteredID = m_platform->getSelectData()._id;

	setupFirstUI();

	m_platform->setAlertParent(this);
	updateStepTitle(ui->okButton);
	if (!PLS_PLATFORM_API->isPrepareLive()) {
		ui->bottomButtonWidget->layout()->addWidget(ui->cancelButton);
		ui->rehearsalButton->setHidden(true);
	}
}

PLSLiveInfoVLive::~PLSLiveInfoVLive()
{
	delete ui;
}
void PLSLiveInfoVLive::updateUIEnable()
{
	doUpdateOkState();
	ui->scheCombox->setButtonEnable(!m_platform->getTempProfileData().memberId.isEmpty());
	if (PLS_PLATFORM_API->isLiving()) {
		ui->thumbnailButton->setEnabled(false);
		ui->scheCombox->setButtonEnable(false);
		ui->lineEditTitle->setEnabled(false);
	}
}

void PLSLiveInfoVLive::updateProfileAndBoardUI()
{
	auto &profileData = m_platform->getTempProfileData();
	if (profileData.memberId.isEmpty()) {
		ui->profileCombox->updateTitle(tr("LiveInfo.live.title.select.profile"));
		if (m_platform->getTempBoardData().boardId > 0) {
			m_platform->setTempBoardData(PLSVLiveBoardData());
		}
	} else {
		ui->profileCombox->updateTitle(profileData.nickname);
	}

	auto &boardData = m_platform->getTempBoardData();
	bool isNotSelectBoard = boardData.boardId <= 0;
	ui->userWidget->setHidden(isNotSelectBoard);
	if (isNotSelectBoard) {
		ui->boardCombox->updateTitle(tr("LiveInfo.live.title.select.board"));
	} else {
		ui->boardCombox->updateTitle(boardData.title);
	}

	if (profileData.memberId.isEmpty() || m_platform->getIsTempSchedule()) {
		ui->boardCombox->setButtonEnable(false);
	} else {
		ui->boardCombox->setButtonEnable(true);
	}

	if (PLS_PLATFORM_API->isLiving()) {
		ui->profileCombox->setButtonEnable(false);
		ui->boardCombox->setButtonEnable(false);
	}

	ui->userBoardLeftLabel->setHidden(isNotSelectBoard);
	ui->usrLabel->setHidden(isNotSelectBoard);
	ui->boardTipLabel->setHidden(isNotSelectBoard);

	if (isNotSelectBoard) {
		ui->countryLeftLabel->setHidden(isNotSelectBoard);
		ui->countryLabel->setHidden(isNotSelectBoard);
	} else {
		updateBoardUserLabelElid();
		updateCountryLabels();
	}
}

void PLSLiveInfoVLive::updateBoardUserLabelElid()
{
	if (ui->usrLabel->isHidden()) {
		return;
	}

	const auto &text = m_platform->getTempBoardData().readAllowedLabel;

	const static int maxLineLimit = 2;
	QFontMetrics fontWidth(ui->usrLabel->font());
	QTextLayout textLayout(text);
	textLayout.setFont(ui->usrLabel->font());
	int widthTotal = 0;
	int lineCount = 0;

	textLayout.beginLayout();
	while (++lineCount <= maxLineLimit) {
		QTextLine line = textLayout.createLine();
		if (!line.isValid())
			break;
		line.setLineWidth(ui->usrLabel->width());
		widthTotal += line.naturalTextWidth();
	}
	textLayout.endLayout();

	QString elidedText = fontWidth.elidedText(text, Qt::ElideRight, widthTotal);
	ui->usrLabel->setText(elidedText);
}

void PLSLiveInfoVLive::updateCountryLabels()
{

	auto &boardData = m_platform->getTempBoardData();
	const auto &includedCountries = boardData.includedCountries;
	QStringList countriesList;
	for (int i = 0; i < includedCountries.size(); i++) {
		countriesList << includedCountries[i].toString();
	}

	const auto &excludedCountries = boardData.excludedCountries;

	QJsonArray selectCoun;
	if (!excludedCountries.isEmpty()) {
		ui->countryLeftLabel->setText(tr("LiveInfo.live.title.board.country.unlist"));
		selectCoun = excludedCountries;
	} else if (!includedCountries.isEmpty()) {
		ui->countryLeftLabel->setText(tr("LiveInfo.live.title.board.country.list"));
		selectCoun = includedCountries;
	}

	if (!selectCoun.isEmpty()) {
		auto showStr = selectCoun[0].toString();
		showStr = PLSVliveStatic::instance()->convertToFullName(showStr);
		if (selectCoun.size() - 1 > 0) {
			showStr.append(tr("LiveInfo.live.title.board.country.other").arg(selectCoun.size() - 1));
		}
		ui->countryLabel->setText(showStr);
	}
	bool willHidden = includedCountries.isEmpty() && excludedCountries.isEmpty();

	if (willHidden) {
		ui->userFormLayout->removeWidget(ui->countryLeftLabel);
		ui->userFormLayout->removeWidget(ui->countryLabel);
	} else {
		ui->userFormLayout->insertRow(2, ui->countryLeftLabel, ui->countryLabel);
	}

	ui->countryLeftLabel->setHidden(willHidden);
	ui->countryLabel->setHidden(willHidden);
}

void PLSLiveInfoVLive::getBoardDetailData()
{
	//m_platform->requestBoardDetail([=](bool) { /*updateBoardUserLabelElid();*/ }, this);
}

void PLSLiveInfoVLive::setupFirstUI()
{
	ui->profileLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("LiveInfo.profile.title")));
	ui->boardLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("LiveInfo.board.title")));

	ui->lineEditTitle->setText(m_platform->getTempSelectData().title);
	ui->titleWidget->layout()->addWidget(createResolutionButtonsFrame());

	ui->uerScrollArea->verticalScrollBar()->setEnabled(false);

	refreshUI();
	ui->lineEditTitle->setCursorPosition(0);
	connect(ui->thumbnailButton, &PLSSelectImageButton::takeButtonClicked, this, []() { PLS_UI_STEP(liveInfoMoudule, "VLIVE LiveInfo's Thumbnail Take Button", ACTION_CLICK); });
	connect(ui->thumbnailButton, &PLSSelectImageButton::selectButtonClicked, this, []() { PLS_UI_STEP(liveInfoMoudule, "VLIVE LiveInfo's Thumbnail Select Button", ACTION_CLICK); });
	connect(ui->thumbnailButton, &PLSSelectImageButton::imageSelected, this, &PLSLiveInfoVLive::onImageSelected);

	connect(ui->scheCombox, &PLSScheduleCombox::pressed, this, &PLSLiveInfoVLive::scheduleButtonClicked);
	connect(ui->scheCombox, &PLSScheduleCombox::menuItemClicked, this, &PLSLiveInfoVLive::scheduleItemClick);
	connect(ui->scheCombox, &PLSScheduleCombox::menuItemExpired, this, &PLSLiveInfoVLive::scheduleItemExpired);

	connect(ui->profileCombox, &PLSScheduleCombox::pressed, this, &PLSLiveInfoVLive::profileComboxClicked);
	connect(ui->profileCombox, &PLSScheduleCombox::menuItemClicked, this, &PLSLiveInfoVLive::profileMenuItemClick);

	connect(ui->boardCombox, &PLSScheduleCombox::pressed, this, &PLSLiveInfoVLive::boardComboxClicked);
	connect(ui->boardCombox, &PLSScheduleCombox::menuItemClicked, this, &PLSLiveInfoVLive::boardMenuItemClick);

	connect(ui->lineEditTitle, &QLineEdit::textChanged, this, &PLSLiveInfoVLive::titleEdited, Qt::QueuedConnection);

	connect(ui->rehearsalButton, &QPushButton::clicked, this, &PLSLiveInfoVLive::rehearsalButtonClicked);
	connect(ui->okButton, &QPushButton::clicked, this, &PLSLiveInfoVLive::okButtonClicked);
	connect(ui->cancelButton, &QPushButton::clicked, this, &PLSLiveInfoVLive::cancelButtonClicked);
	connect(
		m_platform, &PLSPlatformVLive::closeDialogByExpired, this, [=]() { reject(); }, Qt::DirectConnection);

	connect(
		m_platform, &PLSPlatformVLive::toShowLoading, this,
		[=](bool isShow) {
			if (isShow) {
				showLoading(this->content());
			} else {

				hideLoading();
			}
		},
		Qt::DirectConnection);
	connect(m_platform, &PLSPlatformVLive::profileIsInvalid, this, &PLSLiveInfoVLive::refreshUI, Qt::DirectConnection);
}

void PLSLiveInfoVLive::showEvent(QShowEvent *event)
{
	Q_UNUSED(event)
	showLoading(content());

	__super::showEvent(event);

	auto _onNext = [=](bool value) {
		hideLoading();

		if (!value) {
			reject();
		}
	};
	if (m_platform->getSelectData().isNormalLive || PLS_PLATFORM_API->isLiving()) {
		_onNext(true);
		return;
	}

	m_platform->requestUpdateScheduleList(
		[=](bool isSucceed, bool isDelete) {
			if (!isSucceed) {
				_onNext(false);
				return;
			}

			if (isDelete) {
				m_platform->reInitLiveInfo(true, true);
			} else {
				m_platform->setSelectDataByID(m_platform->getSelectID());
			}
			m_platform->liveInfoisShowing();
			refreshUI();
			_onNext(true);
		},
		this);
}

void PLSLiveInfoVLive::refreshUI()
{
	refreshTitleEdit();
	refreshSchedulePopButton();
	refreshThumbnailButton();

	updateProfileAndBoardUI();
	updateUIEnable();
}

void PLSLiveInfoVLive::refreshTitleEdit()
{
	const auto &data = m_platform->getTempSelectData();
	bool isProfileSelect = !m_platform->getTempProfileData().memberId.isEmpty();
	ui->lineEditTitle->setText(data.title);
	ui->lineEditTitle->setEnabled(data.isNormalLive && isProfileSelect);
}
void PLSLiveInfoVLive::refreshSchedulePopButton()
{

	const auto &data = m_platform->getTempSelectData();

	PLSScheComboxItemData scheData = PLSScheComboxItemData();
	scheData.title = data.isNormalLive ? tr("New") : data.title;
	scheData._id = data._id;
	scheData.type = data.isNormalLive ? PLSScheComboxItemType::Ty_NormalLive : PLSScheComboxItemType::Ty_Schedule;
	scheData.time = data.startTimeShort;
	scheData.isNewLive = data.isNewLive;
	scheData.timeStamp = data.startTimeStamp;
	scheData.endTimeStamp = data.endTimeStamp;
	scheData.needShowTimeLeftTimer = PLS_PLATFORM_API->isLiving() ? false : true;
	ui->scheCombox->setupButton(scheData);
}

void PLSLiveInfoVLive::refreshThumbnailButton()
{
	ui->thumDetailLabel->setHidden(m_platform->getTempSelectData().isNormalLive);
	const auto &data = m_platform->getTempSelectData();
	ui->thumbnailButton->setPixmap(data.pixMap);
	if (!data.thumRemoteUrl.isEmpty() && data.pixMap.isNull()) {
		m_platform->downloadThumImage([=]() { refreshThumbnailButton(); }, ui->thumbnailButton);
	}
	ui->thumbnailButton->setEnabled(data.isNormalLive);
}

void PLSLiveInfoVLive::updateScheduleComboxItems()
{
	if (ui->scheCombox == nullptr || ui->scheCombox->isMenuNULL() || ui->scheCombox->getMenuHide()) {
		return;
	}

	m_vecItemDatas.clear();
	for (const auto &data : m_platform->getScheduleDatas()) {
		PLSScheComboxItemData scheData = PLSScheComboxItemData();
		scheData.title = data.title;
		scheData._id = data._id;
		scheData.type = PLSScheComboxItemType::Ty_Schedule;
		scheData.time = data.startTimeUTC;
		scheData.isNewLive = data.isNewLive;
		scheData.timeStamp = data.startTimeStamp;
		scheData.endTimeStamp = data.endTimeStamp;
		scheData.needShowTimeLeftTimer = true;

		if (data._id != m_platform->getTempSelectID()) {
			m_vecItemDatas.push_back(scheData);
		}
	}

	if (m_platform->getIsTempSchedule()) {
		PLSScheComboxItemData nomarlData = PLSScheComboxItemData();
		nomarlData._id = "";
		nomarlData.title = tr("New");
		nomarlData.time = "";
		nomarlData.type = PLSScheComboxItemType::Ty_NormalLive;
		m_vecItemDatas.insert(m_vecItemDatas.begin(), nomarlData);
	}

	{
		//add a placeholder item, if item is 0, then will show this.
		PLSScheComboxItemData nomarlData = PLSScheComboxItemData();
		nomarlData._id = "";
		nomarlData.title = tr("New");
		nomarlData.time = tr("LiveInfo.Youtube.no.scheduled");
		nomarlData.type = PLSScheComboxItemType::Ty_Placehoder;
		m_vecItemDatas.insert(m_vecItemDatas.begin(), nomarlData);
	}

	ui->scheCombox->showScheduleMenu(m_vecItemDatas);
}

void PLSLiveInfoVLive::onImageSelected(const QString &imageFilePath)
{
	if (QFile::exists(imageFilePath)) {
		QFile::remove(imageFilePath);
	}

	updateUIEnable();
}

void PLSLiveInfoVLive::saveTempNormalDataWhenSwitch()
{
	PLSVLiveLiveinfoData &tempData = m_platform->getTempNormalData();
	tempData.title = ui->lineEditTitle->text();
	tempData.pixMap = ui->thumbnailButton->getOriginalPixmap();
	tempData.board = m_platform->getTempBoardData();
	tempData.profile = m_platform->getTempProfileData();
}

void PLSLiveInfoVLive::okButtonClicked()
{
	m_platform->setIsRehearsal(false);
	PLS_UI_STEP(liveInfoMoudule, "VLIVE liveinfo OK Button Click", ACTION_CLICK);
	saveDateWhenClickButton();
}

void PLSLiveInfoVLive::cancelButtonClicked()
{
	PLS_UI_STEP(liveInfoMoudule, "VLIVE liveinfo Cancel Button Click", ACTION_CLICK);
	reject();
}

void PLSLiveInfoVLive::rehearsalButtonClicked()
{

	PLS_UI_STEP(liveInfoMoudule, "VLIVE liveinfo Rehearsal Button Click", ACTION_CLICK);
	m_platform->setIsRehearsal(true);
	saveDateWhenClickButton();
}

void PLSLiveInfoVLive::saveDateWhenClickButton()
{
	auto _onNext = [=](bool isSucceed) {
		hideLoading();
		PLS_INFO(liveInfoMoudule, "vlive liveinfo Save %s", (isSucceed ? "succeed" : "failed"));
		if (isSucceed) {
			accept();
		}
	};

	showLoading(content());

	PLSVLiveLiveinfoData uiData = m_platform->getTempSelectData();

	m_platform->isModifiedWithNewData(ui->lineEditTitle->text(), ui->thumbnailButton->getOriginalPixmap(), &uiData);
	if (ui->lineEditTitle->text().trimmed().isEmpty()) {
		uiData.title = m_platform->getDefaultTitle(uiData.profile.nickname);
	}
	m_platform->saveSettings(_onNext, uiData);
}

void PLSLiveInfoVLive::scheduleButtonClicked()
{
	PLS_UI_STEP(liveInfoMoudule, "VLive liveinfo schedule pop button click", ACTION_CLICK);
	if (!ui->scheCombox->getMenuHide()) {
		return;
	}
	m_vecItemDatas.clear();
	for (int i = 0; i < 1; i++) {
		PLSScheComboxItemData data = PLSScheComboxItemData();
		data.title = tr("LiveInfo.live.loading.scheduled");
		data.type = PLSScheComboxItemType::Ty_Loading;
		m_vecItemDatas.push_back(data);
	}

	auto _onNext = [=](bool value) {
		Q_UNUSED(value)
		updateScheduleComboxItems();
	};

	if (nullptr != m_platform) {
		m_platform->requestScheduleList(_onNext, ui->scheCombox);
	}

	ui->scheCombox->showScheduleMenu(m_vecItemDatas);
}

void PLSLiveInfoVLive::scheduleItemClick(const QString selectID)
{

	for (const auto &data : m_vecItemDatas) {
		PLSScheComboxItemType type = data.type;
		if (type == PLSScheComboxItemType::Ty_Loading || type == PLSScheComboxItemType::Ty_Placehoder) {
			continue;
		}

		if (m_platform->getTempSelectID() == selectID) {
			//if select same id, ignore
			break;
		}
		if (data._id != selectID) {
			continue;
		}

		bool isSchedule = type == PLSScheComboxItemType::Ty_Schedule;
		if (m_platform->getTempSelectData().isNormalLive && isSchedule) {
			//normal to schedule, temp saved.
			saveTempNormalDataWhenSwitch();
		}
		m_platform->setTempSchedule(isSchedule);
		m_platform->setTempSelectID(selectID);
		m_platform->setTempBoardData(m_platform->getTempSelectData().board);
		m_platform->setTempProfileData(m_platform->getTempSelectData().profile);
		if (isSchedule) {
			getBoardDetailData();
		}
		refreshUI();
		break;
	}
}

void PLSLiveInfoVLive::scheduleItemExpired(vector<QString> &ids)
{
	m_platform->removeExpiredSchedule(ids);
	updateScheduleComboxItems();
}

void PLSLiveInfoVLive::profileComboxClicked()
{
	PLS_UI_STEP(liveInfoMoudule, __FUNCTION__, ACTION_CLICK);
	if (!ui->profileCombox->getMenuHide()) {
		return;
	}
	vector<PLSScheComboxItemData> items;
	PLSScheComboxItemData data = PLSScheComboxItemData();
	data.title = tr("LiveInfo.live.loading.scheduled");
	data.type = PLSScheComboxItemType::Ty_Loading;
	data.itemHeight = s_itemHeight_53;
	items.push_back(data);

	auto _onNext = [=](bool value) {
		Q_UNUSED(value)
		updateProfileComboxItems();
	};

	if (nullptr != m_platform) {
		m_platform->requestProfileList(_onNext, ui->profileCombox);
	}

	ui->profileCombox->showScheduleMenu(items);
}

void PLSLiveInfoVLive::profileMenuItemClick(const QString selectID)
{
	if (selectID.isEmpty() || m_platform->getTempProfileData().memberId == selectID) {
		return;
	}
	for (const auto &data : m_platform->getProfileDatas()) {
		if (data.memberId != selectID) {
			continue;
		}
		m_platform->setTempProfileData(data);
		m_platform->setTempBoardData({});
		ui->lineEditTitle->setText(m_platform->getDefaultTitle(data.nickname));

		if (!m_platform->getTempSelectData().isNormalLive) {
			m_platform->setTempSchedule(false);
			m_platform->setTempSelectID({});
			m_platform->setThumPixmap({});
			m_platform->getTempNormalData().title = ui->lineEditTitle->text();
		} else {
			saveTempNormalDataWhenSwitch();
		}
		break;
	}

	refreshUI();
}

void PLSLiveInfoVLive::updateProfileComboxItems()
{
	if (ui->profileCombox == nullptr || ui->profileCombox->isMenuNULL() || ui->profileCombox->getMenuHide()) {
		return;
	}

	vector<PLSScheComboxItemData> items;
	for (const auto &data : m_platform->getProfileDatas()) {
		PLSScheComboxItemData itemData = PLSScheComboxItemData();
		itemData.title = data.nickname;
		itemData._id = data.memberId;
		itemData.imgUrl = data.profileImageUrl;
		itemData.type = PLSScheComboxItemType::Ty_NormalLive;
		itemData.itemHeight = s_itemHeight_53;
		if (!data.memberId.isEmpty() && data.memberId == m_platform->getTempProfileData().memberId) {
			itemData.isSelect = true;
		}
		items.push_back(itemData);
	}

	if (m_platform->getProfileDatas().empty()) {
		PLSScheComboxItemData nomarlData = PLSScheComboxItemData();
		nomarlData._id = "";
		nomarlData.title = tr("LiveInfo.live.profile.no.data");
		nomarlData.type = PLSScheComboxItemType::Ty_Placehoder;
		nomarlData.itemHeight = s_itemHeight_53;
		items.push_back(nomarlData);
	}

	ui->profileCombox->showScheduleMenu(items);
}

void PLSLiveInfoVLive::boardComboxClicked()
{
	PLS_UI_STEP(liveInfoMoudule, __FUNCTION__, ACTION_CLICK);
	if (!ui->boardCombox->getMenuHide()) {
		return;
	}
	vector<PLSScheComboxItemData> items;
	PLSScheComboxItemData data = PLSScheComboxItemData();
	data.title = tr("LiveInfo.live.loading.scheduled");
	data.type = PLSScheComboxItemType::Ty_Loading;
	data.itemHeight = s_itemHeight_40;
	items.push_back(data);

	auto _onNext = [=](bool value) {
		Q_UNUSED(value)
		updateBoardComboxItems();
	};

	if (nullptr != m_platform) {
		m_platform->requestBoardList(_onNext, ui->boardCombox);
	}

	ui->boardCombox->showScheduleMenu(items);
}

void PLSLiveInfoVLive::boardMenuItemClick(const QString selectID)
{
	if (selectID.isEmpty()) {
		return;
	}
	auto id_int = selectID.toInt();

	for (const auto &data : m_platform->getBoardDatas()) {
		if (data.boardId == id_int) {
			m_platform->setTempBoardData(data);
			ui->boardCombox->updateTitle(data.title);
			break;
		}
	}
	getBoardDetailData();
	updateProfileAndBoardUI();
	updateUIEnable();
}

void PLSLiveInfoVLive::updateBoardComboxItems()
{
	if (ui->boardCombox == nullptr || ui->boardCombox->isMenuNULL() || ui->boardCombox->getMenuHide()) {
		return;
	}

	vector<PLSScheComboxItemData> items;
	auto selectboardID = m_platform->getTempBoardData().boardId;
	for (const auto &data : m_platform->getBoardDatas()) {
		PLSScheComboxItemData itemData = PLSScheComboxItemData();
		itemData.title = data.title;
		itemData.isShowRightIcon = !data.expose;
		itemData._id = QString::number(data.boardId);
		itemData.type = data.isGroup ? PLSScheComboxItemType::Ty_Header : PLSScheComboxItemType::Ty_NormalLive;
		itemData.itemHeight = itemData.type == PLSScheComboxItemType::Ty_Header ? s_itemHeight_30 : s_itemHeight_40;

		if (data.boardId > 0 && data.boardId == selectboardID) {
			itemData.isSelect = true;
		}
		items.push_back(itemData);
	}

	if (m_platform->getBoardDatas().empty()) {
		PLSScheComboxItemData nomarlData = PLSScheComboxItemData();
		nomarlData._id = "";
		nomarlData.title = tr("LiveInfo.live.board.no.data");
		nomarlData.type = PLSScheComboxItemType::Ty_Placehoder;
		nomarlData.itemHeight = s_itemHeight_40;
		items.push_back(nomarlData);
	}

	ui->boardCombox->showScheduleMenu(items);
}

void PLSLiveInfoVLive::doUpdateOkState()
{
	if (m_platform->getTempProfileData().memberId.isEmpty() || m_platform->getTempBoardData().boardId <= 0) {
		ui->okButton->setEnabled(false);
		ui->rehearsalButton->setEnabled(false);
		return;
	}

	ui->okButton->setEnabled(true);
	ui->rehearsalButton->setEnabled(true);
}

bool PLSLiveInfoVLive::isModified()
{
	bool isModified = m_platform->isModifiedWithNewData(ui->lineEditTitle->text(), ui->thumbnailButton->getOriginalPixmap(), nullptr);
	if (!isModified && m_enteredID != m_platform->getTempSelectData()._id) {
		isModified = true;
	}
	return isModified;
}

QSize PLSLiveInfoVLive::getNeedDialogSize(double dpi)
{
	const static int windowWidth = 720;
	const static int windowHeightLow = 550;
	const static int windowHeightTop = 665;
	auto &boardData = m_platform->getTempBoardData();
	bool isNotSelectBoard = boardData.boardId <= 0;
	return PLSDpiHelper::calculate(dpi, QSize(windowWidth, isNotSelectBoard ? windowHeightLow : windowHeightTop));
}

void PLSLiveInfoVLive::titleEdited()
{
	static const int TitleLengthLimit = 100;
	QString newText = ui->lineEditTitle->text();

	bool isLargeToMax = false;
	if (newText.length() > TitleLengthLimit) {
		isLargeToMax = true;
		newText = newText.left(TitleLengthLimit);
	}

	if (newText.compare(ui->lineEditTitle->text()) != 0) {
		QSignalBlocker signalBlocker(ui->lineEditTitle);
		ui->lineEditTitle->setText(newText);
	}
	updateUIEnable();

	if (isLargeToMax) {
		const auto channelName = m_platform->getInitData().value(ChannelData::g_platformName).toString();
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("LiveInfo.Title.Length.Check.arg").arg(TitleLengthLimit).arg("V LIVE"));
	}
}

void PLSLiveInfoVLive::resizeEvent(QResizeEvent *event)
{
	updateBoardUserLabelElid();
	__super::resizeEvent(event);
}
