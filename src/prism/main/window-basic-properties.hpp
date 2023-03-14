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
#include "dialogbuttonbox.hpp"
#include "PLSGetPropertiesThread.h"

class PLSPropertiesView;
class PLSBasic;
class PLSLoadingEvent;

#define OPERATION_NONE 0X00000000
#define OPERATION_ADD_SOURCE 0X00000001

class PLSBasicProperties : public PLSDialogView {
	Q_OBJECT

private:
	QPointer<PLSQTDisplay> preview;

	bool acceptClicked;
	bool m_isSaveClick = false;

	OBSSource source;
	OBSSignal removedSignal;
	OBSSignal renamedSignal;
	OBSSignal updatePropertiesSignal;
	OBSSignal updatePropertiesNotifySignal;
	OBSSignal updatePropertiesOKButtonSignal;
	OBSData oldSettings;
	PLSPropertiesView *view;
	PLSDialogButtonBox *buttonBox;
	QSplitter *windowSplitter;
	PLSLoadingEvent *m_pLoadingEvent = nullptr;
	QWidget *m_pWidgetLoadingBG = nullptr;

	OBSSource sourceA;
	OBSSource sourceB;
	OBSSource sourceClone;
	bool direction = true;
	unsigned operationFlags;
	bool isPrivSetting = false;
	bool autoRenameTitle = true;
	bool questionHasCancelButton = true;
	obs_properties_t *(*obsSourceProperties)(const obs_source_t *source);
	void (*obsSourceUpdate)(obs_source_t *source, obs_data_t *settings);
	obs_data_t *(*obsSourceGetSettings)(const obs_source_t *source);
	void (*obsSourceReset)(obs_source_t *source);
	// camera properties
	QSize originalCameraResolution;
	QSize oldCameraResolution;
	QSize currentCameraResolution;

	static void SourceRemoved(void *data, calldata_t *params);
	static void SourceRenamed(void *data, calldata_t *params);
	static void UpdateProperties(void *data, calldata_t *params);
	static void PropertyUpdateNotify(void *data, calldata_t *params);
	static void UpdatePropertiesOkButtonEnable(void *data, calldata_t *params);
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static void DrawTransitionPreview(void *data, uint32_t cx, uint32_t cy);
	void UpdateCallback(void *obj, obs_data_t *settings);
	bool ConfirmQuit();
	int CheckSettings();
	void Cleanup();

	//camera properties operation
	void openCameraProperties(bool defer = false);
	void closeCameraProperties(bool ok /*ok or cancel*/);
	void cameraPropertiesResolutionChanged();

	void ShowLoading();
	void HideLoading();
	void AsyncLoadCameraProperties();
	void dialogClosedToSendNoti();

private slots:
	void on_buttonBox_clicked(QAbstractButton *button);
	void onReloadOldSettings();

public:
	static void SetOwnerWindow(OBSSource source, long long hwnd);

	explicit PLSBasicProperties(QWidget *parent, OBSSource source_, unsigned flags = OPERATION_NONE, bool isPrivSetting = false, bool autoRenameTitle = true,
				    PLSDpiHelper dpiHelper = PLSDpiHelper());
	~PLSBasicProperties();

	void Init();
	OBSSource GetSource();
	void OnButtonBoxCancelClicked(OBSSource source);
	void ReloadProperties();
	void AddPreviewButton(QWidget *widget);
	void UpdateOldSettings(obs_source_t *source);
	bool isFirstAddSource() const;
	void closeNoCancelButton();

protected:
	virtual void showEvent(QShowEvent *) override;
	virtual void closeEvent(QCloseEvent *event) override;
	virtual bool eventFilter(QObject *watcher, QEvent *event) override;
	virtual void reject() override;
	virtual void accept() override;

signals:
	void OpenMusicButtonClicked();

	void OpenFilters(OBSSource source);
	void OpenStickers(OBSSource source);
	void AboutToClose();
};
