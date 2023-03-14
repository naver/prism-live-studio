#include "PLSDialogSavePCM.h"
#include "pls-common-define.hpp"

PLSDialogSavePCM::PLSDialogSavePCM(QWidget *parent, PLSDpiHelper dpiHelper) : PLSDialogView(parent, dpiHelper)
{
	ui.setupUi(content());

	dpiHelper.setCss(this, {PLSCssIndex::QPushButton, PLSCssIndex::QComboBox, PLSCssIndex::QLineEdit});
	dpiHelper.setFixedSize(this, {480, 200});
	dpiHelper.setFixedSize(ui.pcmSaveDuration, {30, 30});
	dpiHelper.setFixedSize(ui.label, {80, 30});
	dpiHelper.setFixedSize(ui.btnStartSavePCM, {150, 40});

	ui.pcmSaveDuration->setValidator(new QRegExpValidator(QRegExp("[0-9]+$")));
	ui.pcmSaveDuration->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

	ui.label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
	ui.label->setText("seconds");

	connect(ui.btnStartSavePCM, &QPushButton::clicked, this, &PLSDialogSavePCM::OnStartSaveClicked);

	auto enumAudioSource = [](void *param, obs_source_t *source) -> bool {
		if (source) {
			QVector<OBSSource> &outputList = *reinterpret_cast<QVector<OBSSource> *>(param);
			if (0 == strcmp(obs_source_get_id(source), AUDIO_INPUT_SOURCE_ID) || 0 == strcmp(obs_source_get_id(source), AUDIO_OUTPUT_SOURCE_ID)) {
				outputList.push_back(source);
			}
		}

		return true;
	};

	QVector<OBSSource> sourcesList;
	obs_enum_sources(enumAudioSource, &sourcesList);

	for (int i = 0; i < sourcesList.count(); i++) {
		const char *name = obs_source_get_name(sourcesList[i]);
		if (name) {
			ui.audioSourceList->addItem(name);
		}
	}
}

PLSDialogSavePCM::~PLSDialogSavePCM() {}

void PLSDialogSavePCM::OnStartSaveClicked()
{
	int sec = ui.pcmSaveDuration->text().toInt();
	if (sec <= 0) {
		ui.pcmSaveDuration->setFocus();
		return;
	}

	int index = ui.audioSourceList->currentIndex();
	if (index < 0) {
		return;
	}

	QString text = ui.audioSourceList->currentText();
	OBSSource source = pls_get_source_by_name(text.toStdString().c_str());
	if (!source) {
		return;
	}

	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "method", "save_pcm");
	obs_data_set_int(data, "duration_seconds", sec);
	obs_source_set_private_data(source, data);
	obs_data_release(data);

	done(Accepted);
}
