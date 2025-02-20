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
	setFixedSize(848, 542);
#endif

	setupUi(ui);
	setResizeEnabled(false);
	serviceName = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName();
	setWindowTitle(QTStr("Ncpb2b.Browser.Settings.Title").arg(serviceName));
	ui->descriptionLabel->setText(QTStr("Ncpb2b.Browser.Settings.Description").arg(serviceName));
	ui->selectLabel->setVisible(false);
	ui->stackedWidget->setCurrentWidget(ui->noContentPage);
	ui->refreshBtn->setAttribute(Qt::WA_AlwaysShowToolTips);
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
		if (ui->stackedWidget->currentWidget() == ui->listPage) {
			updateChecked();
		}
	});
	updateLogo();
}

PLSNCB2bBrowserSettings::~PLSNCB2bBrowserSettings()
{
	pls_delete(ui, nullptr);
}

void PLSNCB2bBrowserSettings::refreshUI()
{
	onRefreshButtonClicked();
}

void PLSNCB2bBrowserSettings::closeEvent(QCloseEvent *event)
{
	hide();
	event->ignore();
}

void PLSNCB2bBrowserSettings::showEvent(QShowEvent *event)
{
	updateChecked();
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
	auto basic = OBSBasic::Get();
	auto docks = basic->getNcb2bCustomDocks();

	PLSNCB2bBroSettingsManager::instance()->setSelected(cacheSelectedDatas);
	cacheSelectedDatas.clear();

	auto datas = PLSNCB2bBroSettingsManager::instance()->getDatas();
	if (datas.count() != docks.count()) {
		PLS_WARN(ncb2bBrowserSettingsModuleName, "ncb2b browser settings item was not Synchronize with docks.");
		return;
	}
	for (int i = 0; i < docks.count(); i++) {
		auto dock = docks[i].get();
		if (!dock) {
			continue;
		}
		auto selected = datas[i].selected;
		dock->blockSignals(true);
		dock->toggleViewAction()->setChecked(selected);
		dock->setVisible(selected);
		dock->setProperty("vis", selected);
		dock->blockSignals(false);
	}
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
	auto names = basic->getNcb2bCustomDocksNames();
	auto docks = basic->getNcb2bCustomDocks();
	QMap<QString, QByteArray> geometrys;
	for (int i = 0; i < names.size(); i++) {
		auto dock = docks[i].get();
		if (dock) {
			geometrys.insert(names[i], dock->saveGeometry());
		}
	}

	auto selectedDatas = PLSNCB2bBroSettingsManager::instance()->getDatas();
	if (selectedDatas.count() >= docks.count()) {
		for (int i = 0; i < docks.count(); i++) {
			updateDock(selectedDatas[i], i, geometrys.value(selectedDatas[i].title));
		}

		for (int j = docks.count(); j < selectedDatas.count(); j++) {
			auto data = selectedDatas[j];
			auto dock = basic->addNcb2bCustomDock(data.title, data.url, QUuid::createUuid().toString(), true, geometrys.value(data.title));
			dock->setVisible(data.selected);
		}

	} else {
		for (int i = 0; i < selectedDatas.count(); i++) {
			updateDock(selectedDatas[i], i, geometrys.value(selectedDatas[i].title));
		}
		QList<QString> delNames;
		for (int j = selectedDatas.count(); j < docks.count(); j++) {
			delNames.push_back(names[j]);
		}
		for (auto name : delNames) {
			basic->RemoveDockWidget(name);
		}
	}
}

void PLSNCB2bBrowserSettings::updateDock(const PLSNCB2bBrowserSettingData &data, int index, QByteArray geometry)
{
	auto basic = OBSBasic::Get();
	if (!basic) {
		return;
	}

	auto docks = basic->getNcb2bCustomDocks();
	BrowserDock *dock = reinterpret_cast<BrowserDock *>(docks[index].get());
	if (!dock) {
		return;
	}
	auto names = basic->getNcb2bCustomDocksNames();
	auto urls = basic->getNcb2bCustomDocksUrls();
	if (names[index] == data.title && urls[index] == data.url && data.selected == dock->toggleViewAction()->isChecked()) {
		return;
	}

	dock->setTitle(data.title);
	dock->setWindowTitle(data.title);
	if (!geometry.isEmpty()) {
		dock->restoreGeometry(geometry);
	} else {
		dock->setFloating(true);
		dock->resize(460, 600);

		QPoint curPos = pos();
		QSize wSizeD2 = size() / 2;
		QSize dSizeD2 = dock->size() / 2;

		curPos.setX(curPos.x() + qAbs(wSizeD2.width() - dSizeD2.width()));
		curPos.setY(curPos.y() + qAbs(wSizeD2.height() - dSizeD2.height()));

		dock->move(curPos);
	}
	dock->setVisible(data.selected);
	dock->setProperty("vis", data.selected);
	if (names[index] != data.title) {
		basic->updateNcb2bDockName(index, data.title);
		dock->toggleViewAction()->setText(data.title);
		dock->setTitle(data.title);
		dock->setWindowTitle(data.title);
	}
	if (urls[index] != data.url) {
		dock->cefWidget->setURL(QT_TO_UTF8(data.url));
		basic->updateNcb2bDockUrl(index, data.url);
	}
}

bool PLSNCB2bBrowserSettings::getDockChecked(const QString &title)
{
	auto basic = OBSBasic::Get();
	auto names = basic->getNcb2bCustomDocksNames();
	auto docks = basic->getNcb2bCustomDocks();
	if (names.contains(title)) {
		auto index = names.indexOf(title);
		return docks[index].get()->toggleViewAction()->isChecked();
	}
	return true;
}

void PLSNCB2bBrowserSettings::updateSelected()
{
	bool listPage = ui->stackedWidget->currentWidget() == ui->listPage;

	ui->selectLabel->setVisible(seletedNumbers > 0 && listPage);
	ui->selectLabel->setText(QTStr("Ncpb2b.Browser.Settings.Selected").arg(seletedNumbers));
}

void PLSNCB2bBrowserSettings::updateChecked()
{
	auto count = ui->listWidget->count();
	for (int i = 0; i < count; i++) {
		PLSNCB2bBroSettingsItem *item = static_cast<PLSNCB2bBroSettingsItem *>(ui->listWidget->itemWidget(ui->listWidget->item(i)));
		if (!item) {
			continue;
		}
		auto data = item->getData();
		bool checked = getDockChecked(data.title);
		item->setChecked(checked);
		PLSNCB2bBroSettingsManager::instance()->setSelected(data, checked);
	}

	cacheSelectedDatas = PLSNCB2bBroSettingsManager::instance()->getDatas(true);
	seletedNumbers = cacheSelectedDatas.count();
	updateSelected();
}

QList<PLSNCB2bBrowserSettingData> PLSNCB2bBrowserSettings::parseSupportUrls(const QJsonObject &obj)
{
	QList<PLSNCB2bBrowserSettingData> datas;
	for (auto key : obj.keys()) {
		PLSNCB2bBrowserSettingData data;
		data.title = getDisplayTitle(key);
		auto value = obj.value(key);
		if (value.isDouble()) {
			data.url = QString::number(value.toInt());
		} else {
			data.url = value.toString();
		}
		data.selected = getDockChecked(data.title);
		datas.push_back(data);
	}
	return datas;
}

QString PLSNCB2bBrowserSettings::getDisplayTitle(const QString &title)
{
	return serviceName + "_" + title;
}

void PLSNCB2bBrowserSettings::onRefreshButtonClicked()
{
	if (!pls_get_network_state()) {
		ui->noNetworkLabel->setText(QTStr("Ncpb2b.Browser.Settings.No.Network.Desc"));
		ui->stackedWidget->setCurrentWidget(ui->noNetworkPage);
		return;
	}
	if (requestExisted) {
		PLS_INFO(ncb2bBrowserSettingsModuleName, "The refresh browser settings urls request already exists, avoid duplicate request.");
		return;
	}

	auto okCallback = [this](const QJsonObject &data) {
		QJsonObject supportUrl = data.value("serviceSupportUrlPc").toObject();
		if (supportUrl.isEmpty()) {
			PLS_INFO(ncb2bBrowserSettingsModuleName, "There was no serviceSupportUrlPc field value was retrieved from the api.");
			updateUI({});
		} else {
			QList<PLSNCB2bBrowserSettingData> datas = parseSupportUrls(supportUrl);
			updateUI(datas);
		}
		requestExisted = false;
	};

	auto failCallback = [this](const QJsonObject &, const PLSErrorHandler::RetData &retData) {
		PLS_INFO(ncb2bBrowserSettingsModuleName, "There was some errors was retrieved from the api.");
		if (retData.prismCode == PLSErrorHandler::CHANNEL_NCP_B2B_1101_SERVICE_DISABLED) {
			ui->noNetworkLabel->setText(QTStr("Ncb2b.Service.Disable.Status"));
		} else {
			ui->noNetworkLabel->setText(QTStr("Ncpb2b.Browser.Settings.No.Network.Desc"));
		}

		ui->stackedWidget->setCurrentWidget(ui->noNetworkPage);
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
	PLSNCB2bBroSettingsManager::instance()->initDatas(datas);

	removeAll();
	createItems(datas);

	cacheSelectedDatas = PLSNCB2bBroSettingsManager::instance()->getDatas(true);
	seletedNumbers = cacheSelectedDatas.count();

	updateDocks();
	updateSelected();
	updateLogo();
	if (datas.isEmpty()) {
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
