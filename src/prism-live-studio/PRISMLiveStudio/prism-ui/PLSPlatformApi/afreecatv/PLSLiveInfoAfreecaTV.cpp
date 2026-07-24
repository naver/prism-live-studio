#include "PLSLiveInfoAfreecaTV.h"
#include "../PLSPlatformApi.h"
#include "ChannelCommonFunctions.h"
#include "PLSChannelDataAPI.h"
#include "PLSLiveInfoBase.h"
#include "frontend-api.h"
#include "log/log.h"
#include "ui_PLSLiveInfoAfreecaTV.h"
#include "libui.h"
using namespace common;
static const char *const liveInfoMoudule = "PLSLiveInfoAfreecaTV";
const int SOOP_TITLE_LENGTH_LIMIT = 75;

PLSLiveInfoAfreecaTV::PLSLiveInfoAfreecaTV(PLSPlatformBase *pPlatformBase, QWidget *parent) : PLSLiveInfoBase(pPlatformBase, parent)
{
	PLS_DISABLE_UISTEP_V2(this);
	ui = pls_new<Ui::PLSLiveInfoAfreecaTV>();
	PLS_INFO(liveInfoMoudule, "AfreecaTV liveinfo Will show");
	pls_add_css(this, {"PLSLiveinfoAfreecaTV"});
	setupUi(ui);
	ui->horizontalLayout->addWidget(createResolutionButtonsFrame());
	setHasCloseButton(false);
	//setHasBorder(true);
	this->setWindowTitle(tr("LiveInfo.liveinformation"));
	ui->dualWidget->setText(tr("Channels.afreeca_tv"))->setUUID(PLS_PLATFORM_AFREECATV->getChannelUUID());

	content()->setFocusPolicy(Qt::StrongFocus);

	PLS_PLATFORM_AFREECATV->setAlertParent(this);

	setupFirstUI();

	updateStepTitle(ui->okButton);

	ui->lineEditCategory->setEnabled(false);

	doUpdateOkState();
	connect(PLS_PLATFORM_AFREECATV, &PLSPlatformAfreecaTV::closeDialogByExpired, this, &PLSLiveInfoAfreecaTV::reject, Qt::DirectConnection);
#if defined(Q_OS_WIN)
	if (!PLS_PLATFORM_API->isPrepareLive()) {
		ui->bottomButtonWidget->layout()->addWidget(ui->cancelButton);
	}
#endif
	pls_uistep_v2_bind(ui->lineEditTitle, ui->scheduleLabel);
	pls_uistep_v2_set_title(this, QStringLiteral("SOOP Channel Live Info"));
}

PLSLiveInfoAfreecaTV::~PLSLiveInfoAfreecaTV()
{
	pls_delete(ui, nullptr);
}

void PLSLiveInfoAfreecaTV::refreshUI()
{
	const auto &data = PLS_PLATFORM_AFREECATV->getSelectData();
	ui->lineEditTitle->setText(data.title);
	ui->lineEditCategory->setText(data.categoryStr);
}

void PLSLiveInfoAfreecaTV::showEvent(QShowEvent *event)
{
	Q_UNUSED(event)
	showLoading(content());

	auto _onNext = [this](bool value) {
		hideLoading();

		refreshUI();
		if (!value && !PLS_PLATFORM_API->isPrepareLive()) {
			reject();
		}
	};

	PLS_PLATFORM_AFREECATV->requestDashborad(_onNext, this, false);
	PLSLiveInfoBase::showEvent(event);
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
	QString newText = ui->lineEditTitle->text();

	bool isLargeToMax = false;
	if (newText.length() > SOOP_TITLE_LENGTH_LIMIT) {
		isLargeToMax = true;
		newText = newText.left(SOOP_TITLE_LENGTH_LIMIT);
	}

	if (newText.compare(ui->lineEditTitle->text()) != 0) {
		QSignalBlocker signalBlocker(ui->lineEditTitle);
		ui->lineEditTitle->setText(newText);
	}
	doUpdateOkState();

	if (isLargeToMax) {
		PLSErrorHandler::ExtraData extraData("SOOP live info title length max");
		extraData.defaultArg = {QString::number(SOOP_TITLE_LENGTH_LIMIT), QString::fromUtf8(SOOP)};
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_LIVEINFO_AFREECA_TV_TITLE_LENGTH_MAX, PLSErrKeyAllAlert, {}, extraData, this);
	}
}

void PLSLiveInfoAfreecaTV::okButtonClicked()
{
	saveDateWhenClickButton();
}

void PLSLiveInfoAfreecaTV::cancelButtonClicked()
{
	reject();
}

void PLSLiveInfoAfreecaTV::saveDateWhenClickButton()
{
	auto _onNext = [this](bool isSucceed) {
		hideLoading();
		PLS_INFO(liveInfoMoudule, "afreecatv liveinfo Save %s", (isSucceed ? "succeed" : "failed"));

		if (PLS_PLATFORM_API->isPrepareLive()) {
			if (isSucceed) {
				PLS_LOGEX(PLS_LOG_INFO, liveInfoMoudule,
					  {
						  {"platformName", AFREECATV},
						  {"startLiveStatus", "Success"},
					  },
					  "afreecaTV start live success");
			} else {
				PLS_LOGEX(PLS_LOG_ERROR, liveInfoMoudule,
					  {{"platformName", AFREECATV}, {"startLiveStatus", "Failed"}, {"startLiveFailed", PLS_PLATFORM_AFREECATV->getFailedErr().toUtf8().constData()}},
					  "afreecaTV start live failed");
			}
		}

		if (isSucceed) {
			accept();
		}
	};
	if (getIsRunLoading()) {
		PLS_INFO(liveInfoMoudule, "afreecatv ignore OK Button Click, because is loading");
		return;
	}

	PLS_PLATFORM_AFREECATV->setFailedErr("");
	showLoading(content());
	PLS_PLATFORM_AFREECATV->saveSettings(_onNext, ui->lineEditTitle->text());
}
