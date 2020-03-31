/******************************************************************************
    Copyright (C) 2014 by Hugh Bailey <obs.jim@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#include <QDialogButtonBox>
#include <QPointer>
#include <QSplitter>
#include <obs.hpp>

#include "dialog-view.hpp"
#include "qt-display.hpp"

class PLSPropertiesView;
class PLSBasic;

#define OPERATION_NONE 0X00000000
#define OPERATION_ADD_SOURCE 0X00000001

class PLSBasicProperties : public PLSDialogView {
	Q_OBJECT

private:
	QPointer<PLSQTDisplay> preview;

	bool acceptClicked;

	OBSSource source;
	OBSSignal removedSignal;
	OBSSignal renamedSignal;
	OBSSignal updatePropertiesSignal;
	OBSData oldSettings;
	PLSPropertiesView *view;
	QDialogButtonBox *buttonBox;
	QSplitter *windowSplitter;

	OBSSource sourceA;
	OBSSource sourceB;
	OBSSource sourceClone;
	bool direction = true;
	unsigned operationFlags;

	static void SourceRemoved(void *data, calldata_t *params);
	static void SourceRenamed(void *data, calldata_t *params);
	static void UpdateProperties(void *data, calldata_t *params);
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static void DrawTransitionPreview(void *data, uint32_t cx, uint32_t cy);
	void UpdateCallback(void *obj, obs_data_t *settings);
	bool ConfirmQuit();
	int CheckSettings();
	void Cleanup();

private slots:
	void on_buttonBox_clicked(QAbstractButton *button);
	void AddPreviewButton();

public:
	explicit PLSBasicProperties(QWidget *parent, OBSSource source_, unsigned flags = OPERATION_NONE);
	~PLSBasicProperties();

	void Init();
	OBSSource GetSource();
	void OnButtonBoxCancelClicked(OBSSource source);

protected:
	virtual void closeEvent(QCloseEvent *event) override;
	virtual void reject() override;

signals:
	void propertiesChanged(OBSSource source);
	void OpenFilters();
};
