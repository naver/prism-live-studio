#include "window-basic-vcam-config.hpp"
#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"

#include <util/util.hpp>
#include <util/platform.h>

#include "log/module_names.h"
#include "liblog.h"

#include <QStandardItem>

OBSBasicVCamConfig::OBSBasicVCamConfig(const VCamConfig &_config,
				       bool _vcamActive, QWidget *parent)
	: config(_config),
	  vcamActive(_vcamActive),
	  activeType(_config.type),
	  PLSDialogView(parent),
	  ui(new Ui::OBSBasicVCamConfig)
{
	//setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	pls_add_css(this, {"OBSBasicVCamConfig"});

	ui->setupUi(this->content());

	ui->outputType->addItem(QTStr("Basic.VCam.OutputType.Program"),
				(int)VCamOutputType::ProgramView);
	ui->outputType->addItem(QTStr("StudioMode.Preview"),
				(int)VCamOutputType::PreviewOutput);
	ui->outputType->addItem(QTStr("Basic.Scene"),
				(int)VCamOutputType::SceneOutput);
	ui->outputType->addItem(QTStr("Basic.Main.Source"),
				(int)VCamOutputType::SourceOutput);

	ui->outputType->setCurrentIndex(
		ui->outputType->findData((int)config.type));
	OutputTypeChanged();
	connect(ui->outputType, &QComboBox::currentIndexChanged, this,
		&OBSBasicVCamConfig::OutputTypeChanged);

	connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
		&OBSBasicVCamConfig::UpdateConfig);

	connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
		&QDialog::accept);

	connect(ui->buttonBox, &QDialogButtonBox::rejected, this,
		&QDialog::reject);
}

void OBSBasicVCamConfig::OutputTypeChanged()
{
	VCamOutputType type =
		(VCamOutputType)ui->outputType->currentData().toInt();
	ui->outputSelection->setDisabled(false);

	auto list = ui->outputSelection;
	list->clear();

	switch (type) {
	case VCamOutputType::Invalid:
	case VCamOutputType::ProgramView:
	case VCamOutputType::PreviewOutput:
		ui->outputSelection->setDisabled(true);
		list->addItem(QTStr("Basic.VCam.OutputSelection.NoSelection"));
		break;
	case VCamOutputType::SceneOutput: {
		// Scenes in default order
		BPtr<char *> scenes = obs_frontend_get_scene_names();
		for (char **temp = scenes; *temp; temp++) {
			list->addItem(*temp);

			if (config.scene.compare(*temp) == 0)
				list->setCurrentIndex(list->count() - 1);
		}
		break;
	}
	case VCamOutputType::SourceOutput: {
		// Sources in alphabetical order
		std::vector<std::string> sources;
		auto AddSource = [&](obs_source_t *source) {
			auto name = obs_source_get_name(source);

			if (!(obs_source_get_output_flags(source) &
			      OBS_SOURCE_VIDEO))
				return;

			sources.push_back(name);
		};
		using AddSource_t = decltype(AddSource);

		obs_enum_sources(
			[](void *data, obs_source_t *source) {
				auto &AddSource =
					*static_cast<AddSource_t *>(data);
				if (!obs_source_removed(source))
					AddSource(source);
				return true;
			},
			static_cast<void *>(&AddSource));

		// Sort and select current item
		sort(sources.begin(), sources.end());
		for (auto &&source : sources) {
			list->addItem(source.c_str());

			if (config.source == source)
				list->setCurrentIndex(list->count() - 1);
		}
		break;
	}
	}

	if (!vcamActive)
		return;

	requireRestart = (activeType == VCamOutputType::ProgramView &&
			  type != VCamOutputType::ProgramView) ||
			 (activeType != VCamOutputType::ProgramView &&
			  type == VCamOutputType::ProgramView);

	ui->warningLabel->setVisible(requireRestart);
}

static const char *getVCamOutputTypeStr(VCamOutputType type)
{
	switch (type) {
	case VCamOutputType::Invalid:
		return "Invalid";
	case VCamOutputType::SceneOutput:
		return "SceneOutput";
	case VCamOutputType::SourceOutput:
		return "SourceOutput";
	case VCamOutputType::ProgramView:
		return "ProgramView";
	case VCamOutputType::PreviewOutput:
		return "PreviewOutput";
	default:
		return "Invalid";
	}
}

void OBSBasicVCamConfig::UpdateConfig()
{
	VCamOutputType type =
		(VCamOutputType)ui->outputType->currentData().toInt();
	switch (type) {
	case VCamOutputType::ProgramView:
	case VCamOutputType::PreviewOutput:
		break;
	case VCamOutputType::SceneOutput:
		config.scene = ui->outputSelection->currentText().toStdString();
		break;
	case VCamOutputType::SourceOutput:
		config.source =
			ui->outputSelection->currentText().toStdString();
		break;
	default:
		// unknown value, don't save type
		return;
	}

	PLS_LOGEX(PLS_LOG_INFO, MAIN_OUTPUT,
		  {{"virtualCamOutputType", getVCamOutputTypeStr(type)}},
		  "[Output] virtualCamOutputType : %s.",
		  getVCamOutputTypeStr(type));

	config.type = type;

	if (requireRestart) {
		emit AcceptedAndRestart(config);
	} else {
		emit Accepted(config);
	}
}
