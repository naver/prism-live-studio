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
#include <QMetaEnum>

using namespace std;

const int LABLE_MAX_LEN = 180;
constexpr const std::array<const char *, 2> sceneDisplayTips = {"Setting.Scene.Display.Text.Tips", "Setting.Scene.Display.Thumbnail.Tips"};

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
		form->setLabelAlignment(Qt::AlignLeft);

		int rows = form->rowCount();
		labels << getLabelsFromForm(form);
	}

	if (rootWidget == ui->advancedPage) {
		QList<QLabel *> otherLabels = {advancedPage->label_7, advancedPage->label_57, advancedPage->label_21, advancedPage->label_17, advancedPage->label_56, advancedPage->bindToIPLabel};
		labels.append(otherLabels);
	}
	int maxLenth = 0;
	for (const auto lb : labels) {
		auto fontm = lb->fontMetrics();
		auto len = fontm.horizontalAdvance(lb->text());
		maxLenth = maxLenth < len ? len : maxLenth;
	}

	if (generalPage && rootWidget == ui->generalPage) {
		maxLenth = 133;
		QList<QLabel *> otherLabels = {generalPage->label_9, generalPage->label_10, generalPage->label_64};
		labels.append(otherLabels);
	}
	if (maxLenth > LABLE_MAX_LEN) {
		maxLenth = LABLE_MAX_LEN;
	}

	for (const auto lb : labels) {
		lb->setWordWrap(true);
		lb->setStyleSheet(QString("min-width: %1").arg(maxLenth));
	}
}

void OBSBasicSettings::alignVideoPage()
{
	auto forms = ui->videoPage->findChildren<QFormLayout *>();
	QList<QLabel *> labels;
	for (const auto &form : forms) {
		labels << getLabelsFromForm(form);
	}

	int maxLenth = 0;
	for (const auto lb : labels) {
		auto fontm = lb->fontMetrics();
		auto len = fontm.horizontalAdvance(lb->text());
		maxLenth = maxLenth < len ? len : maxLenth;
	}
	auto fontm = videoPage->fpsType->fontMetrics();
	auto len = videoPage->fpsType->sizeHint().width();

	maxLenth = maxLenth < len ? len : maxLenth;
	for (const auto lb : labels) {
		lb->setWordWrap(true);
		lb->setFixedWidth(qMin(maxLenth, LABLE_MAX_LEN));
	}
	videoPage->fpsType->setFixedWidth(qMin(maxLenth, LABLE_MAX_LEN));
}

void OBSBasicSettings::alignOutputPageLabels()
{
	alignLabels(ui->outputPage);
}

#define ROW_TYPE QPointer<QLabel>, QPointer<QWidget>

void OBSBasicSettings::OnSceneDisplayMethodIndexChanged(int index) const
{
	if (index < 0 || index >= sceneDisplayTips.size()) {
		return;
	}

	generalPage->sceneDisplayTipsLabel->setText(tr(sceneDisplayTips[index]));
}

void OBSBasicSettings::LoadSceneDisplayMethodSettings()
{
	loading = true;

	QStringList list;
	list << tr("Setting.Scene.Display.Text.View") << tr("Setting.Scene.Display.Thumbnail.View");
	generalPage->sceneDisplayComboBox->blockSignals(true);
	generalPage->sceneDisplayComboBox->clear();
	generalPage->sceneDisplayComboBox->addItems(list);
	generalPage->sceneDisplayComboBox->blockSignals(false);

	auto currentString = config_get_string(App()->GetUserConfig(), "BasicWindow", "SceneDisplayMethod");
	auto method = static_cast<DisplayMethod>(QMetaEnum::fromType<DisplayMethod>().keyToValue(currentString));
	auto currentIndex = static_cast<int>(method);
	if (currentIndex >= list.size() || currentIndex < 0) {
		generalPage->sceneDisplayComboBox->setCurrentIndex(0);
		OnSceneDisplayMethodIndexChanged(0);
		loading = false;
		return;
	}

	generalPage->sceneDisplayComboBox->setCurrentIndex(currentIndex);
	OnSceneDisplayMethodIndexChanged(currentIndex);
	loading = false;
}

void OBSBasicSettings::ResetSceneDisplayMethodSettings()
{
	config_remove_value(App()->GetUserConfig(), "BasicWindow", "SceneDisplayMethod");

	PLSBasic::instance()->SetSceneDisplayMethod(DisplayMethod::TextView);
}

void OBSBasicSettings::SaveSceneDisplayMethodSettings() const
{
	if (generalPage) {
		auto method = static_cast<DisplayMethod>(generalPage->sceneDisplayComboBox->currentIndex());
		auto methodStr = QMetaEnum::fromType<DisplayMethod>().valueToKey(static_cast<int>(method));
		config_set_string(App()->GetUserConfig(), "BasicWindow", "SceneDisplayMethod", methodStr);
		PLSBasic::instance()->SetSceneDisplayMethod(method);
	}
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
}

void OBSBasicSettings::updateOutPutRelatedUI()
{
	bool isOutputActived = pls_is_output_actived();
	generalPage->accountView->setEnabled(!isOutputActived);
	generalPage->language->setEnabled(!isOutputActived);
	generalPage->waterMarkGroupBox->setEnabled(!isOutputActived);

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
