#include "PLSNCB2bBrowserSettings.h"
#include "obs-app.hpp"
#include "login-user-info.hpp"
#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"
#include "liblog.h"
#include "PLSLoginDataHandler.h"
#include "item-widget-helpers.hpp"
#include "ui_PLSNCB2BBrowserSettings.h"

static const char *ncb2bBrowserSettingsModuleName = "PLSNCB2bBrowserSettings";

PLSNCB2bBrowserSettings::PLSNCB2bBrowserSettings(DialogInfo info, QWidget *parent) : PLSSideBarDialogView(info, parent)
{
	ui = pls_new<Ui::PLSNCB2bBrowserSettings>();

#if defined(Q_OS_WIN)
	setFixedSize(848, 570);
#elif defined(Q_OS_MACOS)
	setFixedSize(848, 570 - PLS_TITLE_BAR_HEIGHT);
#endif

	setupUi(ui);
	setResizeEnabled(false);
	auto serviceName = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName();
	setWindowTitle(QTStr("Ncpb2b.Browser.Settings.Title").arg(serviceName));
	ui->descriptionLabel->setText(QTStr("Ncpb2b.Browser.Settings.Description").arg(serviceName));
	ui->selectLabel->setVisible(false);
	ui->stackedWidget->setCurrentWidget(ui->noContentPage);
	ui->refreshBtn->setToolTip(QTStr("Ncpb2b.Browser.Settings.Refresh.Tooltip"));
	ui->listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	ui->listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	pls_add_css(this, {"PLSNCB2bBrowserSettings"});
	ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(QTStr("Close"));
	ui->buttonBox->button(QDialogButtonBox::Ok)->setText(QTStr("Apply"));
	connect(ui->refreshBtn, &QPushButton::clicked, this, &PLSNCB2bBrowserSettings::onRefreshButtonClicked);
	connect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, [this]() { close(); });
	connect(ui->buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, this, &PLSNCB2bBrowserSettings::onOkButtonClicked);
	connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, [this](int index) {
		updateLogo();
		updateSelected();
	});
	updateLogo();
}

PLSNCB2bBrowserSettings::~PLSNCB2bBrowserSettings()
{
	pls_delete(ui, nullptr);
}

void PLSNCB2bBrowserSettings::refreshUI()
{
	m_needRefreshDock = true;
	onRefreshButtonClicked();
}

PLSErrorHandler::ErrCode PLSNCB2bBrowserSettings::getErrorCode()
{
	return m_errorCode;
}

void PLSNCB2bBrowserSettings::closeEvent(QCloseEvent *event)
{
	hide();
	event->ignore();
}

void PLSNCB2bBrowserSettings::showEvent(QShowEvent *event)
{
	PLSSideBarDialogView::showEvent(event);
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::Ncb2bBrowserSettings, true);
}

void PLSNCB2bBrowserSettings::hideEvent(QHideEvent *event)
{
	PLSSideBarDialogView::hideEvent(event);
	App()->getMainView()->updateSideBarButtonStyle(ConfigId::Ncb2bBrowserSettings, false);
}

void PLSNCB2bBrowserSettings::onOkButtonClicked()
{
	PLSNCB2bBroSettingsManager::instance()->setSelected(cacheSelectedDatas);
	cacheSelectedDatas.clear();

	updateDocks();
}

void PLSNCB2bBrowserSettings::createItems(const QList<PLSNCB2bBrowserSettingData> &datas)
{
	for (auto data : datas) {
		createItem(data);
	}
}

void PLSNCB2bBrowserSettings::removeAll()
{
	ClearListItems(ui->listWidget);
}

void PLSNCB2bBrowserSettings::updateDocks()
{
	auto basic = OBSBasic::Get();
	if (!basic) {
		return;
	}

	if (auto docks = basic->getNcb2bDock(); docks) {
		docks->refreshUI(m_errorCode);
	}
}

void PLSNCB2bBrowserSettings::checkNeedUpdateDocks()
{
	if (m_needRefreshDock) {
		updateDocks();
		m_needRefreshDock = false;
	}
}

void PLSNCB2bBrowserSettings::updateSelected()
{
	bool listPage = ui->stackedWidget->currentWidget() == ui->listPage;

	ui->selectLabel->setVisible(seletedNumbers > 0 && listPage);
	ui->selectLabel->setText(QTStr("Ncpb2b.Browser.Settings.Selected").arg(seletedNumbers));
}

void PLSNCB2bBrowserSettings::onRefreshButtonClicked()
{
	if (!pls_get_network_state()) {
		m_errorCode = PLSErrorHandler::COMMON_NETWORK_ERROR;
		ui->noNetworkLabel->setText(QTStr("Ncpb2b.Browser.Settings.No.Network.Desc"));
		ui->stackedWidget->setCurrentWidget(ui->noNetworkPage);
		checkNeedUpdateDocks();
		return;
	}
	if (requestExisted) {
		PLS_INFO(ncb2bBrowserSettingsModuleName, "The refresh browser settings urls request already exists, avoid duplicate request.");
		return;
	}

	auto okCallback = [this](const QJsonObject &data) {
		pls_check_app_exiting();
		m_errorCode = PLSErrorHandler::SUCCESS;
		QJsonObject supportUrl = data.value("serviceSupportUrlPc").toObject();
		if (supportUrl.isEmpty()) {
			PLS_INFO(ncb2bBrowserSettingsModuleName, "There was no serviceSupportUrlPc field value was retrieved from the api.");
			updateUI({});
		} else {
			QList<PLSNCB2bBrowserSettingData> datas = PLSNCB2bBroSettingsManager::instance()->parseSupportUrls(supportUrl);
			updateUI(datas);
		}
		checkNeedUpdateDocks();
		requestExisted = false;
	};

	auto failCallback = [this](const QJsonObject &, const PLSErrorHandler::RetData &retData) {
		pls_check_app_exiting();
		PLS_INFO(ncb2bBrowserSettingsModuleName, "There was some errors was retrieved from the api.");
		if (retData.prismCode == PLSErrorHandler::CHANNEL_NCP_B2B_1101_SERVICE_DISABLED) {
			m_errorCode = PLSErrorHandler::CHANNEL_NCP_B2B_1101_SERVICE_DISABLED;
			ui->noNetworkLabel->setText(QTStr("Ncb2b.Service.Disable.Status"));
		} else {
			m_errorCode = PLSErrorHandler::COMMON_NETWORK_ERROR;
			ui->noNetworkLabel->setText(QTStr("Ncpb2b.Browser.Settings.No.Network.Desc"));
		}

		ui->stackedWidget->setCurrentWidget(ui->noNetworkPage);
		checkNeedUpdateDocks();
		requestExisted = false;
	};
	requestExisted = true;
	PLSLoginDataHandler::instance()->getNCB2BServiceResFromRemote(okCallback, failCallback, this);
}

void PLSNCB2bBrowserSettings::createItem(const PLSNCB2bBrowserSettingData &data)
{
	PLSNCB2bBroSettingsItem *setting = pls_new<PLSNCB2bBroSettingsItem>(data);
	connect(setting, &PLSNCB2bBroSettingsItem::itemSelected, this, [this, setting](bool selected) {
		auto selectedData = setting->getData();
		auto finder = [selectedData](const PLSNCB2bBrowserSettingData &data_) { return data_.title == selectedData.title && data_.selected == selectedData.selected; };
		if (selected) {
			seletedNumbers += 1;
			selectedData.selected = true;
		} else {
			seletedNumbers -= 1;
			selectedData.selected = false;
		}
		auto iter = std::find_if(cacheSelectedDatas.begin(), cacheSelectedDatas.end(), finder);
		if (iter == cacheSelectedDatas.end()) {
			cacheSelectedDatas.push_back(selectedData);
		} else {
			auto &d = *iter;
			d.selected = selected;
		}
		updateSelected();
	});
	QListWidgetItem *item = pls_new<QListWidgetItem>();
	ui->listWidget->addItem(item);
	ui->listWidget->setItemWidget(item, setting);
}

void PLSNCB2bBrowserSettings::updateUI(const QList<PLSNCB2bBrowserSettingData> &datas)
{
	auto oldDatas = PLSNCB2bBroSettingsManager::instance()->getDatas();
	PLSNCB2bBroSettingsManager::instance()->initDatas(datas);
	PLSNCB2bBroSettingsManager::instance()->setSelected(oldDatas);

	removeAll();
	createItems(PLSNCB2bBroSettingsManager::instance()->getDatas());

	cacheSelectedDatas = PLSNCB2bBroSettingsManager::instance()->getDatas(true);
	seletedNumbers = cacheSelectedDatas.count();

	updateSelected();
	updateLogo();
	if (PLSNCB2bBroSettingsManager::instance()->getDatas().isEmpty()) {
		ui->stackedWidget->setCurrentWidget(ui->noContentPage);
	} else {
		ui->stackedWidget->setCurrentWidget(ui->listPage);
	}
}

void PLSNCB2bBrowserSettings::updateLogo()
{
	QString outroPath = PLSLoginDataHandler::instance()->getNCB2BServiceOutro();
	if (outroPath.isEmpty() || !QFile::exists(outroPath)) {
		ui->logoWidget->setVisible(false);
	} else {
		QImage original;
		original.load(outroPath);
		QSize originalSize = original.size();
		int targetWidth = (double)originalSize.width() * 22 / (double)originalSize.height();
		QImage image = original.scaled(targetWidth * 4, 22 * 4, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		ui->logo->setFixedWidth(targetWidth);
		ui->logo->setPixmap(QPixmap::fromImage(image));
		ui->logoWidget->setVisible(true);
	}
}
