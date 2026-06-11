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

#include <QDialog>
#include <QDialogButtonBox>
#include <QPointer>
#include <QSplitter>
#include "qt-display.hpp"
#include <obs.hpp>
#include "PLSDialogView.h"

class PLSPropertiesView;
class OBSBasic;

const auto OPERATION_NONE = 0X00000000;
const auto OPERATION_ADD_SOURCE = 0X00000001;

#include "ui_OBSBasicProperties.h"

class OBSBasicProperties : public PLSDialogView {
	Q_OBJECT

protected:
	OBSBasic *main;

	std::unique_ptr<Ui::OBSBasicProperties> ui;
	bool acceptClicked;
	bool isClosed = false;

	OBSSource source;
	OBSSignal removedSignal;
	OBSSignal renamedSignal;
	OBSSignal updatePropertiesSignal;
	OBSData oldSettings;
	PLSPropertiesView *view;

	OBSSourceAutoRelease sourceA;
	OBSSourceAutoRelease sourceB;
	OBSSourceAutoRelease sourceClone;
	bool direction = true;

	bool m_isSaveClick = false;
	qreal m_dpi = 1.0;

	static void SourceRemoved(void *data, calldata_t *params);
	static void SourceRenamed(void *data, calldata_t *params);
	static void UpdateProperties(void *data, calldata_t *params);
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static void DrawTransitionPreview(void *data, uint32_t cx, uint32_t cy);
	void UpdateCallback(void *obj, obs_data_t *settings);
	bool ConfirmQuit();
	int CheckSettings();
	void UpdateOldSettings();
	void Cleanup();
	void dialogClosedToSendNoti();
	bool isPaidSource();

private slots:
	void on_buttonBox_clicked(QAbstractButton *button);
	void AddPreviewButton();

public:
	OBSBasicProperties(QWidget *parent, OBSSource source_);

	//PRISM/renjinbo/ 20230104/#/add virtual method
	virtual ~OBSBasicProperties();

	void Init();
	OBSSource GetSource() const;
	void OnButtonBoxCancelClicked(OBSSource source);
	void ReloadProperties();

protected:
	virtual void closeEvent(QCloseEvent *event) override;
	virtual bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
	virtual void reject() override;
	virtual void paintEvent(QPaintEvent *event) override;
};
