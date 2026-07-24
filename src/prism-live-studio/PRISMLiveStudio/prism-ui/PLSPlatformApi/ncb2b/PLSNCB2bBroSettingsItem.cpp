#include "PLSNCB2bBroSettingsItem.h"
#include "libui.h"
#include "liblog.h"
#include "login-user-info.hpp"
#include "libutils-api.h"
#include <QDesktopServices>

#include "ui_PLSNCB2bBroSettingsItem.h"

PLSNCB2bBroSettingsItem::PLSNCB2bBroSettingsItem(const PLSNCB2bBrowserSettingData &data_, QWidget *parent) : QFrame(parent), data(data_)
{
	ui = pls_new<Ui::PLSNCB2bBroSettingsItem>();
	ui->setupUi(this);

	pls_add_css(this, {"PLSNCB2bBrowserSettings"});
	initUI();
}

PLSNCB2bBroSettingsItem::~PLSNCB2bBroSettingsItem()
{
	pls_delete(ui, nullptr);
}

const PLSNCB2bBrowserSettingData &PLSNCB2bBroSettingsItem::getData()
{
	return data;
}

void PLSNCB2bBroSettingsItem::setChecked(bool checked)
{
	ui->selectedBtn->setChecked(checked);
}

void PLSNCB2bBroSettingsItem::initUI()
{
	ui->selectedBtn->setCheckable(true);
	ui->selectedBtn->setChecked(data.selected);
	connect(ui->selectedBtn, &QPushButton::toggled, this, [this](bool checked) { emit itemSelected(checked); });
	connect(ui->linkBtn, &QPushButton::clicked, this, [this](bool) { pls_async_invoke([this]() { QDesktopServices::openUrl(QUrl(data.url, QUrl::TolerantMode)); }); });

	ui->realNameLabel->SetText(data.title);
	QFontMetrics font(ui->realUrlLabel->font());
	int width = font.horizontalAdvance(data.url);
	if (width < 423) {
		ui->realUrlLabel->setFixedWidth(width);
	} else {
		ui->realUrlLabel->setFixedWidth(423);
	}
	ui->realUrlLabel->SetText(data.url);
}

PLSNCB2bBroSettingsManager *PLSNCB2bBroSettingsManager::instance()
{
	static PLSNCB2bBroSettingsManager manager;
	return &manager;
}

QList<PLSNCB2bBrowserSettingData> PLSNCB2bBroSettingsManager::getDatas()
{
	return datas;
}

QList<PLSNCB2bBrowserSettingData> PLSNCB2bBroSettingsManager::getDatas(bool selected)
{
	QList<PLSNCB2bBrowserSettingData> tmpDatas;
	for (auto data : datas) {
		if (data.selected == selected) {
			tmpDatas.push_back(data);
		}
	}
	return tmpDatas;
}

QList<QString> PLSNCB2bBroSettingsManager::getTitles(bool selected)
{
	QList<QString> tmpDatas;
	for (auto data : datas) {
		if (data.selected == selected) {
			tmpDatas.push_back(data.title);
		}
	}
	return tmpDatas;
}

void PLSNCB2bBroSettingsManager::initDatas(const QList<PLSNCB2bBrowserSettingData> &datas_)
{
	datas = datas_;
}

void PLSNCB2bBroSettingsManager::setSelected(const PLSNCB2bBrowserSettingData &data, bool selected)
{
	auto iter = std::find_if(datas.begin(), datas.end(), [data](const PLSNCB2bBrowserSettingData &data_) { return data_.title == data.title && data_.url == data.url; });
	if (iter != datas.end()) {
		PLSNCB2bBrowserSettingData &setting = *iter;
		setting.selected = selected;
	}
}

void PLSNCB2bBroSettingsManager::setSelected(const QList<PLSNCB2bBrowserSettingData> &srcDatas)
{
	for (const auto &data : srcDatas) {
		setSelected(data, data.selected);
	}
}

bool PLSNCB2bBroSettingsManager::getSelected(const PLSNCB2bBrowserSettingData &data)
{
	auto iter = std::find_if(datas.begin(), datas.end(), [data](const PLSNCB2bBrowserSettingData &data_) { return data_.title == data.title && data_.url == data.url; });
	if (iter != datas.end()) {
		return (*iter).selected;
	}
	return true;
}

QList<PLSNCB2bBrowserSettingData> PLSNCB2bBroSettingsManager::parseSupportUrls(const QJsonObject &obj)
{
	QList<PLSNCB2bBrowserSettingData> datas;
	for (auto key : obj.keys()) {
		PLSNCB2bBrowserSettingData data;
		data.title = getDisplayTitle(key);
		auto value = obj.value(key);
		if (value.isDouble()) {
			data.url = QString::number(value.toInteger());
		} else {
			data.url = value.toString();
		}
		datas.push_back(data);
	}
	return datas;
}

QString PLSNCB2bBroSettingsManager::getDisplayTitle(const QString &title)
{
	return PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName() + "_" + title;
}
