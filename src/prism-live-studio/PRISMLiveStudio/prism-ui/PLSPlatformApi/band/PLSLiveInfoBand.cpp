#include "PLSLiveInfoBand.h"
#include "ui_PLSLiveInfoBand.h"
#include "PLSChannelDataAPI.h"
#include "ChannelCommonFunctions.h"
#include "PLSPlatformApi.h"
#include "QTextFrame"
#include <QTextDocument>
#include <QTextFrameFormat>
#include <qdebug.h>
#include "login-common-helper.hpp"
#include "PLSPlatformBand.h"
#include "../channels/ChannelsDataApi/PLSChannelDataAPI.h"
#include "utils-api.h"
#include "libui.h"
constexpr auto MAXINPUTCONTENT = 500;
static const char *const liveInfoBandModule = "PLSLiveInfoBand";

PLSLiveInfoBand::PLSLiveInfoBand(PLSPlatformBase *pPlatformBase, QWidget *parent) : PLSLiveInfoBase(pPlatformBase, parent), platform(dynamic_cast<PLSPlatformBand *>(pPlatformBase))
{
	ui = pls_new<Ui::PLSLiveInfoBand>();
	pls_add_css(this, {"PLSLiveinfoBand"});
	setupUi(ui);
	ui->TitleFrameLayout->addWidget(createResolutionButtonsFrame());
	platform->setAlertParent(this);
	ui->plainTextEdit->setProperty("readonly", false);
	if (PLSCHANNELS_API->isLiving()) {
		ui->plainTextEdit->setProperty("readonly", true);
		ui->okButton->setDisabled(true);
		ui->plainTextEdit->setDisabled(true);

		ui->describeLabel->setDisabled(true);
	}
	m_uuid = platform->getChannelUUID();
	content()->setFocusPolicy(Qt::StrongFocus);

	ui->dualWidget->setText(tr("BAND"))->setUUID(m_uuid);

	if (auto description = QString::fromUtf8(platform->getTitle().c_str()); !description.isEmpty()) {
		ui->plainTextEdit->setPlainText(description);
	}
	connect(ui->plainTextEdit, &QTextEdit::textChanged, this, &PLSLiveInfoBand::textChangeHandler, Qt::QueuedConnection);
	connect(ui->okButton, &QPushButton::clicked, this, &PLSLiveInfoBand::okButtonClicked);
	connect(ui->cancelButton, &QPushButton::clicked, this, &PLSLiveInfoBand::cancelButtonClicked);

	updateStepTitle(ui->okButton);

#if defined(Q_OS_WIN)
	if (!PLS_PLATFORM_API->isPrepareLive()) {
		ui->horizontalLayout_2->addWidget(ui->cancelButton);
	}
#endif
}

PLSLiveInfoBand::~PLSLiveInfoBand()
{
	pls_delete(ui);
}

QString PLSLiveInfoBand::getDescription() const
{
	return m_description;
}

bool PLSLiveInfoBand::isModify() const
{
	return (m_description == ui->plainTextEdit->toPlainText());
}

void PLSLiveInfoBand::okButtonClicked()
{
	PLS_UI_STEP(liveInfoBandModule, "Band liveinfo OK Button Click", ACTION_CLICK);
	m_description = ui->plainTextEdit->toPlainText();
	platform->setTitle(m_description.toUtf8().data());

	auto checkStreamKey = [this](const PLSErrorHandler::RetData &retData) {
		hideLoading();
		if (retData.prismCode == PLSErrorHandler::SUCCESS) {
			done(Accepted);
		} else if (retData.errorType == PLSErrorHandler::ErrorType::TokenExpired || retData.prismCode == PLSErrorHandler::CHANNEL_BAND_FORBIDDEN_ERROR) {
			PLS_LOGEX(PLS_LOG_ERROR, MODULE_PLATFORM_BAND, {{"channel start error", "band"}}, "band token expired.");
			auto retDataTmp = retData;
			PLSErrorHandler::directShowAlert(retDataTmp, nullptr);

			PLSAlertView::Button btn = retDataTmp.clickedBtn;
			done(Rejected);
			if (btn != PLSAlertView::Button::NoButton) {
				PLSCHANNELS_API->channelExpired(platform->getChannelUUID(), false);
			}
		}
	};

	if (PLS_PLATFORM_API->isPrepareLive()) {
		showLoading(content());
		platform->setDescription(m_description.toUtf8().data());
		platform->requestLiveStreamKey(checkStreamKey);

	} else {
		done(Accepted);
	}
}

void PLSLiveInfoBand::cancelButtonClicked()
{
	PLS_UI_STEP(liveInfoBandModule, "Band liveinfo Cancel Button Click", ACTION_CLICK);
	done(Rejected);
}

void PLSLiveInfoBand::textChangeHandler()
{
	m_isModify = true;
	QString text = ui->plainTextEdit->toPlainText();
	if (text.length() > MAXINPUTCONTENT) {
		QSignalBlocker signalBlocker(ui->plainTextEdit);
		ui->plainTextEdit->setText(text.left(MAXINPUTCONTENT));
		PLSAlertView::warning(this, QTStr("Alert.Title"), QTStr("Live.Check.Band.Description.Max.Limit"));
		ui->plainTextEdit->moveCursor(QTextCursor::End);
	}
}
