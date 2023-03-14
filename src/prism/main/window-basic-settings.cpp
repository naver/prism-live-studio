/******************************************************************************
    Copyright (C) 2013-2014 by Hugh Bailey <obs.jim@gmail.com>
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
#include <QCompleter>
#include <QGuiApplication>
#include <QLineEdit>
#include <QCloseEvent>
#include <QFileDialog>
#include <QDirIterator>
#include <QVariant>
#include <QTreeView>
#include <QScreen>
#include <QStandardItemModel>
#include <QSpacerItem>
#include <QResizeEvent>
#include <algorithm>
#include <QSignalBlocker>

#include "audio-encoders.hpp"
#include "hotkey-edit.hpp"
#include "source-label.hpp"
#include "pls-app.hpp"
#include "platform.hpp"
#include "properties-view.hpp"
#include "qt-wrappers.hpp"
#include "window-basic-main.hpp"
#include "window-basic-settings.hpp"
#include "window-basic-main-outputs.hpp"
#include "window-projector.hpp"

#include <util/platform.h>
#include "ui-config.h"
#include "pls-common-define.hpp"
#include "frontend-api.h"
#include "PLSCompleter.hpp"
#include "ChannelCommonFunctions.h"
#include "ResolutionGuidePage.h"
#include "PLSServerStreamHandler.hpp"

#define ENCODER_HIDE_FLAGS (OBS_ENCODER_CAP_DEPRECATED | OBS_ENCODER_CAP_INTERNAL)

static uint64_t g_maxRolutionSize = 0;

using namespace std;

// Used for QVariant in codec comboboxes
namespace {
static bool StringEquals(QString left, QString right)
{
	return left == right;
}
struct FormatDesc {
	const char *name = nullptr;
	const char *mimeType = nullptr;
	const ff_format_desc *desc = nullptr;

	inline FormatDesc() = default;
	inline FormatDesc(const char *name, const char *mimeType, const ff_format_desc *desc = nullptr) : name(name), mimeType(mimeType), desc(desc) {}

	bool operator==(const FormatDesc &f) const
	{
		if (!StringEquals(name, f.name))
			return false;
		return StringEquals(mimeType, f.mimeType);
	}
};
struct CodecDesc {
	const char *name = nullptr;
	int id = 0;

	inline CodecDesc() = default;
	inline CodecDesc(const char *name, int id) : name(name), id(id) {}

	bool operator==(const CodecDesc &codecDesc) const
	{
		if (id != codecDesc.id)
			return false;
		return StringEquals(name, codecDesc.name);
	}
};

class CustomPropertiesView : public PLSPropertiesView {
	PLSBasicSettings *m_basicSettings;

public:
	explicit CustomPropertiesView(PLSBasicSettings *basicSettings, PLSPropertiesView *&rview, QWidget *parent, OBSData settings, const char *type, PropertiesReloadCallback reloadCallback,
				      int minSize = 0, int maxSize = -1)
		: PLSPropertiesView(parent, settings, type, reloadCallback, minSize, maxSize, false, true, false, false), m_basicSettings(basicSettings)
	{
		rview = this;
		RefreshProperties();
	}

	void RefreshProperties()
	{
		PLSPropertiesView::RefreshProperties(
			[](QWidget *widget) {
				widget->setContentsMargins(0, 0, 0, 0);
				PLSDpiHelper::dpiDynamicUpdate(widget, false);
			},
			false);

		emit m_basicSettings->updateStreamEncoderPropsSize(this);
	}
};
bool isChild(QWidget *parent, QWidget *child)
{
	for (; child; child = child->parentWidget()) {
		if (child == parent) {
			return true;
		}
	}
	return false;
}
}

Q_DECLARE_METATYPE(FormatDesc)
Q_DECLARE_METATYPE(CodecDesc)

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

	try {
		cx = std::stoul(token.text.array);
	} catch (...) {
		return false;
	}

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

	try {
		cy = std::stoul(token.text.array);
	} catch (...) {
		return false;
	}

	//PRISM/Liu.Haibin/20200410/#None/limit resolution
	//Get max resolution limit from graphics engine,
	//While we also keep using the min resolution limit
	obs_enter_graphics();
	g_maxRolutionSize = gs_texture_get_max_size();
	obs_leave_graphics();
	if (!g_maxRolutionSize)
		g_maxRolutionSize = RESOLUTION_SIZE_MAX;
	if (cy < RESOLUTION_SIZE_MIN || cy > g_maxRolutionSize || cx < RESOLUTION_SIZE_MIN || cx > g_maxRolutionSize) {
		cx = cy = 0;
		return false;
	}

	/* shouldn't be any more tokens after this */
	if (lexer_getbasetoken(lex, &token, IGNORE_WHITESPACE))
		return false;

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
	CodecDesc codecDesc(name, id);
	for (int i = 0; i < combo->count(); i++) {
		QVariant v = combo->itemData(i);
		if (!v.isNull()) {
			if (codecDesc == v.value<CodecDesc>()) {
				return i;
				break;
			}
		}
	}
	return -1;
}

static CodecDesc GetDefaultCodecDesc(const ff_format_desc *formatDesc, ff_codec_type codecType)
{
	int id = 0;
	switch (codecType) {
	case FF_CODEC_AUDIO:
		id = ff_format_desc_audio(formatDesc);
		break;
	case FF_CODEC_VIDEO:
		id = ff_format_desc_video(formatDesc);
		break;
	default:
		return CodecDesc();
	}

	return CodecDesc(ff_format_desc_get_default_name(formatDesc, codecType), id);
}

template<typename Current, typename... Others> static void setOutputSettingsAdvStreamTabBtnSelected(Current current, Others... others)
{
	QWidget *all[] = {current, others...};
	for (size_t i = 0, count = 1 + sizeof...(others); i < count; ++i) {
		pls_flush_style(all[i], "selected", all[i] == current);
	}
};

template<typename Current, typename... Others> static void setWidgetShow(QWidget *scrollContent, Current show, Others... hides)
{
	QWidget *all[] = {hides...};
	for (size_t i = 0, count = sizeof...(hides); i < count; ++i) {
		all[i]->hide();
	}

	show->show();
	scrollContent->adjustSize();
};

template<typename Widget> static void setLabelFixedWidth(int fixedWidth, const QList<Widget *> &labels)
{
	for (QWidget *label : labels) {
		if (label) {
			label->setFixedWidth(fixedWidth);
		}
	}
}

template<typename Widget> static void setLabelNoLimitedWidth(const QList<Widget *> &labels)
{
	for (QWidget *label : labels) {
		if (label) {
			label->setMinimumWidth(0);
			label->setMaximumWidth(QWIDGETSIZE_MAX);
			label->adjustSize();
		}
	}
}

template<typename Widget> static int calcLabelFixedWidth(int maxWidth, const QList<Widget *> &labels)
{
	int fixedWidth = 0;
	for (QWidget *label : labels) {
		fixedWidth = qMin(maxWidth, qMax(fixedWidth, label ? label->width() : 0));
	}
	return fixedWidth;
}

template<typename Widget0, typename Widget1> static int calcLabelFixedWidth(int maxWidth, const QList<Widget0 *> &labels0, const QList<Widget1 *> &labels1)
{
	int fixedWidth = 0;
	for (QWidget *label : labels0) {
		fixedWidth = qMin(maxWidth, qMax(fixedWidth, label ? label->width() : 0));
	}
	for (QWidget *label : labels1) {
		fixedWidth = qMin(maxWidth, qMax(fixedWidth, label ? label->width() : 0));
	}
	return fixedWidth;
}

template<typename Widget> static void setLabelLimited(const char *page, int maxWidth, const QList<Widget *> &labels)
{
	setLabelNoLimitedWidth(labels);
	int fixedWidth = calcLabelFixedWidth(maxWidth, labels);
	PLS_DEBUG(SETTING_MODULE, "%s label flexible width: %d", page, fixedWidth);
	setLabelFixedWidth(fixedWidth, labels);
}

template<typename Widget0, typename Widget1> static void setLabelLimited(const char *page, int maxWidth, const QList<Widget0 *> &labels0, const QList<Widget1 *> &labels1)
{
	setLabelNoLimitedWidth(labels0);
	setLabelNoLimitedWidth(labels1);
	int fixedWidth = calcLabelFixedWidth(maxWidth, labels0, labels1);
	PLS_DEBUG(SETTING_MODULE, "%s label flexible width: %d", page, fixedWidth);
	setLabelFixedWidth(fixedWidth, labels0);
	setLabelFixedWidth(fixedWidth, labels1);
}

template<typename Widget0, typename Widget1> static void setLabelLimitedExclude(const char *page, int maxWidth, const QList<Widget0 *> &labels, const QList<Widget1 *> &excludes)
{
	setLabelNoLimitedWidth(labels);
	int fixedWidth = calcLabelFixedWidth(maxWidth, labels, excludes);
	PLS_DEBUG(SETTING_MODULE, "%s label flexible width: %d", page, fixedWidth);
	setLabelFixedWidth(fixedWidth, labels);
}

static void componentValueChanged(QWidget *page, QObject *sender)
{
	if (!page || !sender) {
		return;
	}

	bool hasUiStep = false;
	QString uistep, type, value;
	if (QCheckBox *checkBox = dynamic_cast<QCheckBox *>(sender); checkBox) {
		hasUiStep = true;
		type = QStringLiteral("CheckBox");
		value = checkBox->isChecked() ? "checked" : "unchecked";
	} else if (QComboBox *comboBox = dynamic_cast<QComboBox *>(sender); comboBox) {
		hasUiStep = true;
		type = QStringLiteral("ComboBox");
		if (comboBox->property("maskAddress").toBool()) {
			value = comboBox->isEditable() ? pls_masking_person_info(comboBox->lineEdit()->text()) : pls_masking_person_info(comboBox->currentText());
		} else {
			value = comboBox->isEditable() ? comboBox->lineEdit()->text() : comboBox->currentText();
		}
	} else if (dynamic_cast<QPushButton *>(sender)) {
		hasUiStep = true;
		type = QStringLiteral("Button");
	} else if (QRadioButton *radioButton = dynamic_cast<QRadioButton *>(sender); radioButton) {
		hasUiStep = true;
		type = QStringLiteral("RadioButton");
		value = radioButton->isChecked() ? "checked" : "unchecked";
	} else if (QLineEdit *lineEdit = dynamic_cast<QLineEdit *>(sender); lineEdit) {
		type = QStringLiteral("LineEdit");
		if (lineEdit->property("hidePath").toBool()) {
			value = GetFileName(lineEdit->text().toStdString()).c_str();
		} else {
			value = lineEdit->text();
		}
	} else if (QTextEdit *textEdit = dynamic_cast<QTextEdit *>(sender); textEdit) {
		type = QStringLiteral("TextEdit");
		value = textEdit->toPlainText();
	} else if (QSpinBox *spinBox = dynamic_cast<QSpinBox *>(sender); spinBox) {
		type = QStringLiteral("SpinBox");
		value = QString::number(spinBox->value());
	} else if (QDoubleSpinBox *spinBox = dynamic_cast<QDoubleSpinBox *>(sender); spinBox) {
		type = QStringLiteral("DoubleSpinBox");
		value = QString::number(spinBox->value(), 'f');
	} else if (PLSHotkeyWidget *hotkeyWidget = dynamic_cast<PLSHotkeyWidget *>(sender); hotkeyWidget) {
		uistep = QString::fromStdString(hotkeyWidget->name);
		type = QStringLiteral("HotkeyWidget");
		value = hotkeyWidget->getHotkeyText();
	}

	QString componentName;
	if (sender->dynamicPropertyNames().contains("uistep")) {
		componentName = QStringLiteral("%1 %2").arg(sender->property("uistep").toString(), type);
	} else if (uistep.isEmpty()) {
		componentName = QStringLiteral("%1 %2").arg(sender->objectName(), type);
	} else {
		componentName = QStringLiteral("%1 %2").arg(uistep, type);
	}

	for (QObject *component = sender->parent(), *end = page->parent(); component != end; component = component->parent()) {
		if (!component) {
			return;
		} else if (component->dynamicPropertyNames().contains("uistep")) {
			componentName = QStringLiteral("%1 > %2").arg(component->property("uistep").toString(), componentName);
		}
	}

	componentName = QStringLiteral("Settings > ") + componentName;
	QByteArray componentNameUtf8 = componentName.toUtf8();
	if (hasUiStep) {
		PLS_UI_STEP(SETTING_MODULE, componentNameUtf8.constData(), ACTION_CLICK);
	}
	if (!value.isEmpty()) {
		PLS_INFO(SETTING_MODULE, "%s %s", componentNameUtf8.constData(), value.toUtf8().constData());
	}
}

static void layoutRemoveWidget(QFormLayout *layout, QWidget *widget)
{
	if (widget) {
		layout->removeWidget(widget);
		widget->hide();
	}
}

#ifdef _WIN32
void PLSBasicSettings::ToggleDisableAero(bool checked)
{
	SetAeroEnabled(!checked);
}
#endif

static void PopulateAACBitrates(initializer_list<QComboBox *> boxes)
{
	auto &bitrateMap = GetAACEncoderBitrateMap();
	if (bitrateMap.empty())
		return;

	vector<pair<QString, QString>> pairs;
	for (auto &entry : bitrateMap)
		pairs.emplace_back(QString::number(entry.first), obs_encoder_get_display_name(entry.second));

	for (auto box : boxes) {
		QString currentText = box->currentText();
		box->clear();

		for (auto &pair : pairs) {
			box->addItem(pair.first);
			box->setItemData(box->count() - 1, pair.second, Qt::ToolTipRole);
		}

		box->setCurrentText(currentText);
	}
}

void RestrictResetBitrates(initializer_list<QComboBox *> boxes, int maxbitrate);

void PLSBasicSettings::HookWidget(QWidget *widget, const char *signal, const char *slot)
{
	QObject::connect(widget, signal, this, slot);
	widget->setProperty("changed", QVariant(false));
}

/* clang-format off */
#define COMBO_CHANGED   SIGNAL(currentIndexChanged(int))
#define EDIT_CHANGED    SIGNAL(textChanged(const QString &))
#define CBEDIT_CHANGED  SIGNAL(editTextChanged(const QString &))
#define CHECK_CHANGED   SIGNAL(clicked(bool))
#define SCROLL_CHANGED  SIGNAL(valueChanged(int))
#define DSCROLL_CHANGED SIGNAL(valueChanged(double))
#define TOGGLE_CHANGED  SIGNAL(toggled(bool))

#define GENERAL_CHANGED SLOT(GeneralChanged())
#define STREAM1_CHANGED SLOT(Stream1Changed())
#define OUTPUTS_CHANGED SLOT(OutputsChanged())
#define AUDIO_RESTART   SLOT(AudioChangedRestart())
#define AUDIO_CHANGED   SLOT(AudioChanged())
#define VIDEO_RESTART   SLOT(VideoChangedRestart())
#define VIDEO_RES       SLOT(VideoChangedResolution())
#define VIDEO_CHANGED   SLOT(VideoChanged())
#define ADV_CHANGED     SLOT(AdvancedChanged())
#define ADV_RESTART     SLOT(AdvancedChangedRestart())
/* clang-format on */

#define GENERAL_PAGE_FORMLABELS ui->label_21, ui->label_45

#define OUTPUT_PAGE_FORMLABELS                                                                                                                                                                   \
	ui->label_71, ui->label_68, ui->fpsType, ui->outputModeLabel, ui->label_19, ui->simpleOutRecEncoderLabel_2, ui->label_20, ui->label_24, ui->label_23, ui->label_18, ui->label_26,        \
		ui->simpleOutRecFormatLabel, ui->simpleOutRecEncoderLabel, ui->label_420, ui->label_35, ui->simpleRBMegsMaxLabel, ui->label_28, ui->advOutEncLabel, ui->widget_14, ui->label_31, \
		ui->label_32, ui->label_43, ui->label_29, ui->advOutRecEncLabel, ui->advOutRecUseRescaleContainer, ui->label_9001, ui->label_48, ui->label_36, ui->label_16, ui->label_44,       \
		ui->label_1337, ui->label_40, ui->label_63, ui->widget_15, ui->label_37, ui->label_38, ui->label_41, ui->label_47, ui->label_39, ui->label_46, ui->label_25, ui->label_55,       \
		ui->label_49, ui->label_50, ui->label_51, ui->label_52, ui->label_53, ui->label_54, ui->label_59, ui->label_60, ui->label_61, ui->advRBSecMaxLabel, ui->advRBMegsMaxLabel,       \
		ui->label_7, ui->label_57, ui->label_56, ui->label_17, ui->label_22, ui->label_27, ui->label_72, ui->label_25, ui->label_55, ui->label_81, ui->label_82, ui->label_79

#define AUDIO_PAGE_FORMLABELS \
	ui->label_14, ui->label_15, ui->label_2, ui->label_3, ui->label_4, ui->label_5, ui->label_6, ui->label_67, ui->label_65, ui->label_66, ui->label_66, ui->monitoringDeviceLabel

#define VIEW_PAGE_FORMLABELS ui->label_8, ui->label_10, ui->rendererLabel, ui->adapterLabel, ui->label_30, ui->label_33, ui->label_73, ui->label_74, ui->label_75, ui->label_64, ui->label_78

#define SOURCE_PAGE_FORMLABELS ui->label_9, ui->label_76

static const int SCENE_DISPLAY_TIPS_NUM = 3;
static const char *sceneDisplayTips[SCENE_DISPLAY_TIPS_NUM] = {"Setting.Scene.Display.Realtime.Tips", "Setting.Scene.Display.Thumbnail.Tips", "Setting.Scene.Display.Text.Tips"};

PLSBasicSettings::PLSBasicSettings(QWidget *parent, PLSDpiHelper dpiHelper) : PLSDialogView(parent, dpiHelper), main(nullptr), ui(new Ui::PLSBasicSettings)
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSBasicSettings});
	dpiHelper.setInitSize(this, {940, 700});
	dpiHelper.updateCssWithParent(this);

	main = PLSBasic::Get();
	setCloseEventCallback([this](QCloseEvent *e) { return onCloseEvent(e); });

	string path;

	EnableThreadedMessageBoxes(true);

	ui->setupUi(this->content());
	QMetaObject::connectSlotsByName(this);

	initGeneralView();
	main->EnableOutputs(false);

	InitImmersiveAudioUI();

	ui->advOutRecPath->setProperty("hidePath", true);
	ui->simpleOutputPath->setProperty("hidePath", true);
	ui->bindToIP->setProperty("maskAddress", true);

	ui->studioPortraitLayout->hide();
	ui->prevProgLabelToggle->hide();

	ui->fpsInteger->hide();
	ui->fpsDenNumWidget->hide();

	typedef void (PLSComboBox::*PLSComboBox_currentIndexChanged_int)(int);
	PLSComboBox_currentIndexChanged_int currentIndexChanged_int = &PLSComboBox::currentIndexChanged;
	connect(ui->fpsType, currentIndexChanged_int, this, [this](int index) {
		switch (index) {
		case 0:
		default:
			setWidgetShow(ui->scrollAreaWidgetContents_3, ui->fpsCommon, ui->fpsInteger, ui->fpsDenNumWidget);
			break;
		case 1:
			setWidgetShow(ui->scrollAreaWidgetContents_3, ui->fpsInteger, ui->fpsCommon, ui->fpsDenNumWidget);
			break;
		case 2:
			setWidgetShow(ui->scrollAreaWidgetContents_3, ui->fpsDenNumWidget, ui->fpsCommon, ui->fpsInteger);
			break;
		}
	});

	ui->outputSettingsAdvTabs->installEventFilter(this);
	outputSettingsAdvTabsHLine = new QWidget(ui->outputSettingsAdvTabs);
	outputSettingsAdvTabsHLine->setObjectName(QString::fromUtf8("outputSettingsAdvTabsHLine"));
	outputSettingsAdvTabsHLine->lower();

	ui->advOutputModeContainer->hide();
	connect(ui->outputMode, currentIndexChanged_int, this, [this](int index) {
		if (index == 0) {
			setWidgetShow(ui->scrollAreaWidgetContents_3, ui->simpleOutputModeContainer, ui->advOutputModeContainer);
		} else {
			setWidgetShow(ui->scrollAreaWidgetContents_3, ui->advOutputModeContainer, ui->simpleOutputModeContainer);
		}
		SimpleRecordingEncoderChanged();
		SimpleReplayBufferChanged();
		AdvOutRecCheckWarnings();
		AdvOutStreamEncoderCheckWarnings();
	});

	outputSettingsAdvCurrentTab = ui->outputSettingsAdvStreamTab;
	setOutputSettingsAdvStreamTabBtnSelected(ui->outputSettingsAdvStreamTabBtn, ui->outputSettingsAdvRecordTabBtn, ui->outputSettingsAdvAudioTabBtn, ui->outputSettingsAdvReplayBufTabBtn);
	setWidgetShow(ui->scrollAreaWidgetContents_3, ui->outputSettingsAdvStreamTab, ui->outputSettingsAdvRecordTab, ui->outputSettingsAdvAudioTab, ui->outputSettingsAdvReplayBufTab);

	connect(ui->outputSettingsAdvStreamTabBtn, &QPushButton::clicked, this, [this]() {
		outputSettingsAdvCurrentTab = ui->outputSettingsAdvStreamTab;
		setOutputSettingsAdvStreamTabBtnSelected(ui->outputSettingsAdvStreamTabBtn, ui->outputSettingsAdvRecordTabBtn, ui->outputSettingsAdvAudioTabBtn, ui->outputSettingsAdvReplayBufTabBtn);
		setWidgetShow(ui->scrollAreaWidgetContents_3, ui->outputSettingsAdvStreamTab, ui->outputSettingsAdvRecordTab, ui->outputSettingsAdvAudioTab, ui->outputSettingsAdvReplayBufTab);
		AdvOutRecCheckWarnings();
		AdvOutStreamEncoderCheckWarnings();
		componentValueChanged(getPageOfSender(ui->outputSettingsAdvStreamTabBtn), ui->outputSettingsAdvStreamTabBtn);
	});
	connect(ui->outputSettingsAdvRecordTabBtn, &QPushButton::clicked, this, [this]() {
		outputSettingsAdvCurrentTab = ui->outputSettingsAdvRecordTab;
		setOutputSettingsAdvStreamTabBtnSelected(ui->outputSettingsAdvRecordTabBtn, ui->outputSettingsAdvStreamTabBtn, ui->outputSettingsAdvAudioTabBtn, ui->outputSettingsAdvReplayBufTabBtn);
		setWidgetShow(ui->scrollAreaWidgetContents_3, ui->outputSettingsAdvRecordTab, ui->outputSettingsAdvStreamTab, ui->outputSettingsAdvAudioTab, ui->outputSettingsAdvReplayBufTab);
		AdvOutRecCheckWarnings();
		AdvOutStreamEncoderCheckWarnings();
		componentValueChanged(getPageOfSender(ui->outputSettingsAdvRecordTabBtn), ui->outputSettingsAdvRecordTabBtn);
	});
	connect(ui->outputSettingsAdvAudioTabBtn, &QPushButton::clicked, this, [this]() {
		outputSettingsAdvCurrentTab = ui->outputSettingsAdvAudioTab;
		setOutputSettingsAdvStreamTabBtnSelected(ui->outputSettingsAdvAudioTabBtn, ui->outputSettingsAdvRecordTabBtn, ui->outputSettingsAdvStreamTabBtn, ui->outputSettingsAdvReplayBufTabBtn);
		setWidgetShow(ui->scrollAreaWidgetContents_3, ui->outputSettingsAdvAudioTab, ui->outputSettingsAdvRecordTab, ui->outputSettingsAdvStreamTab, ui->outputSettingsAdvReplayBufTab);
		AdvOutRecCheckWarnings();
		AdvOutStreamEncoderCheckWarnings();
		componentValueChanged(getPageOfSender(ui->outputSettingsAdvAudioTabBtn), ui->outputSettingsAdvAudioTabBtn);
	});
	connect(ui->outputSettingsAdvReplayBufTabBtn, &QPushButton::clicked, this, [this]() {
		outputSettingsAdvCurrentTab = ui->outputSettingsAdvReplayBufTab;
		setOutputSettingsAdvStreamTabBtnSelected(ui->outputSettingsAdvReplayBufTabBtn, ui->outputSettingsAdvAudioTabBtn, ui->outputSettingsAdvRecordTabBtn, ui->outputSettingsAdvStreamTabBtn);
		setWidgetShow(ui->scrollAreaWidgetContents_3, ui->outputSettingsAdvReplayBufTab, ui->outputSettingsAdvAudioTab, ui->outputSettingsAdvRecordTab, ui->outputSettingsAdvStreamTab);
		AdvReplayBufferChanged();
		AdvOutRecCheckWarnings();
		AdvOutStreamEncoderCheckWarnings();
		componentValueChanged(getPageOfSender(ui->outputSettingsAdvReplayBufTabBtn), ui->outputSettingsAdvReplayBufTabBtn);
	});

	setWidgetShow(ui->scrollAreaWidgetContents_3, ui->advOutRecStandard, ui->advOutRecFFmpeg);
	ui->advOutRecType->removeItem(1); // remove custom ffmpeg
	connect(ui->advOutRecType, currentIndexChanged_int, this, [this](int index) {
		switch (index) {
		case 0:
		default:
			setWidgetShow(ui->scrollAreaWidgetContents_3, ui->advOutRecStandard, ui->advOutRecFFmpeg);
			break;
		case 1:
			setWidgetShow(ui->scrollAreaWidgetContents_3, ui->advOutRecFFmpeg, ui->advOutRecStandard);
			break;
		}
	});

	connect(this, &PLSBasicSettings::updateStreamEncoderPropsSize, this, [this](PLSPropertiesView *) {
		QList<QWidget *> labels{OUTPUT_PAGE_FORMLABELS};
		if (streamEncoderProps) {
			labels.append(streamEncoderProps->findChildren<QWidget *>(OBJECT_NAME_FORMLABEL));
		}
		if (recordEncoderProps) {
			labels.append(recordEncoderProps->findChildren<QWidget *>(OBJECT_NAME_FORMLABEL));
		}
		setLabelLimited("output page", PLSDpiHelper::calculate(this, 170), labels);
	});

	connect(
		this, &PLSBasicSettings::updateStreamEncoderPropsSize, this,
		[this](PLSPropertiesView *) {
			if (streamEncoderProps) {
				int minimumHeight = streamEncoderProps->QScrollArea::widget()->minimumSizeHint().height();
				streamEncoderProps->setMinimumHeight(minimumHeight);
			}
			if (recordEncoderProps) {
				int minimumHeight = recordEncoderProps->QScrollArea::widget()->minimumSizeHint().height();
				recordEncoderProps->setMinimumHeight(minimumHeight);
			}
		},
		Qt::QueuedConnection);

	dpiHelper.notifyDpiChanged(this, [=](double dpi, double /*oldDpi*/, bool firstShow) {
		ui->streamDelaySec->setMaximumWidth(PLSDpiHelper::calculate(dpi, 180));
		ui->colorSpace->setMaximumWidth(PLSDpiHelper::calculate(dpi, 185));
		ui->colorRange->setMaximumWidth(PLSDpiHelper::calculate(dpi, 185));
		ui->horizontalLayout_20->invalidate();

		if (firstShow) {
			activateWindow();
			return;
		}

		updateLabelSize(dpi);
		if (ui->alertMessageFrame->isVisible()) {
			ui->alertMessageLayout->setContentsMargins(PLSDpiHelper::calculate(dpi, QMargins(20, 0, 20, 20)));
		} else {
			ui->alertMessageLayout->setMargin(0);
		}
	});

	if (pls_is_immersive_audio()) {
		PopulateAACBitrates({ui->simpleOutputABitrate, ui->advOutTrackStereoBitrate, ui->advOutTrackImmersiveBitrate});
	} else {
		PopulateAACBitrates({ui->simpleOutputABitrate, ui->advOutTrack1Bitrate, ui->advOutTrack2Bitrate, ui->advOutTrack3Bitrate, ui->advOutTrack4Bitrate, ui->advOutTrack5Bitrate,
				     ui->advOutTrack6Bitrate});
	}
	QString text = tr("Basic.Settings.Output.ReplayBuffer.HotkeyMessage").arg(config_get_string(main->Config(), "Others", "Hotkeys.ReplayBuffer"));
	ui->replayBufferHotkeyMessage1->setText(text);
	ui->replayBufferHotkeyMessage2->setText(text);
	connect(this, &PLSBasicSettings::asyncUpdateReplayBufferHotkeyMessage, this, &PLSBasicSettings::onAsyncUpdateReplayBufferHotkeyMessage, Qt::QueuedConnection);

	layoutRemoveWidget(ui->formLayout_17, ui->replayWhileStreaming);
	layoutRemoveWidget(ui->formLayout_17, ui->keepReplayStreamStops);
	ui->formLayout_17->removeRow(1);
	ui->formLayout_17->removeRow(1);

	ui->listWidget->setAttribute(Qt::WA_MacShowFocusRect, false);

	ui->generalPage->installEventFilter(this);
	ui->outputPage->installEventFilter(this);
	ui->audioPage->installEventFilter(this);
	ui->viewPage->installEventFilter(this);
	ui->sourcePage->installEventFilter(this);
	ui->hotkeyPage->installEventFilter(this);

	connect(
		this, &PLSBasicSettings::asyncNotifyComponentValueChanged, this, [](QWidget *page, QObject *sender) { componentValueChanged(page, sender); }, Qt::QueuedConnection);

	// lineEditYellowBorder(ui->simpleOutCustom);
	// lineEditYellowBorder(ui->simpleOutMuxCustom);
	// lineEditYellowBorder(ui->advOutMuxCustom);
	// lineEditYellowBorder(ui->advOutTrack1Name);
	// lineEditYellowBorder(ui->advOutTrack2Name);
	// lineEditYellowBorder(ui->advOutTrack3Name);
	// lineEditYellowBorder(ui->advOutTrack4Name);
	// lineEditYellowBorder(ui->advOutTrack5Name);
	// lineEditYellowBorder(ui->advOutTrack6Name);
	// lineEditYellowBorder(ui->filenameFormatting);
	// lineEditYellowBorder(ui->simpleRBPrefix);
	// lineEditYellowBorder(ui->simpleRBSuffix);

	// Zhangdewen remove stream delay feature issue: 2231
	ui->groupBox_5->hide();
	// Zhangdewen remove Auto remux to mp4
	ui->formLayout_17->removeWidget(ui->autoRemux);
	ui->autoRemux->hide();

	typedef void (QSpinBox::*QSpinBox_valueChanged_int)(int);
	QSpinBox_valueChanged_int fpsNumeratorValueChanged = &QSpinBox::valueChanged;
	connect(ui->fpsNumerator, fpsNumeratorValueChanged, ui->fpsDenominator, [this](int value) {
		int curValue = ui->fpsDenominator->value();
		ui->fpsDenominator->setMaximum(value);
		if (curValue > value) {
			ui->fpsDenominator->setValue(value);
		}
	});

	dpiHelper.setDynamicContentsMargins(ui->alertMessageLayout, true);

	if (pls_is_living_or_recording()) {
		ui->resetButton->setEnabled(false);
	}

	/* clang-format off */
	HookWidget(ui->language,             COMBO_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->theme, 		     COMBO_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->enableAutoUpdates,    CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->openStatsOnStartup,   CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->warnBeforeStreamStart,CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->warnBeforeStreamStop, CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->warnBeforeRecordStop, CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->hideProjectorCursor,  CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->projectorAlwaysOnTop, CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->recordWhenStreaming,  CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->keepRecordStreamStops,CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->replayWhileStreaming, CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->keepReplayStreamStops,CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->systemTrayEnabled,    CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->systemTrayWhenStarted,CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->systemTrayAlways,     CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->saveProjectors,       CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->snappingEnabled,      CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->screenSnapping,       CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->centerSnapping,       CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->sourceSnapping,       CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->snapDistance,         DSCROLL_CHANGED,GENERAL_CHANGED);
	HookWidget(ui->overflowHide,         CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->overflowAlwaysVisible,CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->overflowSelectionHide,CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->doubleClickSwitch,    CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->studioPortraitLayout, CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->prevProgLabelToggle,  CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->multiviewMouseSwitch, CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->multiviewDrawNames,   CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->multiviewDrawAreas,   CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->multiviewLayout,      COMBO_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->checkBox,             CHECK_CHANGED,  GENERAL_CHANGED);
	HookWidget(ui->service,              COMBO_CHANGED,  STREAM1_CHANGED);
	HookWidget(ui->server,               COMBO_CHANGED,  STREAM1_CHANGED);
	HookWidget(ui->customServer,         EDIT_CHANGED,   STREAM1_CHANGED);
	HookWidget(ui->key,                  EDIT_CHANGED,   STREAM1_CHANGED);
	HookWidget(ui->bandwidthTestEnable,  CHECK_CHANGED,  STREAM1_CHANGED);
	HookWidget(ui->twitchAddonDropdown,  COMBO_CHANGED,  STREAM1_CHANGED);
	HookWidget(ui->useAuth,              CHECK_CHANGED,  STREAM1_CHANGED);
	HookWidget(ui->authUsername,         EDIT_CHANGED,   STREAM1_CHANGED);
	HookWidget(ui->authPw,               EDIT_CHANGED,   STREAM1_CHANGED);
	HookWidget(ui->outputMode,           COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->simpleOutputPath,     EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->simpleNoSpace,        CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->simpleOutRecFormat,   COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->simpleOutputVBitrate, SCROLL_CHANGED, OUTPUTS_CHANGED);
	HookWidget(ui->simpleOutStrEncoder,  COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->simpleOutputABitrate, COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->simpleOutAdvanced,    CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->simpleOutEnforce,     CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->simpleOutPreset,      COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->simpleOutCustom,      EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->simpleOutRecQuality,  COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->simpleOutRecEncoder,  COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->simpleOutMuxCustom,   EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->simpleReplayBuf,      CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->simpleRBSecMax,       SCROLL_CHANGED, OUTPUTS_CHANGED);
	HookWidget(ui->simpleRBMegsMax,      SCROLL_CHANGED, OUTPUTS_CHANGED);
	HookWidget(ui->advOutEncoder,        COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutUseRescale,     CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutRescale,        CBEDIT_CHANGED, OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack1,         CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack2,         CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack3,         CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack4,         CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack5,         CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack6,         CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutApplyService,   CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutRecType,        COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutRecPath,        EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutNoSpace,        CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutRecFormat,      COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutRecEncoder,     COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutRecUseRescale,  CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutRecRescale,     CBEDIT_CHANGED, OUTPUTS_CHANGED);
	HookWidget(ui->advOutMuxCustom,      EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutRecTrack1,      CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutRecTrack2,      CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutRecTrack3,      CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutRecTrack4,      CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutRecTrack5,      CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutRecTrack6,      CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->simpleImmersiveAudioStreaming, CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->immersiveAudioStreaming, CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->flvTrack1,            CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->flvTrack2,            CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->flvTrack3,            CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->flvTrack4,            CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->flvTrack5,            CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->flvTrack6,            CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFType,         COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFRecPath,      EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFNoSpace,      CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFURL,          EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFFormat,       COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFMCfg,         EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFVBitrate,     SCROLL_CHANGED, OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFVGOPSize,     SCROLL_CHANGED, OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFUseRescale,   CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFIgnoreCompat, CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFRescale,      CBEDIT_CHANGED, OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFVEncoder,     COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFVCfg,         EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFABitrate,     SCROLL_CHANGED, OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFTrack1,       CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFTrack2,       CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFTrack3,       CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFTrack4,       CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFTrack5,       CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFTrack6,       CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFAEncoder,     COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutFFACfg,         EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack1Bitrate,  COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack1Name,     EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack2Bitrate,  COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack2Name,     EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack3Bitrate,  COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack3Name,     EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack4Bitrate,  COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack4Name,     EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack5Bitrate,  COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack5Name,     EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack6Bitrate,  COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrack6Name,     EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrackStereoBitrate,  COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrackStereoName,     EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrackImmersiveBitrate,  COMBO_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advOutTrackImmersiveName,     EDIT_CHANGED,   OUTPUTS_CHANGED);
	HookWidget(ui->advReplayBuf,         CHECK_CHANGED,  OUTPUTS_CHANGED);
	HookWidget(ui->advRBSecMax,          SCROLL_CHANGED, OUTPUTS_CHANGED);
	HookWidget(ui->advRBMegsMax,         SCROLL_CHANGED, OUTPUTS_CHANGED);
	HookWidget(ui->channelSetup,         COMBO_CHANGED,  AUDIO_RESTART);
	HookWidget(ui->sampleRate,           COMBO_CHANGED,  AUDIO_RESTART);
	HookWidget(ui->meterDecayRate,       COMBO_CHANGED,  AUDIO_CHANGED);
	HookWidget(ui->peakMeterType,        COMBO_CHANGED,  AUDIO_CHANGED);
	HookWidget(ui->desktopAudioDevice1,  COMBO_CHANGED,  AUDIO_CHANGED);
	HookWidget(ui->desktopAudioDevice2,  COMBO_CHANGED,  AUDIO_CHANGED);
	HookWidget(ui->auxAudioDevice1,      COMBO_CHANGED,  AUDIO_CHANGED);
	HookWidget(ui->auxAudioDevice2,      COMBO_CHANGED,  AUDIO_CHANGED);
	HookWidget(ui->auxAudioDevice3,      COMBO_CHANGED,  AUDIO_CHANGED);
	HookWidget(ui->auxAudioDevice4,      COMBO_CHANGED,  AUDIO_CHANGED);
	HookWidget(ui->baseResolution,       CBEDIT_CHANGED, VIDEO_RES);
	HookWidget(ui->outputResolution,     CBEDIT_CHANGED, VIDEO_RES);
	HookWidget(ui->outputResolution_2,   CBEDIT_CHANGED, VIDEO_RES);
	HookWidget(ui->downscaleFilter,      COMBO_CHANGED,  VIDEO_CHANGED);
	HookWidget(ui->fpsType,              COMBO_CHANGED,  VIDEO_CHANGED);
	HookWidget(ui->fpsCommon,            COMBO_CHANGED,  VIDEO_CHANGED);
	HookWidget(ui->fpsInteger,           SCROLL_CHANGED, VIDEO_CHANGED);
	HookWidget(ui->fpsNumerator,         SCROLL_CHANGED, VIDEO_CHANGED);
	HookWidget(ui->fpsDenominator,       SCROLL_CHANGED, VIDEO_CHANGED);
	HookWidget(ui->renderer,             COMBO_CHANGED,  ADV_RESTART);
	HookWidget(ui->adapter,              COMBO_CHANGED,  ADV_RESTART);
	HookWidget(ui->colorFormat,          COMBO_CHANGED,  ADV_CHANGED);
	HookWidget(ui->colorSpace,           COMBO_CHANGED,  ADV_CHANGED);
	HookWidget(ui->colorRange,           COMBO_CHANGED,  ADV_CHANGED);
	HookWidget(ui->disableOSXVSync,      CHECK_CHANGED,  ADV_CHANGED);
	HookWidget(ui->resetOSXVSync,        CHECK_CHANGED,  ADV_CHANGED);
#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	HookWidget(ui->monitoringDevice,     COMBO_CHANGED,  ADV_CHANGED);
#endif
#ifdef _WIN32
	HookWidget(ui->disableAudioDucking,  CHECK_CHANGED,  ADV_CHANGED);
	HookWidget(ui->browserHWAccel,       CHECK_CHANGED,  ADV_RESTART);
#endif
	HookWidget(ui->filenameFormatting,   EDIT_CHANGED,   ADV_CHANGED);
	HookWidget(ui->overwriteIfExists,    CHECK_CHANGED,  ADV_CHANGED);
	HookWidget(ui->simpleRBPrefix,       EDIT_CHANGED,   ADV_CHANGED);
	HookWidget(ui->simpleRBSuffix,       EDIT_CHANGED,   ADV_CHANGED);
	HookWidget(ui->streamDelayEnable,    CHECK_CHANGED,  ADV_CHANGED);
	HookWidget(ui->streamDelaySec,       SCROLL_CHANGED, ADV_CHANGED);
	HookWidget(ui->streamDelayPreserve,  CHECK_CHANGED,  ADV_CHANGED);
	HookWidget(ui->reconnectEnable,      CHECK_CHANGED,  ADV_CHANGED);
	HookWidget(ui->reconnectRetryDelay,  SCROLL_CHANGED, ADV_CHANGED);
	HookWidget(ui->reconnectMaxRetries,  SCROLL_CHANGED, ADV_CHANGED);
	HookWidget(ui->processPriority,      COMBO_CHANGED,  ADV_CHANGED);
	HookWidget(ui->bindToIP,             COMBO_CHANGED,  ADV_CHANGED);
	HookWidget(ui->enableNewSocketLoop,  CHECK_CHANGED,  ADV_CHANGED);
	HookWidget(ui->enableLowLatencyMode, CHECK_CHANGED,  ADV_CHANGED);
	HookWidget(ui->hotkeyFocusType,      COMBO_CHANGED,  ADV_CHANGED);
	HookWidget(ui->autoRemux,            CHECK_CHANGED,  ADV_CHANGED);
	HookWidget(ui->dynBitrate,           CHECK_CHANGED,  ADV_CHANGED);
	/* clang-format on */

#define ADD_HOTKEY_FOCUS_TYPE(s) ui->hotkeyFocusType->addItem(QTStr("Basic.Settings.Advanced.Hotkeys." s), s)

	ADD_HOTKEY_FOCUS_TYPE("NeverDisableHotkeys");
	ADD_HOTKEY_FOCUS_TYPE("DisableHotkeysInFocus");
	ADD_HOTKEY_FOCUS_TYPE("DisableHotkeysOutOfFocus");

#undef ADD_HOTKEY_FOCUS_TYPE

	ui->simpleOutputVBitrate->setSingleStep(50);
	ui->simpleOutputVBitrate->setSuffix(" Kbps");
	ui->advOutFFVBitrate->setSingleStep(50);
	ui->advOutFFVBitrate->setSuffix(" Kbps");
	ui->advOutFFABitrate->setSuffix(" Kbps");

#if !defined(_WIN32) && !defined(__APPLE__)
	delete ui->enableAutoUpdates;
	ui->enableAutoUpdates = nullptr;
#endif

#if !defined(_WIN32) && !defined(__APPLE__) && !HAVE_PULSEAUDIO
	delete ui->audioAdvGroupBox;
	ui->audioAdvGroupBox = nullptr;
#endif

#ifdef _WIN32
	/*
	uint32_t winVer = GetWindowsVersion();
	if (winVer > 0 && winVer < 0x602) {
		toggleAero = new QCheckBox(QTStr("Basic.Settings.Video.DisableAero"), this);
		QFormLayout *videoLayout = reinterpret_cast<QFormLayout *>(ui->videoPage->layout());
		videoLayout->addRow(nullptr, toggleAero);

		HookWidget(toggleAero, CHECK_CHANGED, VIDEO_CHANGED);
		connect(toggleAero, &QAbstractButton::toggled, this, &PLSBasicSettings::ToggleDisableAero);
	}*/

#define PROCESS_PRIORITY(val)                                               \
	{                                                                   \
		"Basic.Settings.Advanced.General.ProcessPriority." val, val \
	}

	static struct ProcessPriority {
		const char *name;
		const char *val;
	} processPriorities[] = {PROCESS_PRIORITY("High"), PROCESS_PRIORITY("AboveNormal"), PROCESS_PRIORITY("Normal"), PROCESS_PRIORITY("BelowNormal"), PROCESS_PRIORITY("Idle")};
#undef PROCESS_PRIORITY

	for (ProcessPriority pri : processPriorities)
		ui->processPriority->addItem(QTStr(pri.name), pri.val);

#else
	delete ui->rendererLabel;
	delete ui->renderer;
	delete ui->adapterLabel;
	delete ui->adapter;
	delete ui->processPriorityLabel;
	delete ui->processPriority;
	delete ui->advancedGeneralGroupBox;
	delete ui->enableNewSocketLoop;
	delete ui->enableLowLatencyMode;
	delete ui->browserHWAccel;
	delete ui->sourcesGroup;
#if defined(__APPLE__) || HAVE_PULSEAUDIO
	delete ui->disableAudioDucking;
#endif
	ui->rendererLabel = nullptr;
	ui->renderer = nullptr;
	ui->adapterLabel = nullptr;
	ui->adapter = nullptr;
	ui->processPriorityLabel = nullptr;
	ui->processPriority = nullptr;
	ui->advancedGeneralGroupBox = nullptr;
	ui->enableNewSocketLoop = nullptr;
	ui->enableLowLatencyMode = nullptr;
	ui->browserHWAccel = nullptr;
	ui->sourcesGroup = nullptr;
#if defined(__APPLE__) || HAVE_PULSEAUDIO
	ui->disableAudioDucking = nullptr;
#endif
#endif

#ifndef __APPLE__
	delete ui->disableOSXVSync;
	delete ui->resetOSXVSync;
	ui->disableOSXVSync = nullptr;
	ui->resetOSXVSync = nullptr;
#endif

	connect(ui->streamDelaySec, SIGNAL(valueChanged(int)), this, SLOT(UpdateStreamDelayEstimate()));
	connect(ui->outputMode, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateStreamDelayEstimate()));
	connect(ui->simpleOutputVBitrate, SIGNAL(valueChanged(int)), this, SLOT(UpdateStreamDelayEstimate()));
	connect(ui->simpleOutputABitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateStreamDelayEstimate()));
	connect(ui->advOutTrack1Bitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateStreamDelayEstimate()));
	connect(ui->advOutTrack2Bitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateStreamDelayEstimate()));
	connect(ui->advOutTrack3Bitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateStreamDelayEstimate()));
	connect(ui->advOutTrack4Bitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateStreamDelayEstimate()));
	connect(ui->advOutTrack5Bitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateStreamDelayEstimate()));
	connect(ui->advOutTrack6Bitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateStreamDelayEstimate()));
	connect(ui->advOutTrackStereoBitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateStreamDelayEstimate()));
	connect(ui->advOutTrackImmersiveBitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateStreamDelayEstimate()));

	//Apply button disabled until change.
	EnableApplyButton(false);

	// Initialize libff library
	ff_init();

	installEventFilter(CreateShortcutFilter());

	LoadEncoderTypes();
	LoadColorRanges();
	LoadFormats();

	auto ReloadAudioSources = [](void *data, calldata_t *param) {
		auto settings = static_cast<PLSBasicSettings *>(data);
		auto source = static_cast<obs_source_t *>(calldata_ptr(param, "source"));

		if (!source)
			return;

		if (!(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO))
			return;

		QMetaObject::invokeMethod(settings, "ReloadAudioSources", Qt::QueuedConnection);
	};
	sourceCreated.Connect(obs_get_signal_handler(), "source_create", ReloadAudioSources, this);
	channelChanged.Connect(obs_get_signal_handler(), "channel_change", ReloadAudioSources, this);

	auto ReloadHotkeys = [](void *data, calldata_t *) {
		auto settings = static_cast<PLSBasicSettings *>(data);
		QMetaObject::invokeMethod(settings, "ReloadHotkeys");
	};
	hotkeyRegistered.Connect(obs_get_signal_handler(), "hotkey_register", ReloadHotkeys, this);

	auto ReloadHotkeysIgnore = [](void *data, calldata_t *param) {
		auto settings = static_cast<PLSBasicSettings *>(data);
		auto key = static_cast<obs_hotkey_t *>(calldata_ptr(param, "key"));
		QMetaObject::invokeMethod(settings, "ReloadHotkeys", Q_ARG(obs_hotkey_id, obs_hotkey_get_id(key)));
	};
	hotkeyUnregistered.Connect(obs_get_signal_handler(), "hotkey_unregister", ReloadHotkeysIgnore, this);

	FillSimpleRecordingValues();
	FillSimpleStreamingValues();
#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	FillAudioMonitoringDevices();
#endif

	connect(ui->channelSetup, SIGNAL(currentIndexChanged(int)), this, SLOT(SurroundWarning(int)));
	connect(ui->channelSetup, SIGNAL(currentIndexChanged(int)), this, SLOT(SpeakerLayoutChanged(int)));
	connect(ui->simpleOutRecQuality, SIGNAL(currentIndexChanged(int)), this, SLOT(SimpleRecordingQualityChanged()));
	connect(ui->simpleOutRecQuality, SIGNAL(currentIndexChanged(int)), this, SLOT(SimpleRecordingQualityLosslessWarning(int)));
	connect(ui->simpleOutRecFormat, SIGNAL(currentIndexChanged(int)), this, SLOT(SimpleRecordingEncoderChanged()));
	connect(ui->simpleOutStrEncoder, SIGNAL(currentIndexChanged(int)), this, SLOT(SimpleStreamingEncoderChanged()));
	connect(ui->simpleOutStrEncoder, SIGNAL(currentIndexChanged(int)), this, SLOT(SimpleRecordingEncoderChanged()));
	connect(ui->simpleOutRecEncoder, SIGNAL(currentIndexChanged(int)), this, SLOT(SimpleRecordingEncoderChanged()));
	connect(ui->simpleOutputVBitrate, SIGNAL(valueChanged(int)), this, SLOT(SimpleRecordingEncoderChanged()));
	connect(ui->simpleOutputABitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(SimpleRecordingEncoderChanged()));
	connect(ui->simpleOutAdvanced, SIGNAL(toggled(bool)), this, SLOT(SimpleRecordingEncoderChanged()));
	connect(ui->simpleOutEnforce, SIGNAL(toggled(bool)), this, SLOT(SimpleRecordingEncoderChanged()));
	connect(ui->simpleReplayBuf, SIGNAL(toggled(bool)), this, SLOT(SimpleReplayBufferChanged()));
	connect(ui->simpleOutputVBitrate, SIGNAL(valueChanged(int)), this, SLOT(SimpleReplayBufferChanged()));
	connect(ui->simpleOutputABitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(SimpleReplayBufferChanged()));
	connect(ui->simpleRBSecMax, SIGNAL(valueChanged(int)), this, SLOT(SimpleReplayBufferChanged()));
	connect(ui->advReplayBuf, SIGNAL(toggled(bool)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutRecTrack1, SIGNAL(toggled(bool)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutRecTrack2, SIGNAL(toggled(bool)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutRecTrack3, SIGNAL(toggled(bool)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutRecTrack4, SIGNAL(toggled(bool)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutRecTrack5, SIGNAL(toggled(bool)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutRecTrack6, SIGNAL(toggled(bool)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutTrack1Bitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutTrack2Bitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutTrack3Bitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutTrack4Bitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutTrack5Bitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutTrack6Bitrate, SIGNAL(currentIndexChanged(int)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutRecType, SIGNAL(currentIndexChanged(int)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advOutRecEncoder, SIGNAL(currentIndexChanged(int)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->advRBSecMax, SIGNAL(valueChanged(int)), this, SLOT(AdvReplayBufferChanged()));
	connect(ui->listWidget, SIGNAL(currentRowChanged(int)), this, SLOT(SimpleRecordingEncoderChanged()));
	connect(ui->sceneDisplayComboBox, QOverload<int>::of(&PLSComboBox::currentIndexChanged), this, [=](int index) {
		componentValueChanged(ui->viewPage, ui->sceneDisplayComboBox);

		OnSceneDisplayMethodIndexChanged(index);

		if (!loading) {
			EnableApplyButton(true);
		}
	});

	// Get Bind to IP Addresses
	obs_properties_t *ppts = obs_get_output_properties("rtmp_output");
	obs_property_t *p = obs_properties_get(ppts, "bind_ip");

	size_t count = obs_property_list_item_count(p);
	for (size_t i = 0; i < count; i++) {
		const char *name = obs_property_list_item_name(p, i);
		const char *val = obs_property_list_item_string(p, i);

		ui->bindToIP->addItem(QT_UTF8(name), val);
	}

	obs_properties_destroy(ppts);

	InitStreamPage();
	LoadSettings(false);

	// Add warning checks to advanced output recording section controls
	connect(ui->advOutRecTrack1, SIGNAL(clicked()), this, SLOT(AdvOutRecCheckWarnings()));
	connect(ui->advOutRecTrack2, SIGNAL(clicked()), this, SLOT(AdvOutRecCheckWarnings()));
	connect(ui->advOutRecTrack3, SIGNAL(clicked()), this, SLOT(AdvOutRecCheckWarnings()));
	connect(ui->advOutRecTrack4, SIGNAL(clicked()), this, SLOT(AdvOutRecCheckWarnings()));
	connect(ui->advOutRecTrack5, SIGNAL(clicked()), this, SLOT(AdvOutRecCheckWarnings()));
	connect(ui->advOutRecTrack6, SIGNAL(clicked()), this, SLOT(AdvOutRecCheckWarnings()));
	connect(ui->advOutRecFormat, SIGNAL(currentIndexChanged(int)), this, SLOT(AdvOutRecCheckWarnings()));
	connect(ui->advOutRecEncoder, SIGNAL(currentIndexChanged(int)), this, SLOT(AdvOutRecCheckWarnings()));
	AdvOutRecCheckWarnings();

	connect(ui->advOutEncoder, SIGNAL(currentIndexChanged(int)), this, SLOT(AdvOutStreamEncoderCheckWarnings()));
	AdvOutStreamEncoderCheckWarnings();

	ui->buttonBox->button(QDialogButtonBox::Apply)->setIcon(QIcon());
	ui->buttonBox->button(QDialogButtonBox::Ok)->setIcon(QIcon());
	ui->buttonBox->button(QDialogButtonBox::Cancel)->setIcon(QIcon());

	SimpleRecordingQualityChanged();

	UpdateAutomaticReplayBufferCheckboxes();

	m_isShowChangeLanguageMsg = true;
	channelIndex = ui->channelSetup->currentIndex();
	sampleRateIndex = ui->sampleRate->currentIndex();
}

PLSBasicSettings::~PLSBasicSettings()
{
	delete ui->filenameFormatting->completer();
	main->EnableOutputs(true);

	EnableThreadedMessageBoxes(false);
}

void PLSBasicSettings::SaveCombo(QComboBox *widget, const char *section, const char *value)
{
	if (WidgetChanged(widget))
		config_set_string(main->Config(), section, value, QT_TO_UTF8(widget->currentText()));
}

void PLSBasicSettings::SaveComboData(QComboBox *widget, const char *section, const char *value)
{
	if (WidgetChanged(widget)) {
		QString str = GetComboData(widget);
		config_set_string(main->Config(), section, value, QT_TO_UTF8(str));
	}
}

void PLSBasicSettings::SaveCheckBox(QAbstractButton *widget, const char *section, const char *value, bool invert)
{
	if (WidgetChanged(widget)) {
		bool checked = widget->isChecked();
		if (invert)
			checked = !checked;

		config_set_bool(main->Config(), section, value, checked);
	}
}

void PLSBasicSettings::SaveEdit(QLineEdit *widget, const char *section, const char *value)
{
	if (WidgetChanged(widget))
		config_set_string(main->Config(), section, value, QT_TO_UTF8(widget->text()));
}

void PLSBasicSettings::SaveSpinBox(QSpinBox *widget, const char *section, const char *value)
{
	if (WidgetChanged(widget))
		config_set_int(main->Config(), section, value, widget->value());
}

#define TEXT_USE_STREAM_ENC QTStr("Basic.Settings.Output.Adv.Recording.UseStreamEncoder")

void PLSBasicSettings::LoadEncoderTypes()
{
	const char *type;
	size_t idx = 0;

	ui->advOutRecEncoder->addItem(TEXT_USE_STREAM_ENC, "none");

	while (obs_enum_encoder_types(idx++, &type)) {
		const char *name = obs_encoder_get_display_name(type);
		const char *codec = obs_get_encoder_codec(type);
		uint32_t caps = obs_get_encoder_caps(type);

		if (obs_get_encoder_type(type) != OBS_ENCODER_VIDEO)
			continue;

		bool h265opened = PLSGpopData::instance()->getH265opened();
		QStringList streaming_codecs;
		streaming_codecs << "h264";
		if (h265opened)
			streaming_codecs << "hevc";

		bool is_streaming_codec = false;
		for (const auto &test_codec : streaming_codecs) {
			if (strcmp(codec, test_codec.toStdString().c_str()) == 0) {
				is_streaming_codec = true;
				break;
			}
		}
		if ((caps & ENCODER_HIDE_FLAGS) != 0)
			continue;

		QString qName = QT_UTF8(name);
		QString qType = QT_UTF8(type);

		if (is_streaming_codec)
			ui->advOutEncoder->addItem(qName, qType);
		ui->advOutRecEncoder->addItem(qName, qType);
	}
}

#define CS_PARTIAL_STR QTStr("Basic.Settings.Advanced.Video.ColorRange.Partial")
#define CS_FULL_STR QTStr("Basic.Settings.Advanced.Video.ColorRange.Full")

void PLSBasicSettings::LoadColorRanges()
{
	ui->colorRange->addItem(CS_PARTIAL_STR, "Partial");
	ui->colorRange->addItem(CS_FULL_STR, "Full");
}

#define AV_FORMAT_DEFAULT_STR QTStr("Basic.Settings.Output.Adv.FFmpeg.FormatDefault")
#define AUDIO_STR QTStr("Basic.Settings.Output.Adv.FFmpeg.FormatAudio")
#define VIDEO_STR QTStr("Basic.Settings.Output.Adv.FFmpeg.FormatVideo")

void PLSBasicSettings::LoadFormats()
{
	ui->advOutFFFormat->blockSignals(true);

	formats.reset(ff_format_supported());
	const ff_format_desc *format = formats.get();

	while (format != nullptr) {
		bool audio = ff_format_desc_has_audio(format);
		bool video = ff_format_desc_has_video(format);
		FormatDesc formatDesc(ff_format_desc_name(format), ff_format_desc_mime_type(format), format);
		if (audio || video) {
			QString itemText(ff_format_desc_name(format));
			if (audio ^ video)
				itemText += QString(" (%1)").arg(audio ? AUDIO_STR : VIDEO_STR);

			ui->advOutFFFormat->addItem(itemText, qVariantFromValue(formatDesc));
		}

		format = ff_format_desc_next(format);
	}

	ui->advOutFFFormat->model()->sort(0);

	ui->advOutFFFormat->insertItem(0, AV_FORMAT_DEFAULT_STR);

	ui->advOutFFFormat->blockSignals(false);
}

static void AddCodec(QComboBox *combo, const ff_codec_desc *codec_desc)
{
	QString itemText(ff_codec_desc_name(codec_desc));
	if (ff_codec_desc_is_alias(codec_desc))
		itemText += QString(" (%1)").arg(ff_codec_desc_base_name(codec_desc));

	CodecDesc cd(ff_codec_desc_name(codec_desc), ff_codec_desc_id(codec_desc));

	combo->addItem(itemText, qVariantFromValue(cd));
}

#define AV_ENCODER_DEFAULT_STR QTStr("Basic.Settings.Output.Adv.FFmpeg.AVEncoderDefault")

static void AddDefaultCodec(QComboBox *combo, const ff_format_desc *formatDesc, ff_codec_type codecType)
{
	CodecDesc cd = GetDefaultCodecDesc(formatDesc, codecType);

	int existingIdx = FindEncoder(combo, cd.name, cd.id);
	if (existingIdx >= 0)
		combo->removeItem(existingIdx);

	combo->addItem(QString("%1 (%2)").arg(cd.name, AV_ENCODER_DEFAULT_STR), qVariantFromValue(cd));
}

#define AV_ENCODER_DISABLE_STR QTStr("Basic.Settings.Output.Adv.FFmpeg.AVEncoderDisable")

void PLSBasicSettings::ReloadCodecs(const ff_format_desc *formatDesc)
{
	ui->advOutFFAEncoder->blockSignals(true);
	ui->advOutFFVEncoder->blockSignals(true);
	ui->advOutFFAEncoder->clear();
	ui->advOutFFVEncoder->clear();

	if (formatDesc == nullptr)
		return;

	bool ignore_compatability = ui->advOutFFIgnoreCompat->isChecked();
	PLSFFCodecDesc codecDescs(ff_codec_supported(formatDesc, ignore_compatability));

	const ff_codec_desc *codec = codecDescs.get();

	while (codec != nullptr) {
		switch (ff_codec_desc_type(codec)) {
		case FF_CODEC_AUDIO:
			AddCodec(ui->advOutFFAEncoder, codec);
			break;
		case FF_CODEC_VIDEO:
			AddCodec(ui->advOutFFVEncoder, codec);
			break;
		default:
			break;
		}

		codec = ff_codec_desc_next(codec);
	}

	if (ff_format_desc_has_audio(formatDesc))
		AddDefaultCodec(ui->advOutFFAEncoder, formatDesc, FF_CODEC_AUDIO);
	if (ff_format_desc_has_video(formatDesc))
		AddDefaultCodec(ui->advOutFFVEncoder, formatDesc, FF_CODEC_VIDEO);

	ui->advOutFFAEncoder->model()->sort(0);
	ui->advOutFFVEncoder->model()->sort(0);

	QVariant disable = qVariantFromValue(CodecDesc());

	ui->advOutFFAEncoder->insertItem(0, AV_ENCODER_DISABLE_STR, disable);
	ui->advOutFFVEncoder->insertItem(0, AV_ENCODER_DISABLE_STR, disable);

	ui->advOutFFAEncoder->blockSignals(false);
	ui->advOutFFVEncoder->blockSignals(false);
}

void PLSBasicSettings::LoadLanguageList()
{
	const char *lang = App()->GetLocale();

	ui->language->clear();

	for (const auto &locale : GetLocaleNames()) {
		int idx = ui->language->count();

		ui->language->addItem(QT_UTF8(locale.second.second.c_str()), QT_UTF8(locale.first.c_str()));

		if (locale.first == lang) {
			ui->language->setCurrentIndex(idx);
			m_currentLanguage.first = locale.first;
			m_currentLanguage.second = locale.second.second;
		}
		qDebug() << "language = " << QT_UTF8(locale.second.second.c_str()) << "------" << locale.first.c_str();
	}

	ui->language->model()->sort(0);
}

void PLSBasicSettings::LoadThemeList()
{
	/* Save theme if user presses Cancel */
	savedTheme = string(App()->GetTheme());

	ui->theme->clear();
	QSet<QString> uniqueSet;
	string themeDir;
	char userThemeDir[512];
	int ret = GetConfigPath(userThemeDir, sizeof(userThemeDir), "PRISMLiveStudio/themes/");
	GetDataFilePath("themes/", themeDir);

	/* Check user dir first. */
	if (ret > 0) {
		QDirIterator it(QString(userThemeDir), QStringList() << "*.qss", QDir::Files);
		while (it.hasNext()) {
			it.next();
			QString name = it.fileName().section(".", 0, 0);
			ui->theme->addItem(name);
			uniqueSet.insert(name);
		}
	}

	/* Check shipped themes. */
	QDirIterator uIt(QString(themeDir.c_str()), QStringList() << "*.qss", QDir::Files);
	while (uIt.hasNext()) {
		uIt.next();
		QString name = uIt.fileName().section(".", 0, 0);

		if (name == QStringLiteral(DEFAULT_THEME)) {
			name = QStringLiteral(DEFAULT_THEME " (Default)");
		}

		if (!uniqueSet.contains(name))
			ui->theme->addItem(name);
	}

	std::string themeName = App()->GetTheme();
	if (themeName == DEFAULT_THEME) {
		themeName = QT_TO_UTF8(QStringLiteral(DEFAULT_THEME " (Default)"));
	}

	int idx = ui->theme->findText(themeName.c_str());
	if (idx != -1)
		ui->theme->setCurrentIndex(idx);
}

void PLSBasicSettings::initGeneralView()
{
	ui->groupBox_15->setVisible(false);
	ui->groupBox_20->setVisible(false);
}

void PLSBasicSettings::LoadGeneralSettings()
{
	loading = true;

	LoadLanguageList();
	LoadThemeList();

#if defined(_WIN32) || defined(__APPLE__)
	bool enableAutoUpdates = config_get_bool(GetGlobalConfig(), "General", "EnableAutoUpdates");
	ui->enableAutoUpdates->setChecked(enableAutoUpdates);
#endif
	bool openStatsOnStartup = config_get_bool(main->Config(), "General", "OpenStatsOnStartup");
	ui->openStatsOnStartup->setChecked(openStatsOnStartup);

	bool recordWhenStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming");
	ui->recordWhenStreaming->setChecked(recordWhenStreaming);

	bool keepRecordStreamStops = config_get_bool(GetGlobalConfig(), "BasicWindow", "KeepRecordingWhenStreamStops");
	ui->keepRecordStreamStops->setChecked(keepRecordStreamStops);

	bool replayWhileStreaming = config_get_bool(GetGlobalConfig(), "BasicWindow", "ReplayBufferWhileStreaming");
	ui->replayWhileStreaming->setChecked(replayWhileStreaming);

	bool keepReplayStreamStops = config_get_bool(GetGlobalConfig(), "BasicWindow", "KeepReplayBufferStreamStops");
	ui->keepReplayStreamStops->setChecked(keepReplayStreamStops);

	bool systemTrayEnabled = config_get_bool(GetGlobalConfig(), "BasicWindow", "SysTrayEnabled");
	ui->systemTrayEnabled->setChecked(systemTrayEnabled);

	bool systemTrayWhenStarted = config_get_bool(GetGlobalConfig(), "BasicWindow", "SysTrayWhenStarted");
	ui->systemTrayWhenStarted->setChecked(systemTrayWhenStarted);

	bool systemTrayAlways = config_get_bool(GetGlobalConfig(), "BasicWindow", "SysTrayMinimizeToTray");
	ui->systemTrayAlways->setChecked(systemTrayAlways);

	bool saveProjectors = config_get_bool(GetGlobalConfig(), "BasicWindow", "SaveProjectors");
	ui->saveProjectors->setChecked(saveProjectors);

	bool snappingEnabled = config_get_bool(GetGlobalConfig(), "BasicWindow", "SnappingEnabled");
	ui->snappingEnabled->setChecked(snappingEnabled);

	bool screenSnapping = config_get_bool(GetGlobalConfig(), "BasicWindow", "ScreenSnapping");
	ui->screenSnapping->setChecked(screenSnapping);

	bool centerSnapping = config_get_bool(GetGlobalConfig(), "BasicWindow", "CenterSnapping");
	ui->centerSnapping->setChecked(centerSnapping);

	bool sourceSnapping = config_get_bool(GetGlobalConfig(), "BasicWindow", "SourceSnapping");
	ui->sourceSnapping->setChecked(sourceSnapping);

	double snapDistance = config_get_double(GetGlobalConfig(), "BasicWindow", "SnapDistance");
	ui->snapDistance->setValue(snapDistance);

	bool warnBeforeStreamStart = config_get_bool(GetGlobalConfig(), "BasicWindow", "WarnBeforeStartingStream");
	ui->warnBeforeStreamStart->setChecked(warnBeforeStreamStart);

	bool warnBeforeStreamStop = config_get_bool(GetGlobalConfig(), "BasicWindow", "WarnBeforeStoppingStream");
	ui->warnBeforeStreamStop->setChecked(warnBeforeStreamStop);

	bool warnBeforeRecordStop = config_get_bool(GetGlobalConfig(), "BasicWindow", "WarnBeforeStoppingRecord");
	ui->warnBeforeRecordStop->setChecked(warnBeforeRecordStop);

	bool hideProjectorCursor = config_get_bool(GetGlobalConfig(), "BasicWindow", "HideProjectorCursor");
	ui->hideProjectorCursor->setChecked(hideProjectorCursor);

	bool projectorAlwaysOnTop = config_get_bool(GetGlobalConfig(), "BasicWindow", "ProjectorAlwaysOnTop");
	ui->projectorAlwaysOnTop->setChecked(projectorAlwaysOnTop);

	bool overflowHide = config_get_bool(GetGlobalConfig(), "BasicWindow", "OverflowHidden");
	ui->overflowHide->setChecked(overflowHide);

	bool overflowAlwaysVisible = config_get_bool(GetGlobalConfig(), "BasicWindow", "OverflowAlwaysVisible");
	ui->overflowAlwaysVisible->setChecked(overflowAlwaysVisible);

	bool overflowSelectionHide = config_get_bool(GetGlobalConfig(), "BasicWindow", "OverflowSelectionHidden");
	ui->overflowSelectionHide->setChecked(overflowSelectionHide);

	bool doubleClickSwitch = config_get_bool(GetGlobalConfig(), "BasicWindow", "TransitionOnDoubleClick");
	ui->doubleClickSwitch->setChecked(doubleClickSwitch);

	bool studioPortraitLayout = config_get_bool(GetGlobalConfig(), "BasicWindow", "StudioPortraitLayout");
	ui->studioPortraitLayout->setChecked(studioPortraitLayout);

	bool prevProgLabels = config_get_bool(GetGlobalConfig(), "BasicWindow", "StudioModeLabels");
	ui->prevProgLabelToggle->setChecked(prevProgLabels);

	bool multiviewMouseSwitch = config_get_bool(GetGlobalConfig(), "BasicWindow", "MultiviewMouseSwitch");
	ui->multiviewMouseSwitch->setChecked(multiviewMouseSwitch);

	bool multiviewDrawNames = config_get_bool(GetGlobalConfig(), "BasicWindow", "MultiviewDrawNames");
	ui->multiviewDrawNames->setChecked(multiviewDrawNames);

	bool multiviewDrawAreas = config_get_bool(GetGlobalConfig(), "BasicWindow", "MultiviewDrawAreas");
	ui->multiviewDrawAreas->setChecked(multiviewDrawAreas);

	ui->multiviewLayout->clear();
	ui->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.Horizontal.Top"), static_cast<int>(MultiviewLayout::HORIZONTAL_TOP_8_SCENES));
	ui->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.Horizontal.Bottom"), static_cast<int>(MultiviewLayout::HORIZONTAL_BOTTOM_8_SCENES));
	ui->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.Vertical.Left"), static_cast<int>(MultiviewLayout::VERTICAL_LEFT_8_SCENES));
	ui->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.Vertical.Right"), static_cast<int>(MultiviewLayout::VERTICAL_RIGHT_8_SCENES));
	ui->multiviewLayout->addItem(QTStr("Basic.Settings.General.MultiviewLayout.Horizontal.Extended.Top"), static_cast<int>(MultiviewLayout::HORIZONTAL_TOP_24_SCENES));

	ui->multiviewLayout->setCurrentIndex(config_get_int(GetGlobalConfig(), "BasicWindow", "MultiviewLayout"));

	QString _watermarksTips;
	bool _isWaterMarkEnable;
	bool _isWaterMarkChecked;
	PLSServerStreamHandler::instance()->getWatermarkInfo(_watermarksTips, _isWaterMarkEnable, _isWaterMarkChecked);
	if (_isWaterMarkEnable) {
		ui->checkBox->setChecked(_isWaterMarkChecked);
	} else {
		ui->checkBox->setChecked(_isWaterMarkChecked);
	}
	ui->checkBox->setEnabled(_isWaterMarkEnable);
	ui->label_watermarkTips->setEnabled(_isWaterMarkEnable);
	ui->label_watermarkTips->setText(_watermarksTips);

	if (pls_is_living_or_recording()) {

		//add account disenable
		ui->groupBox_20->setEnabled(false);
		ui->groupBox_21->setEnabled(false);
		ui->groupBox_19->setEnabled(false);

		ui->accountView->setEnabled(false);
		ui->language->setEnabled(false);
		ui->checkBox->setEnabled(false);
	}

	loading = false;
}

void PLSBasicSettings::LoadRendererList()
{
#ifdef _WIN32
	const char *renderer = config_get_string(GetGlobalConfig(), "Video", "Renderer");

	ui->renderer->clear();
	ui->renderer->addItem(QT_UTF8("Direct3D 11"));
	if (opt_allow_opengl || strcmp(renderer, "OpenGL") == 0)
		ui->renderer->addItem(QT_UTF8("OpenGL"));

	int idx = ui->renderer->findText(QT_UTF8(renderer));
	if (idx == -1)
		idx = 0;

	// the video adapter selection is not currently implemented, hide for now
	// to avoid user confusion. was previously protected by
	// if (strcmp(renderer, "OpenGL") == 0)
	delete ui->adapter;
	delete ui->adapterLabel;
	ui->adapter = nullptr;
	ui->adapterLabel = nullptr;

	ui->renderer->setCurrentIndex(idx);
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

static const int PresetResolutionType = 2;
static const int PresetResolutionCount = 8;
static const int presetResolutions[PresetResolutionType][PresetResolutionCount][2] = {{{3840, 2160}, {2560, 1440}, {1920, 1080}, {1600, 900}, {1280, 720}, {854, 480}, {640, 360}, {426, 240}},
										      {{2160, 3840}, {1440, 2560}, {1080, 1920}, {900, 1600}, {720, 1280}, {480, 854}, {360, 640}, {240, 426}}};
static bool isPresetResolution(int width, int height, int *type, int *start)
{
	for (int i = 0; i < PresetResolutionType; ++i) {
		for (int j = 0; j < PresetResolutionCount; ++j) {
			if ((presetResolutions[i][j][0] == width) && (presetResolutions[i][j][1] == height)) {
				*type = i;
				*start = j;
				return true;
			}
		}
	}
	return false;
}

void PLSBasicSettings::ResetDownscales(uint32_t cx, uint32_t cy, bool modified)
{
	QString advRescale;
	QString advRecRescale;
	QString advFFRescale;
	QString oldOutputRes;
	string bestScale;
	int bestPixelDiff = 0x7FFFFFFF;
	uint32_t out_cx = outputCX;
	uint32_t out_cy = outputCY;

	advRescale = ui->advOutRescale->lineEdit()->text();
	advRecRescale = ui->advOutRecRescale->lineEdit()->text();
	advFFRescale = ui->advOutFFRescale->lineEdit()->text();

	ui->outputResolution->blockSignals(true);
	ui->outputResolution_2->blockSignals(true);

	ui->outputResolution->clear();
	ui->outputResolution_2->clear();
	ui->advOutRescale->clear();
	ui->advOutRecRescale->clear();
	ui->advOutFFRescale->clear();

	if (!out_cx || !out_cy) {
		out_cx = cx;
		out_cy = cy;
		oldOutputRes = ui->baseResolution->lineEdit()->text();
	} else {
		oldOutputRes = QString::number(out_cx) + "x" + QString::number(out_cy);
	}

	int type = 0, start = 0;
	if (isPresetResolution(cx, cy, &type, &start)) {
		for (; start < PresetResolutionCount; ++start) {
			uint32_t downscaleCX = presetResolutions[type][start][0];
			uint32_t downscaleCY = presetResolutions[type][start][1];
			string res = ResString(downscaleCX, downscaleCY);
			ui->outputResolution->addItem(res.c_str());
			ui->outputResolution_2->addItem(res.c_str());

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

		if (isPresetResolution(out_cx, out_cy, &type, &start)) {
			for (; start < PresetResolutionCount; ++start) {
				uint32_t outDownscaleCX = presetResolutions[type][start][0];
				uint32_t outDownscaleCY = presetResolutions[type][start][1];
				string outRes = ResString(outDownscaleCX, outDownscaleCY);
				ui->advOutRescale->addItem(outRes.c_str());
				ui->advOutRecRescale->addItem(outRes.c_str());
				ui->advOutFFRescale->addItem(outRes.c_str());
			}
		} else {
			for (size_t idx = 0; idx < numVals; idx++) {
				uint32_t outDownscaleCX = uint32_t(double(out_cx) / vals[idx]);
				uint32_t outDownscaleCY = uint32_t(double(out_cy) / vals[idx]);

				outDownscaleCX &= 0xFFFFFFFE;
				outDownscaleCY &= 0xFFFFFFFE;

				string outRes = ResString(outDownscaleCX, outDownscaleCY);
				ui->advOutRescale->addItem(outRes.c_str());
				ui->advOutRecRescale->addItem(outRes.c_str());
				ui->advOutFFRescale->addItem(outRes.c_str());
			}
		}
	} else {
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
			ui->outputResolution->addItem(res.c_str());
			ui->outputResolution_2->addItem(res.c_str());
			ui->advOutRescale->addItem(outRes.c_str());
			ui->advOutRecRescale->addItem(outRes.c_str());
			ui->advOutFFRescale->addItem(outRes.c_str());

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
	}

	string res = ResString(cx, cy);

	if ((out_cx * out_cy) > (cx * cy)) {
		RecalcOutputResPixels(res.c_str());

		ui->outputResolution->lineEdit()->setText(res.c_str());
		ui->outputResolution_2->lineEdit()->setText(res.c_str());

		ui->outputResolution->setProperty("changed", QVariant(true));
		ui->outputResolution_2->setProperty("changed", QVariant(true));
		videoChanged = modified ? true : false;
	} else {
		float baseAspect = float(cx) / float(cy);
		float outputAspect = float(out_cx) / float(out_cy);

		bool closeAspect = close_float(baseAspect, outputAspect, 0.01f);
		if (closeAspect) {
			ui->outputResolution->lineEdit()->setText(oldOutputRes);
			ui->outputResolution_2->lineEdit()->setText(oldOutputRes);
		} else {
			RecalcOutputResPixels(res.c_str());

			ui->outputResolution->lineEdit()->setText(bestScale.c_str());
			ui->outputResolution_2->lineEdit()->setText(bestScale.c_str());
		}

		if (!closeAspect) {
			ui->outputResolution->setProperty("changed", QVariant(true));
			ui->outputResolution_2->setProperty("changed", QVariant(true));
			videoChanged = modified ? true : false;
		}
	}

	ui->outputResolution->blockSignals(false);
	ui->outputResolution_2->blockSignals(false);

	if (advRescale.isEmpty())
		advRescale = res.c_str();
	if (advRecRescale.isEmpty())
		advRecRescale = res.c_str();
	if (advFFRescale.isEmpty())
		advFFRescale = res.c_str();

	ui->advOutRescale->lineEdit()->setText(advRescale);
	ui->advOutRecRescale->lineEdit()->setText(advRecRescale);
	ui->advOutFFRescale->lineEdit()->setText(advFFRescale);
}

void PLSBasicSettings::LoadDownscaleFilters()
{
	ui->downscaleFilter->clear();
	ui->downscaleFilter->addItem(QTStr("Basic.Settings.Video.DownscaleFilter.Bilinear"), QT_UTF8("bilinear"));
	ui->downscaleFilter->addItem(QTStr("Basic.Settings.Video.DownscaleFilter.Area"), QT_UTF8("area"));
	ui->downscaleFilter->addItem(QTStr("Basic.Settings.Video.DownscaleFilter.Bicubic"), QT_UTF8("bicubic"));
	ui->downscaleFilter->addItem(QTStr("Basic.Settings.Video.DownscaleFilter.Lanczos"), QT_UTF8("lanczos"));

	const char *scaleType = config_get_string(main->Config(), "Video", "ScaleType");

	if (astrcmpi(scaleType, "bilinear") == 0) {
		ui->downscaleFilter->setCurrentIndex(0);
	} else if (astrcmpi(scaleType, "lanczos") == 0) {
		ui->downscaleFilter->setCurrentIndex(3);
	} else if (astrcmpi(scaleType, "area") == 0) {
		ui->downscaleFilter->setCurrentIndex(1);
	} else {
		ui->downscaleFilter->setCurrentIndex(2);
	}
}

void PLSBasicSettings::LoadResolutionLists()
{
	uint32_t cx = config_get_uint(main->Config(), "Video", "BaseCX");
	uint32_t cy = config_get_uint(main->Config(), "Video", "BaseCY");
	uint32_t out_cx = config_get_uint(main->Config(), "Video", "OutputCX");
	uint32_t out_cy = config_get_uint(main->Config(), "Video", "OutputCY");

	ui->baseResolution->clear();

	auto addRes = [this](int cx, int cy) {
		QString res = ResString(cx, cy).c_str();
		if (ui->baseResolution->findText(res) == -1)
			ui->baseResolution->addItem(res);
	};

	for (QScreen *screen : QGuiApplication::screens()) {
		QSize as = screen->size();
		addRes(as.width(), as.height());
	}

	addRes(1920, 1080);
	addRes(1280, 720);

	string outputResString = ResString(out_cx, out_cy);

	ui->baseResolution->lineEdit()->setText(ResString(cx, cy).c_str());

	RecalcOutputResPixels(outputResString.c_str());
	ResetDownscales(cx, cy, false);

	ui->outputResolution->lineEdit()->setText(outputResString.c_str());
	ui->outputResolution_2->lineEdit()->setText(outputResString.c_str());
}

static inline void LoadFPSCommon(PLSBasic *main, Ui::PLSBasicSettings *ui)
{
	const char *val = config_get_string(main->Config(), "Video", "FPSCommon");

	int idx = ui->fpsCommon->findText(val);
	if (idx == -1)
		idx = 4;
	ui->fpsCommon->setCurrentIndex(idx);
}

static inline void LoadFPSInteger(PLSBasic *main, Ui::PLSBasicSettings *ui)
{
	uint32_t val = config_get_uint(main->Config(), "Video", "FPSInt");
	ui->fpsInteger->setValue(val);
}

static inline void LoadFPSFraction(PLSBasic *main, Ui::PLSBasicSettings *ui)
{
	uint32_t num = config_get_uint(main->Config(), "Video", "FPSNum");
	uint32_t den = config_get_uint(main->Config(), "Video", "FPSDen");

	ui->fpsNumerator->setValue(num);
	ui->fpsDenominator->setValue(den);
}

void PLSBasicSettings::LoadFPSData()
{
	LoadFPSCommon(main, ui.get());
	LoadFPSInteger(main, ui.get());
	LoadFPSFraction(main, ui.get());

	uint32_t fpsType = config_get_uint(main->Config(), "Video", "FPSType");
	if (fpsType > 2)
		fpsType = 0;

	ui->fpsType->setCurrentIndex(fpsType);
}

void PLSBasicSettings::LoadVideoSettings()
{
	loading = true;

	if (pls_is_living_or_recording()) {
		ui->groupBoxOutputResolutionFps->setEnabled(false);
		ui->groupBoxViewResolution->setEnabled(false);
		updateAlertMessage(AlertMessageType::Warning, ui->viewPage, QTStr("Basic.Settings.Video.CurrentlyActive"));
	}

	LoadResolutionLists();
	LoadFPSData();
	LoadDownscaleFilters();

#ifdef _WIN32
	if (toggleAero) {
		bool disableAero = config_get_bool(main->Config(), "Video", "DisableAero");
		toggleAero->setChecked(disableAero);

		aeroWasDisabled = disableAero;
	}
#endif

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

void PLSBasicSettings::InitImmersiveAudioUI()
{
	if (pls_is_immersive_audio()) {
		// hide Streaming -> Audio Track
		ui->label_28->hide();
		ui->widget_8->hide();

		// hide Recording -> Audio Track
		ui->label_29->hide();
		ui->advRecTrackWidget->hide();

		// hide Audio -> Track1 - Track6 and Bitrate
		ui->groupBox_22->hide();
		ui->groupBox_2->hide();
		ui->groupBox_3->hide();
		ui->groupBox_4->hide();
		ui->groupBox_9->hide();
		ui->groupBox_12->hide();

		// hide simple rate : 44.1 kHz
		ui->sampleRate->removeItem(0);

		// only add channel : Binaural (2.0)
		ui->channelSetup->clear();
		ui->channelSetup->addItem("Binaural (2.0)");
		ui->channelSetup->setCurrentText("Binaural (2.0)");
	} else {
		// hide simple/Advanced -> immersive audio
		ui->simpleImmersiveAudioStreaming->hide();
		ui->immersiveAudioStreaming->hide();

		// hide immersive audio Track and Bitrare
		ui->groupBox->hide();
		ui->groupBox_24->hide();
	}
}

void PLSBasicSettings::LoadSimpleOutputSettings()
{
	const char *path = config_get_string(main->Config(), "SimpleOutput", "FilePath");
	bool noSpace = config_get_bool(main->Config(), "SimpleOutput", "FileNameWithoutSpace");
	const char *format = config_get_string(main->Config(), "SimpleOutput", "RecFormat");
	int videoBitrate = config_get_uint(main->Config(), "SimpleOutput", "VBitrate");
	const char *streamEnc = config_get_string(main->Config(), "SimpleOutput", "StreamEncoder");
	int audioBitrate = config_get_uint(main->Config(), "SimpleOutput", "ABitrate");
	bool advanced = config_get_bool(main->Config(), "SimpleOutput", "UseAdvanced");
	bool enforceBitrate = config_get_bool(main->Config(), "SimpleOutput", "EnforceBitrate");
	const char *preset = config_get_string(main->Config(), "SimpleOutput", "Preset");
	const char *qsvPreset = config_get_string(main->Config(), "SimpleOutput", "QSVPreset");
	const char *nvPreset = config_get_string(main->Config(), "SimpleOutput", "NVENCPreset");
	const char *amdPreset = config_get_string(main->Config(), "SimpleOutput", "AMDPreset");
	const char *custom = config_get_string(main->Config(), "SimpleOutput", "x264Settings");
	const char *recQual = config_get_string(main->Config(), "SimpleOutput", "RecQuality");
	const char *recEnc = config_get_string(main->Config(), "SimpleOutput", "RecEncoder");
	const char *muxCustom = config_get_string(main->Config(), "SimpleOutput", "MuxerCustom");
	bool replayBuf = config_get_bool(main->Config(), "SimpleOutput", "RecRB");
	int rbTime = config_get_int(main->Config(), "SimpleOutput", "RecRBTime");
	int rbSize = config_get_int(main->Config(), "SimpleOutput", "RecRBSize");

	if (pls_is_immersive_audio()) {
		int trackIndex = config_get_int(main->Config(), "SimpleOutput", "TrackIndex");
		switch (trackIndex) {
		case 1: // Stereo Only
			ui->simpleImmersiveAudioStreaming->setChecked(false);
			break;
		case 2: // Stereo, Immersive
			ui->simpleImmersiveAudioStreaming->setChecked(true);
		}
	}

	curPreset = preset;
	curQSVPreset = qsvPreset;
	curNVENCPreset = nvPreset;
	curAMDPreset = amdPreset;

	audioBitrate = FindClosestAvailableAACBitrate(audioBitrate);

	ui->simpleOutputPath->setText(path);
	ui->simpleNoSpace->setChecked(noSpace);
	ui->simpleOutputVBitrate->setValue(videoBitrate);

	int idx = ui->simpleOutRecFormat->findText(format);
	ui->simpleOutRecFormat->setCurrentIndex(idx);

	const char *speakers = config_get_string(main->Config(), "Audio", "ChannelSetup");

	// restrict list of bitrates when multichannel is OFF
	if (!IsSurround(speakers))
		RestrictResetBitrates({ui->simpleOutputABitrate}, 320);

	SetComboByName(ui->simpleOutputABitrate, std::to_string(audioBitrate).c_str());

	ui->simpleOutAdvanced->setChecked(advanced);
	ui->simpleOutEnforce->setChecked(enforceBitrate);
	ui->simpleOutCustom->setText(custom);

	idx = ui->simpleOutRecQuality->findData(QString(recQual));
	if (idx == -1)
		idx = 0;
	ui->simpleOutRecQuality->setCurrentIndex(idx);

	idx = ui->simpleOutStrEncoder->findData(QString(streamEnc));
	if (idx == -1)
		idx = 0;
	ui->simpleOutStrEncoder->setCurrentIndex(idx);

	idx = ui->simpleOutRecEncoder->findData(QString(recEnc));
	if (idx == -1)
		idx = 0;
	ui->simpleOutRecEncoder->setCurrentIndex(idx);

	ui->simpleOutMuxCustom->setText(muxCustom);

	ui->simpleReplayBuf->setChecked(replayBuf);
	ui->simpleRBSecMax->setValue(rbTime);
	ui->simpleRBMegsMax->setValue(rbSize);

	SimpleStreamingEncoderChanged();
}

void PLSBasicSettings::LoadAdvOutputStreamingSettings()
{
	bool rescale = config_get_bool(main->Config(), "AdvOut", "Rescale");
	const char *rescaleRes = config_get_string(main->Config(), "AdvOut", "RescaleRes");
	int trackIndex = config_get_int(main->Config(), "AdvOut", "TrackIndex");
	int immersiveTrackIndex = config_get_int(main->Config(), "AdvOut", "ImmersiveTrackIndex");
	bool applyServiceSettings = config_get_bool(main->Config(), "AdvOut", "ApplyServiceSettings");

	ui->advOutApplyService->setChecked(applyServiceSettings);
	ui->advOutUseRescale->setChecked(rescale);
	ui->advOutRescale->setEnabled(rescale);
	ui->advOutRescale->setCurrentText(rescaleRes);

	QStringList specList = QTStr("FilenameFormatting.completer").split("\n");
	ui->filenameFormatting->setToolTip(QTStr("FilenameFormatting.TT"));
	if (PLSCompleter *completer = PLSCompleter::attachLineEdit(this, ui->filenameFormatting, specList); completer) {
		connect(completer, &PLSCompleter::activated, this, [=]() {
			advancedChanged = true;
			ui->filenameFormatting->setProperty("changed", true);
			componentValueChanged(ui->outputPage, ui->filenameFormatting);
			EnableApplyButton(true);
		});
	}

	if (!pls_is_immersive_audio()) {
		switch (trackIndex) {
		case 1:
			ui->advOutTrack1->setChecked(true);
			break;
		case 2:
			ui->advOutTrack2->setChecked(true);
			break;
		case 3:
			ui->advOutTrack3->setChecked(true);
			break;
		case 4:
			ui->advOutTrack4->setChecked(true);
			break;
		case 5:
			ui->advOutTrack5->setChecked(true);
			break;
		case 6:
			ui->advOutTrack6->setChecked(true);
			break;
		}
	} else {
		switch (immersiveTrackIndex) {
		case 1: // Stereo Only
			ui->immersiveAudioStreaming->setChecked(false);
			break;
		case 2: // Stereo, Immersive
			ui->immersiveAudioStreaming->setChecked(true);
			break;
		}
	}
}

void PLSBasicSettings::CreateEncoderPropertyView(PLSPropertiesView *&rview, QWidget *parent, const char *encoder, const char *path, bool changed)
{
	obs_data_t *settings = obs_encoder_defaults(encoder);
	PLSPropertiesView *view;

	if (path) {
		char encoderJsonPath[512];
		int ret = GetProfilePath(encoderJsonPath, sizeof(encoderJsonPath), path);
		if (ret > 0) {
			obs_data_t *data = obs_data_create_from_json_file_safe(encoderJsonPath, "bak");
			obs_data_apply(settings, data);
			obs_data_release(data);
		}
	}

	view = new CustomPropertiesView(this, rview, parent, settings, encoder, (PropertiesReloadCallback)obs_get_encoder_properties);
	view->setProperty("changed", QVariant(changed));
	QObject::connect(view, SIGNAL(Changed()), this, SLOT(OutputsChanged()));

	obs_data_release(settings);
}

void PLSBasicSettings::LoadAdvOutputStreamingEncoderProperties()
{
	const char *type = config_get_string(main->Config(), "AdvOut", "Encoder");
	delete streamEncoderProps;
	streamEncoderProps = nullptr;

	CreateEncoderPropertyView(streamEncoderProps, ui->advOutputStreamTab, type, "streamEncoder.json");
	ui->advOutputStreamTab->layout()->addWidget(streamEncoderProps);

	connect(streamEncoderProps, SIGNAL(Changed()), this, SLOT(UpdateStreamDelayEstimate()));
	connect(streamEncoderProps, SIGNAL(Changed()), this, SLOT(AdvReplayBufferChanged()));

	curAdvStreamEncoder = type;

	if (!SetComboByValue(ui->advOutEncoder, type)) {
		uint32_t caps = obs_get_encoder_caps(type);
		if ((caps & ENCODER_HIDE_FLAGS) != 0) {
			const char *name = obs_encoder_get_display_name(type);

			ui->advOutEncoder->insertItem(0, QT_UTF8(name), QT_UTF8(type));
			SetComboByValue(ui->advOutEncoder, type);
		}
	}

	UpdateStreamDelayEstimate();
}

void PLSBasicSettings::LoadAdvOutputRecordingSettings()
{
	const char *type = config_get_string(main->Config(), "AdvOut", "RecType");
	const char *format = config_get_string(main->Config(), "AdvOut", "RecFormat");
	const char *path = config_get_string(main->Config(), "AdvOut", "RecFilePath");
	bool noSpace = config_get_bool(main->Config(), "AdvOut", "RecFileNameWithoutSpace");
	bool rescale = config_get_bool(main->Config(), "AdvOut", "RecRescale");
	const char *rescaleRes = config_get_string(main->Config(), "AdvOut", "RecRescaleRes");
	const char *muxCustom = config_get_string(main->Config(), "AdvOut", "RecMuxerCustom");
	int tracks = config_get_int(main->Config(), "AdvOut", "RecTracks");
	int flvTrack = config_get_int(main->Config(), "AdvOut", "FLVTrack");

	int typeIndex = (astrcmpi(type, "FFmpeg") == 0) ? 1 : 0;
	ui->advOutRecType->setCurrentIndex(typeIndex);
	ui->advOutRecPath->setText(path);
	ui->advOutNoSpace->setChecked(noSpace);
	ui->advOutRecUseRescale->setChecked(rescale);
	ui->advOutRecRescale->setCurrentText(rescaleRes);
	ui->advOutMuxCustom->setText(muxCustom);

	int idx = ui->advOutRecFormat->findText(format);
	ui->advOutRecFormat->setCurrentIndex(idx);

	ui->advOutRecTrack1->setChecked(tracks & (1 << 0));
	ui->advOutRecTrack2->setChecked(tracks & (1 << 1));
	ui->advOutRecTrack3->setChecked(tracks & (1 << 2));
	ui->advOutRecTrack4->setChecked(tracks & (1 << 3));
	ui->advOutRecTrack5->setChecked(tracks & (1 << 4));
	ui->advOutRecTrack6->setChecked(tracks & (1 << 5));

	switch (flvTrack) {
	case 1:
		ui->flvTrack1->setChecked(true);
		break;
	case 2:
		ui->flvTrack2->setChecked(true);
		break;
	case 3:
		ui->flvTrack3->setChecked(true);
		break;
	case 4:
		ui->flvTrack4->setChecked(true);
		break;
	case 5:
		ui->flvTrack5->setChecked(true);
		break;
	case 6:
		ui->flvTrack6->setChecked(true);
		break;
	default:
		ui->flvTrack1->setChecked(true);
		break;
	}
}

void PLSBasicSettings::LoadAdvOutputRecordingEncoderProperties()
{
	const char *type = config_get_string(main->Config(), "AdvOut", "RecEncoder");

	delete recordEncoderProps;
	recordEncoderProps = nullptr;

	if (astrcmpi(type, "none") != 0) {
		CreateEncoderPropertyView(recordEncoderProps, ui->advOutRecStandard, type, "recordEncoder.json");
		ui->advOutRecStandard->layout()->addWidget(recordEncoderProps);
		connect(recordEncoderProps, SIGNAL(Changed()), this, SLOT(AdvReplayBufferChanged()));
	}

	curAdvRecordEncoder = type;

	if (!SetComboByValue(ui->advOutRecEncoder, type)) {
		uint32_t caps = obs_get_encoder_caps(type);
		if ((caps & ENCODER_HIDE_FLAGS) != 0) {
			const char *name = obs_encoder_get_display_name(type);

			ui->advOutRecEncoder->insertItem(1, QT_UTF8(name), QT_UTF8(type));
			SetComboByValue(ui->advOutRecEncoder, type);
		}
	}
}

static void SelectFormat(QComboBox *combo, const char *name, const char *mimeType)
{
	FormatDesc formatDesc(name, mimeType);

	for (int i = 0; i < combo->count(); i++) {
		QVariant v = combo->itemData(i);
		if (!v.isNull()) {
			if (formatDesc == v.value<FormatDesc>()) {
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

void PLSBasicSettings::LoadAdvOutputFFmpegSettings()
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

	ui->advOutFFType->setCurrentIndex(saveFile ? 0 : 1);
	ui->advOutFFRecPath->setText(QT_UTF8(path));
	ui->advOutFFNoSpace->setChecked(noSpace);
	ui->advOutFFURL->setText(QT_UTF8(url));
	SelectFormat(ui->advOutFFFormat, format, mimeType);
	ui->advOutFFMCfg->setText(muxCustom);
	ui->advOutFFVBitrate->setValue(videoBitrate);
	ui->advOutFFVGOPSize->setValue(gopSize);
	ui->advOutFFUseRescale->setChecked(rescale);
	ui->advOutFFIgnoreCompat->setChecked(codecCompat);
	ui->advOutFFRescale->setEnabled(rescale);
	ui->advOutFFRescale->setCurrentText(rescaleRes);
	SelectEncoder(ui->advOutFFVEncoder, vEncoder, vEncoderId);
	ui->advOutFFVCfg->setText(vEncCustom);
	ui->advOutFFABitrate->setValue(audioBitrate);
	SelectEncoder(ui->advOutFFAEncoder, aEncoder, aEncoderId);
	ui->advOutFFACfg->setText(aEncCustom);

	ui->advOutFFTrack1->setChecked(audioMixes & (1 << 0));
	ui->advOutFFTrack2->setChecked(audioMixes & (1 << 1));
	ui->advOutFFTrack3->setChecked(audioMixes & (1 << 2));
	ui->advOutFFTrack4->setChecked(audioMixes & (1 << 3));
	ui->advOutFFTrack5->setChecked(audioMixes & (1 << 4));
	ui->advOutFFTrack6->setChecked(audioMixes & (1 << 5));
}

void PLSBasicSettings::LoadAdvOutputAudioSettings()
{
	int track1Bitrate = config_get_uint(main->Config(), "AdvOut", "Track1Bitrate");
	int track2Bitrate = config_get_uint(main->Config(), "AdvOut", "Track2Bitrate");
	int track3Bitrate = config_get_uint(main->Config(), "AdvOut", "Track3Bitrate");
	int track4Bitrate = config_get_uint(main->Config(), "AdvOut", "Track4Bitrate");
	int track5Bitrate = config_get_uint(main->Config(), "AdvOut", "Track5Bitrate");
	int track6Bitrate = config_get_uint(main->Config(), "AdvOut", "Track6Bitrate");
	int trackStereoBitrate = config_get_uint(main->Config(), "AdvOut", "TrackStereoBitrate");
	int trackImmersiveBitrate = config_get_uint(main->Config(), "AdvOut", "TrackImmersiveBitrate");
	const char *name1 = config_get_string(main->Config(), "AdvOut", "Track1Name");
	const char *name2 = config_get_string(main->Config(), "AdvOut", "Track2Name");
	const char *name3 = config_get_string(main->Config(), "AdvOut", "Track3Name");
	const char *name4 = config_get_string(main->Config(), "AdvOut", "Track4Name");
	const char *name5 = config_get_string(main->Config(), "AdvOut", "Track5Name");
	const char *name6 = config_get_string(main->Config(), "AdvOut", "Track6Name");
	const char *nameStereo = config_get_string(main->Config(), "AdvOut", "TrackStereoName");
	const char *nameImmersive = config_get_string(main->Config(), "AdvOut", "TrackImmersiveName");

	track1Bitrate = FindClosestAvailableAACBitrate(track1Bitrate);
	track2Bitrate = FindClosestAvailableAACBitrate(track2Bitrate);
	track3Bitrate = FindClosestAvailableAACBitrate(track3Bitrate);
	track4Bitrate = FindClosestAvailableAACBitrate(track4Bitrate);
	track5Bitrate = FindClosestAvailableAACBitrate(track5Bitrate);
	track6Bitrate = FindClosestAvailableAACBitrate(track6Bitrate);
	trackStereoBitrate = FindClosestAvailableAACBitrate(trackStereoBitrate);
	trackImmersiveBitrate = FindClosestAvailableAACBitrate(trackImmersiveBitrate);

	// restrict list of bitrates when multichannel is OFF
	const char *speakers = config_get_string(main->Config(), "Audio", "ChannelSetup");

	if (pls_is_immersive_audio()) {
		RestrictResetBitrates({ui->advOutTrackStereoBitrate}, 384);
		if (!IsSurround(speakers)) {
			RestrictResetBitrates({ui->advOutTrackImmersiveBitrate}, 384);
		}

	} else {
		// restrict list of bitrates when multichannel is OFF
		if (!IsSurround(speakers)) {
			RestrictResetBitrates({ui->advOutTrack1Bitrate, ui->advOutTrack2Bitrate, ui->advOutTrack3Bitrate, ui->advOutTrack4Bitrate, ui->advOutTrack5Bitrate, ui->advOutTrack6Bitrate},
					      320);
		}
	}

	SetComboByName(ui->advOutTrack1Bitrate, std::to_string(track1Bitrate).c_str());
	SetComboByName(ui->advOutTrack2Bitrate, std::to_string(track2Bitrate).c_str());
	SetComboByName(ui->advOutTrack3Bitrate, std::to_string(track3Bitrate).c_str());
	SetComboByName(ui->advOutTrack4Bitrate, std::to_string(track4Bitrate).c_str());
	SetComboByName(ui->advOutTrack5Bitrate, std::to_string(track5Bitrate).c_str());
	SetComboByName(ui->advOutTrack6Bitrate, std::to_string(track6Bitrate).c_str());
	SetComboByName(ui->advOutTrackStereoBitrate, std::to_string(trackStereoBitrate).c_str());
	SetComboByName(ui->advOutTrackImmersiveBitrate, std::to_string(trackImmersiveBitrate).c_str());

	ui->advOutTrack1Name->setText(name1);
	ui->advOutTrack2Name->setText(name2);
	ui->advOutTrack3Name->setText(name3);
	ui->advOutTrack4Name->setText(name4);
	ui->advOutTrack5Name->setText(name5);
	ui->advOutTrack6Name->setText(name6);
	ui->advOutTrackStereoName->setText(nameStereo);
	ui->advOutTrackImmersiveName->setText(nameImmersive);
}

void PLSBasicSettings::LoadOutputSettings()
{
	loading = true;

	const char *mode = config_get_string(main->Config(), "Output", "Mode");

	int modeIdx = astrcmpi(mode, "Advanced") == 0 ? 1 : 0;
	ui->outputMode->setCurrentIndex(modeIdx);

	LoadSimpleOutputSettings();
	LoadAdvOutputStreamingSettings();
	LoadAdvOutputStreamingEncoderProperties();
	LoadAdvOutputRecordingSettings();
	LoadAdvOutputRecordingEncoderProperties();
	LoadAdvOutputFFmpegSettings();
	LoadAdvOutputAudioSettings();

	if (pls_is_living_or_recording()) {
		ui->outputMode->setEnabled(false);
		ui->outputModeLabel->setEnabled(false);
		ui->simpleRecordingGroupBox->setEnabled(false);
		ui->replayBufferGroupBox->setEnabled(false);
		ui->advReplayBuf->setEnabled(false);
		ui->advReplayBufferGroupBox->setEnabled(false);
		ui->advOutTopContainer->setEnabled(false);
		ui->advOutRecTopContainer->setEnabled(false);
		ui->advOutRecTypeContainer->setEnabled(false);
		ui->advOutputAudioTracksTab->setEnabled(false);
		ui->advNetworkGroupBox->setEnabled(false);
	}

	loading = false;
}

void PLSBasicSettings::SetAdvOutputFFmpegEnablement(ff_codec_type encoderType, bool enabled, bool enableEncoder)
{
	bool rescale = config_get_bool(main->Config(), "AdvOut", "FFRescale");

	switch (encoderType) {
	case FF_CODEC_VIDEO:
		ui->advOutFFVBitrate->setEnabled(enabled);
		ui->advOutFFVGOPSize->setEnabled(enabled);
		ui->advOutFFUseRescale->setEnabled(enabled);
		ui->advOutFFRescale->setEnabled(enabled && rescale);
		ui->advOutFFVEncoder->setEnabled(enabled || enableEncoder);
		ui->advOutFFVCfg->setEnabled(enabled);
		break;
	case FF_CODEC_AUDIO:
		ui->advOutFFABitrate->setEnabled(enabled);
		ui->advOutFFAEncoder->setEnabled(enabled || enableEncoder);
		ui->advOutFFACfg->setEnabled(enabled);
		ui->advOutFFTrack1->setEnabled(enabled);
		ui->advOutFFTrack2->setEnabled(enabled);
		ui->advOutFFTrack3->setEnabled(enabled);
		ui->advOutFFTrack4->setEnabled(enabled);
		ui->advOutFFTrack5->setEnabled(enabled);
		ui->advOutFFTrack6->setEnabled(enabled);
		break;
	default:
		break;
	}
}

static inline void LoadListValue(QComboBox *widget, const char *text, const char *val)
{
	widget->addItem(QT_UTF8(text), QT_UTF8(val));
}

void PLSBasicSettings::LoadListValues(QComboBox *widget, obs_property_t *prop, int index)
{
	widget->clear();

	size_t count = obs_property_list_item_count(prop);

	obs_source_t *source = obs_get_output_source(index);
	const char *deviceId = nullptr;
	obs_data_t *settings = nullptr;

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
		}
	}

	if (settings)
		obs_data_release(settings);
	if (source)
		obs_source_release(source);
}

void PLSBasicSettings::LoadAudioDevices()
{
	const char *input_id = App()->InputAudioSource();
	const char *output_id = App()->OutputAudioSource();

	obs_properties_t *input_props = obs_get_source_properties(input_id);
	obs_properties_t *output_props = obs_get_source_properties(output_id);

	if (input_props) {
		obs_property_t *inputs = obs_properties_get(input_props, "device_id");
		LoadListValues(ui->auxAudioDevice1, inputs, 3);
		LoadListValues(ui->auxAudioDevice2, inputs, 4);
		LoadListValues(ui->auxAudioDevice3, inputs, 5);
		LoadListValues(ui->auxAudioDevice4, inputs, 6);
		obs_properties_destroy(input_props);
	}

	if (output_props) {
		obs_property_t *outputs = obs_properties_get(output_props, "device_id");
		LoadListValues(ui->desktopAudioDevice1, outputs, 1);
		LoadListValues(ui->desktopAudioDevice2, outputs, 2);
		obs_properties_destroy(output_props);
	}

	if (pls_is_living_or_recording()) {
		ui->sampleRate->setEnabled(false);
		ui->channelSetup->setEnabled(false);
	}
}

#define NBSP "\xC2\xA0"

void PLSBasicSettings::LoadAudioSources()
{
	if (ui->audioSourceLayout->rowCount() > 0) {
		QLayoutItem *forDeletion = ui->audioSourceLayout->takeAt(0);
		delete forDeletion->widget();
		delete forDeletion;
	}
	auto layout = new QFormLayout();
	layout->setHorizontalSpacing(20);
	layout->setVerticalSpacing(15);
	layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	layout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	layout->setContentsMargins(0, 0, 0, 0);

	audioSourceSignals.clear();
	audioSources.clear();

	auto widget = new QWidget();
	widget->setLayout(layout);
	ui->audioSourceLayout->addRow(widget);

	const char *enablePtm = Str("Basic.Settings.Audio.EnablePushToMute");
	const char *ptmDelay = Str("Basic.Settings.Audio.PushToMuteDelay");
	const char *enablePtt = Str("Basic.Settings.Audio.EnablePushToTalk");
	const char *pttDelay = Str("Basic.Settings.Audio.PushToTalkDelay");
	auto AddSource = [&](obs_source_t *source) {
		if (!(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO))
			return true;

		auto form = new QFormLayout();
		form->setVerticalSpacing(15);
		form->setHorizontalSpacing(20);
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		form->setContentsMargins(0, 0, 0, 0);

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
			[](void *data, calldata_t *param) { QMetaObject::invokeMethod(static_cast<QObject *>(data), "setCheckedSilently", Q_ARG(bool, calldata_bool(param, "enabled"))); }, ptmCB);
		audioSourceSignals.emplace_back(
			handler, "push_to_mute_delay",
			[](void *data, calldata_t *param) { QMetaObject::invokeMethod(static_cast<QObject *>(data), "setValueSilently", Q_ARG(int, calldata_int(param, "delay"))); }, ptmSB);
		audioSourceSignals.emplace_back(
			handler, "push_to_talk_changed",
			[](void *data, calldata_t *param) { QMetaObject::invokeMethod(static_cast<QObject *>(data), "setCheckedSilently", Q_ARG(bool, calldata_bool(param, "enabled"))); }, pttCB);
		audioSourceSignals.emplace_back(
			handler, "push_to_talk_delay",
			[](void *data, calldata_t *param) { QMetaObject::invokeMethod(static_cast<QObject *>(data), "setValueSilently", Q_ARG(int, calldata_int(param, "delay"))); }, pttSB);

		audioSources.emplace_back(OBSGetWeakRef(source), ptmCB, ptmSB, pttCB, pttSB);

		auto label = new OBSSourceLabel(source);
		label->setObjectName("audioHotkeyFormLabel");
		label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		label->setProperty("useFor", "FormLabelRole");
		label->setWordWrap(true);
		label->setMinimumSize(ui->label_67->minimumSize());
		label->setMaximumSize(ui->label_67->maximumSize());
		connect(label, &OBSSourceLabel::Removed, this, &PLSBasicSettings::LoadAudioSources);
		connect(label, &OBSSourceLabel::Destroyed, this, &PLSBasicSettings::LoadAudioSources);

		layout->addRow(label, form);

		connect(ptmCB, &SilentUpdateCheckBox::clicked, [label, this]() {
			PLS_UI_STEP(SETTING_MODULE, QStringLiteral("Settings > Audio > Hotkeys > %1 > Enable Push-to-mute ComboBox").arg(label->text()).toUtf8().data(), ACTION_CLICK);
		});
		connect(pttCB, &SilentUpdateCheckBox::clicked, [label, this]() {
			PLS_UI_STEP(SETTING_MODULE, QStringLiteral("Settings > Audio > Hotkeys > %1 > Enable Push-to-talk ComboBox").arg(label->text()).toUtf8().data(), ACTION_CLICK);
		});
		return true;
	};

	using AddSource_t = decltype(AddSource);
	obs_enum_sources(
		[](void *data, obs_source_t *source) {
			auto &AddSource = *static_cast<AddSource_t *>(data);
			AddSource(source);
			return true;
		},
		static_cast<void *>(&AddSource));

	PLSDpiHelper::dpiDynamicUpdate(ui->audioHotkeysGroupBox);

	if (layout->rowCount() == 0)
		ui->audioHotkeysGroupBox->hide();
	else
		ui->audioHotkeysGroupBox->show();
}

void PLSBasicSettings::LoadAudioSettings()
{
	uint32_t sampleRate = config_get_uint(main->Config(), "Audio", "SampleRate");
	const char *speakers = config_get_string(main->Config(), "Audio", "ChannelSetup");
	double meterDecayRate = config_get_double(main->Config(), "Audio", "MeterDecayRate");
	uint32_t peakMeterTypeIdx = config_get_uint(main->Config(), "Audio", "PeakMeterType");

	loading = true;

	const char *str;
	if (pls_is_immersive_audio()) {
		str = "48 kHz";
	} else {
		if (sampleRate == 48000)
			str = "48 kHz";
		else
			str = "44.1 kHz";
	}
	int sampleRateIdx = ui->sampleRate->findText(str);
	if (sampleRateIdx != -1)
		ui->sampleRate->setCurrentIndex(sampleRateIdx);

	if (pls_is_immersive_audio()) {
		if (strcmp(speakers, "Stereo") == 0)
			ui->channelSetup->setCurrentIndex(0);
	} else {
		if (strcmp(speakers, "Mono") == 0)
			ui->channelSetup->setCurrentIndex(0);
		else if (strcmp(speakers, "2.1") == 0)
			ui->channelSetup->setCurrentIndex(2);
		else if (strcmp(speakers, "4.0") == 0)
			ui->channelSetup->setCurrentIndex(3);
		else if (strcmp(speakers, "4.1") == 0)
			ui->channelSetup->setCurrentIndex(4);
		else if (strcmp(speakers, "5.1") == 0)
			ui->channelSetup->setCurrentIndex(5);
		else if (strcmp(speakers, "7.1") == 0)
			ui->channelSetup->setCurrentIndex(6);
		else
			ui->channelSetup->setCurrentIndex(1);
	}
	if (meterDecayRate == VOLUME_METER_DECAY_MEDIUM)
		ui->meterDecayRate->setCurrentIndex(1);
	else if (meterDecayRate == VOLUME_METER_DECAY_SLOW)
		ui->meterDecayRate->setCurrentIndex(2);
	else
		ui->meterDecayRate->setCurrentIndex(0);

	ui->peakMeterType->setCurrentIndex(peakMeterTypeIdx);

	LoadAudioDevices();
	LoadAudioSources();

	loading = false;
}

void PLSBasicSettings::LoadAdvancedSettings()
{
	const char *videoColorFormat = config_get_string(main->Config(), "Video", "ColorFormat");
	const char *videoColorSpace = config_get_string(main->Config(), "Video", "ColorSpace");
	const char *videoColorRange = config_get_string(main->Config(), "Video", "ColorRange");
#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	const char *monDevName = config_get_string(main->Config(), "Audio", "MonitoringDeviceName");
	const char *monDevId = config_get_string(main->Config(), "Audio", "MonitoringDeviceId");
#endif
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
	bool replayBuf = config_get_bool(main->Config(), "AdvOut", "RecRB");
	int rbTime = config_get_int(main->Config(), "AdvOut", "RecRBTime");
	int rbSize = config_get_int(main->Config(), "AdvOut", "RecRBSize");
	bool autoRemux = config_get_bool(main->Config(), "Video", "AutoRemux");
	const char *hotkeyFocusType = config_get_string(App()->GlobalConfig(), "General", "HotkeyFocusType");
	bool dynBitrate = config_get_bool(main->Config(), "Output", "DynamicBitrate");

	loading = true;

	LoadRendererList();

#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	if (!SetComboByValue(ui->monitoringDevice, monDevId))
		SetInvalidValue(ui->monitoringDevice, monDevName, monDevId);
#endif

	ui->filenameFormatting->setText(filename);
	ui->overwriteIfExists->setChecked(overwriteIfExists);
	ui->simpleRBPrefix->setText(rbPrefix);
	ui->simpleRBSuffix->setText(rbSuffix);

	ui->advReplayBuf->setChecked(replayBuf);
	ui->advRBSecMax->setValue(rbTime);
	ui->advRBMegsMax->setValue(rbSize);

	ui->reconnectEnable->setChecked(reconnect);
	ui->reconnectRetryDelay->setValue(retryDelay);
	ui->reconnectMaxRetries->setValue(maxRetries);

	ui->streamDelaySec->setValue(delaySec);
	ui->streamDelayPreserve->setChecked(preserveDelay);
	ui->streamDelayEnable->setChecked(enableDelay);
	ui->autoRemux->setChecked(autoRemux);
	ui->dynBitrate->setChecked(dynBitrate);

	SetComboByName(ui->colorFormat, videoColorFormat);
	SetComboByName(ui->colorSpace, videoColorSpace);
	SetComboByValue(ui->colorRange, videoColorRange);

	if (!SetComboByValue(ui->bindToIP, bindIP))
		SetInvalidValue(ui->bindToIP, bindIP, bindIP);

	if (pls_is_living_or_recording()) {
		ui->advancedVideoContainer->setEnabled(false);
	}

#ifdef __APPLE__
	bool disableOSXVSync = config_get_bool(App()->GlobalConfig(), "Video", "DisableOSXVSync");
	bool resetOSXVSync = config_get_bool(App()->GlobalConfig(), "Video", "ResetOSXVSyncOnExit");
	ui->disableOSXVSync->setChecked(disableOSXVSync);
	ui->resetOSXVSync->setChecked(resetOSXVSync);
	ui->resetOSXVSync->setEnabled(disableOSXVSync);
#elif _WIN32
	bool disableAudioDucking = config_get_bool(App()->GlobalConfig(), "Audio", "DisableAudioDucking");
	ui->disableAudioDucking->setChecked(disableAudioDucking);

	const char *processPriority = config_get_string(App()->GlobalConfig(), "General", "ProcessPriority");
	bool enableNewSocketLoop = config_get_bool(main->Config(), "Output", "NewSocketLoopEnable");
	bool enableLowLatencyMode = config_get_bool(main->Config(), "Output", "LowLatencyEnable");

	int idx = ui->processPriority->findData(processPriority);
	if (idx == -1)
		idx = ui->processPriority->findData("Normal");
	ui->processPriority->setCurrentIndex(idx);

	ui->enableNewSocketLoop->setChecked(enableNewSocketLoop);
	ui->enableLowLatencyMode->setChecked(enableLowLatencyMode);
	ui->enableLowLatencyMode->setToolTip(QTStr("Basic.Settings.Advanced.Network.TCPPacing.Tooltip"));

	bool browserHWAccel = config_get_bool(App()->GlobalConfig(), "General", "BrowserHWAccel");
	ui->browserHWAccel->setChecked(browserHWAccel);
#endif

	SetComboByValue(ui->hotkeyFocusType, hotkeyFocusType);

	loading = false;
}

#define TRUNCATE_TEXT_LENGTH 80

template<typename Func> static inline void LayoutHotkey(obs_hotkey_id id, obs_hotkey_t *key, Func &&fun, const map<obs_hotkey_id, vector<obs_key_combination_t>> &keys)
{
	auto *label = new PLSHotkeyLabel;
	label->setObjectName("hotkeyHotkeyFormLabel");
	label->setWordWrap(true);
	label->setText(QT_UTF8(obs_hotkey_get_description(key)));
	label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

	PLSHotkeyWidget *hw = nullptr;

	auto combos = keys.find(id);
	if (combos == std::end(keys))
		hw = new PLSHotkeyWidget(id, obs_hotkey_get_name(key));
	else
		hw = new PLSHotkeyWidget(id, obs_hotkey_get_name(key), combos->second);

	hw->label = label;
	label->widget = hw;

	fun(key, label, hw);
}

template<typename Func, typename T> static QLabel *makeLabel(T &t, Func &&getName)
{
	return new QLabel(getName(t));
}

template<typename Func> static QLabel *makeLabel(const OBSSource &source, Func &&)
{
	OBSSourceLabel *label = new OBSSourceLabel(source);
	label->setProperty("useFor", QStringLiteral("QGroupBox"));
	label->setText(QT_UTF8(obs_source_get_name(source)));
	return label;
}

static QLabel *makeGroupBoxStart()
{
	QLabel *start = new QLabel();
	start->setFixedHeight(30);
	return start;
}

static QLabel *makeGroupBoxEnd()
{
	QLabel *start = new QLabel();
	start->setFixedHeight(0);
	return start;
}

static void layoutAddGroup(QFormLayout *layout, QWidget *start, QWidget *label, QWidget *end)
{
	if (start) {
		layout->setWidget(layout->rowCount(), QFormLayout::SpanningRole, start);
		start->show();
	}

	if (label) {
		layout->addRow(label);
		label->show();
	}

	if (end) {
		layout->setWidget(layout->rowCount(), QFormLayout::SpanningRole, end);
		end->show();
	}
}

static void layoutAddRow(QFormLayout *layout, QWidget *label, QWidget *widget)
{
	layout->addRow(label, widget);
	label->show();
	widget->show();
}

template<typename Func, typename T>
static inline void AddHotkeys(QList<std::tuple<bool, QLabel *, QWidget *, QWidget *>> &hotkeyRows, QFormLayout &layout, Func &&getName,
			      std::vector<std::tuple<T, QPointer<QLabel>, QPointer<QWidget>>> &hotkeys, PLSBasicSettings *bscSettings)
{
	if (hotkeys.empty())
		return;

	using tuple_type = std::tuple<T, QPointer<QLabel>, QPointer<QWidget>>;

	stable_sort(begin(hotkeys), end(hotkeys), [&](const tuple_type &a, const tuple_type &b) {
		const auto &o_a = get<0>(a);
		const auto &o_b = get<0>(b);
		return o_a != o_b && string(getName(o_a)) < getName(o_b);
	});

	string prevName;
	for (const auto &hotkey : hotkeys) {
		const auto &o = get<0>(hotkey);
		const char *name = getName(o);
		if (prevName != name) {
			prevName = name;
			QLabel *groupBoxStart = makeGroupBoxStart();
			layout.setWidget(layout.rowCount(), QFormLayout::SpanningRole, groupBoxStart);
			QLabel *groupBoxlabel = makeLabel(o, getName);
			groupBoxlabel->setProperty("useFor", QStringLiteral("QGroupBox"));
			groupBoxlabel->setProperty("groupName", QString(name));
			groupBoxlabel->installEventFilter(bscSettings);
			layout.addRow(groupBoxlabel);
			QLabel *groupBoxEnd = makeGroupBoxEnd();
			layout.setWidget(layout.rowCount(), QFormLayout::SpanningRole, groupBoxEnd);
			hotkeyRows.append(std::tuple<bool, QLabel *, QWidget *, QWidget *>(true, groupBoxlabel, groupBoxStart, groupBoxEnd));
		}

		auto hlabel = get<1>(hotkey);
		auto widget = get<2>(hotkey);
		widget->setProperty("uistep", name);
		layout.addRow(hlabel, widget);
		hotkeyRows.append(std::tuple<bool, QLabel *, QWidget *, QWidget *>(false, hlabel, widget, nullptr));
	}
}

void PLSBasicSettings::LoadHotkeySettings(obs_hotkey_id ignoreKey)
{
	hotkeys.clear();

	replayBufferHotkeyWidget = nullptr;
	hotkeyRows.clear();

	using keys_t = map<obs_hotkey_id, vector<obs_key_combination_t>>;
	keys_t keys;
	obs_enum_hotkey_bindings(
		[](void *data, size_t, obs_hotkey_binding_t *binding) {
			auto &keys = *static_cast<keys_t *>(data);

			keys[obs_hotkey_binding_get_hotkey_id(binding)].emplace_back(obs_hotkey_binding_get_key_combination(binding));

			return true;
		},
		&keys);

	auto layout = new QFormLayout();
	layout->setVerticalSpacing(10);
	layout->setHorizontalSpacing(20);
	layout->setContentsMargins(0, 0, 25, 50);
	layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	layout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	auto widget = new QWidget(ui->hotkeyScrollArea);
	widget->setLayout(layout);

	ui->hotkeyFocusTypeLabel->setParent(widget);
	ui->hotkeyFocusType->setParent(widget);

	layout->addRow(ui->hotkeyFocusTypeLabel, ui->hotkeyFocusType);

	hotkeyFilterLabel = new QLabel(QTStr("Basic.Settings.Hotkeys.Filter"));
	hotkeyFilterLabel->setProperty("useFor", QStringLiteral("FormLabelRole"));
	hotkeyFilterLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	auto filter = new QLineEdit();
	filter->setObjectName("hotkeyFilterLineEdit");
	// lineEditYellowBorder(filter);

	layout->addRow(hotkeyFilterLabel, filter);

	int commonCount = layout->rowCount();

	QLabel *groupBoxStart = new QLabel();
	groupBoxStart->setFixedHeight(30);
	layout->setWidget(layout->rowCount(), QFormLayout::SpanningRole, groupBoxStart);
	hotkeyRows.append(std::tuple<bool, QLabel *, QWidget *, QWidget *>(true, nullptr, groupBoxStart, nullptr));

	auto searchFunction = [=](const QString &text) {
		for (int i = 0; i < hotkeyRows.count(); ++i) {
			auto &r = hotkeyRows.at(i);
			layoutRemoveWidget(layout, std::get<1>(r));
			layoutRemoveWidget(layout, std::get<2>(r));
			layoutRemoveWidget(layout, std::get<3>(r));
		}

		while (layout->rowCount() > commonCount) {
			layout->removeRow(layout->rowCount() - 1);
		}

		for (int i = 0; i < hotkeyRows.count();) {
			auto &r1 = hotkeyRows.at(i);
			if (std::get<0>(r1)) {
				// group box
				bool groupAdd = false;
				for (i += 1; i < hotkeyRows.count();) {
					auto &r2 = hotkeyRows.at(i);
					if (std::get<0>(r2)) {
						break;
					}

					++i;
					PLSHotkeyLabel *label = dynamic_cast<PLSHotkeyLabel *>(std::get<1>(r2));
					if (label && label->text().toLower().contains(text.toLower())) {
						if (!groupAdd) {
							groupAdd = true;
							layoutAddGroup(layout, std::get<2>(r1), std::get<1>(r1), std::get<3>(r1));
						}

						layoutAddRow(layout, std::get<1>(r2), std::get<2>(r2));
					}
				}
			} else {
				++i;
				PLSHotkeyLabel *label = dynamic_cast<PLSHotkeyLabel *>(std::get<1>(r1));
				if (label && label->text().toLower().contains(text.toLower())) {
					layoutAddRow(layout, std::get<1>(r1), std::get<2>(r1));
				}
			}
		}
	};

	connect(filter, &QLineEdit::textChanged, this, searchFunction);

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
	map<obs_hotkey_id, pair<obs_hotkey_id, PLSHotkeyLabel *>> pairLabels;

	using std::move;

	auto HandleEncoder = [&](void *registerer, PLSHotkeyLabel *label, PLSHotkeyWidget *hw) {
		auto weak_encoder = static_cast<obs_weak_encoder_t *>(registerer);
		auto encoder = OBSGetStrongRef(weak_encoder);

		if (!encoder)
			return true;

		encoders.emplace_back(move(encoder), label, hw);
		return false;
	};

	auto HandleOutput = [&](void *registerer, PLSHotkeyLabel *label, PLSHotkeyWidget *hw) {
		auto weak_output = static_cast<obs_weak_output_t *>(registerer);
		auto output = OBSGetStrongRef(weak_output);

		if (!output)
			return true;

		emit asyncUpdateReplayBufferHotkeyMessage(output, hw);
		outputs.emplace_back(move(output), label, hw);
		return false;
	};

	auto HandleService = [&](void *registerer, PLSHotkeyLabel *label, PLSHotkeyWidget *hw) {
		auto weak_service = static_cast<obs_weak_service_t *>(registerer);
		auto service = OBSGetStrongRef(weak_service);

		if (!service)
			return true;

		services.emplace_back(move(service), label, hw);
		return false;
	};

	auto HandleSource = [&](void *registerer, PLSHotkeyLabel *label, PLSHotkeyWidget *hw) {
		auto weak_source = static_cast<obs_weak_source_t *>(registerer);
		auto source = OBSGetStrongRef(weak_source);

		// zhangdewen fix crash 20200916
		if (!source)
			return true;

		if (obs_source_is_private(source))
			return true;

		if (obs_scene_from_source(source))
			scenes.emplace_back(source, label, hw);
		else
			sources.emplace_back(source, label, hw);

		return false;
	};

	auto RegisterHotkey = [&](obs_hotkey_t *key, PLSHotkeyLabel *label, PLSHotkeyWidget *hw) {
		auto registerer_type = obs_hotkey_get_registerer_type(key);
		void *registerer = obs_hotkey_get_registerer(key);

		if (obs_hotkey_get_flags(key) & HOTKEY_FLAG_INVISIBLE) {
			return;
		}

		obs_hotkey_id partner = obs_hotkey_get_pair_partner_id(key);
		if (partner != OBS_INVALID_HOTKEY_ID) {
			pairLabels.emplace(obs_hotkey_get_id(key), make_pair(partner, label));
			pairIds.push_back(obs_hotkey_get_id(key));
		}

		using std::move;

		switch (registerer_type) {
		case OBS_HOTKEY_REGISTERER_FRONTEND: {
			layout->addRow(label, hw);
			hotkeyRows.append(std::tuple<bool, QLabel *, QWidget *, QWidget *>(false, label, hw, nullptr));
			break;
		}
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
			if (HandleSource(registerer, label, hw))
				return;
			break;
		}

		hotkeys.emplace_back(registerer_type == OBS_HOTKEY_REGISTERER_FRONTEND, hw);
		connect(hw, &PLSHotkeyWidget::KeyChanged, this, &PLSBasicSettings::HotkeysChanged);
		connect(hw, &PLSHotkeyWidget::clearButtonClicked, this, &PLSBasicSettings::hotkeysClearButtonClicked);
	};

	auto data = make_tuple(RegisterHotkey, std::move(keys), ignoreKey);
	using data_t = decltype(data);
	obs_enum_hotkeys(
		[](void *data, obs_hotkey_id id, obs_hotkey_t *key) {
			data_t &d = *static_cast<data_t *>(data);
			if (id != get<2>(d))
				LayoutHotkey(id, key, get<0>(d), get<1>(d));
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

		auto Update = [&](PLSHotkeyLabel *label, const QString &name, PLSHotkeyLabel *other, const QString &otherName) {
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

	AddHotkeys(hotkeyRows, *layout, obs_output_get_name, outputs, this);
	AddHotkeys(hotkeyRows, *layout, obs_source_get_name, scenes, this);
	AddHotkeys(hotkeyRows, *layout, obs_source_get_name, sources, this);
	AddHotkeys(hotkeyRows, *layout, obs_encoder_get_name, encoders, this);
	AddHotkeys(hotkeyRows, *layout, obs_service_get_name, services, this);

	PLSDpiHelper::dpiDynamicUpdate(widget, false);
	ui->hotkeyScrollArea->takeWidget()->deleteLater();
	ui->hotkeyScrollArea->setWidget(widget);
}

void PLSBasicSettings::LoadSettings(bool changedOnly)
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
	if (!changedOnly || hotkeysChanged)
		LoadHotkeySettings();
	if (!changedOnly || advancedChanged)
		LoadAdvancedSettings();
	LoadSceneDisplayMethodSettings();
}

void PLSBasicSettings::LoadSceneDisplayMethodSettings()
{
	loading = true;

	QStringList list;
	list << tr("Setting.Scene.Display.Realtime.View") << tr("Setting.Scene.Display.Thumbnail.View") << tr("Setting.Scene.Display.Text.View");
	ui->sceneDisplayComboBox->blockSignals(true);
	ui->sceneDisplayComboBox->addItems(list);
	ui->sceneDisplayComboBox->blockSignals(false);
	int currentIndex = config_get_int(GetGlobalConfig(), "BasicWindow", "SceneDisplayMethod");
	if (currentIndex > list.size() - 1 || currentIndex < 0) {
		ui->sceneDisplayComboBox->setCurrentIndex(0);

		OnSceneDisplayMethodIndexChanged(0);
		loading = false;
		return;
	}
	ui->sceneDisplayComboBox->setCurrentIndex(currentIndex);
	OnSceneDisplayMethodIndexChanged(currentIndex);

	loading = false;
}

void PLSBasicSettings::SaveGeneralSettings()
{
	int languageIndex = ui->language->currentIndex();
	QVariant langData = ui->language->itemData(languageIndex);
	string language = langData.toString().toStdString();

	if (WidgetChanged(ui->language)) {
		config_set_string(GetGlobalConfig(), "General", "Language", language.c_str());

		QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PRISM Live Studio", QSettings::NativeFormat);
		int languageID = locale2languageID(language);
		settings.setValue("InstallLanguage", languageID);
		settings.sync();
	}

	int themeIndex = ui->theme->currentIndex();
	QString themeData = ui->theme->itemText(themeIndex);
	if (themeData == QStringLiteral(DEFAULT_THEME " (Default)")) {
		themeData = QStringLiteral(DEFAULT_THEME);
	}

	if (WidgetChanged(ui->theme)) {
		config_set_string(GetGlobalConfig(), "General", "CurrentTheme", QT_TO_UTF8(themeData));

		App()->SetTheme(themeData.toUtf8().constData());
	}

#if defined(_WIN32) || defined(__APPLE__)
	if (WidgetChanged(ui->enableAutoUpdates))
		config_set_bool(GetGlobalConfig(), "General", "EnableAutoUpdates", ui->enableAutoUpdates->isChecked());
#endif
	if (WidgetChanged(ui->checkBox)) {
		bool value = ui->checkBox->isChecked();
		obs_watermark_set_enabled(value);
		config_set_bool(App()->GlobalConfig(), "General", "Watermark", value);
	}

	if (WidgetChanged(ui->openStatsOnStartup))
		config_set_bool(main->Config(), "General", "OpenStatsOnStartup", ui->openStatsOnStartup->isChecked());
	if (WidgetChanged(ui->snappingEnabled))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "SnappingEnabled", ui->snappingEnabled->isChecked());
	if (WidgetChanged(ui->screenSnapping))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "ScreenSnapping", ui->screenSnapping->isChecked());
	if (WidgetChanged(ui->centerSnapping))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "CenterSnapping", ui->centerSnapping->isChecked());
	if (WidgetChanged(ui->sourceSnapping))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "SourceSnapping", ui->sourceSnapping->isChecked());
	if (WidgetChanged(ui->snapDistance))
		config_set_double(GetGlobalConfig(), "BasicWindow", "SnapDistance", ui->snapDistance->value());
	if (WidgetChanged(ui->overflowAlwaysVisible))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "OverflowAlwaysVisible", ui->overflowAlwaysVisible->isChecked());
	if (WidgetChanged(ui->overflowHide))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "OverflowHidden", ui->overflowHide->isChecked());
	if (WidgetChanged(ui->overflowSelectionHide))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "OverflowSelectionHidden", ui->overflowSelectionHide->isChecked());
	if (WidgetChanged(ui->doubleClickSwitch))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "TransitionOnDoubleClick", ui->doubleClickSwitch->isChecked());

	config_set_bool(GetGlobalConfig(), "BasicWindow", "WarnBeforeStartingStream", ui->warnBeforeStreamStart->isChecked());
	config_set_bool(GetGlobalConfig(), "BasicWindow", "WarnBeforeStoppingStream", ui->warnBeforeStreamStop->isChecked());
	config_set_bool(GetGlobalConfig(), "BasicWindow", "WarnBeforeStoppingRecord", ui->warnBeforeRecordStop->isChecked());

	config_set_bool(GetGlobalConfig(), "BasicWindow", "HideProjectorCursor", ui->hideProjectorCursor->isChecked());
	config_set_bool(GetGlobalConfig(), "BasicWindow", "ProjectorAlwaysOnTop", ui->projectorAlwaysOnTop->isChecked());

	if (WidgetChanged(ui->recordWhenStreaming))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "RecordWhenStreaming", ui->recordWhenStreaming->isChecked());
	if (WidgetChanged(ui->keepRecordStreamStops))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "KeepRecordingWhenStreamStops", ui->keepRecordStreamStops->isChecked());

	if (WidgetChanged(ui->replayWhileStreaming))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "ReplayBufferWhileStreaming", ui->replayWhileStreaming->isChecked());
	if (WidgetChanged(ui->keepReplayStreamStops))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "KeepReplayBufferStreamStops", ui->keepReplayStreamStops->isChecked());

	if (WidgetChanged(ui->systemTrayEnabled))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "SysTrayEnabled", ui->systemTrayEnabled->isChecked());

	if (WidgetChanged(ui->systemTrayWhenStarted))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "SysTrayWhenStarted", ui->systemTrayWhenStarted->isChecked());

	if (WidgetChanged(ui->systemTrayAlways))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "SysTrayMinimizeToTray", ui->systemTrayAlways->isChecked());

	if (WidgetChanged(ui->saveProjectors))
		config_set_bool(GetGlobalConfig(), "BasicWindow", "SaveProjectors", ui->saveProjectors->isChecked());

	if (WidgetChanged(ui->studioPortraitLayout)) {
		config_set_bool(GetGlobalConfig(), "BasicWindow", "StudioPortraitLayout", ui->studioPortraitLayout->isChecked());

		main->ResetUI();
	}

	if (WidgetChanged(ui->prevProgLabelToggle)) {
		config_set_bool(GetGlobalConfig(), "BasicWindow", "StudioModeLabels", ui->prevProgLabelToggle->isChecked());

		main->ResetUI();
	}

	bool multiviewChanged = false;
	if (WidgetChanged(ui->multiviewMouseSwitch)) {
		config_set_bool(GetGlobalConfig(), "BasicWindow", "MultiviewMouseSwitch", ui->multiviewMouseSwitch->isChecked());
		multiviewChanged = true;
	}

	if (WidgetChanged(ui->multiviewDrawNames)) {
		config_set_bool(GetGlobalConfig(), "BasicWindow", "MultiviewDrawNames", ui->multiviewDrawNames->isChecked());
		multiviewChanged = true;
	}

	if (WidgetChanged(ui->multiviewDrawAreas)) {
		config_set_bool(GetGlobalConfig(), "BasicWindow", "MultiviewDrawAreas", ui->multiviewDrawAreas->isChecked());
		multiviewChanged = true;
	}

	if (WidgetChanged(ui->multiviewLayout)) {
		config_set_int(GetGlobalConfig(), "BasicWindow", "MultiviewLayout", ui->multiviewLayout->currentData().toInt());
		multiviewChanged = true;
	}

	if (multiviewChanged)
		PLSProjector::UpdateMultiviewProjectors();
}

void PLSBasicSettings::on_ToResolutionBtn_clicked()
{
	PLS_UI_STEP("Basic seeting ", " Resolution button ", " clicked ");
	ResolutionGuidePage::setVisibleOfGuide(this, [this]() { this->LoadVideoSettings(); });
}

void PLSBasicSettings::SaveVideoSettings()
{
	QString baseResolution = ui->baseResolution->currentText();
	QString outputResolution = ui->outputResolution->currentText();
	int fpsType = ui->fpsType->currentIndex();
	uint32_t cx = 0, cy = 0;

	/* ------------------- */

	if (WidgetChanged(ui->baseResolution) && ConvertResText(QT_TO_UTF8(baseResolution), cx, cy)) {
		config_set_uint(main->Config(), "Video", "BaseCX", cx);
		config_set_uint(main->Config(), "Video", "BaseCY", cy);
	}

	if (WidgetChanged(ui->outputResolution) && ConvertResText(QT_TO_UTF8(outputResolution), cx, cy)) {
		config_set_uint(main->Config(), "Video", "OutputCX", cx);
		config_set_uint(main->Config(), "Video", "OutputCY", cy);
	}

	if (WidgetChanged(ui->fpsType))
		config_set_uint(main->Config(), "Video", "FPSType", fpsType);

	SaveCombo(ui->fpsCommon, "Video", "FPSCommon");
	SaveSpinBox(ui->fpsInteger, "Video", "FPSInt");
	SaveSpinBox(ui->fpsNumerator, "Video", "FPSNum");
	SaveSpinBox(ui->fpsDenominator, "Video", "FPSDen");
	SaveComboData(ui->downscaleFilter, "Video", "ScaleType");

#ifdef _WIN32
	if (toggleAero) {
		SaveCheckBox(toggleAero, "Video", "DisableAero");
		aeroWasDisabled = toggleAero->isChecked();
	}
#endif
}

void PLSBasicSettings::SaveAdvancedSettings()
{
	QString lastMonitoringDevice = config_get_string(main->Config(), "Audio", "MonitoringDeviceId");

#ifdef _WIN32
	if (WidgetChanged(ui->renderer))
		config_set_string(App()->GlobalConfig(), "Video", "Renderer", QT_TO_UTF8(ui->renderer->currentText()));

	std::string priority = QT_TO_UTF8(ui->processPriority->currentData().toString());
	config_set_string(App()->GlobalConfig(), "General", "ProcessPriority", priority.c_str());
	if (main->Active())
		SetProcessPriority(priority.c_str());

	SaveCheckBox(ui->enableNewSocketLoop, "Output", "NewSocketLoopEnable");
	SaveCheckBox(ui->enableLowLatencyMode, "Output", "LowLatencyEnable");

	bool browserHWAccel = ui->browserHWAccel->isChecked();
	config_set_bool(App()->GlobalConfig(), "General", "BrowserHWAccel", browserHWAccel);
#endif

	if (WidgetChanged(ui->hotkeyFocusType)) {
		QString str = GetComboData(ui->hotkeyFocusType);
		config_set_string(App()->GlobalConfig(), "General", "HotkeyFocusType", QT_TO_UTF8(str));
	}

#ifdef __APPLE__
	if (WidgetChanged(ui->disableOSXVSync)) {
		bool disable = ui->disableOSXVSync->isChecked();
		config_set_bool(App()->GlobalConfig(), "Video", "DisableOSXVSync", disable);
		EnableOSXVSync(!disable);
	}
	if (WidgetChanged(ui->resetOSXVSync))
		config_set_bool(App()->GlobalConfig(), "Video", "ResetOSXVSyncOnExit", ui->resetOSXVSync->isChecked());
#endif

	SaveCombo(ui->colorFormat, "Video", "ColorFormat");
	SaveCombo(ui->colorSpace, "Video", "ColorSpace");
	SaveComboData(ui->colorRange, "Video", "ColorRange");
#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	SaveCombo(ui->monitoringDevice, "Audio", "MonitoringDeviceName");
	SaveComboData(ui->monitoringDevice, "Audio", "MonitoringDeviceId");
#endif

#ifdef _WIN32
	if (WidgetChanged(ui->disableAudioDucking)) {
		bool disable = ui->disableAudioDucking->isChecked();
		config_set_bool(App()->GlobalConfig(), "Audio", "DisableAudioDucking", disable);
		DisableAudioDucking(disable);
	}
#endif

	SaveEdit(ui->filenameFormatting, "Output", "FilenameFormatting");
	SaveEdit(ui->simpleRBPrefix, "SimpleOutput", "RecRBPrefix");
	SaveEdit(ui->simpleRBSuffix, "SimpleOutput", "RecRBSuffix");
	SaveCheckBox(ui->overwriteIfExists, "Output", "OverwriteIfExists");
	SaveCheckBox(ui->streamDelayEnable, "Output", "DelayEnable");
	SaveSpinBox(ui->streamDelaySec, "Output", "DelaySec");
	SaveCheckBox(ui->streamDelayPreserve, "Output", "DelayPreserve");
	SaveCheckBox(ui->reconnectEnable, "Output", "Reconnect");
	SaveSpinBox(ui->reconnectRetryDelay, "Output", "RetryDelay");
	SaveSpinBox(ui->reconnectMaxRetries, "Output", "MaxRetries");
	SaveComboData(ui->bindToIP, "Output", "BindIP");
	SaveCheckBox(ui->autoRemux, "Video", "AutoRemux");
	SaveCheckBox(ui->dynBitrate, "Output", "DynamicBitrate");

#if defined(_WIN32) || defined(__APPLE__) || HAVE_PULSEAUDIO
	QString newDevice = ui->monitoringDevice->currentData().toString();

	if (lastMonitoringDevice != newDevice) {
		obs_set_audio_monitoring_device(QT_TO_UTF8(ui->monitoringDevice->currentText()), QT_TO_UTF8(newDevice));

		PLS_INFO(SETTING_MODULE, "Audio monitoring device:\n\tname: %s\n\tid: %s", QT_TO_UTF8(ui->monitoringDevice->currentText()), QT_TO_UTF8(newDevice));
	}
#endif
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

static void WriteJsonData(PLSPropertiesView *view, const char *path)
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

static void removeJsonData(const char *path)
{
	char full_path[512];
	int ret = GetProfilePath(full_path, sizeof(full_path), path);
	if (ret > 0) {
		QFile::remove(full_path);
	}
}

static void SaveTrackIndex(config_t *config, const char *section, const char *name, QAbstractButton *check1, QAbstractButton *check2, QAbstractButton *check3, QAbstractButton *check4,
			   QAbstractButton *check5, QAbstractButton *check6)
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

void PLSBasicSettings::SaveFormat(QComboBox *combo)
{
	QVariant v = combo->currentData();
	if (!v.isNull()) {
		FormatDesc desc = v.value<FormatDesc>();
		config_set_string(main->Config(), "AdvOut", "FFFormat", desc.name);
		config_set_string(main->Config(), "AdvOut", "FFFormatMimeType", desc.mimeType);

		const char *ext = ff_format_desc_extensions(desc.desc);
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

void PLSBasicSettings::SaveEncoder(QComboBox *combo, const char *section, const char *value)
{
	QVariant v = combo->currentData();
	CodecDesc cd;
	if (!v.isNull())
		cd = v.value<CodecDesc>();
	config_set_int(main->Config(), section, QT_TO_UTF8(QString("%1Id").arg(value)), cd.id);
	if (cd.id != 0)
		config_set_string(main->Config(), section, value, cd.name);
	else
		config_set_string(main->Config(), section, value, nullptr);
}

void PLSBasicSettings::SaveOutputSettings()
{
	const char *mode = OutputModeFromIdx(ui->outputMode->currentIndex());

	config_set_string(main->Config(), "Output", "Mode", mode);

	QString encoder = ui->simpleOutStrEncoder->currentData().toString();
	const char *presetType;

	if (encoder == SIMPLE_ENCODER_QSV)
		presetType = "QSVPreset";
	else if (encoder == SIMPLE_ENCODER_NVENC)
		presetType = "NVENCPreset";
	else if (encoder == SIMPLE_ENCODER_AMD)
		presetType = "AMDPreset";
	else
		presetType = "Preset";

	SaveSpinBox(ui->simpleOutputVBitrate, "SimpleOutput", "VBitrate");
	SaveComboData(ui->simpleOutStrEncoder, "SimpleOutput", "StreamEncoder");
	SaveCombo(ui->simpleOutputABitrate, "SimpleOutput", "ABitrate");
	SaveEdit(ui->simpleOutputPath, "SimpleOutput", "FilePath");
	SaveCheckBox(ui->simpleNoSpace, "SimpleOutput", "FileNameWithoutSpace");
	SaveCombo(ui->simpleOutRecFormat, "SimpleOutput", "RecFormat");
	SaveCheckBox(ui->simpleOutAdvanced, "SimpleOutput", "UseAdvanced");
	SaveCheckBox(ui->simpleOutEnforce, "SimpleOutput", "EnforceBitrate");
	SaveComboData(ui->simpleOutPreset, "SimpleOutput", presetType);
	SaveEdit(ui->simpleOutCustom, "SimpleOutput", "x264Settings");
	SaveComboData(ui->simpleOutRecQuality, "SimpleOutput", "RecQuality");
	SaveComboData(ui->simpleOutRecEncoder, "SimpleOutput", "RecEncoder");
	SaveEdit(ui->simpleOutMuxCustom, "SimpleOutput", "MuxerCustom");
	SaveCheckBox(ui->simpleReplayBuf, "SimpleOutput", "RecRB");
	SaveSpinBox(ui->simpleRBSecMax, "SimpleOutput", "RecRBTime");
	SaveSpinBox(ui->simpleRBMegsMax, "SimpleOutput", "RecRBSize");
	if (pls_is_immersive_audio()) {
		config_set_int(main->Config(), "SimpleOutput", "TrackIndex", ui->simpleImmersiveAudioStreaming->isChecked() ? 2 : 1);
		config_set_int(main->Config(), "SimpleOut", "ImmersiveRecTracks", ui->immersiveAudioStreaming->isChecked() ? (1 << 0) | (1 << 1) : (1 << 0));
	}
	curAdvStreamEncoder = GetComboData(ui->advOutEncoder);

	SaveCheckBox(ui->advOutApplyService, "AdvOut", "ApplyServiceSettings");
	SaveComboData(ui->advOutEncoder, "AdvOut", "Encoder");
	SaveCheckBox(ui->advOutUseRescale, "AdvOut", "Rescale");
	SaveCombo(ui->advOutRescale, "AdvOut", "RescaleRes");
	if (!pls_is_immersive_audio()) {
		SaveTrackIndex(main->Config(), "AdvOut", "TrackIndex", ui->advOutTrack1, ui->advOutTrack2, ui->advOutTrack3, ui->advOutTrack4, ui->advOutTrack5, ui->advOutTrack6);
	} else {
		config_set_int(main->Config(), "AdvOut", "ImmersiveTrackIndex", ui->immersiveAudioStreaming->isChecked() ? 2 : 1);
	}
	config_set_string(main->Config(), "AdvOut", "RecType", RecTypeFromIdx(ui->advOutRecType->currentIndex()));

	curAdvRecordEncoder = GetComboData(ui->advOutRecEncoder);

	SaveEdit(ui->advOutRecPath, "AdvOut", "RecFilePath");
	SaveCheckBox(ui->advOutNoSpace, "AdvOut", "RecFileNameWithoutSpace");
	SaveCombo(ui->advOutRecFormat, "AdvOut", "RecFormat");
	SaveComboData(ui->advOutRecEncoder, "AdvOut", "RecEncoder");
	SaveCheckBox(ui->advOutRecUseRescale, "AdvOut", "RecRescale");
	SaveCombo(ui->advOutRecRescale, "AdvOut", "RecRescaleRes");
	SaveEdit(ui->advOutMuxCustom, "AdvOut", "RecMuxerCustom");

	if (pls_is_immersive_audio()) {
		config_set_int(main->Config(), "AdvOut", "ImmersiveRecTracks", ui->immersiveAudioStreaming->isChecked() ? (1 << 0) | (1 << 1) : (1 << 0));
	} else {
		config_set_int(main->Config(), "AdvOut", "RecTracks",
			       (ui->advOutRecTrack1->isChecked() ? (1 << 0) : 0) | (ui->advOutRecTrack2->isChecked() ? (1 << 1) : 0) | (ui->advOutRecTrack3->isChecked() ? (1 << 2) : 0) |
				       (ui->advOutRecTrack4->isChecked() ? (1 << 3) : 0) | (ui->advOutRecTrack5->isChecked() ? (1 << 4) : 0) | (ui->advOutRecTrack6->isChecked() ? (1 << 5) : 0));
	}
	config_set_int(main->Config(), "AdvOut", "FLVTrack", CurrentFLVTrack());

	config_set_bool(main->Config(), "AdvOut", "FFOutputToFile", ui->advOutFFType->currentIndex() == 0 ? true : false);
	SaveEdit(ui->advOutFFRecPath, "AdvOut", "FFFilePath");
	SaveCheckBox(ui->advOutFFNoSpace, "AdvOut", "FFFileNameWithoutSpace");
	SaveEdit(ui->advOutFFURL, "AdvOut", "FFURL");
	SaveFormat(ui->advOutFFFormat);
	SaveEdit(ui->advOutFFMCfg, "AdvOut", "FFMCustom");
	SaveSpinBox(ui->advOutFFVBitrate, "AdvOut", "FFVBitrate");
	SaveSpinBox(ui->advOutFFVGOPSize, "AdvOut", "FFVGOPSize");
	SaveCheckBox(ui->advOutFFUseRescale, "AdvOut", "FFRescale");
	SaveCheckBox(ui->advOutFFIgnoreCompat, "AdvOut", "FFIgnoreCompat");
	SaveCombo(ui->advOutFFRescale, "AdvOut", "FFRescaleRes");
	SaveEncoder(ui->advOutFFVEncoder, "AdvOut", "FFVEncoder");
	SaveEdit(ui->advOutFFVCfg, "AdvOut", "FFVCustom");
	SaveSpinBox(ui->advOutFFABitrate, "AdvOut", "FFABitrate");
	SaveEncoder(ui->advOutFFAEncoder, "AdvOut", "FFAEncoder");
	SaveEdit(ui->advOutFFACfg, "AdvOut", "FFACustom");
	config_set_int(main->Config(), "AdvOut", "FFAudioMixes",
		       (ui->advOutFFTrack1->isChecked() ? (1 << 0) : 0) | (ui->advOutFFTrack2->isChecked() ? (1 << 1) : 0) | (ui->advOutFFTrack3->isChecked() ? (1 << 2) : 0) |
			       (ui->advOutFFTrack4->isChecked() ? (1 << 3) : 0) | (ui->advOutFFTrack5->isChecked() ? (1 << 4) : 0) | (ui->advOutFFTrack6->isChecked() ? (1 << 5) : 0));
	SaveCombo(ui->advOutTrack1Bitrate, "AdvOut", "Track1Bitrate");
	SaveCombo(ui->advOutTrack2Bitrate, "AdvOut", "Track2Bitrate");
	SaveCombo(ui->advOutTrack3Bitrate, "AdvOut", "Track3Bitrate");
	SaveCombo(ui->advOutTrack4Bitrate, "AdvOut", "Track4Bitrate");
	SaveCombo(ui->advOutTrack5Bitrate, "AdvOut", "Track5Bitrate");
	SaveCombo(ui->advOutTrack6Bitrate, "AdvOut", "Track6Bitrate");
	SaveCombo(ui->advOutTrackStereoBitrate, "AdvOut", "TrackStereoBitrate");
	SaveCombo(ui->advOutTrackImmersiveBitrate, "AdvOut", "TrackImmersiveBitrate");
	SaveEdit(ui->advOutTrack1Name, "AdvOut", "Track1Name");
	SaveEdit(ui->advOutTrack2Name, "AdvOut", "Track2Name");
	SaveEdit(ui->advOutTrack3Name, "AdvOut", "Track3Name");
	SaveEdit(ui->advOutTrack4Name, "AdvOut", "Track4Name");
	SaveEdit(ui->advOutTrack5Name, "AdvOut", "Track5Name");
	SaveEdit(ui->advOutTrack6Name, "AdvOut", "Track6Name");
	SaveEdit(ui->advOutTrackStereoName, "AdvOut", "TrackStereoName");
	SaveEdit(ui->advOutTrackImmersiveName, "AdvOut", "TrackImmersiveName");

	SaveCheckBox(ui->advReplayBuf, "AdvOut", "RecRB");
	SaveSpinBox(ui->advRBSecMax, "AdvOut", "RecRBTime");
	SaveSpinBox(ui->advRBMegsMax, "AdvOut", "RecRBSize");

	WriteJsonData(streamEncoderProps, "streamEncoder.json");
	WriteJsonData(recordEncoderProps, "recordEncoder.json");

	if (main->outputHandler && main->outputHandler->replayBuffer && !strcmp(obs_obj_get_id(main->outputHandler->replayBuffer), "replay_buffer")) {
		obs_data_t *hotkeys = obs_hotkeys_save_output(main->outputHandler->replayBuffer);
		config_set_string(main->Config(), "Hotkeys", "ReplayBuffer", obs_data_get_json(hotkeys));
		obs_data_release(hotkeys);
	}

	main->ResetOutputs();
}

void PLSBasicSettings::SaveAudioSettings()
{
	QString sampleRateStr = ui->sampleRate->currentText();
	int channelSetupIdx = ui->channelSetup->currentIndex();

	const char *channelSetup;
	if (pls_is_immersive_audio()) {
		switch (channelSetupIdx) {
		case 0:
			channelSetup = "Stereo";
			break;
		default:
			channelSetup = "Stereo";
			break;
		}
	} else {
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
	}

	int sampleRate = 44100;
	if (sampleRateStr == "48 kHz")
		sampleRate = 48000;
	if (pls_is_immersive_audio()) {
		sampleRate = 48000;
	}

	if (WidgetChanged(ui->sampleRate))
		config_set_uint(main->Config(), "Audio", "SampleRate", sampleRate);

	if (WidgetChanged(ui->channelSetup))
		config_set_string(main->Config(), "Audio", "ChannelSetup", channelSetup);

	if (WidgetChanged(ui->meterDecayRate)) {
		double meterDecayRate;
		switch (ui->meterDecayRate->currentIndex()) {
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

	if (WidgetChanged(ui->peakMeterType)) {
		uint32_t peakMeterTypeIdx = ui->peakMeterType->currentIndex();
		config_set_uint(main->Config(), "Audio", "PeakMeterType", peakMeterTypeIdx);

		main->UpdateVolumeControlsPeakMeterType();
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
		main->ResetAudioDevice(input ? App()->InputAudioSource() : App()->OutputAudioSource(), QT_TO_UTF8(GetComboData(combo)), Str(name), index);
	};

	UpdateAudioDevice(false, ui->desktopAudioDevice1, "Basic.DesktopDevice1", 1);
	UpdateAudioDevice(false, ui->desktopAudioDevice2, "Basic.DesktopDevice2", 2);
	UpdateAudioDevice(true, ui->auxAudioDevice1, "Basic.AuxDevice1", 3);
	UpdateAudioDevice(true, ui->auxAudioDevice2, "Basic.AuxDevice2", 4);
	UpdateAudioDevice(true, ui->auxAudioDevice3, "Basic.AuxDevice3", 5);
	UpdateAudioDevice(true, ui->auxAudioDevice4, "Basic.AuxDevice4", 6);
	main->SaveProject();
}

void PLSBasicSettings::SaveHotkeySettings()
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

		obs_data_array_t *array = obs_hotkey_save(hw.id);
		obs_data_t *data = obs_data_create();
		obs_data_set_array(data, "bindings", array);
		const char *json = obs_data_get_json(data);
		config_set_string(config, "Hotkeys", hw.name.c_str(), json);
		obs_data_release(data);
		obs_data_array_release(array);
	}

	if (!main->outputHandler || !main->outputHandler->replayBuffer)
		return;

	const char *id = obs_obj_get_id(main->outputHandler->replayBuffer);
	if (strcmp(id, "replay_buffer") == 0) {
		obs_data_t *hotkeys = obs_hotkeys_save_output(main->outputHandler->replayBuffer);
		config_set_string(config, "Hotkeys", "ReplayBuffer", obs_data_get_json(hotkeys));
		obs_data_release(hotkeys);
	}
}

#define MINOR_SEPARATOR "------------------------------------------------"

static void AddChangedVal(std::string &changed, const char *str)
{
	if (changed.size())
		changed += ", ";
	changed += str;
}

void PLSBasicSettings::SaveSettings()
{
	if (replayBufferHotkeyWidget) {
		config_set_string(main->Config(), "Others", "Hotkeys.ReplayBuffer", replayBufferHotkeyWidget->getHotkeyText().remove(' ').toUtf8().constData());
	}
	if (hotkeysChanged)
		SaveHotkeySettings();
	if (generalChanged)
		SaveGeneralSettings();
	if (stream1Changed)
		SaveStream1Settings();
	if (outputsChanged)
		SaveOutputSettings();
	if (audioChanged)
		SaveAudioSettings();
	if (videoChanged)
		SaveVideoSettings();
	if (advancedChanged)
		SaveAdvancedSettings();

	if (videoChanged || advancedChanged)
		main->ResetVideo();
	SaveSceneDisplayMethodSettings();

	config_save_safe(main->Config(), "tmp", nullptr);
	config_save_safe(GetGlobalConfig(), "tmp", nullptr);
	main->SaveProject();

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
		if (advancedChanged)
			AddChangedVal(changed, "advanced");

		PLS_INFO(SETTING_MODULE, "Settings changed (%s)", changed.c_str());
		PLS_INFO(SETTING_MODULE, MINOR_SEPARATOR);
	}

	main->showEncodingInStatusBar();

	// Restart program when audio framerate or channel changed
	if (ui->channelSetup->currentIndex() != channelIndex || ui->sampleRate->currentIndex() != sampleRateIndex) {
		m_doneValue = Qt::UserRole + 1025;
	}
}

void PLSBasicSettings::SaveSceneDisplayMethodSettings()
{
	config_set_int(GetGlobalConfig(), "BasicWindow", "SceneDisplayMethod", ui->sceneDisplayComboBox->currentIndex());
	if (main) {
		main->SetSceneDisplayMethod(ui->sceneDisplayComboBox->currentIndex());
	}
}

void PLSBasicSettings::ResetSettings()
{
	auto config = main->Config();
	auto globalConfig = GetGlobalConfig();

	// reset general settings
#if defined(_WIN32) || defined(__APPLE__)
	config_remove_value(globalConfig, "General", "EnableAutoUpdates");
#endif

	obs_watermark_set_enabled(true);
	config_set_bool(globalConfig, "General", "Watermark", true);

	config_remove_value(config, "General", "OpenStatsOnStartup");
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

	config_remove_value(globalConfig, "BasicWindow", "HideProjectorCursor");
	config_remove_value(globalConfig, "BasicWindow", "ProjectorAlwaysOnTop");

	config_remove_value(globalConfig, "BasicWindow", "RecordWhenStreaming");
	config_remove_value(globalConfig, "BasicWindow", "KeepRecordingWhenStreamStops");

	config_remove_value(globalConfig, "BasicWindow", "ReplayBufferWhileStreaming");
	config_remove_value(globalConfig, "BasicWindow", "KeepReplayBufferStreamStops");

	config_remove_value(globalConfig, "BasicWindow", "SysTrayEnabled");
	config_remove_value(globalConfig, "BasicWindow", "SysTrayWhenStarted");
	config_remove_value(globalConfig, "BasicWindow", "SysTrayMinimizeToTray");

	config_remove_value(globalConfig, "BasicWindow", "SaveProjectors");
	config_remove_value(globalConfig, "BasicWindow", "StudioPortraitLayout");
	config_remove_value(globalConfig, "BasicWindow", "StudioModeLabels");

	config_remove_value(globalConfig, "BasicWindow", "MultiviewMouseSwitch");
	config_remove_value(globalConfig, "BasicWindow", "MultiviewDrawNames");
	config_remove_value(globalConfig, "BasicWindow", "MultiviewDrawAreas");
	config_remove_value(globalConfig, "BasicWindow", "MultiviewLayout");

	// reset output settings
	config_remove_value(config, "Output", "Mode");
	config_remove_value(config, "SimpleOutput", "VBitrate");
	config_remove_value(config, "SimpleOutput", "StreamEncoder");
	config_remove_value(config, "SimpleOutput", "ABitrate");
	config_remove_value(config, "SimpleOutput", "FilePath");
	config_remove_value(config, "SimpleOutput", "FileNameWithoutSpace");
	config_remove_value(config, "SimpleOutput", "RecFormat");
	config_remove_value(config, "SimpleOutput", "UseAdvanced");
	config_remove_value(config, "SimpleOutput", "EnforceBitrate");
	config_remove_value(config, "SimpleOutput", "QSVPreset");
	config_remove_value(config, "SimpleOutput", "NVENCPreset");
	config_remove_value(config, "SimpleOutput", "AMDPreset");
	config_remove_value(config, "SimpleOutput", "Preset");
	config_remove_value(config, "SimpleOutput", "x264Settings");
	config_remove_value(config, "SimpleOutput", "RecQuality");
	config_remove_value(config, "SimpleOutput", "RecEncoder");
	config_remove_value(config, "SimpleOutput", "MuxerCustom");
	config_remove_value(config, "SimpleOutput", "RecRB");
	config_remove_value(config, "SimpleOutput", "RecRBTime");
	config_remove_value(config, "SimpleOutput", "RecRBSize");
	config_remove_value(config, "SimpleOutput", "TrackIndex");

	config_remove_value(config, "AdvOut", "ApplyServiceSettings");
	config_remove_value(config, "AdvOut", "Encoder");
	config_remove_value(config, "AdvOut", "Rescale");
	config_remove_value(config, "AdvOut", "RescaleRes");
	config_remove_value(config, "AdvOut", "TrackIndex");
	config_remove_value(config, "AdvOut", "ImmersiveTrackIndex");

	config_remove_value(config, "AdvOut", "RecType");

	config_remove_value(config, "AdvOut", "RecFilePath");
	config_remove_value(config, "AdvOut", "RecFileNameWithoutSpace");
	config_remove_value(config, "AdvOut", "RecFormat");
	config_remove_value(config, "AdvOut", "RecEncoder");
	config_remove_value(config, "AdvOut", "RecRescale");
	config_remove_value(config, "AdvOut", "RecRescaleRes");
	config_remove_value(config, "AdvOut", "RecMuxerCustom");

	config_remove_value(config, "AdvOut", "RecTracks");

	config_remove_value(config, "AdvOut", "FLVTrack");

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
	config_remove_value(config, "AdvOut", "TrackStereoBitrate");
	config_remove_value(config, "AdvOut", "TrackImmersiveBitrate");
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

#ifdef _WIN32
	config_remove_value(config, "Video", "DisableAero");
#endif

	// reset audio settings
	config_remove_value(config, "Audio", "SampleRate");
	config_remove_value(config, "Audio", "ChannelSetup");
	config_remove_value(config, "Audio", "MeterDecayRate");
	config_remove_value(config, "Audio", "PeakMeterType");

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
		main->ResetAudioDevice(input ? App()->InputAudioSource() : App()->OutputAudioSource(), value, Str(name), index);
	};

	resetAudioDevice(false, "default", "Basic.DesktopDevice1", 1);
	resetAudioDevice(false, "disabled", "Basic.DesktopDevice2", 2);
	resetAudioDevice(true, "default", "Basic.AuxDevice1", 3);
	resetAudioDevice(true, "disabled", "Basic.AuxDevice2", 4);
	resetAudioDevice(true, "disabled", "Basic.AuxDevice3", 5);
	resetAudioDevice(true, "disabled", "Basic.AuxDevice4", 6);

	// reset advanced settings
	config_remove_value(config, "Audio", "MonitoringDeviceId");

	config_remove_value(globalConfig, "Video", "Renderer");

	config_remove_value(globalConfig, "General", "ProcessPriority");
	SetProcessPriority(config_get_default_string(globalConfig, "General", "ProcessPriority"));

	config_remove_value(config, "Output", "NewSocketLoopEnable");
	config_remove_value(config, "Output", "LowLatencyEnable");

	config_remove_value(globalConfig, "General", "BrowserHWAccel");

	config_remove_value(globalConfig, "General", "HotkeyFocusType");

	config_remove_value(config, "Video", "ColorFormat");
	config_remove_value(config, "Video", "ColorSpace");
	config_remove_value(config, "Video", "ColorRange");

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

	ui->monitoringDevice->setCurrentIndex(0);
	QString newDevice = ui->monitoringDevice->currentData().toString();
	obs_set_audio_monitoring_device(QT_TO_UTF8(ui->monitoringDevice->currentText()), QT_TO_UTF8(newDevice));
	PLS_INFO(SETTING_MODULE, "Audio monitoring device:\n\tname: %s\n\tid: %s", QT_TO_UTF8(ui->monitoringDevice->currentText()), QT_TO_UTF8(newDevice));

	// reset hotkey settings
	for (auto &hotkey : hotkeys) {
		hotkey.second->Clear();
	}

	ui->sceneDisplayComboBox->clear();
	config_remove_value(globalConfig, "BasicWindow", "SceneDisplayMethod");
	main->SetSceneDisplayMethod(0);

	SaveHotkeySettings();

	config_set_string(config, "Hotkeys", "ReplayBuffer", "{\"ReplayBuffer.Save\":[{\"alt\":true,\"key\":\"OBS_KEY_R\"}]}");
	config_set_string(config, "Others", "Hotkeys.ReplayBuffer", "Alt+R");

	main->InitBasicConfigDefaults();

	DisableAudioDucking(config_get_default_bool(globalConfig, "Audio", "DisableAudioDucking"));
	main->UpdateVolumeControlsDecayRate();
	main->UpdateVolumeControlsPeakMeterType();

	main->ResetUI();
	PLSProjector::UpdateMultiviewProjectors();

	main->ResetOutputs();

	main->ResetVideo();

	main->SaveProject();

	config_save_safe(config, "tmp", nullptr);
	config_save_safe(globalConfig, "tmp", nullptr);

	main->showEncodingInStatusBar();
}

bool PLSBasicSettings::QueryChanges()
{
	PLSAlertView::Button button = PLSMessageBox::question(this, QTStr("Basic.Settings.ConfirmTitle"), QTStr("Basic.Settings.Confirm"),
							      PLSAlertView::Button::Yes | PLSAlertView::Button::No | PLSAlertView::Button::Cancel);

	if (button == PLSAlertView::Button::Cancel || button == PLSAlertView::Button::NoButton) {
		ClearChanged();
		return false;
	} else if (button == PLSAlertView::Button::Yes) {
		if ((ui->language->currentData().toString().toStdString() != m_currentLanguage.first) &&
		    PLSAlertView::Button::Yes ==
			    PLSAlertView::warning(this, QTStr("Basic.Settings.ConfirmTitle"), QTStr("Basic.Settings.General.language.changed"), PLSAlertView::Button::Yes | PLSAlertView::Button::No)) {
			m_doneValue = Qt::UserRole + 1024;
		} else {
			ui->language->setCurrentText(QString::fromStdString(m_currentLanguage.second));
		}
		SaveSettings();
	} else {
		LoadSettings(true);
#ifdef _WIN32
		if (toggleAero)
			SetAeroEnabled(!aeroWasDisabled);
#endif
	}

	ClearChanged();
	return true;
}

bool PLSBasicSettings::onCloseEvent(QCloseEvent *event)
{
	bool result = true;
	bool isCallBaseCloseEvent = true;
	if ((m_doneValue != LoginInfoType::PrismLogoutInfo) && (m_doneValue != LoginInfoType::PrismSignoutInfo) && Changed() && !QueryChanges()) {
		result = false;
		if (event) {
			event->ignore();
			isCallBaseCloseEvent = false;
		}
	}

	if (forceAuthReload) {
		main->auth->Save();
		main->auth->Load();
		forceAuthReload = false;
	}
	if (isCallBaseCloseEvent) {
		callBaseCloseEvent(event);
	}
	return result;
}

void PLSBasicSettings::hotkeysClearButtonClicked(QPushButton *button)
{
	componentValueChanged(getPageOfSender(button), button);
}
void PLSBasicSettings::OnSceneDisplayMethodIndexChanged(int index)
{
	if (index < 0 || index >= SCENE_DISPLAY_TIPS_NUM) {
		return;
	}

	ui->sceneDisplayTipsLabel->setText(tr(sceneDisplayTips[index]));
}

bool PLSBasicSettings::event(QEvent *event)
{
	return PLSDialogView::event(event);
}

bool PLSBasicSettings::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->outputSettingsAdvTabs) {
		switch (event->type()) {
		case QEvent::Resize: {
			QSize size = dynamic_cast<QResizeEvent *>(event)->size();
			outputSettingsAdvTabsHLine->setGeometry(0, size.height() - PLSDpiHelper::calculate(this, 2), size.width(), PLSDpiHelper::calculate(this, 1));
			break;
		}
		}
	} else if (watched == ui->generalPage) {
		// if (event->type() == QEvent::Show) {
		// 	QMetaObject::invokeMethod(
		// 		this, [=]() { setLabelLimitedExclude<QWidget, QWidget>("general page", PLSDpiHelper::calculate(dpi, 170), {GENERAL_PAGE_FORMLABELS}, {ui->label}); },
		// 		Qt::QueuedConnection);
		// }
	} else if (watched == ui->outputPage) {
		if (event->type() == QEvent::Show) {
			QList<QWidget *> labels{OUTPUT_PAGE_FORMLABELS};
			if (streamEncoderProps) {
				labels.append(streamEncoderProps->findChildren<QWidget *>(OBJECT_NAME_FORMLABEL));
			}
			if (recordEncoderProps) {
				labels.append(recordEncoderProps->findChildren<QWidget *>(OBJECT_NAME_FORMLABEL));
			}
			setLabelLimited("output page", PLSDpiHelper::calculate(this, 170), labels);

			QString text =
				tr("Basic.Settings.Output.ReplayBuffer.HotkeyMessage")
					.arg(replayBufferHotkeyWidget ? replayBufferHotkeyWidget->getHotkeyText().remove(' ') : config_get_string(main->Config(), "Others", "Hotkeys.ReplayBuffer"));
			ui->replayBufferHotkeyMessage1->setText(text);
			ui->replayBufferHotkeyMessage2->setText(text);
		}
	} else if (watched == ui->audioPage) {
		if (event->type() == QEvent::Show) {
			setLabelLimited<OBSSourceLabel, QWidget>("audio page", PLSDpiHelper::calculate(this, 170), ui->audioPage->findChildren<OBSSourceLabel *>("audioHotkeyFormLabel"),
								 {AUDIO_PAGE_FORMLABELS});
		}
	} else if (watched == ui->viewPage) {
		if (event->type() == QEvent::Show) {
			setLabelLimited<QWidget>("view page", PLSDpiHelper::calculate(this, 170), {VIEW_PAGE_FORMLABELS});
		}
	} else if (watched == ui->sourcePage) {
		if (event->type() == QEvent::Show) {
			QMetaObject::invokeMethod(
				this, [=]() { setLabelLimited<QWidget>("source page", PLSDpiHelper::calculate(this, 170), {SOURCE_PAGE_FORMLABELS}); }, Qt::QueuedConnection);
		}
	} else if (watched == ui->hotkeyPage) {
		if (event->type() == QEvent::Show) {
			setLabelLimited<PLSHotkeyLabel, QWidget>("hotkey page", PLSDpiHelper::calculate(this, 170), ui->hotkeyPage->findChildren<PLSHotkeyLabel *>("hotkeyHotkeyFormLabel"),
								 {ui->hotkeyFocusTypeLabel, hotkeyFilterLabel});
		}
	} else if (QLabel *label = dynamic_cast<QLabel *>(watched); label && isChild(ui->hotkeyPage, label) && watched->dynamicPropertyNames().contains("groupName")) {
		if (event->type() == QEvent::Resize) {
			QFontMetrics fontWidth(label->font());
			label->setText(fontWidth.elidedText(label->property("groupName").toString(), Qt::ElideRight, ui->hotkeyPage->width() - PLSDpiHelper::calculate(this, 50)));
		}
	}
	return PLSDialogView::eventFilter(watched, event);
}

void PLSBasicSettings::on_theme_activated(int idx)
{
	QString currT = ui->theme->itemText(idx);
	App()->SetTheme(currT.toUtf8().constData());
}

void PLSBasicSettings::on_listWidget_itemSelectionChanged()
{
	int row = ui->listWidget->currentRow();

	if (loading || row == pageIndex)
		return;

	pageIndex = row;
	PLS_UI_STEP(SETTING_MODULE, QStringLiteral("Settings > listWidgetItem %1").arg(ui->listWidget->item(row)->text()).toUtf8().constData(), ACTION_CLICK);
	updateAlertMessage();
}

void PLSBasicSettings::on_buttonBox_clicked(QAbstractButton *button)
{
	QDialogButtonBox::ButtonRole val = ui->buttonBox->buttonRole(button);

	if (val == QDialogButtonBox::ApplyRole || val == QDialogButtonBox::AcceptRole) {

		if (App()->GetLocale() != ui->language->currentData()) {
			if (PLSAlertView::Button::Yes ==
			    PLSAlertView::warning(this, QTStr("Basic.Settings.ConfirmTitle"), QTStr("Basic.Settings.General.language.changed"), PLSAlertView::Button::Yes | PLSAlertView::Button::No)) {
				SaveSettings();
				ClearChanged();
				done(Qt::UserRole + 1024);
				m_doneValue = Qt::UserRole + 1024;
				return;
			} else {
				ui->language->setCurrentText(QString::fromStdString(m_currentLanguage.second));
			}
		}
		this->setFocus();
		SaveSettings();
		ClearChanged();
	}
	if (val == QDialogButtonBox::AcceptRole || val == QDialogButtonBox::RejectRole) {
		if (val == QDialogButtonBox::RejectRole) {
			// App()->SetTheme(savedTheme);
#ifdef _WIN32
			if (toggleAero)
				SetAeroEnabled(!aeroWasDisabled);
#endif
		}
		ClearChanged();
		close();
	}
}

void PLSBasicSettings::on_simpleOutputBrowse_clicked()
{
	componentValueChanged(getPageOfSender(), sender());

	QString dir =
		QFileDialog::getExistingDirectory(this, QTStr("Basic.Settings.Output.SelectDirectory"), ui->simpleOutputPath->text(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (dir.isEmpty())
		return;

	ui->simpleOutputPath->setText(dir);
}

void PLSBasicSettings::on_advOutRecPathBrowse_clicked()
{
	componentValueChanged(getPageOfSender(), sender());

	QString dir = QFileDialog::getExistingDirectory(this, QTStr("Basic.Settings.Output.SelectDirectory"), ui->advOutRecPath->text(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (dir.isEmpty())
		return;

	ui->advOutRecPath->setText(dir);
}

void PLSBasicSettings::on_advOutFFPathBrowse_clicked()
{
	componentValueChanged(getPageOfSender(), sender());

	QString dir = QFileDialog::getExistingDirectory(this, QTStr("Basic.Settings.Output.SelectDirectory"), ui->advOutRecPath->text(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
	if (dir.isEmpty())
		return;

	ui->advOutFFRecPath->setText(dir);
}

void PLSBasicSettings::on_advOutEncoder_currentIndexChanged(int idx)
{
	QString encoder = GetComboData(ui->advOutEncoder);
	if (!loading) {
		bool loadSettings = encoder == curAdvStreamEncoder;

		delete streamEncoderProps;
		streamEncoderProps = nullptr;

		CreateEncoderPropertyView(streamEncoderProps, ui->advOutputStreamTab, QT_TO_UTF8(encoder), loadSettings ? "streamEncoder.json" : nullptr, true);
		ui->advOutputStreamTab->layout()->addWidget(streamEncoderProps);
	}

	uint32_t caps = obs_get_encoder_caps(QT_TO_UTF8(encoder));

	if (caps & OBS_ENCODER_CAP_PASS_TEXTURE) {
		ui->advOutUseRescale->setChecked(false);
		ui->advOutUseRescale->setVisible(false);
		ui->advOutRescale->setVisible(false);
		ui->widget_14->setVisible(false);
	} else {
		ui->advOutUseRescale->setVisible(true);
		ui->advOutRescale->setVisible(true);
		ui->widget_14->setVisible(true);
	}

	UNUSED_PARAMETER(idx);
}

void PLSBasicSettings::on_advOutRecEncoder_currentIndexChanged(int idx)
{
	if (!loading) {
		delete recordEncoderProps;
		recordEncoderProps = nullptr;
	}

	auto setRescaleVisible = [=](bool visible) {
		if (visible) {
			ui->formLayout_16->setWidget(5, QFormLayout::LabelRole, ui->advOutRecUseRescaleContainer);
			ui->formLayout_16->setWidget(5, QFormLayout::FieldRole, ui->advOutRecRescaleContainer);
			ui->advOutRecUseRescaleContainer->show();
			ui->advOutRecRescaleContainer->show();
		} else {
			layoutRemoveWidget(ui->formLayout_16, ui->advOutRecUseRescaleContainer);
			layoutRemoveWidget(ui->formLayout_16, ui->advOutRecRescaleContainer);
		}
	};

	if (idx <= 0) {
		ui->advOutRecUseRescale->setChecked(false);
		ui->advOutRecUseRescale->setVisible(false);
		setRescaleVisible(false);
		return;
	}

	QString encoder = GetComboData(ui->advOutRecEncoder);
	bool loadSettings = encoder == curAdvRecordEncoder;

	if (!loading) {
		CreateEncoderPropertyView(recordEncoderProps, ui->advOutRecStandard, QT_TO_UTF8(encoder), loadSettings ? "recordEncoder.json" : nullptr, true);
		ui->advOutRecStandard->layout()->addWidget(recordEncoderProps);
		connect(recordEncoderProps, SIGNAL(Changed()), this, SLOT(AdvReplayBufferChanged()));
	}

	uint32_t caps = obs_get_encoder_caps(QT_TO_UTF8(encoder));

	if (caps & OBS_ENCODER_CAP_PASS_TEXTURE) {
		ui->advOutRecUseRescale->setChecked(false);
		ui->advOutRecUseRescale->setVisible(false);
		setRescaleVisible(false);
	} else {
		ui->advOutRecUseRescale->setVisible(true);
		setRescaleVisible(true);
	}
}

void PLSBasicSettings::on_advOutFFIgnoreCompat_stateChanged(int)
{
	/* Little hack to reload codecs when checked */
	on_advOutFFFormat_currentIndexChanged(ui->advOutFFFormat->currentIndex());
}

#define DEFAULT_CONTAINER_STR QTStr("Basic.Settings.Output.Adv.FFmpeg.FormatDescDef")

void PLSBasicSettings::on_advOutFFFormat_currentIndexChanged(int idx)
{
	const QVariant itemDataVariant = ui->advOutFFFormat->itemData(idx);

	if (!itemDataVariant.isNull()) {
		FormatDesc desc = itemDataVariant.value<FormatDesc>();
		SetAdvOutputFFmpegEnablement(FF_CODEC_AUDIO, ff_format_desc_has_audio(desc.desc), false);
		SetAdvOutputFFmpegEnablement(FF_CODEC_VIDEO, ff_format_desc_has_video(desc.desc), false);
		ReloadCodecs(desc.desc);
		ui->advOutFFFormatDesc->setText(ff_format_desc_long_name(desc.desc));

		CodecDesc defaultAudioCodecDesc = GetDefaultCodecDesc(desc.desc, FF_CODEC_AUDIO);
		CodecDesc defaultVideoCodecDesc = GetDefaultCodecDesc(desc.desc, FF_CODEC_VIDEO);
		SelectEncoder(ui->advOutFFAEncoder, defaultAudioCodecDesc.name, defaultAudioCodecDesc.id);
		SelectEncoder(ui->advOutFFVEncoder, defaultVideoCodecDesc.name, defaultVideoCodecDesc.id);
	} else {
		ReloadCodecs(nullptr);
		ui->advOutFFFormatDesc->setText(DEFAULT_CONTAINER_STR);
	}
}

void PLSBasicSettings::on_advOutFFAEncoder_currentIndexChanged(int idx)
{
	const QVariant itemDataVariant = ui->advOutFFAEncoder->itemData(idx);
	if (!itemDataVariant.isNull()) {
		CodecDesc desc = itemDataVariant.value<CodecDesc>();
		SetAdvOutputFFmpegEnablement(FF_CODEC_AUDIO, desc.id != 0 || desc.name != nullptr, true);
	}
}

void PLSBasicSettings::on_advOutFFVEncoder_currentIndexChanged(int idx)
{
	const QVariant itemDataVariant = ui->advOutFFVEncoder->itemData(idx);
	if (!itemDataVariant.isNull()) {
		CodecDesc desc = itemDataVariant.value<CodecDesc>();
		SetAdvOutputFFmpegEnablement(FF_CODEC_VIDEO, desc.id != 0 || desc.name != nullptr, true);
	}
}

void PLSBasicSettings::on_advOutFFType_currentIndexChanged(int idx)
{
	ui->advOutFFNoSpace->setHidden(idx != 0);
}

void PLSBasicSettings::on_colorFormat_currentIndexChanged(const QString &text)
{
	bool usingNV12 = text == "NV12";

	if (usingNV12) {
		clearAlertMessage(AlertMessageType::Warning, ui->colorFormat);
	} else {
		updateAlertMessage(AlertMessageType::Warning, ui->colorFormat, QTStr("Basic.Settings.Advanced.FormatWarning"));
	}
}

#define INVALID_RES_STR "Basic.Settings.Video.InvalidResolution"

static bool ValidResolutions(PLSBasicSettings *settings, Ui::PLSBasicSettings *ui)
{
	bool result = true;

	uint32_t cx, cy;
	if (!ConvertResText(QT_TO_UTF8(ui->baseResolution->lineEdit()->text()), cx, cy) || !ConvertResText(QT_TO_UTF8(ui->outputResolution->lineEdit()->text()), cx, cy)) {
		//PRISM/Liu.Haibin/20200413/#None/limit resolution
		QString text = QTStr(INVALID_RES_STR).arg(g_maxRolutionSize ? QString::number(g_maxRolutionSize).toStdString().c_str() : QString::number(RESOLUTION_SIZE_MAX).toStdString().c_str());
		settings->updateAlertMessage(PLSBasicSettings::AlertMessageType::Error, ui->baseResolution, text);
		result = false;
	} else {
		settings->clearAlertMessage(PLSBasicSettings::AlertMessageType::Error, ui->baseResolution);
	}

	if (!ConvertResText(QT_TO_UTF8(ui->outputResolution_2->lineEdit()->text()), cx, cy)) {
		//PRISM/Liu.Haibin/20200413/#None/limit resolution
		QString text = QTStr(INVALID_RES_STR).arg(g_maxRolutionSize ? QString::number(g_maxRolutionSize).toStdString().c_str() : QString::number(RESOLUTION_SIZE_MAX).toStdString().c_str());
		settings->updateAlertMessage(PLSBasicSettings::AlertMessageType::Error, ui->outputResolution_2, text);
		result = false;
	} else {
		settings->clearAlertMessage(PLSBasicSettings::AlertMessageType::Error, ui->outputResolution_2);
	}
	return result;
}

void PLSBasicSettings::RecalcOutputResPixels(const char *resText)
{
	uint32_t newCX;
	uint32_t newCY;

	ConvertResText(resText, newCX, newCY);
	if (newCX && newCY) {
		outputCX = newCX;
		outputCY = newCY;
	}
}

void PLSBasicSettings::updateLabelSize(double dpi)
{
	{
		// setLabelLimitedExclude<QWidget, QWidget>("general page", PLSDpiHelper::calculate(dpi, 170), {GENERAL_PAGE_FORMLABELS}, {ui->label});
	}

	{
		QList<QWidget *> labels{OUTPUT_PAGE_FORMLABELS};
		if (streamEncoderProps) {
			int minimumHeight = streamEncoderProps->QScrollArea::widget()->minimumSizeHint().height();
			streamEncoderProps->setMinimumHeight(minimumHeight);
			labels.append(streamEncoderProps->findChildren<QWidget *>(OBJECT_NAME_FORMLABEL));
		}
		if (recordEncoderProps) {
			int minimumHeight = recordEncoderProps->QScrollArea::widget()->minimumSizeHint().height();
			recordEncoderProps->setMinimumHeight(minimumHeight);
			labels.append(recordEncoderProps->findChildren<QWidget *>(OBJECT_NAME_FORMLABEL));
		}
		setLabelLimited("output page", PLSDpiHelper::calculate(dpi, 170), labels);
	}

	{
		setLabelLimited<OBSSourceLabel, QWidget>("audio page", PLSDpiHelper::calculate(dpi, 170), ui->audioPage->findChildren<OBSSourceLabel *>("audioHotkeyFormLabel"),
							 {AUDIO_PAGE_FORMLABELS});
	}

	{
		setLabelLimited<QWidget>("view page", PLSDpiHelper::calculate(dpi, 170), {VIEW_PAGE_FORMLABELS});
	}

	{
		setLabelLimited<QWidget>("source page", PLSDpiHelper::calculate(dpi, 170), {SOURCE_PAGE_FORMLABELS});
	}

	{
		setLabelLimited<PLSHotkeyLabel, QWidget>("hotkey page", PLSDpiHelper::calculate(dpi, 170), ui->hotkeyPage->findChildren<PLSHotkeyLabel *>("hotkeyHotkeyFormLabel"),
							 {ui->hotkeyFocusTypeLabel, hotkeyFilterLabel});
	}
}

void PLSBasicSettings::on_filenameFormatting_textEdited(const QString &text)
{
#ifdef __APPLE__
	size_t invalidLocation = text.toStdString().find_first_of(":");
#elif _WIN32
	size_t invalidLocation = text.toStdString().find_first_of("<>:\"|?*");
#else
	size_t invalidLocation = string::npos;
	UNUSED_PARAMETER(text);
#endif

	if (invalidLocation != string::npos)
		ui->filenameFormatting->backspace();
}

void PLSBasicSettings::on_outputResolution_editTextChanged(const QString &text)
{
	if (!loading && ValidResolutions(this, ui.get())) {
		RecalcOutputResPixels(QT_TO_UTF8(text));
	}

	if (ui->outputResolution_2->lineEdit()->text() != text) {
		ui->outputResolution_2->lineEdit()->setText(text);
	}
}

void PLSBasicSettings::on_outputResolution_2_editTextChanged(const QString &text)
{
	if (!loading && ValidResolutions(this, ui.get())) {
		RecalcOutputResPixels(QT_TO_UTF8(text));
	}

	if (ui->outputResolution->lineEdit()->text() != text) {
		ui->outputResolution->lineEdit()->setText(text);
	}
}

void PLSBasicSettings::on_baseResolution_editTextChanged(const QString &text)
{
	if (!loading && ValidResolutions(this, ui.get())) {
		QString baseResolution = text;
		uint32_t cx, cy;

		ConvertResText(QT_TO_UTF8(baseResolution), cx, cy);
		ResetDownscales(cx, cy, true);
	}
}

void PLSBasicSettings::GeneralChanged()
{
	if (!loading) {
		generalChanged = true;
		QObject *sender = this->sender();
		sender->setProperty("changed", QVariant(true));
		componentValueChanged(getPageOfSender(), sender);
		EnableApplyButton(true);
		updateAlertMessage();
	}
}

void PLSBasicSettings::Stream1Changed()
{
	if (!loading) {
		stream1Changed = true;
		QObject *sender = this->sender();
		sender->setProperty("changed", QVariant(true));
		componentValueChanged(getPageOfSender(), sender);
		EnableApplyButton(true);
	}
}

void PLSBasicSettings::OutputsChanged()
{
	if (!loading) {
		outputsChanged = true;
		QObject *sender = this->sender();
		sender->setProperty("changed", QVariant(true));
		componentValueChanged(getPageOfSender(), sender);
		EnableApplyButton(true);
		UpdateAutomaticReplayBufferCheckboxes();
	}
}

void PLSBasicSettings::AudioChanged()
{
	if (!loading) {
		audioChanged = true;
		QObject *sender = this->sender();
		sender->setProperty("changed", QVariant(true));
		componentValueChanged(getPageOfSender(), sender);
		EnableApplyButton(true);
	}
}

void PLSBasicSettings::AudioChangedRestart()
{
	if (!loading) {
		audioChanged = true;
		QObject *sender = this->sender();
		QWidget *page = getPageOfSender();

		int currentChannelIndex = ui->channelSetup->currentIndex();
		int currentSampleRateIndex = ui->sampleRate->currentIndex();
		if (currentChannelIndex != channelIndex || currentSampleRateIndex != sampleRateIndex) {
			updateAlertMessage(AlertMessageType::Error, page, QTStr("Basic.Settings.ProgramRestart"));
		} else {
			clearAlertMessage(AlertMessageType::Error, page);
		}
		sender->setProperty("changed", QVariant(true));
		componentValueChanged(page, sender);
		EnableApplyButton(true);
	}
}

void PLSBasicSettings::ReloadAudioSources()
{
	LoadAudioSources();
}

#define MULTI_CHANNEL_WARNING "Basic.Settings.Audio.MultichannelWarning"

void PLSBasicSettings::SpeakerLayoutChanged(int idx)
{
	QString speakerLayoutQstr = ui->channelSetup->itemText(idx);
	std::string speakerLayout = QT_TO_UTF8(speakerLayoutQstr);
	bool surround = IsSurround(speakerLayout.c_str());

	if (surround) {
		updateAlertMessage(AlertMessageType::Warning, ui->channelSetup, QTStr(MULTI_CHANNEL_WARNING ".Enabled") + QStringLiteral("\n\n") + QTStr(MULTI_CHANNEL_WARNING));
		if (pls_is_immersive_audio()) {
			PopulateAACBitrates({ui->simpleOutputABitrate, ui->advOutTrackStereoBitrate, ui->advOutTrackImmersiveBitrate});
			RestrictResetBitrates({ui->advOutTrackStereoBitrate}, 384);
		} else {
			PopulateAACBitrates({ui->simpleOutputABitrate, ui->advOutTrack1Bitrate, ui->advOutTrack2Bitrate, ui->advOutTrack3Bitrate, ui->advOutTrack4Bitrate, ui->advOutTrack5Bitrate,
					     ui->advOutTrack6Bitrate});
		}

	} else {
		/*
		 * Reset audio bitrate for simple and adv mode, update list of
		 * bitrates and save setting.
		 */
		clearAlertMessage(AlertMessageType::Warning, ui->channelSetup);
		if (pls_is_immersive_audio()) {
			RestrictResetBitrates({ui->simpleOutputABitrate, ui->advOutTrackStereoBitrate, ui->advOutTrackImmersiveBitrate}, 384);
		} else {
			RestrictResetBitrates({ui->simpleOutputABitrate, ui->advOutTrack1Bitrate, ui->advOutTrack2Bitrate, ui->advOutTrack3Bitrate, ui->advOutTrack4Bitrate, ui->advOutTrack5Bitrate,
					       ui->advOutTrack6Bitrate},
					      320);
		}

		SaveCombo(ui->simpleOutputABitrate, "SimpleOutput", "ABitrate");
		SaveCombo(ui->advOutTrack1Bitrate, "AdvOut", "Track1Bitrate");
		SaveCombo(ui->advOutTrack2Bitrate, "AdvOut", "Track2Bitrate");
		SaveCombo(ui->advOutTrack3Bitrate, "AdvOut", "Track3Bitrate");
		SaveCombo(ui->advOutTrack4Bitrate, "AdvOut", "Track4Bitrate");
		SaveCombo(ui->advOutTrack5Bitrate, "AdvOut", "Track5Bitrate");
		SaveCombo(ui->advOutTrack6Bitrate, "AdvOut", "Track6Bitrate");
		SaveCombo(ui->advOutTrackStereoBitrate, "AdvOut", "TrackStereoBitrate");
		SaveCombo(ui->advOutTrackImmersiveBitrate, "AdvOut", "TrackImmersiveBitrate");
	}
}

/*
 * resets current bitrate if too large and restricts the number of bitrates
 * displayed when multichannel OFF
 */

void RestrictResetBitrates(initializer_list<QComboBox *> boxes, int maxbitrate)
{
	for (auto box : boxes) {
		int idx = box->currentIndex();
		int max_bitrate = FindClosestAvailableAACBitrate(maxbitrate);
		int count = box->count();
		int max_idx = box->findText(QT_UTF8(std::to_string(max_bitrate).c_str()));

		for (int i = (count - 1); i > max_idx; i--)
			box->removeItem(i);

		if (idx > max_idx) {
			int default_bitrate = FindClosestAvailableAACBitrate(maxbitrate / 2);
			int default_idx = box->findText(QT_UTF8(std::to_string(default_bitrate).c_str()));

			box->setCurrentIndex(default_idx);
			box->setProperty("changed", QVariant(true));
		} else {
			box->setCurrentIndex(idx);
		}
	}
}

void PLSBasicSettings::VideoChangedRestart()
{
	if (!loading) {
		videoChanged = true;
		QObject *sender = this->sender();
		QWidget *page = getPageOfSender();
		updateAlertMessage(AlertMessageType::Error, page, QTStr("Basic.Settings.ProgramRestart"));
		sender->setProperty("changed", QVariant(true));
		componentValueChanged(page, sender);
		EnableApplyButton(true);
	}
}

void PLSBasicSettings::AdvancedChangedRestart()
{
	if (!loading) {
		advancedChanged = true;
		QObject *sender = this->sender();
		QWidget *page = getPageOfSender();
		updateAlertMessage(AlertMessageType::Error, page, QTStr("Basic.Settings.ProgramRestart"));
		sender->setProperty("changed", QVariant(true));
		componentValueChanged(page, sender);
		EnableApplyButton(true);
	}
}

void PLSBasicSettings::VideoChangedResolution()
{
	if (!loading && ValidResolutions(this, ui.get())) {
		videoChanged = true;
		QObject *sender = this->sender();
		sender->setProperty("changed", QVariant(true));
		componentValueChanged(getPageOfSender(), sender);
		EnableApplyButton(true);
	}
}

void PLSBasicSettings::VideoChanged()
{
	if (!loading) {
		videoChanged = true;
		QObject *sender = this->sender();
		sender->setProperty("changed", QVariant(true));
		componentValueChanged(getPageOfSender(), sender);
		EnableApplyButton(true);
	}
}

void PLSBasicSettings::HotkeysChanged()
{
	using namespace std;
	if (loading)
		return;

	hotkeysChanged = any_of(begin(hotkeys), end(hotkeys), [](const pair<bool, QPointer<PLSHotkeyWidget>> &hotkey) {
		const auto &hw = *hotkey.second;
		return hw.Changed();
	});

	if (hotkeysChanged) {
		asyncNotifyComponentValueChanged(getPageOfSender(), sender());
		EnableApplyButton(true);
	}
}

void PLSBasicSettings::ReloadHotkeys(obs_hotkey_id ignoreKey)
{
	LoadHotkeySettings(ignoreKey);
	updateLabelSize(PLSDpiHelper::getDpi(this));
}

void PLSBasicSettings::AdvancedChanged()
{
	if (!loading) {
		advancedChanged = true;
		QObject *sender = this->sender();
		sender->setProperty("changed", QVariant(true));
		componentValueChanged(getPageOfSender(), sender);
		EnableApplyButton(true);
	}
}

void PLSBasicSettings::AdvOutRecCheckWarnings()
{
	auto Checked = [](QCheckBox *box) { return box->isChecked() ? 1 : 0; };

	uint32_t tracks =
		Checked(ui->advOutRecTrack1) + Checked(ui->advOutRecTrack2) + Checked(ui->advOutRecTrack3) + Checked(ui->advOutRecTrack4) + Checked(ui->advOutRecTrack5) + Checked(ui->advOutRecTrack6);
	if (pls_is_immersive_audio()) {
		tracks = ui->immersiveAudioStreaming->isChecked() ? 2 : 1;
	}
	clearAlertMessage(AlertMessageType::Warning, ui->advOutRecEncoder, false);
	clearAlertMessage(AlertMessageType::Error, ui->advOutRecFormat, false);
	clearAlertMessage(AlertMessageType::Warning, ui->advOutRecFormat, false);

	QString recEncoderID;
	if (0 == ui->advOutRecEncoder->currentIndex()) { // use stream's encoder
		// remove Warning: Recordings cannot be paused if the recording encoder is set to \"(Use stream encoder)\"
		// not support pause recordings current version
		// updateAlertMessage(AlertMessageType::Warning, ui->advOutRecEncoder, QTStr("OutputWarnings.CannotPause"));

		recEncoderID = GetComboData(ui->advOutEncoder);

	} else {
		recEncoderID = GetComboData(ui->advOutRecEncoder);
	}

	if (ui->advOutRecFormat->currentText().compare("flv") == 0) {
		ui->advRecTrackWidget->setCurrentWidget(ui->flvTracks);
		if (!recEncoderID.isEmpty()) {
			const char *codec = obs_get_encoder_codec(recEncoderID.toStdString().c_str());
			if (codec && (0 == strcmp(codec, "hevc"))) {
				updateAlertMessage(AlertMessageType::Warning, ui->advOutRecEncoder, QTStr("Hevc.alert.unsupport.FLV"));
			}
		}
	} else {
		ui->advRecTrackWidget->setCurrentWidget(ui->recTracks);
		if (tracks == 0) {
			updateAlertMessage(AlertMessageType::Error, ui->advOutRecFormat, QTStr("OutputWarnings.NoTracksSelected"));
		}
	}

	if (ui->advOutRecFormat->currentText().compare("mp4") == 0 || ui->advOutRecFormat->currentText().compare("mov") == 0) {
		ui->autoRemux->setText(QTStr("Basic.Settings.Advanced.AutoRemux") + " " + QTStr("Basic.Settings.Advanced.AutoRemux.MP4"));
		updateAlertMessage(AlertMessageType::Warning, ui->advOutRecFormat, QTStr("OutputWarnings.MP4Recording"));
	} else {
		ui->autoRemux->setText(QTStr("Basic.Settings.Advanced.AutoRemux"));
	}

	updateAlertMessage();
}

void PLSBasicSettings::AdvOutStreamEncoderCheckWarnings()
{
	clearAlertMessage(AlertMessageType::Warning, ui->advOutEncoder, false);

	QString encoder = GetComboData(ui->advOutEncoder);
	if (!encoder.isEmpty()) {
		const char *codec = obs_get_encoder_codec(encoder.toStdString().c_str());
		if (0 == strcmp(codec, "hevc")) {
			updateAlertMessage(AlertMessageType::Warning, ui->advOutEncoder, QTStr("Hevc.tip.vlive"));
		}
	}

	updateAlertMessage();
}

static inline QString MakeMemorySizeString(int bitrate, int seconds)
{
	QString str = QTStr("Basic.Settings.Advanced.StreamDelay.MemoryUsage");
	int megabytes = bitrate * seconds / 1000 / 8;

	return str.arg(QString::number(megabytes));
}

void PLSBasicSettings::UpdateSimpleOutStreamDelayEstimate()
{
	int seconds = ui->streamDelaySec->value();
	int vBitrate = ui->simpleOutputVBitrate->value();
	int aBitrate = ui->simpleOutputABitrate->currentText().toInt();

	QString msg = MakeMemorySizeString(vBitrate + aBitrate, seconds);

	ui->streamDelayInfo->setText(msg);
}

void PLSBasicSettings::UpdateAdvOutStreamDelayEstimate()
{
	if (!streamEncoderProps)
		return;

	OBSData settings = streamEncoderProps->GetSettings();
	int trackIndex = config_get_int(main->Config(), "AdvOut", "TrackIndex");
	int immersiveTrackIndex = config_get_int(main->Config(), "AdvOut", "ImmersiveTrackIndex");
	QString aBitrateText;

	if (pls_is_immersive_audio()) {
		switch (immersiveTrackIndex) {
		case 1:
			aBitrateText = ui->advOutTrackStereoBitrate->currentText();
			break;
		case 2:
			aBitrateText = ui->advOutTrackImmersiveBitrate->currentText();
			break;
		}
	} else {

		switch (trackIndex) {
		case 1:
			aBitrateText = ui->advOutTrack1Bitrate->currentText();
			break;
		case 2:
			aBitrateText = ui->advOutTrack2Bitrate->currentText();
			break;
		case 3:
			aBitrateText = ui->advOutTrack3Bitrate->currentText();
			break;
		case 4:
			aBitrateText = ui->advOutTrack4Bitrate->currentText();
			break;
		case 5:
			aBitrateText = ui->advOutTrack5Bitrate->currentText();
			break;
		case 6:
			aBitrateText = ui->advOutTrack6Bitrate->currentText();
			break;
		}
	}

	int seconds = ui->streamDelaySec->value();
	int vBitrate = (int)obs_data_get_int(settings, "bitrate");
	int aBitrate = aBitrateText.toInt();

	QString msg = MakeMemorySizeString(vBitrate + aBitrate, seconds);

	ui->streamDelayInfo->setText(msg);
}

void PLSBasicSettings::UpdateStreamDelayEstimate()
{
	if (ui->outputMode->currentIndex() == 0)
		UpdateSimpleOutStreamDelayEstimate();
	else
		UpdateAdvOutStreamDelayEstimate();

	UpdateAutomaticReplayBufferCheckboxes();
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

void PLSBasicSettings::FillSimpleRecordingValues()
{
#define ADD_QUALITY(str) ui->simpleOutRecQuality->addItem(QTStr("Basic.Settings.Output.Simple.RecordingQuality." str), QString(str));
#define ENCODER_STR(str) QTStr("Basic.Settings.Output.Simple.Encoder." str)
	ADD_QUALITY("Stream");
	ADD_QUALITY("Small");
	ADD_QUALITY("HQ");
	ADD_QUALITY("Lossless");

	ui->simpleOutRecEncoder->addItem(ENCODER_STR("Software"), QString(SIMPLE_ENCODER_X264));
	ui->simpleOutRecEncoder->addItem(ENCODER_STR("SoftwareLowCPU"), QString(SIMPLE_ENCODER_X264_LOWCPU));
	if (EncoderAvailable("obs_qsv11"))
		ui->simpleOutRecEncoder->addItem(ENCODER_STR("Hardware.QSV"), QString(SIMPLE_ENCODER_QSV));
	if (EncoderAvailable("ffmpeg_nvenc"))
		ui->simpleOutRecEncoder->addItem(ENCODER_STR("Hardware.NVENC"), QString(SIMPLE_ENCODER_NVENC));
	if (EncoderAvailable("amd_amf_h264"))
		ui->simpleOutRecEncoder->addItem(ENCODER_STR("Hardware.AMD"), QString(SIMPLE_ENCODER_AMD));
#undef ADD_QUALITY
}

void PLSBasicSettings::FillSimpleStreamingValues()
{
	ui->simpleOutStrEncoder->addItem(ENCODER_STR("Software"), QString(SIMPLE_ENCODER_X264));
	if (EncoderAvailable("obs_qsv11"))
		ui->simpleOutStrEncoder->addItem(ENCODER_STR("Hardware.QSV"), QString(SIMPLE_ENCODER_QSV));
	if (EncoderAvailable("ffmpeg_nvenc"))
		ui->simpleOutStrEncoder->addItem(ENCODER_STR("Hardware.NVENC"), QString(SIMPLE_ENCODER_NVENC));
	if (EncoderAvailable("amd_amf_h264"))
		ui->simpleOutStrEncoder->addItem(ENCODER_STR("Hardware.AMD"), QString(SIMPLE_ENCODER_AMD));
#undef ENCODER_STR
}

void PLSBasicSettings::FillAudioMonitoringDevices()
{
	QComboBox *cb = ui->monitoringDevice;

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

void PLSBasicSettings::SimpleRecordingQualityChanged()
{
	QString qual = ui->simpleOutRecQuality->currentData().toString();
	bool streamQuality = qual == "Stream";
	bool losslessQuality = !streamQuality && qual == "Lossless";

	bool showEncoder = !streamQuality && !losslessQuality;
	ui->simpleOutRecEncoder->setVisible(showEncoder);
	ui->simpleOutRecEncoderLabel->setVisible(showEncoder);
	ui->simpleOutRecFormat->setVisible(!losslessQuality);
	ui->simpleOutRecFormatLabel->setVisible(!losslessQuality);

	SimpleRecordingEncoderChanged();
	SimpleReplayBufferChanged();
}

void PLSBasicSettings::SimpleStreamingEncoderChanged()
{
	QString encoder = ui->simpleOutStrEncoder->currentData().toString();
	QString preset;
	const char *defaultPreset = nullptr;

	ui->simpleOutPreset->clear();

	if (encoder == SIMPLE_ENCODER_QSV) {
		ui->simpleOutPreset->addItem("speed", "speed");
		ui->simpleOutPreset->addItem("balanced", "balanced");
		ui->simpleOutPreset->addItem("quality", "quality");

		defaultPreset = "balanced";
		preset = curQSVPreset;

	} else if (encoder == SIMPLE_ENCODER_NVENC) {
		obs_properties_t *props = obs_get_encoder_properties("ffmpeg_nvenc");

		obs_property_t *p = obs_properties_get(props, "preset");
		size_t num = obs_property_list_item_count(p);
		for (size_t i = 0; i < num; i++) {
			const char *name = obs_property_list_item_name(p, i);
			const char *val = obs_property_list_item_string(p, i);

			/* bluray is for ideal bluray disc recording settings,
			 * not streaming */
			if (strcmp(val, "bd") == 0)
				continue;
			/* lossless should of course not be used to stream */
			if (astrcmp_n(val, "lossless", 8) == 0)
				continue;

			ui->simpleOutPreset->addItem(QT_UTF8(name), val);
		}

		obs_properties_destroy(props);

		defaultPreset = "default";
		preset = curNVENCPreset;

	} else if (encoder == SIMPLE_ENCODER_AMD) {
		ui->simpleOutPreset->addItem("Speed", "speed");
		ui->simpleOutPreset->addItem("Balanced", "balanced");
		ui->simpleOutPreset->addItem("Quality", "quality");

		defaultPreset = "balanced";
		preset = curAMDPreset;
	} else {
		ui->simpleOutPreset->addItem("ultrafast", "ultrafast");
		ui->simpleOutPreset->addItem("superfast", "superfast");
		ui->simpleOutPreset->addItem("veryfast", "veryfast");
		ui->simpleOutPreset->addItem("faster", "faster");
		ui->simpleOutPreset->addItem("fast", "fast");
		ui->simpleOutPreset->addItem("medium", "medium");
		ui->simpleOutPreset->addItem("slow", "slow");
		ui->simpleOutPreset->addItem("slower", "slower");

		defaultPreset = "veryfast";
		preset = curPreset;
	}

	int idx = ui->simpleOutPreset->findData(QVariant(preset));
	if (idx == -1)
		idx = ui->simpleOutPreset->findData(QVariant(defaultPreset));

	ui->simpleOutPreset->setCurrentIndex(idx);
}

#define ESTIMATE_STR "Basic.Settings.Output.ReplayBuffer.Estimate"
#define ESTIMATE_UNKNOWN_STR "Basic.Settings.Output.ReplayBuffer.EstimateUnknown"

void PLSBasicSettings::UpdateAutomaticReplayBufferCheckboxes()
{
	bool state = false;
	switch (ui->outputMode->currentIndex()) {
	case 0:
		state = ui->simpleReplayBuf->isChecked();
		break;
	case 1:
		state = ui->advReplayBuf->isChecked();
		break;
	}
	ui->replayWhileStreaming->setEnabled(state);
	ui->keepReplayStreamStops->setEnabled(state && ui->replayWhileStreaming->isChecked());

	if (!loading && outputsChanged && state) {
		hotkeysChanged = true;
	}
}

void PLSBasicSettings::SimpleReplayBufferChanged()
{
	QString qual = ui->simpleOutRecQuality->currentData().toString();
	bool replayBufferEnabled = ui->simpleReplayBuf->isChecked();
	bool lossless = qual == "Lossless";
	bool streamQuality = qual == "Stream";

	ui->simpleRBMegsMax->setVisible(!streamQuality);
	ui->simpleRBMegsMaxLabel->setVisible(!streamQuality);

	int vbitrate = ui->simpleOutputVBitrate->value();
	int abitrate = ui->simpleOutputABitrate->currentText().toInt();
	int seconds = ui->simpleRBSecMax->value();

	int64_t memMB = int64_t(seconds) * int64_t(vbitrate + abitrate) * 1000 / 8 / 1024 / 1024;
	if (memMB < 1)
		memMB = 1;

	if (streamQuality)
		ui->simpleRBEstimate->setText(QTStr(ESTIMATE_STR).arg(QString::number(int(memMB))));
	else
		ui->simpleRBEstimate->setText(QTStr(ESTIMATE_UNKNOWN_STR));

	ui->replayBufferGroupBox->setVisible(!lossless && replayBufferEnabled && ui->outputMode->currentIndex() == 0);
	ui->simpleReplayBuf->setVisible(!lossless);

	UpdateAutomaticReplayBufferCheckboxes();

	if (replayBufferEnabled) {
		QString text = tr("Basic.Settings.Output.ReplayBuffer.HotkeyMessage")
				       .arg(replayBufferHotkeyWidget ? replayBufferHotkeyWidget->getHotkeyText().remove(' ') : config_get_string(main->Config(), "Others", "Hotkeys.ReplayBuffer"));
		ui->replayBufferHotkeyMessage1->setText(text);
	}

	ui->scrollAreaWidgetContents_3->adjustSize();
}

void PLSBasicSettings::AdvReplayBufferChanged()
{
	obs_data_t *settings;
	QString encoder = ui->advOutRecEncoder->currentText();
	bool useStream = QString::compare(encoder, TEXT_USE_STREAM_ENC) == 0;

	if (useStream && streamEncoderProps) {
		settings = streamEncoderProps->GetSettings();
	} else if (!useStream && recordEncoderProps) {
		settings = recordEncoderProps->GetSettings();
	} else {
		if (useStream)
			encoder = GetComboData(ui->advOutEncoder);
		settings = obs_encoder_defaults(encoder.toUtf8().constData());

		if (!settings)
			return;

		char encoderJsonPath[512];
		int ret = GetProfilePath(encoderJsonPath, sizeof(encoderJsonPath), "recordEncoder.json");
		if (ret > 0) {
			obs_data_t *data = obs_data_create_from_json_file_safe(encoderJsonPath, "bak");
			obs_data_apply(settings, data);
			obs_data_release(data);
		}
	}

	int vbitrate = (int)obs_data_get_int(settings, "bitrate");
	const char *rateControl = obs_data_get_string(settings, "rate_control");

	if (!rateControl)
		rateControl = "";

	bool lossless = strcmp(rateControl, "lossless") == 0 || ui->advOutRecType->currentIndex() == 1;
	bool replayBufferEnabled = ui->advReplayBuf->isChecked();

	int abitrate = 0;
	if (pls_is_immersive_audio()) {
		abitrate += ui->advOutTrackStereoBitrate->currentText().toInt();
		if (ui->immersiveAudioStreaming->isChecked())
			abitrate += ui->advOutTrackImmersiveBitrate->currentText().toInt();
	} else {
		if (ui->advOutRecTrack1->isChecked())
			abitrate += ui->advOutTrack1Bitrate->currentText().toInt();
		if (ui->advOutRecTrack2->isChecked())
			abitrate += ui->advOutTrack2Bitrate->currentText().toInt();
		if (ui->advOutRecTrack3->isChecked())
			abitrate += ui->advOutTrack3Bitrate->currentText().toInt();
		if (ui->advOutRecTrack4->isChecked())
			abitrate += ui->advOutTrack4Bitrate->currentText().toInt();
		if (ui->advOutRecTrack5->isChecked())
			abitrate += ui->advOutTrack5Bitrate->currentText().toInt();
		if (ui->advOutRecTrack6->isChecked())
			abitrate += ui->advOutTrack6Bitrate->currentText().toInt();
	}
	int seconds = ui->advRBSecMax->value();

	int64_t memMB = int64_t(seconds) * int64_t(vbitrate + abitrate) * 1000 / 8 / 1024 / 1024;
	if (memMB < 1)
		memMB = 1;

	bool varRateControl = (astrcmpi(rateControl, "CBR") == 0 || astrcmpi(rateControl, "VBR") == 0 || astrcmpi(rateControl, "ABR") == 0);
	if (vbitrate == 0)
		varRateControl = false;

	ui->advRBMegsMax->setVisible(!varRateControl);
	ui->advRBMegsMaxLabel->setVisible(!varRateControl);

	if (varRateControl)
		ui->advRBEstimate->setText(QTStr(ESTIMATE_STR).arg(QString::number(int(memMB))));
	else
		ui->advRBEstimate->setText(QTStr(ESTIMATE_UNKNOWN_STR));

	ui->advReplayBufferGroupBox->setVisible(!lossless && replayBufferEnabled);
	ui->advReplayBuf->setEnabled(!pls_is_living_or_recording() && !lossless);

	UpdateAutomaticReplayBufferCheckboxes();

	if (replayBufferEnabled) {
		QString text = tr("Basic.Settings.Output.ReplayBuffer.HotkeyMessage")
				       .arg(replayBufferHotkeyWidget ? replayBufferHotkeyWidget->getHotkeyText().remove(' ') : config_get_string(main->Config(), "Others", "Hotkeys.ReplayBuffer"));
		ui->replayBufferHotkeyMessage2->setText(text);
	}

	ui->advOutputReplayTab->adjustSize();
	ui->outputSettingsAdvReplayBufTab->adjustSize();
	ui->scrollAreaWidgetContents_3->adjustSize();
}

#define SIMPLE_OUTPUT_WARNING(str) QTStr("Basic.Settings.Output.Simple.Warn." str)

void PLSBasicSettings::SimpleRecordingEncoderChanged()
{
	QString qual = ui->simpleOutRecQuality->currentData().toString();
	bool advanced = ui->simpleOutAdvanced->isChecked();
	bool enforceBitrate = ui->simpleOutEnforce->isChecked() || !advanced;
	OBSService service;

	clearAlertMessage(AlertMessageType::Warning, ui->simpleOutRecQuality, false);
	clearAlertMessage(AlertMessageType::Warning, ui->simpleOutRecEncoder, false);
	clearAlertMessage(AlertMessageType::Warning, ui->simpleOutputVBitrate, false);
	clearAlertMessage(AlertMessageType::Warning, ui->simpleOutputABitrate, false);
	clearAlertMessage(AlertMessageType::Warning, ui->simpleOutRecFormat, false);

	if (stream1Changed) {
		service = SpawnTempService();
	} else {
		service = main->GetService();
	}

	if (enforceBitrate && service) {
		obs_data_t *videoSettings = obs_data_create();
		obs_data_t *audioSettings = obs_data_create();
		int oldVBitrate = ui->simpleOutputVBitrate->value();
		int oldABitrate = ui->simpleOutputABitrate->currentText().toInt();
		obs_data_set_int(videoSettings, "bitrate", oldVBitrate);
		obs_data_set_int(audioSettings, "bitrate", oldABitrate);

		obs_service_apply_encoder_settings(service, videoSettings, audioSettings);

		int newVBitrate = obs_data_get_int(videoSettings, "bitrate");
		int newABitrate = obs_data_get_int(audioSettings, "bitrate");

		if (newVBitrate < oldVBitrate) {
			updateAlertMessage(AlertMessageType::Warning, ui->simpleOutputVBitrate, SIMPLE_OUTPUT_WARNING("VideoBitrate").arg(newVBitrate));
		}
		if (newABitrate < oldABitrate) {
			updateAlertMessage(AlertMessageType::Warning, ui->simpleOutputABitrate, SIMPLE_OUTPUT_WARNING("AudioBitrate").arg(newABitrate));
		}

		obs_data_release(videoSettings);
		obs_data_release(audioSettings);
	}

	if (qual == "Lossless") {
		updateAlertMessage(AlertMessageType::Warning, ui->simpleOutRecQuality, SIMPLE_OUTPUT_WARNING("Lossless"));
		updateAlertMessage(AlertMessageType::Warning, ui->simpleOutRecEncoder, SIMPLE_OUTPUT_WARNING("Encoder"));
	} else if (qual != "Stream") {
		QString enc = ui->simpleOutRecEncoder->currentData().toString();
		QString streamEnc = ui->simpleOutStrEncoder->currentData().toString();
		bool x264RecEnc = (enc == SIMPLE_ENCODER_X264 || enc == SIMPLE_ENCODER_X264_LOWCPU);

		if (streamEnc == SIMPLE_ENCODER_X264 && x264RecEnc) {
			updateAlertMessage(AlertMessageType::Warning, ui->simpleOutRecEncoder, SIMPLE_OUTPUT_WARNING("Encoder"));
		}
	} else {
		// updateAlertMessage(AlertMessageType::Warning, ui->simpleOutRecQuality, SIMPLE_OUTPUT_WARNING("CannotPause"));
	}

	if (qual != "Lossless" && (ui->simpleOutRecFormat->currentText().compare("mp4") == 0 || ui->simpleOutRecFormat->currentText().compare("mov") == 0)) {
		ui->autoRemux->setText(QTStr("Basic.Settings.Advanced.AutoRemux") + " " + QTStr("Basic.Settings.Advanced.AutoRemux.MP4"));
		updateAlertMessage(AlertMessageType::Warning, ui->simpleOutRecFormat, QTStr("OutputWarnings.MP4Recording"));
	} else {
		ui->autoRemux->setText(QTStr("Basic.Settings.Advanced.AutoRemux"));
	}

	updateAlertMessage();
}

void PLSBasicSettings::SurroundWarning(int idx)
{
	if (idx == lastChannelSetupIdx || idx == -1)
		return;

	if (loading) {
		lastChannelSetupIdx = idx;
		return;
	}

	QString speakerLayoutQstr = ui->channelSetup->itemText(idx);
	bool surround = IsSurround(QT_TO_UTF8(speakerLayoutQstr));

	QString lastQstr = ui->channelSetup->itemText(lastChannelSetupIdx);
	bool wasSurround = IsSurround(QT_TO_UTF8(lastQstr));

	if (surround && !wasSurround) {
		QString warningString =
			QTStr("Basic.Settings.ProgramRestart") + QStringLiteral("\n\n") + QTStr(MULTI_CHANNEL_WARNING) + QStringLiteral("\n\n") + QTStr(MULTI_CHANNEL_WARNING ".Confirm");

		PLSAlertView::Button button = PLSMessageBox::question(this, QTStr(MULTI_CHANNEL_WARNING ".Title"), warningString);
		if (button != PLSAlertView::Button::Yes) {
			QMetaObject::invokeMethod(ui->channelSetup, "setCurrentIndex", Qt::QueuedConnection, Q_ARG(int, lastChannelSetupIdx));
			return;
		}
	}

	lastChannelSetupIdx = idx;
}

void PLSBasicSettings::SimpleRecordingQualityLosslessWarning(int idx)
{
	if (idx == lastSimpleRecQualityIdx || idx == -1)
		return;

	QString qual = ui->simpleOutRecQuality->itemData(idx).toString();

	if (loading) {
		lastSimpleRecQualityIdx = idx;
		return;
	}

	if (qual == "Lossless") {
		QString warningString = SIMPLE_OUTPUT_WARNING("Lossless") + QString("\n\n") + SIMPLE_OUTPUT_WARNING("Lossless.Msg");

		PLSAlertView::Button button = PLSMessageBox::question(this, SIMPLE_OUTPUT_WARNING("Lossless.Title"), warningString);
		if (button != PLSAlertView::Button::Yes) {
			QMetaObject::invokeMethod(ui->simpleOutRecQuality, "setCurrentIndex", Qt::QueuedConnection, Q_ARG(int, lastSimpleRecQualityIdx));
			return;
		}
	}

	lastSimpleRecQualityIdx = idx;
}

void PLSBasicSettings::on_disableOSXVSync_clicked()
{
	componentValueChanged(getPageOfSender(), sender());

#ifdef __APPLE__
	if (!loading) {
		bool disable = ui->disableOSXVSync->isChecked();
		ui->resetOSXVSync->setEnabled(disable);
	}
#endif
}

void PLSBasicSettings::on_resetButton_clicked()
{
	PLS_UI_STEP(SETTING_MODULE, "Reset Button", ACTION_CLICK);

	if (PLSAlertView::question(this, tr("Confirm"), tr("Basic.Settings.Reset.Question"), PLSAlertView::Button::Yes | PLSAlertView::Button::No) != PLSAlertView::Button::Yes) {
		return;
	}

	ResetSettings();
	LoadSettings(false);
	ClearChanged();

	updateLabelSize(PLSDpiHelper::getDpi(this));
}

QIcon PLSBasicSettings::GetGeneralIcon() const
{
	return generalIcon;
}

QIcon PLSBasicSettings::GetStreamIcon() const
{
	return streamIcon;
}

QIcon PLSBasicSettings::GetOutputIcon() const
{
	return outputIcon;
}

QIcon PLSBasicSettings::GetAudioIcon() const
{
	return audioIcon;
}

QIcon PLSBasicSettings::GetVideoIcon() const
{
	return videoIcon;
}

QIcon PLSBasicSettings::GetHotkeysIcon() const
{
	return hotkeysIcon;
}

QIcon PLSBasicSettings::GetAdvancedIcon() const
{
	return advancedIcon;
}

void PLSBasicSettings::SetGeneralIcon(const QIcon &icon)
{
	ui->listWidget->item(0)->setIcon(icon);
}

void PLSBasicSettings::SetStreamIcon(const QIcon &icon)
{
	ui->listWidget->item(1)->setIcon(icon);
}

void PLSBasicSettings::SetOutputIcon(const QIcon &icon)
{
	ui->listWidget->item(2)->setIcon(icon);
}

void PLSBasicSettings::SetAudioIcon(const QIcon &icon)
{
	ui->listWidget->item(3)->setIcon(icon);
}

void PLSBasicSettings::SetVideoIcon(const QIcon &icon)
{
	ui->listWidget->item(4)->setIcon(icon);
}

void PLSBasicSettings::SetHotkeysIcon(const QIcon &icon)
{
	ui->listWidget->item(5)->setIcon(icon);
}

void PLSBasicSettings::SetAdvancedIcon(const QIcon &icon)
{
	ui->listWidget->item(6)->setIcon(icon);
}

void PLSBasicSettings::onAsyncUpdateReplayBufferHotkeyMessage(void *output, PLSHotkeyWidget *hw)
{
	if (main->outputHandler && main->outputHandler->replayBuffer && main->outputHandler->replayBuffer == output) {
		replayBufferHotkeyWidget = hw;
		hw->setClearBtnDisabled();

		QString text = tr("Basic.Settings.Output.ReplayBuffer.HotkeyMessage").arg(replayBufferHotkeyWidget->getHotkeyText().remove(' '));
		ui->replayBufferHotkeyMessage1->setText(text);
		ui->replayBufferHotkeyMessage2->setText(text);
	}
}

int PLSBasicSettings::CurrentFLVTrack()
{
	if (ui->flvTrack1->isChecked())
		return 1;
	else if (ui->flvTrack2->isChecked())
		return 2;
	else if (ui->flvTrack3->isChecked())
		return 3;
	else if (ui->flvTrack4->isChecked())
		return 4;
	else if (ui->flvTrack5->isChecked())
		return 5;
	else if (ui->flvTrack6->isChecked())
		return 6;

	return 1;
}

QWidget *PLSBasicSettings::getPageOfSender(QObject *sender) const
{
	QWidget *pages[] = {ui->generalPage, ui->outputPage, ui->audioPage, ui->viewPage, ui->sourcePage, ui->hotkeyPage};
	for (QObject *object = !sender ? this->sender() : sender; object != nullptr; object = object->parent()) {
		if (QWidget **pos = std::find(&pages[0], &pages[6], object); pos != &pages[6]) {
			return *pos;
		}
	}
	return nullptr;
}

void PLSBasicSettings::switchTo(const QString &tab, const QString &)
{
	if (tab == QStringLiteral("General")) {
		ui->listWidget->setCurrentRow(0);
	} else if (tab == QStringLiteral("Output")) {
		ui->listWidget->setCurrentRow(1);
	} else if (QStringLiteral("Video") == tab) {
		ui->listWidget->setCurrentRow(4);
	} else {
		ui->listWidget->setCurrentRow(0);
	}
}

void PLSBasicSettings::updateAlertMessage(AlertMessageType type, QWidget *widget, const QString &message)
{
	auto &alertMessages = type == AlertMessageType::Error ? errorAlertMessages : warningAlertMessages;
	auto iter = std::find_if(alertMessages.begin(), alertMessages.end(), [widget](const std::tuple<QWidget *, QLabel *> &a) { return std::get<0>(a) == widget; });
	if (iter != alertMessages.end()) {
		auto &v = *iter;
		std::get<1>(v)->setProperty("alertMessageType", type == AlertMessageType::Error ? "Error" : "Warning");
		std::get<1>(v)->setText(message);
	} else {
		QLabel *label = new QLabel(message);
		label->setObjectName("alertMessageLabel");
		label->setWordWrap(true);
		label->setProperty("alertMessageType", type == AlertMessageType::Error ? "Error" : "Warning");
		label->hide();

		if (type == AlertMessageType::Warning) {
			ui->alertMessageFrameLayout->addWidget(label);
		} else if (!alertMessages.isEmpty()) {
			ui->alertMessageFrameLayout->insertWidget(ui->alertMessageFrameLayout->indexOf(std::get<1>(alertMessages.last())), label);
		} else {
			ui->alertMessageFrameLayout->insertWidget(0, label);
		}

		alertMessages.append(std::make_tuple(widget, label));
	}

	updateAlertMessage();
}

void PLSBasicSettings::clearAlertMessage(AlertMessageType type, QWidget *widget, bool update)
{
	auto &alertMessages = type == AlertMessageType::Error ? errorAlertMessages : warningAlertMessages;
	auto iter = std::find_if(alertMessages.begin(), alertMessages.end(), [widget](const std::tuple<QWidget *, QLabel *> &a) { return std::get<0>(a) == widget; });
	if (iter != alertMessages.end()) {
		std::get<1>(*iter)->setText("");
	}

	if (update) {
		updateAlertMessage();
	}
}

static bool checkParent(QWidget *widget, QWidget *parent)
{
	if (!widget) {
		return false;
	}

	for (QObject *obj = widget; obj; obj = obj->parent()) {
		if (obj == parent) {
			return true;
		}
	}
	return false;
}

static bool checkOutputPageWidget(Ui::PLSBasicSettings *ui, QWidget *widget, QWidget *advCurrentTab)
{
	if (ui->outputMode->currentIndex() == 0) {
		if (checkParent(widget, ui->simpleOutputModeContainer)) {
			return true;
		} else if (checkParent(widget, ui->advOutputModeContainer)) {
			return false;
		}
		return true;
	} else {
		if (checkParent(widget, ui->simpleOutputModeContainer)) {
			return false;
		} else if (checkParent(widget, ui->advOutputModeContainer)) {
			return checkParent(widget, advCurrentTab);
		}
		return true;
	}
}

static bool isInCurrentPage(Ui::PLSBasicSettings *ui, QWidget *page, QWidget *widget, QWidget *advCurrentTab)
{
	if (page == ui->generalPage) {
		return ui->listWidget->currentRow() == 0;
	} else if (page == ui->outputPage) {
		return ui->listWidget->currentRow() == 1 && checkOutputPageWidget(ui, widget, advCurrentTab);
	} else if (page == ui->audioPage) {
		return ui->listWidget->currentRow() == 2;
	} else if (page == ui->viewPage) {
		return ui->listWidget->currentRow() == 3;
	} else if (page == ui->sourcePage) {
		return ui->listWidget->currentRow() == 4;
	} else if (page == ui->hotkeyPage) {
		return ui->listWidget->currentRow() == 5;
	} else {
		return false;
	}
}

template<typename getPageOfSenderFunc>
static void updateAlertMessage(QList<std::tuple<QWidget *, QLabel *>> &alertMessages, int &alertMessageCount, Ui::PLSBasicSettings *ui, getPageOfSenderFunc getPageOfSender, QWidget *advCurrentTab)
{
	for (auto &v : alertMessages) {
		QWidget *widget = std::get<0>(v);
		QLabel *label = std::get<1>(v);
		QWidget *page = getPageOfSender(widget);
		if (isInCurrentPage(ui, page, widget, advCurrentTab) && !label->text().isEmpty()) {
			++alertMessageCount;
			label->show();
		} else {
			label->hide();
		}
	}
}

void PLSBasicSettings::updateAlertMessage()
{
	int alertMessageCount = 0;
	auto getPageOfSender = [this](QWidget *widget) { return this->getPageOfSender(widget); };
	::updateAlertMessage(errorAlertMessages, alertMessageCount, ui.get(), getPageOfSender, outputSettingsAdvCurrentTab);
	::updateAlertMessage(warningAlertMessages, alertMessageCount, ui.get(), getPageOfSender, outputSettingsAdvCurrentTab);
	if (alertMessageCount > 0) {
		ui->alertMessageFrame->show();
		ui->alertMessageLayout->setContentsMargins(PLSDpiHelper::calculate(this, QMargins(20, 0, 20, 20)));
	} else {
		ui->alertMessageFrame->hide();
		ui->alertMessageLayout->setMargin(0);
	}
}

void PLSBasicSettings::cancel()
{
	ClearChanged();
	close();
}
