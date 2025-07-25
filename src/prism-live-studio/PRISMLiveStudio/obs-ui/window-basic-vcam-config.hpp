#pragma once

#include <obs.hpp>
#include <QDialog>
#include <memory>

#include "window-basic-vcam.hpp"
#include "PLSDialogView.h"

#include "ui_OBSBasicVCamConfig.h"

struct VCamConfig;

class OBSBasicVCamConfig : public PLSDialogView {
	Q_OBJECT

	VCamConfig config;

	bool vcamActive;
	VCamOutputType activeType;
	bool requireRestart;

public:
	explicit OBSBasicVCamConfig(const VCamConfig &config, bool VCamActive, QWidget *parent = 0);

private slots:
	void OutputTypeChanged();
	void UpdateConfig();

private:
	std::unique_ptr<Ui::OBSBasicVCamConfig> ui;

signals:
	void Accepted(const VCamConfig &config);
	void AcceptedAndRestart(const VCamConfig &config);
};
