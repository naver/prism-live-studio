#include "PLSLiveInfoTwitch.h"

#include <QObject>
#include <QNetworkReply>
#include <QDebug>
#include <QComboBox>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "../PLSPlatformApi.h"
#include "../PLSLiveInfoDialogs.h"
#include "PLSAlertView.h"
#include "PLSChannelDataAPI.h"
#include "ChannelCommonFunctions.h"
#include "libui.h"
#include "libresource.h"

using namespace common;
PLSLiveInfoTwitch::PLSLiveInfoTwitch(PLSPlatformBase *pPlatformBase, QWidget *parent) : PLSLiveInfoBase(pPlatformBase, parent)
{
	ui = pls_new<Ui::PLSLiveInfoTwitch>();

	pls_add_css(this, {"PLSLiveInfoTwitch"});

	setupUi(ui);
	ui->horizontalLayout_3->addWidget(createResolutionButtonsFrame());
	connect(ui->pushButtonOk, &QPushButton::clicked, this, &PLSLiveInfoTwitch::doOk);
	connect(ui->pushButtonCancel, &QPushButton::clicked, this, &QDialog::reject);
	updateStepTitle(ui->pushButtonOk);
	PLS_PLATFORM_TWITCH->setAlertParent(this);

	ui->dualWidget->setText(tr("LiveInfo.Twitch.Caption"))->setUUID(PLS_PLATFORM_TWITCH->getChannelUUID());

	ui->pushButtonOk->setFocusPolicy(Qt::NoFocus);
	ui->pushButtonCancel->setFocusPolicy(Qt::NoFocus);
	getJs();
	m_updateTimer.setInterval(10000);
	m_updateTimer.setSingleShot(true);
	m_updateTimer.start();
	connect(&m_updateTimer, &QTimer::timeout, [this]() {
		PLS_INFO(MODULE_PLATFORM_TWITCH, "twitch update timeout.");
		if (!m_isUpdateFinished) {
			PLS_INFO(MODULE_PLATFORM_TWITCH, "show twitch update timeout alert.");
			pls_async_call_mt(this, [this]() {
				hideLoading();
				PLSErrorHandler::showAlertByCustomErrName(PLSErrCustomKey_LoadLiveInfoFailed, TWITCH, {});
			});
		}
	});

	auto nickName = PLSCHANNELS_API->getValueOfChannel(pPlatformBase->getChannelUUID(), ChannelData::g_userName, QString());
	m_browserWidget = pls::browser::newBrowserWidget(pls::browser::Params() //
								 .url("https://dashboard.twitch.tv/popout/u/" + nickName + "/stream-manager/edit-stream-info")
								 .initBkgColor(QColor(39, 39, 39))
								 .css("html, body { background-color: #272727; }")
								 .showAtLoadEnded(true)
								 .cookieStoragePath(PLSBasic::cookiePath("Twitch"))
								 .allowPopups(false)
								 .script(m_jsCode)
								 .loadEnded([](pls::browser::Browser *browser) { PLS_INFO(MODULE_PLATFORM_TWITCH, "web twitch live info load end."); }));
	ui->verticalLayout_web->addWidget(m_browserWidget);

	connect(m_browserWidget, &pls::browser::BrowserWidget::msgRecevied, this, [this](const QString &_t1, const QJsonObject &_t2) {
		auto status = _t2.value("data").toString();
		if (0 == status.compare("clickStart", Qt::CaseInsensitive)) {
			PLS_INFO(MODULE_PLATFORM_TWITCH, "clickStart--start update live info");
			m_isUpdateFinished = false;
			m_updateSuccess = false;
			showLoading(content());
			if (!m_updateTimer.isActive()) {
				m_updateTimer.start();
			}
		} else if (0 == status.compare("clickEnd", Qt::CaseInsensitive)) {
			PLS_INFO(MODULE_PLATFORM_TWITCH, "clickEnd--end update live info");
			m_updateTimer.stop();
			m_isUpdateFinished = true;
		} else if (!m_updateSuccess && 0 == status.compare("saveSuccess", Qt::CaseInsensitive)) {
			m_updateSuccess = true;
			PLS_INFO(MODULE_PLATFORM_TWITCH, "saveSuccess--update live info success");
			if (!PLS_PLATFORM_API->isPrepareLive()) {
				accept();
				PLS_PLATFORM_TWITCH->getChannelInfo();
			} else {
				PLS_INFO(MODULE_PLATFORM_TWITCH, "start prepare live.");
				PLS_PLATFORM_TWITCH->requestStreamKey(true, [this](bool isSuccess) {
					if (pls_object_is_valid(this)) {
						if (isSuccess) {
							accept();
							PLS_PLATFORM_TWITCH->getChannelInfo();
						} else {
							PLS_ERROR(MODULE_PLATFORM_TWITCH, "twitch stream key api failed");
							hideLoading();
						}
					}
				});
			}

		} else if (0 == status.compare("isErrorInput", Qt::CaseInsensitive) && m_inputError) {
			PLS_INFO(MODULE_PLATFORM_TWITCH, "isErrorInput--update live info failed");
			hideLoading();
			m_updateTimer.stop();
			m_inputError = false;
			m_isUpdateFinished = true;
		} else if (0 == status.compare("pageShow", Qt::CaseInsensitive)) {
			PLS_INFO(MODULE_PLATFORM_TWITCH, "show twitch live info");
			m_updateTimer.stop();
			m_isUpdateFinished = true;
			pls_async_call_mt(this, [this]() { hideLoading(); });
		}
	});
	showLoading(content());
	auto closeEvent = [this](QCloseEvent *) -> bool {
		if (m_browserWidget) {
			m_browserWidget->closeBrowser();
		}
		return true;
	};
	setCloseEventCallback(closeEvent);

	if (!PLS_PLATFORM_TWITCH->isActive()) {
		ui->labelTooltip->setToolTip(QTStr("Twitch.Service.Off.Tooltip"));
		ui->twitchService->setText(QTStr("Twitch.Default.Service.Output"));

	} else {
		ui->labelTooltip->setToolTip(QTStr("Twitch.Service.On.Tooltip"));
		bool isWHIP = PLSPlatformApi::instance()->isTwitchWHIP();
		auto serviceStr = isWHIP ? QTStr("Twitch.Hhip.Service.Output") : QTStr("Twitch.Rtmps.Service.Output");
		ui->twitchService->setText(serviceStr);
	}

#if defined(Q_OS_WIN)
	if (!PLS_PLATFORM_API->isPrepareLive()) {
		ui->horizontalLayout_4->addWidget(ui->pushButtonCancel);
	}
#endif
}

PLSLiveInfoTwitch::~PLSLiveInfoTwitch()
{
	pls_delete(ui);
}

void PLSLiveInfoTwitch::doOk()
{
	PLS_INFO(MODULE_PLATFORM_TWITCH, "doOk twitch ok button click");
	if (m_browserWidget) {
		PLS_INFO(MODULE_PLATFORM_TWITCH, " start doOk twitch ok button click");
		m_inputError = true;
		m_isUpdateFinished = false;
		m_updateSuccess = false;
		showLoading(content());
		m_updateTimer.stop();
		m_updateTimer.start();
		m_browserWidget->send("clickSave", {});
	} else {
		PLS_INFO(MODULE_PLATFORM_TWITCH, "browserWidget is null");
		reject();
	}
}

void PLSLiveInfoTwitch::showEvent(QShowEvent *event)
{
	Q_UNUSED(event)

	activateWindow();
	PLSLiveInfoBase::showEvent(event);
}
void PLSLiveInfoTwitch::getJs()
{
	QFile file(PLS_RSM_getLibraryPolicyPC_Path(QStringLiteral("Library_Policy_PC/twitch.js")));
	if (!file.exists()) {
		PLS_INFO(MODULE_PLATFORM_TWITCH, "use local js file");
		QString filePath = (":/Configs/resource/DefaultResources/twitch.js");
		file.setFileName(filePath);
	}
	bool isSuccess = file.open(QIODevice::ReadOnly);

	PLS_INFO(MODULE_PLATFORM_TWITCH, "js file open %s", isSuccess ? "success" : "failed");
	if (isSuccess) {
		m_jsCode = file.readAll();
	}
	file.close();
};
