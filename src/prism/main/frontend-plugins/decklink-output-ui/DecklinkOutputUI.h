#pragma once

#include <dialog-view.hpp>

#include "ui_output.h"
#include "../../main/properties-view.hpp"

class DecklinkOutputUI : public PLSDialogView {
	Q_OBJECT
private:
	PLSPropertiesView *propertiesView;
	PLSPropertiesView *previewPropertiesView;

public slots:
	void StartOutput();
	void StopOutput();
	void PropertiesChanged();

	void StartPreviewOutput();
	void StopPreviewOutput();
	void PreviewPropertiesChanged();

public:
	std::unique_ptr<Ui_Output> ui;
	DecklinkOutputUI(QWidget *parent);

	void ShowHideDialog();

	void SetupPropertiesView();
	void SaveSettings();

	void SetupPreviewPropertiesView();
	void SavePreviewSettings();
};
