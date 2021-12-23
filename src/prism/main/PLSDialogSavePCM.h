#pragma once
#include "dialog-view.hpp"
#include "ui_PLSDialogSavePCM.h"

class PLSDialogSavePCM : public PLSDialogView {
	Q_OBJECT

public:
	PLSDialogSavePCM(QWidget *parent = Q_NULLPTR, PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSDialogSavePCM();

public slots:
	void OnStartSaveClicked();

private:
	Ui::PLSDialogSavePCM ui;
};
