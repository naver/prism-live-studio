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

#include <obs.hpp>
#include <util/util.hpp>
#include <util/lexer.h>
#include <graphics/math-defs.h>
#include <initializer_list>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <QCompleter>
#include <QGuiApplication>
#include <QLineEdit>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDirIterator>
#include <QVariant>
#include <QTreeView>
#include <QScreen>
#include <QStandardItemModel>
#include <QSpacerItem>
#include <QColor>
#include <qt-wrappers.hpp>

#include "audio-encoders.hpp"
#include "hotkey-edit.hpp"
#include "source-label.hpp"
#include "obs-app.hpp"
#include "pls/pls-dual-output.h"
#include "platform.hpp"
#include "properties-view.hpp"
#include "window-basic-main.hpp"
#include "moc_window-basic-settings.cpp"
#include "window-basic-main-outputs.hpp"
#include "window-projector.hpp"

#ifdef YOUTUBE_ENABLED
#include "youtube-api-wrappers.hpp"
#endif

#include <util/platform.h>
#include <util/dstr.hpp>
#include "ui-config.h"

#include "PLSBasic.h"
#include "PLSPropertiesView.hpp"
#include "PLSCompleter.hpp"
#include <utils-api.h>
#include "PLSPlatformApi.h"
#include "login-user-info.hpp"
#include "pls-performance.h"
#include "PLSLoadingView.h"
#include "PLSWatchers.h"

static bool ValidResolutions(OBSBasicSettings *settings, PLSEditableComboBox *baseResolution,
			     PLSEditableComboBox *outputResolution);

using namespace std;

class SettingsEventFilter : public QObject {
	QScopedPointer<OBSEventFilter> shortcutFilter;

public:
	inline SettingsEventFilter() : shortcutFilter((OBSEventFilter *)CreateShortcutFilter(this)) {}

protected:
	bool eventFilter(QObject *obj, QEvent *event) override
	{
		int key;

		switch (event->type()) {
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
			key = static_cast<QKeyEvent *>(event)->key();
			if (key == Qt::Key_Escape) {
				return false;
			}
		default:
			break;
		}

		return shortcutFilter->filter(obj, event);
	}
};

static inline bool ResTooHigh(uint32_t cx, uint32_t cy)
{
	return cx > 16384 || cy > 16384;
}

static inline bool ResTooLow(uint32_t cx, uint32_t cy)
{
	return cx < 32 || cy < 32;
}

/* parses "[width]x[height]", string, i.e. 1024x768 */
static bool ConvertResText(const char *res, uint32_t &cx, uint32_t &cy)
{
	BaseLexer lex;
	base_token token;

	lexer_start(lex, res);

	/* parse width */
	if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
		return false;
	if (token.type != BASETOKEN_DIGIT)
		return false;

	cx = std::stoul(token.text.array);

	/* parse 'x' */
	if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
		return false;
	if (strref_cmpi(&token.text, "x") != 0)
		return false;

	/* parse height */
	if (!lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
		return false;
	if (token.type != BASETOKEN_DIGIT)
		return false;

	cy = std::stoul(token.text.array);

	/* shouldn't be any more tokens after this */
	if (lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
		return false;

	if (ResTooHigh(cx, cy) || ResTooLow(cx, cy)) {
		cx = cy = 0;
		return false;
	}

	return true;
}

static inline bool WidgetChanged(QWidget *widget)
{
	return widget->property("changed").toBool();
}

static inline void SetComboByName(QComboBox *combo, const char *name)
{
	int idx = combo->findText(QT_UTF8(name));
	if (idx != -1)
		combo->setCurrentIndex(idx);
}

static inline bool SetComboByValue(QComboBox *combo, const char *name)
{
	int idx = combo->findData(QT_UTF8(name));
	if (idx != -1) {
		combo->setCurrentIndex(idx);
		return true;
	}

	return false;
}

static inline bool SetInvalidValue(QComboBox *combo, const char *name, const char *data = nullptr)
{
	combo->insertItem(0, name, data);

	QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(combo->model());
	if (!model)
		return false;

	QStandardItem *item = model->item(0);
	item->setFlags(Qt::NoItemFlags);

	combo->setCurrentIndex(0);
	return true;
}

static inline QString GetComboData(QComboBox *combo)
{
	int idx = combo->currentIndex();
	if (idx == -1)
		return QString();

	return combo->itemData(idx).toString();
}

static int FindEncoder(QComboBox *combo, const char *name, int id)
{
	FFmpegCodec codec{name, id};

	for (int i = 0; i < combo->count(); i++) {
		QVariant v = combo->itemData(i);
		if (!v.isNull()) {
			if (codec == v.value<FFmpegCodec>()) {
				return i;
			}
		}
	}
	return -1;
}

#define INVALID_BITRATE 10000
static int FindClosestAvailableAudioBitrate(QComboBox *box, int bitrate)
{
	QList<int> bitrates;
	int prev = 0;
	int next = INVALID_BITRATE;

	for (int i = 0; i < box->count(); i++)
		bitrates << box->itemText(i).toInt();

	for (int val : bitrates) {
		if (next > val) {
			if (val == bitrate)
				return bitrate;

			if (val < next && val > bitrate)
				next = val;
			if (val > prev && val < bitrate)
				prev = val;
		}
	}

	if (next != INVALID_BITRATE)
		return next;
	if (prev != 0)
		return prev;
	return 192;
}
#undef INVALID_BITRATE

static void PopulateSimpleBitrates(QComboBox *box, bool opus)
{
	auto &bitrateMap = opus ? GetSimpleOpusEncoderBitrateMap() : GetSimpleAACEncoderBitrateMap();
	if (bitrateMap.empty())
		return;

	vector<pair<QString, QString>> pairs;
	for (auto &entry : bitrateMap)
		pairs.emplace_back(QString::number(entry.first), obs_encoder_get_display_name(entry.second.c_str()));

	QString currentBitrate = box->currentText();
	box->clear();

	for (auto &pair : pairs) {
		box->addItem(pair.first);
		box->setItemData(box->count() - 1, pair.second, Qt::ToolTipRole);
	}

	if (box->findData(currentBitrate) == -1) {
		int bitrate = FindClosestAvailableAudioBitrate(box, currentBitrate.toInt());
		box->setCurrentText(QString::number(bitrate));
	} else
		box->setCurrentText(currentBitrate);
}

static void PopulateAdvancedBitrates(initializer_list<QComboBox *> boxes, const char *stream_id, const char *rec_id)
{
	auto &streamBitrates = GetAudioEncoderBitrates(stream_id);
	auto &recBitrates = GetAudioEncoderBitrates(rec_id);
	if (streamBitrates.empty() || recBitrates.empty())
		return;

	QList<int> streamBitratesList;
	for (auto &bitrate : streamBitrates)
		streamBitratesList << bitrate;

	for (auto box : boxes) {
		QString currentBitrate = box->currentText();
		box->clear();

		for (auto &bitrate : recBitrates) {
			if (streamBitratesList.indexOf(bitrate) == -1)
				continue;

			box->addItem(QString::number(bitrate));
		}

		if (box->findData(currentBitrate) == -1) {
			int bitrate = FindClosestAvailableAudioBitrate(box, currentBitrate.toInt());
			box->setCurrentText(QString::number(bitrate));
		} else
			box->setCurrentText(currentBitrate);
	}
}

static std::tuple<int, int> aspect_ratio(int cx, int cy)
{
	int common = std::gcd(cx, cy);
	int newCX = cx / common;
	int newCY = cy / common;

	if (newCX == 8 && newCY == 5) {
		newCX = 16;
		newCY = 10;
	}

	return std::make_tuple(newCX, newCY);
}

static inline void HighlightGroupBoxLabel(QGroupBox *gb, QWidget *widget, QString objectName)
{
	QFormLayout *layout = qobject_cast<QFormLayout *>(gb->layout());

	if (!layout)
		return;

	QLabel *label = qobject_cast<QLabel *>(layout->labelForField(widget));

	if (label) {
		label->setObjectName(objectName);

		label->style()->unpolish(label);
		label->style()->polish(label);
	}
}

static void layoutRemoveWidget(QFormLayout *layout, QWidget *widget)
{
	if (widget) {
		layout->removeWidget(widget);
		widget->hide();
	}
}

void RestrictResetBitrates(initializer_list<QComboBox *> boxes, int maxbitrate);

/* clang-format off */
#define COMBO_CHANGED   &QComboBox::currentIndexChanged
#define EDIT_CHANGED    &QLineEdit::textChanged
#define CBEDIT_CHANGED  &QComboBox::editTextChanged
#define CHECK_CHANGED   &PLSCheckBox::clicked
#define RADIO_CHANGED   &PLSRadioButton::toggled
#define GROUP_CHANGED   &QGroupBox::toggled
#define SCROLL_CHANGED  &PLSSpinBox::valueChanged
#define DSCROLL_CHANGED &PLSDoubleSpinBox::valueChanged
#define TEXT_CHANGED    &QPlainTextEdit::textChanged

#define GENERAL_CHANGED &OBSBasicSettings::GeneralChanged
#define STREAM1_CHANGED &OBSBasicSettings::Stream1Changed
#define OUTPUTS_CHANGED &OBSBasicSettings::OutputsChanged
#define AUDIO_RESTART   &OBSBasicSettings::AudioChangedRestart
#define AUDIO_CHANGED   &OBSBasicSettings::AudioChanged
#define VIDEO_RES       &OBSBasicSettings::VideoChangedResolution
#define VIDEO_CHANGED   &OBSBasicSettings::VideoChanged
#define A11Y_CHANGED    &OBSBasicSettings::A11yChanged
#define ADV_CHANGED     &OBSBasicSettings::AdvancedChanged
#define ADV_RESTART     &OBSBasicSettings::AdvancedChangedRestart
/* clang-format on */

OBSBasicSettings::OBSBasicSettings(QWidget *parent)
	: PLSDialogView(parent),
	  main(qobject_cast<OBSBasic *>(parent)),
	  ui(new Ui::OBSBasicSettings)
{
	PLS_PERFORMANCE_FUNCTION();
	PLS_DISABLE_UISTEP_V2(this);
	string path;

	EnableThreadedMessageBoxes(true);

	PLS_PERFORMANCE_START(OBSBasicSettings_setupUi);
	setupUi(ui);
	PLS_PERFORMANCE_END(OBSBasicSettings_setupUi);
	//PRISM/sonic.yang/20260421/PRISM_PC-5879/show loading overlay before stack page switch (avoid empty page flash)
	disconnect(ui->listWidget, &QListWidget::currentRowChanged, ui->settingsPages,
		   &QStackedWidget::setCurrentIndex);
	pls_add_css(this, {"OBSBasicSettings", "PrismPasswordView"});
#if defined(Q_OS_MACOS)
	initSize({940, 760 - PLS_TITLE_BAR_HEIGHT});
#else
	initSize({940, 760});
#endif
	setHasMaxResButton(false);
	setHasMinButton(false);
	main->EnableOutputs(false);

	ui->listWidget->setAttribute(Qt::WA_MacShowFocusRect, false);

#undef ADD_HOTKEY_FOCUS_TYPE

	connect(this, &OBSBasicSettings::updateStreamEncoderPropsSize, this,
		[this](const PLSPropertiesView *) { alignOutputPageLabels(); });

	//Apply button disabled until change.
	EnableApplyButton(false);

	installEventFilter(new SettingsEventFilter());

	auto ReloadAudioSources = [](void *data, calldata_t *param) {
		auto settings = static_cast<OBSBasicSettings *>(data);
		auto source = static_cast<obs_source_t *>(calldata_ptr(param, "source"));

		if (!source)
			return;

		if (!(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO))
			return;

		QMetaObject::invokeMethod(settings, "ReloadAudioSources", Qt::QueuedConnection);
	};
	sourceCreated.Connect(obs_get_signal_handler(), "source_create", ReloadAudioSources, this);
	channelChanged.Connect(obs_get_signal_handler(), "channel_change", ReloadAudioSources, this);

	hotkeyConflictIcon = QIcon::fromTheme("obs", QIcon(":/res/images/warning.svg"));

	auto ReloadHotkeys = [](void *data, calldata_t *) {
		auto settings = static_cast<OBSBasicSettings *>(data);
		QMetaObject::invokeMethod(settings, "ReloadHotkeys", Qt::QueuedConnection);
	};
	hotkeyRegistered.Connect(obs_get_signal_handler(), "hotkey_register", ReloadHotkeys, this);

	auto ReloadHotkeysIgnore = [](void *data, calldata_t *param) {
		auto settings = static_cast<OBSBasicSettings *>(data);
		auto key = static_cast<obs_hotkey_t *>(calldata_ptr(param, "key"));
		QMetaObject::invokeMethod(settings, "ReloadHotkeys", Qt::QueuedConnection,
					  Q_ARG(obs_hotkey_id, obs_hotkey_get_id(key)));
	};
	hotkeyUnregistered.Connect(obs_get_signal_handler(), "hotkey_unregister", ReloadHotkeysIgnore, this);

	PLS_PERFORMANCE_START(OBSBasicSettings_LoadSettings);
	LoadSettings(false);
	PLS_PERFORMANCE_END(OBSBasicSettings_LoadSettings);

	App()->DisableHotkeys();

	PLS_PERFORMANCE_START(OBSBasicSettings_InitOutputTip);
	initOutPutChangedTipUi();
	PLS_PERFORMANCE_END(OBSBasicSettings_InitOutputTip);
	ui->alertMessageFrame->hide();
#ifdef Q_OS_MACOS
	for (QScrollArea *_area : this->findChildren<QScrollArea *>()) {
		pls_scroll_area_clips_to_bounds(_area);
	}
#endif

	pls_uistep_v2_set_title(this, QStringLiteral("Settings"));
	pls_uistep_v2_set_title(ui->accessPage, QStringLiteral("Settings - Accessibility"), true);
	pls_uistep_v2_set_title(ui->advancedPage, QStringLiteral("Settings - Advanced"), true);
	pls_uistep_v2_set_title(ui->audioPage, QStringLiteral("Settings - Audio"), true);
	pls_uistep_v2_set_title(ui->generalPage, QStringLiteral("Settings - General"), true);
	pls_uistep_v2_set_title(ui->hotkeyPage, QStringLiteral("Settings - HotKeys"), true);
	pls_uistep_v2_set_title(ui->outputPage, QStringLiteral("Settings - Output"), true);
	pls_uistep_v2_set_title(ui->videoPage, QStringLiteral("Settings - Video"), true);
	pls_uistep_v2_auto_bind(this);
}

OBSBasicSettings::~OBSBasicSettings()
{
	pls_check_app_exiting();
	PLSLoadingView::deleteLoadingView(m_settingsPageLoadingView);
	if (advancedPage) {
		delete advancedPage->filenameFormatting->completer();
	}
	main->EnableOutputs(true);

	App()->UpdateHotkeyFocusSetting();

	EnableThreadedMessageBoxes(false);
}

void OBSBasicSettings::switchToDualOutputMode(const QString &tab, const QString &group) const
{
	PLS_PERFORMANCE_FUNCTION();
	if (tab == QStringLiteral("General")) {
		ui->listWidget->setCurrentRow(Pages::GENERAL);
	} else if (tab == QStringLiteral("Output")) {
		ui->listWidget->setCurrentRow(Pages::OUTPUT);
		if (group == common::AUDIO_MIXER_DUAL_OUTPUT_ADVANCE_PAGE) {
			pls_async_call(this, [this]() {
				if (outputPage) {
					outputPage->outputMode->setCurrentIndex(1);
				}
			});
		}
	} else if (QStringLiteral("Video") == tab) {
		ui->listWidget->setCurrentRow(Pages::VIDEO);
	} else {
		ui->listWidget->setCurrentRow(Pages::GENERAL);
	}
}
void OBSBasicSettings::cancel()
{
	setParent(nullptr);
	ClearChanged();
	close();
}
void OBSBasicSettings::SaveCombo(QComboBox *widget, const char *section, const char *value)
{
	if (WidgetChanged(widget))
		config_set_string(main->Config(), section, value, QT_TO_UTF8(widget->currentText()));
}

void OBSBasicSettings::SaveComboData(QComboBox *widget, const char *section, const char *value)
{
	if (WidgetChanged(widget)) {
		QString str = GetComboData(widget);
		config_set_string(main->Config(), section, value, QT_TO_UTF8(str));
	}
}

void OBSBasicSettings::SaveCheckBox(PLSCheckBox *widget, const char *section, const char *value, bool invert)
{
	if (WidgetChanged(widget)) {
		bool checked = widget->isChecked();
		if (invert)
			checked = !checked;

		config_set_bool(main->Config(), section, value, checked);
	}
}

void OBSBasicSettings::SaveEdit(QLineEdit *widget, const char *section, const char *value)
{
	if (WidgetChanged(widget))
		config_set_string(main->Config(), section, value, QT_TO_UTF8(widget->text()));
}

void OBSBasicSettings::SaveSpinBox(QSpinBox *widget, const char *section, const char *value)
{
	if (WidgetChanged(widget))
		config_set_int(main->Config(), section, value, widget->value());
}

void OBSBasicSettings::SaveText(QPlainTextEdit *widget, const char *section, const char *value)
{
	if (!WidgetChanged(widget))
		return;

	auto utf8 = widget->toPlainText().toUtf8();

	OBSDataAutoRelease safe_text = obs_data_create();
	obs_data_set_string(safe_text, "text", utf8.constData());

	config_set_string(main->Config(), section, value, obs_data_get_json(safe_text));
}

std::string DeserializeConfigText(const char *value)
{
	OBSDataAutoRelease data = obs_data_create_from_json(value);
	return obs_data_get_string(data, "text");
}

void OBSBasicSettings::SaveGroupBox(QGroupBox *widget, const char *section, const char *value)
{
	if (WidgetChanged(widget))
		config_set_bool(main->Config(), section, value, widget->isChecked());
}

#define CS_PARTIAL_STR QTStr("Basic.Settings.Advanced.Video.ColorRange.Partial")
#define CS_FULL_STR QTStr("Basic.Settings.Advanced.Video.ColorRange.Full")

void OBSBasicSettings::LoadColorRanges()
{
	advancedPage->colorRange->addItem(CS_PARTIAL_STR, "Partial");
	advancedPage->colorRange->addItem(CS_FULL_STR, "Full");
}

#define CS_SRGB_STR QTStr("Basic.Settings.Advanced.Video.ColorSpace.sRGB")
#define CS_709_STR QTStr("Basic.Settings.Advanced.Video.ColorSpace.709")
#define CS_601_STR QTStr("Basic.Settings.Advanced.Video.ColorSpace.601")
#define CS_2100PQ_STR QTStr("Basic.Settings.Advanced.Video.ColorSpace.2100PQ")
#define CS_2100HLG_STR QTStr("Basic.Settings.Advanced.Video.ColorSpace.2100HLG")

void OBSBasicSettings::LoadColorSpaces()
{
	advancedPage->colorSpace->addItem(CS_SRGB_STR, "sRGB");
	advancedPage->colorSpace->addItem(CS_709_STR, "709");
	advancedPage->colorSpace->addItem(CS_601_STR, "601");
	advancedPage->colorSpace->addItem(CS_2100PQ_STR, "2100PQ");
	advancedPage->colorSpace->addItem(CS_2100HLG_STR, "2100HLG");
}

#define CF_NV12_STR QTStr("Basic.Settings.Advanced.Video.ColorFormat.NV12")
#define CF_I420_STR QTStr("Basic.Settings.Advanced.Video.ColorFormat.I420")
#define CF_I444_STR QTStr("Basic.Settings.Advanced.Video.ColorFormat.I444")
#define CF_P010_STR QTStr("Basic.Settings.Advanced.Video.ColorFormat.P010")
#define CF_I010_STR QTStr("Basic.Settings.Advanced.Video.ColorFormat.I010")
#define CF_P216_STR QTStr("Basic.Settings.Advanced.Video.ColorFormat.P216")
#define CF_P416_STR QTStr("Basic.Settings.Advanced.Video.ColorFormat.P416")
#define CF_BGRA_STR QTStr("Basic.Settings.Advanced.Video.ColorFormat.BGRA")

void OBSBasicSettings::LoadColorFormats()
{
	advancedPage->colorFormat->addItem(CF_NV12_STR, "NV12");
	advancedPage->colorFormat->addItem(CF_I420_STR, "I420");
	advancedPage->colorFormat->addItem(CF_I444_STR, "I444");
	advancedPage->colorFormat->addItem(CF_P010_STR, "P010");
	advancedPage->colorFormat->addItem(CF_I010_STR, "I010");
	advancedPage->colorFormat->addItem(CF_P216_STR, "P216");
	advancedPage->colorFormat->addItem(CF_P416_STR, "P416");
	advancedPage->colorFormat->addItem(CF_BGRA_STR, "RGB"); // Avoid config break
}

#define AV_FORMAT_DEFAULT_STR QTStr("Basic.Settings.Output.Adv.FFmpeg.FormatDefault")
#define AUDIO_STR QTStr("Basic.Settings.Output.Adv.FFmpeg.FormatAudio")
#define VIDEO_STR QTStr("Basic.Settings.Output.Adv.FFmpeg.FormatVideo")

void OBSBasicSettings::LoadSimpleFormats()
{
#define FORMAT_STR(str) QTStr("Basic.Settings.Output.Format." str)

	outputSimplePage->simpleOutRecFormat->addItem(FORMAT_STR("FLV"), "flv");
	outputSimplePage->simpleOutRecFormat->addItem(FORMAT_STR("MKV"), "mkv");
	outputSimplePage->simpleOutRecFormat->addItem(FORMAT_STR("MP4"), "mp4");
	outputSimplePage->simpleOutRecFormat->addItem(FORMAT_STR("MOV"), "mov");
	outputSimplePage->simpleOutRecFormat->addItem(FORMAT_STR("hMP4"), "hybrid_mp4");
	outputSimplePage->simpleOutRecFormat->addItem(FORMAT_STR("fMP4"), "fragmented_mp4");
	outputSimplePage->simpleOutRecFormat->addItem(FORMAT_STR("fMOV"), "fragmented_mov");
	outputSimplePage->simpleOutRecFormat->addItem(FORMAT_STR("TS"), "mpegts");

#undef FORMAT_STR
}

void OBSBasicSettings::LoadRecordFormats()
{
#define FORMAT_STR(str) QTStr("Basic.Settings.Output.Format." str)

	outputRecordPage->advOutFFFormat->blockSignals(true);

	formats = GetSupportedFormats();
	for (auto &format : formats) {
		bool audio = format.HasAudio();
		bool video = format.HasVideo();

		if (audio || video) {
			QString itemText(format.name);
			if (audio ^ video)
				itemText += QString(" (%1)").arg(audio ? AUDIO_STR : VIDEO_STR);

			outputRecordPage->advOutFFFormat->addItem(itemText, QVariant::fromValue(format));
		}
	}
	outputRecordPage->advOutFFFormat->model()->sort(0);
	outputRecordPage->advOutFFFormat->insertItem(0, AV_FORMAT_DEFAULT_STR);

	outputRecordPage->advOutFFFormat->blockSignals(false);

	outputRecordPage->advOutRecFormat->addItem(FORMAT_STR("FLV"), "flv");
	outputRecordPage->advOutRecFormat->addItem(FORMAT_STR("MKV"), "mkv");
	outputRecordPage->advOutRecFormat->addItem(FORMAT_STR("MP4"), "mp4");
	outputRecordPage->advOutRecFormat->addItem(FORMAT_STR("MOV"), "mov");
	outputRecordPage->advOutRecFormat->addItem(FORMAT_STR("hMP4"), "hybrid_mp4");
	outputRecordPage->advOutRecFormat->addItem(FORMAT_STR("fMP4"), "fragmented_mp4");
	outputRecordPage->advOutRecFormat->addItem(FORMAT_STR("fMOV"), "fragmented_mov");
	outputRecordPage->advOutRecFormat->addItem(FORMAT_STR("TS"), "mpegts");
	outputRecordPage->advOutRecFormat->addItem(FORMAT_STR("HLS"), "hls");

#undef FORMAT_STR
}

static void AddCodec(QComboBox *combo, const FFmpegCodec &codec)
{
	QString itemText;
	if (codec.long_name)
		itemText = QString("%1 - %2").arg(codec.name, codec.long_name);
	else
		itemText = codec.name;

	combo->addItem(itemText, QVariant::fromValue(codec));
}

#define AV_ENCODER_DEFAULT_STR QTStr("Basic.Settings.Output.Adv.FFmpeg.AVEncoderDefault")

static void AddDefaultCodec(QComboBox *combo, const FFmpegFormat &format, FFmpegCodecType codecType)
{
	FFmpegCodec codec = format.GetDefaultEncoder(codecType);

	int existingIdx = FindEncoder(combo, codec.name, codec.id);
	if (existingIdx >= 0)
		combo->removeItem(existingIdx);

	QString itemText;
	if (codec.long_name) {
		itemText = QString("%1 - %2 (%3)").arg(codec.name, codec.long_name, AV_ENCODER_DEFAULT_STR);
	} else {
		itemText = QString("%1 (%2)").arg(codec.name, AV_ENCODER_DEFAULT_STR);
	}

	combo->addItem(itemText, QVariant::fromValue(codec));
}

#define AV_ENCODER_DISABLE_STR QTStr("Basic.Settings.Output.Adv.FFmpeg.AVEncoderDisable")

void OBSBasicSettings::ReloadCodecs(const FFmpegFormat &format)
{
	outputRecordPage->advOutFFAEncoder->blockSignals(true);
	outputRecordPage->advOutFFVEncoder->blockSignals(true);
	outputRecordPage->advOutFFAEncoder->clear();
	outputRecordPage->advOutFFVEncoder->clear();

	bool ignore_compatibility = outputRecordPage->advOutFFIgnoreCompat->isChecked();
	vector<FFmpegCodec> supportedCodecs = GetFormatCodecs(format, ignore_compatibility);

	for (auto &codec : supportedCodecs) {
		switch (codec.type) {
		case FFmpegCodecType::AUDIO:
			AddCodec(outputRecordPage->advOutFFAEncoder, codec);
			break;
		case FFmpegCodecType::VIDEO:
			AddCodec(outputRecordPage->advOutFFVEncoder, codec);
			break;
		default:
			break;
		}
	}

	if (format.HasAudio())
		AddDefaultCodec(outputRecordPage->advOutFFAEncoder, format, FFmpegCodecType::AUDIO);
	if (format.HasVideo())
		AddDefaultCodec(outputRecordPage->advOutFFVEncoder, format, FFmpegCodecType::VIDEO);

	outputRecordPage->advOutFFAEncoder->model()->sort(0);
	outputRecordPage->advOutFFVEncoder->model()->sort(0);

	QVariant disable = QVariant::fromValue(FFmpegCodec());

	outputRecordPage->advOutFFAEncoder->insertItem(0, AV_ENCODER_DISABLE_STR, disable);
	outputRecordPage->advOutFFVEncoder->insertItem(0, AV_ENCODER_DISABLE_STR, disable);

	outputRecordPage->advOutFFAEncoder->blockSignals(false);
	outputRecordPage->advOutFFVEncoder->blockSignals(false);
}

void OBSBasicSettings::LoadLanguageList()
{
	const char *currentLang = App()->GetLocale();

	generalPage->language->clear();

	for (const auto &locale : GetLocaleNames()) {
		int idx = generalPage->language->count();

		generalPage->language->addItem(QT_UTF8(locale.second.c_str()), QT_UTF8(locale.first.c_str()));

		if (locale.first == currentLang) {
			generalPage->language->setCurrentIndex(idx);
			m_currentLanguage.first = locale.first;
			m_currentLanguage.second = locale.second;
		}
	}

	generalPage->language->model()->sort(0);
}

#if defined(_WIN32) || defined(ENABLE_SPARKLE_UPDATER)
void TranslateBranchInfo(const QString &name, QString &displayName, QString &description)
{
	QString translatedName = QTStr("Basic.Settings.General.ChannelName." + name.toUtf8());
	QString translatedDesc = QTStr("Basic.Settings.General.ChannelDescription." + name.toUtf8());

	if (!translatedName.startsWith("Basic.Settings."))
		displayName = translatedName;
	if (!translatedDesc.startsWith("Basic.Settings."))
		description = translatedDesc;
}
#endif

void OBSBasicSettings::LoadGeneralSettings()
{
	if (!generalPage) {
		return;
	}

	loading = true;

	LoadLanguageList();

#if defined(_WIN32)
	if (generalPage->hideOBSFromCapture) {
		bool hideWindowFromCapture =
			config_get_bool(App()->GetUserConfig(), "BasicWindow", "HideOBSWindowsFromCapture");
		generalPage->hideOBSFromCapture->setChecked(hideWindowFromCapture);
		connect(generalPage->hideOBSFromCapture, &PLSCheckBox::stateChanged, this,
			&OBSBasicSettings::HideOBSWindowWarning);
	}
#else
	delete generalPage->hideOBSFromCapture;
	generalPage->hideOBSFromCapture = nullptr;
#endif

	bool bEnableWaterMark = config_get_bool(App()->GetUserConfig(), "General", "Watermark");
	generalPage->watermarkCheckBox->setChecked(bEnableWaterMark);

	bool recordWhenStreaming = config_get_bool(App()->GetUserConfig(), "BasicWindow", "RecordWhenStreaming");
	generalPage->recordWhenStreaming->setChecked(recordWhenStreaming);

	bool keepRecordStreamStops =
		config_get_bool(App()->GetUserConfig(), "BasicWindow", "KeepRecordingWhenStreamStops");
	generalPage->keepRecordStreamStops->setChecked(keepRecordStreamStops);

	bool replayWhileStreaming =
		config_get_bool(App()->GetUserConfig(), "BasicWindow", "ReplayBufferWhileStreaming");
	generalPage->replayWhileStreaming->setChecked(replayWhileStreaming);

	bool keepReplayStreamStops =
		config_get_bool(App()->GetUserConfig(), "BasicWindow", "KeepReplayBufferStreamStops");
	generalPage->keepReplayStreamStops->setChecked(keepReplayStreamStops);

	bool systemTrayEnabled = config_get_bool(App()->GetUserConfig(), "BasicWindow", "SysTrayEnabled");
	generalPage->systemTrayEnabled->setChecked(systemTrayEnabled);

	bool systemTrayWhenStarted = config_get_bool(App()->GetUserConfig(), "BasicWindow", "SysTrayWhenStarted");
	generalPage->systemTrayWhenStarted->setChecked(systemTrayWhenStarted);

	bool systemTrayAlways = config_get_bool(App()->GetUserConfig(), "BasicWindow", "SysTrayMinimizeToTray");
	generalPage->systemTrayAlways->setChecked(systemTrayAlways);

	bool saveProjectors = config_get_bool(App()->GetUserConfig(), "BasicWindow", "SaveProjectors");
	generalPage->saveProjectors->setChecked(saveProjectors);

	bool closeProjectors = config_get_bool(App()->GetUserConfig(), "BasicWindow", "CloseExistingProjectors");
	generalPage->closeProjectors->setChecked(closeProjectors);

	bool snappingEnabled = config_get_bool(App()->GetUserConfig(), "BasicWindow", "SnappingEnabled");
	generalPage->snappingEnabled->setChecked(snappingEnabled);

	bool screenSnapping = config_get_bool(App()->GetUserConfig(), "BasicWindow", "ScreenSnapping");
	generalPage->screenSnapping->setChecked(screenSnapping);

	bool centerSnapping = config_get_bool(App()->GetUserConfig(), "BasicWindow", "CenterSnapping");
	generalPage->centerSnapping->setChecked(centerSnapping);

	bool sourceSnapping = config_get_bool(App()->GetUserConfig(), "BasicWindow", "SourceSnapping");
	generalPage->sourceSnapping->setChecked(sourceSnapping);

	double snapDistance = config_get_double(App()->GetUserConfig(), "BasicWindow", "SnapDistance");
	generalPage->snapDistance->setValue(snapDistance);

	bool spacingHelpersEnabled = config_get_bool(App()->GetUserConfig(), "BasicWindow", "SpacingHelpersEnabled");
	generalPage->previewSpacingHelpers->setChecked(spacingHelpersEnabled);

	bool previewZoomEnabled = config_get_bool(App()->GetUserConfig(), "BasicWindow", "PreviewZoomEnabled");
	generalPage->previewZoomEnabled->setChecked(previewZoomEnabled);

	bool hideProjectorCursor = config_get_bool(App()->GetUserConfig(), "BasicWindow", "HideProjectorCursor");
	generalPage->hideProjectorCursor->setChecked(hideProjectorCursor);

	bool projectorAlwaysOnTop = config_get_bool(App()->GetUserConfig(), "BasicWindow", "ProjectorAlwaysOnTop");
	generalPage->projectorAlwaysOnTop->setChecked(projectorAlwaysOnTop);

	bool overflowHide = config_get_bool(App()->GetUserConfig(), "BasicWindow", "OverflowHidden");
	generalPage->overflowHide->setChecked(overflowHide);

	bool overflowAlwaysVisible = config_get_bool(App()->GetUserConfig(), "BasicWindow", "OverflowAlwaysVisible");
	generalPage->overflowAlwaysVisible->setChecked(overflowAlwaysVisible);

	bool overflowSelectionHide = config_get_bool(App()->GetUserConfig(), "BasicWindow", "OverflowSelectionHidden");
	generalPage->overflowSelectionHide->setChecked(overflowSelectionHide);

	bool safeAreas = config_get_bool(App()->GetUserConfig(), "BasicWindow", "ShowSafeAreas");
	generalPage->previewSafeAreas->setChecked(safeAreas);

	bool doubleClickSwitch = config_get_bool(App()->GetUserConfig(), "BasicWindow", "TransitionOnDoubleClick");
	generalPage->doubleClickSwitch->setChecked(doubleClickSwitch);

	bool studioPortraitLayout = config_get_bool(App()->GetUserConfig(), "BasicWindow", "StudioPortraitLayout");
	generalPage->studioPortraitLayout->setChecked(studioPortraitLayout);

	bool prevProgLabels = config_get_bool(App()->GetUserConfig(), "BasicWindow", "StudioModeLabels");
	generalPage->prevProgLabelToggle->setChecked(prevProgLabels);

	bool multiviewMouseSwitch = config_get_bool(App()->GetUserConfig(), "BasicWindow", "MultiviewMouseSwitch");
	generalPage->multiviewMouseSwitch->setChecked(multiviewMouseSwitch);

	bool multiviewDrawNames = config_get_bool(App()->GetUserConfig(), "BasicWindow", "MultiviewDrawNames");
	generalPage->multiviewDrawNames->setChecked(multiviewDrawNames);

	bool multiviewDrawAreas = config_get_bool(App()->GetUserConfig(), "BasicWindow", "MultiviewDrawAreas");
	generalPage->multiviewDrawAreas->setChecked(multiviewDrawAreas);

	generalPage->multiviewLayout->clear();
	generalPage->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.Horizontal.Top"),
					      static_cast<int>(MultiviewLayout::HORIZONTAL_TOP_8_SCENES));
	generalPage->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.Horizontal.Bottom"),
					      static_cast<int>(MultiviewLayout::HORIZONTAL_BOTTOM_8_SCENES));
	generalPage->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.Vertical.Left"),
					      static_cast<int>(MultiviewLayout::VERTICAL_LEFT_8_SCENES));
	generalPage->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.Vertical.Right"),
					      static_cast<int>(MultiviewLayout::VERTICAL_RIGHT_8_SCENES));
	generalPage->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.Horizontal.18Scene.Top"),
					      static_cast<int>(MultiviewLayout::HORIZONTAL_TOP_18_SCENES));
	generalPage->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.Horizontal.Extended.Top"),
					      static_cast<int>(MultiviewLayout::HORIZONTAL_TOP_24_SCENES));
	generalPage->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.4Scene"),
					      static_cast<int>(MultiviewLayout::SCENES_ONLY_4_SCENES));
	generalPage->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.9Scene"),
					      static_cast<int>(MultiviewLayout::SCENES_ONLY_9_SCENES));
	generalPage->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.16Scene"),
					      static_cast<int>(MultiviewLayout::SCENES_ONLY_16_SCENES));
	generalPage->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.25Scene"),
					      static_cast<int>(MultiviewLayout::SCENES_ONLY_25_SCENES));

	generalPage->multiviewLayout->setCurrentIndex(generalPage->multiviewLayout->findData(
		QVariant::fromValue(config_get_int(App()->GetUserConfig(), "BasicWindow", "MultiviewLayout"))));

	prevLangIndex = generalPage->language->currentIndex();

	if (obs_video_active())
		generalPage->language->setEnabled(false);

	LoadSceneDisplayMethodSettings();

	loading = false;
}

void OBSBasicSettings::LoadRendererList()
{
#ifdef _WIN32
	const char *renderer = config_get_string(App()->GetAppConfig(), "Video", "Renderer");

	advancedPage->renderer->clear();
	advancedPage->renderer->addItem(QT_UTF8("Direct3D 11"));
	if (GlobalVars::opt_allow_opengl || strcmp(renderer, "OpenGL") == 0)
		advancedPage->renderer->addItem(QT_UTF8("OpenGL"));

	int idx = advancedPage->renderer->findText(QT_UTF8(renderer));
	if (idx == -1)
		idx = 0;

	// the video adapter selection is not currently implemented, hide for now
	// to avoid user confusion. was previously protected by
	// if (strcmp(renderer, "OpenGL") == 0)
	delete advancedPage->adapter;
	delete advancedPage->adapterLabel;
	advancedPage->adapter = nullptr;
	advancedPage->adapterLabel = nullptr;

	advancedPage->renderer->setCurrentIndex(idx);
#endif
}

static string ResString(uint32_t cx, uint32_t cy)
{
	stringstream res;
	res << cx << "x" << cy;
	return res.str();
}

/* some nice default output resolution vals */
static const double vals[] = {1.0, 1.25, (1.0 / 0.75), 1.5, (1.0 / 0.6), 1.75, 2.0, 2.25, 2.5, 2.75, 3.0};

static const size_t numVals = sizeof(vals) / sizeof(double);

void OBSBasicSettings::ResetDownscales(uint32_t cx, uint32_t cy, bool bVideoPage, bool bStreamPage, bool bRecordPage,
				       bool ignoreAllSignals)
{
	QString advRescale;
	QString advRecRescale;
	QString advFFRescale;
	QString oldOutputRes;
	string bestScale;
	int bestPixelDiff = 0x7FFFFFFF;
	uint32_t out_cx = outputCX;
	uint32_t out_cy = outputCY;

	if (bStreamPage && outputStreamPage) {
		advRescale = outputStreamPage->advOutRescale->lineEdit()->text();
	}
	if (bRecordPage && outputRecordPage) {
		advRecRescale = outputRecordPage->advOutRecRescale->lineEdit()->text();
		advFFRescale = outputRecordPage->advOutFFRescale->lineEdit()->text();
	}

	bool lockedOutputRes = true;
	if (bVideoPage) {
		lockedOutputRes = !videoPage->outputResolution->isEditable();

		if (!lockedOutputRes) {
			videoPage->outputResolution->blockSignals(true);
			videoPage->outputResolution->clear();
		}
	}

	if (bStreamPage && outputStreamPage) {
		if (ignoreAllSignals) {
			outputStreamPage->advOutRescale->blockSignals(true);
		}
		outputStreamPage->advOutRescale->clear();
	}
	if (bRecordPage && outputRecordPage) {
		if (ignoreAllSignals) {
			outputRecordPage->advOutRecRescale->blockSignals(true);
			outputRecordPage->advOutFFRescale->blockSignals(true);
		}
		outputRecordPage->advOutRecRescale->clear();
		outputRecordPage->advOutFFRescale->clear();
	}

	if (!out_cx || !out_cy) {
		out_cx = cx;
		out_cy = cy;
		if (bVideoPage) {
			oldOutputRes = videoPage->baseResolution->lineEdit()->text();
		}
	} else {
		if (bVideoPage) {
			oldOutputRes = QString::number(out_cx) + "x" + QString::number(out_cy);
		}
	}

	for (size_t idx = 0; idx < numVals; idx++) {
		uint32_t downscaleCX = uint32_t(double(cx) / vals[idx]);
		uint32_t downscaleCY = uint32_t(double(cy) / vals[idx]);
		uint32_t outDownscaleCX = uint32_t(double(out_cx) / vals[idx]);
		uint32_t outDownscaleCY = uint32_t(double(out_cy) / vals[idx]);

		downscaleCX &= 0xFFFFFFFC;
		downscaleCY &= 0xFFFFFFFE;
		outDownscaleCX &= 0xFFFFFFFE;
		outDownscaleCY &= 0xFFFFFFFE;

		string res = ResString(downscaleCX, downscaleCY);
		string outRes = ResString(outDownscaleCX, outDownscaleCY);
		if (bVideoPage && !lockedOutputRes)
			videoPage->outputResolution->addItem(res.c_str());

		if (bStreamPage && outputStreamPage) {
			outputStreamPage->advOutRescale->addItem(outRes.c_str());
		}
		if (bRecordPage && outputRecordPage) {
			outputRecordPage->advOutRecRescale->addItem(outRes.c_str());
			outputRecordPage->advOutFFRescale->addItem(outRes.c_str());
		}

		/* always try to find the closest output resolution to the
		 * previously set output resolution */
		int newPixelCount = int(downscaleCX * downscaleCY);
		int oldPixelCount = int(out_cx * out_cy);
		int diff = abs(newPixelCount - oldPixelCount);

		if (diff < bestPixelDiff) {
			bestScale = res;
			bestPixelDiff = diff;
		}
	}

	string res = ResString(cx, cy);

	if (bVideoPage && !lockedOutputRes) {
		float baseAspect = float(cx) / float(cy);
		float outputAspect = float(out_cx) / float(out_cy);
		bool closeAspect = close_float(baseAspect, outputAspect, 0.01f);

		if (closeAspect) {
			videoPage->outputResolution->lineEdit()->setText(oldOutputRes);
			on_outputResolution_editTextChanged(oldOutputRes);
		} else {
			videoPage->outputResolution->lineEdit()->setText(bestScale.c_str());
			on_outputResolution_editTextChanged(bestScale.c_str());
		}

		videoPage->outputResolution->blockSignals(false);

		if (!closeAspect) {
			videoPage->outputResolution->setProperty("changed", QVariant(true));
			videoChanged = true;
		}
	}

	if (outputStreamPage) {
		if (advRescale.isEmpty())
			advRescale = res.c_str();

		outputStreamPage->advOutRescale->lineEdit()->setText(advRescale);

		if (ignoreAllSignals) {
			outputStreamPage->advOutRescale->blockSignals(false);
		}
	}
	if (outputRecordPage) {
		if (advRecRescale.isEmpty())
			advRecRescale = res.c_str();
		if (advFFRescale.isEmpty())
			advFFRescale = res.c_str();

		outputRecordPage->advOutRecRescale->lineEdit()->setText(advRecRescale);
		outputRecordPage->advOutFFRescale->lineEdit()->setText(advFFRescale);

		if (ignoreAllSignals) {
			outputRecordPage->advOutRecRescale->blockSignals(false);
			outputRecordPage->advOutFFRescale->blockSignals(false);
		}
	}
}

void OBSBasicSettings::ResetVerticalDownscales(uint32_t cx, uint32_t cy)
{
	if (!videoPage->outputResolution_2->isEditable())
		return;

	uint32_t out_cx = config_get_uint(main->Config(), "Video", "OutputCXV");
	uint32_t out_cy = config_get_uint(main->Config(), "Video", "OutputCYV");

	QString oldOutputRes = videoPage->outputResolution_2->lineEdit()->text();
	if (!out_cx || !out_cy) {
		out_cx = cx;
		out_cy = cy;
		if (oldOutputRes.isEmpty())
			oldOutputRes = videoPage->baseResolution_2->lineEdit()->text();
	} else if (oldOutputRes.isEmpty()) {
		oldOutputRes = QString::number(out_cx) + "x" + QString::number(out_cy);
	}

	string bestScaleV;
	int bestPixelDiff = 0x7FFFFFFF;

	videoPage->outputResolution_2->blockSignals(true);
	videoPage->outputResolution_2->clear();

	for (size_t idx = 0; idx < numVals; idx++) {
		uint32_t downscaleCX = uint32_t(double(cx) / vals[idx]);
		uint32_t downscaleCY = uint32_t(double(cy) / vals[idx]);

		downscaleCX &= 0xFFFFFFFC;
		downscaleCY &= 0xFFFFFFFE;

		string res = ResString(downscaleCX, downscaleCY);
		videoPage->outputResolution_2->addItem(res.c_str());

		int newPixelCount = int(downscaleCX * downscaleCY);
		int oldPixelCount = int(out_cx * out_cy);
		int diff = abs(newPixelCount - oldPixelCount);

		if (diff < bestPixelDiff) {
			bestScaleV = res;
			bestPixelDiff = diff;
		}
	}

	float baseAspect = float(cx) / float(cy);
	float outputAspect = float(out_cx) / float(out_cy);
	bool closeAspect = close_float(baseAspect, outputAspect, 0.01f);

	QString textToSet = closeAspect ? oldOutputRes : QString::fromStdString(bestScaleV);
	videoPage->outputResolution_2->lineEdit()->setText(textToSet);
	videoPage->outputResolution_2->blockSignals(false);

	RecalcResPixels(videoPage->scaledAspect_2, QT_TO_UTF8(textToSet));

	if (!closeAspect) {
		videoPage->outputResolution_2->setProperty("changed", QVariant(true));
		videoChanged = true;
	}
}

void OBSBasicSettings::LoadDownscaleFilters(bool bHorizontal)
{
	QSignalBlocker signalBlocker(videoPage->downscaleFilter);

	QString downscaleFilter = videoPage->downscaleFilter->currentData().toString();
	if (downscaleFilter.isEmpty())
		downscaleFilter = config_get_string(main->Config(), "Video", "ScaleType");

	videoPage->downscaleFilter->clear();
	if (bHorizontal ? videoPage->baseResolution->currentText() == videoPage->outputResolution->currentText()
			: videoPage->baseResolution_2->currentText() == videoPage->outputResolution_2->currentText()) {
		videoPage->downscaleFilter->setEnabled(false);
		videoPage->downscaleFilter->addItem(QTStr("Basic.Settings.Video.DownscaleFilter.Unavailable"),
						    downscaleFilter);
	} else {
		videoPage->downscaleFilter->setEnabled(true);
		videoPage->downscaleFilter->addItem(QTStr("Basic.Settings.Video.DownscaleFilter.Bicubic"),
						    QT_UTF8("bicubic"));
		videoPage->downscaleFilter->addItem(QTStr("Basic.Settings.Video.DownscaleFilter.Bilinear"),
						    QT_UTF8("bilinear"));
		videoPage->downscaleFilter->addItem(QTStr("Basic.Settings.Video.DownscaleFilter.Lanczos"),
						    QT_UTF8("lanczos"));
		videoPage->downscaleFilter->addItem(QTStr("Basic.Settings.Video.DownscaleFilter.Area"),
						    QT_UTF8("area"));

		if (downscaleFilter == "bilinear")
			videoPage->downscaleFilter->setCurrentIndex(1);
		else if (downscaleFilter == "lanczos")
			videoPage->downscaleFilter->setCurrentIndex(2);
		else if (downscaleFilter == "area")
			videoPage->downscaleFilter->setCurrentIndex(3);
		else
			videoPage->downscaleFilter->setCurrentIndex(0);
	}
}

void OBSBasicSettings::LoadResolutionLists()
{
	uint32_t cx = config_get_uint(main->Config(), "Video", "BaseCX");
	uint32_t cy = config_get_uint(main->Config(), "Video", "BaseCY");
	uint32_t out_cx = config_get_uint(main->Config(), "Video", "OutputCX");
	uint32_t out_cy = config_get_uint(main->Config(), "Video", "OutputCY");

	videoPage->baseResolution->clear();

	auto addRes = [this](int cx, int cy) {
		QString res = ResString(cx, cy).c_str();
		if (videoPage->baseResolution->findText(res) == -1)
			videoPage->baseResolution->addItem(res);
	};

	QList<QPair<int, int>> lstReversed;
	auto dualOutputOn = pls_is_dual_output_on();
	for (QScreen *screen : QGuiApplication::screens()) {
		QSize as = screen->size();
		uint32_t as_width = as.width();
		uint32_t as_height = as.height();

		// Calculate physical screen resolution based on the virtual screen resolution
		// They might differ if scaling is enabled, e.g. for HiDPI screens
		as_width = round(as_width * screen->devicePixelRatio());
		as_height = round(as_height * screen->devicePixelRatio());

		addRes(as_width, as_height);
		if (!dualOutputOn)
			lstReversed.append({as_height, as_width});
	}

	addRes(1920, 1080);
	addRes(1280, 720);

	if (!dualOutputOn) {
		for (auto &[w, h] : lstReversed) {
			addRes(w, h);
		}
		addRes(1080, 1920);
		addRes(720, 1280);
	}

	string outputResString = ResString(out_cx, out_cy);

	videoPage->baseResolution->lineEdit()->setText(ResString(cx, cy).c_str());

	RecalcOutputResPixels(outputResString.c_str());
	ResetDownscales(cx, cy, true, false, false);

	if (videoPage->outputResolution->isEditable()) {
		videoPage->outputResolution->lineEdit()->setText(outputResString.c_str());
	} else {
		videoPage->outputResolution->setCurrentText(outputResString.c_str());
	}

	std::tuple<int, int> aspect = aspect_ratio(cx, cy);

	videoPage->baseAspect->setText(
		QTStr("AspectRatio").arg(QString::number(std::get<0>(aspect)), QString::number(std::get<1>(aspect))));
}

void OBSBasicSettings::LoadVerticalResolutionLists()
{
	uint32_t cx = config_get_uint(main->Config(), "Video", "BaseCXV");
	uint32_t cy = config_get_uint(main->Config(), "Video", "BaseCYV");

	videoPage->baseResolution_2->blockSignals(true);
	videoPage->baseResolution_2->clear();

	for (int i = 0; i < videoPage->baseResolution->count(); ++i) {
		const QStringList parts = videoPage->baseResolution->itemText(i).split('x');
		if (parts.size() != 2)
			continue;
		const QString flipped = parts[1] + 'x' + parts[0];
		if (videoPage->baseResolution_2->findText(flipped) == -1)
			videoPage->baseResolution_2->addItem(flipped);
	}

	QString currentBaseV = ResString(cx, cy).c_str();
	if (cx && cy && videoPage->baseResolution_2->findText(currentBaseV) == -1)
		videoPage->baseResolution_2->addItem(currentBaseV);
	videoPage->baseResolution_2->lineEdit()->setText(currentBaseV);
	videoPage->baseResolution_2->blockSignals(false);

	ResetVerticalDownscales(cx, cy);

	std::tuple<int, int> aspect = aspect_ratio(cx, cy);

	videoPage->baseAspect_2->setText(
		QTStr("AspectRatio").arg(QString::number(std::get<0>(aspect)), QString::number(std::get<1>(aspect))));
}

static inline void LoadFPSCommon(OBSBasic *main, Ui::SettingVideoPage *ui)
{
	const char *val = config_get_string(main->Config(), "Video", "FPSCommon");

	int idx = ui->fpsCommon->findText(val);
	if (idx == -1)
		idx = 4;
	ui->fpsCommon->setCurrentIndex(idx);
}

static inline void LoadFPSInteger(OBSBasic *main, Ui::SettingVideoPage *ui)
{
	uint32_t val = config_get_uint(main->Config(), "Video", "FPSInt");
	ui->fpsInteger->setValue(val);
}

static inline void LoadFPSFraction(OBSBasic *main, Ui::SettingVideoPage *ui)
{
	uint32_t num = config_get_uint(main->Config(), "Video", "FPSNum");
	uint32_t den = config_get_uint(main->Config(), "Video", "FPSDen");

	ui->fpsNumerator->setValue(num);
	ui->fpsDenominator->setValue(den);
}

void OBSBasicSettings::LoadFPSData()
{
	LoadFPSCommon(main, videoPage.get());
	LoadFPSInteger(main, videoPage.get());
	LoadFPSFraction(main, videoPage.get());

	uint32_t fpsType = config_get_uint(main->Config(), "Video", "FPSType");
	if (fpsType > 2)
		fpsType = 0;

	videoPage->fpsType->setCurrentIndex(fpsType);
	videoPage->fpsTypes->setCurrentIndex(fpsType);
}

void OBSBasicSettings::LoadVideoSettings()
{
	if (!videoPage) {
		return;
	}

	loading = true;

	if (obs_video_active()) {
		if (pls_is_dual_output_on()) {
			videoPage->checkBoxDualOutput->setEnabled(false);
			videoPage->labelDualOutputTooltip->setEnabled(false);
			videoPage->tabVideoHorizontal->setEnabled(false);
			videoPage->tabVideoVertical->setEnabled(false);
		} else {
			ui->videoPage->setEnabled(false);
		}
		updateAlertMessage(AlertMessageType::Warning, ui->videoPage,
				   QTStr("Basic.Settings.Video.CurrentlyActive"));
	}
	pls_uistep_v2_set_custom_enter_leave_name(videoPage->labelDualOutputTooltip, "Dual Output Help");
	LoadResolutionLists();
	LoadVerticalResolutionLists();
	LoadFPSData();
	LoadDownscaleFilters(true);

	loading = false;
}

static inline bool IsSurround(const char *speakers)
{
	static const char *surroundLayouts[] = {"2.1", "4.0", "4.1", "5.1", "7.1", nullptr};

	if (!speakers || !*speakers)
		return false;

	const char **curLayout = surroundLayouts;
	for (; *curLayout; ++curLayout) {
		if (strcmp(*curLayout, speakers) == 0) {
			return true;
		}
	}

	return false;
}

void OBSBasicSettings::LoadSimpleOutputSettings()
{
	if (!outputSimplePage) {
		return;
	}

	const char *path = config_get_string(main->Config(), "SimpleOutput", "FilePath");
	bool noSpace = config_get_bool(main->Config(), "SimpleOutput", "FileNameWithoutSpace");
	const char *format = config_get_string(main->Config(), "SimpleOutput", "RecFormat2");
	int videoBitrate = config_get_uint(main->Config(), "SimpleOutput", "VBitrate");
	const char *streamEnc = config_get_string(main->Config(), "SimpleOutput", "StreamEncoder");
	const char *streamAudioEnc = config_get_string(main->Config(), "SimpleOutput", "StreamAudioEncoder");
	int audioBitrate = config_get_uint(main->Config(), "SimpleOutput", "ABitrate");
	bool advanced = config_get_bool(main->Config(), "SimpleOutput", "UseAdvanced");
	const char *preset = config_get_string(main->Config(), "SimpleOutput", "Preset");
	const char *qsvPreset = config_get_string(main->Config(), "SimpleOutput", "QSVPreset");
	const char *nvPreset = config_get_string(main->Config(), "SimpleOutput", "NVENCPreset2");
	const char *amdPreset = config_get_string(main->Config(), "SimpleOutput", "AMDPreset");
	const char *amdAV1Preset = config_get_string(main->Config(), "SimpleOutput", "AMDAV1Preset");
	const char *custom = config_get_string(main->Config(), "SimpleOutput", "x264Settings");
	const char *recQual = config_get_string(main->Config(), "SimpleOutput", "RecQuality");
	const char *recEnc = config_get_string(main->Config(), "SimpleOutput", "RecEncoder");
	const char *recAudioEnc = config_get_string(main->Config(), "SimpleOutput", "RecAudioEncoder");
	const char *muxCustom = config_get_string(main->Config(), "SimpleOutput", "MuxerCustom");
	bool replayBuf = config_get_bool(main->Config(), "SimpleOutput", "RecRB");
	int rbTime = config_get_int(main->Config(), "SimpleOutput", "RecRBTime");
	int rbSize = config_get_int(main->Config(), "SimpleOutput", "RecRBSize");
	int tracks = config_get_int(main->Config(), "SimpleOutput", "RecTracks");

	outputSimplePage->simpleOutRecTrack1->setChecked(tracks & (1 << 0));
	outputSimplePage->simpleOutRecTrack2->setChecked(tracks & (1 << 1));
	outputSimplePage->simpleOutRecTrack3->setChecked(tracks & (1 << 2));
	outputSimplePage->simpleOutRecTrack4->setChecked(tracks & (1 << 3));
	outputSimplePage->simpleOutRecTrack5->setChecked(tracks & (1 << 4));
	outputSimplePage->simpleOutRecTrack6->setChecked(tracks & (1 << 5));

	curPreset = preset;
	curQSVPreset = qsvPreset;
	curNVENCPreset = nvPreset;
	curAMDPreset = amdPreset;
	curAMDAV1Preset = amdAV1Preset;

	bool isOpus = strcmp(streamAudioEnc, "opus") == 0;
	audioBitrate = isOpus ? FindClosestAvailableSimpleOpusBitrate(audioBitrate)
			      : FindClosestAvailableSimpleAACBitrate(audioBitrate);

	outputSimplePage->simpleOutputPath->setText(path);
	outputSimplePage->simpleNoSpace->setChecked(noSpace);
	outputSimplePage->simpleOutputVBitrate->setValue(videoBitrate);

	int idx = outputSimplePage->simpleOutRecFormat->findData(format);
	outputSimplePage->simpleOutRecFormat->setCurrentIndex(idx);

	PopulateSimpleBitrates(outputSimplePage->simpleOutputABitrate, isOpus);

	const char *speakers = config_get_string(main->Config(), "Audio", "ChannelSetup");

	// restrict list of bitrates when multichannel is OFF
	if (!IsSurround(speakers))
		RestrictResetBitrates({outputSimplePage->simpleOutputABitrate}, 320);

	SetComboByName(outputSimplePage->simpleOutputABitrate, std::to_string(audioBitrate).c_str());

	outputSimplePage->simpleOutAdvanced->setChecked(advanced);
	outputSimplePage->simpleOutCustom->setText(custom);

	idx = outputSimplePage->simpleOutRecQuality->findData(QString(recQual));
	if (idx == -1)
		idx = 0;
	outputSimplePage->simpleOutRecQuality->setCurrentIndex(idx);

	idx = outputSimplePage->simpleOutStrEncoder->findData(QString(streamEnc));
	if (idx == -1)
		idx = 0;
	outputSimplePage->simpleOutStrEncoder->setCurrentIndex(idx);

	idx = outputSimplePage->simpleOutStrAEncoder->findData(QString(streamAudioEnc));
	if (idx == -1)
		idx = 0;
	outputSimplePage->simpleOutStrAEncoder->setCurrentIndex(idx);

	idx = outputSimplePage->simpleOutRecEncoder->findData(QString(recEnc));
	outputSimplePage->simpleOutRecEncoder->setCurrentIndex(idx);

	idx = outputSimplePage->simpleOutRecAEncoder->findData(QString(recAudioEnc));
	outputSimplePage->simpleOutRecAEncoder->setCurrentIndex(idx);

	outputSimplePage->simpleOutMuxCustom->setText(muxCustom);

	outputSimplePage->simpleReplayBuf->setChecked(replayBuf);
	outputSimplePage->simpleRBSecMax->setValue(rbTime);
	outputSimplePage->simpleRBMegsMax->setValue(rbSize);

	SimpleStreamingEncoderChanged();
}

static inline QString makeFormatToolTip()
{
	static const char *format_list[][2] = {
		{"CCYY", "FilenameFormatting.TT.CCYY"}, {"YY", "FilenameFormatting.TT.YY"},
		{"MM", "FilenameFormatting.TT.MM"},     {"DD", "FilenameFormatting.TT.DD"},
		{"hh", "FilenameFormatting.TT.hh"},     {"mm", "FilenameFormatting.TT.mm"},
		{"ss", "FilenameFormatting.TT.ss"},     {"%", "FilenameFormatting.TT.Percent"},
		{"a", "FilenameFormatting.TT.a"},       {"A", "FilenameFormatting.TT.A"},
		{"b", "FilenameFormatting.TT.b"},       {"B", "FilenameFormatting.TT.B"},
		{"d", "FilenameFormatting.TT.d"},       {"H", "FilenameFormatting.TT.H"},
		{"I", "FilenameFormatting.TT.I"},       {"m", "FilenameFormatting.TT.m"},
		{"M", "FilenameFormatting.TT.M"},       {"p", "FilenameFormatting.TT.p"},
		{"s", "FilenameFormatting.TT.s"},       {"S", "FilenameFormatting.TT.S"},
		{"y", "FilenameFormatting.TT.y"},       {"Y", "FilenameFormatting.TT.Y"},
		{"z", "FilenameFormatting.TT.z"},       {"Z", "FilenameFormatting.TT.Z"},
		{"FPS", "FilenameFormatting.TT.FPS"},   {"CRES", "FilenameFormatting.TT.CRES"},
		{"ORES", "FilenameFormatting.TT.ORES"}, {"VF", "FilenameFormatting.TT.VF"},
	};

	QString html = "<table>";

	for (auto f : format_list) {
		html += "<tr><th align='left'>%";
		html += f[0];
		html += "</th><td>";
		html += QTStr(f[1]);
		html += "</td></tr>";
	}

	html += "</table>";
	return html;
}

void OBSBasicSettings::LoadAdvOutputStreamingSettings()
{
	if (!outputStreamPage) {
		return;
	}

	const char *rescaleRes = config_get_string(main->Config(), "AdvOut", "RescaleRes");
	int rescaleFilter = config_get_int(main->Config(), "AdvOut", "RescaleFilter");
	int trackIndex = config_get_int(main->Config(), "AdvOut", "TrackIndex");
	int trackIndexV = trackIndex;
	if (config_has_user_value(main->Config(), "AdvOut", "TrackIndexV")) {
		trackIndexV = config_get_int(main->Config(), "AdvOut", "TrackIndexV");
	}
	int audioMixes = config_get_int(main->Config(), "AdvOut", "StreamMultiTrackAudioMixes");

	outputStreamPage->advOutRescale->setEnabled(rescaleFilter != OBS_SCALE_DISABLE);
	if (nullptr != rescaleRes) {
		outputStreamPage->advOutRescale->setCurrentText(rescaleRes);
	}

	int idx = outputStreamPage->advOutRescaleFilter->findData(rescaleFilter);
	if (idx != -1)
		outputStreamPage->advOutRescaleFilter->setCurrentIndex(idx);

	switch (trackIndex) {
	case 1:
		outputStreamPage->advOutTrack1->setChecked(true);
		break;
	case 2:
		outputStreamPage->advOutTrack2->setChecked(true);
		break;
	case 3:
		outputStreamPage->advOutTrack3->setChecked(true);
		break;
	case 4:
		outputStreamPage->advOutTrack4->setChecked(true);
		break;
	case 5:
		outputStreamPage->advOutTrack5->setChecked(true);
		break;
	case 6:
		outputStreamPage->advOutTrack6->setChecked(true);
		break;
	}
	switch (trackIndexV) {
	case 1:
		outputStreamPage->advOutTrack1_2->setChecked(true);
		break;
	case 2:
		outputStreamPage->advOutTrack2_2->setChecked(true);
		break;
	case 3:
		outputStreamPage->advOutTrack3_2->setChecked(true);
		break;
	case 4:
		outputStreamPage->advOutTrack4_2->setChecked(true);
		break;
	case 5:
		outputStreamPage->advOutTrack5_2->setChecked(true);
		break;
	case 6:
		outputStreamPage->advOutTrack6_2->setChecked(true);
		break;
	}
	outputStreamPage->advOutMultiTrack1->setChecked(audioMixes & (1 << 0));
	outputStreamPage->advOutMultiTrack2->setChecked(audioMixes & (1 << 1));
	outputStreamPage->advOutMultiTrack3->setChecked(audioMixes & (1 << 2));
	outputStreamPage->advOutMultiTrack4->setChecked(audioMixes & (1 << 3));
	outputStreamPage->advOutMultiTrack5->setChecked(audioMixes & (1 << 4));
	outputStreamPage->advOutMultiTrack6->setChecked(audioMixes & (1 << 5));

	if (PLS_PLATFORM_API->AllowsMultiTrack()) {
		outputStreamPage->advStreamTrackWidget->setCurrentWidget(outputStreamPage->streamMultiTracks);
	} else {
		outputStreamPage->advStreamTrackWidget->setCurrentWidget(outputStreamPage->streamSingleTracks);
	}
}

OBSPropertiesView *OBSBasicSettings::CreateEncoderPropertyView(const char *encoder, const char *path, bool changed,
							       bool bChzzkKeyframeTip)
{
	OBSDataAutoRelease settings = obs_encoder_defaults(encoder);
	OBSPropertiesView *view;

	if (path) {
		char encoderJsonPath[512];
		int ret = GetProfilePath(encoderJsonPath, sizeof(encoderJsonPath), path);
		if (ret > 0) {
			obs_data_t *data = obs_data_create_from_json_file_safe(encoderJsonPath, "bak");
			obs_data_apply(settings, data);
			obs_data_release(data);
		}
	}

	PLSPropertiesData proData;
	proData.bChzzkKeyframeTip = bChzzkKeyframeTip;
	proData.bFromSetting = true;

	//PRISM/renjinbo/20230104/#/change to PLSPropertiesView
	view = new PLSPropertiesView(this, settings.Get(), encoder,
				     (PropertiesReloadCallback)obs_get_encoder_properties, proData);
	view->setFrameShape(QFrame::NoFrame);
	view->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
	view->setProperty("changed", QVariant(changed));
	QObject::connect(view, &OBSPropertiesView::Changed, this, &OBSBasicSettings::OutputsChanged);

	return view;
}

void OBSBasicSettings::LoadAdvOutputStreamingEncoderProperties()
{
	if (!outputStreamPage) {
		return;
	}

	const char *type = config_get_string(main->Config(), "AdvOut", "Encoder");

	delete streamEncoderProps;
	streamEncoderProps = CreateEncoderPropertyView(type, "streamEncoder.json", false, true);
	streamEncoderProps->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
	outputStreamPage->advOutEncoderLayout->addWidget(streamEncoderProps);

	connect(streamEncoderProps, &OBSPropertiesView::Changed, this, &OBSBasicSettings::UpdateStreamDelayEstimate);
	connect(streamEncoderProps, &OBSPropertiesView::Changed, this, &OBSBasicSettings::AdvReplayBufferChanged);

	curAdvStreamEncoder = type;

	if (!SetComboByValue(outputStreamPage->advOutEncoder, type)) {
		uint32_t caps = obs_get_encoder_caps(type);
		if ((caps & ENCODER_HIDE_FLAGS) != 0) {
			QString encName = QT_UTF8(obs_encoder_get_display_name(type));
			if (caps & OBS_ENCODER_CAP_DEPRECATED)
				encName += " (" + QTStr("Deprecated") + ")";

			outputStreamPage->advOutEncoder->insertItem(0, encName, QT_UTF8(type));
			SetComboByValue(outputStreamPage->advOutEncoder, type);
		}
	}

	UpdateStreamDelayEstimate();
}

void OBSBasicSettings::LoadAdvOutputRecordingSettings()
{
	const char *type = config_get_string(main->Config(), "AdvOut", "RecType");
	const char *format = config_get_string(main->Config(), "AdvOut", "RecFormat2");
	const char *path = config_get_string(main->Config(), "AdvOut", "RecFilePath");
	bool noSpace = config_get_bool(main->Config(), "AdvOut", "RecFileNameWithoutSpace");
	const char *rescaleRes = config_get_string(main->Config(), "AdvOut", "RecRescaleRes");
	int rescaleFilter = config_get_int(main->Config(), "AdvOut", "RecRescaleFilter");
	const char *muxCustom = config_get_string(main->Config(), "AdvOut", "RecMuxerCustom");
	int tracks = config_get_int(main->Config(), "AdvOut", "RecTracks");
	int flvTrack = config_get_int(main->Config(), "AdvOut", "FLVTrack");
	bool splitFile = config_get_bool(main->Config(), "AdvOut", "RecSplitFile");
	const char *splitFileType = config_get_string(main->Config(), "AdvOut", "RecSplitFileType");
	int splitFileTime = config_get_int(main->Config(), "AdvOut", "RecSplitFileTime");
	int splitFileSize = config_get_int(main->Config(), "AdvOut", "RecSplitFileSize");

	int typeIndex = (astrcmpi(type, "FFmpeg") == 0) ? 1 : 0;
	outputRecordPage->advOutRecType->setCurrentIndex(typeIndex);
	outputRecordPage->advOutRecPath->setText(path);
	outputRecordPage->advOutNoSpace->setChecked(noSpace);
	outputRecordPage->advOutRecRescale->setCurrentText(rescaleRes);
	int idx = outputRecordPage->advOutRecRescaleFilter->findData(rescaleFilter);
	if (idx != -1)
		outputRecordPage->advOutRecRescaleFilter->setCurrentIndex(idx);
	outputRecordPage->advOutMuxCustom->setText(muxCustom);

	idx = outputRecordPage->advOutRecFormat->findData(format);
	outputRecordPage->advOutRecFormat->setCurrentIndex(idx);

	outputRecordPage->advOutRecTrack1->setChecked(tracks & (1 << 0));
	outputRecordPage->advOutRecTrack2->setChecked(tracks & (1 << 1));
	outputRecordPage->advOutRecTrack3->setChecked(tracks & (1 << 2));
	outputRecordPage->advOutRecTrack4->setChecked(tracks & (1 << 3));
	outputRecordPage->advOutRecTrack5->setChecked(tracks & (1 << 4));
	outputRecordPage->advOutRecTrack6->setChecked(tracks & (1 << 5));

	if (astrcmpi(splitFileType, "Size") == 0)
		idx = 1;
	else if (astrcmpi(splitFileType, "Manual") == 0)
		idx = 2;
	else
		idx = 0;
	outputRecordPage->advOutSplitFile->setChecked(splitFile);
	outputRecordPage->advOutSplitFileType->setCurrentIndex(idx);
	outputRecordPage->advOutSplitFileTime->setValue(splitFileTime);
	outputRecordPage->advOutSplitFileSize->setValue(splitFileSize);

	switch (flvTrack) {
	case 1:
		outputRecordPage->flvTrack1->setChecked(true);
		break;
	case 2:
		outputRecordPage->flvTrack2->setChecked(true);
		break;
	case 3:
		outputRecordPage->flvTrack3->setChecked(true);
		break;
	case 4:
		outputRecordPage->flvTrack4->setChecked(true);
		break;
	case 5:
		outputRecordPage->flvTrack5->setChecked(true);
		break;
	case 6:
		outputRecordPage->flvTrack6->setChecked(true);
		break;
	default:
		outputRecordPage->flvTrack1->setChecked(true);
		break;
	}
}

void OBSBasicSettings::LoadAdvOutputRecordingEncoderProperties()
{
	const char *type = config_get_string(main->Config(), "AdvOut", "RecEncoder");

	delete recordEncoderProps;
	recordEncoderProps = nullptr;

	if (astrcmpi(type, "none") != 0) {
		recordEncoderProps = CreateEncoderPropertyView(type, "recordEncoder.json");
		recordEncoderProps->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
		outputRecordPage->advOutRecEncoderProps->layout()->addWidget(recordEncoderProps);
		connect(recordEncoderProps, &OBSPropertiesView::Changed, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
	}

	curAdvRecordEncoder = type;

	if (!SetComboByValue(outputRecordPage->advOutRecEncoder, type)) {
		uint32_t caps = obs_get_encoder_caps(type);
		if ((caps & ENCODER_HIDE_FLAGS) != 0) {
			QString encName = QT_UTF8(obs_encoder_get_display_name(type));
			if (caps & OBS_ENCODER_CAP_DEPRECATED)
				encName += " (" + QTStr("Deprecated") + ")";

			outputRecordPage->advOutRecEncoder->insertItem(1, encName, QT_UTF8(type));
			SetComboByValue(outputRecordPage->advOutRecEncoder, type);
		} else {
			outputRecordPage->advOutRecEncoder->setCurrentIndex(-1);
		}
	}
}

static void SelectFormat(QComboBox *combo, const char *name, const char *mimeType)
{
	FFmpegFormat format{name, mimeType};

	for (int i = 0; i < combo->count(); i++) {
		QVariant v = combo->itemData(i);
		if (!v.isNull()) {
			if (format == v.value<FFmpegFormat>()) {
				combo->setCurrentIndex(i);
				return;
			}
		}
	}

	combo->setCurrentIndex(0);
}

static void SelectEncoder(QComboBox *combo, const char *name, int id)
{
	int idx = FindEncoder(combo, name, id);
	if (idx >= 0)
		combo->setCurrentIndex(idx);
}

void OBSBasicSettings::LoadAdvOutputFFmpegSettings()
{
	bool saveFile = config_get_bool(main->Config(), "AdvOut", "FFOutputToFile");
	const char *path = config_get_string(main->Config(), "AdvOut", "FFFilePath");
	bool noSpace = config_get_bool(main->Config(), "AdvOut", "FFFileNameWithoutSpace");
	const char *url = config_get_string(main->Config(), "AdvOut", "FFURL");
	const char *format = config_get_string(main->Config(), "AdvOut", "FFFormat");
	const char *mimeType = config_get_string(main->Config(), "AdvOut", "FFFormatMimeType");
	const char *muxCustom = config_get_string(main->Config(), "AdvOut", "FFMCustom");
	int videoBitrate = config_get_int(main->Config(), "AdvOut", "FFVBitrate");
	int gopSize = config_get_int(main->Config(), "AdvOut", "FFVGOPSize");
	bool rescale = config_get_bool(main->Config(), "AdvOut", "FFRescale");
	bool codecCompat = config_get_bool(main->Config(), "AdvOut", "FFIgnoreCompat");
	const char *rescaleRes = config_get_string(main->Config(), "AdvOut", "FFRescaleRes");
	const char *vEncoder = config_get_string(main->Config(), "AdvOut", "FFVEncoder");
	int vEncoderId = config_get_int(main->Config(), "AdvOut", "FFVEncoderId");
	const char *vEncCustom = config_get_string(main->Config(), "AdvOut", "FFVCustom");
	int audioBitrate = config_get_int(main->Config(), "AdvOut", "FFABitrate");
	int audioMixes = config_get_int(main->Config(), "AdvOut", "FFAudioMixes");
	const char *aEncoder = config_get_string(main->Config(), "AdvOut", "FFAEncoder");
	int aEncoderId = config_get_int(main->Config(), "AdvOut", "FFAEncoderId");
	const char *aEncCustom = config_get_string(main->Config(), "AdvOut", "FFACustom");

	outputRecordPage->advOutFFType->setCurrentIndex(saveFile ? 0 : 1);
	outputRecordPage->advOutFFRecPath->setText(QT_UTF8(path));
	outputRecordPage->advOutFFNoSpace->setChecked(noSpace);
	outputRecordPage->advOutFFURL->setText(QT_UTF8(url));
	SelectFormat(outputRecordPage->advOutFFFormat, format, mimeType);
	outputRecordPage->advOutFFMCfg->setText(muxCustom);
	outputRecordPage->advOutFFVBitrate->setValue(videoBitrate);
	outputRecordPage->advOutFFVGOPSize->setValue(gopSize);
	outputRecordPage->advOutFFUseRescale->setChecked(rescale);
	outputRecordPage->advOutFFIgnoreCompat->setChecked(codecCompat);
	outputRecordPage->advOutFFRescale->setEnabled(rescale);
	outputRecordPage->advOutFFRescale->setCurrentText(rescaleRes);
	SelectEncoder(outputRecordPage->advOutFFVEncoder, vEncoder, vEncoderId);
	outputRecordPage->advOutFFVCfg->setText(vEncCustom);
	outputRecordPage->advOutFFABitrate->setValue(audioBitrate);
	SelectEncoder(outputRecordPage->advOutFFAEncoder, aEncoder, aEncoderId);
	outputRecordPage->advOutFFACfg->setText(aEncCustom);

	outputRecordPage->advOutFFTrack1->setChecked(audioMixes & (1 << 0));
	outputRecordPage->advOutFFTrack2->setChecked(audioMixes & (1 << 1));
	outputRecordPage->advOutFFTrack3->setChecked(audioMixes & (1 << 2));
	outputRecordPage->advOutFFTrack4->setChecked(audioMixes & (1 << 3));
	outputRecordPage->advOutFFTrack5->setChecked(audioMixes & (1 << 4));
	outputRecordPage->advOutFFTrack6->setChecked(audioMixes & (1 << 5));
}

void OBSBasicSettings::LoadAdvOutputAudioSettings()
{
	int track1Bitrate = config_get_uint(main->Config(), "AdvOut", "Track1Bitrate");
	int track2Bitrate = config_get_uint(main->Config(), "AdvOut", "Track2Bitrate");
	int track3Bitrate = config_get_uint(main->Config(), "AdvOut", "Track3Bitrate");
	int track4Bitrate = config_get_uint(main->Config(), "AdvOut", "Track4Bitrate");
	int track5Bitrate = config_get_uint(main->Config(), "AdvOut", "Track5Bitrate");
	int track6Bitrate = config_get_uint(main->Config(), "AdvOut", "Track6Bitrate");
	const char *name1 = config_get_string(main->Config(), "AdvOut", "Track1Name");
	const char *name2 = config_get_string(main->Config(), "AdvOut", "Track2Name");
	const char *name3 = config_get_string(main->Config(), "AdvOut", "Track3Name");
	const char *name4 = config_get_string(main->Config(), "AdvOut", "Track4Name");
	const char *name5 = config_get_string(main->Config(), "AdvOut", "Track5Name");
	const char *name6 = config_get_string(main->Config(), "AdvOut", "Track6Name");

	const char *encoder_id = config_get_string(main->Config(), "AdvOut", "AudioEncoder");
	const char *rec_encoder_id = config_get_string(main->Config(), "AdvOut", "RecAudioEncoder");

	PopulateAdvancedBitrates({outputAudioPage->advOutTrack1Bitrate, outputAudioPage->advOutTrack2Bitrate,
				  outputAudioPage->advOutTrack3Bitrate, outputAudioPage->advOutTrack4Bitrate,
				  outputAudioPage->advOutTrack5Bitrate, outputAudioPage->advOutTrack6Bitrate},
				 encoder_id, strcmp(rec_encoder_id, "none") != 0 ? rec_encoder_id : encoder_id);

	track1Bitrate = FindClosestAvailableAudioBitrate(outputAudioPage->advOutTrack1Bitrate, track1Bitrate);
	track2Bitrate = FindClosestAvailableAudioBitrate(outputAudioPage->advOutTrack2Bitrate, track2Bitrate);
	track3Bitrate = FindClosestAvailableAudioBitrate(outputAudioPage->advOutTrack3Bitrate, track3Bitrate);
	track4Bitrate = FindClosestAvailableAudioBitrate(outputAudioPage->advOutTrack4Bitrate, track4Bitrate);
	track5Bitrate = FindClosestAvailableAudioBitrate(outputAudioPage->advOutTrack5Bitrate, track5Bitrate);
	track6Bitrate = FindClosestAvailableAudioBitrate(outputAudioPage->advOutTrack6Bitrate, track6Bitrate);

	// restrict list of bitrates when multichannel is OFF
	const char *speakers = config_get_string(main->Config(), "Audio", "ChannelSetup");

	// restrict list of bitrates when multichannel is OFF
	if (!IsSurround(speakers)) {
		RestrictResetBitrates({outputAudioPage->advOutTrack1Bitrate, outputAudioPage->advOutTrack2Bitrate,
				       outputAudioPage->advOutTrack3Bitrate, outputAudioPage->advOutTrack4Bitrate,
				       outputAudioPage->advOutTrack5Bitrate, outputAudioPage->advOutTrack6Bitrate},
				      320);
	}

	SetComboByName(outputAudioPage->advOutTrack1Bitrate, std::to_string(track1Bitrate).c_str());
	SetComboByName(outputAudioPage->advOutTrack2Bitrate, std::to_string(track2Bitrate).c_str());
	SetComboByName(outputAudioPage->advOutTrack3Bitrate, std::to_string(track3Bitrate).c_str());
	SetComboByName(outputAudioPage->advOutTrack4Bitrate, std::to_string(track4Bitrate).c_str());
	SetComboByName(outputAudioPage->advOutTrack5Bitrate, std::to_string(track5Bitrate).c_str());
	SetComboByName(outputAudioPage->advOutTrack6Bitrate, std::to_string(track6Bitrate).c_str());

	outputAudioPage->advOutTrack1Name->setText(name1);
	outputAudioPage->advOutTrack2Name->setText(name2);
	outputAudioPage->advOutTrack3Name->setText(name3);
	outputAudioPage->advOutTrack4Name->setText(name4);
	outputAudioPage->advOutTrack5Name->setText(name5);
	outputAudioPage->advOutTrack6Name->setText(name6);
}

void OBSBasicSettings::LoadAdvOutputReplaySettings()
{
	bool replayBuf = config_get_bool(main->Config(), "AdvOut", "RecRB");
	int rbTime = config_get_int(main->Config(), "AdvOut", "RecRBTime");
	int rbSize = config_get_int(main->Config(), "AdvOut", "RecRBSize");

	outputReplayPage->advReplayBuf->setChecked(replayBuf);
	outputReplayPage->advRBSecMax->setValue(rbTime);
	outputReplayPage->advRBMegsMax->setValue(rbSize);
}

void OBSBasicSettings::LoadOutputSettings()
{
	if (!outputPage) {
		return;
	}

	loading = true;

	ResetSimpleEncoders();
	ResetStreamEncoders();
	ResetRecordEncoders();

	const char *mode = config_get_string(main->Config(), "Output", "Mode");
	int modeIdx = astrcmpi(mode, "Advanced") == 0 ? 1 : 0;
	outputPage->outputMode->setCurrentIndex(modeIdx);
	if (0 == modeIdx) {
		initOutputSimplePage();

		auto position = outputSimplePage->verticalLayout_52->indexOf(outputSimplePage->simpleStreamingGroupBox);
		outputSimplePage->verticalLayout_52->insertWidget(position + 1, outputPage->multitrackVideoGroupBox);
	}

	LoadSimpleOutputSettings();
	LoadAdvOutputStreamingSettings();
	LoadAdvOutputStreamingEncoderProperties();

	if (outputStreamPage) {
		const char *type = config_get_string(main->Config(), "AdvOut", "AudioEncoder");
		if (!SetComboByValue(outputStreamPage->advOutAEncoder, type)) {
			outputStreamPage->advOutAEncoder->setCurrentIndex(0);
		}
		outputStreamPage->advOutAEncoder->setProperty("changed", QVariant(true));
	}

	if (outputRecordPage) {
		LoadAdvOutputRecordingSettings();
		LoadAdvOutputRecordingEncoderProperties();

		const char *type = config_get_string(main->Config(), "AdvOut", "RecAudioEncoder");
		if (!SetComboByValue(outputRecordPage->advOutRecAEncoder, type))
			outputRecordPage->advOutRecAEncoder->setCurrentIndex(-1);

		LoadAdvOutputFFmpegSettings();
	}

	if (outputAudioPage) {
		LoadAdvOutputAudioSettings();
	}

	if (outputReplayPage) {
		LoadAdvOutputReplaySettings();
	}

	if (obs_video_active() || pls_is_output_actived()) {
		outputPage->outputMode->setEnabled(false);
		outputPage->outputModeLabel->setEnabled(false);

		if (outputSimplePage) {
			outputSimplePage->simpleOutStrEncoderLabel->setEnabled(false);
			outputSimplePage->simpleOutStrEncoder->setEnabled(false);
			outputSimplePage->simpleOutStrAEncoderLabel->setEnabled(false);
			outputSimplePage->simpleOutStrAEncoder->setEnabled(false);
			outputSimplePage->simpleRecordingGroupBox->setEnabled(false);
			outputSimplePage->simpleReplayBuf->setEnabled(false);
		}

		if (outputStreamPage) {
			outputStreamPage->advOutTopContainer->setEnabled(false);
		}

		if (outputRecordPage) {
			outputRecordPage->advOutRecTopContainer->setEnabled(false);
			outputRecordPage->advOutRecTypeContainer->setEnabled(false);
		}

		outputPage->advOutputAudioTracksTab->setEnabled(false);
		outputPage->widget->setEnabled(false);
	}

	loading = false;
}

void OBSBasicSettings::SetAdvOutputFFmpegEnablement(FFmpegCodecType encoderType, bool enabled, bool enableEncoder)
{
	bool rescale = config_get_bool(main->Config(), "AdvOut", "FFRescale");

	switch (encoderType) {
	case FFmpegCodecType::VIDEO:
		outputRecordPage->advOutFFVBitrate->setEnabled(enabled);
		outputRecordPage->advOutFFVGOPSize->setEnabled(enabled);
		outputRecordPage->advOutFFUseRescale->setEnabled(enabled);
		outputRecordPage->advOutFFRescale->setEnabled(enabled && rescale);
		outputRecordPage->advOutFFVEncoder->setEnabled(enabled || enableEncoder);
		outputRecordPage->advOutFFVCfg->setEnabled(enabled);
		break;
	case FFmpegCodecType::AUDIO:
		outputRecordPage->advOutFFABitrate->setEnabled(enabled);
		outputRecordPage->advOutFFAEncoder->setEnabled(enabled || enableEncoder);
		outputRecordPage->advOutFFTrack1->setEnabled(enabled);
		outputRecordPage->advOutFFTrack2->setEnabled(enabled);
		outputRecordPage->advOutFFTrack3->setEnabled(enabled);
		outputRecordPage->advOutFFTrack4->setEnabled(enabled);
		outputRecordPage->advOutFFTrack5->setEnabled(enabled);
		outputRecordPage->advOutFFTrack6->setEnabled(enabled);
	default:
		break;
	}
}

static inline void LoadListValue(QComboBox *widget, const char *text, const char *val)
{
	widget->addItem(QT_UTF8(text), QT_UTF8(val));
}

void OBSBasicSettings::LoadListValues(QComboBox *widget, obs_property_t *prop, int index)
{
	widget->clear();

	size_t count = obs_property_list_item_count(prop);

	OBSSourceAutoRelease source = obs_get_output_source(index);
	const char *deviceId = nullptr;
	OBSDataAutoRelease settings = nullptr;

	if (source) {
		settings = obs_source_get_settings(source);
		if (settings)
			deviceId = obs_data_get_string(settings, "device_id");
	}

	widget->addItem(QTStr("Basic.Settings.Audio.Disabled"), "disabled");

	for (size_t i = 0; i < count; i++) {
		const char *name = obs_property_list_item_name(prop, i);
		const char *val = obs_property_list_item_string(prop, i);
		LoadListValue(widget, name, val);
	}

	if (deviceId) {
		QVariant var(QT_UTF8(deviceId));
		int idx = widget->findData(var);
		if (idx != -1) {
			widget->setCurrentIndex(idx);
		} else {
			widget->insertItem(0,
					   QTStr("Basic.Settings.Audio."
						 "UnknownAudioDevice"),
					   var);
			widget->setCurrentIndex(0);
			HighlightGroupBoxLabel(audioPage->audioDevicesGroupBox, widget, "errorLabel");
		}
	}
}

void OBSBasicSettings::LoadAudioDevices()
{
	const char *input_id = App()->InputAudioSource();
	const char *output_id = App()->OutputAudioSource();

	obs_properties_t *input_props = obs_get_source_properties(input_id);
	obs_properties_t *output_props = obs_get_source_properties(output_id);

	if (input_props) {
		obs_property_t *inputs = obs_properties_get(input_props, "device_id");
		LoadListValues(audioPage->auxAudioDevice1, inputs, 3);
		LoadListValues(audioPage->auxAudioDevice2, inputs, 4);
		LoadListValues(audioPage->auxAudioDevice3, inputs, 5);
		LoadListValues(audioPage->auxAudioDevice4, inputs, 6);
		obs_properties_destroy(input_props);
	}

	if (output_props) {
		obs_property_t *outputs = obs_properties_get(output_props, "device_id");
		LoadListValues(audioPage->desktopAudioDevice1, outputs, 1);
		LoadListValues(audioPage->desktopAudioDevice2, outputs, 2);
		obs_properties_destroy(output_props);
	}

	if (obs_video_active()) {
		audioPage->sampleRate->setEnabled(false);
		audioPage->channelSetup->setEnabled(false);
	}
}

#define NBSP "\xC2\xA0"

void OBSBasicSettings::LoadAudioSources()
{
	bReloadAudioSources = false;

	if (audioPage->audioSourceLayout->rowCount() > 0) {
		QLayoutItem *forDeletion = audioPage->audioSourceLayout->takeAt(0);
		forDeletion->widget()->deleteLater();
		delete forDeletion;
	}
	auto layout = new QFormLayout();
	layout->setVerticalSpacing(10);
	layout->setHorizontalSpacing(20);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

	audioSourceSignals.clear();
	audioSources.clear();

	auto widget = new QWidget();
	widget->setLayout(layout);
	audioPage->audioSourceLayout->addRow(widget);

	const char *enablePtm = Str("Basic.Settings.Audio.EnablePushToMute");
	const char *ptmDelay = Str("Basic.Settings.Audio.PushToMuteDelay");
	const char *enablePtt = Str("Basic.Settings.Audio.EnablePushToTalk");
	const char *pttDelay = Str("Basic.Settings.Audio.PushToTalkDelay");
	auto AddSource = [&](obs_source_t *source) {
		if (!(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO))
			return true;

		auto form = new QFormLayout();
		form->setVerticalSpacing(10);
		form->setHorizontalSpacing(20);
		form->setContentsMargins(0, 0, 0, 0);
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

		auto ptmCB = new SilentUpdateCheckBox();
		ptmCB->setText(enablePtm);
		ptmCB->setChecked(obs_source_push_to_mute_enabled(source));
		form->addRow(ptmCB);

		auto ptmSB = new SilentUpdateSpinBox();
		ptmSB->setSuffix(NBSP "ms");
		ptmSB->setRange(0, INT_MAX);
		ptmSB->setValue(obs_source_get_push_to_mute_delay(source));
		form->addRow(ptmDelay, ptmSB);

		auto pttCB = new SilentUpdateCheckBox();
		pttCB->setText(enablePtt);
		pttCB->setChecked(obs_source_push_to_talk_enabled(source));
		form->addRow(pttCB);

		auto pttSB = new SilentUpdateSpinBox();
		pttSB->setSuffix(NBSP "ms");
		pttSB->setRange(0, INT_MAX);
		pttSB->setValue(obs_source_get_push_to_talk_delay(source));
		form->addRow(pttDelay, pttSB);

		HookWidget(ptmCB, CHECK_CHANGED, AUDIO_CHANGED);
		HookWidget(ptmSB, SCROLL_CHANGED, AUDIO_CHANGED);
		HookWidget(pttCB, CHECK_CHANGED, AUDIO_CHANGED);
		HookWidget(pttSB, SCROLL_CHANGED, AUDIO_CHANGED);

		audioSourceSignals.reserve(audioSourceSignals.size() + 4);

		auto handler = obs_source_get_signal_handler(source);
		audioSourceSignals.emplace_back(
			handler, "push_to_mute_changed",
			[](void *data, calldata_t *param) {
				QMetaObject::invokeMethod(static_cast<QObject *>(data), "setCheckedSilently",
							  Qt::QueuedConnection,
							  Q_ARG(bool, calldata_bool(param, "enabled")));
			},
			ptmCB);
		audioSourceSignals.emplace_back(
			handler, "push_to_mute_delay",
			[](void *data, calldata_t *param) {
				QMetaObject::invokeMethod(static_cast<QObject *>(data), "setValueSilently",
							  Qt::QueuedConnection,
							  Q_ARG(int, calldata_int(param, "delay")));
			},
			ptmSB);
		audioSourceSignals.emplace_back(
			handler, "push_to_talk_changed",
			[](void *data, calldata_t *param) {
				QMetaObject::invokeMethod(static_cast<QObject *>(data), "setCheckedSilently",
							  Qt::QueuedConnection,
							  Q_ARG(bool, calldata_bool(param, "enabled")));
			},
			pttCB);
		audioSourceSignals.emplace_back(
			handler, "push_to_talk_delay",
			[](void *data, calldata_t *param) {
				QMetaObject::invokeMethod(static_cast<QObject *>(data), "setValueSilently",
							  Qt::QueuedConnection,
							  Q_ARG(int, calldata_int(param, "delay")));
			},
			pttSB);

		audioSources.emplace_back(OBSGetWeakRef(source), ptmCB, ptmSB, pttCB, pttSB);

		auto label = new OBSSourceLabel(source);
		TruncateLabel(label, label->text());
		label->setMinimumSize(QSize(170, 0));
		label->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
		label->setPadding(0);
		connect(label, &OBSSourceLabel::Removed,
			[=]() { QMetaObject::invokeMethod(this, "ReloadAudioSources"); });
		connect(label, &OBSSourceLabel::Destroyed,
			[=]() { QMetaObject::invokeMethod(this, "ReloadAudioSources"); });

		layout->addRow(label, form);
		label->setProperty("useFor", "FormLabelRole");
		return true;
	};

	using AddSource_t = decltype(AddSource);
	obs_enum_sources(
		[](void *data, obs_source_t *source) {
			auto &AddSource = *static_cast<AddSource_t *>(data);
			if (!obs_source_removed(source))
				AddSource(source);
			return true;
		},
		static_cast<void *>(&AddSource));

	if (layout->rowCount() == 0)
		audioPage->audioHotkeysGroupBox->hide();
	else
		audioPage->audioHotkeysGroupBox->show();
}

void OBSBasicSettings::LoadAudioSettings()
{
	if (!audioPage) {
		return;
	}

	uint32_t sampleRate = config_get_uint(main->Config(), "Audio", "SampleRate");
	const char *speakers = config_get_string(main->Config(), "Audio", "ChannelSetup");
	double meterDecayRate = config_get_double(main->Config(), "Audio", "MeterDecayRate");
	uint32_t peakMeterTypeIdx = config_get_uint(main->Config(), "Audio", "PeakMeterType");
	bool enableLLAudioBuffering = config_get_bool(App()->GetUserConfig(), "Audio", "LowLatencyAudioBuffering");

	loading = true;

	const char *str;
	if (sampleRate == 48000)
		str = "48 kHz";
	else
		str = "44.1 kHz";

	int sampleRateIdx = audioPage->sampleRate->findText(str);
	if (sampleRateIdx != -1)
		audioPage->sampleRate->setCurrentIndex(sampleRateIdx);

	if (strcmp(speakers, "Mono") == 0)
		audioPage->channelSetup->setCurrentIndex(0);
	else if (strcmp(speakers, "2.1") == 0)
		audioPage->channelSetup->setCurrentIndex(2);
	else if (strcmp(speakers, "4.0") == 0)
		audioPage->channelSetup->setCurrentIndex(3);
	else if (strcmp(speakers, "4.1") == 0)
		audioPage->channelSetup->setCurrentIndex(4);
	else if (strcmp(speakers, "5.1") == 0)
		audioPage->channelSetup->setCurrentIndex(5);
	else if (strcmp(speakers, "7.1") == 0)
		audioPage->channelSetup->setCurrentIndex(6);
	else
		audioPage->channelSetup->setCurrentIndex(1);

	if (meterDecayRate == VOLUME_METER_DECAY_MEDIUM)
		audioPage->meterDecayRate->setCurrentIndex(1);
	else if (meterDecayRate == VOLUME_METER_DECAY_SLOW)
		audioPage->meterDecayRate->setCurrentIndex(2);
	else
		audioPage->meterDecayRate->setCurrentIndex(0);

	QString monDevName;
	QString monDevId;
	if (obs_audio_monitoring_available()) {
		monDevName = config_get_string(main->Config(), "Audio", "MonitoringDeviceName");
		monDevId = config_get_string(main->Config(), "Audio", "MonitoringDeviceId");
	}

	if (obs_audio_monitoring_available() && !SetComboByValue(audioPage->monitoringDevice, monDevId.toUtf8()))
		SetInvalidValue(audioPage->monitoringDevice, monDevName.toUtf8(), monDevId.toUtf8());

#ifdef _WIN32
	bool disableAudioDucking = config_get_bool(App()->GetAppConfig(), "Audio", "DisableAudioDucking");
	audioPage->disableAudioDucking->setChecked(disableAudioDucking);
#else
	delete audioPage->disableAudioDucking;
	audioPage->disableAudioDucking = nullptr;
#endif

	audioPage->peakMeterType->setCurrentIndex(peakMeterTypeIdx);
	audioPage->lowLatencyBuffering->setChecked(enableLLAudioBuffering);

	LoadAudioDevices();
	LoadAudioSources();

	loading = false;
}

void OBSBasicSettings::UpdateColorFormatSpaceWarning()
{
	const QString format = advancedPage->colorFormat->currentData().toString();
	switch (advancedPage->colorSpace->currentIndex()) {
	case 3: /* Rec.2100 (PQ) */
	case 4: /* Rec.2100 (HLG) */
		if ((format == "P010") || (format == "P216") || (format == "P416")) {
			clearAlertMessage(AlertMessageType::Warning, advancedPage->colorFormat);
		} else if (format == "I010") {
			updateAlertMessage(AlertMessageType::Warning, advancedPage->colorFormat,
					   QTStr("Basic.Settings.Advanced.FormatWarning"));
		} else {
			updateAlertMessage(AlertMessageType::Warning, advancedPage->colorFormat,
					   QTStr("Basic.Settings.Advanced.FormatWarning2100"));
		}
		break;
	default:
		if (format == "NV12") {
			clearAlertMessage(AlertMessageType::Warning, advancedPage->colorFormat);
		} else if ((format == "I010") || (format == "P010") || (format == "P216") || (format == "P416")) {
			updateAlertMessage(AlertMessageType::Warning, advancedPage->colorFormat,
					   QTStr("Basic.Settings.Advanced.FormatWarningPreciseSdr"));
		} else {
			updateAlertMessage(AlertMessageType::Warning, advancedPage->colorFormat,
					   QTStr("Basic.Settings.Advanced.FormatWarning"));
		}
	}
}

void OBSBasicSettings::LoadAdvancedSettings()
{
	if (!advancedPage) {
		return;
	}

	const char *videoColorFormat = config_get_string(main->Config(), "Video", "ColorFormat");
	const char *videoColorSpace = config_get_string(main->Config(), "Video", "ColorSpace");
	const char *videoColorRange = config_get_string(main->Config(), "Video", "ColorRange");
	uint32_t sdrWhiteLevel = (uint32_t)config_get_uint(main->Config(), "Video", "SdrWhiteLevel");
	uint32_t hdrNominalPeakLevel = (uint32_t)config_get_uint(main->Config(), "Video", "HdrNominalPeakLevel");

	bool enableDelay = config_get_bool(main->Config(), "Output", "DelayEnable");
	int delaySec = config_get_int(main->Config(), "Output", "DelaySec");
	bool preserveDelay = config_get_bool(main->Config(), "Output", "DelayPreserve");
	bool reconnect = config_get_bool(main->Config(), "Output", "Reconnect");
	int retryDelay = config_get_int(main->Config(), "Output", "RetryDelay");
	int maxRetries = config_get_int(main->Config(), "Output", "MaxRetries");
	const char *filename = config_get_string(main->Config(), "Output", "FilenameFormatting");
	bool overwriteIfExists = config_get_bool(main->Config(), "Output", "OverwriteIfExists");
	const char *bindIP = config_get_string(main->Config(), "Output", "BindIP");
	const char *rbPrefix = config_get_string(main->Config(), "SimpleOutput", "RecRBPrefix");
	const char *rbSuffix = config_get_string(main->Config(), "SimpleOutput", "RecRBSuffix");
	bool autoRemux = config_get_bool(main->Config(), "Video", "AutoRemux");
	const char *hotkeyFocusType = config_get_string(App()->GetUserConfig(), "General", "HotkeyFocusType");
	bool dynBitrate = config_get_bool(main->Config(), "Output", "DynamicBitrate");
	const char *ipFamily = config_get_string(main->Config(), "Output", "IPFamily");
	bool confirmOnExit = config_get_bool(App()->GetUserConfig(), "General", "ConfirmOnExit");

	loading = true;

	LoadRendererList();

	advancedPage->confirmOnExit->setChecked(confirmOnExit);

	QStringList specList = QTStr("FilenameFormatting.completer").split(QRegularExpression("\n"));
	if (const PLSCompleter *completer =
		    PLSCompleter::attachLineEdit(this, advancedPage->filenameFormatting, specList);
	    completer) {
		connect(completer, &PLSCompleter::activated, this, [this]() {
			advancedChanged = true;
			advancedPage->filenameFormatting->setProperty("changed", true);
			EnableApplyButton(true);
		});
	}
	advancedPage->filenameFormatting->setText(filename);
	advancedPage->filenameFormatting->setToolTip(makeFormatToolTip());

	advancedPage->overwriteIfExists->setChecked(overwriteIfExists);
	advancedPage->simpleRBPrefix->setText(rbPrefix);
	advancedPage->simpleRBSuffix->setText(rbSuffix);

	advancedPage->reconnectEnable->setChecked(reconnect);
	advancedPage->reconnectRetryDelay->setValue(retryDelay);
	advancedPage->reconnectMaxRetries->setValue(maxRetries);

	advancedPage->streamDelaySec->setValue(delaySec);
	advancedPage->streamDelayPreserve->setChecked(preserveDelay);
	advancedPage->streamDelayEnable->setChecked(enableDelay);
	advancedPage->autoRemux->setChecked(autoRemux);
	advancedPage->dynBitrate->setChecked(dynBitrate);

	SetComboByValue(advancedPage->colorFormat, videoColorFormat);
	SetComboByValue(advancedPage->colorSpace, videoColorSpace);
	SetComboByValue(advancedPage->colorRange, videoColorRange);
	advancedPage->sdrWhiteLevel->setValue(sdrWhiteLevel);
	advancedPage->hdrNominalPeakLevel->setValue(hdrNominalPeakLevel);

	SetComboByValue(advancedPage->ipFamily, ipFamily);
	if (!SetComboByValue(advancedPage->bindToIP, bindIP))
		SetInvalidValue(advancedPage->bindToIP, bindIP, bindIP);

	if (obs_video_active()) {
		advancedPage->advancedVideoContainer->setEnabled(false);
	}

#ifdef __APPLE__
	bool disableOSXVSync = config_get_bool(App()->GetAppConfig(), "Video", "DisableOSXVSync");
	bool resetOSXVSync = config_get_bool(App()->GetAppConfig(), "Video", "ResetOSXVSyncOnExit");
	advancedPage->disableOSXVSync->setChecked(disableOSXVSync);
	advancedPage->resetOSXVSync->setChecked(resetOSXVSync);
	advancedPage->resetOSXVSync->setEnabled(disableOSXVSync);
#elif _WIN32
	const char *processPriority = config_get_string(App()->GetAppConfig(), "General", "ProcessPriority");
	bool enableNewSocketLoop = config_get_bool(main->Config(), "Output", "NewSocketLoopEnable");
	bool enableLowLatencyMode = config_get_bool(main->Config(), "Output", "LowLatencyEnable");

	int idx = advancedPage->processPriority->findData(processPriority);
	if (idx == -1)
		idx = advancedPage->processPriority->findData("Normal");
	advancedPage->processPriority->setCurrentIndex(idx);

	advancedPage->enableNewSocketLoop->setChecked(enableNewSocketLoop);
	advancedPage->enableLowLatencyMode->setChecked(enableLowLatencyMode);
	advancedPage->enableLowLatencyMode->setToolTip(QTStr("Basic.Settings.Advanced.Network.TCPPacing.Tooltip"));
#endif

	if (obs_video_active()) {
		advancedPage->advNetworkGroupBox->setEnabled(false);
	}

#if defined(_WIN32) || defined(__APPLE__)
	bool browserHWAccel = config_get_bool(App()->GetAppConfig(), "General", "BrowserHWAccel");
	advancedPage->browserHWAccel->setChecked(browserHWAccel);
	prevBrowserAccel = advancedPage->browserHWAccel->isChecked();
#endif

	SetComboByValue(advancedPage->hotkeyFocusType, hotkeyFocusType);

	loading = false;
}

template<typename Func>
static inline void LayoutHotkey(OBSBasicSettings *settings, obs_hotkey_id id, obs_hotkey_t *key, Func &&fun,
				const map<obs_hotkey_id, vector<obs_key_combination_t>> &keys)
{
	auto *label = new OBSHotkeyLabel;
	QString text = QT_UTF8(obs_hotkey_get_description(key));

	label->setProperty("fullName", text);
	label->setWordWrap(true);
	TruncateLabel(label, text);
	label->setToolTip(text);

	OBSHotkeyWidget *hw = nullptr;

	auto combos = keys.find(id);
	if (combos == std::end(keys))
		hw = new OBSHotkeyWidget(settings, id, obs_hotkey_get_name(key), settings);
	else
		hw = new OBSHotkeyWidget(settings, id, obs_hotkey_get_name(key), settings, combos->second);

	hw->label = label;
	hw->setAccessibleName(text);
	label->widget = hw;

	fun(key, label, hw);
}

template<typename Func, typename T> static QLabel *makeLabel(T &t, Func &&getName)
{
	QLabel *label = new QLabel(getName(t));
	label->setStyleSheet("font-weight: bold;");
	return label;
}

template<typename Func> static QLabel *makeLabel(const OBSSource &source, Func &&)
{
	auto *label = new QLabel(obs_source_get_name(source));
	label->setStyleSheet("font-weight: bold;");
	QString name = QT_UTF8(obs_source_get_name(source));
	TruncateLabel(label, name);

	return label;
}

template<typename Func, typename T>
static inline void AddHotkeys(QFormLayout &layout, Func &&getName,
			      std::vector<std::tuple<T, QPointer<QLabel>, QPointer<QWidget>>> &hotkeys,
			      bool bTwoColumns = false)
{
	if (hotkeys.empty())
		return;

	layout.setItem(layout.rowCount(), QFormLayout::SpanningRole, new QSpacerItem(0, 10));

	using tuple_type = std::tuple<T, QPointer<QLabel>, QPointer<QWidget>>;

	auto extractQuotedText = [](const QString &text) -> QString {
		int start = text.indexOf('\'');
		if (start == -1)
			return text;
		int end = text.lastIndexOf('\'');
		if (end == -1 || end <= start)
			return text;
		return text.mid(start + 1, end - start - 1);
	};

	stable_sort(begin(hotkeys), end(hotkeys), [&](const tuple_type &a, const tuple_type &b) {
		const auto &o_a = get<0>(a);
		const auto &o_b = get<0>(b);
		if (o_a != o_b) {
			return string(getName(o_a)) < getName(o_b);
		} else if (bTwoColumns) {
			auto widget_a = qobject_cast<OBSHotkeyWidget *>(get<2>(a));
			auto widget_b = qobject_cast<OBSHotkeyWidget *>(get<2>(b));
			if (!widget_a || !widget_b)
				return false;
			if (!pls_is_sceneitem_hotkey_id(widget_a->id) || !pls_is_sceneitem_hotkey_id(widget_b->id))
				return false;

			auto label_a = get<1>(a);
			auto label_b = get<1>(b);
			if (!label_a || !label_b)
				return false;

			QString quoted_a = extractQuotedText(label_a->text());
			QString quoted_b = extractQuotedText(label_b->text());
			return quoted_a < quoted_b;
		}

		return false;
	});

	QFormLayout *hFormLayout = nullptr;
	QFormLayout *vFormLayout = nullptr;

	auto createInternalLayout = [&] {
		auto tabWidget = new QTabWidget();
		tabWidget->setStyleSheet(
			"QTabWidget::pane { padding-top: 0px; } QTabBar::tab:selected { padding-bottom: 7px; } QTabBar::tab:!selected { font-weight:bold; }");

		auto hFormWidget = new QWidget();
		hFormLayout = new QFormLayout(hFormWidget);
		hFormLayout->setContentsMargins(0, 20, 0, 0);
		hFormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		hFormLayout->setHorizontalSpacing(20);
		hFormLayout->setVerticalSpacing(10);

		auto vFormWidget = new QWidget();
		vFormLayout = new QFormLayout(vFormWidget);
		vFormLayout->setContentsMargins(0, 20, 0, 0);
		vFormLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		vFormLayout->setHorizontalSpacing(20);
		vFormLayout->setVerticalSpacing(10);

		tabWidget->addTab(hFormWidget, QTStr("DualOutput.Settings.Video.Horizontal"));
		tabWidget->addTab(vFormWidget, QTStr("DualOutput.Settings.Video.Vertical"));

		layout.addRow(tabWidget);
	};

	string prevName;
	for (const auto &hotkey : hotkeys) {
		const auto &o = get<0>(hotkey);
		const char *name = getName(o);
		if (prevName != name) {
			prevName = name;
			layout.setItem(layout.rowCount(), QFormLayout::SpanningRole, new QSpacerItem(0, 10));
			auto group = makeLabel(o, getName);
			group->setProperty("useFor", "QGroupBox");
			layout.addRow(group);

			if (bTwoColumns) {
				hFormLayout = nullptr;
				vFormLayout = nullptr;
			}
		}

		QFormLayout *formLayout = nullptr;
		if (bTwoColumns) {
			if (auto widget = qobject_cast<OBSHotkeyWidget *>(get<2>(hotkey)); nullptr != widget) {
				if (pls_is_sceneitem_hotkey_id(widget->id)) {
					if (pls_is_vertical_hotkey_id(widget->id)) {
						if (nullptr == vFormLayout) {
							createInternalLayout();
						}
						formLayout = vFormLayout;
					} else {
						if (nullptr == hFormLayout) {
							createInternalLayout();
						}
						formLayout = hFormLayout;
					}
				} else {
					formLayout = &layout;
				}
			} else {
				formLayout = &layout;
			}
		} else {
			formLayout = &layout;
		}

		auto hlabel = get<1>(hotkey);
		auto widget = get<2>(hotkey);
		hlabel->setProperty("useFor", "FormLabelRole");
		formLayout->addRow(hlabel, widget);
	}
}

void OBSBasicSettings::LoadHotkeySettings(obs_hotkey_id ignoreKey)
{
	if (!hotkeyPage) {
		return;
	}

	bReloadHotKey = false;

	hotkeyPage->hotkeyFormLayout->setRowVisible(hotkeyPage->pleaseWaitLabel, true);
	hotkeyPage->hotkeyScrollArea->ensureVisible(0, 0);

	hotkeys.clear();
	if (hotkeyPage->hotkeyFormLayout->rowCount() > 0) {
		QLayoutItem *forDeletion = hotkeyPage->hotkeyFormLayout->takeAt(0);
		if (forDeletion->widget() == hotkeyPage->pleaseWaitLabel) {
			QMetaObject::invokeMethod(
				this,
				[this] {
					hotkeyPage->hotkeyFormLayout->setRowVisible(hotkeyPage->pleaseWaitLabel, false);
				},
				Qt::QueuedConnection);
		} else {
			forDeletion->widget()->hide();
			forDeletion->widget()->deleteLater();
			delete forDeletion;
		}
	}
	hotkeyPage->hotkeyScrollFrame->repaint();

	hotkeyPage->hotkeyFilterSearch->blockSignals(true);
	hotkeyPage->hotkeyFilterInput->blockSignals(true);
	hotkeyPage->hotkeyFilterSearch->setText("");
	hotkeyPage->hotkeyFilterInput->ResetKey();
	hotkeyPage->hotkeyFilterSearch->blockSignals(false);
	hotkeyPage->hotkeyFilterInput->blockSignals(false);
	hotkeyPage->pushButton->setEnabled(false);
	hotkeyPage->hotkeyFilterReset->setEnabled(false);

	using keys_t = map<obs_hotkey_id, vector<obs_key_combination_t>>;
	keys_t keys;
	decltype(pls_enum_hotkey_bindings_all) *pEnumHotKeyBinding = obs_enum_hotkey_bindings;
	if (pls_is_dual_output_on()) {
		pEnumHotKeyBinding = pls_enum_hotkey_bindings_all;
	}
	pEnumHotKeyBinding(
		[](void *data, size_t, obs_hotkey_binding_t *binding) {
			auto &keys = *static_cast<keys_t *>(data);

			keys[obs_hotkey_binding_get_hotkey_id(binding)].emplace_back(
				obs_hotkey_binding_get_key_combination(binding));

			return true;
		},
		&keys);

	QFormLayout *hotkeysLayout = new QFormLayout();
	hotkeysLayout->setVerticalSpacing(10);
	hotkeysLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	hotkeysLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hotkeysLayout->setHorizontalSpacing(20);
	hotkeysLayout->setContentsMargins(0, 0, 0, 0);
	hotkeysLayout->setSizeConstraint(QLayout::SetMinimumSize);
	auto hotkeyChildWidget = new QWidget(hotkeyPage->hotkeyScrollContents);
	hotkeyChildWidget->setVisible(false);
	hotkeyChildWidget->setLayout(hotkeysLayout);

	using namespace std;
	using encoders_elem_t = tuple<OBSEncoder, QPointer<QLabel>, QPointer<QWidget>>;
	using outputs_elem_t = tuple<OBSOutput, QPointer<QLabel>, QPointer<QWidget>>;
	using services_elem_t = tuple<OBSService, QPointer<QLabel>, QPointer<QWidget>>;
	using sources_elem_t = tuple<OBSSource, QPointer<QLabel>, QPointer<QWidget>>;
	vector<encoders_elem_t> encoders;
	vector<outputs_elem_t> outputs;
	vector<services_elem_t> services;
	vector<sources_elem_t> scenes;
	vector<sources_elem_t> sources;

	vector<obs_hotkey_id> pairIds;
	map<obs_hotkey_id, pair<obs_hotkey_id, OBSHotkeyLabel *>> pairLabels;

	using std::move;

	auto HandleEncoder = [&](void *registerer, OBSHotkeyLabel *label, OBSHotkeyWidget *hw) {
		auto weak_encoder = static_cast<obs_weak_encoder_t *>(registerer);
		auto encoder = OBSGetStrongRef(weak_encoder);

		if (!encoder)
			return true;

		encoders.emplace_back(std::move(encoder), label, hw);
		return false;
	};

	auto HandleOutput = [&](void *registerer, OBSHotkeyLabel *label, OBSHotkeyWidget *hw) {
		auto weak_output = static_cast<obs_weak_output_t *>(registerer);
		auto output = OBSGetStrongRef(weak_output);

		if (!output)
			return true;

		outputs.emplace_back(std::move(output), label, hw);
		return false;
	};

	auto HandleService = [&](void *registerer, OBSHotkeyLabel *label, OBSHotkeyWidget *hw) {
		auto weak_service = static_cast<obs_weak_service_t *>(registerer);
		auto service = OBSGetStrongRef(weak_service);

		if (!service)
			return true;

		services.emplace_back(std::move(service), label, hw);
		return false;
	};

	auto HandleSource = [&](void *registerer, OBSHotkeyLabel *label, OBSHotkeyWidget *hw, obs_hotkey_t *key) {
		auto weak_source = static_cast<obs_weak_source_t *>(registerer);
		auto source = OBSGetStrongRef(weak_source);

		if (!source)
			return true;

		auto pScene = obs_scene_from_source(source);
		if (nullptr != pScene && pls_is_vertical_scene(pScene)) {
			label->hide();
			hw->hide();
			return true;
		}

		if (obs_obj_is_private(source)) {
			label->hide();
			hw->hide();
			return true;
		}
		if (IgnoreInvisibleHotkeys(source, obs_hotkey_get_name(key))) {
			label->hide();
			hw->hide();
			return true;
		}

		if (obs_scene_from_source(source))
			scenes.emplace_back(source, label, hw);
		else if (obs_source_get_name(source) != NULL)
			sources.emplace_back(source, label, hw);

		return false;
	};

	auto RegisterHotkey = [&](obs_hotkey_t *key, OBSHotkeyLabel *label, OBSHotkeyWidget *hw) {
		auto registerer_type = obs_hotkey_get_registerer_type(key);
		void *registerer = obs_hotkey_get_registerer(key);

		obs_hotkey_id partner = obs_hotkey_get_pair_partner_id(key);
		if (partner != OBS_INVALID_HOTKEY_ID) {
			pairLabels.emplace(obs_hotkey_get_id(key), make_pair(partner, label));
			pairIds.push_back(obs_hotkey_get_id(key));
		}
		label->setProperty("useFor", "FormLabelRole");
		using std::move;

		switch (registerer_type) {
		case OBS_HOTKEY_REGISTERER_FRONTEND:
			hotkeysLayout->addRow(label, hw);
			break;

		case OBS_HOTKEY_REGISTERER_ENCODER:
			if (HandleEncoder(registerer, label, hw))
				return;
			break;

		case OBS_HOTKEY_REGISTERER_OUTPUT:
			if (HandleOutput(registerer, label, hw))
				return;
			break;

		case OBS_HOTKEY_REGISTERER_SERVICE:
			if (HandleService(registerer, label, hw))
				return;
			break;

		case OBS_HOTKEY_REGISTERER_SOURCE:
			if (HandleSource(registerer, label, hw, key))
				return;
			break;
		}

		hotkeys.emplace_back(registerer_type == OBS_HOTKEY_REGISTERER_FRONTEND, hw);
		connect(hw, &OBSHotkeyWidget::KeyChanged, this, [=]() {
			HotkeysChanged();
			ScanDuplicateHotkeys(hotkeysLayout);
		});
		connect(hw, &OBSHotkeyWidget::SearchKey, [=](obs_key_combination_t combo) {
			hotkeyPage->hotkeyFilterSearch->setText("");
			hotkeyPage->hotkeyFilterInput->HandleNewKey(combo);
			hotkeyPage->hotkeyFilterInput->KeyChanged(combo);
		});
	};

	auto data = make_tuple(RegisterHotkey, std::move(keys), ignoreKey, this);
	using data_t = decltype(data);
	decltype(pls_enum_hotkeys_all) *penumHotkey = obs_enum_hotkeys;
	if (pls_is_dual_output_on()) {
		penumHotkey = pls_enum_hotkeys_all;
	}
	penumHotkey(
		[](void *data, obs_hotkey_id id, obs_hotkey_t *key) {
			data_t &d = *static_cast<data_t *>(data);
			if (id != get<2>(d))
				LayoutHotkey(get<3>(d), id, key, get<0>(d), get<1>(d));
			return true;
		},
		&data);

	for (auto keyId : pairIds) {
		auto data1 = pairLabels.find(keyId);
		if (data1 == end(pairLabels))
			continue;

		auto &label1 = data1->second.second;
		if (label1->pairPartner)
			continue;

		auto data2 = pairLabels.find(data1->second.first);
		if (data2 == end(pairLabels))
			continue;

		auto &label2 = data2->second.second;
		if (label2->pairPartner)
			continue;

		QString tt = QTStr("Basic.Settings.Hotkeys.Pair");
		auto name1 = label1->text();
		auto name2 = label2->text();

		auto Update = [&](OBSHotkeyLabel *label, const QString &name, OBSHotkeyLabel *other,
				  const QString &otherName) {
			QString string = other->property("fullName").value<QString>();

			if (string.isEmpty() || string.isNull())
				string = otherName;

			label->setToolTip(tt.arg(string));
			label->setText(name + " *");
			label->pairPartner = other;
		};
		Update(label1, name1, label2, name2);
		Update(label2, name2, label1, name1);
	}

	AddHotkeys(*hotkeysLayout, obs_output_get_name, outputs);
	AddHotkeys(*hotkeysLayout, obs_source_get_name, scenes, pls_is_dual_output_on());
	AddHotkeys(*hotkeysLayout, obs_source_get_name, sources, pls_is_dual_output_on());
	AddHotkeys(*hotkeysLayout, obs_encoder_get_name, encoders);
	AddHotkeys(*hotkeysLayout, obs_service_get_name, services);

	ScanDuplicateHotkeys(hotkeysLayout);
	/* After this function returns the UI can still be unresponsive for a bit.
	 * So by deferring the call to unsetCursor() to the Qt event loop it will
	 * take until it has actually finished processing the created widgets
	 * before the cursor is reset. */
	QTimer::singleShot(1, this, [this]() {
		PLS_INFO("setting", "singleShot unsetCursor");
		unsetCursor();
	});
	hotkeysLoaded = true;
	hotkeysLayout->update();

	hotkeyPage->hotkeyFormLayout->addRow(hotkeyChildWidget);
	pls_async_call(hotkeyChildWidget, [this, hotkeyChildWidget]() {
		hotkeyPage->hotkeyScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
		auto scrollBar = hotkeyPage->hotkeyScrollArea->verticalScrollBar();
		scrollBar->setVisible(false);

		hotkeyChildWidget->setVisible(true);
		pls_async_call(scrollBar, [scrollBar]() { scrollBar->setVisible(true); });
	});
}

void OBSBasicSettings::LoadSettings(bool changedOnly)
{
	if (!changedOnly || generalChanged)
		LoadGeneralSettings();
	if (!changedOnly || stream1Changed)
		LoadStream1Settings();
	if (!changedOnly || outputsChanged)
		LoadOutputSettings();
	if (!changedOnly || audioChanged)
		LoadAudioSettings();
	if (!changedOnly || videoChanged)
		LoadVideoSettings();
	if (!changedOnly || a11yChanged)
		LoadA11ySettings();
	if (!changedOnly || advancedChanged)
		LoadAdvancedSettings();
}

void OBSBasicSettings::SaveGeneralSettings()
{
	if (!generalPage) {
		return;
	}

	int languageIndex = generalPage->language->currentIndex();
	QVariant langData = generalPage->language->itemData(languageIndex);
	string language = langData.toString().toStdString();

	if (WidgetChanged(generalPage->language)) {
		config_set_string(App()->GetUserConfig(), "General", "Language", language.c_str());
		pls_set_locale(QString::fromStdString(language));
	}

#ifdef _WIN32
	if (generalPage->hideOBSFromCapture && WidgetChanged(generalPage->hideOBSFromCapture)) {
		bool hide_window = generalPage->hideOBSFromCapture->isChecked();
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "HideOBSWindowsFromCapture", hide_window);

		QWindowList windows = QGuiApplication::allWindows();
		for (auto window : windows) {
			if (window->isVisible()) {
				main->SetDisplayAffinity(window);
			}
		}

		blog(LOG_INFO, "Hide OBS windows from screen capture: %s", hide_window ? "true" : "false");
	}
#endif

	if (WidgetChanged(generalPage->watermarkCheckBox)) {
		bool value = generalPage->watermarkCheckBox->isChecked();
		config_set_bool(App()->GetUserConfig(), "General", "Watermark", value);
	}

	if (WidgetChanged(generalPage->snappingEnabled))
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "SnappingEnabled",
				generalPage->snappingEnabled->isChecked());
	if (WidgetChanged(generalPage->screenSnapping))
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "ScreenSnapping",
				generalPage->screenSnapping->isChecked());
	if (WidgetChanged(generalPage->centerSnapping))
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "CenterSnapping",
				generalPage->centerSnapping->isChecked());
	if (WidgetChanged(generalPage->sourceSnapping))
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "SourceSnapping",
				generalPage->sourceSnapping->isChecked());
	if (WidgetChanged(generalPage->snapDistance))
		config_set_double(App()->GetUserConfig(), "BasicWindow", "SnapDistance",
				  generalPage->snapDistance->value());
	if (WidgetChanged(generalPage->overflowAlwaysVisible) || WidgetChanged(generalPage->overflowHide) ||
	    WidgetChanged(generalPage->overflowSelectionHide)) {
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "OverflowAlwaysVisible",
				generalPage->overflowAlwaysVisible->isChecked());
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "OverflowHidden",
				generalPage->overflowHide->isChecked());
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "OverflowSelectionHidden",
				generalPage->overflowSelectionHide->isChecked());
		main->UpdatePreviewOverflowSettings();
	}
	if (WidgetChanged(generalPage->previewSafeAreas)) {
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "ShowSafeAreas",
				generalPage->previewSafeAreas->isChecked());
		main->UpdatePreviewSafeAreas();
	}

	if (WidgetChanged(generalPage->previewSpacingHelpers)) {
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "SpacingHelpersEnabled",
				generalPage->previewSpacingHelpers->isChecked());
		main->UpdatePreviewSpacingHelpers();
	}

	if (WidgetChanged(generalPage->previewZoomEnabled)) {
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "PreviewZoomEnabled",
				generalPage->previewZoomEnabled->isChecked());
		main->UpdatePreviewZoomEnabled();
	}

	if (WidgetChanged(generalPage->doubleClickSwitch))
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "TransitionOnDoubleClick",
				generalPage->doubleClickSwitch->isChecked());

	if (WidgetChanged(generalPage->hideProjectorCursor)) {
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "HideProjectorCursor",
				generalPage->hideProjectorCursor->isChecked());
		main->UpdateProjectorHideCursor();
	}

	if (WidgetChanged(generalPage->projectorAlwaysOnTop)) {
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "ProjectorAlwaysOnTop",
				generalPage->projectorAlwaysOnTop->isChecked());
#if defined(_WIN32) || defined(__APPLE__)
		main->UpdateProjectorAlwaysOnTop(generalPage->projectorAlwaysOnTop->isChecked());
#else
		main->ResetProjectors();
#endif
	}

	if (WidgetChanged(generalPage->recordWhenStreaming))
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "RecordWhenStreaming",
				generalPage->recordWhenStreaming->isChecked());
	if (WidgetChanged(generalPage->keepRecordStreamStops))
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "KeepRecordingWhenStreamStops",
				generalPage->keepRecordStreamStops->isChecked());

	if (WidgetChanged(generalPage->replayWhileStreaming))
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "ReplayBufferWhileStreaming",
				generalPage->replayWhileStreaming->isChecked());
	if (WidgetChanged(generalPage->keepReplayStreamStops))
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "KeepReplayBufferStreamStops",
				generalPage->keepReplayStreamStops->isChecked());

	if (WidgetChanged(generalPage->systemTrayEnabled)) {
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "SysTrayEnabled",
				generalPage->systemTrayEnabled->isChecked());

		main->SystemTray(false);
	}

	if (WidgetChanged(generalPage->systemTrayWhenStarted))
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "SysTrayWhenStarted",
				generalPage->systemTrayWhenStarted->isChecked());

	if (WidgetChanged(generalPage->systemTrayAlways))
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "SysTrayMinimizeToTray",
				generalPage->systemTrayAlways->isChecked());

	if (WidgetChanged(generalPage->saveProjectors))
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "SaveProjectors",
				generalPage->saveProjectors->isChecked());

	if (WidgetChanged(generalPage->closeProjectors))
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "CloseExistingProjectors",
				generalPage->closeProjectors->isChecked());

	if (WidgetChanged(generalPage->studioPortraitLayout)) {
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "StudioPortraitLayout",
				generalPage->studioPortraitLayout->isChecked());

		main->ResetUI();
	}

	if (WidgetChanged(generalPage->prevProgLabelToggle)) {
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "StudioModeLabels",
				generalPage->prevProgLabelToggle->isChecked());

		main->ResetUI();
	}

	bool multiviewChanged = false;
	if (WidgetChanged(generalPage->multiviewMouseSwitch)) {
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "MultiviewMouseSwitch",
				generalPage->multiviewMouseSwitch->isChecked());
		multiviewChanged = true;
	}

	if (WidgetChanged(generalPage->multiviewDrawNames)) {
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "MultiviewDrawNames",
				generalPage->multiviewDrawNames->isChecked());
		multiviewChanged = true;
	}

	if (WidgetChanged(generalPage->multiviewDrawAreas)) {
		config_set_bool(App()->GetUserConfig(), "BasicWindow", "MultiviewDrawAreas",
				generalPage->multiviewDrawAreas->isChecked());
		multiviewChanged = true;
	}

	if (WidgetChanged(generalPage->multiviewLayout)) {
		config_set_int(App()->GetUserConfig(), "BasicWindow", "MultiviewLayout",
			       generalPage->multiviewLayout->currentData().toInt());
		multiviewChanged = true;
	}

	if (multiviewChanged)
		OBSProjector::UpdateMultiviewProjectors();
}

void OBSBasicSettings::SaveVideoSettings()
{
	if (!videoPage) {
		return;
	}

	QString baseResolution = videoPage->baseResolution->currentText();
	QString outputResolution = videoPage->outputResolution->currentText();
	int fpsType = videoPage->fpsType->currentIndex();
	uint32_t cx = 0, cy = 0;

	/* ------------------- */

	if (WidgetChanged(videoPage->baseResolution) && ConvertResText(QT_TO_UTF8(baseResolution), cx, cy)) {
		config_set_uint(main->Config(), "Video", "BaseCX", cx);
		config_set_uint(main->Config(), "Video", "BaseCY", cy);
	}

	if (WidgetChanged(videoPage->outputResolution) && ConvertResText(QT_TO_UTF8(outputResolution), cx, cy)) {
		config_set_uint(main->Config(), "Video", "OutputCX", cx);
		config_set_uint(main->Config(), "Video", "OutputCY", cy);
	}

	if (WidgetChanged(videoPage->fpsType))
		config_set_uint(main->Config(), "Video", "FPSType", fpsType);

	SaveCombo(videoPage->fpsCommon, "Video", "FPSCommon");
	SaveSpinBox(videoPage->fpsInteger, "Video", "FPSInt");
	SaveSpinBox(videoPage->fpsNumerator, "Video", "FPSNum");
	SaveSpinBox(videoPage->fpsDenominator, "Video", "FPSDen");
	SaveComboData(videoPage->downscaleFilter, "Video", "ScaleType");

	SaveVerticalVideoSettings();
}

void OBSBasicSettings::SaveVerticalVideoSettings()
{
	QString baseResolution = videoPage->baseResolution_2->currentText();
	QString outputResolution = videoPage->outputResolution_2->currentText();
	uint32_t cx = 0, cy = 0;

	if (WidgetChanged(videoPage->baseResolution_2) && ConvertResText(QT_TO_UTF8(baseResolution), cx, cy)) {
		config_set_uint(main->Config(), "Video", "BaseCXV", cx);
		config_set_uint(main->Config(), "Video", "BaseCYV", cy);
	}

	if (WidgetChanged(videoPage->outputResolution_2) && ConvertResText(QT_TO_UTF8(outputResolution), cx, cy)) {
		config_set_uint(main->Config(), "Video", "OutputCXV", cx);
		config_set_uint(main->Config(), "Video", "OutputCYV", cy);
	}
}

void OBSBasicSettings::SaveAdvancedSettings()
{
	if (!advancedPage) {
		return;
	}

#ifdef _WIN32
	if (WidgetChanged(advancedPage->renderer))
		config_set_string(App()->GetAppConfig(), "Video", "Renderer",
				  QT_TO_UTF8(advancedPage->renderer->currentText()));

	std::string priority = QT_TO_UTF8(advancedPage->processPriority->currentData().toString());
	config_set_string(App()->GetAppConfig(), "General", "ProcessPriority", priority.c_str());
	if (main->Active())
		SetProcessPriority(priority.c_str());

	SaveCheckBox(advancedPage->enableNewSocketLoop, "Output", "NewSocketLoopEnable");
	SaveCheckBox(advancedPage->enableLowLatencyMode, "Output", "LowLatencyEnable");
#endif
#if defined(_WIN32) || defined(__APPLE__)
	bool browserHWAccel = advancedPage->browserHWAccel->isChecked();
	config_set_bool(App()->GetAppConfig(), "General", "BrowserHWAccel", browserHWAccel);
#endif

	if (WidgetChanged(advancedPage->hotkeyFocusType)) {
		QString str = GetComboData(advancedPage->hotkeyFocusType);
		config_set_string(App()->GetUserConfig(), "General", "HotkeyFocusType", QT_TO_UTF8(str));
	}

#ifdef __APPLE__
	if (WidgetChanged(advancedPage->disableOSXVSync)) {
		bool disable = advancedPage->disableOSXVSync->isChecked();
		config_set_bool(App()->GetAppConfig(), "Video", "DisableOSXVSync", disable);
		EnableOSXVSync(!disable);
	}
	if (WidgetChanged(advancedPage->resetOSXVSync))
		config_set_bool(App()->GetAppConfig(), "Video", "ResetOSXVSyncOnExit",
				advancedPage->resetOSXVSync->isChecked());
#endif

	SaveComboData(advancedPage->colorFormat, "Video", "ColorFormat");
	SaveComboData(advancedPage->colorSpace, "Video", "ColorSpace");
	SaveComboData(advancedPage->colorRange, "Video", "ColorRange");
	SaveSpinBox(advancedPage->sdrWhiteLevel, "Video", "SdrWhiteLevel");
	SaveSpinBox(advancedPage->hdrNominalPeakLevel, "Video", "HdrNominalPeakLevel");

	if (WidgetChanged(advancedPage->confirmOnExit))
		config_set_bool(App()->GetUserConfig(), "General", "ConfirmOnExit",
				advancedPage->confirmOnExit->isChecked());

	SaveEdit(advancedPage->filenameFormatting, "Output", "FilenameFormatting");
	SaveEdit(advancedPage->simpleRBPrefix, "SimpleOutput", "RecRBPrefix");
	SaveEdit(advancedPage->simpleRBSuffix, "SimpleOutput", "RecRBSuffix");
	SaveCheckBox(advancedPage->overwriteIfExists, "Output", "OverwriteIfExists");
	SaveCheckBox(advancedPage->streamDelayEnable, "Output", "DelayEnable");
	SaveSpinBox(advancedPage->streamDelaySec, "Output", "DelaySec");
	SaveCheckBox(advancedPage->streamDelayPreserve, "Output", "DelayPreserve");
	SaveCheckBox(advancedPage->reconnectEnable, "Output", "Reconnect");
	SaveSpinBox(advancedPage->reconnectRetryDelay, "Output", "RetryDelay");
	SaveSpinBox(advancedPage->reconnectMaxRetries, "Output", "MaxRetries");
	SaveComboData(advancedPage->bindToIP, "Output", "BindIP");
	SaveComboData(advancedPage->ipFamily, "Output", "IPFamily");
	SaveCheckBox(advancedPage->autoRemux, "Video", "AutoRemux");
	SaveCheckBox(advancedPage->dynBitrate, "Output", "DynamicBitrate");
}

static inline const char *OutputModeFromIdx(int idx)
{
	if (idx == 1)
		return "Advanced";
	else
		return "Simple";
}

static inline const char *RecTypeFromIdx(int idx)
{
	if (idx == 1)
		return "FFmpeg";
	else
		return "Standard";
}

static inline const char *SplitFileTypeFromIdx(int idx)
{
	if (idx == 1)
		return "Size";
	else if (idx == 2)
		return "Manual";
	else
		return "Time";
}

void removeJsonData(const char *path)
{
	std::array<char, 512> full_path;
	int ret = OBSBasic::Get()->GetProfilePath(full_path.data(), sizeof(full_path), path);
	if (ret > 0) {
		QFile::remove(full_path.data());
	}
}

static void WriteJsonData(OBSPropertiesView *view, const char *path)
{
	char full_path[512];

	if (!view || !WidgetChanged(view))
		return;

	int ret = GetProfilePath(full_path, sizeof(full_path), path);
	if (ret > 0) {
		obs_data_t *settings = view->GetSettings();
		if (settings) {
			obs_data_save_json_safe(settings, full_path, "tmp", "bak");
		}
	}
}

static void SaveTrackIndex(config_t *config, const char *section, const char *name, PLSRadioButton *check1,
			   PLSRadioButton *check2, PLSRadioButton *check3, PLSRadioButton *check4,
			   PLSRadioButton *check5, PLSRadioButton *check6)
{
	if (check1->isChecked())
		config_set_int(config, section, name, 1);
	else if (check2->isChecked())
		config_set_int(config, section, name, 2);
	else if (check3->isChecked())
		config_set_int(config, section, name, 3);
	else if (check4->isChecked())
		config_set_int(config, section, name, 4);
	else if (check5->isChecked())
		config_set_int(config, section, name, 5);
	else if (check6->isChecked())
		config_set_int(config, section, name, 6);
}

void OBSBasicSettings::SaveFormat(QComboBox *combo)
{
	QVariant v = combo->currentData();
	if (!v.isNull()) {
		auto format = v.value<FFmpegFormat>();
		config_set_string(main->Config(), "AdvOut", "FFFormat", format.name);
		config_set_string(main->Config(), "AdvOut", "FFFormatMimeType", format.mime_type);

		const char *ext = format.extensions;
		string extStr = ext ? ext : "";

		char *comma = strchr(&extStr[0], ',');
		if (comma)
			*comma = 0;

		config_set_string(main->Config(), "AdvOut", "FFExtension", extStr.c_str());
	} else {
		config_set_string(main->Config(), "AdvOut", "FFFormat", nullptr);
		config_set_string(main->Config(), "AdvOut", "FFFormatMimeType", nullptr);

		config_remove_value(main->Config(), "AdvOut", "FFExtension");
	}
}

void OBSBasicSettings::SaveEncoder(QComboBox *combo, const char *section, const char *value)
{
	QVariant v = combo->currentData();
	FFmpegCodec cd{};
	if (!v.isNull())
		cd = v.value<FFmpegCodec>();

	config_set_int(main->Config(), section, QT_TO_UTF8(QString("%1Id").arg(value)), cd.id);
	if (cd.id != 0)
		config_set_string(main->Config(), section, value, cd.name);
	else
		config_set_string(main->Config(), section, value, nullptr);
}

void OBSBasicSettings::SaveOutputSettings()
{
	if (!outputPage) {
		return;
	}

	config_set_string(main->Config(), "Output", "Mode", OutputModeFromIdx(outputPage->outputMode->currentIndex()));

	if (outputSimplePage) {
		QString encoder = outputSimplePage->simpleOutStrEncoder->currentData().toString();
		const char *presetType;

		if (encoder == SIMPLE_ENCODER_QSV)
			presetType = "QSVPreset";
		else if (encoder == SIMPLE_ENCODER_QSV_AV1)
			presetType = "QSVPreset";
		else if (encoder == SIMPLE_ENCODER_NVENC)
			presetType = "NVENCPreset2";
		else if (encoder == SIMPLE_ENCODER_NVENC_AV1)
			presetType = "NVENCPreset2";
#ifdef ENABLE_HEVC
		else if (encoder == SIMPLE_ENCODER_AMD_HEVC)
			presetType = "AMDPreset";
		else if (encoder == SIMPLE_ENCODER_NVENC_HEVC)
			presetType = "NVENCPreset2";
#endif
		else if (encoder == SIMPLE_ENCODER_AMD)
			presetType = "AMDPreset";
		else if (encoder == SIMPLE_ENCODER_AMD_AV1)
			presetType = "AMDAV1Preset";
		else if (encoder == SIMPLE_ENCODER_APPLE_H264
#ifdef ENABLE_HEVC
			 || encoder == SIMPLE_ENCODER_APPLE_HEVC
#endif
		)
			/* The Apple encoders don't have presets like the other encoders
         do. This only exists to make sure that the x264 preset doesn't
         get overwritten with empty data. */
			presetType = "ApplePreset";
		else
			presetType = "Preset";

		SaveSpinBox(outputSimplePage->simpleOutputVBitrate, "SimpleOutput", "VBitrate");
		SaveComboData(outputSimplePage->simpleOutStrEncoder, "SimpleOutput", "StreamEncoder");
		if (int idx = outputSimplePage->simpleOutStrAEncoder->currentIndex(); idx != -1) {
			SaveComboData(outputSimplePage->simpleOutStrAEncoder, "SimpleOutput", "StreamAudioEncoder");
		}
		SaveCombo(outputSimplePage->simpleOutputABitrate, "SimpleOutput", "ABitrate");
		SaveEdit(outputSimplePage->simpleOutputPath, "SimpleOutput", "FilePath");
		SaveCheckBox(outputSimplePage->simpleNoSpace, "SimpleOutput", "FileNameWithoutSpace");
		SaveComboData(outputSimplePage->simpleOutRecFormat, "SimpleOutput", "RecFormat2");
		SaveCheckBox(outputSimplePage->simpleOutAdvanced, "SimpleOutput", "UseAdvanced");
		SaveComboData(outputSimplePage->simpleOutPreset, "SimpleOutput", presetType);
		SaveEdit(outputSimplePage->simpleOutCustom, "SimpleOutput", "x264Settings");
		SaveComboData(outputSimplePage->simpleOutRecQuality, "SimpleOutput", "RecQuality");
		SaveComboData(outputSimplePage->simpleOutRecEncoder, "SimpleOutput", "RecEncoder");
		SaveComboData(outputSimplePage->simpleOutRecAEncoder, "SimpleOutput", "RecAudioEncoder");
		SaveEdit(outputSimplePage->simpleOutMuxCustom, "SimpleOutput", "MuxerCustom");
		SaveGroupBox(outputSimplePage->simpleReplayBuf, "SimpleOutput", "RecRB");
		SaveSpinBox(outputSimplePage->simpleRBSecMax, "SimpleOutput", "RecRBTime");
		SaveSpinBox(outputSimplePage->simpleRBMegsMax, "SimpleOutput", "RecRBSize");
		config_set_int(main->Config(), "SimpleOutput", "RecTracks", SimpleOutGetSelectedAudioTracks());
	}

	if (outputStreamPage) {
		curAdvStreamEncoder = GetComboData(outputStreamPage->advOutEncoder);

		SaveComboData(outputStreamPage->advOutEncoder, "AdvOut", "Encoder");

		if (int idx = outputStreamPage->advOutAEncoder->currentIndex(); idx != -1) {
			SaveComboData(outputStreamPage->advOutAEncoder, "AdvOut", "AudioEncoder");
		}
		SaveCombo(outputStreamPage->advOutRescale, "AdvOut", "RescaleRes");
		SaveComboData(outputStreamPage->advOutRescaleFilter, "AdvOut", "RescaleFilter");
		SaveTrackIndex(main->Config(), "AdvOut", "TrackIndex", outputStreamPage->advOutTrack1,
			       outputStreamPage->advOutTrack2, outputStreamPage->advOutTrack3,
			       outputStreamPage->advOutTrack4, outputStreamPage->advOutTrack5,
			       outputStreamPage->advOutTrack6);
		SaveTrackIndex(main->Config(), "AdvOut", "TrackIndexV", outputStreamPage->advOutTrack1_2,
			       outputStreamPage->advOutTrack2_2, outputStreamPage->advOutTrack3_2,
			       outputStreamPage->advOutTrack4_2, outputStreamPage->advOutTrack5_2,
			       outputStreamPage->advOutTrack6_2);
		config_set_int(main->Config(), "AdvOut", "StreamMultiTrackAudioMixes",
			       AdvOutGetStreamingSelectedAudioTracks());
	}

	if (outputRecordPage) {
		config_set_string(main->Config(), "AdvOut", "RecType",
				  RecTypeFromIdx(outputRecordPage->advOutRecType->currentIndex()));

		curAdvRecordEncoder = GetComboData(outputRecordPage->advOutRecEncoder);

		SaveEdit(outputRecordPage->advOutRecPath, "AdvOut", "RecFilePath");
		SaveCheckBox(outputRecordPage->advOutNoSpace, "AdvOut", "RecFileNameWithoutSpace");
		SaveComboData(outputRecordPage->advOutRecFormat, "AdvOut", "RecFormat2");
		SaveComboData(outputRecordPage->advOutRecEncoder, "AdvOut", "RecEncoder");
		SaveComboData(outputRecordPage->advOutRecAEncoder, "AdvOut", "RecAudioEncoder");

		SaveCombo(outputRecordPage->advOutRecRescale, "AdvOut", "RecRescaleRes");
		SaveComboData(outputRecordPage->advOutRecRescaleFilter, "AdvOut", "RecRescaleFilter");
		SaveEdit(outputRecordPage->advOutMuxCustom, "AdvOut", "RecMuxerCustom");
		SaveCheckBox(outputRecordPage->advOutSplitFile, "AdvOut", "RecSplitFile");
		config_set_string(main->Config(), "AdvOut", "RecSplitFileType",
				  SplitFileTypeFromIdx(outputRecordPage->advOutSplitFileType->currentIndex()));
		SaveSpinBox(outputRecordPage->advOutSplitFileTime, "AdvOut", "RecSplitFileTime");
		SaveSpinBox(outputRecordPage->advOutSplitFileSize, "AdvOut", "RecSplitFileSize");

		config_set_int(main->Config(), "AdvOut", "RecTracks", AdvOutGetSelectedAudioTracks());

		config_set_int(main->Config(), "AdvOut", "FLVTrack", CurrentFLVTrack());

		config_set_bool(main->Config(), "AdvOut", "FFOutputToFile",
				outputRecordPage->advOutFFType->currentIndex() == 0 ? true : false);
		SaveEdit(outputRecordPage->advOutFFRecPath, "AdvOut", "FFFilePath");
		SaveCheckBox(outputRecordPage->advOutFFNoSpace, "AdvOut", "FFFileNameWithoutSpace");
		SaveEdit(outputRecordPage->advOutFFURL, "AdvOut", "FFURL");
		SaveFormat(outputRecordPage->advOutFFFormat);
		SaveEdit(outputRecordPage->advOutFFMCfg, "AdvOut", "FFMCustom");
		SaveSpinBox(outputRecordPage->advOutFFVBitrate, "AdvOut", "FFVBitrate");
		SaveSpinBox(outputRecordPage->advOutFFVGOPSize, "AdvOut", "FFVGOPSize");
		SaveCheckBox(outputRecordPage->advOutFFUseRescale, "AdvOut", "FFRescale");
		SaveCheckBox(outputRecordPage->advOutFFIgnoreCompat, "AdvOut", "FFIgnoreCompat");
		SaveCombo(outputRecordPage->advOutFFRescale, "AdvOut", "FFRescaleRes");
		SaveEncoder(outputRecordPage->advOutFFVEncoder, "AdvOut", "FFVEncoder");
		SaveEdit(outputRecordPage->advOutFFVCfg, "AdvOut", "FFVCustom");
		SaveSpinBox(outputRecordPage->advOutFFABitrate, "AdvOut", "FFABitrate");
		SaveEncoder(outputRecordPage->advOutFFAEncoder, "AdvOut", "FFAEncoder");
		SaveEdit(outputRecordPage->advOutFFACfg, "AdvOut", "FFACustom");
		config_set_int(main->Config(), "AdvOut", "FFAudioMixes",
			       (outputRecordPage->advOutFFTrack1->isChecked() ? (1 << 0) : 0) |
				       (outputRecordPage->advOutFFTrack2->isChecked() ? (1 << 1) : 0) |
				       (outputRecordPage->advOutFFTrack3->isChecked() ? (1 << 2) : 0) |
				       (outputRecordPage->advOutFFTrack4->isChecked() ? (1 << 3) : 0) |
				       (outputRecordPage->advOutFFTrack5->isChecked() ? (1 << 4) : 0) |
				       (outputRecordPage->advOutFFTrack6->isChecked() ? (1 << 5) : 0));
	}

	if (outputAudioPage) {
		SaveCombo(outputAudioPage->advOutTrack1Bitrate, "AdvOut", "Track1Bitrate");
		SaveCombo(outputAudioPage->advOutTrack2Bitrate, "AdvOut", "Track2Bitrate");
		SaveCombo(outputAudioPage->advOutTrack3Bitrate, "AdvOut", "Track3Bitrate");
		SaveCombo(outputAudioPage->advOutTrack4Bitrate, "AdvOut", "Track4Bitrate");
		SaveCombo(outputAudioPage->advOutTrack5Bitrate, "AdvOut", "Track5Bitrate");
		SaveCombo(outputAudioPage->advOutTrack6Bitrate, "AdvOut", "Track6Bitrate");
		SaveEdit(outputAudioPage->advOutTrack1Name, "AdvOut", "Track1Name");
		SaveEdit(outputAudioPage->advOutTrack2Name, "AdvOut", "Track2Name");
		SaveEdit(outputAudioPage->advOutTrack3Name, "AdvOut", "Track3Name");
		SaveEdit(outputAudioPage->advOutTrack4Name, "AdvOut", "Track4Name");
		SaveEdit(outputAudioPage->advOutTrack5Name, "AdvOut", "Track5Name");
		SaveEdit(outputAudioPage->advOutTrack6Name, "AdvOut", "Track6Name");
	}

	if (simpleVodTrack) {
		SaveCheckBox(simpleVodTrack, "SimpleOutput", "VodTrackEnabled");
	}
	if (vodTrackCheckbox) {
		SaveCheckBox(vodTrackCheckbox, "AdvOut", "VodTrackEnabled");
		SaveTrackIndex(main->Config(), "AdvOut", "VodTrackIndex", vodTrack[0], vodTrack[1], vodTrack[2],
			       vodTrack[3], vodTrack[4], vodTrack[5]);
	}

	if (outputReplayPage) {
		SaveCheckBox(outputReplayPage->advReplayBuf, "AdvOut", "RecRB");
		SaveSpinBox(outputReplayPage->advRBSecMax, "AdvOut", "RecRBTime");
		SaveSpinBox(outputReplayPage->advRBMegsMax, "AdvOut", "RecRBSize");
	}

	WriteJsonData(streamEncoderProps, "streamEncoder.json");
	WriteJsonData(recordEncoderProps, "recordEncoder.json");
	main->ResetOutputs();
}

void OBSBasicSettings::SaveAudioSettings()
{
	if (!audioPage) {
		return;
	}

	QString lastMonitoringDevice = config_get_string(main->Config(), "Audio", "MonitoringDeviceId");

	QString sampleRateStr = audioPage->sampleRate->currentText();
	int channelSetupIdx = audioPage->channelSetup->currentIndex();

	const char *channelSetup;
	switch (channelSetupIdx) {
	case 0:
		channelSetup = "Mono";
		break;
	case 1:
		channelSetup = "Stereo";
		break;
	case 2:
		channelSetup = "2.1";
		break;
	case 3:
		channelSetup = "4.0";
		break;
	case 4:
		channelSetup = "4.1";
		break;
	case 5:
		channelSetup = "5.1";
		break;
	case 6:
		channelSetup = "7.1";
		break;

	default:
		channelSetup = "Stereo";
		break;
	}

	int sampleRate = 44100;
	if (sampleRateStr == "48 kHz")
		sampleRate = 48000;

	if (WidgetChanged(audioPage->sampleRate))
		config_set_uint(main->Config(), "Audio", "SampleRate", sampleRate);

	if (WidgetChanged(audioPage->channelSetup))
		config_set_string(main->Config(), "Audio", "ChannelSetup", channelSetup);

	if (WidgetChanged(audioPage->meterDecayRate)) {
		double meterDecayRate;
		switch (audioPage->meterDecayRate->currentIndex()) {
		case 0:
			meterDecayRate = VOLUME_METER_DECAY_FAST;
			break;
		case 1:
			meterDecayRate = VOLUME_METER_DECAY_MEDIUM;
			break;
		case 2:
			meterDecayRate = VOLUME_METER_DECAY_SLOW;
			break;
		default:
			meterDecayRate = VOLUME_METER_DECAY_FAST;
			break;
		}
		config_set_double(main->Config(), "Audio", "MeterDecayRate", meterDecayRate);

		main->UpdateVolumeControlsDecayRate();
	}

	if (WidgetChanged(audioPage->peakMeterType)) {
		uint32_t peakMeterTypeIdx = audioPage->peakMeterType->currentIndex();
		config_set_uint(main->Config(), "Audio", "PeakMeterType", peakMeterTypeIdx);

		main->UpdateVolumeControlsPeakMeterType();
	}

	if (WidgetChanged(audioPage->lowLatencyBuffering)) {
		bool enableLLAudioBuffering = audioPage->lowLatencyBuffering->isChecked();
		config_set_bool(App()->GetUserConfig(), "Audio", "LowLatencyAudioBuffering", enableLLAudioBuffering);
	}

	for (auto &audioSource : audioSources) {
		auto source = OBSGetStrongRef(get<0>(audioSource));
		if (!source)
			continue;

		auto &ptmCB = get<1>(audioSource);
		auto &ptmSB = get<2>(audioSource);
		auto &pttCB = get<3>(audioSource);
		auto &pttSB = get<4>(audioSource);

		obs_source_enable_push_to_mute(source, ptmCB->isChecked());
		obs_source_set_push_to_mute_delay(source, ptmSB->value());

		obs_source_enable_push_to_talk(source, pttCB->isChecked());
		obs_source_set_push_to_talk_delay(source, pttSB->value());
	}

	auto UpdateAudioDevice = [this](bool input, QComboBox *combo, const char *name, int index) {
		main->ResetAudioDevice(input ? App()->InputAudioSource() : App()->OutputAudioSource(),
				       QT_TO_UTF8(GetComboData(combo)), Str(name), index);
	};

	UpdateAudioDevice(false, audioPage->desktopAudioDevice1, "Basic.DesktopDevice1", 1);
	UpdateAudioDevice(false, audioPage->desktopAudioDevice2, "Basic.DesktopDevice2", 2);
	UpdateAudioDevice(true, audioPage->auxAudioDevice1, "Basic.AuxDevice1", 3);
	UpdateAudioDevice(true, audioPage->auxAudioDevice2, "Basic.AuxDevice2", 4);
	UpdateAudioDevice(true, audioPage->auxAudioDevice3, "Basic.AuxDevice3", 5);
	UpdateAudioDevice(true, audioPage->auxAudioDevice4, "Basic.AuxDevice4", 6);

	if (obs_audio_monitoring_available()) {
		SaveCombo(audioPage->monitoringDevice, "Audio", "MonitoringDeviceName");
		SaveComboData(audioPage->monitoringDevice, "Audio", "MonitoringDeviceId");
	}

#ifdef _WIN32
	if (WidgetChanged(audioPage->disableAudioDucking)) {
		bool disable = audioPage->disableAudioDucking->isChecked();
		config_set_bool(App()->GetAppConfig(), "Audio", "DisableAudioDucking", disable);
		DisableAudioDucking(disable);
	}
#endif

	if (obs_audio_monitoring_available()) {
		QString newDevice = audioPage->monitoringDevice->currentData().toString();

		if (lastMonitoringDevice != newDevice) {
			obs_set_audio_monitoring_device(QT_TO_UTF8(audioPage->monitoringDevice->currentText()),
							QT_TO_UTF8(newDevice));

			blog(LOG_INFO, "Audio monitoring device:\n\tname: %s\n\tid: %s",
			     QT_TO_UTF8(audioPage->monitoringDevice->currentText()), QT_TO_UTF8(newDevice));
		}
	}

	main->SaveProject();
}

void OBSBasicSettings::SaveHotkeySettings()
{
	const auto &config = main->Config();

	using namespace std;

	std::vector<obs_key_combination> combinations;
	for (auto &hotkey : hotkeys) {
		auto &hw = *hotkey.second;
		if (!hw.Changed())
			continue;

		hw.Save(combinations);

		if (!hotkey.first)
			continue;

		OBSDataArrayAutoRelease array = obs_hotkey_save(hw.id);
		OBSDataAutoRelease data = obs_data_create();
		obs_data_set_array(data, "bindings", array);
		const char *json = obs_data_get_json(data);
		config_set_string(config, "Hotkeys", hw.name.c_str(), json);
	}

	if (!main->outputHandler || !main->outputHandler->replayBuffer)
		return;

	const char *id = obs_obj_get_id(main->outputHandler->replayBuffer);
	if (strcmp(id, "replay_buffer") == 0) {
		OBSDataAutoRelease hotkeys = obs_hotkeys_save_output(main->outputHandler->replayBuffer);
		config_set_string(config, "Hotkeys", "ReplayBuffer", obs_data_get_json(hotkeys));
	}
}

#define MINOR_SEPARATOR "------------------------------------------------"

static void AddChangedVal(std::string &changed, const char *str)
{
	if (changed.size())
		changed += ", ";
	changed += str;
}

void OBSBasicSettings::SaveSettings()
{
	if (generalChanged)
		SaveGeneralSettings();
	//Save the service every time without in living  issue4153
	if (!PLS_PLATFORM_API->isGoLive())
		SaveStream1Settings();
	if (outputsChanged)
		SaveOutputSettings();
	if (audioChanged)
		SaveAudioSettings();
	if (videoChanged)
		SaveVideoSettings();
	if (hotkeysChanged)
		SaveHotkeySettings();
	if (a11yChanged)
		SaveA11ySettings();
	if (advancedChanged)
		SaveAdvancedSettings();
	if (videoChanged || advancedChanged)
		main->ResetVideo();

	config_save_safe(main->Config(), "tmp", nullptr);
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
	main->SaveProject();

	SaveSceneDisplayMethodSettings();

	if (Changed()) {
		std::string changed;
		if (generalChanged)
			AddChangedVal(changed, "general");
		if (stream1Changed)
			AddChangedVal(changed, "stream 1");
		if (outputsChanged)
			AddChangedVal(changed, "outputs");
		if (audioChanged)
			AddChangedVal(changed, "audio");
		if (videoChanged)
			AddChangedVal(changed, "video");
		if (hotkeysChanged)
			AddChangedVal(changed, "hotkeys");
		if (a11yChanged)
			AddChangedVal(changed, "a11y");
		if (advancedChanged)
			AddChangedVal(changed, "advanced");

		blog(LOG_INFO, "Settings changed (%s)", changed.c_str());
		blog(LOG_INFO, MINOR_SEPARATOR);
	}

	bool langChanged = false;
	if (generalPage) {
		langChanged = generalPage->language->currentIndex() != prevLangIndex;
	}
	bool audioRestart = false;
	if (audioPage) {
		audioRestart = audioPage->channelSetup->currentIndex() != channelIndex ||
			       audioPage->sampleRate->currentIndex() != sampleRateIndex;
	}
	bool browserHWAccelChanged = false;
	if (advancedPage) {
		browserHWAccelChanged = advancedPage->browserHWAccel &&
					advancedPage->browserHWAccel->isChecked() != prevBrowserAccel;
	}

	if (langChanged || audioRestart || browserHWAccelChanged)
		GlobalVars::restart = true;
	else
		GlobalVars::restart = false;
}

bool OBSBasicSettings::QueryChanges()
{
	PLSErrorHandler::ExtraData extraData("OBSBasicSettings");

	auto button = PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_BASIC_SETTINGS_CONFIRM,
							    PLSErrKeyAllAlert, {}, extraData);

	if (button.clickedBtn == PLSAlertView::Button::Cancel || button.clickedBtn == PLSAlertView::Button::NoButton) {
		return false;
	}
	if (button.clickedBtn == PLSAlertView::Button::Yes) {
		if (generalPage) {
			if ((generalPage->language->currentData().toString().toStdString() !=
			     m_currentLanguage.first) &&
			    PLSAlertView::Button::Yes ==
				    PLSErrorHandler::showAlertByPrismCode(
					    PLSErrorHandler::ALERT_SETTINGS_GENERAL_LANGUAGE_CHANGED, PLSErrKeyAllAlert,
					    {}, extraData)
					    .clickedBtn) {
				setResult(Qt::UserRole + RESTARTAPP);
			} else {
				generalPage->language->setCurrentText(QString::fromStdString(m_currentLanguage.second));
			}
		}
		if (!QueryAllowedToClose())
			return false;
		SaveSettings();
	} else {
		LoadSettings(true);
		GlobalVars::restart = false;
	}

	ClearChanged();
	return true;
}

bool OBSBasicSettings::QueryAllowedToClose()
{
	if (!outputPage) {
		return true;
	}

	bool simple = (outputPage->outputMode->currentIndex() == 0);

	bool invalidEncoder = false;
	bool invalidFormat = false;
	bool invalidTracks = false;
	if (simple) {
		if ((outputSimplePage->simpleOutRecEncoder->currentIndex() == -1 &&
		     outputSimplePage->simpleOutRecEncoder->isVisible()) ||
		    (outputSimplePage->simpleOutStrEncoder->currentIndex() == -1 &&
		     outputSimplePage->simpleOutStrEncoder->isVisible()) ||
		    (outputSimplePage->simpleOutRecAEncoder->currentIndex() == -1 &&
		     outputSimplePage->simpleOutRecAEncoder->isVisible()) ||
		    (outputSimplePage->simpleOutStrAEncoder->currentIndex() == -1 &&
		     outputSimplePage->simpleOutStrAEncoder->isVisible()))
			invalidEncoder = true;

		if (outputSimplePage->simpleOutRecFormat->currentIndex() == -1)
			invalidFormat = true;

		QString qual = outputSimplePage->simpleOutRecQuality->currentData().toString();
		QString format = outputSimplePage->simpleOutRecFormat->currentData().toString();
		if (SimpleOutGetSelectedAudioTracks() == 0 && qual != "Stream" && format != "flv")
			invalidTracks = true;
	} else {
		if (outputStreamPage && (outputStreamPage->advOutEncoder->currentIndex() == -1 ||
					 outputStreamPage->advOutAEncoder->currentIndex() == -1))
			invalidEncoder = true;

		if (outputRecordPage && (outputRecordPage->advOutRecEncoder->currentIndex() == -1 ||
					 outputRecordPage->advOutRecAEncoder->currentIndex() == -1))
			invalidEncoder = true;

		if (outputRecordPage) {
			QString format = outputRecordPage->advOutRecFormat->currentData().toString();
			if (AdvOutGetSelectedAudioTracks() == 0 && format != "flv")
				invalidTracks = true;
		}
		if (AdvOutGetStreamingSelectedAudioTracks() == 0)
			invalidTracks = true;
	}
	PLSErrorHandler::ExtraData extraData("OBSBasicSettings");
	if (invalidEncoder) {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_CODEC_COMPACT_CODEC_MISSING_ONEXIT_TEXT,
						      PLSErrKeyAllAlert, {}, extraData);
		return false;
	} else if (invalidFormat) {
		PLSErrorHandler::showAlertByPrismCode(
			PLSErrorHandler::ALERT_CODEC_COMPACT_CONTAINER_MISSING_ONEXIT_TEXT, PLSErrKeyAllAlert, {},
			extraData);
		return false;
	} else if (invalidTracks) {
		PLSErrorHandler::showAlertByPrismCode(
			PLSErrorHandler::ALERT_OUTPUT_WARNINGS_NOTRACKS_SELECTED_ONEXIT_TEXT, PLSErrKeyAllAlert, {},
			extraData);
		return false;
	}

	return true;
}

void OBSBasicSettings::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Escape) {
		event->accept();
		reject();
		return;
	}
	PLSDialogView::keyPressEvent(event);
}

void OBSBasicSettings::closeEvent(QCloseEvent *event)
{
	if (!AskIfCanCloseSettings())
		event->ignore();
}

void OBSBasicSettings::reject()
{
	if (AskIfCanCloseSettings())
		close();
}

bool OBSBasicSettings::eventFilter(QObject *watched, QEvent *event)
{
	if (generalPage && watched == generalPage->widget_2 && event->type() == QEvent::Resize) {
		generalPage->accountView->setNickNameWidth(static_cast<QResizeEvent *>(event)->size().width());
	}
	return PLSDialogView::eventFilter(watched, event);
}

void OBSBasicSettings::resizeEvent(QResizeEvent *e)
{
	PLSDialogView::resizeEvent(e);
	if (ui->alertMessageFrame->isVisibleTo(this)) {
		calculateErrorMsgSize();
	}
}

void OBSBasicSettings::initGeneralage()
{
	PLS_PERFORMANCE_FUNCTION();
	if (!generalPage) {
		loading = true;

		PLS_DISABLE_UISTEP_V2(ui->generalPage);
		generalPage.reset(new Ui::SettingGeneralPage);
		PLS_PERFORMANCE_START(initGeneralage_setupUi);

		generalPage->setupUi(ui->generalPage);
		PLS_PERFORMANCE_END(initGeneralage_setupUi);

		auto watcher = new PLSShowWatcher(generalPage->language);
		connect(watcher, &PLSShowWatcher::signalShow, generalPage->language,
			[this]() { hideSettingsPageLoadingIfStillCurrentRow(Pages::GENERAL); });

		PLS_PERFORMANCE_START(initGeneralage_hooksLoad);
		pls_uistep_v2_set_custom_show_hide_name(ui->generalPage, QByteArrayLiteral("General Page"));
		pls_uistep_v2_value_auto_to_english_enable(generalPage->language, QStringLiteral("*"), false);

		HookWidget(generalPage->language, COMBO_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->hideOBSFromCapture, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->hideProjectorCursor, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->projectorAlwaysOnTop, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->recordWhenStreaming, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->keepRecordStreamStops, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->replayWhileStreaming, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->keepReplayStreamStops, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->systemTrayEnabled, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->systemTrayWhenStarted, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->systemTrayAlways, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->saveProjectors, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->closeProjectors, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->snappingEnabled, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->screenSnapping, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->centerSnapping, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->sourceSnapping, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->snapDistance, DSCROLL_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->overflowHide, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->overflowAlwaysVisible, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->overflowSelectionHide, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->previewSafeAreas, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->previewSpacingHelpers, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->previewZoomEnabled, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->doubleClickSwitch, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->studioPortraitLayout, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->prevProgLabelToggle, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->multiviewMouseSwitch, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->multiviewDrawNames, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->multiviewDrawAreas, CHECK_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->multiviewLayout, COMBO_CHANGED, GENERAL_CHANGED);
		HookWidget(generalPage->watermarkCheckBox, CHECK_CHANGED, GENERAL_CHANGED);

#ifdef _WIN32
		if (!SetDisplayAffinitySupported()) {
			delete generalPage->hideOBSFromCapture;
			generalPage->hideOBSFromCapture = nullptr;
		}
#endif

		generalPage->widget_2->installEventFilter(this);

		LoadGeneralSettings();
		UpdateGeneralReplayBufferCheckboxes();

		connect(generalPage->sceneDisplayComboBox, QOverload<int>::of(&PLSComboBox::currentIndexChanged), this,
			[this](int index) {
				OnSceneDisplayMethodIndexChanged(index);

				if (!loading) {
					EnableApplyButton(true);
				}
			});

		generalPage->snappingEnabled->setAccessibleName(QTStr("Basic.Settings.General.Snapping"));
		generalPage->systemTrayEnabled->setAccessibleName(QTStr("Basic.Settings.General.SysTray"));

		QString userServiceName = PLSLoginUserInfo::getInstance()->getNCPPlatformServiceName();
		if (userServiceName.isEmpty()) {
			generalPage->waterMarkGroupBox->hide();
		} else {
			generalPage->waterMarkGroupBox->show();
		}

		alignLabels(ui->generalPage);
#ifdef Q_OS_MACOS
		pls_scroll_area_clips_to_bounds(generalPage->scrollArea_2);
#endif
		updateOutPutRelatedUI();
		PLS_PERFORMANCE_END(initGeneralage_hooksLoad);

		PLS_PERFORMANCE_START(initGeneralage_autoBind);
		pls_uistep_v2_auto_bind(ui->generalPage);
		PLS_PERFORMANCE_END(initGeneralage_autoBind);
		loading = false;
	}
}

void OBSBasicSettings::initOutputPage()
{
	PLS_PERFORMANCE_FUNCTION();
	if (!outputPage) {
		loading = true;

		PLS_DISABLE_UISTEP_V2(ui->outputPage);
		outputPage.reset(new Ui::SettingOutputPage);
		PLS_PERFORMANCE_START(initOutputPage_setupUi);
		outputPage->setupUi(ui->outputPage);
		PLS_PERFORMANCE_END(initOutputPage_setupUi);
		auto watcher = new PLSShowWatcher(outputPage->service);
		connect(watcher, &PLSShowWatcher::signalShow, outputPage->service,
			[this]() { hideSettingsPageLoadingIfStillCurrentRow(Pages::OUTPUT); });

		PLS_PERFORMANCE_START(initOutputPage_hooksAndLoad);
		connect(outputPage->service, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::on_service_currentIndexChanged);
		connect(outputPage->customServer, &QLineEdit::textChanged, this,
			&OBSBasicSettings::on_customServer_textChanged);

		HookWidget(outputPage->service, COMBO_CHANGED, STREAM1_CHANGED);
		HookWidget(outputPage->server, COMBO_CHANGED, STREAM1_CHANGED);
		HookWidget(outputPage->customServer, EDIT_CHANGED, STREAM1_CHANGED);

		HookWidget(outputPage->outputMode, COMBO_CHANGED, OUTPUTS_CHANGED);

		HookWidget(outputPage->enableMultitrackVideo, CHECK_CHANGED, STREAM1_CHANGED);
		HookWidget(outputPage->multitrackVideoMaximumAggregateBitrateAuto, CHECK_CHANGED, STREAM1_CHANGED);
		HookWidget(outputPage->multitrackVideoMaximumAggregateBitrate, SCROLL_CHANGED, STREAM1_CHANGED);
		HookWidget(outputPage->multitrackVideoMaximumVideoTracksAuto, CHECK_CHANGED, STREAM1_CHANGED);
		HookWidget(outputPage->multitrackVideoMaximumVideoTracks, SCROLL_CHANGED, STREAM1_CHANGED);
		HookWidget(outputPage->multitrackVideoStreamDumpEnable, CHECK_CHANGED, STREAM1_CHANGED);
		HookWidget(outputPage->multitrackVideoConfigOverrideEnable, CHECK_CHANGED, STREAM1_CHANGED);
		HookWidget(outputPage->multitrackVideoConfigOverride, TEXT_CHANGED, STREAM1_CHANGED);

		outputSettingsAdvCurrentTab = outputPage->advOutputStreamTab;
		connect(outputPage->advOutTabs, &QTabWidget::currentChanged, [this](int index) {
			switch (index) {
			case 0:
				initOutputStreamPage();
				break;

			case 1:
				initOutputRecordPage();
				break;

			case 2:
				initOutputAudioPage();
				break;

			case 3:
				initOutputReplayPage();
				break;

			default:
				break;
			}

			outputSettingsAdvCurrentTab = outputPage->advOutTabs->widget(index);
			AdvOutStreamEncoderCheckWarnings();
		});

		connect(outputPage->outputMode, &PLSComboBox::currentIndexChanged, this, [this](int index) {
			switch (index) {
			case 0:
				initOutputSimplePage();
				break;

			case 1:
				initOutputStreamPage();
				break;

			default:
				break;
			}

			UpdateStreamDelayEstimate();
			AdvOutStreamEncoderCheckWarnings();

			if (1 == index) {
				auto position = outputStreamPage->verticalLayout_14->indexOf(
					outputStreamPage->advOutTopContainer);
				outputStreamPage->verticalLayout_14->insertWidget(position + 1,
										  outputPage->multitrackVideoGroupBox);
			} else {
				auto position = outputSimplePage->verticalLayout_52->indexOf(
					outputSimplePage->simpleStreamingGroupBox);
				outputSimplePage->verticalLayout_52->insertWidget(position + 1,
										  outputPage->multitrackVideoGroupBox);
			}
		});

		LoadServices(false);

		connect(outputPage->enableMultitrackVideo, &PLSCheckBox::toggled, this,
			&OBSBasicSettings::UpdateMultitrackVideo);
		connect(outputPage->multitrackVideoMaximumAggregateBitrateAuto, &PLSCheckBox::toggled, this,
			&OBSBasicSettings::UpdateMultitrackVideo);
		connect(outputPage->multitrackVideoMaximumVideoTracksAuto, &PLSCheckBox::toggled, this,
			&OBSBasicSettings::UpdateMultitrackVideo);
		connect(outputPage->multitrackVideoConfigOverrideEnable, &PLSCheckBox::toggled, this,
			&OBSBasicSettings::UpdateMultitrackVideo);

		LoadStream1Settings();
		LoadOutputSettings();
		PLS_PERFORMANCE_END(initOutputPage_hooksAndLoad);

		PLS_PERFORMANCE_START(initOutputPage_alignBind);
		alignLabels(ui->outputPage);

		pls_uistep_v2_set_title(outputPage->advOutputStreamTab,
					QStringLiteral("Settings - Output - Advanced - Streaming"));
		pls_uistep_v2_set_title(outputPage->advOutputRecordTab,
					QStringLiteral("Settings - Output - Advanced - Recording"));
		pls_uistep_v2_set_title(outputPage->advOutputAudioTracksTab,
					QStringLiteral("Settings - Output - Advanced - Audio"));
		pls_uistep_v2_set_title(outputPage->advOutputReplayTab,
					QStringLiteral("Settings - Output - Advanced - Replay Buffer"));
		pls_uistep_v2_auto_bind(ui->outputPage);

		pls_uistep_v2_set_custom_show_hide_name(outputPage->advOutTabs, "Advance Page");
		pls_uistep_v2_set_custom_show_hide_name(outputPage->easyOutputsPage, "Simple Page");
		pls_uistep_v2_set_custom_show_hide_name(outputPage->advOutputAudioTracksTab, "Advance Audio Page");
		pls_uistep_v2_set_custom_show_hide_name(outputPage->advOutputRecordTab, "Advance Record Page");
		pls_uistep_v2_set_custom_show_hide_name(outputPage->advOutputReplayTab, "Advance Replay Page");
		pls_uistep_v2_set_custom_show_hide_name(outputPage->advOutputStreamTab, "Advance Stream Page");
		pls_uistep_v2_set_name(outputPage->multitrackVideoMaximumAggregateBitrate,
				       "MultitrackVideo Maximum Aggregate Bitrate SpinBox");
		pls_uistep_v2_set_name(outputPage->multitrackVideoMaximumVideoTracks,
				       "MultitrackVideo Maximum Video Tracks SpinBox");
		pls_connect(outputPage->multitrackVideoInfo, &QLabel::linkActivated, [](const QString &link) {
			QDesktopServices::openUrl(QUrl(link));
			PLS_UI_ACTION("In Setting View Open Learn More Url Done");
		});
		pls_uistep_v2_custom(outputPage->multitrackVideoInfo, QStringLiteral("linkActivated"),
				     QStringLiteral("Click"), QStringLiteral("button"), QStringLiteral("Learn More"));
		PLS_PERFORMANCE_END(initOutputPage_alignBind);
		loading = false;
	}
}

void OBSBasicSettings::initAudioPage()
{
	PLS_PERFORMANCE_FUNCTION();
	if (!audioPage) {
		loading = true;

		PLS_DISABLE_UISTEP_V2(ui->audioPage);
		audioPage.reset(new Ui::SettingAudioPage);
		PLS_PERFORMANCE_START(initAudioPage_setupUi);
		audioPage->setupUi(ui->audioPage);
		PLS_PERFORMANCE_END(initAudioPage_setupUi);
		auto watcher = new PLSShowWatcher(audioPage->channelSetup);
		connect(watcher, &PLSShowWatcher::signalShow, audioPage->channelSetup,
			[this]() { hideSettingsPageLoadingIfStillCurrentRow(Pages::AUDIO); });

		PLS_PERFORMANCE_START(initAudioPage_hooksAndLoad);
		HookWidget(audioPage->channelSetup, COMBO_CHANGED, AUDIO_RESTART);
		HookWidget(audioPage->sampleRate, COMBO_CHANGED, AUDIO_RESTART);
		HookWidget(audioPage->meterDecayRate, COMBO_CHANGED, AUDIO_CHANGED);
		HookWidget(audioPage->peakMeterType, COMBO_CHANGED, AUDIO_CHANGED);
		HookWidget(audioPage->desktopAudioDevice1, COMBO_CHANGED, AUDIO_CHANGED);
		HookWidget(audioPage->desktopAudioDevice2, COMBO_CHANGED, AUDIO_CHANGED);
		HookWidget(audioPage->auxAudioDevice1, COMBO_CHANGED, AUDIO_CHANGED);
		HookWidget(audioPage->auxAudioDevice2, COMBO_CHANGED, AUDIO_CHANGED);
		HookWidget(audioPage->auxAudioDevice3, COMBO_CHANGED, AUDIO_CHANGED);
		HookWidget(audioPage->auxAudioDevice4, COMBO_CHANGED, AUDIO_CHANGED);

		if (obs_audio_monitoring_available())
			HookWidget(audioPage->monitoringDevice, COMBO_CHANGED, AUDIO_CHANGED);
#ifdef _WIN32
		HookWidget(audioPage->disableAudioDucking, CHECK_CHANGED, AUDIO_CHANGED);
#endif

		if (!obs_audio_monitoring_available()) {
			delete audioPage->monitoringDeviceLabel;
			audioPage->monitoringDeviceLabel = nullptr;
			delete audioPage->monitoringDevice;
			audioPage->monitoringDevice = nullptr;
		}

		if (obs_audio_monitoring_available())
			FillAudioMonitoringDevices();

		connect(audioPage->channelSetup, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::SurroundWarning);
		connect(audioPage->channelSetup, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::SpeakerLayoutChanged);
		connect(audioPage->lowLatencyBuffering, &PLSCheckBox::clicked, this,
			&OBSBasicSettings::LowLatencyBufferingChanged);

		LoadAudioSettings();

		channelIndex = audioPage->channelSetup->currentIndex();
		sampleRateIndex = audioPage->sampleRate->currentIndex();
		llBufferingEnabled = audioPage->lowLatencyBuffering->isChecked();

		UpdateAudioWarnings();

		alignLabels(ui->audioPage);
#ifdef Q_OS_MACOS
		pls_scroll_area_clips_to_bounds(audioPage->scrollArea_50);
#endif
		PLS_PERFORMANCE_END(initAudioPage_hooksAndLoad);

		PLS_PERFORMANCE_START(initAudioPage_autoBind);
		pls_uistep_v2_auto_bind(ui->audioPage);
		PLS_PERFORMANCE_END(initAudioPage_autoBind);
		loading = false;
	}
}

void OBSBasicSettings::initVideoPage()
{
	PLS_PERFORMANCE_FUNCTION();
	if (!videoPage) {
		loading = true;

		PLS_DISABLE_UISTEP_V2(ui->videoPage);
		videoPage.reset(new Ui::SettingVideoPage);
		PLS_PERFORMANCE_START(initVideoPage_setupUi);
		videoPage->setupUi(ui->videoPage);
		PLS_PERFORMANCE_END(initVideoPage_setupUi);
		auto watcher = new PLSShowWatcher(videoPage->baseResolution);
		connect(watcher, &PLSShowWatcher::signalShow, videoPage->baseResolution,
			[this]() { hideSettingsPageLoadingIfStillCurrentRow(Pages::VIDEO); });

		PLS_PERFORMANCE_START(initVideoPage_hooksLoadConnect);
		HookWidget(videoPage->baseResolution, CBEDIT_CHANGED, VIDEO_RES);
		HookWidget(videoPage->outputResolution, CBEDIT_CHANGED, VIDEO_RES);
		HookWidget(videoPage->downscaleFilter, COMBO_CHANGED, VIDEO_CHANGED);
		HookWidget(videoPage->fpsType, COMBO_CHANGED, VIDEO_CHANGED);
		HookWidget(videoPage->fpsCommon, COMBO_CHANGED, VIDEO_CHANGED);
		HookWidget(videoPage->fpsInteger, SCROLL_CHANGED, VIDEO_CHANGED);
		HookWidget(videoPage->fpsNumerator, SCROLL_CHANGED, VIDEO_CHANGED);
		HookWidget(videoPage->fpsDenominator, SCROLL_CHANGED, VIDEO_CHANGED);
		HookWidget(videoPage->baseResolution_2, CBEDIT_CHANGED, VIDEO_CHANGED);
		HookWidget(videoPage->outputResolution_2, CBEDIT_CHANGED, VIDEO_CHANGED);

		connect(videoPage->baseResolution, &PLSEditableComboBox::editTextChanged, this,
			&OBSBasicSettings::on_baseResolution_editTextChanged);
		connect(videoPage->outputResolution, &PLSEditableComboBox::editTextChanged, this,
			&OBSBasicSettings::on_outputResolution_editTextChanged);

		LoadVideoSettings();

		QRegularExpression rx("\\d{1,5}x\\d{1,5}");
		QValidator *validator = new QRegularExpressionValidator(rx, this);
		videoPage->baseResolution->lineEdit()->setValidator(validator);
		videoPage->outputResolution->lineEdit()->setValidator(validator);

		alignVideoPage();

		videoPage->checkBoxDualOutput->setChecked(pls_is_dual_output_on());
		connect(PLSBasic::instance(), &PLSBasic::sigOpenDualOutput, this, [this] {
			if (pls_is_dual_output_on()) {
				lastServiceIdx = -1;
				showDualoutputSetting(true, true);
			} else {
				showNormalSetting(true, true);
			}
		});
		connect(videoPage->checkBoxDualOutput, &PLSCheckBox::clicked, this, [this](bool bChecked) {
			if (!PLSBasic::instance()->setDualOutputEnabled(bChecked, true)) {
				QSignalBlocker signalBlocker(videoPage->checkBoxDualOutput);

				videoPage->checkBoxDualOutput->setChecked(false);
			} else {
				bReloadHotKey = true;
				bReloadAudioSources = true;
				LoadResolutionLists();
				LoadVerticalResolutionLists();
			}
		});
		if (pls_is_dual_output_on()) {
			showDualoutputSetting(true, false);
		} else {
			showNormalSetting(true, false);
		}

		connect(videoPage->baseResolution_2, &PLSEditableComboBox::editTextChanged, this,
			[this](const QString &text) {
				if (!loading && ValidResolutions(this, videoPage->baseResolution_2,
								 videoPage->outputResolution_2)) {
					QString baseResolution = text;
					uint32_t cx, cy;

					ConvertResText(QT_TO_UTF8(baseResolution), cx, cy);

					std::tuple<int, int> aspect = aspect_ratio(cx, cy);

					videoPage->baseAspect_2->setText(
						QTStr("AspectRatio")
							.arg(QString::number(std::get<0>(aspect)),
							     QString::number(std::get<1>(aspect))));

					ResetVerticalDownscales(cx, cy);
					LoadDownscaleFilters(false);
				}
			});

		connect(videoPage->outputResolution_2, &PLSEditableComboBox::editTextChanged, this,
			[this](const QString &text) {
				if (!loading) {
					RecalcResPixels(videoPage->scaledAspect_2, QT_TO_UTF8(text));
					LoadDownscaleFilters(false);
				}
			});

		connect(videoPage->tabWidgetDualOutputVideo, &QTabWidget::currentChanged, this, [this](int index) {
			switch (index) {
			case 0:
				videoPage->formLayout_3->addRow(videoPage->label_11, videoPage->downscaleFilter);
				videoPage->formLayout_3->addRow(videoPage->label, videoPage->downscaleFilterDesc);
				videoPage->formLayout_3->addRow(videoPage->fpsType, videoPage->fpsTypes);
				videoPage->formLayout_3->addRow(videoPage->spacer);
				LoadDownscaleFilters(true);
				PLS_UI_ACTION("Setting Video: dual output Horizontal selected.");
				break;

			case 1:
				videoPage->formLayout_15->addRow(videoPage->label_11, videoPage->downscaleFilter);
				videoPage->formLayout_15->addRow(videoPage->label, videoPage->downscaleFilterDesc);
				videoPage->formLayout_15->addRow(videoPage->fpsType, videoPage->fpsTypes);
				videoPage->formLayout_15->addRow(videoPage->spacer);
				LoadDownscaleFilters(false);
				PLS_UI_ACTION("Setting Video: dual output Vertical selected.");
				break;

			default:
				break;
			}
		});

		alignLabels(ui->videoPage);
		PLS_PERFORMANCE_END(initVideoPage_hooksLoadConnect);

		PLS_PERFORMANCE_START(initVideoPage_autoBind);
		pls_uistep_v2_auto_bind(ui->videoPage);
		PLS_PERFORMANCE_END(initVideoPage_autoBind);

		loading = false;
	}
}

void OBSBasicSettings::initHotkeyPage()
{
	PLS_PERFORMANCE_FUNCTION();
	if (!hotkeyPage) {
		PLS_DISABLE_UISTEP_V2(ui->hotkeyPage);
		hotkeyPage.reset(new Ui::SettingHotkeyPage);
		hotkeyPage->setupUi(ui->hotkeyPage);
		auto watcher = new PLSShowWatcher(hotkeyPage->pushButton);
		connect(watcher, &PLSShowWatcher::signalShow, hotkeyPage->pushButton,
			[this]() { hideSettingsPageLoadingIfStillCurrentRow(Pages::HOTKEYS); });
		pls_uistep_v2_set_custom_enter_leave_name(hotkeyPage->pushButton, "hotkeyPage clear");
		connect(hotkeyPage->pushButton, &QPushButton::clicked, this, &OBSBasicSettings::on_pushButton_clicked);
		connect(hotkeyPage->hotkeyFilterReset, &QPushButton::clicked, this,
			&OBSBasicSettings::on_hotkeyFilterReset_clicked);
		connect(hotkeyPage->hotkeyFilterSearch, &QLineEdit::textChanged, this,
			&OBSBasicSettings::on_hotkeyFilterSearch_textChanged);
		connect(hotkeyPage->hotkeyFilterInput, &OBSHotkeyEdit::KeyChanged, this,
			&OBSBasicSettings::on_hotkeyFilterInput_KeyChanged);

#ifdef Q_OS_MACOS
		pls_scroll_area_clips_to_bounds(hotkeyPage->hotkeyScrollArea);
#endif
		pls_uistep_v2_auto_bind(ui->hotkeyPage);
	}
}

void OBSBasicSettings::initAccessPage()
{
	PLS_PERFORMANCE_FUNCTION();
	if (!accessPage) {
		PLS_DISABLE_UISTEP_V2(ui->accessPage);
		accessPage.reset(new Ui::SettingAccessPage);
		PLS_PERFORMANCE_START(initAccessPage_setupUi);
		accessPage->setupUi(ui->accessPage);
		PLS_PERFORMANCE_END(initAccessPage_setupUi);
		auto watcher = new PLSShowWatcher(accessPage->choose1);
		connect(watcher, &PLSShowWatcher::signalShow, accessPage->choose1,
			[this]() { hideSettingsPageLoadingIfStillCurrentRow(Pages::ACCESSIBILITY); });

		PLS_PERFORMANCE_START(initAccessPage_hooksLoad);
		connect(accessPage->choose1, &QPushButton::clicked, this, &OBSBasicSettings::on_choose1_clicked);
		connect(accessPage->choose2, &QPushButton::clicked, this, &OBSBasicSettings::on_choose2_clicked);
		connect(accessPage->choose3, &QPushButton::clicked, this, &OBSBasicSettings::on_choose3_clicked);
		connect(accessPage->choose4, &QPushButton::clicked, this, &OBSBasicSettings::on_choose4_clicked);
		connect(accessPage->choose5, &QPushButton::clicked, this, &OBSBasicSettings::on_choose5_clicked);
		connect(accessPage->choose6, &QPushButton::clicked, this, &OBSBasicSettings::on_choose6_clicked);
		connect(accessPage->choose7, &QPushButton::clicked, this, &OBSBasicSettings::on_choose7_clicked);
		connect(accessPage->choose8, &QPushButton::clicked, this, &OBSBasicSettings::on_choose8_clicked);
		connect(accessPage->choose9, &QPushButton::clicked, this, &OBSBasicSettings::on_choose9_clicked);
		connect(accessPage->choose10, &QPushButton::clicked, this, &OBSBasicSettings::on_choose10_clicked);

		connect(accessPage->colorPreset, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::on_colorPreset_currentIndexChanged);

		connect(accessPage->colorCheckBox, &PLSCheckBox::clicked,
			[this](bool isChecked) { accessPage->colorsGroupBox->setEnabled(isChecked); });

		HookWidget(accessPage->colorCheckBox, CHECK_CHANGED, A11Y_CHANGED);
		HookWidget(accessPage->colorPreset, COMBO_CHANGED, A11Y_CHANGED);

		LoadA11ySettings();

		alignLabels(ui->accessPage);
#ifdef Q_OS_MACOS
		pls_scroll_area_clips_to_bounds(accessPage->scrollArea_7);
#endif
		PLS_PERFORMANCE_END(initAccessPage_hooksLoad);

		PLS_PERFORMANCE_START(initAccessPage_autoBind);
		pls_uistep_v2_auto_bind(ui->accessPage);
		PLS_PERFORMANCE_END(initAccessPage_autoBind);
	}
}

void OBSBasicSettings::initAdvancedPage()
{
	PLS_PERFORMANCE_FUNCTION();
	if (!advancedPage) {
		loading = true;
		PLS_DISABLE_UISTEP_V2(ui->advancedPage);
		advancedPage.reset(new Ui::SettingAdvancedPage);
		PLS_PERFORMANCE_START(initAdvancedPage_setupUi);
		advancedPage->setupUi(ui->advancedPage);
		PLS_PERFORMANCE_END(initAdvancedPage_setupUi);
		auto watcher = new PLSShowWatcher(advancedPage->colorFormat);
		connect(watcher, &PLSShowWatcher::signalShow, advancedPage->colorFormat,
			[this]() { hideSettingsPageLoadingIfStillCurrentRow(Pages::ADVANCED); });

		PLS_PERFORMANCE_START(initAdvancedPage_hooksLoad);
		connect(advancedPage->colorFormat, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::on_colorFormat_currentIndexChanged);
		connect(advancedPage->colorSpace, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::on_colorSpace_currentIndexChanged);

		connect(advancedPage->filenameFormatting, &QLineEdit::textEdited, this,
			&OBSBasicSettings::on_filenameFormatting_textEdited);

		connect(advancedPage->disableOSXVSync, &PLSCheckBox::clicked, this,
			&OBSBasicSettings::on_disableOSXVSync_clicked);

		HookWidget(advancedPage->renderer, COMBO_CHANGED, ADV_RESTART);
		HookWidget(advancedPage->adapter, COMBO_CHANGED, ADV_RESTART);
		HookWidget(advancedPage->colorFormat, COMBO_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->colorSpace, COMBO_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->colorRange, COMBO_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->sdrWhiteLevel, SCROLL_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->hdrNominalPeakLevel, SCROLL_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->disableOSXVSync, CHECK_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->resetOSXVSync, CHECK_CHANGED, ADV_CHANGED);
#if defined(_WIN32) || defined(__APPLE__)
		HookWidget(advancedPage->browserHWAccel, CHECK_CHANGED, ADV_RESTART);
#endif
		HookWidget(advancedPage->filenameFormatting, EDIT_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->overwriteIfExists, CHECK_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->simpleRBPrefix, EDIT_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->simpleRBSuffix, EDIT_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->streamDelayEnable, CHECK_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->streamDelaySec, SCROLL_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->streamDelayPreserve, CHECK_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->reconnectEnable, CHECK_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->reconnectRetryDelay, SCROLL_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->reconnectMaxRetries, SCROLL_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->processPriority, COMBO_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->confirmOnExit, CHECK_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->bindToIP, COMBO_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->ipFamily, COMBO_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->enableNewSocketLoop, CHECK_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->enableLowLatencyMode, CHECK_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->hotkeyFocusType, COMBO_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->autoRemux, CHECK_CHANGED, ADV_CHANGED);
		HookWidget(advancedPage->dynBitrate, CHECK_CHANGED, ADV_CHANGED);

#define ADD_HOTKEY_FOCUS_TYPE(s) advancedPage->hotkeyFocusType->addItem(QTStr("Basic.Settings.Advanced.Hotkeys." s), s)

		ADD_HOTKEY_FOCUS_TYPE("NeverDisableHotkeys");
		ADD_HOTKEY_FOCUS_TYPE("DisableHotkeysInFocus");
		ADD_HOTKEY_FOCUS_TYPE("DisableHotkeysOutOfFocus");

#ifdef _WIN32
		static struct ProcessPriority {
			const char *name;
			const char *val;
		} processPriorities[] = {
			{"Basic.Settings.Advanced.General.ProcessPriority.High", "High"},
			{"Basic.Settings.Advanced.General.ProcessPriority.AboveNormal", "AboveNormal"},
			{"Basic.Settings.Advanced.General.ProcessPriority.Normal", "Normal"},
			{"Basic.Settings.Advanced.General.ProcessPriority.BelowNormal", "BelowNormal"},
			{"Basic.Settings.Advanced.General.ProcessPriority.Idle", "Idle"},
		};

		for (ProcessPriority pri : processPriorities)
			advancedPage->processPriority->addItem(QTStr(pri.name), pri.val);

#else
		delete advancedPage->rendererLabel;
		delete advancedPage->renderer;
		delete advancedPage->adapterLabel;
		delete advancedPage->adapter;
		delete advancedPage->processPriorityLabel;
		delete advancedPage->processPriority;
		delete advancedPage->enableNewSocketLoop;
		delete advancedPage->enableLowLatencyMode;
#ifdef __linux__
		delete advancedPage->browserHWAccel;
		delete advancedPage->sourcesGroup;
#endif
		advancedPage->rendererLabel = nullptr;
		advancedPage->renderer = nullptr;
		advancedPage->adapterLabel = nullptr;
		advancedPage->adapter = nullptr;
		advancedPage->processPriorityLabel = nullptr;
		advancedPage->processPriority = nullptr;
		advancedPage->enableNewSocketLoop = nullptr;
		advancedPage->enableLowLatencyMode = nullptr;
#ifdef __linux__
		advancedPage->browserHWAccel = nullptr;
		advancedPage->sourcesGroup = nullptr;
#endif
#endif

#ifndef __APPLE__
		delete advancedPage->disableOSXVSync;
		delete advancedPage->resetOSXVSync;
		advancedPage->disableOSXVSync = nullptr;
		advancedPage->resetOSXVSync = nullptr;
#else
		advancedPage->disableOSXVSync->setWordWrap(true);
		advancedPage->resetOSXVSync->setWordWrap(true);
#endif

		connect(advancedPage->streamDelaySec, &QSpinBox::valueChanged, this,
			&OBSBasicSettings::UpdateStreamDelayEstimate);

		LoadColorRanges();
		LoadColorSpaces();
		LoadColorFormats();

		obs_properties_t *ppts = obs_get_output_properties("rtmp_output");

		obs_property_t *p = obs_properties_get(ppts, "bind_ip");
		size_t count = obs_property_list_item_count(p);
		for (size_t i = 0; i < count; i++) {
			const char *name = obs_property_list_item_name(p, i);
			const char *val = obs_property_list_item_string(p, i);

			advancedPage->bindToIP->addItem(QT_UTF8(name), val);
		}

		p = obs_properties_get(ppts, "ip_family");
		count = obs_property_list_item_count(p);
		for (size_t i = 0; i < count; i++) {
			const char *name = obs_property_list_item_name(p, i);
			const char *val = obs_property_list_item_string(p, i);

			advancedPage->ipFamily->addItem(QT_UTF8(name), val);
		}

		obs_properties_destroy(ppts);

		LoadAdvancedSettings();
		PLS_PERFORMANCE_END(initAdvancedPage_hooksLoad);

		PLS_PERFORMANCE_START(initAdvancedPage_alignBind);
		advancedPage->streamDelayEnable->setAccessibleName(QTStr("Basic.Settings.Advanced.StreamDelay"));
		advancedPage->reconnectEnable->setAccessibleName(QTStr("Basic.Settings.Output.Reconnect"));

		advancedPage->autoRemux->setText(QTStr("Basic.Settings.Advanced.AutoRemux").arg("mp4"));

		UpdateAdvNetworkGroup();

		alignLabels(ui->advancedPage);
#ifdef Q_OS_MACOS
		pls_scroll_area_clips_to_bounds(advancedPage->scrollArea);
#endif
		pls_uistep_v2_auto_bind(ui->advancedPage);
		pls_uistep_v2_bind(advancedPage->colorRange, advancedPage->label_34);
		PLS_PERFORMANCE_END(initAdvancedPage_alignBind);
		loading = false;
	}
}

void OBSBasicSettings::initOutputSimplePage()
{
	if (!outputSimplePage) {
		auto bOldLoading = loading;
		loading = true;

		PLS_DISABLE_UISTEP_V2(outputPage->easyOutputsPage);
		outputSimplePage.reset(new Ui::SettingOutputSimplePage);
		outputSimplePage->setupUi(outputPage->easyOutputsPage);

		connect(outputSimplePage->simpleOutputBrowse, &QPushButton::clicked, this,
			&OBSBasicSettings::on_simpleOutputBrowse_clicked);

		HookWidget(outputSimplePage->simpleOutputPath, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleNoSpace, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutRecFormat, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutputVBitrate, SCROLL_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutStrEncoder, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutStrAEncoder, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutputABitrate, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutAdvanced, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutPreset, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutCustom, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutRecQuality, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutRecEncoder, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutRecAEncoder, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutRecTrack1, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutRecTrack2, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutRecTrack3, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutRecTrack4, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutRecTrack5, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutRecTrack6, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleOutMuxCustom, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleReplayBuf, GROUP_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleRBSecMax, SCROLL_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputSimplePage->simpleRBMegsMax, SCROLL_CHANGED, OUTPUTS_CHANGED);

		outputSimplePage->simpleOutputVBitrate->setSingleStep(50);
		outputSimplePage->simpleOutputVBitrate->setSuffix(" Kbps");

		connect(outputSimplePage->simpleOutputVBitrate, &QSpinBox::valueChanged, this,
			&OBSBasicSettings::UpdateStreamDelayEstimate);
		connect(outputSimplePage->simpleOutputABitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::UpdateStreamDelayEstimate);

		LoadSimpleFormats();
		LoadSimpleOutputSettings();

		connect(outputSimplePage->simpleOutRecQuality, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::SimpleRecordingQualityChanged);
		connect(outputSimplePage->simpleOutRecQuality, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::SimpleRecordingQualityLosslessWarning);
		connect(outputSimplePage->simpleOutRecFormat, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::SimpleRecordingEncoderChanged);
		connect(outputSimplePage->simpleOutStrEncoder, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::SimpleStreamingEncoderChanged);
		connect(outputSimplePage->simpleOutStrEncoder, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::SimpleRecordingEncoderChanged);
		connect(outputSimplePage->simpleOutRecEncoder, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::SimpleRecordingEncoderChanged);
		connect(outputSimplePage->simpleOutRecAEncoder, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::SimpleRecordingEncoderChanged);
		connect(outputSimplePage->simpleOutputVBitrate, &QSpinBox::valueChanged, this,
			&OBSBasicSettings::SimpleRecordingEncoderChanged);
		connect(outputSimplePage->simpleOutputABitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::SimpleRecordingEncoderChanged);
		connect(outputSimplePage->simpleOutAdvanced, &PLSCheckBox::toggled, this,
			&OBSBasicSettings::SimpleRecordingEncoderChanged);
		connect(outputSimplePage->simpleReplayBuf, &QGroupBox::toggled, this,
			&OBSBasicSettings::SimpleReplayBufferChanged);
		connect(outputSimplePage->simpleOutputVBitrate, &QSpinBox::valueChanged, this,
			&OBSBasicSettings::SimpleReplayBufferChanged);
		connect(outputSimplePage->simpleOutputABitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::SimpleReplayBufferChanged);
		connect(outputSimplePage->simpleRBSecMax, &QSpinBox::valueChanged, this,
			&OBSBasicSettings::SimpleReplayBufferChanged);

		FillSimpleRecordingValues();
		ResetSimpleEncoders();

		if (obs_video_active()) {
			outputSimplePage->simpleOutStrEncoderLabel->setEnabled(false);
			outputSimplePage->simpleOutStrEncoder->setEnabled(false);
			outputSimplePage->simpleOutStrAEncoderLabel->setEnabled(false);
			outputSimplePage->simpleOutStrAEncoder->setEnabled(false);
			outputSimplePage->simpleRecordingGroupBox->setEnabled(false);
			outputSimplePage->simpleReplayBuf->setEnabled(false);
		}

		outputSimplePage->simpleOutRecEncoder->setPlaceholderText(QTStr("CodecCompat.CodecPlaceholder"));
		outputSimplePage->simpleOutRecAEncoder->setPlaceholderText(QTStr("CodecCompat.CodecPlaceholder"));
		outputSimplePage->simpleOutRecFormat->setPlaceholderText(QTStr("CodecCompat.ContainerPlaceholder"));

		outputSimplePage->simpleOutStrAEncoder->setPlaceholderText(QTStr("CodecCompat.CodecPlaceholder"));

		connect(outputSimplePage->simpleOutStrAEncoder, &PLSComboBox::currentIndexChanged, this,
			&OBSBasicSettings::SimpleStreamAudioEncoderChanged);

		auto simpleCheckboxs = outputSimplePage->simpleRecTracks->findChildren<PLSCheckBox *>();
		for (auto checkbox : simpleCheckboxs) {
			checkbox->setSpac(5);
		}

		alignLabels(ui->outputPage);
#ifdef Q_OS_MACOS
		pls_scroll_area_clips_to_bounds(outputSimplePage->simpleOutScroll);
#endif

		pls_uistep_v2_set_title(outputSimplePage->simpleStreamingGroupBox,
					QStringLiteral("Settings - Output - Simple - Streaming"));
		pls_uistep_v2_set_title(outputSimplePage->simpleRecordingGroupBox,
					QStringLiteral("Settings - Output - Simple - Recording"));
		pls_uistep_v2_auto_bind(outputPage->easyOutputsPage);
		pls_uistep_v2_set_custom_show_hide_name(outputSimplePage->simpleOutRecEncoderLabel,
							"Simple Video Encoder");
		loading = bOldLoading;
		UpdateVodTrackSetting();
	}
}

void OBSBasicSettings::initOutputStreamPage()
{
	if (!outputStreamPage) {
		auto bOldLoading = loading;
		loading = true;
		PLS_DISABLE_UISTEP_V2(outputPage->advOutputStreamTab);
		outputStreamPage.reset(new Ui::SettingOutputStreamPage);
		outputStreamPage->setupUi(outputPage->advOutputStreamTab);

		connect(outputStreamPage->advOutEncoder, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::on_advOutEncoder_currentIndexChanged);

		HookWidget(outputStreamPage->advOutEncoder, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutAEncoder, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutRescale, CBEDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutRescaleFilter, COMBO_CHANGED, OUTPUTS_CHANGED);

		HookWidget(outputStreamPage->advOutTrack1, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutTrack2, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutTrack3, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutTrack4, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutTrack5, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutTrack6, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutMultiTrack1, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutMultiTrack2, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutMultiTrack3, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutMultiTrack4, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutMultiTrack5, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutMultiTrack6, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutTrack1_2, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutTrack2_2, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutTrack3_2, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutTrack4_2, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutTrack5_2, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputStreamPage->advOutTrack6_2, RADIO_CHANGED, OUTPUTS_CHANGED);

		auto addScaleFilter = [&](const char *string, int value) -> void {
			outputStreamPage->advOutRescaleFilter->addItem(QTStr(string), value);
		};

		addScaleFilter("Basic.Settings.Output.Adv.Rescale.Disabled", OBS_SCALE_DISABLE);
		addScaleFilter("Basic.Settings.Video.DownscaleFilter.Bilinear", OBS_SCALE_BILINEAR);
		addScaleFilter("Basic.Settings.Video.DownscaleFilter.Area", OBS_SCALE_AREA);
		addScaleFilter("Basic.Settings.Video.DownscaleFilter.Bicubic", OBS_SCALE_BICUBIC);
		addScaleFilter("Basic.Settings.Video.DownscaleFilter.Lanczos", OBS_SCALE_LANCZOS);

		connect(outputStreamPage->advOutRescaleFilter, &QComboBox::currentIndexChanged, this, [this] {
			outputStreamPage->advOutRescale->setEnabled(
				outputStreamPage->advOutRescaleFilter->currentData() != OBS_SCALE_DISABLE);
		});

		uint32_t cx = config_get_uint(main->Config(), "Video", "BaseCX");
		uint32_t cy = config_get_uint(main->Config(), "Video", "BaseCY");
		ResetDownscales(cx, cy, false, true, false);

		ResetStreamEncoders();
		LoadAdvOutputStreamingSettings();
		LoadAdvOutputStreamingEncoderProperties();

		const char *type = config_get_string(main->Config(), "AdvOut", "AudioEncoder");
		if (!SetComboByValue(outputStreamPage->advOutAEncoder, type)) {
			outputStreamPage->advOutAEncoder->setCurrentIndex(0);
		}
		outputStreamPage->advOutAEncoder->setProperty("changed", QVariant(true));

		if (obs_video_active()) {
			outputStreamPage->advOutTopContainer->setEnabled(false);
		}

		outputStreamPage->advOutTrack1->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track1"));
		outputStreamPage->advOutTrack2->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track2"));
		outputStreamPage->advOutTrack3->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track3"));
		outputStreamPage->advOutTrack4->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track4"));
		outputStreamPage->advOutTrack5->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track5"));
		outputStreamPage->advOutTrack6->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track6"));

		outputStreamPage->advOutTrack1_2->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track1"));
		outputStreamPage->advOutTrack2_2->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track2"));
		outputStreamPage->advOutTrack3_2->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track3"));
		outputStreamPage->advOutTrack4_2->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track4"));
		outputStreamPage->advOutTrack5_2->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track5"));
		outputStreamPage->advOutTrack6_2->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track6"));

		outputStreamPage->advOutAEncoder->setPlaceholderText(QTStr("CodecCompat.CodecPlaceholder"));

		QRegularExpression rx("\\d{1,5}x\\d{1,5}");
		QValidator *validator = new QRegularExpressionValidator(rx, this);
		outputStreamPage->advOutRescale->lineEdit()->setValidator(validator);

		connect(outputStreamPage->advOutAEncoder, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvAudioEncodersChanged);

		if (pls_is_dual_output_on()) {
			showDualoutputSetting(false, true);
		} else {
			showNormalSetting(false, true);
		}

		alignLabels(ui->outputPage);
#ifdef Q_OS_MACOS
		pls_scroll_area_clips_to_bounds(outputStreamPage->scrollArea_3);
#endif
		pls_uistep_v2_auto_bind(outputPage->advOutputStreamTab);
		loading = bOldLoading;

		UpdateVodTrackSetting();
	}
}

void OBSBasicSettings::initOutputRecordPage()
{
	if (!outputRecordPage) {
		loading = true;
		PLS_DISABLE_UISTEP_V2(outputPage->advOutputRecordTab);
		outputRecordPage.reset(new Ui::SettingOutputRecordPage);
		outputRecordPage->setupUi(outputPage->advOutputRecordTab);

		connect(outputRecordPage->advOutRecPathBrowse, &QPushButton::clicked, this,
			&OBSBasicSettings::on_advOutRecPathBrowse_clicked);
		connect(outputRecordPage->advOutFFPathBrowse, &QPushButton::clicked, this,
			&OBSBasicSettings::on_advOutFFPathBrowse_clicked);

		connect(outputRecordPage->advOutRecEncoder, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::on_advOutRecEncoder_currentIndexChanged);
		connect(outputRecordPage->advOutFFIgnoreCompat, &PLSCheckBox::stateChanged, this,
			&OBSBasicSettings::on_advOutFFIgnoreCompat_stateChanged);
		connect(outputRecordPage->advOutFFFormat, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::on_advOutFFFormat_currentIndexChanged);
		connect(outputRecordPage->advOutFFAEncoder, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::on_advOutFFAEncoder_currentIndexChanged);
		connect(outputRecordPage->advOutFFVEncoder, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::on_advOutFFVEncoder_currentIndexChanged);
		connect(outputRecordPage->advOutFFType, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::on_advOutFFType_currentIndexChanged);

		HookWidget(outputRecordPage->advOutRecType, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutRecPath, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutNoSpace, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutRecFormat, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutRecEncoder, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutRecAEncoder, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutRecRescale, CBEDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutRecRescaleFilter, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutMuxCustom, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutSplitFile, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutSplitFileType, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutSplitFileTime, SCROLL_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutSplitFileSize, SCROLL_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutRecTrack1, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutRecTrack2, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutRecTrack3, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutRecTrack4, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutRecTrack5, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->flvTrack1, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->flvTrack2, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->flvTrack3, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->flvTrack4, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->flvTrack5, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->flvTrack6, RADIO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFType, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFRecPath, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFNoSpace, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFURL, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFFormat, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFMCfg, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFVBitrate, SCROLL_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFVGOPSize, SCROLL_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFUseRescale, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFIgnoreCompat, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFRescale, CBEDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFVEncoder, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFVCfg, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFABitrate, SCROLL_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFTrack1, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFTrack2, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFTrack3, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFTrack4, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFTrack5, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFTrack6, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFAEncoder, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputRecordPage->advOutFFACfg, EDIT_CHANGED, OUTPUTS_CHANGED);

		outputRecordPage->advOutFFVBitrate->setSingleStep(50);
		outputRecordPage->advOutFFVBitrate->setSuffix(" Kbps");
		outputRecordPage->advOutFFABitrate->setSuffix(" Kbps");

		LoadRecordFormats();

		ResetRecordEncoders();

		LoadAdvOutputRecordingSettings();
		LoadAdvOutputRecordingEncoderProperties();

		auto type = config_get_string(main->Config(), "AdvOut", "RecAudioEncoder");
		if (!SetComboByValue(outputRecordPage->advOutRecAEncoder, type))
			outputRecordPage->advOutRecAEncoder->setCurrentIndex(-1);

		LoadAdvOutputFFmpegSettings();

		if (obs_video_active()) {
			outputRecordPage->advOutRecTopContainer->setEnabled(false);
			outputRecordPage->advOutRecTypeContainer->setEnabled(false);
		}

		connect(outputRecordPage->advOutSplitFile, &PLSCheckBox::stateChanged, this,
			&OBSBasicSettings::AdvOutSplitFileChanged);
		connect(outputRecordPage->advOutSplitFileType, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvOutSplitFileChanged);

		connect(outputRecordPage->advOutRecTrack1, &PLSCheckBox::toggled, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
		connect(outputRecordPage->advOutRecTrack2, &PLSCheckBox::toggled, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
		connect(outputRecordPage->advOutRecTrack3, &PLSCheckBox::toggled, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
		connect(outputRecordPage->advOutRecTrack4, &PLSCheckBox::toggled, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
		connect(outputRecordPage->advOutRecTrack5, &PLSCheckBox::toggled, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
		connect(outputRecordPage->advOutRecTrack6, &PLSCheckBox::toggled, this,
			&OBSBasicSettings::AdvReplayBufferChanged);

		connect(outputRecordPage->advOutRecType, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
		connect(outputRecordPage->advOutRecType, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvOutRecCheckWarnings);
		connect(outputRecordPage->advOutRecEncoder, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvReplayBufferChanged);

		auto addScaleFilter = [&](const char *string, int value) -> void {
			outputRecordPage->advOutRecRescaleFilter->addItem(QTStr(string), value);
		};

		addScaleFilter("Basic.Settings.Output.Adv.Rescale.Disabled", OBS_SCALE_DISABLE);
		addScaleFilter("Basic.Settings.Video.DownscaleFilter.Bilinear", OBS_SCALE_BILINEAR);
		addScaleFilter("Basic.Settings.Video.DownscaleFilter.Area", OBS_SCALE_AREA);
		addScaleFilter("Basic.Settings.Video.DownscaleFilter.Bicubic", OBS_SCALE_BICUBIC);
		addScaleFilter("Basic.Settings.Video.DownscaleFilter.Lanczos", OBS_SCALE_LANCZOS);

		connect(outputRecordPage->advOutRecRescaleFilter, &QComboBox::currentIndexChanged, this, [this] {
			outputRecordPage->advOutRecRescale->setEnabled(
				outputRecordPage->advOutRecRescaleFilter->currentData() != OBS_SCALE_DISABLE);
		});

		uint32_t cx = config_get_uint(main->Config(), "Video", "BaseCX");
		uint32_t cy = config_get_uint(main->Config(), "Video", "BaseCY");
		ResetDownscales(cx, cy, false, false, true);

		outputRecordPage->advOutRecTrack1->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track1"));
		outputRecordPage->advOutRecTrack2->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track2"));
		outputRecordPage->advOutRecTrack3->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track3"));
		outputRecordPage->advOutRecTrack4->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track4"));
		outputRecordPage->advOutRecTrack5->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track5"));
		outputRecordPage->advOutRecTrack6->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track6"));

		outputRecordPage->advOutFFTrack1->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track1"));
		outputRecordPage->advOutFFTrack2->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track2"));
		outputRecordPage->advOutFFTrack3->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track3"));
		outputRecordPage->advOutFFTrack4->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track4"));
		outputRecordPage->advOutFFTrack5->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track5"));
		outputRecordPage->advOutFFTrack6->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Audio.Track6"));

		outputRecordPage->label_31->setAccessibleName(QTStr("Basic.Settings.Output.Adv.Recording.RecType"));

		connect(outputRecordPage->advOutRecTrack1, &PLSCheckBox::clicked, this,
			&OBSBasicSettings::AdvOutRecCheckWarnings);
		connect(outputRecordPage->advOutRecTrack2, &PLSCheckBox::clicked, this,
			&OBSBasicSettings::AdvOutRecCheckWarnings);
		connect(outputRecordPage->advOutRecTrack3, &PLSCheckBox::clicked, this,
			&OBSBasicSettings::AdvOutRecCheckWarnings);
		connect(outputRecordPage->advOutRecTrack4, &PLSCheckBox::clicked, this,
			&OBSBasicSettings::AdvOutRecCheckWarnings);
		connect(outputRecordPage->advOutRecTrack5, &PLSCheckBox::clicked, this,
			&OBSBasicSettings::AdvOutRecCheckWarnings);
		connect(outputRecordPage->advOutRecTrack6, &PLSCheckBox::clicked, this,
			&OBSBasicSettings::AdvOutRecCheckWarnings);
		connect(outputRecordPage->advOutRecFormat, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvOutRecCheckWarnings);
		connect(outputRecordPage->advOutRecEncoder, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvOutRecCheckWarnings);

		connect(outputRecordPage->advOutRecFormat, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvOutRecCheckCodecs);

		outputRecordPage->advOutRecEncoder->setPlaceholderText(QTStr("CodecCompat.CodecPlaceholder"));
		outputRecordPage->advOutRecAEncoder->setPlaceholderText(QTStr("CodecCompat.CodecPlaceholder"));

		AdvOutSplitFileChanged();
		AdvOutRecCheckCodecs();
		AdvOutRecCheckWarnings();

		QRegularExpression rx("\\d{1,5}x\\d{1,5}");
		QValidator *validator = new QRegularExpressionValidator(rx, this);
		outputRecordPage->advOutRecRescale->lineEdit()->setValidator(validator);
		outputRecordPage->advOutFFRescale->lineEdit()->setValidator(validator);

		connect(outputRecordPage->advOutRecAEncoder, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvAudioEncodersChanged);

		auto advCheckboxs = outputRecordPage->recTracks->findChildren<PLSCheckBox *>();
		for (auto checkbox : advCheckboxs) {
			checkbox->setSpac(5);
		}

		auto advOutFFTracks = outputRecordPage->widget_10->findChildren<PLSCheckBox *>();
		for (auto checkbox : advOutFFTracks) {
			checkbox->setSpac(5);
		}

		alignLabels(ui->outputPage);
#ifdef Q_OS_MACOS
		pls_scroll_area_clips_to_bounds(outputRecordPage->scrollArea_4);
		pls_scroll_area_clips_to_bounds(outputRecordPage->scrollArea_5);
#endif
		pls_uistep_v2_auto_bind(outputPage->advOutputRecordTab);
		pls_uistep_v2_set_custom_show_hide_name(outputRecordPage->advOutRecStandard, "Rec Standard Page");
		pls_uistep_v2_set_custom_show_hide_name(outputRecordPage->advOutRecFFmpegPage, "Rec FFmpeg page");
		loading = false;
	}
}

void OBSBasicSettings::initOutputAudioPage()
{
	if (!outputAudioPage) {
		loading = true;
		PLS_DISABLE_UISTEP_V2(outputPage->advOutputAudioTracksTab);
		outputAudioPage.reset(new Ui::SettingOutputAudioPage);
		outputAudioPage->setupUi(outputPage->advOutputAudioTracksTab);

		HookWidget(outputAudioPage->advOutTrack1Bitrate, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputAudioPage->advOutTrack1Name, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputAudioPage->advOutTrack2Bitrate, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputAudioPage->advOutTrack2Name, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputAudioPage->advOutTrack3Bitrate, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputAudioPage->advOutTrack3Name, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputAudioPage->advOutTrack4Bitrate, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputAudioPage->advOutTrack4Name, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputAudioPage->advOutTrack5Bitrate, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputAudioPage->advOutTrack5Name, EDIT_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputAudioPage->advOutTrack6Bitrate, COMBO_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputAudioPage->advOutTrack6Name, EDIT_CHANGED, OUTPUTS_CHANGED);

		connect(outputAudioPage->advOutTrack1Bitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::UpdateStreamDelayEstimate);
		connect(outputAudioPage->advOutTrack2Bitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::UpdateStreamDelayEstimate);
		connect(outputAudioPage->advOutTrack3Bitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::UpdateStreamDelayEstimate);
		connect(outputAudioPage->advOutTrack4Bitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::UpdateStreamDelayEstimate);
		connect(outputAudioPage->advOutTrack5Bitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::UpdateStreamDelayEstimate);
		connect(outputAudioPage->advOutTrack6Bitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::UpdateStreamDelayEstimate);

		connect(outputAudioPage->advOutTrack1Bitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
		connect(outputAudioPage->advOutTrack2Bitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
		connect(outputAudioPage->advOutTrack3Bitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
		connect(outputAudioPage->advOutTrack4Bitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
		connect(outputAudioPage->advOutTrack5Bitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
		connect(outputAudioPage->advOutTrack6Bitrate, &QComboBox::currentIndexChanged, this,
			&OBSBasicSettings::AdvReplayBufferChanged);

		LoadAdvOutputAudioSettings();

		alignLabels(ui->outputPage);
#ifdef Q_OS_MACOS
		pls_scroll_area_clips_to_bounds(outputAudioPage->scrollArea_6);
#endif
		pls_uistep_v2_auto_bind(outputPage->advOutputAudioTracksTab);
		loading = false;
	}
}

void OBSBasicSettings::initOutputReplayPage()
{
	if (!outputReplayPage) {
		loading = true;
		PLS_DISABLE_UISTEP_V2(outputPage->advOutputReplayTab);
		outputReplayPage.reset(new Ui::SettingOutputReplayPage);
		outputReplayPage->setupUi(outputPage->advOutputReplayTab);

		HookWidget(outputReplayPage->advReplayBuf, CHECK_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputReplayPage->advRBSecMax, SCROLL_CHANGED, OUTPUTS_CHANGED);
		HookWidget(outputReplayPage->advRBMegsMax, SCROLL_CHANGED, OUTPUTS_CHANGED);

		connect(outputReplayPage->advReplayBuf, &PLSCheckBox::toggled, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
		connect(outputReplayPage->advRBSecMax, &QSpinBox::valueChanged, this,
			&OBSBasicSettings::AdvReplayBufferChanged);

		LoadAdvOutputReplaySettings();

		alignLabels(ui->outputPage);

		pls_uistep_v2_auto_bind(outputPage->advOutputReplayTab);
		loading = false;
	}
}

bool OBSBasicSettings::settingsPageNeedsInit(int row) const
{
	switch (row) {
	case Pages::GENERAL:
		return !generalPage;
	case Pages::OUTPUT:
		return !outputPage;
	case Pages::AUDIO:
		return !audioPage;
	case Pages::VIDEO:
		return !videoPage;
	case Pages::HOTKEYS:
		return !hotkeyPage;
	case Pages::ACCESSIBILITY:
		return !accessPage;
	case Pages::ADVANCED:
		return !advancedPage;
	default:
		return false;
	}
}

//PRISM/sonic.yang/20260421/PRISM_PC-5879/settings page lazy-init loading overlay (sibling of stack, not a stacked page)
void OBSBasicSettings::syncSettingsPageLoadingOverlay(int row)
{
	PLSLoadingView::deleteLoadingView(m_settingsPageLoadingView);
	if (!settingsPageNeedsInit(row)) {
		return;
	}
	QWidget *stackParent = ui->settingsPages->parentWidget();
	if (!stackParent) {
		return;
	}
	m_settingsPageLoadingView = PLSLoadingView::newLoadingView(
		stackParent, -1,
		[this](QRect &geometry, PLSLoadingView *) {
			geometry = ui->settingsPages->geometry();
			return true;
		},
		QString(), QColor(39, 39, 39, 255));
}

void OBSBasicSettings::hideSettingsPageLoadingIfStillCurrentRow(int scheduledRow)
{
	if (scheduledRow != ui->listWidget->currentRow()) {
		return;
	}
	PLSLoadingView::deleteLoadingView(m_settingsPageLoadingView);
}

void OBSBasicSettings::on_listWidget_currentRowChanged(int row)
{
	PLS_PERFORMANCE_FUNCTION();
	ui->listWidget->repaint();

	//PRISM/sonic.yang/20260421/PRISM_PC-5879/settings page lazy-init loading overlay
	syncSettingsPageLoadingOverlay(row);
	if (row >= 0 && row < ui->settingsPages->count()) {
		ui->settingsPages->setCurrentIndex(row);
	}

	// Defer tab init to next event loop to reduce time from onPopupSettingView to emit shown()
	pls_async_call(this, [this, row]() {
		switch (row) {
		case Pages::ACCESSIBILITY:
			initAccessPage();
			break;

		case Pages::ADVANCED:
			initAdvancedPage();
			break;

		case Pages::AUDIO:
			initAudioPage();
			break;

		case Pages::GENERAL:
			initGeneralage();
			break;

		case Pages::HOTKEYS:
			initHotkeyPage();
			break;

		case Pages::OUTPUT:
			initOutputPage();
			break;

		case Pages::VIDEO:
			initVideoPage();
			break;

		default:
			break;
		}

		if (loading || row == pageIndex) {
			return;
		}

		if (row == Pages::HOTKEYS && (!hotkeysLoaded || bReloadHotKey)) {
			setCursor(Qt::BusyCursor);
			/* Look, I know this /feels/ wrong, but the specific issue we're dealing with
			 * here means that the UI locks up immediately even when using "invokeMethod".
			 * So the only way for the user to see the loading message on the page is to
			 * give the Qt event loop a tiny bit of time to switch to the hotkey page,
			 * and only then start loading. This could maybe be done by subclassing QWidget
			 * for the hotkey page and then using showEvent() but I *really* don't want
			 * to deal with that right now. I've got better things to do with my life
			 * than to work around this god damn stupid issue for something we'll remove
			 * soon enough anyway. So this solution it is. */
			QTimer::singleShot(1, this, [this]() {
				PLS_INFO("setting", "singleShot LoadHotkeySettings");
				LoadHotkeySettings();
			});
		} else if (row == Pages::OUTPUT && serviceDualOutput != pls_is_dual_output_on()) {
			LoadServices(false);
			on_service_currentIndexChanged(outputPage->service->currentIndex());
		} else if (row == Pages::AUDIO && bReloadAudioSources) {
			LoadAudioSources();
			alignLabels(ui->audioPage);
		}

		pageIndex = row;
		ui->listWidget->repaint();
	});
}

void OBSBasicSettings::UpdateYouTubeAppDockSettings()
{
#if defined(BROWSER_AVAILABLE) && defined(YOUTUBE_ENABLED)
	if (cef_js_avail) {
		std::string service = ui->service->currentText().toStdString();
		if (IsYouTubeService(service)) {
			if (!main->GetYouTubeAppDock()) {
				main->NewYouTubeAppDock();
			}
			main->GetYouTubeAppDock()->SettingsUpdated(!IsYouTubeService(service) || stream1Changed);
		} else {
			if (main->GetYouTubeAppDock()) {
				main->GetYouTubeAppDock()->AccountDisconnected();
			}
			main->DeleteYouTubeAppDock();
		}
	}
#endif
}

void OBSBasicSettings::on_buttonBox_clicked(QAbstractButton *button)
{
	QDialogButtonBox::ButtonRole val = ui->buttonBox->buttonRole(button);

	if (val == QDialogButtonBox::ApplyRole || val == QDialogButtonBox::AcceptRole) {
		if (!QueryAllowedToClose())
			return;

		if (generalPage) {
			if (pls_get_locale() != generalPage->language->currentData()) {
				if (PLSAlertView::Button::Yes ==
				    PLSAlertView::warning(this, QTStr("Basic.Settings.ConfirmTitle"),
							  QTStr("Basic.Settings.General.language.changed"),
							  PLSAlertView::Button::Yes | PLSAlertView::Button::No)) {
					SaveSettings();
					ClearChanged();
					done(Qt::UserRole + RESTARTAPP);
					return;
				} else {
					generalPage->language->setCurrentText(
						QString::fromStdString(m_currentLanguage.second));
				}
			}
		}

		SaveSettings();

		UpdateYouTubeAppDockSettings();
		ClearChanged();

		bool audioRestart = false;
		if (audioPage) {
			audioRestart = (audioPage->channelSetup->currentIndex() != channelIndex ||
					audioPage->sampleRate->currentIndex() != sampleRateIndex);
		}
		bool browserHWAccelChanged = false;
		if (advancedPage) {
			browserHWAccelChanged = advancedPage->browserHWAccel &&
						advancedPage->browserHWAccel->isChecked() != prevBrowserAccel;
		}

		if (audioRestart || browserHWAccelChanged) {
			setResult(Qt::UserRole + NEED_RESTARTAPP);
		}
	}
	if (val == QDialogButtonBox::RejectRole) {
		setResult(QDialogButtonBox::RejectRole);
	}
	if (val == QDialogButtonBox::AcceptRole || val == QDialogButtonBox::RejectRole) {
		ClearChanged();
		close();
	}
	PLS_UI_ACTION("OBSBasicSettings apply clicked");
}

void OBSBasicSettings::on_simpleOutputBrowse_clicked()
{
	QString dir = SelectDirectory(this, QTStr("Basic.Settings.Output.SelectDirectory"),
				      outputSimplePage->simpleOutputPath->text());
	if (dir.isEmpty())
		return;

	outputSimplePage->simpleOutputPath->setText(dir);
}

void OBSBasicSettings::on_advOutRecPathBrowse_clicked()
{
	QString dir = SelectDirectory(this, QTStr("Basic.Settings.Output.SelectDirectory"),
				      outputRecordPage->advOutRecPath->text());
	if (dir.isEmpty())
		return;

	outputRecordPage->advOutRecPath->setText(dir);
}

void OBSBasicSettings::on_advOutFFPathBrowse_clicked()
{
	QString dir = SelectDirectory(this, QTStr("Basic.Settings.Output.SelectDirectory"),
				      outputRecordPage->advOutRecPath->text());
	if (dir.isEmpty())
		return;

	outputRecordPage->advOutFFRecPath->setText(dir);
}

void OBSBasicSettings::on_advOutEncoder_currentIndexChanged()
{
	QString encoder = GetComboData(outputStreamPage->advOutEncoder);
	if (!loading) {
		bool loadSettings = encoder == curAdvStreamEncoder;

		delete streamEncoderProps;
		streamEncoderProps = CreateEncoderPropertyView(
			QT_TO_UTF8(encoder), loadSettings ? "streamEncoder.json" : nullptr, true, true);
		streamEncoderProps->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
		outputStreamPage->advOutEncoderLayout->addWidget(streamEncoderProps);
	}
	AdvOutStreamEncoderCheckWarnings();
	outputStreamPage->advOutUseRescale->setVisible(true);
	outputStreamPage->advOutRescale->setVisible(true);
}

void OBSBasicSettings::on_advOutRecEncoder_currentIndexChanged(int idx)
{
	if (!loading) {
		delete recordEncoderProps;
		recordEncoderProps = nullptr;
	}

	auto setRescaleVisible = [=](bool visible) {
		if (visible) {
			outputRecordPage->formLayout_16->setWidget(7, QFormLayout::LabelRole,
								   outputRecordPage->advOutRecUseRescale);
			outputRecordPage->formLayout_16->setWidget(7, QFormLayout::FieldRole,
								   outputRecordPage->advOutRecRescaleContainer);
			outputRecordPage->advOutRecRescaleContainer->show();
		} else {
			layoutRemoveWidget(outputRecordPage->formLayout_16,
					   outputRecordPage->advOutRecRescaleContainer);
			outputRecordPage->formLayout_16->removeWidget(outputRecordPage->advOutRecUseRescale);
		}
	};

	if (idx <= 0) {
		outputRecordPage->advOutRecUseRescale->setVisible(false);
		outputRecordPage->advOutRecEncoderProps->setVisible(false);
		setRescaleVisible(false);
		return;
	}

	QString encoder = GetComboData(outputRecordPage->advOutRecEncoder);
	bool loadSettings = encoder == curAdvRecordEncoder;

	if (!loading) {
		recordEncoderProps = CreateEncoderPropertyView(QT_TO_UTF8(encoder),
							       loadSettings ? "recordEncoder.json" : nullptr, true);
		recordEncoderProps->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
		outputRecordPage->advOutRecEncoderProps->layout()->addWidget(recordEncoderProps);
		connect(recordEncoderProps, &OBSPropertiesView::Changed, this,
			&OBSBasicSettings::AdvReplayBufferChanged);
	}

	outputRecordPage->advOutRecUseRescale->setVisible(true);
	outputRecordPage->advOutRecEncoderProps->setVisible(true);
	setRescaleVisible(true);
}

void OBSBasicSettings::on_advOutFFIgnoreCompat_stateChanged(int)
{
	/* Little hack to reload codecs when checked */
	on_advOutFFFormat_currentIndexChanged(outputRecordPage->advOutFFFormat->currentIndex());
}

#define DEFAULT_CONTAINER_STR QTStr("Basic.Settings.Output.Adv.FFmpeg.FormatDescDef")

void OBSBasicSettings::on_advOutFFFormat_currentIndexChanged(int idx)
{
	const QVariant itemDataVariant = outputRecordPage->advOutFFFormat->itemData(idx);

	if (!itemDataVariant.isNull()) {
		auto format = itemDataVariant.value<FFmpegFormat>();
		SetAdvOutputFFmpegEnablement(FFmpegCodecType::AUDIO, format.HasAudio(), false);
		SetAdvOutputFFmpegEnablement(FFmpegCodecType::VIDEO, format.HasVideo(), false);
		ReloadCodecs(format);

		outputRecordPage->advOutFFFormatDesc->setText(format.long_name);

		FFmpegCodec defaultAudioCodecDesc = format.GetDefaultEncoder(FFmpegCodecType::AUDIO);
		FFmpegCodec defaultVideoCodecDesc = format.GetDefaultEncoder(FFmpegCodecType::VIDEO);
		SelectEncoder(outputRecordPage->advOutFFAEncoder, defaultAudioCodecDesc.name, defaultAudioCodecDesc.id);
		SelectEncoder(outputRecordPage->advOutFFVEncoder, defaultVideoCodecDesc.name, defaultVideoCodecDesc.id);
	} else {
		outputRecordPage->advOutFFAEncoder->blockSignals(true);
		outputRecordPage->advOutFFVEncoder->blockSignals(true);
		outputRecordPage->advOutFFAEncoder->clear();
		outputRecordPage->advOutFFVEncoder->clear();

		outputRecordPage->advOutFFFormatDesc->setText(DEFAULT_CONTAINER_STR);
	}
}

void OBSBasicSettings::on_advOutFFAEncoder_currentIndexChanged(int idx)
{
	const QVariant itemDataVariant = outputRecordPage->advOutFFAEncoder->itemData(idx);
	if (!itemDataVariant.isNull()) {
		auto desc = itemDataVariant.value<FFmpegCodec>();
		SetAdvOutputFFmpegEnablement(FFmpegCodecType::AUDIO, desc.id != 0 || desc.name != nullptr, true);
	}
}

void OBSBasicSettings::on_advOutFFVEncoder_currentIndexChanged(int idx)
{
	const QVariant itemDataVariant = outputRecordPage->advOutFFVEncoder->itemData(idx);
	if (!itemDataVariant.isNull()) {
		auto desc = itemDataVariant.value<FFmpegCodec>();
		SetAdvOutputFFmpegEnablement(FFmpegCodecType::VIDEO, desc.id != 0 || desc.name != nullptr, true);
	}
}

void OBSBasicSettings::on_advOutFFType_currentIndexChanged(int idx)
{
	outputRecordPage->advOutFFNoSpace->setHidden(idx != 0);
}

void OBSBasicSettings::on_colorFormat_currentIndexChanged(int)
{
	UpdateColorFormatSpaceWarning();
}

void OBSBasicSettings::on_colorSpace_currentIndexChanged(int)
{
	UpdateColorFormatSpaceWarning();
}

#define INVALID_RES_STR "Basic.Settings.Video.InvalidResolution"

static bool ValidResolutions(OBSBasicSettings *settings, PLSEditableComboBox *baseResolution,
			     PLSEditableComboBox *outputResolution)
{
	QString baseRes = baseResolution->lineEdit()->text();
	uint32_t cx, cy;

	if (!ConvertResText(QT_TO_UTF8(baseRes), cx, cy)) {
		settings->updateAlertMessage(OBSBasicSettings::AlertMessageType::Error, baseResolution,
					     QTStr(INVALID_RES_STR));
		return false;
	} else {
		settings->clearAlertMessage(OBSBasicSettings::AlertMessageType::Error, baseResolution);
	}

	bool lockedOutRes = !outputResolution->isEditable();
	if (!lockedOutRes) {
		QString outRes = outputResolution->lineEdit()->text();
		if (!ConvertResText(QT_TO_UTF8(outRes), cx, cy)) {
			settings->updateAlertMessage(OBSBasicSettings::AlertMessageType::Error, outputResolution,
						     QTStr(INVALID_RES_STR));
			return false;
		} else {
			settings->clearAlertMessage(OBSBasicSettings::AlertMessageType::Error, outputResolution);
		}
	}
	return true;
}

void OBSBasicSettings::RecalcOutputResPixels(const char *resText)
{
	uint32_t newCX;
	uint32_t newCY;

	if (ConvertResText(resText, newCX, newCY) && newCX && newCY) {
		outputCX = newCX;
		outputCY = newCY;

		std::tuple<int, int> aspect = aspect_ratio(outputCX, outputCY);

		videoPage->scaledAspect->setText(
			QTStr("AspectRatio")
				.arg(QString::number(std::get<0>(aspect)), QString::number(std::get<1>(aspect))));
	}
}

void OBSBasicSettings::RecalcResPixels(QLabel *label, const char *resText)
{
	uint32_t newCX;
	uint32_t newCY;

	if (ConvertResText(resText, newCX, newCY) && newCX && newCY) {
		std::tuple<int, int> aspect = aspect_ratio(newCX, newCY);

		label->setText(
			QTStr("AspectRatio")
				.arg(QString::number(std::get<0>(aspect)), QString::number(std::get<1>(aspect))));
	}
}

bool OBSBasicSettings::AskIfCanCloseSettings()
{
	bool canCloseSettings = false;

	if (!Changed() || QueryChanges())
		canCloseSettings = true;

	if (forceAuthReload) {
		main->auth->Save();
		main->auth->Load();
		forceAuthReload = false;
	}

	if (forceUpdateCheck) {
		main->CheckForUpdates(false);
		forceUpdateCheck = false;
	}

	return canCloseSettings;
}

void OBSBasicSettings::on_filenameFormatting_textEdited(const QString &text)
{
	QString safeStr = text;

#ifdef __APPLE__
	safeStr.replace(QRegularExpression("[:]"), "");
#elif defined(_WIN32)
	safeStr.replace(QRegularExpression("[<>:\"\\|\\?\\*]"), "");
#else
	// TODO: Add filtering for other platforms
#endif

	if (text != safeStr)
		advancedPage->filenameFormatting->setText(safeStr);
}

void OBSBasicSettings::on_outputResolution_editTextChanged(const QString &text)
{
	if (!loading) {
		RecalcOutputResPixels(QT_TO_UTF8(text));
		LoadDownscaleFilters(true);
	}
}

void OBSBasicSettings::on_baseResolution_editTextChanged(const QString &text)
{
	if (!loading && ValidResolutions(this, videoPage->baseResolution, videoPage->outputResolution)) {
		QString baseResolution = text;
		uint32_t cx, cy;

		ConvertResText(QT_TO_UTF8(baseResolution), cx, cy);

		std::tuple<int, int> aspect = aspect_ratio(cx, cy);

		videoPage->baseAspect->setText(
			QTStr("AspectRatio")
				.arg(QString::number(std::get<0>(aspect)), QString::number(std::get<1>(aspect))));

		ResetDownscales(cx, cy, true, true, true);
		LoadDownscaleFilters(true);
	}
}

void OBSBasicSettings::GeneralChanged()
{
	if (!loading) {
		generalChanged = true;
		sender()->setProperty("changed", QVariant(true));
		EnableApplyButton(true);

		updateAlertMessage();
	}
}

void OBSBasicSettings::Stream1Changed()
{
	if (!loading) {
		stream1Changed = true;
		sender()->setProperty("changed", QVariant(true));
		EnableApplyButton(true);
	}
}

void OBSBasicSettings::OutputsChanged()
{
	if (!loading) {
		outputsChanged = true;
		if (sender()) {
			sender()->setProperty("changed", QVariant(true));
		}
		EnableApplyButton(true);

		UpdateMultitrackVideo();
	}
}

void OBSBasicSettings::AudioChanged()
{
	if (!loading) {
		audioChanged = true;
		sender()->setProperty("changed", QVariant(true));
		EnableApplyButton(true);
	}
}

void OBSBasicSettings::AudioChangedRestart()
{
	if (!loading) {
		int currentChannelIndex = audioPage->channelSetup->currentIndex();
		int currentSampleRateIndex = audioPage->sampleRate->currentIndex();
		bool currentLLAudioBufVal = audioPage->lowLatencyBuffering->isChecked();
		QWidget *page = getPageOfSender();
		if (currentChannelIndex != channelIndex || currentSampleRateIndex != sampleRateIndex ||
		    currentLLAudioBufVal != llBufferingEnabled) {
			updateAlertMessage(AlertMessageType::Error, page, QTStr("Basic.Settings.ProgramRestart"));
		} else {
			clearAlertMessage(AlertMessageType::Error, page);
		}

		audioChanged = true;
		sender()->setProperty("changed", QVariant(true));
		EnableApplyButton(true);
	}
}

void OBSBasicSettings::ReloadAudioSources()
{
	if (Pages::AUDIO == ui->listWidget->currentRow()) {
		LoadAudioSources();
		alignLabels(ui->audioPage);
	} else {
		bReloadAudioSources = true;
	}
}

#define MULTI_CHANNEL_WARNING "Basic.Settings.Audio.MultichannelWarning"

void OBSBasicSettings::SpeakerLayoutChanged(int idx)
{
	auto config = main->Config();
#define LIMIT_MAX_BITRATE(section, name) \
	config_set_int(config, section, name, qMin(config_get_int(config, section, name), 320))

	if (outputPage) {
		QString speakerLayoutQstr = audioPage->channelSetup->itemText(idx);
		std::string speakerLayout = QT_TO_UTF8(speakerLayoutQstr);
		bool surround = IsSurround(speakerLayout.c_str());
		bool isOpus = (outputSimplePage ? outputSimplePage->simpleOutStrAEncoder->currentData().toString()
						: outputStreamPage->advOutAEncoder->currentData().toString()) == "opus";

		if (surround) {
			if (outputSimplePage) {
				PopulateSimpleBitrates(outputSimplePage->simpleOutputABitrate, isOpus);
			}

			string stream_encoder_id =
				outputStreamPage
					? outputStreamPage->advOutAEncoder->currentData().toString().toStdString()
					: config_get_string(main->Config(), "AdvOut", "AudioEncoder");
			string record_encoder_id =
				outputRecordPage
					? outputRecordPage->advOutRecAEncoder->currentData().toString().toStdString()
					: config_get_string(main->Config(), "AdvOut", "RecAudioEncoder");

			if (outputAudioPage) {
				PopulateAdvancedBitrates(
					{outputAudioPage->advOutTrack1Bitrate, outputAudioPage->advOutTrack2Bitrate,
					 outputAudioPage->advOutTrack3Bitrate, outputAudioPage->advOutTrack4Bitrate,
					 outputAudioPage->advOutTrack5Bitrate, outputAudioPage->advOutTrack6Bitrate},
					stream_encoder_id.c_str(),
					record_encoder_id == "none" ? stream_encoder_id.c_str()
								    : record_encoder_id.c_str());
			}
		} else {
			if (outputSimplePage) {
				RestrictResetBitrates(
					{
						outputSimplePage->simpleOutputABitrate,
					},
					320);

				SaveCombo(outputSimplePage->simpleOutputABitrate, "SimpleOutput", "ABitrate");
			} else {
				LIMIT_MAX_BITRATE("SimpleOutput", "ABitrate");
			}

			if (outputAudioPage) {
				RestrictResetBitrates(
					{outputAudioPage->advOutTrack1Bitrate, outputAudioPage->advOutTrack2Bitrate,
					 outputAudioPage->advOutTrack3Bitrate, outputAudioPage->advOutTrack4Bitrate,
					 outputAudioPage->advOutTrack5Bitrate, outputAudioPage->advOutTrack6Bitrate},
					320);

				SaveCombo(outputAudioPage->advOutTrack1Bitrate, "AdvOut", "Track1Bitrate");
				SaveCombo(outputAudioPage->advOutTrack2Bitrate, "AdvOut", "Track2Bitrate");
				SaveCombo(outputAudioPage->advOutTrack3Bitrate, "AdvOut", "Track3Bitrate");
				SaveCombo(outputAudioPage->advOutTrack4Bitrate, "AdvOut", "Track4Bitrate");
				SaveCombo(outputAudioPage->advOutTrack5Bitrate, "AdvOut", "Track5Bitrate");
				SaveCombo(outputAudioPage->advOutTrack6Bitrate, "AdvOut", "Track6Bitrate");
			} else {
				LIMIT_MAX_BITRATE("AdvOut", "Track1Bitrate");
				LIMIT_MAX_BITRATE("AdvOut", "Track2Bitrate");
				LIMIT_MAX_BITRATE("AdvOut", "Track3Bitrate");
				LIMIT_MAX_BITRATE("AdvOut", "Track4Bitrate");
				LIMIT_MAX_BITRATE("AdvOut", "Track5Bitrate");
				LIMIT_MAX_BITRATE("AdvOut", "Track6Bitrate");
			}
		}
	} else {
		LIMIT_MAX_BITRATE("SimpleOutput", "ABitrate");

		LIMIT_MAX_BITRATE("AdvOut", "Track1Bitrate");
		LIMIT_MAX_BITRATE("AdvOut", "Track2Bitrate");
		LIMIT_MAX_BITRATE("AdvOut", "Track3Bitrate");
		LIMIT_MAX_BITRATE("AdvOut", "Track4Bitrate");
		LIMIT_MAX_BITRATE("AdvOut", "Track5Bitrate");
		LIMIT_MAX_BITRATE("AdvOut", "Track6Bitrate");
	}

	UpdateAudioWarnings();
}

void OBSBasicSettings::HideOBSWindowWarning(int state)
{
	if (loading || state == Qt::Unchecked)
		return;
	PLSErrorHandler::ExtraData extraData("OBSBasicSettings");
	if (config_get_bool(App()->GetUserConfig(), "General", "WarnedAboutHideOBSFromCapture"))
		return;

	PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_BASIC_SETTINGS_GENERAL_HIDE_OBS_CAPTURE_MESSAGE,
					      PLSErrKeyAllAlert, {}, extraData);

	config_set_bool(App()->GetUserConfig(), "General", "WarnedAboutHideOBSFromCapture", true);
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
}

/*
 * resets current bitrate if too large and restricts the number of bitrates
 * displayed when multichannel OFF
 */

void RestrictResetBitrates(initializer_list<QComboBox *> boxes, int maxbitrate)
{
	for (auto box : boxes) {
		int idx = box->currentIndex();
		int max_bitrate = FindClosestAvailableAudioBitrate(box, maxbitrate);
		int count = box->count();
		int max_idx = box->findText(QT_UTF8(std::to_string(max_bitrate).c_str()));

		for (int i = (count - 1); i > max_idx; i--)
			box->removeItem(i);

		if (idx > max_idx) {
			int default_bitrate = FindClosestAvailableAudioBitrate(box, maxbitrate / 2);
			int default_idx = box->findText(QT_UTF8(std::to_string(default_bitrate).c_str()));

			box->setCurrentIndex(default_idx);
			box->setProperty("changed", QVariant(true));
		} else {
			box->setCurrentIndex(idx);
		}
	}
}

void OBSBasicSettings::AdvancedChangedRestart()
{
	if (!loading) {
		advancedChanged = true;
		QWidget *page = getPageOfSender();
		updateAlertMessage(AlertMessageType::Error, page, QTStr("Basic.Settings.ProgramRestart"));
		sender()->setProperty("changed", QVariant(true));
		EnableApplyButton(true);
	}
}

void OBSBasicSettings::VideoChangedResolution()
{
	if (!loading && ValidResolutions(this, videoPage->baseResolution, videoPage->outputResolution)) {
		videoChanged = true;
		sender()->setProperty("changed", QVariant(true));
		EnableApplyButton(true);
	}
}

void OBSBasicSettings::VideoChanged()
{
	if (!loading) {
		videoChanged = true;
		sender()->setProperty("changed", QVariant(true));
		EnableApplyButton(true);
	}
}

void OBSBasicSettings::HotkeysChanged()
{
	using namespace std;
	if (loading)
		return;

	hotkeysChanged = any_of(begin(hotkeys), end(hotkeys), [](const pair<bool, QPointer<OBSHotkeyWidget>> &hotkey) {
		const auto &hw = *hotkey.second;
		return hw.Changed();
	});

	if (hotkeysChanged)
		EnableApplyButton(true);
}

void OBSBasicSettings::SearchHotkeys(const QString &text, obs_key_combination_t filterCombo)
{
	if (hotkeyPage->hotkeyFormLayout->rowCount() == 0)
		return;

	hotkeyPage->hotkeyScrollArea->ensureVisible(0, 0);

	QLayoutItem *hotkeysItem = hotkeyPage->hotkeyFormLayout->itemAt(0);
	QWidget *hotkeys = hotkeysItem->widget();
	if (!hotkeys)
		return;

	QFormLayout *hotkeysLayout = qobject_cast<QFormLayout *>(hotkeys->layout());

	SearchHotkeys(hotkeysLayout, text, filterCombo);
}

void OBSBasicSettings::SearchHotkeys(QFormLayout *hotkeysLayout, const QString &text, obs_key_combination_t filterCombo)
{
	if (!hotkeysLayout)
		return;
	hotkeysLayout->setEnabled(false);

	QString needle = text.toLower();

	for (int i = 0; i < hotkeysLayout->rowCount(); i++) {
		auto label = hotkeysLayout->itemAt(i, QFormLayout::LabelRole);
		if (!label) {
			auto tabWidget = qobject_cast<QTabWidget *>(
				hotkeysLayout->itemAt(i, QFormLayout::SpanningRole)->widget());
			if (nullptr != tabWidget) {
				for (auto iTab = 0; iTab < tabWidget->count(); ++iTab) {
					auto formLayout =
						qobject_cast<QFormLayout *>(tabWidget->widget(iTab)->layout());
					if (nullptr != formLayout) {
						SearchHotkeys(formLayout, text, filterCombo);
					}
				}
			}

			continue;
		}

		OBSHotkeyLabel *item = qobject_cast<OBSHotkeyLabel *>(label->widget());
		if (!item)
			continue;

		QString fullname = item->property("fullName").value<QString>();

		auto showHotkey = needle.isEmpty() || fullname.toLower().contains(needle);

		if (showHotkey && !obs_key_combination_is_empty(filterCombo)) {
			showHotkey = false;

			std::vector<obs_key_combination_t> combos;
			item->widget->GetCombinations(combos);
			for (auto combo : combos) {
				if (combo == filterCombo) {
					showHotkey = true;
					break;
				}
			}
		}

		BlockLayoutEnable LabelRoleBlock(item->layout());
		item->setVisible(showHotkey);

		BlockLayoutEnable widgetBlock(item->widget->layout());
		item->widget->setVisible(showHotkey);
	}

	hotkeysLayout->setEnabled(true);
	hotkeysLayout->activate();
}

void OBSBasicSettings::on_hotkeyFilterReset_clicked()
{
	hotkeyPage->hotkeyFilterInput->ResetKey();
}

void OBSBasicSettings::on_pushButton_clicked()
{
	m_hotkeySearchClearedByButton = true;
	hotkeyPage->hotkeyFilterSearch->setText("");
	hotkeyPage->hotkeySearchLabel->setFocus();
}

void OBSBasicSettings::on_hotkeyFilterSearch_textChanged(const QString text)
{
	if (text.isEmpty()) {
		if (m_hotkeySearchClearedByButton) {
			PLS_UI_ACTION("OBSBasicSettings:on_hotkeyFilterSearch_textChanged(): click clear");
			m_hotkeySearchClearedByButton = false;
		}

		hotkeyPage->pushButton->setEnabled(false);
	} else {
		hotkeyPage->pushButton->setEnabled(true);
	}
	SearchHotkeys(text, hotkeyPage->hotkeyFilterInput->key);
}

void OBSBasicSettings::on_hotkeyFilterInput_KeyChanged(obs_key_combination_t combo)
{
	if (obs_key_combination_is_empty(combo)) {
		hotkeyPage->hotkeyFilterReset->setEnabled(false);
	} else {
		hotkeyPage->hotkeyFilterReset->setEnabled(true);
	}
	SearchHotkeys(hotkeyPage->hotkeyFilterSearch->text(), combo);
}

namespace std {
template<> struct hash<obs_key_combination_t> {
	size_t operator()(obs_key_combination_t value) const
	{
		size_t h1 = hash<uint32_t>{}(value.modifiers);
		size_t h2 = hash<int>{}(value.key);
		// Same as boost::hash_combine()
		h2 ^= h1 + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
		return h2;
	}
};
} // namespace std

typedef struct assignment {
	OBSHotkeyLabel *label;
	OBSHotkeyEdit *edit;
} assignment;

void scanFormLayout(QFormLayout *layout, unordered_map<obs_key_combination_t, vector<assignment>> &assignments,
		    vector<OBSHotkeyLabel *> &items, bool &hasDupes)
{
	for (int i = 0; i < layout->rowCount(); i++) {
		auto label = layout->itemAt(i, QFormLayout::LabelRole);
		if (!label) {
			label = layout->itemAt(i, QFormLayout::SpanningRole);
			if (!label) {
				continue;
			}
			if (auto tabWidget = qobject_cast<QTabWidget *>(label->widget());
			    nullptr != tabWidget && 2 == tabWidget->count()) {
				if (auto hFormLayout = qobject_cast<QFormLayout *>(tabWidget->widget(0)->layout());
				    nullptr != hFormLayout) {
					scanFormLayout(hFormLayout, assignments, items, hasDupes);
				}
				if (auto vFormLayout = qobject_cast<QFormLayout *>(tabWidget->widget(1)->layout());
				    nullptr != vFormLayout) {
					scanFormLayout(vFormLayout, assignments, items, hasDupes);
				}
			}

			continue;
		}
		OBSHotkeyLabel *item = qobject_cast<OBSHotkeyLabel *>(label->widget());
		if (!item)
			continue;

		items.push_back(item);

		for (auto &edit : item->widget->edits) {
			edit->hasDuplicate = false;

			if (obs_key_combination_is_empty(edit->key))
				continue;

			for (assignment &assign : assignments[edit->key]) {
				if (item->pairPartner == assign.label)
					continue;

				assign.edit->hasDuplicate = true;
				edit->hasDuplicate = true;
				hasDupes = true;
			}

			assignments[edit->key].push_back({item, edit});
		}
	}
}

bool OBSBasicSettings::ScanDuplicateHotkeys(QFormLayout *layout)
{
	unordered_map<obs_key_combination_t, vector<assignment>> assignments;
	vector<OBSHotkeyLabel *> items;
	bool hasDupes = false;

	scanFormLayout(layout, assignments, items, hasDupes);

	for (auto *item : items)
		for (auto &edit : item->widget->edits)
			edit->UpdateDuplicationState();

	return hasDupes;
}

void OBSBasicSettings::ReloadHotkeys(obs_hotkey_id ignoreKey)
{
	if (!hotkeysLoaded)
		return;

	if (Pages::HOTKEYS == ui->listWidget->currentRow()) {
		LoadHotkeySettings(ignoreKey);
	} else {
		bReloadHotKey = true;
	}
}

void OBSBasicSettings::A11yChanged()
{
	if (!loading) {
		a11yChanged = true;
		sender()->setProperty("changed", QVariant(true));
		if (sender()->objectName() == "colorCheckBox") {
			accessPage->colorsGroupBox->setProperty("changed", QVariant(true));
		}
		EnableApplyButton(true);
	}
}

void OBSBasicSettings::AdvancedChanged()
{
	if (!loading) {
		advancedChanged = true;
		sender()->setProperty("changed", QVariant(true));
		EnableApplyButton(true);
	}
}

void OBSBasicSettings::AdvOutSplitFileChanged()
{
	bool splitFile = outputRecordPage->advOutSplitFile->isChecked();
	int splitFileType = splitFile ? outputRecordPage->advOutSplitFileType->currentIndex() : -1;

	outputRecordPage->advOutSplitFileType->setEnabled(splitFile);
	outputRecordPage->advOutSplitFileTimeLabel->setVisible(splitFileType == 0);
	outputRecordPage->advOutSplitFileTime->setVisible(splitFileType == 0);
	outputRecordPage->advOutSplitFileSizeLabel->setVisible(splitFileType == 1);
	outputRecordPage->advOutSplitFileSize->setVisible(splitFileType == 1);
	if (splitFileType == -1) {
		outputRecordPage->formLayout_16->takeRow(outputRecordPage->advOutSplitFileSizeLabel);
		outputRecordPage->formLayout_16->takeRow(outputRecordPage->advOutSplitFileTimeLabel);
	}

	if (splitFileType == 0) {
		outputRecordPage->formLayout_16->addRow(outputRecordPage->advOutSplitFileTimeLabel,
							outputRecordPage->advOutSplitFileTime);
		outputRecordPage->formLayout_16->takeRow(outputRecordPage->advOutSplitFileSizeLabel);
	}

	if (splitFileType == 1) {
		outputRecordPage->formLayout_16->addRow(outputRecordPage->advOutSplitFileSizeLabel,
							outputRecordPage->advOutSplitFileSize);
		outputRecordPage->formLayout_16->takeRow(outputRecordPage->advOutSplitFileTimeLabel);
	}
}

static void DisableIncompatibleCodecs(QComboBox *cbox, const QString &format, const QString &formatName,
				      const QString &streamEncoder)
{
	QString strEncLabel = QTStr("Basic.Settings.Output.Adv.Recording.UseStreamEncoder");
	QString recEncoder = cbox->currentData().toString();

	/* Check if selected encoders and output format are compatible, disable incompatible items. */
	bool currentCompatible = true;
	for (int idx = 0; idx < cbox->count(); idx++) {
		QString encName = cbox->itemData(idx).toString();
		string encoderId = (encName == "none") ? streamEncoder.toStdString() : encName.toStdString();
		QString encDisplayName = (encName == "none") ? strEncLabel
							     : obs_encoder_get_display_name(encoderId.c_str());

		/* Something has gone horribly wrong and there's no encoder */
		if (encoderId.empty())
			continue;

		if (obs_get_encoder_caps(encoderId.c_str()) & OBS_ENCODER_CAP_DEPRECATED) {
			encDisplayName += " (" + QTStr("Deprecated") + ")";
		}

		const char *codec = obs_get_encoder_codec(encoderId.c_str());

		bool is_compatible = ContainerSupportsCodec(format.toStdString(), codec);
		/* Fall back to FFmpeg check if codec not one of the built-in ones. */
		if (!is_compatible && !IsBuiltinCodec(codec)) {
			string ext = GetFormatExt(QT_TO_UTF8(format));
			is_compatible = FFCodecAndFormatCompatible(codec, ext.c_str());
		}

		QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(cbox->model());
		QStandardItem *item = model->item(idx);

		if (is_compatible) {
			item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		} else {
			if (recEncoder == encName)
				currentCompatible = false;

			item->setFlags(Qt::NoItemFlags);
			encDisplayName += " ";
			encDisplayName += QTStr("CodecCompat.Incompatible").arg(formatName);
		}

		item->setText(encDisplayName);
	}

	// Set to invalid entry if encoder was incompatible
	if (!currentCompatible)
		cbox->setCurrentIndex(-1);
}

void OBSBasicSettings::AdvOutRecCheckCodecs()
{
	if (!outputRecordPage) {
		return;
	}

	QString recFormat = outputRecordPage->advOutRecFormat->currentData().toString();
	QString recFormatName = outputRecordPage->advOutRecFormat->currentText();

	QString tooltip = QTStr("Basic.Settings.Output.Format.TT." + recFormat.toUtf8());
	if (!tooltip.startsWith("Basic.Settings.Output"))
		outputRecordPage->advOutRecFormat->setToolTip(tooltip);
	else
		outputRecordPage->advOutRecFormat->setToolTip(nullptr);

	QString streamEncoder = outputStreamPage ? outputStreamPage->advOutEncoder->currentData().toString()
						 : config_get_string(main->Config(), "AdvOut", "Encoder");
	QString streamAudioEncoder = outputStreamPage ? outputStreamPage->advOutAEncoder->currentData().toString()
						      : config_get_string(main->Config(), "AdvOut", "AudioEncoder");

	int oldVEncoderIdx = outputRecordPage->advOutRecEncoder->currentIndex();
	int oldAEncoderIdx = outputRecordPage->advOutRecAEncoder->currentIndex();
	DisableIncompatibleCodecs(outputRecordPage->advOutRecEncoder, recFormat, recFormatName, streamEncoder);
	DisableIncompatibleCodecs(outputRecordPage->advOutRecAEncoder, recFormat, recFormatName, streamAudioEncoder);

	/* Only invoke AdvOutRecCheckWarnings() if it wouldn't already have
	 * been triggered by one of the encoder selections being reset. */
	if (outputRecordPage->advOutRecEncoder->currentIndex() == oldVEncoderIdx &&
	    outputRecordPage->advOutRecAEncoder->currentIndex() == oldAEncoderIdx)
		AdvOutRecCheckWarnings();
}

#if defined(__APPLE__) && QT_VERSION < QT_VERSION_CHECK(6, 5, 1)
// Workaround for QTBUG-56064 on macOS
static void ResetInvalidSelection(QComboBox *cbox)
{
	int idx = cbox->currentIndex();
	if (idx < 0)
		return;

	QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(cbox->model());
	QStandardItem *item = model->item(idx);

	if (item->isEnabled())
		return;

	// Reset to "invalid" state if item was disabled
	cbox->blockSignals(true);
	cbox->setCurrentIndex(-1);
	cbox->blockSignals(false);
}
#endif

void OBSBasicSettings::AdvOutRecCheckWarnings()
{
	auto Checked = [](PLSCheckBox *box) {
		return box->isChecked() ? 1 : 0;
	};

	QString errorMsg;
	QString warningMsg;
	uint32_t tracks = Checked(outputRecordPage->advOutRecTrack1) + Checked(outputRecordPage->advOutRecTrack2) +
			  Checked(outputRecordPage->advOutRecTrack3) + Checked(outputRecordPage->advOutRecTrack4) +
			  Checked(outputRecordPage->advOutRecTrack5) + Checked(outputRecordPage->advOutRecTrack6);

	clearAlertMessage(AlertMessageType::Warning, outputRecordPage->advOutRecEncoder, false);
	clearAlertMessage(AlertMessageType::Error, outputRecordPage->advOutRecFormat, false);
	clearAlertMessage(AlertMessageType::Warning, outputRecordPage->advOutRecFormat, false);

	if (outputRecordPage->advOutRecType->currentIndex() == 0) {
		if (outputRecordPage->advOutRecEncoder->currentIndex() == 0) {
			updateAlertMessage(AlertMessageType::Warning, outputRecordPage->advOutRecEncoder,
					   QTStr("setting.adv.pause.rec.warn"));
		} else {
			updateAlertMessage(AlertMessageType::Warning, outputRecordPage->advOutRecEncoder,
					   QTStr("Output.EncoderMismatch.Settings.Advance"));
		}
	}

	QString recFormat = outputRecordPage->advOutRecFormat->currentData().toString();

	if (recFormat == "flv") {
		outputRecordPage->advRecTrackWidget->setCurrentWidget(outputRecordPage->flvTracks);
	} else {
		outputRecordPage->advRecTrackWidget->setCurrentWidget(outputRecordPage->recTracks);

		if (tracks == 0) {
			updateAlertMessage(AlertMessageType::Error, outputRecordPage->advOutRecFormat,
					   QTStr("OutputWarnings.NoTracksSelected"));
		}
	}

	if (recFormat == "mp4" || recFormat == "mov") {
		updateAlertMessage(AlertMessageType::Warning, outputRecordPage->advOutRecFormat,
				   QTStr("OutputWarnings.MP4Recording"));
		if (advancedPage) {
			advancedPage->autoRemux->setText(QTStr("Basic.Settings.Advanced.AutoRemux").arg("mp4") + " " +
							 QTStr("Basic.Settings.Advanced.AutoRemux.MP4"));
		}
	} else if (advancedPage) {
		advancedPage->autoRemux->setText(QTStr("Basic.Settings.Advanced.AutoRemux").arg("mp4"));
	}

#if defined(__APPLE__) && QT_VERSION < QT_VERSION_CHECK(6, 5, 1)
	// Workaround for QTBUG-56064 on macOS
	ResetInvalidSelection(outputRecordPage->advOutRecEncoder);
	ResetInvalidSelection(outputRecordPage->advOutRecAEncoder);
#endif

	// Show warning if codec selection was reset to an invalid state
	if (outputRecordPage->advOutRecEncoder->currentIndex() == -1 ||
	    outputRecordPage->advOutRecAEncoder->currentIndex() == -1) {
		updateAlertMessage(AlertMessageType::Warning, outputRecordPage->advOutRecFormat,
				   QTStr("OutputWarnings.CodecIncompatible"));
	}

	updateAlertMessage();
}

static inline QString MakeMemorySizeString(int bitrate, int seconds)
{
	QString str = QTStr("Basic.Settings.Advanced.StreamDelay.MemoryUsage");
	int megabytes = bitrate * seconds / 1000 / 8;

	return str.arg(QString::number(megabytes));
}

void OBSBasicSettings::UpdateSimpleOutStreamDelayEstimate()
{
	if (advancedPage) {
		int seconds = advancedPage->streamDelaySec->value();

		auto vBitrate = 0;
		auto aBitrate = 0;
		if (outputSimplePage) {
			vBitrate = outputSimplePage->simpleOutputVBitrate->value();
			aBitrate = outputSimplePage->simpleOutputABitrate->currentText().toInt();
		} else {
			vBitrate = config_get_int(main->Config(), "SimpleOutput", "VBitrate");
			aBitrate = config_get_int(main->Config(), "SimpleOutput", "ABitrate");
		}

		QString msg = MakeMemorySizeString(vBitrate + aBitrate, seconds);
		advancedPage->streamDelayInfo->setText(msg);
	}
}
extern OBSData GetDataFromJsonFile(const char *jsonFile);
void OBSBasicSettings::UpdateAdvOutStreamDelayEstimate()
{
	if (!advancedPage) {
		return;
	}

	int trackIndex = config_get_int(main->Config(), "AdvOut", "TrackIndex");

	QString aBitrateText;
	if (outputAudioPage) {
		switch (trackIndex) {
		case 1:
			aBitrateText = outputAudioPage->advOutTrack1Bitrate->currentText();
			break;
		case 2:
			aBitrateText = outputAudioPage->advOutTrack2Bitrate->currentText();
			break;
		case 3:
			aBitrateText = outputAudioPage->advOutTrack3Bitrate->currentText();
			break;
		case 4:
			aBitrateText = outputAudioPage->advOutTrack4Bitrate->currentText();
			break;
		case 5:
			aBitrateText = outputAudioPage->advOutTrack5Bitrate->currentText();
			break;
		case 6:
			aBitrateText = outputAudioPage->advOutTrack6Bitrate->currentText();
			break;
		}
	}

	int seconds = advancedPage->streamDelaySec->value();

	auto vBitrate = 0;
	if (nullptr != streamEncoderProps) {
		OBSData settings = streamEncoderProps->GetSettings();
		vBitrate = (int)obs_data_get_int(settings, "bitrate");
	} else {
		OBSData streamEncSettings = GetDataFromJsonFile("streamEncoder.json");
		vBitrate = obs_data_get_int(streamEncSettings, "bitrate");
	}

	auto aBitrate = 0;
	if (aBitrateText.isEmpty()) {
		switch (trackIndex) {
		case 1:
			aBitrate = config_get_int(main->Config(), "AdvOut", "Track1Bitrate");
			break;
		case 2:
			aBitrate = config_get_int(main->Config(), "AdvOut", "Track2Bitrate");
			break;
		case 3:
			aBitrate = config_get_int(main->Config(), "AdvOut", "Track3Bitrate");
			break;
		case 4:
			aBitrate = config_get_int(main->Config(), "AdvOut", "Track4Bitrate");
			break;
		case 5:
			aBitrate = config_get_int(main->Config(), "AdvOut", "Track5Bitrate");
			break;
		case 6:
			aBitrate = config_get_int(main->Config(), "AdvOut", "Track6Bitrate");
			break;
		default:
			break;
		}
	} else {
		aBitrate = aBitrateText.toInt();
	}

	QString msg = MakeMemorySizeString(vBitrate + aBitrate, seconds);
	advancedPage->streamDelayInfo->setText(msg);
}

int OBSBasicSettings::getOutputMode() const
{
	if (outputPage) {
		return outputPage->outputMode->currentIndex();
	} else {
		const char *mode = config_get_string(main->Config(), "Output", "Mode");
		return astrcmpi(mode, "Advanced") == 0 ? 1 : 0;
	}
}

void OBSBasicSettings::UpdateStreamDelayEstimate()
{
	if (0 == getOutputMode())
		UpdateSimpleOutStreamDelayEstimate();
	else
		UpdateAdvOutStreamDelayEstimate();
}

bool EncoderAvailable(const char *encoder)
{
	const char *val;
	int i = 0;

	while (obs_enum_encoder_types(i++, &val))
		if (strcmp(val, encoder) == 0)
			return true;

	return false;
}

void OBSBasicSettings::FillSimpleRecordingValues()
{
#define ADD_QUALITY(str)                                                                                            \
	outputSimplePage->simpleOutRecQuality->addItem(QTStr("Basic.Settings.Output.Simple.RecordingQuality." str), \
						       QString(str));
#define ENCODER_STR(str) QTStr("Basic.Settings.Output.Simple.Encoder." str)

	ADD_QUALITY("Stream");
	ADD_QUALITY("Small");
	ADD_QUALITY("HQ");
	ADD_QUALITY("Lossless");

	outputSimplePage->simpleOutRecEncoder->addItem(ENCODER_STR("Software"), QString(SIMPLE_ENCODER_X264));
	outputSimplePage->simpleOutRecEncoder->addItem(ENCODER_STR("SoftwareLowCPU"),
						       QString(SIMPLE_ENCODER_X264_LOWCPU));
	if (EncoderAvailable("obs_qsv11"))
		outputSimplePage->simpleOutRecEncoder->addItem(ENCODER_STR("Hardware.QSV.H264"),
							       QString(SIMPLE_ENCODER_QSV));
	if (EncoderAvailable("obs_qsv11_av1"))
		outputSimplePage->simpleOutRecEncoder->addItem(ENCODER_STR("Hardware.QSV.AV1"),
							       QString(SIMPLE_ENCODER_QSV_AV1));
	if (EncoderAvailable("ffmpeg_nvenc"))
		outputSimplePage->simpleOutRecEncoder->addItem(ENCODER_STR("Hardware.NVENC.H264"),
							       QString(SIMPLE_ENCODER_NVENC));
	if (EncoderAvailable("obs_nvenc_av1_tex"))
		outputSimplePage->simpleOutRecEncoder->addItem(ENCODER_STR("Hardware.NVENC.AV1"),
							       QString(SIMPLE_ENCODER_NVENC_AV1));
#ifdef ENABLE_HEVC
	if (EncoderAvailable("h265_texture_amf"))
		outputSimplePage->simpleOutRecEncoder->addItem(ENCODER_STR("Hardware.AMD.HEVC"),
							       QString(SIMPLE_ENCODER_AMD_HEVC));
	if (EncoderAvailable("ffmpeg_hevc_nvenc"))
		outputSimplePage->simpleOutRecEncoder->addItem(ENCODER_STR("Hardware.NVENC.HEVC"),
							       QString(SIMPLE_ENCODER_NVENC_HEVC));
#endif
	if (EncoderAvailable("h264_texture_amf"))
		outputSimplePage->simpleOutRecEncoder->addItem(ENCODER_STR("Hardware.AMD.H264"),
							       QString(SIMPLE_ENCODER_AMD));
	if (EncoderAvailable("av1_texture_amf"))
		outputSimplePage->simpleOutRecEncoder->addItem(ENCODER_STR("Hardware.AMD.AV1"),
							       QString(SIMPLE_ENCODER_AMD_AV1));
	if (EncoderAvailable("com.apple.videotoolbox.videoencoder.ave.avc")
#ifndef __aarch64__
	    && os_get_emulation_status() == true
#endif
	)
		outputSimplePage->simpleOutRecEncoder->addItem(ENCODER_STR("Hardware.Apple.H264"),
							       QString(SIMPLE_ENCODER_APPLE_H264));
#ifdef ENABLE_HEVC
	if (EncoderAvailable("com.apple.videotoolbox.videoencoder.ave.hevc")
#ifndef __aarch64__
	    && os_get_emulation_status() == true
#endif
	)
		outputSimplePage->simpleOutRecEncoder->addItem(ENCODER_STR("Hardware.Apple.HEVC"),
							       QString(SIMPLE_ENCODER_APPLE_HEVC));
#endif

	if (EncoderAvailable("CoreAudio_AAC") || EncoderAvailable("libfdk_aac") || EncoderAvailable("ffmpeg_aac"))
		outputSimplePage->simpleOutRecAEncoder->addItem(QTStr("Basic.Settings.Output.Simple.Codec.AAC.Default"),
								"aac");
	if (EncoderAvailable("ffmpeg_opus"))
		outputSimplePage->simpleOutRecAEncoder->addItem(QTStr("Basic.Settings.Output.Simple.Codec.Opus"),
								"opus");

#undef ADD_QUALITY
#undef ENCODER_STR
}

void OBSBasicSettings::FillAudioMonitoringDevices()
{
	QComboBox *cb = audioPage->monitoringDevice;

	auto enum_devices = [](void *param, const char *name, const char *id) {
		QComboBox *cb = (QComboBox *)param;
		cb->addItem(name, id);
		return true;
	};

	cb->addItem(QTStr("Basic.Settings.Advanced.Audio.MonitoringDevice"
			  ".Default"),
		    "default");

	obs_enum_audio_monitoring_devices(enum_devices, cb);
}

void OBSBasicSettings::SimpleRecordingQualityChanged()
{
	QString qual = outputSimplePage->simpleOutRecQuality->currentData().toString();
	bool streamQuality = qual == "Stream";
	bool losslessQuality = !streamQuality && qual == "Lossless";

	bool showEncoder = !streamQuality && !losslessQuality;
	outputSimplePage->simpleOutRecEncoder->setVisible(showEncoder);
	outputSimplePage->simpleOutRecEncoderLabel->setVisible(showEncoder);
	outputSimplePage->simpleOutRecAEncoder->setVisible(showEncoder);
	outputSimplePage->simpleOutRecAEncoderLabel->setVisible(showEncoder);
	outputSimplePage->simpleOutRecFormat->setVisible(!losslessQuality);
	outputSimplePage->simpleOutRecFormatLabel->setVisible(!losslessQuality);

	UpdateMultitrackVideo();
	SimpleRecordingEncoderChanged();
	SimpleReplayBufferChanged();
}

extern const char *get_simple_output_encoder(const char *encoder);

void OBSBasicSettings::SimpleStreamingEncoderChanged()
{
	SimpleStreamEncoderCheckWarnings();
	QString encoder = outputSimplePage->simpleOutStrEncoder->currentData().toString();
	QString preset;
	const char *defaultPreset = nullptr;

	outputSimplePage->simpleOutAdvanced->setVisible(true);
	outputSimplePage->simpleOutPresetLabel->setVisible(true);
	outputSimplePage->simpleOutPreset->setVisible(true);
	outputSimplePage->simpleOutPreset->clear();

	if (encoder == SIMPLE_ENCODER_QSV || encoder == SIMPLE_ENCODER_QSV_AV1) {
		outputSimplePage->simpleOutPreset->addItem("speed", "speed");
		outputSimplePage->simpleOutPreset->addItem("balanced", "balanced");
		outputSimplePage->simpleOutPreset->addItem("quality", "quality");

		defaultPreset = "balanced";
		preset = curQSVPreset;

	} else if (encoder == SIMPLE_ENCODER_NVENC || encoder == SIMPLE_ENCODER_NVENC_HEVC ||
		   encoder == SIMPLE_ENCODER_NVENC_AV1) {

		const char *name = get_simple_output_encoder(QT_TO_UTF8(encoder));
		const bool isFFmpegEncoder = strncmp(name, "ffmpeg_", 7) == 0;
		obs_properties_t *props = obs_get_encoder_properties(name);

		obs_property_t *p = obs_properties_get(props, isFFmpegEncoder ? "preset2" : "preset");
		size_t num = obs_property_list_item_count(p);
		for (size_t i = 0; i < num; i++) {
			const char *name = obs_property_list_item_name(p, i);
			const char *val = obs_property_list_item_string(p, i);

			outputSimplePage->simpleOutPreset->addItem(QT_UTF8(name), val);
		}

		obs_properties_destroy(props);

		defaultPreset = "default";
		preset = curNVENCPreset;

	} else if (encoder == SIMPLE_ENCODER_AMD || encoder == SIMPLE_ENCODER_AMD_HEVC) {
		outputSimplePage->simpleOutPreset->addItem("Speed", "speed");
		outputSimplePage->simpleOutPreset->addItem("Balanced", "balanced");
		outputSimplePage->simpleOutPreset->addItem("Quality", "quality");

		defaultPreset = "balanced";
		preset = curAMDPreset;
	} else if (encoder == SIMPLE_ENCODER_APPLE_H264
#ifdef ENABLE_HEVC
		   || encoder == SIMPLE_ENCODER_APPLE_HEVC
#endif
	) {
		outputSimplePage->simpleOutAdvanced->setChecked(false);
		outputSimplePage->simpleOutAdvanced->setVisible(false);
		outputSimplePage->simpleOutPreset->setVisible(false);
		outputSimplePage->simpleOutPresetLabel->setVisible(false);

	} else if (encoder == SIMPLE_ENCODER_AMD_AV1) {
		outputSimplePage->simpleOutPreset->addItem("Speed", "speed");
		outputSimplePage->simpleOutPreset->addItem("Balanced", "balanced");
		outputSimplePage->simpleOutPreset->addItem("Quality", "quality");
		outputSimplePage->simpleOutPreset->addItem("High Quality", "highQuality");

		defaultPreset = "balanced";
		preset = curAMDAV1Preset;
	} else {

#define PRESET_STR(val) QString(Str("Basic.Settings.Output.EncoderPreset." val)).arg(val)
		outputSimplePage->simpleOutPreset->addItem(PRESET_STR("ultrafast"), "ultrafast");
		outputSimplePage->simpleOutPreset->addItem("superfast", "superfast");
		outputSimplePage->simpleOutPreset->addItem(PRESET_STR("veryfast"), "veryfast");
		outputSimplePage->simpleOutPreset->addItem("faster", "faster");
		outputSimplePage->simpleOutPreset->addItem(PRESET_STR("fast"), "fast");
#undef PRESET_STR

		/* Users might have previously selected a preset which is no
		 * longer available in simple mode. Make sure we don't mess
		 * with their setups without them knowing. */
		if (outputSimplePage->simpleOutPreset->findData(curPreset) == -1) {
			outputSimplePage->simpleOutPreset->addItem(curPreset, curPreset);
			QStandardItemModel *model =
				qobject_cast<QStandardItemModel *>(outputSimplePage->simpleOutPreset->model());
			QStandardItem *item = model->item(model->rowCount() - 1);
			item->setEnabled(false);
		}

		defaultPreset = "veryfast";
		preset = curPreset;
	}

	int idx = outputSimplePage->simpleOutPreset->findData(QVariant(preset));
	if (idx == -1)
		idx = outputSimplePage->simpleOutPreset->findData(QVariant(defaultPreset));

	outputSimplePage->simpleOutPreset->setCurrentIndex(idx);
}

#define ESTIMATE_STR "Basic.Settings.Output.ReplayBuffer.Estimate"
#define ESTIMATE_TOO_LARGE_STR "Basic.Settings.Output.ReplayBuffer.EstimateTooLarge"
#define ESTIMATE_UNKNOWN_STR "Basic.Settings.Output.ReplayBuffer.EstimateUnknown"

void OBSBasicSettings::UpdateGeneralReplayBufferCheckboxes()
{
	if (!generalPage) {
		return;
	}

	bool state = false;

	if (outputPage) {
		switch (outputPage->outputMode->currentIndex()) {
		case 0:
			state = outputSimplePage->simpleReplayBuf->isChecked();
			break;

		case 1:
			if (outputReplayPage) {
				state = outputReplayPage->advReplayBuf->isChecked();
			} else {
				state = config_get_bool(main->Config(), "AdvOut", "RecRB");
			}
			break;
		}
	} else {
		if (QStringLiteral("Simple") == config_get_string(main->Config(), "Output", "Mode")) {
			state = config_get_bool(main->Config(), "SimpleOutput", "RecRB");
		} else {
			state = config_get_bool(main->Config(), "AdvOut", "RecRB");
		}
	}

	generalPage->replayWhileStreaming->setEnabled(state);
	generalPage->keepReplayStreamStops->setEnabled(state && generalPage->replayWhileStreaming->isChecked());
}

void OBSBasicSettings::UpdateSimpleReplayBufferCheckboxes()
{
	auto lossless = outputSimplePage->simpleOutRecQuality->currentData().toString() == "Lossless";
	outputSimplePage->simpleReplayBuf->setEnabled(!obs_frontend_replay_buffer_active() && !lossless);

	if (generalPage) {
		auto state = outputSimplePage->simpleReplayBuf->isChecked();

		generalPage->replayWhileStreaming->setEnabled(state);
		generalPage->keepReplayStreamStops->setEnabled(state && generalPage->replayWhileStreaming->isChecked());
	}
}

void OBSBasicSettings::UpdateAdvancedReplayBufferCheckboxes()
{
	if (outputReplayPage) {
		bool customFFmpeg = outputRecordPage ? outputRecordPage->advOutRecType->currentIndex() == 1
						     : QStringLiteral("FFmpeg") ==
							       config_get_string(main->Config(), "AdvOut", "RecType");
		outputReplayPage->advReplayBuf->setEnabled(!obs_frontend_replay_buffer_active() && !customFFmpeg);
		outputReplayPage->advReplayBufCustomFFmpeg->setVisible(customFFmpeg);
	}

	if (generalPage) {
		auto state = outputReplayPage ? outputReplayPage->advReplayBuf->isChecked()
					      : config_get_bool(main->Config(), "AdvOut", "RecRB");

		generalPage->replayWhileStreaming->setEnabled(state);
		generalPage->keepReplayStreamStops->setEnabled(state && generalPage->replayWhileStreaming->isChecked());
	}
}

void OBSBasicSettings::SimpleReplayBufferChanged()
{
	QString qual = outputSimplePage->simpleOutRecQuality->currentData().toString();
	bool streamQuality = qual == "Stream";
	int abitrate = 0;

	outputSimplePage->simpleRBMegsMax->setVisible(!streamQuality);
	outputSimplePage->simpleRBMegsMaxLabel->setVisible(!streamQuality);

	if (outputSimplePage->simpleOutRecFormat->currentText().compare("flv") == 0 || streamQuality) {
		abitrate = outputSimplePage->simpleOutputABitrate->currentText().toInt();
	} else {
		int delta = outputSimplePage->simpleOutputABitrate->currentText().toInt();
		if (outputSimplePage->simpleOutRecTrack1->isChecked())
			abitrate += delta;
		if (outputSimplePage->simpleOutRecTrack2->isChecked())
			abitrate += delta;
		if (outputSimplePage->simpleOutRecTrack3->isChecked())
			abitrate += delta;
		if (outputSimplePage->simpleOutRecTrack4->isChecked())
			abitrate += delta;
		if (outputSimplePage->simpleOutRecTrack5->isChecked())
			abitrate += delta;
		if (outputSimplePage->simpleOutRecTrack6->isChecked())
			abitrate += delta;
	}

	int vbitrate = outputSimplePage->simpleOutputVBitrate->value();
	int seconds = outputSimplePage->simpleRBSecMax->value();

	// Set maximum to 75% of installed memory
	uint64_t memTotal = os_get_sys_total_size();
	int64_t memMaxMB = memTotal ? memTotal * 3 / 4 / 1024 / 1024 : 8192;

	int64_t memMB = int64_t(seconds) * int64_t(vbitrate + abitrate) * 1000 / 8 / 1024 / 1024;
	if (memMB < 1)
		memMB = 1;

	outputSimplePage->simpleRBEstimate->setObjectName("");
	if (streamQuality) {
		if (memMB <= memMaxMB) {
			outputSimplePage->simpleRBEstimate->setText(
				QTStr(ESTIMATE_STR).arg(QString::number(int(memMB))));
		} else {
			outputSimplePage->simpleRBEstimate->setText(
				QTStr(ESTIMATE_TOO_LARGE_STR)
					.arg(QString::number(int(memMB)), QString::number(int(memMaxMB))));
			outputSimplePage->simpleRBEstimate->setObjectName("warningLabel");
		}
	} else {
		outputSimplePage->simpleRBEstimate->setText(QTStr(ESTIMATE_UNKNOWN_STR));
		outputSimplePage->simpleRBMegsMax->setMaximum(memMaxMB);
	}

	outputSimplePage->simpleRBEstimate->style()->polish(outputSimplePage->simpleRBEstimate);
	UpdateSimpleReplayBufferCheckboxes();
}

#define TEXT_USE_STREAM_ENC QTStr("Basic.Settings.Output.Adv.Recording.UseStreamEncoder")

void OBSBasicSettings::AdvReplayBufferChanged()
{
	UpdateAdvancedReplayBufferCheckboxes();

	if (outputReplayPage) {
		obs_data_t *settings;
		QString encoder = outputRecordPage ? GetComboData(outputRecordPage->advOutRecEncoder)
						   : config_get_string(main->Config(), "AdvOut", "RecEncoder");
		bool useStream = QString::compare(encoder, "none") == 0;

		if (useStream && streamEncoderProps) {
			settings = streamEncoderProps->GetSettings();
		} else if (!useStream && recordEncoderProps) {
			settings = recordEncoderProps->GetSettings();
		} else {
			if (useStream)
				encoder = outputStreamPage ? GetComboData(outputStreamPage->advOutEncoder)
							   : config_get_string(main->Config(), "AdvOut", "Encoder");
			settings = obs_encoder_defaults(encoder.toUtf8().constData());

			if (!settings)
				return;

			char encoderJsonPath[512];
			int ret = GetProfilePath(encoderJsonPath, sizeof(encoderJsonPath), "recordEncoder.json");
			if (ret > 0) {
				OBSDataAutoRelease data = obs_data_create_from_json_file_safe(encoderJsonPath, "bak");
				obs_data_apply(settings, data);
			}
		}

		int vbitrate = (int)obs_data_get_int(settings, "bitrate");
		const char *rateControl = obs_data_get_string(settings, "rate_control");

		if (!rateControl)
			rateControl = "";

		bool lossless = strcmp(rateControl, "lossless") == 0 ||
				(outputRecordPage ? outputRecordPage->advOutRecType->currentIndex() == 1
						  : config_get_string(main->Config(), "AdvOut", "RecType") ==
							    QStringLiteral("FFmpeg"));
		bool replayBufferEnabled = outputReplayPage->advReplayBuf->isChecked();

		int abitrate = 0;
		int tracks = outputRecordPage ? 0 : config_get_int(main->Config(), "AdvOut", "RecTracks");
		if (outputRecordPage ? outputRecordPage->advOutRecTrack1->isChecked() : tracks & (1 << 0))
			abitrate += outputAudioPage ? outputAudioPage->advOutTrack1Bitrate->currentText().toInt()
						    : config_get_int(main->Config(), "AdvOut", "Track1Bitrate");
		if (outputRecordPage ? outputRecordPage->advOutRecTrack2->isChecked() : tracks & (1 << 1))
			abitrate += outputAudioPage ? outputAudioPage->advOutTrack2Bitrate->currentText().toInt()
						    : config_get_int(main->Config(), "AdvOut", "Track2Bitrate");
		if (outputRecordPage ? outputRecordPage->advOutRecTrack3->isChecked() : tracks & (1 << 2))
			abitrate += outputAudioPage ? outputAudioPage->advOutTrack3Bitrate->currentText().toInt()
						    : config_get_int(main->Config(), "AdvOut", "Track3Bitrate");
		if (outputRecordPage ? outputRecordPage->advOutRecTrack4->isChecked() : tracks & (1 << 3))
			abitrate += outputAudioPage ? outputAudioPage->advOutTrack4Bitrate->currentText().toInt()
						    : config_get_int(main->Config(), "AdvOut", "Track4Bitrate");
		if (outputRecordPage ? outputRecordPage->advOutRecTrack5->isChecked() : tracks & (1 << 4))
			abitrate += outputAudioPage ? outputAudioPage->advOutTrack5Bitrate->currentText().toInt()
						    : config_get_int(main->Config(), "AdvOut", "Track5Bitrate");
		if (outputRecordPage ? outputRecordPage->advOutRecTrack6->isChecked() : tracks & (1 << 5))
			abitrate += outputAudioPage ? outputAudioPage->advOutTrack6Bitrate->currentText().toInt()
						    : config_get_int(main->Config(), "AdvOut", "Track6Bitrate");

		int seconds = outputReplayPage->advRBSecMax->value();

		// Set maximum to 75% of installed memory
		uint64_t memTotal = os_get_sys_total_size();
		int64_t memMaxMB = memTotal ? memTotal * 3 / 4 / 1024 / 1024 : 8192;

		int64_t memMB = int64_t(seconds) * int64_t(vbitrate + abitrate) * 1000 / 8 / 1024 / 1024;
		if (memMB < 1)
			memMB = 1;

		bool varRateControl = (astrcmpi(rateControl, "CBR") == 0 || astrcmpi(rateControl, "VBR") == 0 ||
				       astrcmpi(rateControl, "ABR") == 0);
		if (vbitrate == 0)
			varRateControl = false;

		outputReplayPage->advRBEstimate->setObjectName("");
		if (varRateControl) {
			outputReplayPage->advRBMegsMax->setVisible(false);
			outputReplayPage->advRBMegsMaxLabel->setVisible(false);

			if (memMB <= memMaxMB) {
				outputReplayPage->advRBEstimate->setText(
					QTStr(ESTIMATE_STR).arg(QString::number(int(memMB))));
			} else {
				outputReplayPage->advRBEstimate->setText(
					QTStr(ESTIMATE_TOO_LARGE_STR)
						.arg(QString::number(int(memMB)), QString::number(int(memMaxMB))));
				outputReplayPage->advRBEstimate->setObjectName("warningLabel");
			}
		} else {
			outputReplayPage->advRBMegsMax->setVisible(true);
			outputReplayPage->advRBMegsMaxLabel->setVisible(true);
			outputReplayPage->advRBMegsMax->setMaximum(memMaxMB);
			outputReplayPage->advRBEstimate->setText(QTStr(ESTIMATE_UNKNOWN_STR));
		}

		outputReplayPage->advReplayBufferFrame->setEnabled(!lossless && replayBufferEnabled);
		outputReplayPage->advRBEstimate->style()->polish(outputReplayPage->advRBEstimate);
		outputReplayPage->advReplayBuf->setEnabled(!lossless);
	}
}

#define SIMPLE_OUTPUT_WARNING(str) QTStr("Basic.Settings.Output.Simple.Warn." str)

static void DisableIncompatibleSimpleCodecs(QComboBox *cbox, const QString &format)
{
	/* Unlike in advanced mode the available simple mode encoders are
	 * hardcoded, so this check is also a simpler, hardcoded one. */
	QString encoder = cbox->currentData().toString();

	bool currentCompatible = true;
	for (int idx = 0; idx < cbox->count(); idx++) {
		QString encName = cbox->itemData(idx).toString();
		QString codec;

		/* Simple mode does not expose audio encoder variants directly,
		 * so we have to simply set the codec to the internal name. */
		if (encName == "opus" || encName == "aac") {
			codec = encName;
		} else {
			const char *encoder_id = get_simple_output_encoder(QT_TO_UTF8(encName));
			codec = obs_get_encoder_codec(encoder_id);
		}

		QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(cbox->model());
		QStandardItem *item = model->item(idx);

		if (ContainerSupportsCodec(format.toStdString(), codec.toStdString())) {
			item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		} else {
			if (encoder == encName)
				currentCompatible = false;

			item->setFlags(Qt::NoItemFlags);
		}
	}

	if (!currentCompatible)
		cbox->setCurrentIndex(-1);
}

static void DisableIncompatibleSimpleContainer(QComboBox *cbox, const QString &currentFormat, const QString &vEncoder,
					       const QString &aEncoder)
{
	/* Similar to above, but works in reverse to disable incompatible formats
	 * based on the encoder selection. */
	auto vCodec = obs_get_encoder_codec(get_simple_output_encoder(QT_TO_UTF8(vEncoder)));
	string aCodec = aEncoder.toStdString();

	bool currentCompatible = true;
	for (int idx = 0; idx < cbox->count(); idx++) {
		QString format = cbox->itemData(idx).toString();
		string formatStr = format.toStdString();

		QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(cbox->model());
		QStandardItem *item = model->item(idx);

		if (ContainerSupportsCodec(formatStr, vCodec ? vCodec : "") &&
		    ContainerSupportsCodec(formatStr, aCodec)) {
			item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
		} else {
			if (format == currentFormat)
				currentCompatible = false;

			item->setFlags(Qt::NoItemFlags);
		}
	}

	if (!currentCompatible)
		cbox->setCurrentIndex(-1);
}

void OBSBasicSettings::SimpleRecordingEncoderChanged()
{
	QString qual = outputSimplePage->simpleOutRecQuality->currentData().toString();
	QString warning;

	clearAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutRecQuality, false);
	clearAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutRecEncoder, false);
	clearAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutputVBitrate, false);
	clearAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutputABitrate, false);
	clearAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutRecFormat, false);

	delete simpleOutRecWarning;

	QString format = outputSimplePage->simpleOutRecFormat->currentData().toString();
	QString tooltip = QTStr("Basic.Settings.Output.Format.TT." + format.toUtf8());

	if (!tooltip.startsWith("Basic.Settings.Output"))
		outputSimplePage->simpleOutRecFormat->setToolTip(tooltip);
	else
		outputSimplePage->simpleOutRecFormat->setToolTip(nullptr);

	if (qual == "Lossless") {
		updateAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutRecQuality,
				   SIMPLE_OUTPUT_WARNING("Lossless"));
		updateAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutRecEncoder,
				   SIMPLE_OUTPUT_WARNING("Encoder"));

	} else if (qual != "Stream") {
		updateAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutRecQuality,
				   QTStr("Output.EncoderMismatch.Settings.Simple"));
		QString enc = outputSimplePage->simpleOutRecEncoder->currentData().toString();
		QString streamEnc = outputSimplePage->simpleOutStrEncoder->currentData().toString();
		bool x264RecEnc = (enc == SIMPLE_ENCODER_X264 || enc == SIMPLE_ENCODER_X264_LOWCPU);

		if (streamEnc == SIMPLE_ENCODER_X264 && x264RecEnc) {
			updateAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutRecEncoder,
					   SIMPLE_OUTPUT_WARNING("Encoder"));
		}

		/* Prevent function being called recursively if changes happen. */
		outputSimplePage->simpleOutRecEncoder->blockSignals(true);
		outputSimplePage->simpleOutRecAEncoder->blockSignals(true);
		DisableIncompatibleSimpleCodecs(outputSimplePage->simpleOutRecEncoder, format);
		DisableIncompatibleSimpleCodecs(outputSimplePage->simpleOutRecAEncoder, format);
		outputSimplePage->simpleOutRecAEncoder->blockSignals(false);
		outputSimplePage->simpleOutRecEncoder->blockSignals(false);

		if (outputSimplePage->simpleOutRecEncoder->currentIndex() == -1 ||
		    outputSimplePage->simpleOutRecAEncoder->currentIndex() == -1) {
			updateAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutRecEncoder,
					   QTStr("OutputWarnings.CodecIncompatible"));
		}
	} else {
		/* When using stream encoders do the reverse; Disable containers that are incompatible. */
		QString streamEnc = outputSimplePage->simpleOutStrEncoder->currentData().toString();
		QString streamAEnc = outputSimplePage->simpleOutStrAEncoder->currentData().toString();

		outputSimplePage->simpleOutRecFormat->blockSignals(true);
		DisableIncompatibleSimpleContainer(outputSimplePage->simpleOutRecFormat, format, streamEnc, streamAEnc);
		outputSimplePage->simpleOutRecFormat->blockSignals(false);

		if (outputSimplePage->simpleOutRecFormat->currentIndex() == -1) {
			updateAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutRecEncoder,
					   SIMPLE_OUTPUT_WARNING("IncompatibleContainer"));
		}
		updateAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutRecEncoder,
				   SIMPLE_OUTPUT_WARNING("CannotPause"));
	}

	if (qual != "Lossless" && (format == "mp4" || format == "mov")) {
		if (!warning.isEmpty())
			warning += "\n\n";
		updateAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutRecFormat,
				   QTStr("OutputWarnings.MP4Recording"));
		if (advancedPage) {
			advancedPage->autoRemux->setText(QTStr("Basic.Settings.Advanced.AutoRemux").arg("mp4") + " " +
							 QTStr("Basic.Settings.Advanced.AutoRemux.MP4"));
		}
	} else if (advancedPage) {
		advancedPage->autoRemux->setText(QTStr("Basic.Settings.Advanced.AutoRemux").arg("mp4"));
	}
	if (qual == "Stream") {
		outputSimplePage->simpleRecTrackWidget->setCurrentWidget(outputSimplePage->simpleFlvTracks);
	} else if (qual == "Lossless") {
		outputSimplePage->simpleRecTrackWidget->setCurrentWidget(outputSimplePage->simpleRecTracks);
	} else {
		if (format == "flv") {
			outputSimplePage->simpleRecTrackWidget->setCurrentWidget(outputSimplePage->simpleFlvTracks);
		} else {
			outputSimplePage->simpleRecTrackWidget->setCurrentWidget(outputSimplePage->simpleRecTracks);
		}
	}
	updateAlertMessage();
}

void OBSBasicSettings::SurroundWarning(int idx)
{
	if (idx == lastChannelSetupIdx || idx == -1)
		return;

	if (loading) {
		lastChannelSetupIdx = idx;
		return;
	}

	QString speakerLayoutQstr = audioPage->channelSetup->itemText(idx);
	bool surround = IsSurround(QT_TO_UTF8(speakerLayoutQstr));

	QString lastQstr = audioPage->channelSetup->itemText(lastChannelSetupIdx);
	bool wasSurround = IsSurround(QT_TO_UTF8(lastQstr));

	if (surround && !wasSurround) {

		PLSErrorHandler::ExtraData extraData("OBSBasicSettings");
		auto button = PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_SETTINGS_PROGRAME_RESTART,
								    PLSErrKeyAllAlert, {}, extraData);

		if (button.clickedBtn == QMessageBox::No) {
			QMetaObject::invokeMethod(audioPage->channelSetup, "setCurrentIndex", Qt::QueuedConnection,
						  Q_ARG(int, lastChannelSetupIdx));
			return;
		}
	}

	lastChannelSetupIdx = idx;
}

#define LL_BUFFERING_WARNING "Basic.Settings.Audio.LowLatencyBufferingWarning"

void OBSBasicSettings::UpdateAudioWarnings()
{
	QString speakerLayoutQstr = audioPage->channelSetup->currentText();
	bool surround = IsSurround(QT_TO_UTF8(speakerLayoutQstr));
	bool lowBufferingActive = audioPage->lowLatencyBuffering->isChecked();

	QString text;

	if (surround) {
		text = QTStr(MULTI_CHANNEL_WARNING ".Enabled") + QStringLiteral("\n") + QTStr(MULTI_CHANNEL_WARNING);
	}

	if (lowBufferingActive) {
		if (!text.isEmpty())
			text += QStringLiteral("\n");

		text += QTStr(LL_BUFFERING_WARNING ".Enabled") + QStringLiteral("\n") + QTStr(LL_BUFFERING_WARNING);
	}

	QWidget *page = getPageOfSender();
	if (page) {
		updateAlertMessage(AlertMessageType::Warning, page, text);
	}
}

void OBSBasicSettings::LowLatencyBufferingChanged(bool checked)
{
	if (checked) {
		PLSErrorHandler::ExtraData extraData("OBSBasicSettings");
		auto button = PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_SETTINGS_AUDIO_LOW_LATENCY,
								    PLSErrKeyAllAlert, {}, extraData);

		if (button.clickedBtn == QMessageBox::No) {
			QMetaObject::invokeMethod(audioPage->lowLatencyBuffering, "setChecked", Qt::QueuedConnection,
						  Q_ARG(bool, false));
			return;
		}
	}

	QMetaObject::invokeMethod(this, "UpdateAudioWarnings", Qt::QueuedConnection);
	QMetaObject::invokeMethod(this, "AudioChangedRestart");
}

void OBSBasicSettings::SimpleRecordingQualityLosslessWarning(int idx)
{
	if (idx == lastSimpleRecQualityIdx || idx == -1)
		return;

	QString qual = outputSimplePage->simpleOutRecQuality->itemData(idx).toString();

	if (loading) {
		lastSimpleRecQualityIdx = idx;
		return;
	}

	if (qual == "Lossless") {
		PLSErrorHandler::ExtraData extraData("OBSBasicSettings");

		auto button = PLSErrorHandler::showAlertByPrismCode(
			PLSErrorHandler::ALERT_SETTINGS_OUTPUT_SIMPLE_WARN_LOSSLESS, PLSErrKeyAllAlert, {}, extraData);

		if (button.clickedBtn == QMessageBox::No) {
			QMetaObject::invokeMethod(outputSimplePage->simpleOutRecQuality, "setCurrentIndex",
						  Qt::QueuedConnection, Q_ARG(int, lastSimpleRecQualityIdx));
			return;
		}
	}

	lastSimpleRecQualityIdx = idx;
}

void OBSBasicSettings::on_disableOSXVSync_clicked()
{
#ifdef __APPLE__
	if (!loading) {
		bool disable = advancedPage->disableOSXVSync->isChecked();
		advancedPage->resetOSXVSync->setEnabled(disable);
	}
#endif
}

void OBSBasicSettings::showSaveVideoAlert()
{
	QMetaObject::invokeMethod(
		this,
		[this]() {
			PLSErrorHandler::ExtraData extraData("OBSBasicSettings");
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_RESOLUTION_VIRTUALCAM_ACTIVED,
							      PLSErrKeyAllAlert, {}, extraData);
		},
		Qt::QueuedConnection);
}

void OBSBasicSettings::ResetSettings()
{
	auto config = main->Config();
	auto globalConfig = App()->GetAppConfig();

	// reset general settings
#if defined(_WIN32) || defined(__APPLE__)
	config_remove_value(globalConfig, "General", "EnableAutoUpdates");
#endif

	config_set_bool(globalConfig, "General", "Watermark", true);

	config_remove_value(config, "General", "OpenStatsOnStartup");
	config_remove_value(globalConfig, "BasicWindow", "HideOBSWindowsFromCapture");
	config_remove_value(globalConfig, "BasicWindow", "SnappingEnabled");
	config_remove_value(globalConfig, "BasicWindow", "ScreenSnapping");
	config_remove_value(globalConfig, "BasicWindow", "CenterSnapping");
	config_remove_value(globalConfig, "BasicWindow", "SourceSnapping");
	config_remove_value(globalConfig, "BasicWindow", "SnapDistance");
	config_remove_value(globalConfig, "BasicWindow", "OverflowAlwaysVisible");
	config_remove_value(globalConfig, "BasicWindow", "OverflowHidden");
	config_remove_value(globalConfig, "BasicWindow", "OverflowSelectionHidden");
	config_remove_value(globalConfig, "BasicWindow", "TransitionOnDoubleClick");

	config_remove_value(globalConfig, "BasicWindow", "WarnBeforeStartingStream");
	config_remove_value(globalConfig, "BasicWindow", "WarnBeforeStoppingStream");
	config_remove_value(globalConfig, "BasicWindow", "WarnBeforeStoppingRecord");

	config_remove_value(globalConfig, "BasicWindow", "SpacingHelpersEnabled");
	config_remove_value(globalConfig, "BasicWindow", "PreviewZoomEnabled");
	config_remove_value(globalConfig, "BasicWindow", "HideProjectorCursor");
	config_remove_value(globalConfig, "BasicWindow", "ProjectorAlwaysOnTop");

	config_remove_value(globalConfig, "BasicWindow", "RecordWhenStreaming");
	config_remove_value(globalConfig, "BasicWindow", "KeepRecordingWhenStreamStops");

	config_remove_value(globalConfig, "BasicWindow", "ReplayBufferWhileStreaming");
	config_remove_value(globalConfig, "BasicWindow", "KeepReplayBufferStreamStops");

	config_remove_value(globalConfig, "BasicWindow", "SaveProjectors");
	config_remove_value(globalConfig, "BasicWindow", "CloseExistingProjectors");
	config_remove_value(globalConfig, "BasicWindow", "StudioPortraitLayout");
	config_remove_value(globalConfig, "BasicWindow", "StudioModeLabels");

	config_remove_value(globalConfig, "BasicWindow", "MultiviewMouseSwitch");
	config_remove_value(globalConfig, "BasicWindow", "MultiviewDrawNames");
	config_remove_value(globalConfig, "BasicWindow", "MultiviewDrawAreas");
	config_remove_value(globalConfig, "BasicWindow", "MultiviewLayout");

	config_remove_value(globalConfig, "BasicWindow", "SysTrayEnabled");
	config_remove_value(globalConfig, "BasicWindow", "SysTrayWhenStarted");
	config_remove_value(globalConfig, "BasicWindow", "SysTrayMinimizeToTray");

	config_remove_value(globalConfig, "BasicWindow", "ShowSafeAreas");
	config_remove_value(globalConfig, "General", "AutomaticCollectionSearch");

	// reset output settings
	config_remove_value(config, "Output", "Mode");
	config_remove_value(config, "SimpleOutput", "VBitrate");
	config_remove_value(config, "SimpleOutput", "StreamEncoder");
	config_remove_value(config, "SimpleOutput", "ABitrate");
	config_remove_value(config, "SimpleOutput", "FilePath");
	config_remove_value(config, "SimpleOutput", "FileNameWithoutSpace");
	config_remove_value(config, "SimpleOutput", "RecFormat2");
	config_remove_value(config, "SimpleOutput", "UseAdvanced");
	config_remove_value(config, "SimpleOutput", "EnforceBitrate");
	config_remove_value(config, "SimpleOutput", "QSVPreset");
	config_remove_value(config, "SimpleOutput", "NVENCPreset2");
	config_remove_value(config, "SimpleOutput", "AMDPreset");
	config_remove_value(config, "SimpleOutput", "AMDAV1Preset");
	config_remove_value(config, "SimpleOutput", "Preset");
	config_remove_value(config, "SimpleOutput", "x264Settings");
	config_remove_value(config, "SimpleOutput", "RecQuality");
	config_remove_value(config, "SimpleOutput", "RecEncoder");
	config_remove_value(config, "SimpleOutput", "MuxerCustom");
	config_remove_value(config, "SimpleOutput", "RecRB");
	config_remove_value(config, "SimpleOutput", "RecRBTime");
	config_remove_value(config, "SimpleOutput", "RecRBSize");
	config_remove_value(config, "SimpleOutput", "TrackIndex");

	config_remove_value(config, "SimpleOutput", "StreamAudioEncoder");
	config_remove_value(config, "SimpleOutput", "RecAudioEncoder");
	config_remove_value(config, "SimpleOutput", "RecTracks");

	config_remove_value(config, "AdvOut", "Encoder");
	config_remove_value(config, "AdvOut", "RescaleFilter");
	config_remove_value(config, "AdvOut", "StreamMultiTrackAudioMixes");
	config_remove_value(config, "AdvOut", "RescaleRes");
	config_remove_value(config, "AdvOut", "TrackIndex");
	config_remove_value(config, "AdvOut", "TrackIndexV");

	config_remove_value(config, "AdvOut", "RecType");
	config_remove_value(config, "AdvOut", "RecFilePath");
	config_remove_value(config, "AdvOut", "RecFileNameWithoutSpace");
	config_remove_value(config, "AdvOut", "RecFormat2");
	config_remove_value(config, "AdvOut", "RecEncoder");
	config_remove_value(config, "AdvOut", "RecRescaleFilter");
	config_remove_value(config, "AdvOut", "RecRescaleRes");
	config_remove_value(config, "AdvOut", "RecMuxerCustom");
	config_remove_value(config, "AdvOut", "RecTracks");
	config_remove_value(config, "AdvOut", "FLVTrack");

	config_remove_value(config, "AdvOut", "RecSplitFile");
	config_remove_value(config, "AdvOut", "RecSplitFileType");
	config_remove_value(config, "AdvOut", "RecSplitFileTime");
	config_remove_value(config, "AdvOut", "RecSplitFileSize");

	config_remove_value(config, "AdvOut", "FFOutputToFile");
	config_remove_value(config, "AdvOut", "FFFilePath");
	config_remove_value(config, "AdvOut", "FFFileNameWithoutSpace");
	config_remove_value(config, "AdvOut", "FFURL");
	config_remove_value(config, "AdvOut", "FFFormat");
	config_remove_value(config, "AdvOut", "FFFormatMimeType");
	config_remove_value(config, "AdvOut", "FFExtension");

	config_remove_value(config, "AdvOut", "FFMCustom");
	config_remove_value(config, "AdvOut", "FFVBitrate");
	config_remove_value(config, "AdvOut", "FFVGOPSize");
	config_remove_value(config, "AdvOut", "FFRescale");
	config_remove_value(config, "AdvOut", "FFIgnoreCompat");
	config_remove_value(config, "AdvOut", "FFRescaleRes");
	config_remove_value(config, "AdvOut", "FFVEncoderId");
	config_remove_value(config, "AdvOut", "FFVEncoder");
	config_remove_value(config, "AdvOut", "FFVCustom");
	config_remove_value(config, "AdvOut", "FFABitrate");
	config_remove_value(config, "AdvOut", "FFAEncoderId");
	config_remove_value(config, "AdvOut", "FFAEncoder");
	config_remove_value(config, "AdvOut", "FFACustom");
	config_remove_value(config, "AdvOut", "FFAudioMixes");
	config_remove_value(config, "AdvOut", "Track1Bitrate");
	config_remove_value(config, "AdvOut", "Track2Bitrate");
	config_remove_value(config, "AdvOut", "Track3Bitrate");
	config_remove_value(config, "AdvOut", "Track4Bitrate");
	config_remove_value(config, "AdvOut", "Track5Bitrate");
	config_remove_value(config, "AdvOut", "Track6Bitrate");
	config_remove_value(config, "AdvOut", "Track1Name");
	config_remove_value(config, "AdvOut", "Track2Name");
	config_remove_value(config, "AdvOut", "Track3Name");
	config_remove_value(config, "AdvOut", "Track4Name");
	config_remove_value(config, "AdvOut", "Track5Name");
	config_remove_value(config, "AdvOut", "Track6Name");
	config_remove_value(config, "AdvOut", "TrackStereoName");
	config_remove_value(config, "AdvOut", "TrackImmersiveName");

	config_remove_value(config, "AdvOut", "RecRB");
	config_remove_value(config, "AdvOut", "RecRBTime");
	config_remove_value(config, "AdvOut", "RecRBSize");
	config_remove_value(config, "AdvOut", "AudioEncoder");
	config_remove_value(config, "AdvOut", "RecAudioEncoder");

	config_remove_value(config, "Stream1", "EnableMultitrackVideo");
	config_remove_value(config, "Stream1", "MultitrackVideoMaximumAggregateBitrateAuto");
	config_remove_value(config, "Stream1", "MultitrackVideoMaximumAggregateBitrate");
	config_remove_value(config, "Stream1", "MultitrackVideoMaximumVideoTracksAuto");
	config_remove_value(config, "Stream1", "MultitrackVideoMaximumVideoTracks");
	config_remove_value(config, "Stream1", "MultitrackVideoStreamDumpEnabled");
	config_remove_value(config, "Stream1", "MultitrackVideoConfigOverrideEnabled");
	config_remove_value(config, "Stream1", "MultitrackVideoConfigOverride");

	removeJsonData("streamEncoder.json");
	removeJsonData("recordEncoder.json");

	// reset video settings
	config_remove_value(config, "Video", "BaseCX");
	config_remove_value(config, "Video", "BaseCY");

	config_remove_value(config, "Video", "OutputCX");
	config_remove_value(config, "Video", "OutputCY");

	config_remove_value(config, "Video", "FPSType");

	config_remove_value(config, "Video", "FPSCommon");
	config_remove_value(config, "Video", "FPSInt");
	config_remove_value(config, "Video", "FPSNum");
	config_remove_value(config, "Video", "FPSDen");
	config_remove_value(config, "Video", "ScaleType");

	if (videoPage) {
		videoPage->downscaleFilter->setItemData(videoPage->downscaleFilter->currentIndex(), QT_UTF8("bicubic"));
	}

	config_remove_value(config, "Video", "BaseCXV");
	config_remove_value(config, "Video", "BaseCYV");

	config_remove_value(config, "Video", "OutputCXV");
	config_remove_value(config, "Video", "OutputCYV");

	// reset audio settings
	config_remove_value(config, "Audio", "SampleRate");
	config_remove_value(config, "Audio", "ChannelSetup");
	config_remove_value(config, "Audio", "MeterDecayRate");
	config_remove_value(config, "Audio", "PeakMeterType");
	config_remove_value(globalConfig, "Audio", "LowLatencyAudioBuffering");

	config_remove_value(globalConfig, "Accessibility", "OverrideColors");
	config_remove_value(globalConfig, "Accessibility", "ColorPreset");
	config_remove_value(globalConfig, "Accessibility", "SelectRed");
	config_remove_value(globalConfig, "Accessibility", "SelectGreen");
	config_remove_value(globalConfig, "Accessibility", "SelectBlue");
	config_remove_value(globalConfig, "Accessibility", "SelectPurple");
	config_remove_value(globalConfig, "Accessibility", "MixerGreen");
	config_remove_value(globalConfig, "Accessibility", "MixerYellow");
	config_remove_value(globalConfig, "Accessibility", "MixerRed");
	config_remove_value(globalConfig, "Accessibility", "MixerGreenActive");
	config_remove_value(globalConfig, "Accessibility", "MixerYellowActive");
	config_remove_value(globalConfig, "Accessibility", "MixerRedActive");

	for (auto &audioSource : audioSources) {
		auto source = OBSGetStrongRef(get<0>(audioSource));
		if (source) {
			obs_source_enable_push_to_mute(source, false);
			obs_source_set_push_to_mute_delay(source, 0);

			obs_source_enable_push_to_talk(source, false);
			obs_source_set_push_to_talk_delay(source, 0);
		}
	}

	auto resetAudioDevice = [this](bool input, const char *value, const char *name, int index) {
		main->ResetAudioDevice(input ? App()->InputAudioSource() : App()->OutputAudioSource(), value, Str(name),
				       index);
	};

	main->CreateFirstRunSources();
	resetAudioDevice(false, "disabled", "Basic.DesktopDevice2", 2);
	resetAudioDevice(true, "disabled", "Basic.AuxDevice2", 4);
	resetAudioDevice(true, "disabled", "Basic.AuxDevice3", 5);
	resetAudioDevice(true, "disabled", "Basic.AuxDevice4", 6);

	// reset advanced settings
	config_remove_value(globalConfig, "Video", "Renderer");
	config_remove_value(globalConfig, "General", "ProcessPriority");
#ifdef _WIN32
	SetProcessPriority(config_get_default_string(globalConfig, "General", "ProcessPriority"));
#endif

	config_remove_value(config, "Output", "NewSocketLoopEnable");
	config_remove_value(config, "Output", "LowLatencyEnable");

	config_remove_value(globalConfig, "General", "BrowserHWAccel");
	config_remove_value(globalConfig, "General", "HotkeyFocusType");
	config_remove_value(globalConfig, "General", "ConfirmOnExit");

	config_remove_value(config, "Video", "ColorFormat");
	config_remove_value(config, "Video", "ColorSpace");
	config_remove_value(config, "Video", "ColorRange");
	config_remove_value(config, "Video", "SdrWhiteLevel");
	config_remove_value(config, "Video", "HdrNominalPeakLevel");

	config_remove_value(config, "Audio", "MonitoringDeviceName");
	config_remove_value(config, "Audio", "MonitoringDeviceId");

	config_remove_value(globalConfig, "Audio", "DisableAudioDucking");

	config_remove_value(config, "Output", "FilenameFormatting");
	config_remove_value(config, "SimpleOutput", "RecRBPrefix");
	config_remove_value(config, "SimpleOutput", "RecRBSuffix");
	config_remove_value(config, "Output", "OverwriteIfExists");
	config_remove_value(config, "Output", "DelayEnable");
	config_remove_value(config, "Output", "DelaySec");
	config_remove_value(config, "Output", "DelayPreserve");
	config_remove_value(config, "Output", "Reconnect");
	config_remove_value(config, "Output", "RetryDelay");
	config_remove_value(config, "Output", "MaxRetries");
	config_remove_value(config, "Output", "BindIP");
	config_remove_value(config, "Video", "AutoRemux");
	config_remove_value(config, "Output", "DynamicBitrate");
	config_remove_value(config, "Output", "IPFamily");

	if (!audioPage) {
		initAudioPage();
	}
	audioPage->monitoringDevice->setCurrentIndex(0);
	QString newDevice = audioPage->monitoringDevice->currentData().toString();
	obs_set_audio_monitoring_device(QT_TO_UTF8(audioPage->monitoringDevice->currentText()), QT_TO_UTF8(newDevice));

	for (const auto &hotkey : hotkeys) {
		hotkey.second->Clear();
	}

	ResetSceneDisplayMethodSettings();

	SaveHotkeySettings();

	config_set_string(config, "Hotkeys", "ReplayBuffer",
			  "{\"ReplayBuffer.Save\":[{\"alt\":true,\"key\":\"OBS_KEY_R\"}]}");
	config_set_string(config, "Others", "Hotkeys.ReplayBuffer", "Alt+R");

	main->InitBasicConfigDefaults();
	main->InitBasicConfigDefaults2();

#ifdef _WIN32
	DisableAudioDucking(config_get_default_bool(globalConfig, "Audio", "DisableAudioDucking"));
#endif
	main->UpdateVolumeControlsDecayRate();
	main->UpdateVolumeControlsPeakMeterType();

	main->ResetUI();
	OBSProjector::UpdateMultiviewProjectors();

	main->ClearService();
	main->ResetVideo();

	main->SaveProject();
	main->RefreshVolumeColors();
	main->UpdatePreviewZoomEnabled();

	config_save_safe(config, "tmp", nullptr);
	config_save_safe(globalConfig, "tmp", nullptr);
}

void OBSBasicSettings::on_resetButton_clicked()
{
	PLSErrorHandler::ExtraData extraData("OBSBasicSettings");
	auto button = PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_SETTINGS_RESET_QUESTION,
							    PLSErrKeyAllAlert, {}, extraData);

	if (button.clickedBtn != PLSAlertView::Button::Yes) {
		return;
	}
	if (pls_is_output_actived()) {
		showSaveVideoAlert();
		return;
	}
	ResetSettings();
	LoadSettings(false);
	ClearChanged();

	if (outputSimplePage) {
		if (int idx = outputSimplePage->simpleOutStrAEncoder->currentIndex(); idx != -1) {
			auto str = outputSimplePage->simpleOutStrAEncoder->itemData(idx).toString();
			config_set_string(main->Config(), "SimpleOutput", "StreamAudioEncoder", QT_TO_UTF8(str));
		}
	} else {
	}
	if (outputStreamPage) {
		if (int idx = outputStreamPage->advOutAEncoder->currentIndex(); idx != -1) {
			auto str = outputStreamPage->advOutAEncoder->itemData(idx).toString();
			config_set_string(main->Config(), "AdvOut", "AudioEncoder", QT_TO_UTF8(str));
		}
	} else {
	}
	main->ResetOutputs();
	main->UpdatePreviewSafeAreas();
	AdvOutRecCheckCodecs();
}

QIcon OBSBasicSettings::GetGeneralIcon() const
{
	return generalIcon;
}

QIcon OBSBasicSettings::GetAppearanceIcon() const
{
	return appearanceIcon;
}

QIcon OBSBasicSettings::GetStreamIcon() const
{
	return streamIcon;
}

QIcon OBSBasicSettings::GetOutputIcon() const
{
	return outputIcon;
}

QIcon OBSBasicSettings::GetAudioIcon() const
{
	return audioIcon;
}

QIcon OBSBasicSettings::GetVideoIcon() const
{
	return videoIcon;
}

QIcon OBSBasicSettings::GetHotkeysIcon() const
{
	return hotkeysIcon;
}

QIcon OBSBasicSettings::GetAccessibilityIcon() const
{
	return accessibilityIcon;
}

QIcon OBSBasicSettings::GetAdvancedIcon() const
{
	return advancedIcon;
}

void OBSBasicSettings::SetGeneralIcon(const QIcon &icon)
{
	ui->listWidget->item(Pages::GENERAL)->setIcon(icon);
}

void OBSBasicSettings::SetStreamIcon(const QIcon &icon) {}

void OBSBasicSettings::SetOutputIcon(const QIcon &icon)
{
	ui->listWidget->item(Pages::OUTPUT)->setIcon(icon);
}

void OBSBasicSettings::SetAudioIcon(const QIcon &icon)
{
	ui->listWidget->item(Pages::AUDIO)->setIcon(icon);
}

void OBSBasicSettings::SetVideoIcon(const QIcon &icon)
{
	ui->listWidget->item(Pages::VIDEO)->setIcon(icon);
}

void OBSBasicSettings::SetHotkeysIcon(const QIcon &icon)
{
	ui->listWidget->item(Pages::HOTKEYS)->setIcon(icon);
}

void OBSBasicSettings::SetAccessibilityIcon(const QIcon &icon)
{
	ui->listWidget->item(Pages::ACCESSIBILITY)->setIcon(icon);
}

void OBSBasicSettings::SetAdvancedIcon(const QIcon &icon)
{
	ui->listWidget->item(Pages::ADVANCED)->setIcon(icon);
}

int OBSBasicSettings::CurrentFLVTrack()
{
	if (outputRecordPage->flvTrack1->isChecked())
		return 1;
	else if (outputRecordPage->flvTrack2->isChecked())
		return 2;
	else if (outputRecordPage->flvTrack3->isChecked())
		return 3;
	else if (outputRecordPage->flvTrack4->isChecked())
		return 4;
	else if (outputRecordPage->flvTrack5->isChecked())
		return 5;
	else if (outputRecordPage->flvTrack6->isChecked())
		return 6;

	return 0;
}

int OBSBasicSettings::SimpleOutGetSelectedAudioTracks()
{
	int tracks = (outputSimplePage->simpleOutRecTrack1->isChecked() ? (1 << 0) : 0) |
		     (outputSimplePage->simpleOutRecTrack2->isChecked() ? (1 << 1) : 0) |
		     (outputSimplePage->simpleOutRecTrack3->isChecked() ? (1 << 2) : 0) |
		     (outputSimplePage->simpleOutRecTrack4->isChecked() ? (1 << 3) : 0) |
		     (outputSimplePage->simpleOutRecTrack5->isChecked() ? (1 << 4) : 0) |
		     (outputSimplePage->simpleOutRecTrack6->isChecked() ? (1 << 5) : 0);
	return tracks;
}

int OBSBasicSettings::AdvOutGetSelectedAudioTracks()
{
	int tracks = (outputRecordPage->advOutRecTrack1->isChecked() ? (1 << 0) : 0) |
		     (outputRecordPage->advOutRecTrack2->isChecked() ? (1 << 1) : 0) |
		     (outputRecordPage->advOutRecTrack3->isChecked() ? (1 << 2) : 0) |
		     (outputRecordPage->advOutRecTrack4->isChecked() ? (1 << 3) : 0) |
		     (outputRecordPage->advOutRecTrack5->isChecked() ? (1 << 4) : 0) |
		     (outputRecordPage->advOutRecTrack6->isChecked() ? (1 << 5) : 0);
	return tracks;
}

int OBSBasicSettings::AdvOutGetStreamingSelectedAudioTracks()
{
	int tracks = (outputStreamPage->advOutMultiTrack1->isChecked() ? (1 << 0) : 0) |
		     (outputStreamPage->advOutMultiTrack2->isChecked() ? (1 << 1) : 0) |
		     (outputStreamPage->advOutMultiTrack3->isChecked() ? (1 << 2) : 0) |
		     (outputStreamPage->advOutMultiTrack4->isChecked() ? (1 << 3) : 0) |
		     (outputStreamPage->advOutMultiTrack5->isChecked() ? (1 << 4) : 0) |
		     (outputStreamPage->advOutMultiTrack6->isChecked() ? (1 << 5) : 0);
	return tracks;
}

void OBSBasicSettings::UpdateAdvNetworkGroup()
{
	if (!advancedPage) {
		return;
	}

	bool enabled = protocol.contains("RTMP");

	advancedPage->advNetworkDisabled->setVisible(!enabled);

	advancedPage->bindToIPLabel->setVisible(enabled);
	advancedPage->bindToIP->setVisible(enabled);
	advancedPage->dynBitrate->setVisible(enabled);
	advancedPage->ipFamilyLabel->setVisible(enabled);
	advancedPage->ipFamily->setVisible(enabled);
#ifdef _WIN32
	advancedPage->enableNewSocketLoop->setVisible(enabled);
	advancedPage->enableLowLatencyMode->setVisible(enabled);
#endif
}

extern bool MultitrackVideoDeveloperModeEnabled();

void OBSBasicSettings::UpdateMultitrackVideo()
{
	// Technically, it should currently be safe to toggle multitrackVideo
	// while not streaming (recording should be irrelevant), but practically
	// output settings aren't currently being tracked with that degree of
	// flexibility, so just disable everything while outputs are active.
	auto toggle_available = !main->Active();

	// FIXME: protocol is not updated properly for WHIP; what do?
	auto available = protocol.startsWith("RTMP");

	if (!available) {
		auto activiedPlatforms = PLS_PLATFORM_ACTIVIED;
		if (1 == activiedPlatforms.size()) {
			if (auto pPlatform = activiedPlatforms.front();
			    pPlatform->getChannelType() >= ChannelData::ChannelDataType::CustomType &&
			    pPlatform->getChannelName() == TWITCH) {

				available = true;
			}
		}
	}

	if (available && pls_is_dual_output_on()) {
		available = false;
	}

	if (available && !IsCustomService()) {
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, "service", qUtf8Printable(PLSBasic::instance()->getServiceName()));
		OBSServiceAutoRelease temp_service =
			obs_service_create_private("rtmp_common", "auto config query service", settings);
		settings = obs_service_get_settings(temp_service);
		available = obs_data_has_user_value(settings, "multitrack_video_configuration_url");
		if (!available && outputPage->enableMultitrackVideo->isChecked())
			outputPage->enableMultitrackVideo->setChecked(false);
	}

#ifndef _WIN32
	available = available && MultitrackVideoDeveloperModeEnabled();
#endif

	if (IsCustomService())
		available = available && MultitrackVideoDeveloperModeEnabled();

	outputPage->multitrackVideoGroupBox->setVisible(available);

	outputPage->enableMultitrackVideo->setEnabled(toggle_available);

	outputPage->multitrackVideoMaximumAggregateBitrateLabel->setEnabled(
		toggle_available && outputPage->enableMultitrackVideo->isChecked());
	outputPage->multitrackVideoMaximumAggregateBitrateAuto->setEnabled(
		toggle_available && outputPage->enableMultitrackVideo->isChecked());
	outputPage->multitrackVideoMaximumAggregateBitrate->setEnabled(
		toggle_available && outputPage->enableMultitrackVideo->isChecked() &&
		!outputPage->multitrackVideoMaximumAggregateBitrateAuto->isChecked());

	outputPage->multitrackVideoMaximumVideoTracksLabel->setEnabled(toggle_available &&
								       outputPage->enableMultitrackVideo->isChecked());
	outputPage->multitrackVideoMaximumVideoTracksAuto->setEnabled(toggle_available &&
								      outputPage->enableMultitrackVideo->isChecked());
	outputPage->multitrackVideoMaximumVideoTracks->setEnabled(
		toggle_available && outputPage->enableMultitrackVideo->isChecked() &&
		!outputPage->multitrackVideoMaximumVideoTracksAuto->isChecked());

	outputPage->multitrackVideoStreamDumpEnable->setVisible(available && MultitrackVideoDeveloperModeEnabled());
	outputPage->multitrackVideoConfigOverrideEnable->setVisible(available && MultitrackVideoDeveloperModeEnabled());
	outputPage->multitrackVideoConfigOverrideLabel->setVisible(available && MultitrackVideoDeveloperModeEnabled());
	outputPage->multitrackVideoConfigOverride->setVisible(available && MultitrackVideoDeveloperModeEnabled());

	outputPage->multitrackVideoStreamDumpEnable->setEnabled(toggle_available &&
								outputPage->enableMultitrackVideo->isChecked());
	outputPage->multitrackVideoConfigOverrideEnable->setEnabled(toggle_available &&
								    outputPage->enableMultitrackVideo->isChecked());
	outputPage->multitrackVideoConfigOverrideLabel->setEnabled(
		toggle_available && outputPage->enableMultitrackVideo->isChecked() &&
		outputPage->multitrackVideoConfigOverrideEnable->isChecked());
	outputPage->multitrackVideoConfigOverride->setEnabled(
		toggle_available && outputPage->enableMultitrackVideo->isChecked() &&
		outputPage->multitrackVideoConfigOverrideEnable->isChecked());

	auto update_simple_output_settings = [&](bool mtv_enabled) {
		if (outputSimplePage) {
			auto recording_uses_stream_encoder =
				outputSimplePage->simpleOutRecQuality->currentData().toString() == "Stream";
			mtv_enabled = mtv_enabled && !recording_uses_stream_encoder;

			outputSimplePage->simpleOutputVBitrateLabel->setDisabled(mtv_enabled);
			outputSimplePage->simpleOutputVBitrate->setDisabled(mtv_enabled);

			outputSimplePage->simpleOutputABitrateLabel->setDisabled(mtv_enabled);
			outputSimplePage->simpleOutputABitrate->setDisabled(mtv_enabled);

			bool bVideoActive = obs_video_active();

			outputSimplePage->simpleOutStrEncoderLabel->setDisabled(bVideoActive || mtv_enabled);
			outputSimplePage->simpleOutStrEncoder->setDisabled(bVideoActive || mtv_enabled);

			outputSimplePage->simpleOutPresetLabel->setDisabled(mtv_enabled);
			outputSimplePage->simpleOutPreset->setDisabled(mtv_enabled);

			outputSimplePage->simpleOutCustomLabel->setDisabled(mtv_enabled);
			outputSimplePage->simpleOutCustom->setDisabled(mtv_enabled);

			outputSimplePage->simpleOutStrAEncoderLabel->setDisabled(bVideoActive || mtv_enabled);
			outputSimplePage->simpleOutStrAEncoder->setDisabled(bVideoActive || mtv_enabled);
		}
	};

	auto update_advanced_output_settings = [&](bool mtv_enabled) {
		if (outputStreamPage) {
			auto recording_uses_stream_video_encoder =
				(outputRecordPage ? outputRecordPage->advOutRecEncoder->currentData()
						  : config_get_string(main->Config(), "AdvOut", "RecEncoder")) ==
				QStringLiteral("none");
			auto recording_uses_stream_audio_encoder =
				(outputRecordPage ? outputRecordPage->advOutRecAEncoder->currentData()
						  : config_get_string(main->Config(), "AdvOut", "RecAudioEncoder")) ==
				QStringLiteral("none");
			auto disable_video = mtv_enabled && !recording_uses_stream_video_encoder;
			auto disable_audio = mtv_enabled && !recording_uses_stream_audio_encoder;

			outputStreamPage->advOutAEncLabel->setDisabled(disable_audio);
			outputStreamPage->advOutAEncoder->setDisabled(disable_audio);

			outputStreamPage->advOutEncLabel->setDisabled(disable_video);
			outputStreamPage->advOutEncoder->setDisabled(disable_video);

			outputStreamPage->advOutUseRescale->setDisabled(disable_video);
			outputStreamPage->advOutRescale->setDisabled(
				disable_video ||
				outputStreamPage->advOutRescaleFilter->currentData() == OBS_SCALE_DISABLE);
			outputStreamPage->advOutRescaleFilter->setDisabled(disable_video);

			if (streamEncoderProps)
				streamEncoderProps->SetDisabled(disable_video);
		}
	};

	auto update_advanced_output_audio_tracks = [&](bool mtv_enabled) {
		if (outputAudioPage) {
			auto vod_track_enabled = vodTrackCheckbox && vodTrackCheckbox->isChecked();

			auto vod_track_idx_enabled = [&](size_t idx) {
				return vod_track_enabled && vodTrack[idx - 1] && vodTrack[idx - 1]->isChecked();
			};

			int trackIndex = config_get_int(main->Config(), "AdvOut", "TrackIndex");
			int tracks = config_get_int(main->Config(), "AdvOut", "RecTracks");

			auto track1_warning_visible =
				mtv_enabled &&
				((outputStreamPage ? outputStreamPage->advOutTrack1->isChecked() : 1 == trackIndex) ||
				 vod_track_idx_enabled(1));
			auto track1_disabled = track1_warning_visible &&
					       !(outputRecordPage ? outputRecordPage->advOutRecTrack1->isChecked()
								  : tracks & (1 << 0));
			outputAudioPage->advOutTrack1BitrateLabel->setDisabled(track1_disabled);
			outputAudioPage->advOutTrack1Bitrate->setDisabled(track1_disabled);

			auto track2_warning_visible =
				mtv_enabled &&
				((outputStreamPage ? outputStreamPage->advOutTrack2->isChecked() : 2 == trackIndex) ||
				 vod_track_idx_enabled(2));
			auto track2_disabled = track2_warning_visible &&
					       !(outputRecordPage ? outputRecordPage->advOutRecTrack2->isChecked()
								  : tracks & (1 << 1));
			outputAudioPage->advOutTrack2BitrateLabel->setDisabled(track2_disabled);
			outputAudioPage->advOutTrack2Bitrate->setDisabled(track2_disabled);

			auto track3_warning_visible =
				mtv_enabled &&
				((outputStreamPage ? outputStreamPage->advOutTrack3->isChecked() : 3 == trackIndex) ||
				 vod_track_idx_enabled(3));
			auto track3_disabled = track3_warning_visible &&
					       !(outputRecordPage ? outputRecordPage->advOutRecTrack3->isChecked()
								  : tracks & (1 << 2));
			outputAudioPage->advOutTrack3BitrateLabel->setDisabled(track3_disabled);
			outputAudioPage->advOutTrack3Bitrate->setDisabled(track3_disabled);

			auto track4_warning_visible =
				mtv_enabled &&
				((outputStreamPage ? outputStreamPage->advOutTrack4->isChecked() : 4 == trackIndex) ||
				 vod_track_idx_enabled(4));
			auto track4_disabled = track4_warning_visible &&
					       !(outputRecordPage ? outputRecordPage->advOutRecTrack4->isChecked()
								  : tracks & (1 << 3));
			outputAudioPage->advOutTrack4BitrateLabel->setDisabled(track4_disabled);
			outputAudioPage->advOutTrack4Bitrate->setDisabled(track4_disabled);

			auto track5_warning_visible =
				mtv_enabled &&
				((outputStreamPage ? outputStreamPage->advOutTrack5->isChecked() : 5 == trackIndex) ||
				 vod_track_idx_enabled(5));
			auto track5_disabled = track5_warning_visible &&
					       !(outputRecordPage ? outputRecordPage->advOutRecTrack5->isChecked()
								  : tracks & (1 << 4));
			outputAudioPage->advOutTrack5BitrateLabel->setDisabled(track5_disabled);
			outputAudioPage->advOutTrack5Bitrate->setDisabled(track5_disabled);

			auto track6_warning_visible =
				mtv_enabled &&
				((outputStreamPage ? outputStreamPage->advOutTrack6->isChecked() : 6 == trackIndex) ||
				 vod_track_idx_enabled(6));
			auto track6_disabled = track6_warning_visible &&
					       !(outputRecordPage ? outputRecordPage->advOutRecTrack6->isChecked()
								  : tracks & (1 << 6));
			outputAudioPage->advOutTrack6BitrateLabel->setDisabled(track6_disabled);
			outputAudioPage->advOutTrack6Bitrate->setDisabled(track6_disabled);
		}
	};

	if (available) {
		OBSDataAutoRelease settings;
		{
			auto service_name = PLSBasic::instance()->getServiceName();
			auto custom_server = outputPage->customServer->text().trimmed();

			obs_properties_t *props = obs_get_service_properties("rtmp_common");
			obs_property_t *service = obs_properties_get(props, "service");

			settings = obs_data_create();

			obs_data_set_string(settings, "service", QT_TO_UTF8(service_name));
			obs_property_modified(service, settings);

			obs_properties_destroy(props);
		}

		auto multitrack_video_name = QTStr("Basic.Settings.Stream.MultitrackVideoLabel");
		if (obs_data_has_user_value(settings, "multitrack_video_name"))
			multitrack_video_name = obs_data_get_string(settings, "multitrack_video_name");

		outputPage->enableMultitrackVideo->setText(
			QTStr("Basic.Settings.Stream.EnableMultitrackVideo").arg(multitrack_video_name));

		if (obs_data_has_user_value(settings, "multitrack_video_disclaimer")) {
			outputPage->multitrackVideoInfo->setVisible(true);
			outputPage->multitrackVideoInfo->setText(
				obs_data_get_string(settings, "multitrack_video_disclaimer"));
		} else {
			outputPage->multitrackVideoInfo->setText(
				QTStr("MultitrackVideo.Info")
					.arg(multitrack_video_name, PLSBasic::instance()->getServiceName()));
		}

		auto mtv_enabled = outputPage->enableMultitrackVideo->isChecked();

		update_simple_output_settings(mtv_enabled);
		update_advanced_output_settings(mtv_enabled);
		update_advanced_output_audio_tracks(mtv_enabled);
	} else {
		update_simple_output_settings(false);
		update_advanced_output_settings(false);
		update_advanced_output_audio_tracks(false);
	}
}

void OBSBasicSettings::SimpleStreamAudioEncoderChanged()
{
	PopulateSimpleBitrates(outputSimplePage->simpleOutputABitrate,
			       outputSimplePage->simpleOutStrAEncoder->currentData().toString() == "opus");

	if (IsSurround(audioPage ? QT_TO_UTF8(audioPage->channelSetup->currentText())
				 : config_get_string(main->Config(), "Audio", "ChannelSetup")))
		return;

	RestrictResetBitrates({outputSimplePage->simpleOutputABitrate}, 320);
}

void OBSBasicSettings::AdvAudioEncodersChanged()
{
	if (outputAudioPage) {
		QString streamEncoder = outputStreamPage ? outputStreamPage->advOutAEncoder->currentData().toString()
							 : config_get_string(main->Config(), "AdvOut", "AudioEncoder");
		QString recEncoder = outputRecordPage ? outputRecordPage->advOutRecAEncoder->currentData().toString()
						      : config_get_string(main->Config(), "AdvOut", "RecAudioEncoder");

		if (recEncoder == "none")
			recEncoder = streamEncoder;

		PopulateAdvancedBitrates({outputAudioPage->advOutTrack1Bitrate, outputAudioPage->advOutTrack2Bitrate,
					  outputAudioPage->advOutTrack4Bitrate, outputAudioPage->advOutTrack5Bitrate,
					  outputAudioPage->advOutTrack6Bitrate},
					 QT_TO_UTF8(streamEncoder), QT_TO_UTF8(recEncoder));

		if (IsSurround(audioPage ? QT_TO_UTF8(audioPage->channelSetup->currentText())
					 : config_get_string(main->Config(), "Audio", "ChannelSetup")))
			return;

		RestrictResetBitrates({outputAudioPage->advOutTrack1Bitrate, outputAudioPage->advOutTrack2Bitrate,
				       outputAudioPage->advOutTrack3Bitrate, outputAudioPage->advOutTrack4Bitrate,
				       outputAudioPage->advOutTrack5Bitrate, outputAudioPage->advOutTrack6Bitrate},
				      320);
	}
}

static bool checkParent(const QWidget *widget, const QWidget *parent)
{
	for (; widget; widget = widget->parentWidget()) {
		if (widget == parent) {
			return true;
		}
	}
	return false;
}

static bool checkOutputPageWidget(const Ui::OBSBasicSettings *ui, const Ui::SettingOutputPage *outputPage,
				  const QWidget *widget, const QWidget *advCurrentTab)
{
	if (outputPage) {
		if (outputPage->outputMode->currentIndex() == 0) {
			if (checkParent(widget, outputPage->easyOutputsPage)) {
				return true;
			} else if (checkParent(widget, outputPage->advOutTabs)) {
				return false;
			}
			return true;
		} else {
			if (checkParent(widget, outputPage->easyOutputsPage)) {
				return false;
			} else if (checkParent(widget, outputPage->advOutTabs)) {
				return checkParent(widget, advCurrentTab);
			}
			return true;
		}
	} else {
		return false;
	}
}

static bool isInCurrentPage(const Ui::OBSBasicSettings *ui, const Ui::SettingOutputPage *outputPage,
			    const QWidget *page, const QWidget *widget, const QWidget *advCurrentTab)
{
	if (page == ui->generalPage) {
		return ui->listWidget->currentRow() == OBSBasicSettings::Pages::GENERAL;
	} else if (page == ui->outputPage) {
		return ui->listWidget->currentRow() == OBSBasicSettings::Pages::OUTPUT &&
		       checkOutputPageWidget(ui, outputPage, widget, advCurrentTab);
	} else if (page == ui->audioPage) {
		return ui->listWidget->currentRow() == OBSBasicSettings::Pages::AUDIO;
	} else if (page == ui->videoPage) {
		return ui->listWidget->currentRow() == OBSBasicSettings::Pages::VIDEO;
	} else if (page == ui->hotkeyPage) {
		return ui->listWidget->currentRow() == OBSBasicSettings::Pages::HOTKEYS;
	} else if (page == ui->accessPage) {
		return ui->listWidget->currentRow() == OBSBasicSettings::Pages::ACCESSIBILITY;
	} else if (page == ui->advancedPage) {
		return ui->listWidget->currentRow() == OBSBasicSettings::Pages::ADVANCED;
	} else {
		return false;
	}
}

template<typename getPageOfSenderFunc>
static void updateAlertMessage(QList<std::tuple<QWidget *, QLabel *>> &alertMessages, int &alertMessageCount,
			       const Ui::OBSBasicSettings *ui, const Ui::SettingOutputPage *outputPage,
			       getPageOfSenderFunc getPageOfSender, const QWidget *advCurrentTab)
{
	for (auto &v : alertMessages) {
		QWidget *widget = std::get<0>(v);
		QLabel *label = std::get<1>(v);
		const QWidget *page = getPageOfSender(widget);
		if (isInCurrentPage(ui, outputPage, page, widget, advCurrentTab) && !label->text().isEmpty()) {
			++alertMessageCount;
			label->show();
			label->setProperty("calculate", true);
		} else {
			label->hide();
			label->setProperty("calculate", false);
		}
	}
}

void OBSBasicSettings::updateAlertMessage(AlertMessageType type, QWidget *widget, const QString &message, int order)
{
	auto &alertMessages = type == AlertMessageType::Error ? errorAlertMessages : warningAlertMessages;
	auto iter =
		std::find_if(alertMessages.begin(), alertMessages.end(),
			     [widget](const std::tuple<QWidget *, QLabel *> &a) { return std::get<0>(a) == widget; });
	if (iter != alertMessages.end()) {
		auto &v = *iter;
		std::get<1>(v)->setProperty("alertMessageType", type == AlertMessageType::Error ? "Error" : "Warning");
		std::get<1>(v)->setText(message);
	} else {
		QLabel *label = pls_new<QLabel>(message);
		label->setObjectName("alertMessageLabel");
		label->setWordWrap(true);
		label->setProperty("alertMessageType", type == AlertMessageType::Error ? "Error" : "Warning");
		label->hide();

		//same as before
		if (order < 0) {
			// at end
			if (type == AlertMessageType::Warning) {
				order = -1;
			} else if (!alertMessages.isEmpty()) {
				//before latest alert
				order = ui->alertMessageFrameLayout->indexOf(std::get<1>(alertMessages.last()));

			} else {
				//first
				order = 0;
			}
		}

		ui->alertMessageFrameLayout->insertWidget(order, label);
		alertMessages.append(std::make_tuple(widget, label));
	}

	updateAlertMessage();
}

void OBSBasicSettings::clearAlertMessage(AlertMessageType type, const QWidget *widget, bool update)
{
	auto &alertMessages = type == AlertMessageType::Error ? errorAlertMessages : warningAlertMessages;
	auto iter =
		std::find_if(alertMessages.begin(), alertMessages.end(),
			     [widget](const std::tuple<QWidget *, QLabel *> &a) { return std::get<0>(a) == widget; });
	if (iter != alertMessages.end()) {
		std::get<1>(*iter)->setText("");
	}

	if (update) {
		updateAlertMessage();
	}
}

void OBSBasicSettings::updateAlertMessage()
{
	int alertMessageCount = 0;
	auto getPageOfSender = [this](QWidget *widget) {
		return this->getPageOfSender(widget);
	};
	::updateAlertMessage(errorAlertMessages, alertMessageCount, ui.get(), outputPage.get(), getPageOfSender,
			     outputSettingsAdvCurrentTab);
	::updateAlertMessage(warningAlertMessages, alertMessageCount, ui.get(), outputPage.get(), getPageOfSender,
			     outputSettingsAdvCurrentTab);
	setVisibleOfErrorTips(alertMessageCount > 0);
}

QWidget *OBSBasicSettings::getPageOfSender(QObject *sender) const
{
	auto pages = pls_make_array<QWidget *>(ui->generalPage, ui->outputPage, ui->audioPage, ui->videoPage,
					       ui->accessPage, ui->hotkeyPage, ui->advancedPage);
	for (QObject *object = !sender ? this->sender() : sender; object != nullptr; object = object->parent()) {
		if (auto pos = std::find(pages.begin(), pages.end(), object); pos != pages.end()) {
			return *pos;
		}
	}
	return nullptr;
}
static int calculateDescHeight(int width, QLabel *label)
{
	auto leftPadding = 20;
	auto rightPadding = 20;
	auto topPadding = 12;
	auto bottomPadding = 12;
	QFontMetrics fontWidth(label->font());
	auto availableWidth = width - leftPadding - rightPadding;
	auto textRec = fontWidth.boundingRect(QRect(0, 0, availableWidth, 0), Qt::TextWordWrap, label->text());

	label->setFixedHeight(textRec.height() + topPadding + bottomPadding);
	return label->height();
}

void OBSBasicSettings::setVisibleOfErrorTips(bool visible)
{
	if (visible) {
		calculateErrorMsgSize();
	} else {
		ui->alertMessageLayout->setContentsMargins(0, 0, 0, 0);
	}

	if (ui->alertMessageFrame->isVisibleTo(this) == visible) {
		return;
	}

	if (visible) {
		ui->alertMessageFrame->show();
	} else {
		ui->alertMessageFrame->hide();
	}
}

void OBSBasicSettings::AdvOutStreamEncoderCheckWarnings()
{
	if (!outputStreamPage) {
		return;
	}

	clearAlertMessage(AlertMessageType::Warning, outputStreamPage->advOutEncoder, false);

	QString encoder = GetComboData(outputStreamPage->advOutEncoder);
	if (!encoder.isEmpty()) {
		const char *codec = obs_get_encoder_codec(encoder.toUtf8().constData());
		if (0 == strcmp(codec, "hevc")) {
			updateAlertMessage(AlertMessageType::Warning, outputStreamPage->advOutEncoder,
					   QTStr("Hevc.tip.vlive"));
		}
		if (0 == strcmp(codec, "av1")) {
			updateAlertMessage(AlertMessageType::Warning, outputStreamPage->advOutEncoder,
					   QTStr("Av1.tip"));
		}
	}
	updateAlertMessage();
}

void OBSBasicSettings::SimpleStreamEncoderCheckWarnings()
{
	clearAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutStrEncoder, false);
	QString encoder = GetComboData(outputSimplePage->simpleOutStrEncoder);
	if (!encoder.isEmpty()) {
		const char *id = get_simple_output_encoder(QT_TO_UTF8(encoder));
		const char *codec = obs_get_encoder_codec(id);
		if (0 == strcmp(codec, "hevc")) {
			updateAlertMessage(AlertMessageType::Warning, outputSimplePage->simpleOutStrEncoder,
					   QTStr("Hevc.tip.vlive"));
		}
	}
	updateAlertMessage();
}

void OBSBasicSettings::calculateErrorMsgSize()
{
	auto leftMargin = 20;
	auto rightMargin = 20;
	auto scrollAreaMaxHeight = 300;
	auto width = ui->settingsPages->width() - leftMargin - rightMargin;
	auto scrollHeight = 0;
	auto labels = ui->alertMessageFrame_3->findChildren<QLabel *>();
	auto visableLabelNum = 0;
	for (auto label : labels) {
		if (label->property("calculate").toBool()) {
			scrollHeight += calculateDescHeight(width, label);
			++visableLabelNum;
		}
	}
	if (scrollHeight > scrollAreaMaxHeight) {
		scrollHeight = scrollAreaMaxHeight;
	}
	auto errorMsgMargin = visableLabelNum > 1 ? 5 * (visableLabelNum - 1) : 0;
	ui->alertMessageFrame->setFixedHeight(scrollHeight + errorMsgMargin + 2);

	auto verticalBar = ui->alertMessageFrame->verticalScrollBar();
	pls_async_call_mt([this, verticalBar = pls_qobject_ptr<QScrollBar>(verticalBar)]() {
		if (pls_object_is_valid(verticalBar)) {
			if (pls::get_object(verticalBar)->isVisible()) {
				ui->alertMessageLayout->setContentsMargins(20, 0, 14, 20);
				ui->alertMessageFrame_3->setContentsMargins(0, 0, 25, 0);

			} else {
				ui->alertMessageLayout->setContentsMargins(20, 0, 19, 20);
				ui->alertMessageFrame_3->setContentsMargins(0, 0, 0, 0);
			}
		}
	});
}

void OBSBasicSettings::showNormalSetting(bool bVideoPage, bool bStreamPage)
{
	if (bVideoPage && videoPage) {
		if (-1 == videoPage->formLayout_3->indexOf(videoPage->label_11)) {
			videoPage->formLayout_3->addRow(videoPage->label_11, videoPage->downscaleFilter);
			videoPage->formLayout_3->addRow(videoPage->label, videoPage->downscaleFilterDesc);
			videoPage->formLayout_3->addRow(videoPage->fpsType, videoPage->fpsTypes);
			videoPage->formLayout_3->addRow(videoPage->spacer);
			LoadDownscaleFilters(true);
		}

		videoPage->horizontalLayoutVideoNormal->addWidget(videoPage->widget_VideoNormal);
		videoPage->stackedWidgetVideoDuaOutput->setCurrentIndex(0);
	}

	if (bStreamPage && outputStreamPage) {
		outputStreamPage->label_23->hide();
		outputStreamPage->label_25->hide();
		outputStreamPage->label_27->hide();
		outputStreamPage->advStreamTrackWidgetV->hide();

		outputStreamPage->advStreamTrackWidgetLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		outputStreamPage->verticalLayout_33->setContentsMargins(QMargins());
	}
}

void OBSBasicSettings::showDualoutputSetting(bool bVideoPage, bool bStreamPage)
{
	if (bVideoPage && videoPage) {
		videoPage->verticalLayoutVideoHorizontal->addWidget(videoPage->widget_VideoNormal);
		videoPage->tabWidgetDualOutputVideo->setCurrentIndex(0);
		videoPage->stackedWidgetVideoDuaOutput->setCurrentIndex(1);
	}

	if (bStreamPage && outputStreamPage) {
		outputStreamPage->label_23->show();
		outputStreamPage->label_25->show();
		outputStreamPage->label_27->show();
		outputStreamPage->advStreamTrackWidgetV->show();

		outputStreamPage->advStreamTrackWidgetLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
		outputStreamPage->verticalLayout_33->setContentsMargins(0, 0, 0, 7);
	}
}

