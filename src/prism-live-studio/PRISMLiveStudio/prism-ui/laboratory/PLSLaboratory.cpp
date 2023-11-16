#include "PLSLaboratory.h"
#include "ui_PLSLaboratory.h"
#include "PLSLaboratoryManage.h"
#include "PLSBasic.h"
#include "PLSAlertView.h"
#include "qt-wrappers.hpp"
#include "PLSLaboratoryInstallView.h"
#include "pls-common-define.hpp"
#include <QTextBlock>
#include "PLSLaboratoryItem.h"
#include "PLSMessageBox.h"
#include "liblog.h"
#include "libutils-api.h"
#include "pls/pls-obs-api.h"

PLSLaboratory::PLSLaboratory(QWidget *parent) : PLSDialogView(parent)
{
	LAB_LOG("laboratory view init");
	ui = pls_new<Ui::PLSLaboratory>();
	pls_add_css(this, {"PLSLaboratory"});
	setupUi(ui);
#if defined(Q_OS_WIN)
	setFixedSize(820, 600);
#elif defined(Q_OS_MACOS)
	setFixedSize(820, 560);
#endif
	setWindowTitle(tr("Basic.MainMenu.File.laboratory"));
	setupListWidgetItems();
	m_browserWidget = pls::browser::newBrowserWidget(pls::browser::Params()
								 .url("about:blank")
								 .initBkgColor(QColor(39, 39, 39))
								 .css("html, body{ background-color: #272727; }")
								 .showAtLoadEnded(true)
								 .loadEnded([this](const pls::browser::Browser *browser) {
									 if (pls_object_is_valid(this)) {
										 QJsonObject jsonObject;
										 setupSendJsonObject(jsonObject);
										 browser->send("prismLab", jsonObject);
									 }
								 })
								 .msgReceived([this](const pls::browser::Browser *, const QString &, const QJsonObject &msg) {
									 if (pls_object_is_valid(this)) {
										 receiveWebViewJsMessage(msg);
									 }
								 }));
	connect(&m_buttonGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this, &PLSLaboratory::labItemChanged);
	ui->defaultDescLabel->setText(tr("laboratory.default.desc"));
	QTextCursor textCursor = ui->defaultDescLabel->textCursor();
	QTextBlockFormat textBlockFormat;
	textBlockFormat.setBottomMargin(10);
	textCursor.setBlockFormat(textBlockFormat);
	ui->defaultDescLabel->setTextCursor(textCursor);
	setAttribute(Qt::WA_DeleteOnClose, true);
	setResizeEnabled(false);
	ui->scrollArea->verticalScrollBar()->isVisible() ? ui->verticalLayout_6->setContentsMargins(12, 12, 0, 10) : ui->verticalLayout_6->setContentsMargins(12, 12, 12, 10);
	LabManage->requestLabJsonData();

	auto closeEvent = [this](const QCloseEvent *) {
		hide();
		m_browserWidget->closeBrowser();
		return true;
	};
	setCloseEventCallback(closeEvent);
}

PLSLaboratory::~PLSLaboratory()
{
	LAB_LOG("laboratory view release");
	pls_delete(ui);
}

void PLSLaboratory::changeCheckedState(const QString &itemId, bool used)
{
	auto checkButton = static_cast<PLSLaboratoryItem *>(m_buttonGroup.checkedButton());
	QString curLabId = checkButton->itemId();
	if (itemId == curLabId) {
		LAB_LOG(QString("change current lab open button check state, lab id is %1, lab title is %2 , check state is %3 ")
				.arg(itemId)
				.arg(checkButton->itemName())
				.arg(used ? "checked" : "unchecked"));
		pls_flush_style(ui->openButton, common::STATUS_SELECTED, used ? common::STATUS_TRUE : common::STATUS_FALSE);
		pls_flush_style(ui->openLabel, common::STATUS_SELECTED, used ? common::STATUS_TRUE : common::STATUS_FALSE);
		ui->openLabel->setText(used ? tr("laboratory.item.open.using") : tr("laboratory.item.open.unused"));
		if (m_browserWidget) {
			QJsonObject jsonObject = getDetailPageButtonEnabledJsEvent(LabManage->getStringInfo(curLabId, laboratory_data::g_laboratoryDetailPage));
			LAB_LOG(QString("send detail page button js event is %1").arg(QJsonDocument(jsonObject).toJson().constData()));
			m_browserWidget->send("prismLab", jsonObject);
		}

		LabManage->saveLaboratoryUseState(curLabId, used);
	}
}

bool PLSLaboratory::getCheckedState(const QString &labId) const
{
	return LabManage->getLaboratoryUseState(labId);
}

void PLSLaboratory::showEvent(QShowEvent *event)
{
	refreshCurrentItemSelected();
	PLSDialogView::showEvent(event);
}

void PLSLaboratory::setupListWidgetItems()
{
	auto manage = PLSLaboratoryManage::instance();
	for (auto labId : manage->getLabIdList()) {
		QString name = manage->getStringInfo(labId, laboratory_data::g_laboratoryTitle);
		m_buttonGroup.addButton(new PLSLaboratoryItem(labId, name));
	}
	for (auto button : m_buttonGroup.buttons()) {
		auto item = static_cast<PLSLaboratoryItem *>(button);
		ui->verticalLayout_6->addWidget(item);
		item->setScrollArea(ui->scrollArea);
	}
	ui->verticalLayout_6->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::MinimumExpanding));
}

void PLSLaboratory::refreshCurrentItemSelected()
{
	auto checkButton = static_cast<PLSLaboratoryItem *>(m_buttonGroup.checkedButton());
	int stackWidgetPage = checkButton == nullptr ? 0 : 1;
	LAB_LOG(QString("laboratory view show page index is %1").arg(stackWidgetPage));
	ui->stackedWidget->setCurrentIndex(stackWidgetPage);
	if (ui->stackedWidget->currentIndex() == 0) {
		LAB_LOG("laboratory show default page no selected item");
		ui->labTitleLabel->setText(tr("laboratory.default.title"));
		ui->openLabel->setHidden(true);
		ui->openButton->setHidden(true);
	} else {
		if (!checkButton) {
			return;
		}
		QString labId = checkButton->itemId();
		QString labTitle = PLSLaboratoryManage::instance()->getStringInfo(labId, laboratory_data::g_laboratoryTitle);
		ui->labTitleLabel->setText(labTitle);
		bool isLowPrismVersion = LabManage->isPrismLowVersion(labId);
		bool checked = LabManage->getLaboratoryUseState(labId);
		if (isLowPrismVersion) {
			checked = false;
		}
		if (LabManage->isDllType(labId)) {
			LAB_LOG(QString("currently showing plugin type objects, lab id is %1 , lab name is %2 , checked is %3").arg(labId).arg(labTitle).arg(checked ? "checked" : "unchecked"));
		} else {
			LAB_LOG(QString("currently showing other type object, lab id is %1 , lab name is %2 , checked is %3").arg(labId).arg(labTitle).arg(checked ? "checked" : "unchecked"));
		}
		ui->openLabel->setHidden(false);
		ui->openButton->setHidden(false);
		ui->openButton->setDisabled(isLowPrismVersion);
		changeCheckedState(labId, checked);
		setDetailInfo(labId);
	}
}

void PLSLaboratory::setDetailInfo(const QString &labId)
{
	QUrl fileUrl;
	QString filePath = PLSLaboratoryManage::instance()->getLabDetailPageFilePathByLabId(labId);
	QFileInfo fileInfo(filePath);
	if (fileInfo.exists()) {
		LAB_LOG("currently detail page file path is existed");
		fileUrl = QUrl::fromLocalFile(filePath);
		LAB_KR_LOG(QString("currently detail page lab id is %1 , file path is ").arg(labId) + filePath);
	} else {
		LAB_LOG("currently detail page file path is not existed");
		filePath = PLSLaboratoryManage::instance()->getAppDetailPageFilePathByLabId(labId);
		fileUrl = QUrl::fromLocalFile(filePath);
		LAB_KR_LOG(QString("currently detail app page lab id is %1 , file path is ").arg(labId) + filePath);
	}
	QString url = QString("%1?lang=%2").arg(fileUrl.toString()).arg(pls_get_current_language_short_str());
	m_browserWidget->url(url);

	//refresh cefwidget url
	ui->stackedWidget->addWidget(m_browserWidget);
	ui->stackedWidget->setCurrentWidget(m_browserWidget);
}

void PLSLaboratory::openLab(const QString &labId)
{
	//Determine whether the current laboratory function needs to be installed
	if (!LabManage->getBoolInfo(labId, laboratory_data::g_laboratoryNeedInstall)) {
		LAB_LOG(QString("user click is not need install, lab id is %1").arg(labId));
		pls_laboratory_click_open_button(labId, true);
		return;
	}

	//If the lab needs to be installed and the download is successful
	if (LabManage->isDownloadSuccess(labId)) {
		LAB_LOG(QString("The current lab is downloaded successfully, no installation is required, lab id is %1").arg(labId));
		checkInstallFinished(labId, false);
		return;
	}

	//If the download is not successful, the installation page will pop up
	LAB_LOG("show user install view");
	PLSLaboratoryInstallView installView(labId, this);
	int code = installView.exec();
	if (code == g_installSuccess) {
		if (!LabManage->isDllType(labId)) {
			LAB_LOG("install non-plugin type lab success");
			checkInstallFinished(labId, true);
			return;
		}
		LAB_LOG(QString("start loading the plugins in the plugin directory, lab id is %1").arg(labId));
		if (!pls_load_plugin(LabManage->getAppDllFilePathWithLabId(labId).toUtf8().constData(), LabManage->getAppDllDataDirPathByLabId(labId).toUtf8().constData())) {
			LAB_LOG(QString("loading the plugins in the plugin directory failed, lab id is %1").arg(labId));
			LAB_LOG("install plugin type lab failed");
			pls_alert_error_message(nullptr, tr("Alert.Title"), tr("laboratory.item.open.other.reason.failed.text"));
			return;
		}
		LAB_LOG(QString("loading the plugins in the plugin directory success, lab id is %1").arg(labId));
		LAB_LOG("install plugin type lab success");
		checkInstallFinished(labId, true);
	} else if (code == g_installNetworkError) {
		LAB_LOG("install lab network error");
		PLSAlertView::warning(nullptr, tr("Alert.Title"), tr("laboratory.item.open.network.failed.text"));
	} else if (code == g_installUnkownError) {
		LAB_LOG("install lab unkown error");
		pls_alert_error_message(nullptr, tr("Alert.Title"), tr("laboratory.item.open.other.reason.failed.text"));
	}
}

void PLSLaboratory::closeLab(const QString &labId)
{
	//Determine whether the current laboratory function needs to be installed
	if (!LabManage->getBoolInfo(labId, laboratory_data::g_laboratoryNeedInstall)) {
		LAB_LOG(QString("user close lab is not need install, lab id is %1 ").arg(labId));
		pls_laboratory_click_open_button(labId, false);
		return;
	}

	//First, determine whether a restart is required to open the current function
	if (!LabManage->getBoolInfo(labId, laboratory_data::g_laboratoryCloseRestart)) {
		LAB_LOG(QString("user close lab is not restart, lab id is %1 ").arg(labId));
		changeCheckedState(labId, false);
		pls_laboratory_click_open_button(labId, false);
		return;
	}

	LAB_LOG("show close lab restart alert view");
	PLSAlertView::Button button = PLSMessageBox::question(this, QTStr("Confirm"), QTStr("laboratory.item.close.restart.text"), PLSAlertView::Button::Yes | PLSAlertView::Button::No);
	if (button == PLSAlertView::Button::Yes) {
		LAB_LOG(QString("user close lab click restart button, lab id is %1 ").arg(labId));
		auto basic = PLSBasic::instance();
		basic->restartPrismApp();
		if (pls_is_main_window_closing()) {
			LAB_LOG(QString("user close lab button clicked restart now, current app is closing, lab id is %1").arg(labId));
			LabManage->saveLaboratoryUseState(labId, false);
		}
		return;
	}
	LAB_LOG(QString("user close lab click not restart button, lab id is %1 ").arg(labId));
}

void PLSLaboratory::setupSendJsonObject(QJsonObject &jsonObject) const
{
	QJsonObject dataJsonObject;
	if (isOpenButtonChecked()) {
		dataJsonObject.insert("action", "enableButton");
	} else {
		dataJsonObject.insert("action", "disableButton");
	}
	jsonObject.insert("data", dataJsonObject);
}

void PLSLaboratory::receiveWebViewJsMessage(const QJsonObject &msg) const
{
	QString page = msg.value(common::LABORATORY_JS_PAGE).toString();
	QString action = msg.value(common::LABORATORY_JS_ACTION).toString();
	QString info = msg.value(common::LABORATORY_JS_INFO).toString();
	auto checkButton = static_cast<PLSLaboratoryItem *>(m_buttonGroup.checkedButton());
	QString labId = checkButton->itemId();
	LAB_LOG(QString("receive detail page js event , lab id is %1 , page is %2 , action is %3 , info is %4 ").arg(labId).arg(page).arg(action).arg(info));
	pls_laboratory_detail_page_js_event(page, action, info);
}

bool PLSLaboratory::isOpenButtonChecked() const
{
	return ui->openButton->property(common::STATUS_SELECTED).toBool();
}

void PLSLaboratory::checkInstallFinished(const QString &labId, bool installPageEnter)
{
	LAB_LOG(QString("check if a restart is required to open the lab, lab id is %1").arg(labId));
	if (!LabManage->getBoolInfo(labId, laboratory_data::g_laboratoryOpenRestart)) {
		LAB_LOG(QString("The current lab does not need to restart, lab id is %1").arg(labId));
		changeCheckedState(labId, true);
		pls_laboratory_click_open_button(labId, true);
		return;
	}
	checkInstallFinishedRestart(labId, installPageEnter);
}

bool PLSLaboratory::checkInstallFinishedRestart(const QString &labId, bool installPageEnter)
{
	LAB_LOG(QString("The current lab show restart alert view , lab id is %1 , installPageEnter is %2").arg(labId).arg(installPageEnter));
	QString content = QTStr("laboratory.item.open.restart.text");
	if (installPageEnter) {
		content = QTStr("laboratory.item.install.finished.restartapp.content");
	}
	PLSAlertView::Button button = PLSMessageBox::question(this, QTStr("Confirm"), content, PLSAlertView::Button::Yes | PLSAlertView::Button::No);
	if (button == PLSAlertView::Button::Yes) {
		LAB_LOG(QString("user clicked restart now, lab id is %1").arg(labId));
		auto basic = PLSBasic::instance();
		basic->restartPrismApp();
		if (pls_is_main_window_closing()) {
			LAB_LOG(QString("user open lab button clicked restart now, current app is closing, lab id is %1").arg(labId));
			LabManage->saveLaboratoryUseState(labId, true);
		}
		return true;
	}
	LAB_LOG(QString("user clicked not restart button, lab id is %1").arg(labId));
	return false;
}

QJsonObject PLSLaboratory::getDetailPageButtonEnabledJsEvent(const QString &) const
{
	QJsonObject jsonObject;
	setupSendJsonObject(jsonObject);
	return jsonObject;
}

void PLSLaboratory::on_openButton_clicked()
{
	auto checkButton = static_cast<PLSLaboratoryItem *>(m_buttonGroup.checkedButton());
	QString labId = checkButton->itemId();
	if (isOpenButtonChecked()) {
		LAB_LOG(QString("user click open checkbox button state is unchecked, lab id is %1 ").arg(labId));
		closeLab(labId);
	} else {
		LAB_LOG(QString("user click open checkbox button state is checked , lab id is %1 ").arg(labId));
		PLS_LOGEX(PLS_LOG_INFO, MODULE_ABORATORY, {{"labPluginId", checkButton->itemName().toUtf8().constData()}}, "user click open checkbox button state is checked");
		openLab(labId);
	}
}

void PLSLaboratory::labItemChanged(QAbstractButton *button)
{
	auto item = static_cast<PLSLaboratoryItem *>(button);
	item->getScrollArea()->ensureWidgetVisible(button);
	LAB_LOG(QString("user click lab title is %1").arg(item->itemName()));
	refreshCurrentItemSelected();
}

void PLSLaboratory::on_closeButton_clicked()
{
	this->close();
}
