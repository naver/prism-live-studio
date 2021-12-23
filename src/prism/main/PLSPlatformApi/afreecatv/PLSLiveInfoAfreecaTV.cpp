#include "PLSLiveInfoAfreecaTV.h"
#include "../PLSPlatformApi.h"
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataAPI.h"
#include "PLSLiveInfoBase.h"
#include "alert-view.hpp"
#include "log/log.h"
#include "ui_PLSLiveInfoAfreecaTV.h"

static const char *liveInfoMoudule = "PLSLiveInfoAfreecaTV";

PLSLiveInfoAfreecaTV::PLSLiveInfoAfreecaTV(PLSPlatformBase *pPlatformBase, QWidget *parent, PLSDpiHelper dpiHelper)
	: PLSLiveInfoBase(pPlatformBase, parent, dpiHelper), ui(new Ui::PLSLiveInfoAfreecaTV)
{
	PLS_INFO(liveInfoMoudule, "AfreecaTV liveinfo Will show");
	ui->setupUi(this->content());
	ui->horizontalLayout->addWidget(createResolutionButtonsFrame());
	dpiHelper.setCss(this, {PLSCssIndex::PLSLiveinfoAfreecaTV});

	setHasCloseButton(false);
	setHasBorder(true);
	this->setWindowTitle(tr("LiveInfo.liveinformation"));

	content()->setFocusPolicy(Qt::StrongFocus);

	PLS_PLATFORM_AFREECATV->setAlertParent(this);

	setupFirstUI();

	updateStepTitle(ui->okButton);

	if (!PLS_PLATFORM_API->isPrepareLive()) {
		ui->bottomButtonWidget->layout()->addWidget(ui->cancelButton);
	}
	ui->lineEditCategory->setEnabled(false);

	doUpdateOkState();
	connect(
		PLS_PLATFORM_AFREECATV, &PLSPlatformAfreecaTV::closeDialogByExpired, this, [=]() { reject(); }, Qt::DirectConnection);
}

PLSLiveInfoAfreecaTV::~PLSLiveInfoAfreecaTV()
{
	delete ui;
}

void PLSLiveInfoAfreecaTV::refreshUI()
{
	const auto &data = PLS_PLATFORM_AFREECATV->getSelectData();
	ui->lineEditTitle->setText(data.frmTitle);
	ui->lineEditCategory->setText(data.frmCategoryStr);
}

void PLSLiveInfoAfreecaTV::showEvent(QShowEvent *event)
{
	Q_UNUSED(event)
	showLoading(content());

	auto _onNext = [=](bool value) {
		hideLoading();

		refreshUI();
		if (!value && !PLS_PLATFORM_API->isPrepareLive()) {
			reject();
		}
	};

	PLS_PLATFORM_AFREECATV->requestDashborad(_onNext, this);
	__super::showEvent(event);
}

void PLSLiveInfoAfreecaTV::setupFirstUI()
{
	ui->scheduleLabel->setText(QString(LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("LiveInfo.base.Title")));

	connect(ui->lineEditTitle, &QLineEdit::textChanged, this, &PLSLiveInfoAfreecaTV::titleEdited, Qt::QueuedConnection);

	connect(ui->okButton, &QPushButton::clicked, this, &PLSLiveInfoAfreecaTV::okButtonClicked);
	connect(ui->cancelButton, &QPushButton::clicked, this, &PLSLiveInfoAfreecaTV::cancelButtonClicked);
}

void PLSLiveInfoAfreecaTV::doUpdateOkState()
{
	if (ui->lineEditTitle->text().trimmed().isEmpty()) {
		ui->okButton->setEnabled(false);
		return;
	}
	ui->okButton->setEnabled(true);
}

void PLSLiveInfoAfreecaTV::titleEdited()
{
	static const int TitleLengthLimit = 75;
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
	doUpdateOkState();

	if (isLargeToMax) {
		const auto channelName = PLS_PLATFORM_AFREECATV->getInitData().value(ChannelData::g_platformName).toString();
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("LiveInfo.Title.Length.Check.arg").arg(TitleLengthLimit).arg(channelName));
	}
}

void PLSLiveInfoAfreecaTV::okButtonClicked()
{
	PLS_UI_STEP(liveInfoMoudule, "afreecatv liveinfo OK Button Click", ACTION_CLICK);
	saveDateWhenClickButton();
}

void PLSLiveInfoAfreecaTV::cancelButtonClicked()
{
	PLS_UI_STEP(liveInfoMoudule, "afreecatv liveinfo Cancel Button Click", ACTION_CLICK);
	reject();
}

void PLSLiveInfoAfreecaTV::saveDateWhenClickButton()
{
	auto _onNext = [=](bool isSucceed) {
		hideLoading();
		PLS_INFO(liveInfoMoudule, "afreecatv liveinfo Save %s", (isSucceed ? "succeed" : "failed"));
		if (isSucceed) {
			accept();
		}
	};

	showLoading(content());
	PLS_PLATFORM_AFREECATV->saveSettings(_onNext, ui->lineEditTitle->text());
}
