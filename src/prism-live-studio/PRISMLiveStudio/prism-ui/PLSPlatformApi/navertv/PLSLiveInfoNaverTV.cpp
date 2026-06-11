#include "PLSLiveInfoNaverTV.h"
#include "ui_PLSLiveInfoNaverTV.h"

#include "../PLSPlatformApi.h"
#include "../common/PLSDateFormate.h"
#include "PLSAlertView.h"
#include "PLSChannelDataAPI.h"
#include "libui.h"
#include "liblog.h"
constexpr const char *liveInfoMoudule = "PLSLiveInfoNaverTV";

PLSLiveInfoNaverTV::PLSLiveInfoNaverTV(PLSPlatformBase *pPlatformBase, const QVariantMap &info, QWidget *parent)
	: PLSLiveInfoBase(pPlatformBase, parent), platform(dynamic_cast<PLSPlatformNaverTV *>(pPlatformBase)), srcInfo(info)
{
	ui = pls_new<Ui::PLSLiveInfoNaverTV>();

	pls_add_css(this, {"PLSLiveinfoNaverTV"});

	platform->setAlertParent(this);

	setupUi(ui);
	setResizeEnabled(false);
	ui->nvtTitleWidget->installEventFilter(this);

	QHBoxLayout *hl = pls_new<QHBoxLayout>();
	hl->setContentsMargins(0, 35, 24, 0);
	hl->setSpacing(0);

	ui->dualWidget->setText(tr("NAVER TV"))->setUUID(platform->getChannelUUID());

	QWidget *resolutionButton = createResolutionButtonsFrame();
	hl->addStretch(1);
	hl->addWidget(resolutionButton);

	QVBoxLayout *vl = pls_new<QVBoxLayout>();
	vl->setContentsMargins(0, 0, 0, 0);
	vl->setSpacing(0);
	vl->addWidget(ui->thumbnailButton, 0, Qt::AlignHCenter);
	vl->addSpacing(7);
	vl->addWidget(ui->nvtGuidelabel);

	QVBoxLayout *vl1 = pls_new<QVBoxLayout>(ui->nvtTitleWidget);
	vl1->setContentsMargins(0, 0, 0, 0);
	vl1->setSpacing(0);
	vl1->addLayout(hl);
	vl1->addSpacing(23);
	vl1->addLayout(vl);
	vl1->addStretch(1);

	setHasCloseButton(false);

	ui->scheCombox->setFocusPolicy(Qt::NoFocus);
	content()->setFocusPolicy(Qt::StrongFocus);

	auto liveInfo = platform->getLiveInfo();

	selectedId = liveInfo->oliveId;

	ui->lineEditTitle->setText(liveInfo->title);
	ui->rehearsalButton->setVisible(PLS_PLATFORM_API->isPrepareLive() && PLSCHANNELS_API->currentSelectedCount() == 1);

	connect(ui->okButton, &QPushButton::clicked, this, &PLSLiveInfoNaverTV::okButtonClicked);
	connect(ui->cancelButton, &QPushButton::clicked, this, &PLSLiveInfoNaverTV::cancelButtonClicked);
	connect(ui->rehearsalButton, &QPushButton::clicked, this, &PLSLiveInfoNaverTV::rehearsalButtonClicked);
	connect(ui->thumbnailButton, &PLSSelectImageButton::takeButtonClicked, this, []() { PLS_UI_STEP(liveInfoMoudule, "Naver TV LiveInfo's Thumbnail Take Button", ACTION_CLICK); });
	connect(ui->thumbnailButton, &PLSSelectImageButton::selectButtonClicked, this, []() { PLS_UI_STEP(liveInfoMoudule, "Naver TV LiveInfo's Thumbnail Select Button", ACTION_CLICK); });
	connect(ui->thumbnailButton, &PLSSelectImageButton::imageSelected, this, &PLSLiveInfoNaverTV::onImageSelected);

	connect(platform, &PLSPlatformNaverTV::apiRequestFailed, this, &PLSLiveInfoNaverTV::onApiRequestFailed, Qt::DirectConnection);
	connect(platform, &PLSPlatformNaverTV::closeDialogByExpired, this, &PLSLiveInfoNaverTV::reject, Qt::DirectConnection);

	connect(ui->scheCombox, &PLSScheduleCombox::pressed, this, &PLSLiveInfoNaverTV::scheduleButtonClicked);
	connect(ui->scheCombox, &PLSScheduleCombox::menuItemClicked, this, &PLSLiveInfoNaverTV::scheduleItemClick);

	connect(ui->lineEditTitle, &QLineEdit::textChanged, this, &PLSLiveInfoNaverTV::titleEdited, Qt::QueuedConnection);

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStartBroadcastInInfoView, this, &PLSLiveInfoNaverTV::okButtonClicked);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStartRehearsal, this, &PLSLiveInfoNaverTV::rehearsalButtonClicked);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStopRehearsal, this, &PLSLiveInfoNaverTV::cancelButtonClicked);

	updateStepTitle(ui->okButton);

	ui->nvtGuidelabel->setText(liveInfo->isScheLive ? tr("LiveInfo.thum.schedule.diabale.edit") : QString());
	ui->scheCombox->setupButton(liveInfo->isScheLive ? liveInfo->title : tr("New"), PLSDateFormate::timeStampToShortString((long)(liveInfo->startDate / 1000)));
	ui->thumbnailButton->setImagePath(liveInfo->thumbnailImagePath);

	bool isLiving = PLS_PLATFORM_API->isLiveStarted() || PLS_PLATFORM_API->isLiving();
	ui->thumbnailButton->setButtonEnabled(!isLiving && !liveInfo->isScheLive);
	ui->scheCombox->setButtonEnable(!isLiving);
	ui->lineEditTitle->setEnabled(!isLiving && !liveInfo->isScheLive);
	ui->lineEditTitle->setCursorPosition(0);
	doUpdateOkState();

	if (!liveInfo->isScheLive) {
		tmpNewLiveInfoTitle = liveInfo->title;
		tmpNewLiveInfoThumbnailPath = liveInfo->thumbnailImagePath;
	} else if (!isLiving) {
		refreshingScheLiveInfo = true;
		platform->getScheLives(
			[this, liveInfo](bool ok, int code, const QList<PLSPlatformNaverTV::LiveInfo> &scheLiveInfos) {
				if (pls_object_is_valid(this)) {
					processGetScheLives(ok, code, scheLiveInfos, liveInfo);
				}
			},
			this, true, false);
	}
#if defined(Q_OS_WIN)
	if (!PLS_PLATFORM_API->isPrepareLive()) {
		ui->horizontalLayout->addWidget(ui->cancelButton);
	}
#endif
}

PLSLiveInfoNaverTV::~PLSLiveInfoNaverTV()
{
	pls_delete(ui, nullptr);
}

void PLSLiveInfoNaverTV::onOk(bool isRehearsal)
{
	if (!PLS_PLATFORM_API->isPrepareLive() && !platform->isLiveInfoModified(selectedId, ui->lineEditTitle->text(), ui->thumbnailButton->getImagePath())) {
		reject();
		return;
	}

	auto _onNext = [this, isRehearsal](bool ok, int code) {
		if (!pls_object_is_valid(this)) {
			return;
		}

		hideLoading();

		PLS_INFO(liveInfoMoudule, "Naver TV liveinfo Save %s", (ok ? "success" : "failed"));
		if (ok) {
			accept();
		}
	};

	showLoading(content());

	QString customErrName("UpdateLiveInfoFailedNoService");
	if (isRehearsal) {
		customErrName = "StartRehearsalFailed";
	} else if (PLS_PLATFORM_API->isPrepareLive()) {
		customErrName = "FailedBecauseConnection";
	}

	auto liveInfo = platform->isLiveInfoModified(selectedId) ? getScheLiveInfo(selectedId) : platform->getScheLiveInfo(selectedId);
	if (!liveInfo) {
		platform->updateLiveInfo(scheLiveList, selectedId, isRehearsal, ui->lineEditTitle->text(), ui->thumbnailButton->getImagePath(), _onNext, customErrName);
	} else {
		platform->checkScheLive(liveInfo->oliveId, [this, isRehearsal, _onNext, customErrName](bool ok, bool valid) {
			if (!pls_object_is_valid(this)) {
				return;
			}

			if (!ok || !valid) {
				hideLoading();
				pls_alert_error_message(this, tr("Alert.Title"), tr("broadcast.invalid.schedule"));
			} else {
				platform->updateLiveInfo(scheLiveList, selectedId, isRehearsal, ui->lineEditTitle->text(), ui->thumbnailButton->getImagePath(), _onNext, customErrName);
			}
		});
	}
}

void PLSLiveInfoNaverTV::okButtonClicked()
{
	PLS_UI_STEP(liveInfoMoudule, "Naver TV liveinfo OK Button Click", ACTION_CLICK);
	onOk(false);
}

void PLSLiveInfoNaverTV::cancelButtonClicked()
{
	PLS_UI_STEP(liveInfoMoudule, "Naver TV liveinfo Cancel Button Click", ACTION_CLICK);
	reject();
}

void PLSLiveInfoNaverTV::rehearsalButtonClicked()
{
	PLS_UI_STEP(liveInfoMoudule, "Naver TV liveinfo Rehearsal Button Click", ACTION_CLICK);
	onOk(true);
}

void PLSLiveInfoNaverTV::scheduleButtonClicked()
{
	PLS_UI_STEP(liveInfoMoudule, "Naver TV liveinfo schedule pop button click", ACTION_CLICK);
	if (!ui->scheCombox->getMenuHide()) {
		return;
	}

	scheLiveItems.clear();
	for (int i = 0; i < 1; i++) {
		PLSScheComboxItemData data;
		data.title = tr("LiveInfo.live.loading.scheduled");
		data.type = PLSScheComboxItemType::Ty_Loading;
		scheLiveItems.push_back(data);
	}

	platform->getScheLives(
		[this](bool ok, const QList<PLSPlatformNaverTV::LiveInfo> &scheliveInfos) {
			if (pls_object_is_valid(this)) {
				processRefreshScheLives(ok, scheliveInfos);
			}
		},
		this);

	ui->scheCombox->showScheduleMenu(scheLiveItems);
}

void PLSLiveInfoNaverTV::scheduleItemClick(const QString &selectID)
{
	PLS_UI_STEP(liveInfoMoudule, "Naver TV liveinfo select schedule button click", ACTION_CLICK);

	auto iter = std::find_if(scheLiveItems.begin(), scheLiveItems.end(), [selectID](const PLSScheComboxItemData &v) { return v._id == selectID; });
	if (iter == scheLiveItems.end()) {
		return;
	}

	const PLSScheComboxItemData &data = *iter;
	PLSScheComboxItemType type = data.type;
	if (type != PLSScheComboxItemType::Ty_Schedule && type != PLSScheComboxItemType::Ty_NormalLive) {
		return;
	}

	if (selectedId < 0) {
		tmpNewLiveInfoTitle = ui->lineEditTitle->text();
		tmpNewLiveInfoThumbnailPath = ui->thumbnailButton->getImagePath();
	}

	selectedId = selectID.toInt();
	auto scheLiveInfo = getScheLiveInfo(selectedId);

	if (scheLiveInfo) {
		ui->nvtGuidelabel->setText(tr("LiveInfo.thum.schedule.diabale.edit"));
		ui->scheCombox->setupButton(scheLiveInfo->title, PLSDateFormate::timeStampToShortString((long)(scheLiveInfo->startDate / 1000)));
		ui->thumbnailButton->setImagePath(scheLiveInfo->thumbnailImagePath);
		ui->lineEditTitle->setText(scheLiveInfo->title);

		ui->thumbnailButton->setButtonEnabled(false);
		ui->scheCombox->setButtonEnable(!PLS_PLATFORM_API->isLiving());
		ui->lineEditTitle->setEnabled(false);
	} else {
		ui->nvtGuidelabel->setText(QString());
		ui->scheCombox->setupButton(tr("New"), PLSDateFormate::timeStampToShortString(0));
		ui->thumbnailButton->setImagePath(tmpNewLiveInfoThumbnailPath);
		ui->lineEditTitle->setText(tmpNewLiveInfoTitle.isEmpty() ? platform->getImmediateLiveInfo()->title : tmpNewLiveInfoTitle);

		ui->thumbnailButton->setButtonEnabled(true);
		ui->scheCombox->setButtonEnable(true);
		ui->lineEditTitle->setEnabled(true);
	}

	doUpdateOkState();
}

void PLSLiveInfoNaverTV::doUpdateOkState()
{
	ui->okButton->setEnabled(true);
}

void PLSLiveInfoNaverTV::titleEdited()
{
	static const int TitleLengthLimit = 75;
	QString newText = ui->lineEditTitle->text();

	bool isTooLong = false;
	if (newText.length() > TitleLengthLimit) {
		newText = newText.left(TitleLengthLimit);
		QSignalBlocker signalBlocker(ui->lineEditTitle);
		ui->lineEditTitle->setText(newText);
		isTooLong = true;
	}

	doUpdateOkState();

	if (isTooLong) {
		PLSAlertView::warning(this, tr("Alert.Title"), QTStr("LiveInfo.Title.Length.Check.arg").arg(TitleLengthLimit).arg(platform->getChannelName()));
	}
}

void PLSLiveInfoNaverTV::onImageSelected(const QString &)
{
	doUpdateOkState();
}

void PLSLiveInfoNaverTV::onApiRequestFailed(bool tokenExpired)
{
	if (tokenExpired) {
		showLoading(content());
	}
}

const PLSPlatformNaverTV::LiveInfo *PLSLiveInfoNaverTV::getScheLiveInfo(int scheLiveId) const
{
	for (auto &scheLive : scheLiveList) {
		if (scheLive.oliveId == scheLiveId) {
			return &scheLive;
		}
	}
	return nullptr;
}

void PLSLiveInfoNaverTV::showEvent(QShowEvent *event)
{
	PLSLiveInfoBase::showEvent(event);

	if (refreshingScheLiveInfo) {
		showLoading(content());
	}
}

void PLSLiveInfoNaverTV::processGetScheLives(bool ok, int code, const QList<PLSPlatformNaverTV::LiveInfo> &scheLiveInfos, PLSPlatformNaverTV::LiveInfo *liveInfo)
{
	refreshingScheLiveInfo = false;

	if (ok) {
		scheLiveList = scheLiveInfos;

		auto iter = std::find_if(scheLiveInfos.begin(), scheLiveInfos.end(), [=](const PLSPlatformNaverTV::LiveInfo &i) { return i.oliveId == liveInfo->oliveId; });
		if (iter != scheLiveInfos.end()) {
			*liveInfo = *iter;
			ui->scheCombox->setupButton(liveInfo->title, PLSDateFormate::timeStampToShortString((long)(liveInfo->startDate / 1000)));
			ui->thumbnailButton->setImagePath(liveInfo->thumbnailImagePath);
			ui->lineEditTitle->setText(liveInfo->title);
			hideLoading();
		} else {
			pls_alert_error_message(this, tr("Alert.Title"), tr("broadcast.invalid.schedule"));
			hideLoading();

			selectedId = -1;

			ui->nvtGuidelabel->setText(QString());
			ui->scheCombox->setupButton(tr("New"), PLSDateFormate::timeStampToShortString(0));
			ui->thumbnailButton->setImagePath(tmpNewLiveInfoThumbnailPath);
			ui->lineEditTitle->setText(tmpNewLiveInfoTitle.isEmpty() ? platform->getImmediateLiveInfo()->title : tmpNewLiveInfoTitle);

			ui->thumbnailButton->setButtonEnabled(true);
			ui->scheCombox->setButtonEnable(true);
			ui->lineEditTitle->setEnabled(true);
		}
	} else {
		hideLoading();
		reject();
	}
}

void PLSLiveInfoNaverTV::processRefreshScheLives(bool ok, const QList<PLSPlatformNaverTV::LiveInfo> &scheliveInfos)
{
	if (ok) {
		scheLiveList = scheliveInfos;
	}

	if (ui->scheCombox == nullptr || ui->scheCombox->isMenuNULL() || ui->scheCombox->getMenuHide()) {
		return;
	}

	scheLiveItems.clear();

	for (const auto &scheliveInfo : scheliveInfos) {
		PLSScheComboxItemData scheData;
		scheData.title = scheliveInfo.title;
		scheData._id = QString::number(scheliveInfo.oliveId);
		scheData.type = PLSScheComboxItemType::Ty_Schedule;
		scheData.time = PLSDateFormate::timeStampToUTCString(long(scheliveInfo.startDate / 1000));
		if (scheliveInfo.oliveId != selectedId) {
			scheLiveItems.push_back(scheData);
		}
	}

	if (selectedId > 0) {
		PLSScheComboxItemData normalData;
		normalData._id = "-1";
		normalData.title = tr("New");
		normalData.time = "";
		normalData.type = PLSScheComboxItemType::Ty_NormalLive;
		scheLiveItems.insert(scheLiveItems.begin(), normalData);
	}

	if (scheLiveItems.empty()) {
		PLSScheComboxItemData normalData;
		normalData._id = QString::number(platform->getImmediateLiveInfo()->oliveId);
		normalData.title = tr("New");
		normalData.time = tr("LiveInfo.Youtube.no.scheduled");
		normalData.type = PLSScheComboxItemType::Ty_Placeholder;
		scheLiveItems.insert(scheLiveItems.begin(), normalData);
	}

	ui->scheCombox->showScheduleMenu(scheLiveItems);
}
