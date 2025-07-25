#include "ResolutionGuidePage.h"
#include "PLSMainView.hpp"
#include "window-basic-main.hpp"
#include "PLSBasic.h"
#include "PLSPlatformApi.h"
#include "source-label.hpp"
#include "window-basic-settings.hpp"
#include <string>
#include <algorithm>
#include "qt-wrappers.hpp"
#include "PLSChannelDataAPI.h"
#include "PLSSyncServerManager.hpp"
#include "login-user-info.hpp"
#include "PLSSceneDataMgr.h"
#include "pls-common-define.hpp"
#include "pls/pls-dual-output.h"

#include <QStandardItemModel>

using namespace std;

const int SCENE_DISPLAY_TIPS_NUM = 3;
const int LABLE_MAX_LEN = 180;
constexpr const std::array<const char *, SCENE_DISPLAY_TIPS_NUM> sceneDisplayTips = {"Setting.Scene.Display.Realtime.Tips", "Setting.Scene.Display.Thumbnail.Tips", "Setting.Scene.Display.Text.Tips"};

class OutsideNode : public QObject {
public:
	static OutsideNode *instance()
	{
		static OutsideNode *ins = nullptr;
		if (ins == nullptr) {
			ins = new OutsideNode();
		}
		return ins;
	}

	~OutsideNode() {};

private:
	void initialize()
	{
		auto win = OBSBasic::Get();
		auto platApi = PLSPlatformApi::instance();
	};

	OutsideNode() { initialize(); };
};

void intializeOutNode()
{
	OutsideNode::instance();
}

void OBSBasicSettings::adjustUi()
{
	auto scrollareas = ui->outputPage->findChildren<QScrollArea *>();
	for (const auto area : scrollareas) {
		area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
	}
	ui->advOutTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
	auto forms = this->findChildren<QFormLayout *>();
	for (const auto form : forms) {
		form->setLabelAlignment(Qt::AlignLeft);
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		form->setVerticalSpacing(10);
		form->setHorizontalSpacing(20);
		form->setContentsMargins(0, 0, 0, 0);
	}

	auto grids = this->findChildren<QGridLayout *>();
	for (const auto &grid : grids) {
		grid->setVerticalSpacing(10);
		grid->setContentsMargins(0, 0, 0, 0);
	}

	ui->listWidget->setIconSize(QSize(0, 0));
	ui->listWidget->setRowHidden(STREAM, true);
	ui->listWidget->setProperty("notShowHandCursor", true);

	ui->openStatsOnStartup->hide();
	ui->formLayout_32->takeRow(ui->verticalLayout_32);

	if (ui->updateSettingsGroupBox) {
		ui->updateSettingsGroupBox->hide();
	}

	QString userServiceName = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName();
	if (userServiceName.isEmpty()) {
		ui->waterMarkGroupBox->hide();
	} else {
		ui->waterMarkGroupBox->show();
	}

	ui->warnBeforeStreamStart->hide();
	ui->warnBeforeStreamStop->hide();
	ui->warnBeforeRecordStop->hide();
	ui->formLayout_2->takeRow(ui->warnBeforeStreamStart);
	ui->formLayout_2->takeRow(ui->warnBeforeStreamStop);
	ui->formLayout_2->takeRow(ui->warnBeforeRecordStop);
	ui->groupBox_19->hide();

	ui->verticalLayout->setContentsMargins(0, 0, 25, 0); //video
	ui->hotkeyFormLayout->setContentsMargins(0, 0, 0, 50);

	auto scrolls = this->findChildren<QScrollArea *>();
	for (const auto &scroll : scrolls) {
		auto ctw = scroll->widget();
		if (ctw == nullptr) {
			continue;
		}
		ctw->setContentsMargins(0, 0, 25, 0);
	}
	ui->hotkeyScrollContents->setContentsMargins(0, 0, 0, 0);
	ui->hotkeySearchLayout->setContentsMargins(0, 0, 35, 40);
	ui->formLayout_5->setContentsMargins(0, 0, 35, 10);
	ui->formLayout_9->setContentsMargins(0, 0, 35, 0);
	alignOutputPageLabels();
	alignLabels(ui->audioPage);
	alignLabels(ui->advancedPage);
	alignLabels(ui->widget_2);
	alignVideoPage();

	auto advCheckboxs = ui->recTracks->findChildren<PLSCheckBox *>();
	for (auto checkbox : advCheckboxs) {
		checkbox->setSpac(5);
	}

	auto simpleCheckboxs = ui->simpleRecTracks->findChildren<PLSCheckBox *>();
	for (auto checkbox : simpleCheckboxs) {
		checkbox->setSpac(5);
	}
	auto advOutFFTracks = ui->widget_10->findChildren<PLSCheckBox *>();
	for (auto checkbox : advOutFFTracks) {
		checkbox->setSpac(5);
	}
}

QList<QLabel *> OBSBasicSettings::getLabelsFromForm(const QFormLayout *form) const
{
	QList<QLabel *> labels;
	int rows = form->rowCount();
	for (int i = 0; i < rows; ++i) {
		auto lbitem = form->itemAt(i, QFormLayout::LabelRole);
		if (lbitem == nullptr) {
			continue;
		}
		auto lb = dynamic_cast<QLabel *>(lbitem->widget());
		if (lb) {
			labels << lb;
		}
	}
	return labels;
}

void OBSBasicSettings::alignLabels(QWidget *rootWidget)
{
	auto forms = rootWidget->findChildren<QFormLayout *>();
	QList<QLabel *> labels;
	for (const auto &form : forms) {
		int rows = form->rowCount();
		labels << getLabelsFromForm(form);
	}

	if (rootWidget->objectName() == "advancedPage") {
		QList<QLabel *> otherLabels = {ui->label_7, ui->label_57, ui->label_21, ui->label_17, ui->label_56, ui->bindToIPLabel};
		labels.append(otherLabels);
	}
	int maxLenth = 0;
	for (const auto lb : labels) {
		auto fontm = lb->fontMetrics();
		auto len = fontm.horizontalAdvance(lb->text());
		maxLenth = maxLenth < len ? len : maxLenth;
	}

	if (rootWidget->objectName() == "widget_2") {
		maxLenth = 133; //according to PLSSettingGeneralView spacing
		QList<QLabel *> otherLabels = {ui->label_9, ui->label_10, ui->label_64};
		labels.append(otherLabels);
	}
	if (maxLenth > LABLE_MAX_LEN) {
		maxLenth = LABLE_MAX_LEN;
	}

	for (const auto lb : labels) {
		if (lb != ui->advStreamTrackWidgetLabel) {
			lb->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		}
		lb->setWordWrap(true);
		lb->setStyleSheet(QString("min-width:%1px;").arg(maxLenth));
	}
}

void OBSBasicSettings::alignVideoPage()
{
	auto forms = ui->videoPage->findChildren<QFormLayout *>();
	QList<QLabel *> labels;
	for (const auto &form : forms) {
		int rows = form->rowCount();
		labels << getLabelsFromForm(form);
	}

	int maxLenth = 0;
	for (const auto lb : labels) {
		auto fontm = lb->fontMetrics();
		auto len = fontm.horizontalAdvance(lb->text());
		maxLenth = maxLenth < len ? len : maxLenth;
	}
	auto fontm = ui->fpsType->fontMetrics();
	auto len = fontm.horizontalAdvance(ui->fpsType->itemText(ui->fpsType->currentIndex()));

	maxLenth = maxLenth < len ? len : maxLenth;
	for (const auto lb : labels) {
		lb->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		lb->setWordWrap(true);
		lb->setStyleSheet(QString("min-width:%1px;").arg(maxLenth));
	}
	ui->fpsType->setMinimumWidth(maxLenth);
}

void OBSBasicSettings::alignOutputPageLabels()
{
	alignLabels(ui->outputPage);
}

#define ROW_TYPE QPointer<QLabel>, QPointer<QWidget>

void OBSBasicSettings::OnSceneDisplayMethodIndexChanged(int index) const
{
	if (index < 0 || index >= SCENE_DISPLAY_TIPS_NUM) {
		return;
	}

	ui->sceneDisplayTipsLabel->setText(tr(sceneDisplayTips[index]));
}

void OBSBasicSettings::LoadSceneDisplayMethodSettings()
{
	loading = true;

	QStringList list;
	list << tr("Setting.Scene.Display.Realtime.View") << tr("Setting.Scene.Display.Thumbnail.View") << tr("Setting.Scene.Display.Text.View");
	ui->sceneDisplayComboBox->blockSignals(true);
	ui->sceneDisplayComboBox->addItems(list);
	ui->sceneDisplayComboBox->blockSignals(false);

	if (PLSSceneDataMgr::Instance()->GetSceneSize() > common::SCENE_RENDER_NUMBER) {
		ui->sceneDisplayComboBox->setItemData(static_cast<int>(DisplayMethod::DynamicRealtimeView), QVariant::fromValue(false), Qt::UserRole - 1);
	}

	auto currentIndex = (int)config_get_int(App()->GetUserConfig(), "BasicWindow", "SceneDisplayMethod");
	if (currentIndex >= list.size() || currentIndex < 0) {
		if (PLSSceneDataMgr::Instance()->GetSceneSize() > common::SCENE_RENDER_NUMBER) {
			ui->sceneDisplayComboBox->setCurrentIndex(1);
			OnSceneDisplayMethodIndexChanged(1);
		} else {
			ui->sceneDisplayComboBox->setCurrentIndex(0);
			OnSceneDisplayMethodIndexChanged(0);
		}
		loading = false;
		return;
	}

	ui->sceneDisplayComboBox->setCurrentIndex(currentIndex);
	OnSceneDisplayMethodIndexChanged(currentIndex);
	loading = false;
}

void OBSBasicSettings::ResetSceneDisplayMethodSettings()
{
	ui->sceneDisplayComboBox->clear();
	config_remove_value(App()->GetUserConfig(), "BasicWindow", "SceneDisplayMethod");

	if (PLSSceneDataMgr::Instance()->GetSceneSize() > common::SCENE_RENDER_NUMBER) {
		PLSBasic::instance()->SetSceneDisplayMethod(1); // 5s
	} else {
		PLSBasic::instance()->SetSceneDisplayMethod(0);
	}
}

void OBSBasicSettings::SaveSceneDisplayMethodSettings() const
{
	config_set_int(App()->GetUserConfig(), "BasicWindow", "SceneDisplayMethod", ui->sceneDisplayComboBox->currentIndex());
	PLSBasic::instance()->SetSceneDisplayMethod(ui->sceneDisplayComboBox->currentIndex());
}

void OBSBasicSettings::ConnectUiSignals()
{
	connect(ui->sceneDisplayComboBox, QOverload<int>::of(&PLSComboBox::currentIndexChanged), this, [this](int index) {
		OnSceneDisplayMethodIndexChanged(index);

		if (!loading) {
			EnableApplyButton(true);
		}
	});
}
bool OBSBasicSettings::IgnoreInvisibleHotkeys(obs_source_t *source, const char *name)
{
	OBSData privateSettings = obs_source_get_private_settings(source);
	bool invisible = obs_data_get_bool(privateSettings, "hotkey_invisible");
	if (!invisible) {
		return false;
	}
	OBSDataArrayAutoRelease dataArray = obs_data_get_array(privateSettings, "invisible_lists");
	for (int i = 0; i < obs_data_array_count(dataArray); i++) {
		OBSDataAutoRelease data = obs_data_array_item(dataArray, i);
		const char *hotkeyName = obs_data_get_string(data, "hotkey_name");
		if (0 == strcmp(name, hotkeyName)) {
			return true;
		}
	}

	return false;
}
void OBSBasicSettings::initOutPutChangedTipUi()
{
	mCannotTip.isValid = true;
	connect(ui->listWidget, &QListWidget::currentRowChanged, this, &OBSBasicSettings::checkOutputTipsVisible, Qt::QueuedConnection);
	connect(PLSBasic::instance(), &PLSBasic::outputStateChanged, this, &OBSBasicSettings::checkOutputTipsVisible, Qt::QueuedConnection);
	connect(PLSBasic::instance(), &PLSBasic::outputStateChanged, this, &OBSBasicSettings::updateButtonsState, Qt::QueuedConnection);

	connect(this, &OBSBasicSettings::sigSaveVideoFailed, this, &OBSBasicSettings::reloadOutputRelatedSettings, Qt::QueuedConnection);
	updateOutPutRelatedUI();
}
void OBSBasicSettings::reloadOutputRelatedSettings()
{
	this->LoadVideoSettings();
	this->LoadOutputSettings();
	this->LoadAudioSettings();
	this->LoadAdvancedSettings();
	this->LoadStream1Settings();
	updateButtonsState();
}

void OBSBasicSettings::updateOutPutRelatedUI()
{
	//output related
	bool isOutputActived = pls_is_output_actived();
	ui->language->setEnabled(!isOutputActived);
	ui->waterMarkGroupBox->setEnabled(!isOutputActived);

	updateButtonsState();
	pls_async_call_mt([this]() { checkOutputTipsVisible(); });
}

void OBSBasicSettings::updateButtonsState()
{
	bool isOutputActived = pls_is_output_actived();
	ui->resetButton->setEnabled(!isOutputActived);
	if (isOutputActived) {
		EnableApplyButton(false);
	}
}

void OBSBasicSettings::checkOutputTipsVisible()
{
	switch (ui->listWidget->currentRow()) {
	case Pages::GENERAL:
	case Pages::OUTPUT:
	case Pages::AUDIO:
	case Pages::VIDEO:
	case Pages::ADVANCED:
		updateOutputTipsUI();
		break;
	default:
		setVisibleOfOutputTips(false);
		break;
	}
}

void OBSBasicSettings::updateOutputTipsUI()
{
	if (mCannotTip.checkIsCanChange()) {
		setVisibleOfOutputTips(false);
		return;
	}

	mCannotTip.updateText();
	setVisibleOfOutputTips(true);
}

void OBSBasicSettings::setVisibleOfOutputTips(bool visible)
{
	if (visible) {
		updateAlertMessage(AlertMessageType::Warning, ui->settingsPages->currentWidget(), mCannotTip.mText, 0);
		return;
	}
	clearAlertMessage(AlertMessageType::Warning, ui->settingsPages->currentWidget(), true);
}

void OBSBasicSettings::onOutputTipsVisibilityChanged(bool visible)
{
	if (visible) {
		setVisibleOfErrorTips(false);
		return;
	}
	updateAlertMessage();
}

bool OBSBasicSettings::prepareStreamServiceData(QStringList &names) const
{
	if (pls_is_dual_output_on()) {
		return false;
	}

	if (PLS_PLATFORM_API->isLiving()) {
		obs_service_t *service_obj = main->GetService();
		OBSDataAutoRelease serviceSettings = obs_service_get_settings(service_obj);
		const char *service = obs_data_get_string(serviceSettings, "service");
		if ((pls_is_equal(service, "Prism") || pls_is_equal(service, ""))) {
			names.clear();
			return false;
		}
	}
	auto jsonMap = PLSSyncServerManager::instance()->getStreamService();
	auto activiedPlatforms = PLS_PLATFORM_ACTIVIED;
	if (activiedPlatforms.size() == 1) {

		if (auto pPlatform = activiedPlatforms.front(); pPlatform->getChannelType() >= ChannelData::ChannelDataType::CustomType) {
			if (pPlatform->getChannelName() == TWITCH) {
				names << "Twitch - RTMPS";
				return true;
			}
			return false;
		}
		auto channelName = activiedPlatforms.front()->getChannelName();
		auto param = jsonMap.value(channelName).toMap();
		if (!param.isEmpty()) {
			auto nameList = param.value("name").toList();
			if (nameList.empty()) {
				return false;
			}
			for (int i = 0; i < nameList.count(); i++) {
				names.push_back(nameList.value(i).toString());
			}
			return true;
		}
	}
	return false;
}
