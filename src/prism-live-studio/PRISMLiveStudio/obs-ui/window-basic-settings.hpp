/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>
                          Philippe Groarke <philippe.groarke@gmail.com>

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

#include <util/util.hpp>
#include <QDialog>
#include <QPointer>
#include <memory>
#include <string>

#include <obs.hpp>

#include "auth-base.hpp"

#include <PLSDialogView.h>
#include "ResolutionGuidePage.h"
#include "ffmpeg-utils.hpp"
#include "frontend-api.h"

class OBSBasic;
class QAbstractButton;
class QRadioButton;
class QComboBox;
class QCheckBox;
class QLabel;
class QButtonGroup;
class OBSPropertiesView;
class OBSHotkeyWidget;
class PLSPropertiesView;
class QSpinBox;

#include "ui_OBSBasicSettings.h"

#define VOLUME_METER_DECAY_FAST 23.53
#define VOLUME_METER_DECAY_MEDIUM 11.76
#define VOLUME_METER_DECAY_SLOW 8.57

class SilentUpdateCheckBox : public PLSCheckBox {
	Q_OBJECT

public slots:
	void setCheckedSilently(bool checked)
	{
		bool blocked = blockSignals(true);
		setChecked(checked);
		blockSignals(blocked);
	}
};

class SilentUpdateSpinBox : public QSpinBox {
	Q_OBJECT

public slots:
	void setValueSilently(int val)
	{
		bool blocked = blockSignals(true);
		setValue(val);
		blockSignals(blocked);
	}
};

std::string DeserializeConfigText(const char *value);

class OBSBasicSettings : public PLSDialogView {
	Q_OBJECT
	Q_PROPERTY(QIcon generalIcon READ GetGeneralIcon WRITE SetGeneralIcon DESIGNABLE true)
	Q_PROPERTY(QIcon streamIcon READ GetStreamIcon WRITE SetStreamIcon DESIGNABLE true)
	Q_PROPERTY(QIcon outputIcon READ GetOutputIcon WRITE SetOutputIcon DESIGNABLE true)
	Q_PROPERTY(QIcon audioIcon READ GetAudioIcon WRITE SetAudioIcon DESIGNABLE true)
	Q_PROPERTY(QIcon videoIcon READ GetVideoIcon WRITE SetVideoIcon DESIGNABLE true)
	Q_PROPERTY(QIcon hotkeysIcon READ GetHotkeysIcon WRITE SetHotkeysIcon DESIGNABLE true)
	Q_PROPERTY(QIcon accessibilityIcon READ GetAccessibilityIcon WRITE SetAccessibilityIcon DESIGNABLE true)
	Q_PROPERTY(QIcon advancedIcon READ GetAdvancedIcon WRITE SetAdvancedIcon DESIGNABLE true)

public:
	enum Pages { GENERAL, STREAM, OUTPUT, AUDIO, VIDEO, HOTKEYS, ACCESSIBILITY, ADVANCED, NUM_PAGES };

private:
	OBSBasic *main;

	std::unique_ptr<Ui::OBSBasicSettings> ui;

	std::shared_ptr<Auth> auth;
	bool m_isShowChangeLanguageMsg;
	std::pair<std::string, std::string> m_currentLanguage;

	bool generalChanged = false;
	bool stream1Changed = false;
	bool outputsChanged = false;
	bool audioChanged = false;
	bool videoChanged = false;
	bool hotkeysChanged = false;
	bool a11yChanged = false;
	bool advancedChanged = false;
	int pageIndex = 0;
	bool loading = true;
	bool forceAuthReload = false;
	bool forceUpdateCheck = false;
	int sampleRateIndex = 0;
	int channelIndex = 0;
	bool llBufferingEnabled = false;
	bool hotkeysLoaded = false;
	bool serviceDualOutput = false;
	bool bReloadHotKey = false;
	bool bReloadAudioSources = false;

	int lastSimpleRecQualityIdx = 0;
	int lastServiceIdx = -1;
	int lastIgnoreRecommended = -1;
	int lastChannelSetupIdx = 0;

	static constexpr uint32_t ENCODER_HIDE_FLAGS = (OBS_ENCODER_CAP_DEPRECATED | OBS_ENCODER_CAP_INTERNAL);

	std::vector<FFmpegFormat> formats;

	OBSPropertiesView *streamProperties = nullptr;
	OBSPropertiesView *streamEncoderProps = nullptr;
	OBSPropertiesView *recordEncoderProps = nullptr;

	QPointer<QLabel> advOutRecWarning;
	QPointer<QLabel> simpleOutRecWarning;

	QList<std::tuple<QWidget *, QLabel *>> errorAlertMessages;
	QList<std::tuple<QWidget *, QLabel *>> warningAlertMessages;

	QWidget *outputSettingsAdvCurrentTab = nullptr;

	QString curPreset;
	QString curQSVPreset;
	QString curNVENCPreset;
	QString curAMDPreset;
	QString curAMDAV1Preset;

	QString curAdvStreamEncoder;
	QString curAdvRecordEncoder;

	using AudioSource_t = std::tuple<OBSWeakSource, QPointer<PLSCheckBox>, QPointer<QSpinBox>,
					 QPointer<PLSCheckBox>, QPointer<QSpinBox>>;
	std::vector<AudioSource_t> audioSources;
	std::vector<OBSSignal> audioSourceSignals;
	OBSSignal sourceCreated;
	OBSSignal channelChanged;

	std::vector<std::pair<bool, QPointer<OBSHotkeyWidget>>> hotkeys;

	OBSSignal hotkeyRegistered;
	OBSSignal hotkeyUnregistered;

	uint32_t outputCX = 0;
	uint32_t outputCY = 0;

	QPointer<PLSCheckBox> simpleVodTrack;

	QPointer<PLSCheckBox> vodTrackCheckbox;
	QPointer<QWidget> vodTrackContainer;
	QPointer<QWidget> container;
	QPointer<PLSRadioButton> vodTrack[MAX_AUDIO_MIXES];

	QIcon hotkeyConflictIcon;

	void SaveCombo(QComboBox *widget, const char *section, const char *value);
	void SaveComboData(QComboBox *widget, const char *section, const char *value);
	void SaveCheckBox(PLSCheckBox *widget, const char *section, const char *value, bool invert = false);
	void SaveGroupBox(QGroupBox *widget, const char *section, const char *value);
	void SaveEdit(QLineEdit *widget, const char *section, const char *value);
	void SaveSpinBox(QSpinBox *widget, const char *section, const char *value);
	void SaveText(QPlainTextEdit *widget, const char *section, const char *value);
	void SaveFormat(QComboBox *combo);
	void SaveEncoder(QComboBox *combo, const char *section, const char *value);

	bool ResFPSValid(obs_service_resolution *res_list, size_t res_count, int max_fps);
	void ClosestResFPS(obs_service_resolution *res_list, size_t res_count, int max_fps, int &new_cx, int &new_cy,
			   int &new_fps);

	inline bool Changed() const
	{
		return generalChanged || outputsChanged || stream1Changed || audioChanged || videoChanged ||
		       advancedChanged || hotkeysChanged || a11yChanged;
	}

	inline void EnableApplyButton(bool en) { ui->buttonBox->button(QDialogButtonBox::Apply)->setEnabled(en); }

	inline void ClearChanged()
	{
		generalChanged = false;
		stream1Changed = false;
		outputsChanged = false;
		audioChanged = false;
		videoChanged = false;
		hotkeysChanged = false;
		a11yChanged = false;
		advancedChanged = false;
		EnableApplyButton(false);
	}
	void initOutPutChangedTipUi();

	template<typename Widget, typename WidgetParent, typename... SignalArgs, typename... SlotArgs>
	void HookWidget(Widget *widget, void (WidgetParent::*signal)(SignalArgs...),
			void (OBSBasicSettings::*slot)(SlotArgs...))
	{
		QObject::connect(widget, signal, this, slot);
		widget->setProperty("changed", QVariant(false));
	}

	bool QueryChanges();
	bool QueryAllowedToClose();

	void ResetEncoders(bool streamOnly = false);
	void LoadColorRanges();
	void LoadColorSpaces();
	void LoadColorFormats();
	void LoadFormats();
	void ReloadCodecs(const FFmpegFormat &format);

	void UpdateColorFormatSpaceWarning();

	void LoadGeneralSettings();
	void LoadStream1Settings();
	void LoadOutputSettings();
	void LoadAudioSettings();
	void LoadVideoSettings();
	void LoadHotkeySettings(obs_hotkey_id ignoreKey = OBS_INVALID_HOTKEY_ID);
	void LoadA11ySettings(bool presetChange = false);
	void LoadAppearanceSettings(bool reload = false);
	void LoadAdvancedSettings();
	void LoadSettings(bool changedOnly);
	void LoadSceneDisplayMethodSettings();
	void ResetSceneDisplayMethodSettings();
	void ConnectUiSignals();

	bool IgnoreInvisibleHotkeys(obs_source_t *source, const char *name);

	OBSPropertiesView *CreateEncoderPropertyView(const char *encoder, const char *path, bool changed = false,
						     bool bChzzkKeyframeTip = false);

	/* general */
	void LoadLanguageList();
	void LoadThemeList(bool firstLoad);
	void LoadBranchesList();

	/* stream */
	void InitStreamPage();
	bool IsCustomService() const;
	inline bool IsWHIP() const;
	void LoadServices(bool showAll);
	void OnOAuthStreamKeyConnected();
	void OnAuthConnected();
	QString lastService;
	QString protocol;
	QString lastCustomServer;
	int prevLangIndex;
	bool prevBrowserAccel;

	void ServiceChanged(bool resetFields = false);
	QString FindProtocol();
	void UpdateServerList();
	void UpdateKeyLink();
	void UpdateVodTrackSetting();
	void UpdateServiceRecommendations();
	void UpdateMoreInfoLink();
	void UpdateAdvNetworkGroup();

	bool IsCustomServer();

private slots:
	void UpdateMultitrackVideo();
	void RecreateOutputResolutionWidget();
	bool UpdateResFPSLimits();
	void DisplayEnforceWarning(bool checked);
	void on_show_clicked();
	void on_authPwShow_clicked();
	void on_connectAccount_clicked();
	void on_disconnectAccount_clicked();
	void on_useStreamKey_clicked();
	void on_useAuth_toggled();
	void on_server_currentIndexChanged(int index);

	void on_pushButton_clicked();
	void on_hotkeyFilterReset_clicked();
	void on_hotkeyFilterSearch_textChanged(const QString text);
	void on_hotkeyFilterInput_KeyChanged(obs_key_combination_t combo);

	void OnSceneDisplayMethodIndexChanged(int index) const;

	void setVisibleOfErrorTips(bool visible = true);

private:
	/* output */
	void LoadSimpleOutputSettings();
	void LoadAdvOutputStreamingSettings();
	void LoadAdvOutputStreamingEncoderProperties();
	void LoadAdvOutputRecordingSettings();
	void LoadAdvOutputRecordingEncoderProperties();
	void LoadAdvOutputFFmpegSettings();
	void LoadAdvOutputAudioSettings();
	void SetAdvOutputFFmpegEnablement(FFmpegCodecType encoderType, bool enabled, bool enableEncode = false);

	/* audio */
	void LoadListValues(QComboBox *widget, obs_property_t *prop, int index);
	void LoadAudioDevices();
	void LoadAudioSources();

	/* video */
	void LoadRendererList();
	void ResetDownscales(uint32_t cx, uint32_t cy, bool ignoreAllSignals = false);
	void LoadDownscaleFilters(bool bHorizontal);
	void LoadResolutionLists();
	void LoadVerticalResolutionLists();
	void LoadFPSData();

	/* a11y */
	void UpdateA11yColors();
	void SetDefaultColors();
	void ResetDefaultColors();
	QColor GetColor(uint32_t colorVal, QString label);
	uint32_t preset = 0;
	uint32_t selectRed = 0x35FCEC;
	uint32_t selectGreen = 0x5141C3;
	uint32_t selectBlue = 0xFF9100;
	uint32_t mixerGreen = 0x2E4821;
	uint32_t mixerYellow = 0x275E63;
	uint32_t mixerRed = 0x222255;
	uint32_t mixerGreenActive = 0x4cff4c;
	uint32_t mixerYellowActive = 0x4cffff;
	uint32_t mixerRedActive = 0x4c4cff;

	void SaveGeneralSettings();
	void SaveStream1Settings();
	void SaveOutputSettings();
	void SaveAudioSettings();
	void SaveVideoSettings();
	void SaveVerticalVideoSettings();
	void SaveHotkeySettings();
	void SaveA11ySettings();
	void SaveAppearanceSettings();
	void SaveAdvancedSettings();
	void SaveSettings();
	void SaveSceneDisplayMethodSettings() const;
	void showSaveVideoAlert();
	void ResetSettings();

	void SearchHotkeys(const QString &text, obs_key_combination_t filterCombo);
	void SearchHotkeys(QFormLayout *hotkeysLayout, const QString &text, obs_key_combination_t filterCombo);

	void adjustUi();
	void alignLabels(QWidget *rootWidget);
	void alignVideoPage();
	void alignOutputPageLabels();
	QList<QLabel *> getLabelsFromForm(const QFormLayout *form) const;
	//end add by zzc

	void UpdateSimpleOutStreamDelayEstimate();
	void UpdateAdvOutStreamDelayEstimate();

	void FillSimpleRecordingValues();
	void FillAudioMonitoringDevices();

	void RecalcOutputResPixels(const char *resText);
	void RecalcResPixels(QLabel *label, const char *resText);

	bool AskIfCanCloseSettings();

	void UpdateYouTubeAppDockSettings();
	bool prepareStreamServiceData(QStringList &names) const;

	QIcon generalIcon;
	QIcon appearanceIcon;
	QIcon streamIcon;
	QIcon outputIcon;
	QIcon audioIcon;
	QIcon videoIcon;
	QIcon hotkeysIcon;
	QIcon accessibilityIcon;
	QIcon advancedIcon;

	QIcon GetGeneralIcon() const;
	QIcon GetAppearanceIcon() const;
	QIcon GetStreamIcon() const;
	QIcon GetOutputIcon() const;
	QIcon GetAudioIcon() const;
	QIcon GetVideoIcon() const;
	QIcon GetHotkeysIcon() const;
	QIcon GetAccessibilityIcon() const;
	QIcon GetAdvancedIcon() const;

	int CurrentFLVTrack();
	int SimpleOutGetSelectedAudioTracks();
	int AdvOutGetSelectedAudioTracks();
	int AdvOutGetStreamingSelectedAudioTracks();

	OBSService GetStream1Service();

	bool ServiceAndVCodecCompatible();
	bool ServiceAndACodecCompatible();
	bool ServiceSupportsCodecCheck();

	inline bool AllowsMultiTrack(const char *protocol);
	void SwapMultiTrack(const char *protocol);

	ResolutionGuidePage::CannotTipObject mCannotTip;

private slots:

	void on_listWidget_itemSelectionChanged();
	void on_buttonBox_clicked(QAbstractButton *button);

	void on_service_currentIndexChanged(int idx);
	void on_customServer_textChanged(const QString &text);
	void on_simpleOutputBrowse_clicked();
	void on_advOutRecPathBrowse_clicked();
	void on_advOutFFPathBrowse_clicked();
	void on_advOutEncoder_currentIndexChanged();
	void on_advOutRecEncoder_currentIndexChanged(int idx);
	void on_advOutFFIgnoreCompat_stateChanged(int state);
	void on_advOutFFFormat_currentIndexChanged(int idx);
	void on_advOutFFAEncoder_currentIndexChanged(int idx);
	void on_advOutFFVEncoder_currentIndexChanged(int idx);
	void on_advOutFFType_currentIndexChanged(int idx);

	void on_colorFormat_currentIndexChanged(int idx);
	void on_colorSpace_currentIndexChanged(int idx);

	void on_filenameFormatting_textEdited(const QString &text);
	void on_outputResolution_editTextChanged(const QString &text);
	void on_baseResolution_editTextChanged(const QString &text);

	void on_disableOSXVSync_clicked();

	void on_resetButton_clicked();

	void on_choose1_clicked();
	void on_choose2_clicked();
	void on_choose3_clicked();
	void on_choose4_clicked();
	void on_choose5_clicked();
	void on_choose6_clicked();
	void on_choose7_clicked();
	void on_choose8_clicked();
	void on_choose9_clicked();
	void on_colorPreset_currentIndexChanged(int idx);

	void GeneralChanged();
	void HideOBSWindowWarning(int state);
	void AudioChanged();
	void AudioChangedRestart();
	void ReloadAudioSources();
	void SurroundWarning(int idx);
	void SpeakerLayoutChanged(int idx);
	void LowLatencyBufferingChanged(bool checked);
	void UpdateAudioWarnings();
	void OutputsChanged();
	void Stream1Changed();
	void VideoChanged();
	void VideoChangedResolution();
	void HotkeysChanged();
	bool ScanDuplicateHotkeys(QFormLayout *layout);
	void ReloadHotkeys(obs_hotkey_id ignoreKey = OBS_INVALID_HOTKEY_ID);
	void A11yChanged();
	void AdvancedChanged();
	void AdvancedChangedRestart();

	void UpdateStreamDelayEstimate();

	void UpdateAutomaticReplayBufferCheckboxes();

	void AdvOutSplitFileChanged();
	void AdvOutRecCheckWarnings();
	void AdvOutRecCheckCodecs();

	void SimpleRecordingQualityChanged();
	void SimpleRecordingEncoderChanged();
	void SimpleRecordingQualityLosslessWarning(int idx);

	void SimpleReplayBufferChanged();
	void AdvReplayBufferChanged();

	void SimpleStreamingEncoderChanged();

	OBSService SpawnTempService();

	void SetGeneralIcon(const QIcon &icon);
	void SetStreamIcon(const QIcon &icon);
	void SetOutputIcon(const QIcon &icon);
	void SetAudioIcon(const QIcon &icon);
	void SetVideoIcon(const QIcon &icon);
	void SetHotkeysIcon(const QIcon &icon);
	void SetAccessibilityIcon(const QIcon &icon);
	void SetAdvancedIcon(const QIcon &icon);

	void UseStreamKeyAdvClicked();
	void updateOutPutRelatedUI();
	void updateButtonsState();
	void checkOutputTipsVisible();
	void updateOutputTipsUI();
	void setVisibleOfOutputTips(bool visible = true);
	void onOutputTipsVisibilityChanged(bool visible);
	void reloadOutputRelatedSettings();

	void SimpleStreamAudioEncoderChanged();
	void AdvAudioEncodersChanged();
	void AdvOutStreamEncoderCheckWarnings();
	void SimpleStreamEncoderCheckWarnings();
	void calculateErrorMsgSize();

	void showNormalSetting();
	void showDualoutputSetting();

protected:
	virtual void closeEvent(QCloseEvent *event) override;
	void reject() override;
	bool eventFilter(QObject *watched, QEvent *event) override;
	void resizeEvent(QResizeEvent *e) override;

public:
	OBSBasicSettings(QWidget *parent);
	~OBSBasicSettings();
	void switchToDualOutputMode(const QString &tab, const QString &group) const;
	void cancel();

	enum class AlertMessageType { Error, Warning };
	void updateAlertMessage(AlertMessageType type, QWidget *widget, const QString &message, int order = -1);
	void clearAlertMessage(AlertMessageType type, const QWidget *widget, bool update = true);
	void updateAlertMessage();
	QWidget *getPageOfSender(QObject *sender = nullptr) const;

	inline const QIcon &GetHotkeyConflictIcon() const { return hotkeyConflictIcon; }

signals:
	void updateStreamEncoderPropsSize(PLSPropertiesView *view);
	void sigSaveVideoFailed();
};

class BlockLayoutEnable {

public:
	BlockLayoutEnable(QLayout *pLayout)
	{

		if (pLayout) {
			m_pLayout = pLayout;
			pLayout->setEnabled(false);
		}
	}
	~BlockLayoutEnable()
	{
		if (m_pLayout) {
			m_pLayout->setEnabled(true);
		}
	}

private:
	QLayout *m_pLayout = nullptr;
};
