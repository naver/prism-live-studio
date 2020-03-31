#include "DecklinkOutputUI.h"
#include <obs-module.h>
#include <util/platform.h>
#include <util/util.hpp>
#include "decklink-ui-main.h"
#include "../../pls-common-define.hpp"
#include "log.h"
#include "action.h"

#include <QFormLayout>

namespace {

class CustomPropertiesView : public PLSPropertiesView {
	DecklinkOutputUI *m_output;

public:
	explicit CustomPropertiesView(DecklinkOutputUI *output, OBSData settings, const char *type, PropertiesReloadCallback reloadCallback, int minSize = 0, int maxSize = -1)
		: PLSPropertiesView(settings, type, reloadCallback, minSize, maxSize), m_output(output)
	{
		RefreshProperties();
	}

	void RefreshProperties()
	{
		lastPropertyType = OBS_PROPERTY_INVALID;
		PLSPropertiesView::RefreshProperties();
		QScrollArea::widget()->setContentsMargins(0, 0, 0, 0);
		if (obs_property_t *property = obs_properties_first(properties.get()); !property) {
			dynamic_cast<QFormLayout *>(QScrollArea::widget()->layout())->setHorizontalSpacing(0);
		}
	}

	void AddSpacer(const obs_property_type &currentType, QFormLayout *layout)
	{
		if (lastPropertyType != OBS_PROPERTY_INVALID) {
			QLabel *spaceLabel = new QLabel(this);
			spaceLabel->setObjectName(OBJECT_NAME_SPACELABEL);
			if (isSamePropertyType(lastPropertyType, currentType)) {
				spaceLabel->setFixedSize(10, PROPERTIES_VIEW_VERTICAL_SPACING_MIN);
			} else {
				spaceLabel->setFixedSize(10, PROPERTIES_VIEW_VERTICAL_SPACING_MAX);
			}
			layout->addRow(spaceLabel, spaceLabel);
		}
		lastPropertyType = currentType;
	}

	bool isSamePropertyType(obs_property_type a, obs_property_type b)
	{
		switch (a) {
		case OBS_PROPERTY_BOOL:
			switch (b) {
			case OBS_PROPERTY_BOOL:
				return true;
			default:
				return false;
			}
			break;
		case OBS_PROPERTY_INT:
		case OBS_PROPERTY_FLOAT:
		case OBS_PROPERTY_TEXT:
		case OBS_PROPERTY_PATH:
		case OBS_PROPERTY_LIST:
		case OBS_PROPERTY_COLOR:
		case OBS_PROPERTY_BUTTON:
		case OBS_PROPERTY_FONT:
		case OBS_PROPERTY_EDITABLE_LIST:
		case OBS_PROPERTY_FRAME_RATE:
			switch (b) {
			case OBS_PROPERTY_INT:
			case OBS_PROPERTY_FLOAT:
			case OBS_PROPERTY_TEXT:
			case OBS_PROPERTY_PATH:
			case OBS_PROPERTY_LIST:
			case OBS_PROPERTY_COLOR:
			case OBS_PROPERTY_BUTTON:
			case OBS_PROPERTY_FONT:
			case OBS_PROPERTY_EDITABLE_LIST:
			case OBS_PROPERTY_FRAME_RATE:
				return true;
			default:
				return false;
			}
		default:
			break;
		}
		return true;
	}
};
}

DecklinkOutputUI::DecklinkOutputUI(QWidget *parent) : PLSDialogView(parent), ui(new Ui_Output)
{
	setResizeEnabled(false);
	ui->setupUi(this->content());
	setFixedSize(720, 700);
	QMetaObject::connectSlotsByName(this);
	setSizeGripEnabled(false);

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	propertiesView = nullptr;
	previewPropertiesView = nullptr;

	connect(ui->startOutput, SIGNAL(released()), this, SLOT(StartOutput()));
	connect(ui->stopOutput, SIGNAL(released()), this, SLOT(StopOutput()));

	connect(ui->startPreviewOutput, SIGNAL(released()), this, SLOT(StartPreviewOutput()));
	connect(ui->stopPreviewOutput, SIGNAL(released()), this, SLOT(StopPreviewOutput()));
}

void DecklinkOutputUI::ShowHideDialog()
{
	SetupPropertiesView();
	SetupPreviewPropertiesView();

	setVisible(!isVisible());
}

void DecklinkOutputUI::SetupPropertiesView()
{
	if (propertiesView)
		delete propertiesView;

	obs_data_t *settings = obs_data_create();

	OBSData data = load_settings();
	if (data)
		obs_data_apply(settings, data);

	propertiesView = new CustomPropertiesView(this, settings, "decklink_output", (PropertiesReloadCallback)obs_get_output_properties, 65, 65);

	ui->propertiesLayout->addWidget(propertiesView);
	obs_data_release(settings);

	connect(propertiesView, SIGNAL(Changed()), this, SLOT(PropertiesChanged()));
}

void DecklinkOutputUI::SaveSettings()
{
	BPtr<char> modulePath = obs_module_get_config_path(obs_current_module(), "");

	os_mkdirs(modulePath);

	BPtr<char> path = obs_module_get_config_path(obs_current_module(), "decklinkOutputProps.json");

	obs_data_t *settings = propertiesView->GetSettings();
	if (settings)
		obs_data_save_json_safe(settings, path, "tmp", "bak");
}

void DecklinkOutputUI::SetupPreviewPropertiesView()
{
	if (previewPropertiesView)
		delete previewPropertiesView;

	obs_data_t *settings = obs_data_create();

	OBSData data = load_preview_settings();
	if (data)
		obs_data_apply(settings, data);

	previewPropertiesView = new CustomPropertiesView(this, settings, "decklink_output", (PropertiesReloadCallback)obs_get_output_properties, 65, 65);

	ui->previewPropertiesLayout->addWidget(previewPropertiesView);
	obs_data_release(settings);

	connect(previewPropertiesView, SIGNAL(Changed()), this, SLOT(PreviewPropertiesChanged()));
}

void DecklinkOutputUI::SavePreviewSettings()
{
	char *modulePath = obs_module_get_config_path(obs_current_module(), "");

	os_mkdirs(modulePath);

	char *path = obs_module_get_config_path(obs_current_module(), "decklinkPreviewOutputProps.json");

	obs_data_t *settings = previewPropertiesView->GetSettings();
	if (settings)
		obs_data_save_json_safe(settings, path, "tmp", "bak");
}

void DecklinkOutputUI::StartOutput()
{
	PLS_PLUGIN_UI_STEP("Decklink Output > Output Start Button", ACTION_CLICK);
	SaveSettings();
	output_start();
}

void DecklinkOutputUI::StopOutput()
{
	PLS_PLUGIN_UI_STEP("Decklink Output > Output Stop Button", ACTION_CLICK);
	output_stop();
}

void DecklinkOutputUI::PropertiesChanged()
{
	SaveSettings();
}

void DecklinkOutputUI::StartPreviewOutput()
{
	PLS_PLUGIN_UI_STEP("Decklink Output > Preview Output Start Button", ACTION_CLICK);
	SavePreviewSettings();
	preview_output_start();
}

void DecklinkOutputUI::StopPreviewOutput()
{
	PLS_PLUGIN_UI_STEP("Decklink Output > Preview Output Stop Button", ACTION_CLICK);
	preview_output_stop();
}

void DecklinkOutputUI::PreviewPropertiesChanged()
{
	SavePreviewSettings();
}
