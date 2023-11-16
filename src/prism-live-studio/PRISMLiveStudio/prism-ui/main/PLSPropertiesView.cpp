#include <QFormLayout>
#include <QScrollBar>
#include <QLabel>
#include <QFont>
#include <QLineEdit>
#include <QSpinBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QStandardItem>
#include <QFileDialog>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QMenu>
#include <QMessageBox>
#include <QStackedWidget>
#include <QDir>
#include <QGroupBox>
#include <QObject>
#include <QDesktopServices>
#include "double-slider.hpp"
#include "slider-ignorewheel.hpp"
#include "spinbox-ignorewheel.hpp"
#include "qt-wrappers.hpp"
#include "plain-text-edit.hpp"
#include "obs-app.hpp"

#include <cstdlib>
#include <initializer_list>
#include <obs-data.h>
#include <obs.h>
#include <qtimer.h>
#include <string>

#include <qwidget.h>
#include "PLSPropertiesView.hpp"
#include "pls/pls-properties.h"
#include "pls/pls-obs-api.h"
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include "libutils-api.h"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "log/module_names.h"
#include "PLSLabel.h"
#include "PLSComboBox.h"
#include "PLSCheckBox.h"
#include "PLSColorDialogView.h"
#include "PLSPropertiesExtraUI.hpp"
#include "PLSRegionCapture.h"
#include "frontend-api.h"

#include "properties-view.cpp"
#include "PLSPushButton.h"
#include "PLSSpinBox.h"
#include "window-basic-settings.hpp"
#include "PLSCommonScrollBar.h"
#include "PLSRadioButton.h"
#include "PLSEdit.h"
#include "PLSGetPropertiesThread.h"
#include "PLSCommonFunc.h"

using namespace std;

#define PROPERTY_FLAG_BUTTON_WIDTH_FIXED 1 << 4
constexpr auto MAX_TM_TEXT_CONTENT_LENGTH = 200;

void PLSPropertiesView::AddProperty(obs_property_t *property, QFormLayout *layout)
{
	OBSPropertiesView::AddProperty(property, layout);

	obs_property_type type = obs_property_get_type(property);
	if ((pls_property_type)type < OBS_PROPERTY_BASE)
		return;

	if (!obs_property_visible(property))
		return;

	QLabel *label = nullptr;
	QWidget *widget = nullptr;
	bool warning = false;

	switch ((pls_property_type)type) {
	case PLS_PROPERTY_TIPS: //OBS_PROPERTY_MOBILE_GUIDER
		AddMobileGuider(property, layout);
		break;
	case PLS_PROPERTY_LINE: // OBS_PROPERTY_H_LINE:
		AddHLine(property, layout, label);
		break;
	case PLS_PROPERTY_BOOL_GROUP:
		AddRadioButtonGroup(property, layout);
		return;
	case PLS_PROPERTY_BUTTON_GROUP:
		AddButtonGroup(property, layout);
		break;
	case PLS_PROPERTY_CUSTOM_GROUP:
		AddCustomGroup(property, layout, label);
		break;
	case PLS_PROPERTY_BGM_LIST:
		AddMusicList(property, layout);
		break;
	case PLS_PROPERTY_BGM_TIPS:
		AddTips(property, layout);
		break;
	case PLS_PROPERTY_CHAT_TEMPLATE_LIST: //OBS_PROPERTY_CHAT_TEMPLATE_LIST:
		AddChatTemplateList(property, layout);
		break;
	case PLS_PROPERTY_CHAT_FONT_SIZE: //OBS_PROPERTY_CHAT_FONT_SIZE:
		AddChatFontSize(property, layout);
		break;
	case PLS_PROPERTY_TM_TAB: //OBS_PROPERTY_TM_TAB:
		AddTmTab(property, layout);
		break;
	case PLS_PROPERTY_TM_TEMPLATE_TAB: //OBS_PROPERTY_TM_TEMPLATE_TAB:
		AddTmTemplateTab(property, layout);
		break;
	case PLS_PROPERTY_TM_TEMPLATE_LIST: //OBS_PROPERTY_TM_TEMPLATE_LIST:
		AddTmTabTemplateList(property, layout);
		break;
	case PLS_PROPERTY_TM_DEFAULT_TEXT: //OBS_PROPERTY_TM_TEXT:
		AddDefaultText(property, layout, label);
		break;
	case PLS_PROPERTY_TM_TEXT_CONTENT:
		AddTmTextContent(property, layout);
		break;
	case PLS_PROPERTY_TM_TEXT:
		AddTmText(property, layout, label);
		break;
	case PLS_PROPERTY_TM_COLOR: //OBS_PROPERTY_TM_COLOR:
		AddTmColor(property, layout, label);
		break;
	case PLS_PROPERTY_TM_MOTION: // OBS_PROPERTY_TM_MOTION:
		AddTmMotion(property, layout, label);
		break;
	case PLS_PROPERTY_REGION_SELECT: // OBS_PROPERTY_REGION_SELECT:
		widget = AddSelectRegion(property, warning);
		break;
	case PLS_PROPERTY_IMAGE_GROUP: // OBS_PROPERTY_IMAGE_GROUP:
		AddImageGroup(property, layout, label);
		break;
	case PLS_PROPERTY_VISUALIZER_CUSTOM_GROUP: // OBS_PROPERTY_CUSTOM_GROUP:
		AddvirtualCustomGroup(property, layout, label);
		break;
	case PLS_PROPERTY_BOOL_LEFT: // OBS_PROPERTY_BOOL_LEFT:
		AddPrismCheckbox(property, layout, Qt::RightToLeft);
		break;
	case PLS_PROPERTY_VIRTUAL_BACKGROUND_CAMERA_STATE: //OBS_PROPERTY_CAMERA_VIRTUAL_BACKGROUND_STATE:
		AddCameraVirtualBackgroundState(property, layout, label);
		break;
	case PLS_PROPERTY_VIRTUAL_BACKGROUND_RESOURCE: //OBS_PROPERTY_VIRTUAL_BACKGROUND_RESOURCE:
	{
		QBoxLayout *bLayout = dynamic_cast<QBoxLayout *>(boxLayout);
		AddVirtualBackgroundResource(property, bLayout);
	} break;
	case PLS_PROPERTY_VIRTUAL_BACKGROUND_SWITCH: //OBS_PROPERTY_SWITCH:
		widget = AddSwitch(property, layout);
		break;
	case PLS_PROPERTY_MOBILE_HELP: //OBS_PROPERTY_MOBILE_HELP:
		AddMobileHelp(property, layout);
		break;
	case PLS_PROPERTY_MOBILE_NAME: //OBS_PROPERTY_MOBILE_NAME:
		widget = AddMobileName(property);
		break;
	case PLS_PROPERTY_MOBILE_STATE: //OBS_PROPERTY_MOBILE_STATUS:
		widget = AddMobileStatus(property);
		break;

	case PLS_PROPERTY_FONT_SIMPLE: //OBS_PROPERTY_FONT_SIMPLE:
		AddFontSimple(property, layout, label);
		break;
	case PLS_PROPERTY_COLOR_CHECKBOX: //OBS_PROPERTY_COLOR_CHECKBOX:
		AddColorCheckbox(property, layout, label);
		break;
	case PLS_PROPERTY_TEMPLATE_LIST: // OBS_PROPERTY_TEMPLATE_LIST :
		AddTemplateList(property, layout);
		break;
	case PLS_PROPERTY_COLOR_ALPHA_CHECKBOX: //OBS_PROPERTY_COLOR_ALPHA_CHECKBOX:
		AddColorAlphaCheckbox(property, layout, label);
		break;
	case PLS_PROPERTY_TEXT_CONTENT:
		widget = AddTextContent(property);
		break;

	default:
		Q_ASSERT_X(false, "AddProperty()", "new type need add ui by self");
		break;
	}

	OBSPropertiesView::updateUIWhenAfterAddProperty(property, layout, label, widget, warning);
}
void PLSPropertiesView::AddDefaultText(obs_property_t *prop, QFormLayout *layout, QLabel *&label) const
{
	layout->addItem(new QSpacerItem(10, PROPERTIES_VIEW_VERTICAL_SPACING_MIN, QSizePolicy::Fixed, QSizePolicy::Fixed));
	pls_unused(prop);
	label = pls_new<QLabel>(NO_PROPERTIES_STRING);
	layout->addRow(label);
}
QWidget *PLSPropertiesView::AddList(obs_property_t *prop, bool &warning)
{
	QWidget *subWid = OBSPropertiesView::AddList(prop, warning);
	bool isTimer = PROPERTY_FLAG_LIST_TIMER_LISTEN & pls_property_get_flags(prop);
	if (!isTimer) {
		return subWid;
	}

	auto combo = dynamic_cast<QComboBox *>(subWid);
	if (!combo) {
		return subWid;
	}

	children.pop_back();

	combo->setObjectName(common::OBJECT_NAME_COMBOBOX);
	PLSWidgetInfo *info = pls_new<PLSWidgetInfo>(this, prop, combo);
	connect(combo, SIGNAL(currentIndexChanged(int)), info, SLOT(ControlChanged()));
	children.emplace_back(info);

	QWidget *wid = pls_new<QWidget>();
	auto hlayout = pls_new<QHBoxLayout>(wid);
	hlayout->setContentsMargins(0, 0, 0, 0);
	hlayout->setSpacing(10);

	auto listenButton = pls_new<QPushButton>();
	listenButton->setObjectName("listenBtn");

	auto isChecked = obs_data_get_bool(settings, "listen_list_btn");
	listenButton->setProperty(common::PROPERTY_NAME_SOURCE_SELECT, isChecked);
	listenButton->setEnabled(obs_data_get_bool(settings, "listen_list_btn_enabeled"));

	listenButton->setToolTip(isChecked ? tr("timer.source.listen.btn.checked.tip") : tr("timer.source.listen.btn.no.checked.tip"));
	auto btnInfo = pls_new<PLSWidgetInfo>(this, prop, listenButton);
	connect(listenButton, SIGNAL(clicked()), btnInfo, SLOT(UserOperation()));
	connect(listenButton, SIGNAL(clicked()), btnInfo, SLOT(ControlChanged()));
	children.emplace_back(btnInfo);

	pls_flush_style(listenButton);

	hlayout->addWidget(subWid);
	hlayout->addWidget(listenButton);

	return wid;
}

void PLSPropertiesView::AddMobileGuider(obs_property_t *prop, QFormLayout *layout)
{
	const char *desc = obs_property_description(prop);
	QString value = desc;
	bool prismLensSource = isPrismLensOrMobileSource();
	if (prismLensSource || pls_is_equal("main.property.prism.lens.mac.audio.desc", desc)) {
		value = QTStr(desc);
	}

	obs_data_t *privateData = obs_data_create();
	if (value.isEmpty()) {
		value = pls_property_mobile_name_button_desc(prop);
	}

	AddSpacer(obs_property_get_type(prop), layout);

	auto *label = pls_new<QLabel>(value);
	label->setWordWrap(true);
	if (prismLensSource) {
		label->setObjectName("prismLensLabelGuide");
	} else {
		label->setObjectName("prismMobileLabelGuide");
	}

	const char *name = obs_property_name(prop);
	if (pls_is_equal(name, "main.property.prism.lens.audio")) {
		auto *keyLabel = pls_new<QLabel>(QTStr(name));
		keyLabel->setObjectName("formLabel");
		label->setObjectName("prismLensAudioLabel");
		layout->addItem(pls_new<QSpacerItem>(10, 15, QSizePolicy::Fixed, QSizePolicy::Fixed));
		layout->addRow(keyLabel, label);
	} else {
		layout->addRow(nullptr, label);
		if (prismLensSource) {
			label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
			layout->addItem(pls_new<QSpacerItem>(10, 15, QSizePolicy::Fixed, QSizePolicy::Fixed));
		}

		obs_data_release(privateData);
	}
}

void PLSPropertiesView::AddHLine(obs_property_t *prop, QFormLayout *layout, QLabel *&)
{
	auto w = pls_new<QWidget>(this);
	w->setObjectName("horiLine");

	AddSpacer(obs_property_get_type(prop), layout);
	if (layout)
		layout->addRow(w);
}
void PLSPropertiesView::AddRadioButtonGroup(obs_property_t *prop, QFormLayout *layout)
{
	auto name = obs_property_name(prop);
	const auto value = obs_data_get_int(settings, name);
	size_t count = pls_property_bool_group_item_count(prop);
	if (0 == count) {
		return;
	}

	auto hWidget = pls_new<QWidget>();

	auto hBtnLayout = pls_new<QHBoxLayout>(hWidget);
	hBtnLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hBtnLayout->setSpacing(20);
	hBtnLayout->setContentsMargins(0, 0, 0, 0);

	auto buttonGroup = pls_new<PLSRadioButtonGroup>(hBtnLayout);

	for (int i = 0; i < count; ++i) {
		const char *desc = pls_property_bool_group_item_desc(prop, i);
		auto radiobutton = pls_new<PLSRadioButton>(QT_UTF8(desc), this);
		buttonGroup->addButton(radiobutton);
		radiobutton->setChecked(size_t(value) == i);
		radiobutton->setProperty("idx", i);

		auto enabled = pls_property_bool_group_item_enabled(prop, i);
		radiobutton->setEnabled(enabled);

		auto tooltip = pls_property_bool_group_item_tooltip(prop, i);
		if (nullptr != tooltip) {
			radiobutton->setToolTip(QString::fromUtf8(tooltip));
			if (!enabled) {
				radiobutton->setAttribute(Qt::WA_AlwaysShowToolTips);
			}
		}

		auto info = pls_new<PLSWidgetInfo>(this, prop, radiobutton);
		connect(radiobutton, &PLSRadioButton::toggled, info, &PLSWidgetInfo::UserOperation);
		connect(radiobutton, &PLSRadioButton::toggled, info, &PLSWidgetInfo::ControlChanged);
		children.emplace_back(info);
		hBtnLayout->addWidget(radiobutton);
	}

	auto sourceId = getSourceId();
	if (pls_is_equal(sourceId, common::PRISM_VIEWER_COUNT_SOURCE_ID) || pls_is_equal(sourceId, common::PRISM_TIMER_SOURCE_ID)) {
		layout->addItem(pls_new<QSpacerItem>(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));
	} else {
		AddSpacer(obs_property_get_type(prop), layout);
	}

	auto longDesc = obs_property_long_description(prop);
	if (pls_is_empty(longDesc)) {
		auto nameLabel = pls_new<QLabel>(QT_UTF8(obs_property_description(prop)));
		nameLabel->setObjectName(OBJECT_NAME_FORMLABEL);
		if (pls_is_equal(sourceId, common::PRISM_TIMER_SOURCE_ID)) {
			nameLabel->setWordWrap(true);
		}
		layout->addRow(nameLabel, hWidget);
		return;
	}

	auto nameFrame = pls_new<QWidget>();
	nameFrame->setObjectName("formLabelFrame");
	auto *hNameLayout = pls_new<QHBoxLayout>(nameFrame);
	hNameLayout->setContentsMargins(0, 0, 0, 0);
	hNameLayout->setSpacing(5);
	auto nameLabel = pls_new<QLabel>(QT_UTF8(obs_property_description(prop)));
	nameLabel->setObjectName(OBJECT_NAME_FORMLABEL);
	nameLabel->setProperty("autoWidth", true);
	hNameLayout->addWidget(nameLabel);

	auto *subLayout = pls_new<QVBoxLayout>();
	hNameLayout->setContentsMargins(0, 2, 0, 0);

	auto helpButton = pls_new<QPushButton>();
	subLayout->addWidget(helpButton);

	helpButton->setObjectName("formLabelHelpButton");
	helpButton->setToolTip(QT_UTF8(longDesc));
	hNameLayout->addLayout(subLayout);
	hNameLayout->addStretch(1);
	layout->addRow(nameFrame, hWidget);
}

void PLSPropertiesView::AddButtonGroup(obs_property_t *prop, QFormLayout *layout)
{
	size_t count = pls_property_button_group_item_count(prop);

	auto hBtnLayout = pls_new<QHBoxLayout>();
	hBtnLayout->setSpacing(10);
	hBtnLayout->setContentsMargins(0, 0, 0, 0);
	bool setDefaultHighlight = false;
	if (auto source = pls_get_source_by_pointer_address(GetSourceObj()); source) {
		OBSDataAutoRelease privateSettings = obs_source_get_private_settings(source);
		setDefaultHighlight = obs_data_get_bool(privateSettings, "set_default_highlight");
	}

	bool isFixedWidth = PROPERTY_FLAG_BUTTON_WIDTH_FIXED & pls_property_get_flags(prop);

	for (int i = 0; i < count; i++) {
		const char *desc = pls_property_button_group_item_desc(prop, i);
		bool enabled = pls_property_button_group_item_enabled(prop, i);
		bool hightlight = pls_property_button_group_item_highlight(prop, i);
		PLSPushButton *button = pls_new<PLSPushButton>(this);
		button->setText(QT_UTF8(desc));
		button->setEnabled(enabled);

		auto info = pls_new<PLSWidgetInfo>(this, prop, button);
		connect(button, SIGNAL(clicked()), info, SLOT(UserOperation()));
		connect(button, SIGNAL(clicked()), info, SLOT(ControlChanged()));
		children.emplace_back(info);

		button->setProperty("themeID", "settingsButtons");
		button->setProperty("idx", i);
		button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
		if (setDefaultHighlight) {
			button->setProperty("yellowText", false);
		} else {
			button->setProperty("yellowText", hightlight);
		}
		if (isFixedWidth) {
			button->setObjectName("FixedButton");
		} else {
			button->setObjectName(common::OBJECT_NAME_BUTTON + QString::number(i + 1));
		}

		hBtnLayout->addWidget(button);
	}

	AddSpacer(obs_property_get_type(prop), layout);
	auto nameLabel = pls_new<QLabel>(QT_UTF8(obs_property_description(prop)));
	nameLabel->setObjectName(common::OBJECT_NAME_FORMLABEL);
	if (const char *id = getSourceId(); id && !strcmp(id, common::PRISM_TIMER_SOURCE_ID)) {
		nameLabel->setWordWrap(true);
	}
	layout->addRow(nameLabel, hBtnLayout);
}
void PLSPropertiesView::AddCustomGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	size_t count = pls_property_custom_group_item_count(prop);

	auto hBtnLayout = pls_new<QHBoxLayout>();
	hBtnLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	hBtnLayout->setSpacing(7);
	hBtnLayout->setContentsMargins(0, 0, 0, 0);

	for (size_t i = 0; i < count; ++i) {
		auto subType = pls_property_custom_group_item_type(prop, i);
		pls_used(subType);
		Q_ASSERT_X(subType == PLS_CUSTOM_GROUP_INT, "AddCustomGroup()", "new method need add ui by self");
		int minVal = 0;
		int maxVal = 0;
		int stepVal = 0;
		const char *des;
		const char *subName;
		pls_property_custom_group_item_int_params(prop, i, &minVal, &maxVal, &stepVal, nullptr, nullptr);
		pls_property_custom_group_item_params(prop, i, &subName, &des, nullptr, nullptr, nullptr);

		auto val = (int)obs_data_get_int(settings, subName);

		PLSSpinBox *spinsView_inner = pls_new<PLSSpinBox>(this);
		spinsView_inner->makeTextVCenter();
		spinsView_inner->setObjectName(common::OBJECT_NAME_SPINBOX);
		spinsView_inner->setEnabled(true);
		spinsView_inner->setSuffix(QT_UTF8(""));
		spinsView_inner->setMinimum(minVal);
		spinsView_inner->setMaximum(maxVal);
		spinsView_inner->setSingleStep(stepVal);
		spinsView_inner->setToolTip(QT_UTF8(obs_property_long_description(prop)));
		spinsView_inner->setValue(val);
		spinsView_inner->setProperty("child_name", subName);

		auto info = pls_new<PLSWidgetInfo>(this, prop, spinsView_inner);
		children.emplace_back(info);
		hBtnLayout->addWidget(spinsView_inner);

		auto nameLabel = pls_new<QLabel>(QT_UTF8(des));
		nameLabel->setObjectName("spinSubLabel");
		hBtnLayout->addWidget(nameLabel);

		connect(spinsView_inner, SIGNAL(valueChanged(int)), info, SLOT(UserOperation()));
		connect(spinsView_inner, SIGNAL(valueChanged(int)), info, SLOT(ControlChanged()));
	}

	AddSpacer(obs_property_get_type(prop), layout);
	label = pls_new<QLabel>(QT_UTF8(obs_property_description(prop)));
	layout->addRow(label, hBtnLayout);
}
void PLSPropertiesView::AddMusicList(obs_property_t *prop, QFormLayout *layout)
{
	QFrame *frameContianer = pls_new<QFrame>(this);
	frameContianer->setObjectName("frameContianer");
	QVBoxLayout *layoutContainer = pls_new<QVBoxLayout>(frameContianer);
	layoutContainer->setContentsMargins(0, 3, 0, 3);
	layoutContainer->setSpacing(0);

	auto listwidget = pls_new<QListWidget>(this);
	listwidget->setProperty("notShowHandCursor", true);
	listwidget->setObjectName("propertyMusicList");
	listwidget->setFrameShape(QFrame::NoFrame);
	listwidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	listwidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
	listwidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	listwidget->setContentsMargins(0, 0, 0, 0);
	auto info = pls_new<PLSWidgetInfo>(this, prop, listwidget);
	connect(listwidget, SIGNAL(currentRowChanged(int)), info, SLOT(UserOperation()));

	size_t count = pls_property_bgm_list_item_count(prop);
	for (size_t i = 0; i < count; i++) {
		QString name = pls_property_bgm_list_item_name(prop, i);
		QString producer = pls_property_bgm_list_item_producer(prop, i);
		if (name.isEmpty() || producer.isEmpty()) {
			continue;
		}
		QString text = name + " - " + producer;
		QListWidgetItem *item = pls_new<QListWidgetItem>(text);
		item->setToolTip(text);
		listwidget->blockSignals(true);
		listwidget->addItem(item);
		listwidget->setStyleSheet("font-weight: bold;");
		listwidget->blockSignals(false);
	}

	children.emplace_back(info);

	AddSpacer(obs_property_get_type(prop), layout);
	layoutContainer->addWidget(listwidget);
	layout->addRow(frameContianer);
}

void PLSPropertiesView::AddTips(obs_property_t *prop, QFormLayout *layout)
{
	auto container = pls_new<QFrame>(this);
	container->setFrameShape(QFrame::NoFrame);
	auto hLayout = pls_new<QHBoxLayout>(container);
	hLayout->setAlignment(Qt::AlignLeft);

	hLayout->setContentsMargins(0, 0, 0, 0);
	hLayout->setSpacing(10);

	const char *name = obs_property_name(prop);
	const char *des = obs_property_description(prop);

	QLabel *songCountLabel{};
	if (name && !std::string(name).empty()) {
		songCountLabel = pls_new<QLabel>(name, this);
		songCountLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
		songCountLabel->setObjectName("songCountLabel");

		if (pls_is_equal(getSourceId(), common::PRISM_SPECTRALIZER_SOURCE_ID)) {
			songCountLabel->setObjectName("tmLabel");
		}
	}

	PLSLabel *songTipsLabel{};
	if (des && !std::string(des).empty()) {
		bool textNoCut = PROPERTY_FLAG_NO_TEXT_CUT & pls_property_get_flags(prop);
		songTipsLabel = pls_new<PLSLabel>(this, !textNoCut);
		songTipsLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		songTipsLabel->SetText(des);
		songTipsLabel->setObjectName("songTipsLabel");
	}

	if (songCountLabel)
		hLayout->addWidget(songCountLabel);
	if (songTipsLabel)
		hLayout->addWidget(songTipsLabel);

	AddSpacer(obs_property_get_type(prop), layout);

	if ((PROPERTY_FLAG_NO_LABEL_HEADER & pls_property_get_flags(prop)) || (PROPERTY_FLAG_NO_LABEL_SINGLE & pls_property_get_flags(prop))) {
		layout->addRow(container);
	} else {
		layout->addRow(nullptr, container);
	}
}

void PLSPropertiesView::AddChatTemplateList(obs_property_t *prop, QFormLayout *layout)
{
	QWidget *widget = pls_new<QWidget>();
	widget->setObjectName("chatTemplateList");

	QVBoxLayout *vlayout = pls_new<QVBoxLayout>(widget);
	vlayout->setContentsMargins(0, 0, 0, 0);
	vlayout->setSpacing(15);

	QHBoxLayout *hlayout1 = pls_new<QHBoxLayout>();
	hlayout1->setContentsMargins(0, 0, 0, 0);
	hlayout1->setSpacing(10);

	const char *desc = obs_property_description(prop);
	QLabel *nameLabel = pls_new<QLabel>(QString::fromUtf8(desc && desc[0] ? desc : ""));
	nameLabel->setObjectName("chatTemplateList_nameLabel");

	hlayout1->addWidget(nameLabel);
	if (!obs_frontend_streaming_active()) {
		const char *longDesc = obs_property_long_description(prop);
		QLabel *descLabel = pls_new<QLabel>(QString::fromUtf8(longDesc && longDesc[0] ? longDesc : ""));
		descLabel->setObjectName("chatTemplateList_descLabel");
		hlayout1->addWidget(descLabel, 1);
	}

	QHBoxLayout *hlayout2 = pls_new<QHBoxLayout>();
	hlayout2->setContentsMargins(0, 0, 0, 0);
	hlayout2->setSpacing(10);

	const char *name = obs_property_name(prop);
	auto val = (int)obs_data_get_int(settings, name);

	QButtonGroup *buttonGroup = pls_new<QButtonGroup>(hlayout2);
	buttonGroup->setExclusive(true);
	buttonGroup->setProperty("selected", val);

	for (int i = 1; i <= 4; ++i) {
		QPushButton *button = pls_new<ChatTemplate>(buttonGroup, i, val == i);
		hlayout2->addWidget(button);
	}

	vlayout->addLayout(hlayout1);
	vlayout->addLayout(hlayout2, 1);

	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(widget);

	PLSWidgetInfo *wi = pls_new<PLSWidgetInfo>(this, prop, buttonGroup);

	children.emplace_back(wi);

	void (QButtonGroup::*buttonClicked)(int) = &QButtonGroup::idClicked;
	connect(buttonGroup, buttonClicked, wi, [buttonGroup, wi](int index) {
		int previous = buttonGroup->property("selected").toInt();
		if (previous != index) {
			buttonGroup->setProperty("selected", index);
			wi->ControlChanged();
			PLS_UI_STEP(PROPERTY_MODULE, QString::asprintf("property-window: chat theme button %d", index).toUtf8().constData(), ACTION_CLICK);

			pls_flush_style_recursive(buttonGroup->button(previous));
			pls_flush_style_recursive(buttonGroup->button(index));
		}
	});
}

void PLSPropertiesView::AddChatFontSize(obs_property_t *prop, QFormLayout *layout)
{
	QWidget *widget = pls_new<QWidget>();
	widget->setObjectName("chatFontSize");

	QHBoxLayout *hlayout1 = pls_new<QHBoxLayout>(widget);
	hlayout1->setContentsMargins(0, 0, 0, 0);
	hlayout1->setSpacing(20);

	QLabel *label = pls_new<QLabel>(QString::fromUtf8(obs_property_description(prop)));
	label->setObjectName("label");

	QHBoxLayout *hlayout2 = pls_new<QHBoxLayout>();
	hlayout2->setContentsMargins(0, 0, 0, 0);
	hlayout2->setSpacing(10);

	int minVal = pls_property_chat_font_size_min(prop);
	int maxVal = pls_property_chat_font_size_max(prop);
	int stepVal = pls_property_chat_font_size_step(prop);

	const char *name = obs_property_name(prop);
	auto val = (int)obs_data_get_int(settings, name);

	SliderIgnoreScroll *slider = pls_new<SliderIgnoreScroll>();
	slider->setObjectName("slider");
	slider->setMinimum(minVal);
	slider->setMaximum(maxVal);
	slider->setPageStep(stepVal);
	slider->setValue(val);
	slider->setOrientation(Qt::Horizontal);

	PLSSpinBox *spinBox = pls_new<PLSSpinBox>();
	spinBox->setObjectName("spinBox");
	spinBox->setRange(minVal, maxVal);
	spinBox->setSingleStep(stepVal);
	spinBox->setValue(val);

	hlayout2->addWidget(slider, 1);
	hlayout2->addWidget(spinBox);

	hlayout1->addWidget(label);
	hlayout1->addLayout(hlayout2, 1);

	AddSpacer(obs_property_get_type(prop), layout);
	if (layout)
		layout->addRow(widget);

	connect(slider, SIGNAL(valueChanged(int)), spinBox, SLOT(setValue(int)));
	connect(spinBox, SIGNAL(valueChanged(int)), slider, SLOT(setValue(int)));

	auto *wi = pls_new<PLSWidgetInfo>(this, prop, spinBox);
	connect(spinBox, SIGNAL(valueChanged(int)), wi, SLOT(ControlChanged()));
	children.emplace_back(wi);
}

void PLSPropertiesView::AddTmTab(obs_property_t *prop, QFormLayout *layout)
{
	const char *name = obs_property_name(prop);
	auto tabIndex = (int)obs_data_get_int(settings, name);
	auto tabFrame = pls_new<QFrame>();
	tabFrame->setObjectName("TMTabFrame");
	auto hLayout = pls_new<QHBoxLayout>();
	hLayout->setAlignment(Qt::AlignLeft);
	hLayout->setContentsMargins(0, 0, 0, 0);
	hLayout->setSpacing(0);
	tabFrame->setLayout(hLayout);

	bool audioVisualizer = false;
	bool isTimerSource = false;
	bool isViewerCountSource = false;

	const char *id = getSourceId();
	if (id && id[0]) {
		audioVisualizer = !strcmp(id, common::PRISM_SPECTRALIZER_SOURCE_ID);
		isTimerSource = !strcmp(id, common::PRISM_TIMER_SOURCE_ID);
		isViewerCountSource = pls_is_equal(id, common::PRISM_VIEWER_COUNT_SOURCE_ID);
	}
	if (isTimerSource) {
		tabFrame->setProperty("height_timer", true);
	}

	QStringList tabList = {QTStr("textmotion.select.template"), QTStr("textmotion.detailed.settings")};
	auto buttonGroup = pls_new<QButtonGroup>();
	for (int index = 0; index != tabList.count(); ++index) {
		auto button = pls_new<QPushButton>();
		button->setObjectName("TMTabButton");
		buttonGroup->addButton(button, index);
		button->setAutoExclusive(true);
		button->setCheckable(true);
		button->setText(tabList.value(index));
		button->setChecked(index == tabIndex);
		hLayout->addWidget(button);
	}
	auto button = buttonGroup->button(tabIndex);
	if (!button) {
		return;
	}
	button->setChecked(true);

	auto wi = pls_new<PLSWidgetInfo>(this, prop, buttonGroup);
	connect(buttonGroup, QOverload<int>::of(&QButtonGroup::idClicked), [wi, this]() {
		wi->ControlChanged();
		m_tmTabChanged = true;
	});
	children.emplace_back(wi);

	if (audioVisualizer) {
		layout->addItem(pls_new<QSpacerItem>(10, 10, QSizePolicy::Fixed, QSizePolicy::Fixed));
	} else if (isTimerSource || isViewerCountSource) {
		layout->addItem(pls_new<QSpacerItem>(10, 6, QSizePolicy::Fixed, QSizePolicy::Fixed));
		lastPropertyType = obs_property_get_type(prop);
	} else {
		AddSpacer(obs_property_get_type(prop), layout);
	}

	layout->addRow(tabFrame);
	if (audioVisualizer)
		tabIndex ? layout->addItem(pls_new<QSpacerItem>(10, 10, QSizePolicy::Fixed, QSizePolicy::Fixed)) : layout->addItem(pls_new<QSpacerItem>(10, 5, QSizePolicy::Fixed, QSizePolicy::Fixed));
}

void PLSPropertiesView ::AddTmTemplateTab(obs_property_t *prop, QFormLayout *layout)
{
	const char *name = obs_property_name(prop);
	auto tabIndex = (int)obs_data_get_int(settings, name);
	obs_property_t *propNext = prop;
	bool isNext = obs_property_next(&propNext);
	const char *nextName = nullptr;
	int templateTabIndex = -1;
	if (isNext) {
		nextName = obs_property_name(propNext);
		templateTabIndex = (int)obs_data_get_int(settings, nextName);
		auto defaultInde = m_tmHelper->getDefaultTemplateId();
		if (templateTabIndex == 0 && templateTabIndex != defaultInde) {
			templateTabIndex = defaultInde;
		}
	}
	if (tabIndex < 0 || templateTabIndex < 0) {
		return;
	}
	auto vLayout = pls_new<QVBoxLayout>();
	vLayout->setContentsMargins(0, 15, 0, 0);
	vLayout->setSpacing(15);

	auto tabFrame = pls_new<QFrame>();
	tabFrame->setObjectName("TMTabTemplistFrame");
	auto hLayout = pls_new<QHBoxLayout>();
	hLayout->setSpacing(0);
	hLayout->setContentsMargins(0, 0, 0, 0);
	hLayout->setSpacing(10);
	hLayout->setContentsMargins(16, 0, 10, 0);
	hLayout->setAlignment(Qt::AlignLeft);
	tabFrame->setLayout(hLayout);

	auto buttonGroup = pls_new<QButtonGroup>();
	const QStringList &templateList = m_tmHelper->getTemplateNameList();
	auto templateCount = static_cast<int>(templateList.count());
	QString selectTemplateStr = m_tmHelper->findTemplateGroupStr(templateTabIndex);
	if (selectTemplateStr.isEmpty()) {
		selectTemplateStr = templateList.value(tabIndex);
	}
	for (int index = 0; index != templateCount; ++index) {
		auto button = pls_new<QPushButton>();
		button->setObjectName("TMTabTemplistBtn");
		buttonGroup->addButton(button, index);
		button->setAutoExclusive(true);
		button->setCheckable(true);
		button->setText(templateList.value(index).toUpper());

		button->setProperty("customName", button->text().toLower());

		if (m_tmTemplateChanged) {
			button->setChecked(index == tabIndex);
		} else {
			button->setChecked(0 == templateList.value(index).compare(selectTemplateStr, Qt::CaseInsensitive));
		}
		hLayout->addWidget(button);
	}

	auto gLayout = pls_new<QGridLayout>();
	gLayout->setSpacing(12);
	gLayout->setAlignment(Qt::AlignLeft);

	if (m_tmTemplateChanged) {
		QString templateName = templateList.value(tabIndex);
		updateTMTemplateButtons(tabIndex, templateName.toLower(), gLayout);
	} else {
		updateTMTemplateButtons(tabIndex, selectTemplateStr.toLower(), gLayout);
	}

	auto wi = pls_new<PLSWidgetInfo>(this, prop, buttonGroup);
	connect(buttonGroup, QOverload<int>::of(&QButtonGroup::idClicked), wi, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(wi);

	connect(buttonGroup, QOverload<int>::of(&QButtonGroup::idClicked), [this, buttonGroup, gLayout](int index) {
		if (index >= 0) {
			updateTMTemplateButtons(index, buttonGroup->button(index)->text().toLower(), gLayout);
			m_tmTemplateChanged = true;
		}
	});

	vLayout->addWidget(tabFrame, 0);
	vLayout->addLayout(gLayout, 1);
	layout->addRow(vLayout);
}
void PLSPropertiesView::AddTmTabTemplateList(obs_property_t *prop, QFormLayout *layout)
{
	pls_unused(layout);
	const char *name = obs_property_name(prop);
	auto tabIndex = (int)obs_data_get_int(settings, name);
	if (tabIndex < 0) {
		return;
	}
	auto defaultInde = m_tmHelper->getDefaultTemplateId();
	if (tabIndex == 0 && tabIndex != defaultInde) {
		tabIndex = defaultInde;
	}
	QString selectTemplateStr = m_tmHelper->findTemplateGroupStr(tabIndex);
	if (selectTemplateStr.isEmpty()) {
		selectTemplateStr = m_tmHelper->getTemplateNameList().value(tabIndex);
	}
	auto buttonGroup = m_tmHelper->getTemplateButtons(selectTemplateStr);

	if (buttonGroup) {
		m_tmHelper->resetButtonStyle();
		auto button = buttonGroup->button(tabIndex);
		if (button) {
			button->setChecked(true);
		}
	}
	QMap<int, QString> templateNames = m_tmHelper->getTemplateNames();
	for (auto templateName : templateNames.keys()) {
		auto templateTabGroup = m_tmHelper->getTemplateButtons(templateNames.value(templateName).toLower());
		if (templateTabGroup) {
			auto wi = pls_new<PLSWidgetInfo>(this, prop, templateTabGroup);
			connect(templateTabGroup, QOverload<int>::of(&QButtonGroup::idClicked), wi, &PLSWidgetInfo::ControlChanged);
			children.emplace_back(wi);
		}
	}
}
static QStringList getFilteredFontFamilies()
{
	auto families = QFontDatabase::families();
#if defined(Q_OS_MACOS)
	for (auto iter = families.begin(); iter != families.end();) {
		if (iter->startsWith(".")) {
			iter = families.erase(iter);
		} else {
			++iter;
		}
	}
#endif
	return families;
}
void PLSPropertiesView::AddTmText(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);
	bool issettings = obs_data_get_bool(val, "is-font-settings");

	if (!issettings) {
		obs_data_release(val);
		return;
	}
	label = pls_new<QLabel>(QString::fromUtf8(obs_property_description(prop)));
	auto flayout = pls_new<QFormLayout>();
	flayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	flayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	flayout->setHorizontalSpacing(20);
	flayout->setVerticalSpacing(10);
	if (obs_data_get_bool(val, "is-font")) {
		auto hlayout1 = pls_new<QHBoxLayout>();
		hlayout1->setContentsMargins(0, 0, 0, 0);
		hlayout1->setSpacing(10);

		auto fontCbx = pls_new<PLSComboBox>();
		fontCbx->setObjectName("tmFontBox");
		auto family = obs_data_get_string(val, "font-family");
#if defined(Q_OS_WIN)
		const char *ko_en_family = "Malgun Gothic";
		const char *ko_ko_family = "\353\247\221\354\235\200 \352\263\240\353\224\225";
		if (!strcmp(family, ko_en_family) || !strcmp(family, ko_ko_family)) {
			if (QLocale::system().language() == QLocale::Korean) {
				family = ko_ko_family;
			} else {
				family = ko_en_family;
			}
		}
#endif
		fontCbx->addItem(family);
		fontCbx->setCurrentText(family);
		auto weightCbx = pls_new<PLSComboBox>();
		updateFontSytle(family, weightCbx);
		weightCbx->setCurrentText(obs_data_get_string(val, "font-weight"));

		weightCbx->setObjectName("tmFontStyleBox");
		connect(fontCbx, QOverload<const QString &>::of(&QComboBox::currentTextChanged), [this, weightCbx](const QString &text) { updateFontSytle(text, weightCbx); });

		QMetaObject::invokeMethod(
			fontCbx,
			[family, fontCbx, weightCbx, this]() {
				QSignalBlocker block(fontCbx);
				fontCbx->clear();
				fontCbx->addItems(getFilteredFontFamilies());
				PLS_INFO(PROPERTY_MODULE, "current language = %s", family);
				if (family[0] == '\0' || family == nullptr) {
					auto fontStyle = fontCbx->currentText();
					updateFontSytle(fontStyle, weightCbx);
					return;
				}
				fontCbx->setCurrentText(family);
			},
			Qt::QueuedConnection);
		hlayout1->addWidget(fontCbx);
		hlayout1->addWidget(weightCbx);
		hlayout1->setStretch(0, 292);
		hlayout1->setStretch(1, 210);

		auto fontLabel = pls_new<QLabel>(QTStr("textmotion.font"));
		fontLabel->setObjectName("subLabel");
		flayout->addRow(fontLabel, hlayout1);
		m_tmLabels.append(fontLabel);
		auto fontWidgetInfo = pls_new<PLSWidgetInfo>(this, prop, fontCbx);
		connect(fontCbx, QOverload<const QString &>::of(&QComboBox::currentTextChanged), fontWidgetInfo, &PLSWidgetInfo::ControlChanged);
		children.emplace_back(fontWidgetInfo);

		auto fontStyleWidgetInfo = pls_new<PLSWidgetInfo>(this, prop, weightCbx);
		connect(weightCbx, QOverload<const QString &>::of(&QComboBox::currentTextChanged), fontStyleWidgetInfo, &PLSWidgetInfo::ControlChanged);
		children.emplace_back(fontStyleWidgetInfo);
	}
	if (obs_data_get_bool(val, "is-font-size")) {
		auto hlayout3 = pls_new<QHBoxLayout>();
		hlayout3->setContentsMargins(0, 0, 0, 0);
		hlayout3->setSpacing(20);

		int minVal = pls_property_tm_text_min(prop, PLS_PROPERTY_TM_TEXT);
		int maxVal = pls_property_tm_text_max(prop, PLS_PROPERTY_TM_TEXT);
		int stepVal = pls_property_tm_text_step(prop, PLS_PROPERTY_TM_TEXT);
		auto fontSize = (int)obs_data_get_int(val, "font-size");

		createTMSlider(prop, 0, minVal, maxVal, stepVal, fontSize, hlayout3, false, false, false);
		auto fontSizeLabel = pls_new<QLabel>(QTStr("textmotion.size"));
		fontSizeLabel->setObjectName("subLabel");
		flayout->addRow(fontSizeLabel, hlayout3);
		m_tmLabels.append(fontSizeLabel);
	}
	bool isBoxSize = obs_data_get_bool(val, "is-box-size");
	if (isBoxSize) {
		auto boxSizeHLayout = pls_new<QHBoxLayout>();
		boxSizeHLayout->setSpacing(16);
		boxSizeHLayout->setContentsMargins(0, 0, 0, 0);

		int minWidthVal = 0;
		int maxWidthVal = 5000;
		int widthStepVal = 1;
		auto widthSize = (int)obs_data_get_int(settings, "width");
		createTMSlider(prop, 1, minWidthVal, maxWidthVal, widthStepVal, widthSize, boxSizeHLayout, false, false, false, QTStr("textmotion.text.box.width"));

		int minHeightVal = 0;
		int maxHeightVal = 5000;
		int heightStepVal = 1;
		auto heightSize = (int)obs_data_get_int(settings, "height");
		createTMSlider(prop, 2, minHeightVal, maxHeightVal, heightStepVal, heightSize, boxSizeHLayout, false, false, false, QTStr("textmotion.text.box.height"));

		auto boxSizeLabel = pls_new<QLabel>(QTStr("textmotion.text.box.size"));
		boxSizeLabel->setObjectName("subLabel");
		flayout->addRow(boxSizeLabel, boxSizeHLayout);
		m_tmLabels.append(boxSizeLabel);
	}

	auto textAlignmentLayout = pls_new<QHBoxLayout>();
	textAlignmentLayout->setAlignment(Qt::AlignLeft);
	textAlignmentLayout->setSpacing(10);

	auto hlayout2 = pls_new<QHBoxLayout>();
	hlayout2->setContentsMargins(0, 0, 0, 0);
	hlayout2->setSpacing(1);

	bool isAlign = obs_data_get_bool(val, "is-h-aligin");
	if (isAlign) {
		auto group = pls_new<QButtonGroup>();
		group->setObjectName("group");
		createTMButton(3, val, hlayout2, group, ButtonType::CustomButton, {"TMHLButton", "TMHCButton", "TMHRButton"});

		auto hAlignIndex = (int)obs_data_get_int(val, "h-aligin");

		auto alignBtn = group->button(hAlignIndex);
		if (alignBtn) {
			alignBtn->setChecked(true);
		}

		textAlignmentLayout->addLayout(hlayout2);

		auto alignWidgetInfo = pls_new<PLSWidgetInfo>(this, prop, group);
		connect(group, QOverload<int>::of(&QButtonGroup::idClicked), alignWidgetInfo, &PLSWidgetInfo::ControlChanged);
		children.emplace_back(alignWidgetInfo);
	}

	auto hlayout4 = pls_new<QHBoxLayout>();
	hlayout4->setContentsMargins(0, 0, 0, 0);
	hlayout4->setSpacing(1);
	hlayout4->setAlignment(Qt::AlignLeft);
	auto group2 = pls_new<QButtonGroup>();
	group2->setObjectName("group2");
	group2->setExclusive(false);
	createTMButton(2, val, hlayout4, group2, ButtonType::LetterButton, {"AA", "aa"}, false, false);

	auto letterIndex = (int)obs_data_get_int(val, "letter");
	auto button = group2->button(letterIndex);
	if (letterIndex != -1 && button) {
		button->clicked();
	}

	textAlignmentLayout->addLayout(hlayout4);
	textAlignmentLayout->setStretch(0, 292);
	textAlignmentLayout->setStretch(1, 210);
	auto ali_transformText = "textmotion.align.transform";
	if (!isAlign) {
		ali_transformText = "textmotion.transform";
	}
	auto textAlignmentLabel = pls_new<QLabel>(QTStr(ali_transformText));
	textAlignmentLabel->setObjectName("subLabel");
	flayout->addRow(textAlignmentLabel, textAlignmentLayout);
	m_tmLabels.append(textAlignmentLabel);

	auto letterWidgetInfo = pls_new<PLSWidgetInfo>(this, prop, group2);
	connect(group2, QOverload<int>::of(&QButtonGroup::idClicked), [letterWidgetInfo, group2](int index) {
		auto btn = group2->button(index);
		for (auto btnIndex : group2->buttons()) {
			bool isChecked = btnIndex->isChecked();
			if (btnIndex != btn && isChecked) {
				btnIndex->clicked();
			}
		}
		letterWidgetInfo->ControlChanged();
	});
	children.emplace_back(letterWidgetInfo);

	auto w = pls_new<QWidget>();
	w->setObjectName("horiLine");
	flayout->addItem(new QSpacerItem(10, 0, QSizePolicy::Fixed, QSizePolicy::Fixed));
	flayout->addRow(w);

	layout->addItem(new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));

	layout->addRow(label);
	layout->addItem(new QSpacerItem(10, 27, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(flayout);

	obs_data_release(val);
}

void PLSPropertiesView::AddTmTextContent(obs_property_t *prop, QFormLayout *layout)
{
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);
	auto textCount = (int)obs_data_get_int(val, "text-count");

	auto hLayout = pls_new<QHBoxLayout>();
	hLayout->setContentsMargins(0, 0, 0, 0);
	hLayout->setSpacing(6);

	creatTMTextWidget(prop, textCount, val, hLayout);

	if (layout) {
		layout->addRow(hLayout);
		layout->addItem(new QSpacerItem(10, 5, QSizePolicy::Fixed, QSizePolicy::Fixed));
	}
	obs_data_release(val);
}

void PLSPropertiesView::AddTmColor(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);
	if (!obs_data_get_bool(val, "is-color-settings")) {
		obs_data_release(val);
		return;
	}

	label = pls_new<QLabel>(QString::fromUtf8(obs_property_description(prop)));
	auto glayout = pls_new<QGridLayout>();
	glayout->setHorizontalSpacing(20);
	glayout->setVerticalSpacing(10);
	bool isTextColor = obs_data_get_bool(val, "is-color");
	if (isTextColor) {
		auto insetRow = glayout->rowCount() - 1;
		auto textColorLayout = pls_new<QHBoxLayout>();
		textColorLayout->setContentsMargins(0, 0, 0, 0);
		textColorLayout->setSpacing(20);
		auto textColorLabel = pls_new<QLabel>(QTStr("textmotion.text"));
		textColorLabel->setObjectName("subLabel");
		glayout->addWidget(textColorLabel, insetRow, 0);
		m_tmLabels.append(textColorLabel);

		PLSCheckBox *ChecBox = nullptr;
		pls_used(ChecBox);
		auto colorList = QT_UTF8(obs_data_get_string(val, "text-color-list"));
		if (colorList.isEmpty()) {
			createColorButton(prop, glayout, ChecBox, QTStr("textmotion.opacity"), 0, true, true);
		} else {
			creatColorList(prop, glayout, insetRow, obs_data_get_int(val, "text-color"), colorList);
		}
	}

	bool isEnable = obs_data_get_bool(val, "is-bk-color");
	if (isEnable) {
		bool isChecked = obs_data_get_bool(val, "is-bk-color-on");
		auto bkColorList = QT_UTF8(obs_data_get_string(val, "bk-color-list"));
		auto insetRow = glayout->rowCount();
		if (bkColorList.isEmpty()) {
			PLSCheckBox *bkControlChecBox = nullptr;
			auto bkColorLayout = pls_new<QHBoxLayout>();
			bkColorLayout->setContentsMargins(0, 0, 0, 0);
			bkColorLayout->setSpacing(20);
			auto bkColorLabelFrame = pls_new<QFrame>();
			bkColorLabelFrame->setObjectName("colorFrame");
			createTMColorCheckBox(bkControlChecBox, prop, bkColorLabelFrame, 1, QTStr("textmotion.background"), bkColorLayout, isChecked, obs_data_get_bool(val, "is-bk-init-color-on"));

			glayout->addWidget(bkColorLabelFrame, insetRow, 0);
			createColorButton(prop, glayout, bkControlChecBox, QTStr("textmotion.opacity"), 1, true, true);
		} else {
			auto bkLabel = pls_new<QLabel>(QTStr("textmotion.background"));
			bkLabel->setObjectName("subLabel");
			glayout->addWidget(bkLabel, insetRow, 0);
			glayout->setAlignment(Qt::AlignLeft);
			m_tmLabels.append(bkLabel);
			creatColorList(prop, glayout, insetRow, obs_data_get_int(val, "bk-color"), bkColorList);
		}
	}

	bool isOutlineColor = obs_data_get_bool(val, "is-outline-color");
	if (isOutlineColor) {
		auto insetRow = glayout->rowCount();
		PLSCheckBox *outlineControlCheckBox = nullptr;
		auto outLineLayout = pls_new<QHBoxLayout>();
		outLineLayout->setContentsMargins(0, 0, 0, 0);
		outLineLayout->setSpacing(20);
		auto outLineLabelFrame = pls_new<QFrame>();
		createTMColorCheckBox(outlineControlCheckBox, prop, outLineLabelFrame, 2, QTStr("textmotion.outline"), outLineLayout, obs_data_get_bool(val, "is-outline-color-on"),
				      obs_data_get_bool(val, "is-outline-init-color-on"));

		glayout->addWidget(outLineLabelFrame, insetRow, 0);
		createColorButton(prop, glayout, outlineControlCheckBox, QTStr("textmotion.thickness"), 2, false, true);
	}

	auto w = pls_new<QWidget>();
	w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	w->setObjectName("horiLine");
	glayout->addItem(pls_new<QSpacerItem>(10, 10, QSizePolicy::Fixed, QSizePolicy::Fixed), glayout->rowCount(), 0);
	glayout->addWidget(w, glayout->rowCount(), 0, 1, glayout->columnCount());

	layout->addItem(pls_new<QSpacerItem>(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));

	layout->addRow(label);
	layout->addItem(pls_new<QSpacerItem>(10, 27, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(glayout);

	obs_data_release(val);
}

void PLSPropertiesView::AddTmMotion(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);
	if (!obs_data_get_bool(val, "is-motion-settings")) {
		obs_data_release(val);
		return;
	}
	label = pls_new<QLabel>(QString::fromUtf8(obs_property_description(prop)));
	auto flayout = pls_new<QFormLayout>();
	flayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	flayout->setHorizontalSpacing(20);
	flayout->setVerticalSpacing(0);
	auto hlayout = pls_new<QHBoxLayout>();
	hlayout->setAlignment(Qt::AlignLeft);
	hlayout->setContentsMargins(0, 0, 0, 0);
	hlayout->setSpacing(20);
	auto group2 = pls_new<PLSRadioButtonGroup>();
	group2->setObjectName("group2");

	auto frame = pls_new<QFrame>();
	frame->setFrameStyle(QFrame::NoFrame);
	frame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	frame->setLayout(hlayout);
	createRadioButton(2, val, hlayout, group2, {QTStr("textmotion.repeat.motion.on"), QTStr("textmotion.retpeat.motion.off")}, true, frame);
	auto repeatLabel = pls_new<QLabel>(QTStr("textmotion.repeat.off"));
	repeatLabel->setObjectName("subLabel");
	flayout->addItem(pls_new<QSpacerItem>(10, 6, QSizePolicy::Fixed, QSizePolicy::Fixed));
	flayout->addRow(repeatLabel, frame);
	m_tmLabels.append(repeatLabel);

	auto hlayout2 = pls_new<QHBoxLayout>();
	hlayout2->setContentsMargins(0, 0, 0, 0);
	hlayout2->setSpacing(20);

	int minVal = pls_property_tm_text_min(prop, PLS_PROPERTY_TM_MOTION);
	int maxVal = pls_property_tm_text_max(prop, PLS_PROPERTY_TM_MOTION);
	int stepVal = pls_property_tm_text_step(prop, PLS_PROPERTY_TM_MOTION);
	auto currentVal = (int)obs_data_get_int(val, "text-motion-speed");
	createTMSlider(prop, -1, minVal, maxVal, stepVal, currentVal, hlayout2, true, true, true);
	auto isAgain = (int)obs_data_get_int(val, "text-motion");
	group2->button(isAgain == 1 ? 0 : 1)->setChecked(true);
	setLayoutEnable(hlayout2, isAgain != 0);

	auto wi = pls_new<PLSWidgetInfo>(this, prop, group2);
	connect(group2, &PLSRadioButtonGroup::idClicked, [hlayout2, this, wi](int index) {
		setLayoutEnable(hlayout2, index == 0);
		wi->ControlChanged();
	});
	children.emplace_back(wi);

	auto speedLabel = pls_new<QLabel>(QTStr("textmotion.speed"));
	speedLabel->setObjectName("subLabel");
	speedLabel->setProperty("moveDown", "3");
	speedLabel->setIndent(0);
	flayout->addItem(pls_new<QSpacerItem>(10, 21, QSizePolicy::Fixed, QSizePolicy::Fixed));
	flayout->addRow(speedLabel, hlayout2);
	m_tmLabels.append(speedLabel);
	speedLabel->setProperty("moveDown", "3");

	hlayout2->setEnabled(obs_data_get_bool(val, "is-text-motion-speed"));

	auto w = pls_new<QWidget>();
	w->setObjectName("horiLine");
	flayout->addItem(pls_new<QSpacerItem>(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));
	flayout->addRow(w);

	layout->addItem(pls_new<QSpacerItem>(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));

	layout->addRow(label);
	layout->addItem(pls_new<QSpacerItem>(10, 27, QSizePolicy::Fixed, QSizePolicy::Fixed));
	layout->addRow(flayout);

	obs_data_release(val);
}

QWidget *PLSPropertiesView::AddSelectRegion(obs_property_t *prop, bool &b)
{
	pls_unused(b);
	QWidget *selectRegionRow = pls_new<QWidget>(this);
	QVBoxLayout *layout = pls_new<QVBoxLayout>(selectRegionRow);
	layout->setContentsMargins(0, 0, 0, 4);
	layout->setSpacing(12);
	QPushButton *btnSelectRegion = pls_new<QPushButton>(this);
	const auto *info = pls_new<PLSWidgetInfo>(this, prop, selectRegionRow);
	connect(btnSelectRegion, SIGNAL(clicked()), info, SLOT(UserOperation()));
	connect(btnSelectRegion, SIGNAL(clicked()), info, SLOT(ControlChanged()));
	btnSelectRegion->setObjectName("selectRegionBtn");
	btnSelectRegion->setText(QTStr("Basic.PropertiesView.Capture.selectRegion"));
	QLabel *labelTips = pls_new<QLabel>(selectRegionRow);
	labelTips->setObjectName("selectCaptureRegionTip");
	labelTips->setText(QTStr("Basic.PropertiesView.Capture.tips"));
	layout->addWidget(btnSelectRegion);
	layout->addWidget(labelTips);
	return selectRegionRow;
}

void PLSPropertiesView::AddImageGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	auto index = obs_data_get_int(settings, name);
	auto gLayout = pls_new<QGridLayout>();
	gLayout->setHorizontalSpacing(10);
	gLayout->setContentsMargins(0, 0, 0, 0);

	QPointer<QButtonGroup> buttonGroup = new QButtonGroup(gLayout);

	size_t count = pls_property_image_group_item_count(prop);
	int row = 0;
	int colum = 0;
	pls_image_style_type type_;
	pls_property_image_group_params(prop, &row, &colum, &type_);
	row = row ? row : 1;
	colum = colum ? colum : 1;

	if (type_ == PLS_IMAGE_STYLE_TEMPLATE || type_ == PLS_IMAGE_STYLE_APNG_BUTTON) {
		gLayout->setHorizontalSpacing(12);
		gLayout->setVerticalSpacing(12);
	} else if (type_ == PLS_IMAGE_STYLE_SOLID_COLOR)
		gLayout->setHorizontalSpacing(8);
	else if (type_ == PLS_IMAGE_STYLE_GRADIENT_COLOR)
		gLayout->setHorizontalSpacing(9);
	else if (type_ == PLS_IMAGE_STYLE_BORDER_BUTTON) {
		gLayout->setHorizontalSpacing(6);
		gLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	}

	for (size_t i = 0; i < count; i++) {
		QString url = pls_property_image_group_item_url(prop, int(i));
		const char *subName = pls_property_image_group_item_name(prop, int(i));
		if (type_ == PLS_IMAGE_STYLE_BORDER_BUTTON) {
			BorderImageButton *button = pls_new<BorderImageButton>(buttonGroup, type_, url, int(i), int(i) == index, true);
			gLayout->addWidget(button, int(i) / colum, int(i) % colum);
		} else if (type_ == PLS_IMAGE_STYLE_APNG_BUTTON) {
			ImageAPNGButton *button = pls_new<ImageAPNGButton>(buttonGroup, type_, url, int(i), int(i) == index, subName, 1.0, QSize{157, 88});
			gLayout->addWidget(button, int(i) / colum, int(i) % colum);
			m_movieButtons.append(button);
		} else {
			ImageButton *button = pls_new<ImageButton>(buttonGroup, type_, url, int(i), int(i) == index);
			gLayout->addWidget(button, int(i) / colum, int(i) % colum);
		}
	}

	auto wi = pls_new<PLSWidgetInfo>(this, prop, buttonGroup);
	connect(buttonGroup, SIGNAL(buttonClicked(QAbstractButton *)), wi, SLOT(UserOperation()));
	connect(buttonGroup, SIGNAL(buttonClicked(QAbstractButton *)), wi, SLOT(ControlChanged()));
	children.emplace_back(wi);

	AddSpacer(obs_property_get_type(prop), layout);
	if (PROPERTY_FLAG_NO_LABEL_SINGLE & pls_property_get_flags(prop))
		layout->addRow(gLayout);
	else {
		label = pls_new<QLabel>(QT_UTF8(obs_property_description(prop)));
		label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		layout->addRow(label, gLayout);
	}

	if (type_ == PLS_IMAGE_STYLE_BORDER_BUTTON || type_ == PLS_IMAGE_STYLE_SOLID_COLOR) {
		layout->addItem(new QSpacerItem(10, 4, QSizePolicy::Fixed, QSizePolicy::Fixed));
	}
}

void PLSPropertiesView::AddvirtualCustomGroup(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	auto gLayout = pls_new<QGridLayout>();
	gLayout->setHorizontalSpacing(20);
	gLayout->setVerticalSpacing(10);
	gLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	gLayout->setContentsMargins(0, 0, 0, 0);

	int row = 1;
	int colum = 1;
	pls_property_visualizer_custom_group_params(prop, &row, &colum);
	row = row ? row : 1;
	colum = colum ? colum : 1;

	size_t count = pls_property_visualizer_custom_group_item_count(prop);
	for (int i = 0; i < count; i++) {
		auto hLayout = pls_new<QHBoxLayout>();
		hLayout->setContentsMargins(0, 0, 0, 0);
		hLayout->setSpacing(20);

		QWidget *item = nullptr;
		QLayout *subLayout = nullptr;

		const char *desc;
		const char *c_name;
		pls_property_visualizer_custom_group_item_params(prop, i, &c_name, &desc);
		auto descLabel = pls_new<QLabel>(QT_UTF8(desc));

		descLabel->setObjectName("subLabel");
		hLayout->addWidget(descLabel);

		auto type_ = pls_property_visualizer_custom_group_item_type(prop, i);
		switch (type_) {
		case PLS_CUSTOM_GROUP_UNKNOWN:
			gLayout->addLayout(hLayout, int(i) / colum, int(i) % colum);
			return;
		case PLS_CUSTOM_GROUP_INT:
			item = addIntForCustomGroup(prop, int(i));
			break;
		default:
			break;
		}

		QString name(c_name);
		if (item) {
			item->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
			//Conversion from 'size_t' (aka 'unsigned long') to 'const QVariant' is ambiguous
			item->setProperty("idx", i);
			item->setProperty("child_type", type_);
			item->setProperty("child_name", name);
			hLayout->addWidget(item);
		} else if (subLayout) {
			subLayout->setProperty("idx", i);
			subLayout->setProperty("child_type", type_);
			subLayout->setProperty("child_name", name);
			hLayout->addLayout(subLayout);
		}
		gLayout->addLayout(hLayout, int(i) / colum, int(i) % colum);
	}

	AddSpacer(obs_property_get_type(prop), layout);

	if (PROPERTY_FLAG_NO_LABEL_SINGLE & pls_property_get_flags(prop)) {
		layout->addRow(gLayout);
	} else {
		label = pls_new<QLabel>(QT_UTF8(obs_property_description(prop)));
		layout->addRow(label, gLayout);
	}
}

QWidget *PLSPropertiesView::addIntForCustomGroup(obs_property_t *prop, int index)
{
	auto spins = pls_new<PLSSpinBox>(this);
	bool isEnabled = obs_property_enabled(prop);
	spins->setObjectName(common::OBJECT_NAME_SPINBOX);
	spins->setEnabled(isEnabled);

	int val = 0;
	const char *name;
	pls_property_visualizer_custom_group_item_params(prop, index, &name, nullptr);
	if (name)
		val = (int)obs_data_get_int(settings, name);

	const char *suffix = pls_property_visualizer_custom_group_item_int_suffix(prop, index);
	if (suffix)
		spins->setSuffix(QT_UTF8(suffix));

	int minVal = 0;
	int maxVal = 0;
	int stepVal = 0;
	pls_property_visualizer_custom_group_item_int_params(prop, &minVal, &maxVal, &stepVal, index);

	spins->setMinimum(minVal);
	spins->setMaximum(maxVal);
	spins->setSingleStep(stepVal);
	spins->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	spins->setValue(val);

	auto info = pls_new<PLSWidgetInfo>(this, prop, spins);
	children.emplace_back(info);

	connect(spins, SIGNAL(valueChanged(int)), info, SLOT(UserOperation()));
	connect(spins, SIGNAL(valueChanged(int)), info, SLOT(ControlChanged()));
	connect(spins, SIGNAL(valueChanged(int)), this, SLOT(OnIntValueChanged(int)));
	return spins;
}

void PLSPropertiesView::AddPrismCheckbox(obs_property_t *prop, QFormLayout *layout, Qt::LayoutDirection layoutDirection)
{
	const char *name = obs_property_name(prop);
	bool val = obs_data_get_bool(settings, name);

	auto widget = pls_new<QWidget>(this);
	QHBoxLayout *hlayout = pls_new<QHBoxLayout>(widget);
	hlayout->setAlignment(Qt::AlignLeft);
	hlayout->setContentsMargins(0, 0, 0, 0);

	auto checkbox = pls_new<PLSCheckBox>(widget);
	checkbox->setObjectName(common::OBJECT_NAME_FORMCHECKBOX);
	checkbox->setChecked(val);

	if (layoutDirection == Qt::RightToLeft) {
		checkbox->setLayoutDirection(layoutDirection);
	}

	checkbox->setText(QT_UTF8(obs_property_description(prop)));
	hlayout->addWidget(checkbox);

	auto info = pls_new<PLSWidgetInfo>(this, prop, checkbox);
	connect(checkbox, &PLSCheckBox::clicked, info, [info](int) { info->ControlChanged(); });
	children.emplace_back(info);

	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(widget);

	return;
}

void PLSPropertiesView::AddCameraVirtualBackgroundState(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	label = pls_new<QLabel>(QT_UTF8(obs_property_description(prop)));

	QWidget *widget = pls_new<QWidget>();
	widget->setObjectName("cameraVirtualBackground");

	QHBoxLayout *hlayout = pls_new<QHBoxLayout>(widget);
	hlayout->setContentsMargins(0, 0, 0, 0);
	hlayout->setSpacing(20);

	int val = 0;

	if (auto source = pls_get_source_by_pointer_address(GetSourceObj()); source) {
		obs_data_t *privateSettings = obs_source_get_private_settings(source);
		const char *name = obs_property_name(prop);
		val = (int)obs_data_get_int(privateSettings, name);
		obs_data_release(privateSettings);
	}

	QLabel *state = pls_new<QLabel>(tr(val == 0 ? "CameraProperties.VirtualBackgroundState.State.None" : "CameraProperties.VirtualBackgroundState.State.Using"), widget);
	state->setObjectName("cameraVirtualBackgroundState");

	CameraVirtualBackgroundStateButton *button =
		pls_new<CameraVirtualBackgroundStateButton>(tr("CameraProperties.VirtualBackgroundState.State.Button"), widget, []() { pls_show_virtual_background(); });

	hlayout->addWidget(state);
	hlayout->addWidget(button);
	hlayout->addStretch(1);

	auto *wi = pls_new<PLSWidgetInfo>(this, prop, widget);
	connect(wi, &PLSWidgetInfo::PropertyUpdateNotify, state, [this, prop, state]() {
		int val_ = 0;
		if (auto source = pls_get_source_by_pointer_address(GetSourceObj()); source) {
			obs_data_t *privateSettings = obs_source_get_private_settings(source);
			const char *name = obs_property_name(prop);
			val_ = (int)obs_data_get_int(privateSettings, name);
			obs_data_release(privateSettings);
		}
		state->setText(tr(val_ == 0 ? "CameraProperties.VirtualBackgroundState.State.None" : "CameraProperties.VirtualBackgroundState.State.Using"));
	});
	children.emplace_back(wi);

	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(label, widget);
}

void PLSPropertiesView::AddVirtualBackgroundResource(obs_property_t *prop, QBoxLayout *layout)
{
	bool motionEnabled = obs_data_get_bool(settings, "motion_enabled");
	QString itemId = QString::fromUtf8(obs_data_get_string(settings, "item_id"));

	QWidget *widget = pls_create_virtual_background_resource_widget(
		nullptr,
		[prop, this](QWidget *widget_) {
			connect(widget_, SIGNAL(filterButtonClicked()), this, SLOT(OnVirtualBackgroundResourceOpenFilter()));

			auto *wi = pls_new<PLSWidgetInfo>(this, prop, widget_);
			connect(widget_, SIGNAL(checkState(bool)), wi, SLOT(VirtualBackgroundResourceMotionDisabledChanged(bool)));
			connect(widget_, SIGNAL(currentResourceChanged(QString, int, QString, QString, QString, bool, QString, QString, int)), wi,
				SLOT(VirtualBackgroundResourceSelected(QString, int, QString, QString, QString, bool, QString, QString, int)));
			connect(widget_, SIGNAL(deleteCurrentResource(QString)), wi, SLOT(VirtualBackgroundResourceDeleted(QString)));
			connect(widget_, SIGNAL(removeAllMyResource(QStringList)), wi, SLOT(VirtualBackgroundMyResourceDeleteAll(QStringList)));
			children.emplace_back(wi);
		},
		true, itemId, !motionEnabled, isFirstAddSource());

	layout->addWidget(widget, 1);
}
QWidget *PLSPropertiesView::AddSwitch(obs_property_t *prop, QFormLayout *layout)
{
	pls_unused(layout);
	const char *name = obs_property_name(prop);
	bool val = obs_data_get_bool(settings, name);
	bool isEnabled = obs_property_enabled(prop);

	QWidget *widget = pls_new<QWidget>(this);

	QHBoxLayout *hlayout = pls_new<QHBoxLayout>(widget);
	hlayout->setContentsMargins(0, 0, 0, 0);
	hlayout->setSpacing(0);

	PLSCheckBox *checkbox = pls_new<PLSCheckBox>(widget);
	checkbox->setObjectName("switch");
	checkbox->setChecked(val);
	checkbox->setEnabled(isEnabled);

	hlayout->addWidget(checkbox, 0, Qt::AlignLeft | Qt::AlignVCenter);

	NewWidget(prop, checkbox, SIGNAL(toggled(bool)));

	return widget;
}

void PLSPropertiesView::AddMobileHelp(obs_property_t *prop, QFormLayout *layout)
{
	auto desc = obs_property_description(prop);

	auto subLayout = pls_new<QHBoxLayout>();
	subLayout->setAlignment(Qt::AlignLeft);
	subLayout->setSpacing(5);
	subLayout->setContentsMargins(0, 0, 0, 0);

	auto label = pls_new<QLabel>(QT_UTF8(desc));
	label->setObjectName("prismMobileLabelHelp");
	label->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	subLayout->addWidget(label);

	auto button = pls_new<QPushButton>("", this);
	button->setObjectName("prismMobileButtonHelp");
	subLayout->addWidget(button);

	AddSpacer(obs_property_get_type(prop), layout);
	if (layout)
		layout->addRow(subLayout);
}

QWidget *PLSPropertiesView::AddMobileName(obs_property_t *prop)
{
	OBSSource source = pls_get_source_by_pointer_address(GetSourceObj());

	auto desc = pls_property_mobile_name_value(prop);
	auto button_desc = pls_property_mobile_name_button_desc(prop);
	auto button_enabled = pls_property_mobile_name_button_enabled(prop);
	auto isEmpty = pls_is_empty(desc);

	auto subWidget = pls_new<QWidget>(this);
	auto subLayout = pls_new<QHBoxLayout>(subWidget);
	subLayout->setAlignment(Qt::AlignLeft);
	subLayout->setSpacing(10);
	subLayout->setContentsMargins(0, 0, 0, 0);

	auto edit = pls_new<PLSLineEdit>();
	edit->setObjectName(common::OBJECT_NAME_LINEEDIT);
	edit->setText(QT_UTF8(desc));
	edit->setEnabled(false);
	edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	edit->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	if (edit->text().isEmpty()) {
		edit->setText(QT_UTF8(pls_property_get_placeholder(prop)));
		edit->setStyleSheet("padding-left: /*hdpi*/ 12px; color: #666666;");
	} else {
		edit->setStyleSheet("padding-left: /*hdpi*/ 12px; color: white;");
	}
	subLayout->addWidget(edit);

	auto button = pls_new<QPushButton>(QT_UTF8(button_desc), this);
	button->setEnabled(button_enabled || !isEmpty);
	button->setFixedSize({128, 40});
	subLayout->addWidget(button);

	auto info = pls_new<PLSWidgetInfo>(this, prop, button);
	connect(button, SIGNAL(clicked()), info, SLOT(UserOperation()));
	connect(button, SIGNAL(clicked()), info, SLOT(ControlChanged()));
	children.emplace_back(info);

	return subWidget;
}

QWidget *PLSPropertiesView::AddMobileStatus(obs_property_t *prop)
{
	OBSSource source = pls_get_source_by_pointer_address(GetSourceObj());

	auto desc = pls_property_mobile_state_value(prop);
	string image_url = pls_property_mobile_state_image_url(prop);

	auto subWidget = pls_new<QWidget>(this);
	auto subLayout = pls_new<QHBoxLayout>(subWidget);
	subLayout->setAlignment(Qt::AlignLeft);
	subLayout->setSpacing(5);
	subLayout->setContentsMargins(0, 0, 0, 0);

	if (!image_url.empty()) {
		auto label = pls_new<QLabel>();
		auto setPixmap = [label, image_url] { label->setPixmap(pls_load_svg(QString::fromStdString(image_url), QSize(18, 18))); };
		setPixmap();
		subLayout->addWidget(label);
	}
	if (!pls_is_empty(desc)) {
		auto label = pls_new<QLabel>(QT_UTF8(desc));
		label->setObjectName("prismMobileLabelStatus");
		subLayout->addWidget(label);
	}

	return subWidget;
}

void PLSPropertiesView::AddFontSimple(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	obs_data_t *font_obj = obs_data_get_obj(settings, name);
	const char *family = obs_data_get_string(font_obj, "font-family");
	const char *style = obs_data_get_string(font_obj, "font-weight");
	obs_data_release(font_obj);

	const char *ko_en_family = "Malgun Gothic";
	const char *ko_ko_family = "\353\247\221\354\235\200 \352\263\240\353\224\225";
	if (!strcmp(family, ko_en_family) || !strcmp(family, ko_ko_family)) {
		if (QLocale::system().language() == QLocale::Korean) {
			family = ko_ko_family;
		} else {
			family = ko_en_family;
		}
	}

	auto hlayout = pls_new<QHBoxLayout>();
	hlayout->setContentsMargins(0, 0, 0, 0);
	hlayout->setSpacing(10);

	auto fontCbx = pls_new<PLSComboBox>();
	fontCbx->setObjectName("FontCheckedFamilyBox");
	fontCbx->addItem(family);
	fontCbx->setCurrentText(family);

	auto weightCbx = pls_new<PLSComboBox>();
	weightCbx->blockSignals(true);
	updateFontSytle(family, weightCbx);
	weightCbx->setCurrentText(style);
	weightCbx->blockSignals(false);

	weightCbx->setObjectName("FontCheckedWidgetBox");

	auto _indexChanged = static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged);
	connect(fontCbx, _indexChanged, [this, weightCbx, fontCbx](int index) {
		weightCbx->blockSignals(true);
		updateFontSytle(fontCbx->itemText(index), weightCbx);
		weightCbx->blockSignals(false);
	});

	QMetaObject::invokeMethod(
		fontCbx,
		[family, fontCbx]() {
			QSignalBlocker block(fontCbx);
			fontCbx->clear();
			fontCbx->addItems(getFilteredFontFamilies());
			fontCbx->setCurrentText(family);
		},
		Qt::QueuedConnection);
	hlayout->addWidget(fontCbx);
	hlayout->addWidget(weightCbx);
	hlayout->setStretch(0, 292);
	hlayout->setStretch(1, 210);

	label = pls_new<QLabel>(QT_UTF8(obs_property_description(prop)));
	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(label, hlayout);

	auto fontWidgetInfo = pls_new<PLSWidgetInfo>(this, prop, fontCbx);
	connect(fontCbx, _indexChanged, fontWidgetInfo, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(fontWidgetInfo);

	auto fontStyleWidgetInfo = pls_new<PLSWidgetInfo>(this, prop, weightCbx);
	connect(weightCbx, _indexChanged, fontStyleWidgetInfo, &PLSWidgetInfo::ControlChanged);

	children.emplace_back(fontStyleWidgetInfo);

	if (!obs_property_enabled(prop)) {
		fontCbx->setEnabled(false);
		weightCbx->setEnabled(false);
	}
}

void PLSPropertiesView::AddColorCheckbox(obs_property_t *prop, QFormLayout *layout, QLabel *& /*label*/)
{
	auto button = pls_new<QPushButton>();
	auto colorLabel = pls_new<QLabel>();
	auto checkBox = pls_new<PLSCheckBox>(QT_UTF8(obs_property_description(prop)));

	auto subLayout = pls_new<QHBoxLayout>();
	createColorTemplate(prop, colorLabel, button, subLayout);
	checkBox->setEnabled(true);
	checkBox->setLayoutDirection(Qt::RightToLeft);
	checkBox->setObjectName(common::OBJECT_NAME_FORMCHECKBOX);

	obs_data_t *color_obj = obs_data_get_obj(settings, obs_property_name(prop));
	bool isEnable = obs_data_get_bool(color_obj, "is_enable");
	obs_data_release(color_obj);

	colorLabel->setEnabled(isEnable);
	button->setEnabled(isEnable);
	checkBox->setChecked(isEnable);

	connect(checkBox, &PLSCheckBox::clicked, [button, colorLabel](bool isCheck) {
		button->setEnabled(isCheck);
		colorLabel->setEnabled(isCheck);
	});

	AddSpacer(obs_property_get_type(prop), layout);
	if (layout)
		layout->addRow(checkBox, subLayout);

	auto info = pls_new<PLSWidgetInfo>(this, prop, checkBox);
	connect(checkBox, &PLSCheckBox::toggled, info, &PLSWidgetInfo::UserOperation);
	connect(checkBox, &PLSCheckBox::toggled, info, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(info);
}

void PLSPropertiesView::AddTemplateList(obs_property_t *prop, QFormLayout *layout)
{
	QVBoxLayout *layout2 = pls_new<QVBoxLayout>();
	layout2->setContentsMargins(0, 0, 0, 0);
	layout2->setSpacing(0);
	layout->addRow(layout2);

	if (!(pls_property_get_flags(prop) & PROPERTY_FLAG_NO_LABEL_SINGLE)) {
		auto margins = layout2->contentsMargins();
		margins.setTop(26);
		layout2->setContentsMargins(margins);
		layout2->setSpacing(17);
		QLabel *tips = pls_new<QLabel>(QT_UTF8(obs_property_description(prop)));
		tips->setObjectName("templateListDescription");
		layout2->addWidget(tips);
	}

	auto source = pls_get_source_by_pointer_address(GetSourceObj());
	pls::IPropertyModel *pmodel = pls_get_property_model(source, settings, properties.get(), prop);
	if (!pmodel) {
		return;
	}

	auto tlpmodel = dynamic_cast<pls::ITemplateListPropertyModel *>(pmodel);
	if (!tlpmodel) {
		pmodel->release();
		return;
	}

	const char *name = obs_property_name(prop);
	auto value = (int)obs_data_get_int(settings, name);
	tlpmodel->getButtons(
		this, []() {},
		[this, value, prop, layout2, tlpmodel](bool ok, pls::ITemplateListPropertyModel::IButtonGroup *group) {
			tlpmodel->release();

			if (!ok || !group) {
				return;
			}

			if (pls_object_is_valid(this) && pls_object_is_valid(layout2)) {
				group->selectButton(value);

				auto *wi = pls_new<PLSWidgetInfo>(this, prop, group->widget());
				children.emplace_back(wi);

				group->connectSlot(this, [wi](const pls::ITemplateListPropertyModel::IButton *, const pls::ITemplateListPropertyModel::IButton *) { wi->ControlChanged(); });
				layout2->addWidget(group->widget());
			} else {
				group->release();
			}
		});
}

void PLSPropertiesView::AddColorAlphaCheckbox(obs_property_t *prop, QFormLayout *layout, QLabel *&)
{
	/*
	* color alpha checkbox
	* data obj
	*	color: int
	*	enabled: bool
	*	alpha: int 0-100 (percent)
	*/
	bool propEnabled = obs_property_enabled(prop);
	obs_data_t *color_obj = obs_data_get_obj(settings, obs_property_name(prop));
	auto color = obs_data_get_int(color_obj, "color");
	bool colorEnabled = obs_data_get_bool(color_obj, "enabled");
	auto alpha = (int)obs_data_get_int(color_obj, "alpha");
	obs_data_release(color_obj);
	bool enabled = colorEnabled && propEnabled;

	auto checkbox = pls_new<PLSCheckBox>(QT_UTF8(obs_property_description(prop)));
	checkbox->setLayoutDirection(Qt::RightToLeft);
	checkbox->setObjectName(common::OBJECT_NAME_FORMCHECKBOX);
	checkbox->setChecked(colorEnabled);
	checkbox->setEnabled(propEnabled);

	auto widget = pls_new<QWidget>();
	widget->setObjectName("colorAlphaCheckboxWidget");

	auto widgetLayout = pls_new<QHBoxLayout>(widget);
	widgetLayout->setContentsMargins(0, 0, 0, 0);
	widgetLayout->setSpacing(20);

	auto colorLayout = pls_new<QHBoxLayout>();
	colorLayout->setContentsMargins(0, 0, 0, 0);
	colorLayout->setSpacing(0);

	auto colorLabel = pls_new<QLabel>();
	colorLabel->setObjectName("colorAlphaCheckbox_colorLabel");
	colorLabel->setEnabled(enabled);
	colorLabel->setAlignment(Qt::AlignCenter);
	setLabelColor(colorLabel, color, 0, false);
	auto colorButton = pls_new<QPushButton>();
	colorButton->setObjectName("colorAlphaCheckbox_colorButton");
	colorButton->setEnabled(enabled);
	colorLayout->addWidget(colorLabel);
	colorLayout->addWidget(colorButton);

	widgetLayout->addLayout(colorLayout);

	auto alphaLayout = pls_new<QHBoxLayout>();
	alphaLayout->setContentsMargins(0, 0, 0, 0);
	alphaLayout->setSpacing(20);

	auto alphaLabel = pls_new<QLabel>(tr("textmotion.opacity"));
	alphaLabel->setObjectName("colorAlphaCheckbox_alphaLabel");
	alphaLabel->setEnabled(enabled);
	alphaLabel->setAlignment(Qt::AlignCenter);
	auto alphaSlider = pls_new<SliderIgnoreScroll>(Qt::Horizontal);
	alphaSlider->setObjectName("colorAlphaCheckbox_alphaSlider");
	alphaSlider->setEnabled(enabled);
	alphaSlider->setRange(0, 100);
	alphaSlider->setSingleStep(1);
	alphaSlider->setValue(alpha);
	auto alphaSpinBox = pls_new<PLSSpinBox>();
	alphaSpinBox->setObjectName("colorAlphaCheckbox_alphaSpinBox");
	alphaSpinBox->setEnabled(enabled);
	alphaSpinBox->setSuffix("%");
	alphaSpinBox->setRange(0, 100);
	alphaSpinBox->setSingleStep(1);
	alphaSpinBox->setValue(alpha);

	alphaLayout->addWidget(alphaLabel);
	alphaLayout->addWidget(alphaSlider, 1);
	alphaLayout->addWidget(alphaSpinBox);

	widgetLayout->addLayout(alphaLayout, 1);

	auto info = pls_new<PLSWidgetInfo>(this, prop, checkbox);

	connect(colorButton, &QPushButton::clicked, this, [this, prop, colorLabel, info]() {
		obs_data_t *new_color_obj = obs_data_create();
		obs_data_t *color_obj2 = obs_data_get_obj(settings, obs_property_name(prop));
		obs_data_apply(new_color_obj, color_obj2);
		obs_data_release(color_obj2);

		auto color2 = obs_data_get_int(new_color_obj, "color");
		QColor qcolor2 = color_from_int(color2);

		QColorDialog::ColorDialogOptions options{};
		qcolor2 = PLSColorDialogView::getColor(qcolor2, this, QT_UTF8(obs_property_description(prop)), options);
		pls_check_app_exiting();
		qcolor2.setAlpha(255);
		color2 = color_to_int(qcolor2);

		if (qcolor2.isValid()) {
			setLabelColor(colorLabel, color2, 0, false);
			obs_data_set_int(new_color_obj, "color", color2);
		}

		obs_data_set_obj(settings, obs_property_name(prop), new_color_obj);
		obs_data_release(new_color_obj);
		info->ControlChanged();
	});
	connect(alphaSlider, &SliderIgnoreScroll::valueChanged, this, [this, prop, alphaSpinBox, info](int value) {
		QSignalBlocker blocker(alphaSpinBox);
		alphaSpinBox->setValue(value);

		auto name = obs_property_name(prop);
		obs_data_t *new_color_obj = obs_data_create();
		obs_data_t *color_obj2 = obs_data_get_obj(settings, name);
		obs_data_apply(new_color_obj, color_obj2);
		obs_data_release(color_obj2);

		obs_data_set_int(new_color_obj, "alpha", value);
		obs_data_set_obj(settings, name, new_color_obj);
		obs_data_release(new_color_obj);
		info->ControlChanged();
	});
	connect(alphaSpinBox, qOverload<int>(&PLSSpinBox::valueChanged), this, [this, prop, alphaSlider, info](int value) {
		QSignalBlocker blocker(alphaSlider);
		alphaSlider->setValue(value);

		auto name = obs_property_name(prop);
		obs_data_t *new_color_obj = obs_data_create();
		obs_data_t *color_obj2 = obs_data_get_obj(settings, name);
		obs_data_apply(new_color_obj, color_obj2);
		obs_data_release(color_obj2);

		obs_data_set_int(new_color_obj, "alpha", value);
		obs_data_set_obj(settings, name, new_color_obj);
		obs_data_release(new_color_obj);
		info->ControlChanged();
	});

	connect(checkbox, &PLSCheckBox::clicked, [this, prop, colorLabel, colorButton, alphaLabel, alphaSlider, alphaSpinBox, info](bool checked) {
		colorLabel->setEnabled(checked);
		colorButton->setEnabled(checked);
		alphaLabel->setEnabled(checked);
		alphaSlider->setEnabled(checked);
		alphaSpinBox->setEnabled(checked);

		auto name = obs_property_name(prop);
		obs_data_t *new_color_obj = obs_data_create();
		obs_data_t *color_obj2 = obs_data_get_obj(settings, name);
		obs_data_apply(new_color_obj, color_obj2);
		obs_data_release(color_obj2);

		obs_data_set_bool(new_color_obj, "enabled", checked);
		obs_data_set_obj(settings, name, new_color_obj);
		obs_data_release(new_color_obj);
		info->ControlChanged();
	});

	auto sourceId = getSourceId();
	if (pls_is_equal(sourceId, common::PRISM_VIEWER_COUNT_SOURCE_ID)) {
		layout->addItem(new QSpacerItem(10, common::PROPERTIES_VIEW_VERTICAL_SPACING_MIN, QSizePolicy::Fixed, QSizePolicy::Fixed));
	} else {
		AddSpacer(obs_property_get_type(prop), layout);
	}

	layout->addRow(checkbox, widget);

	connect(checkbox, &PLSCheckBox::toggled, info, &PLSWidgetInfo::UserOperation);
	children.emplace_back(info);
}

void PLSPropertiesView::ReloadPropertiesByBool(bool refreshProperties)
{
	if (!reloadCallback)
		return;

	//textmotion need create templateButton
	m_tmTabChanged = true;
	void *obj = GetSourceObj();
	if (obj) {
		properties.reset(reloadCallback(obj));
	} else {
		properties.reset(reloadCallback((void *)type.c_str()));
		obs_properties_apply_settings(properties.get(), settings);
	}

	uint32_t flags = obs_properties_get_flags(properties.get());
	deferUpdate = (flags & OBS_PROPERTIES_DEFER_UPDATE) != 0;

	if (refreshProperties) {
		RefreshProperties();
	}
}

void PLSPropertiesView::ReloadProperties()
{
	if (weakObj || rawObj) {
		OBSObject strongObj = GetObject();
		void *obj = strongObj ? strongObj.Get() : rawObj;
		if (obj) {
			//PRISM/xiewei/20230802/uiblock/enumerate video devices in work thread.
			auto source = pls_get_source_by_pointer_address(obj);

			if ((source && 0 == strcmp(obs_source_get_id(source), OBS_DSHOW_SOURCE_ID)) || isPrismLensOrMobileSource()) {

				disconnect(PLSGetPropertiesThread::Instance(), nullptr, this, nullptr);
				ShowLoading();
				connect(
					PLSGetPropertiesThread::Instance(), &PLSGetPropertiesThread::OnProperties, this,
					[this](PropertiesParam_t param) {
						if (param.id == (uint64_t)this && param.properties) {
							HideLoading();
							properties.reset(param.properties);

							uint32_t flags = obs_properties_get_flags(properties.get());
							deferUpdate = (flags & OBS_PROPERTIES_DEFER_UPDATE) != 0;

							RefreshProperties();
						}
					},
					Qt::QueuedConnection);
				PLSGetPropertiesThread::Instance()->GetPropertiesBySource(source, (uint64_t)this);
				return;
			} else
				properties.reset(reloadCallback(obj));
		}
	} else {
		properties.reset(reloadCallback((void *)type.c_str()));
		obs_properties_apply_settings(properties.get(), settings);
	}

	uint32_t flags = obs_properties_get_flags(properties.get());
	deferUpdate = (flags & OBS_PROPERTIES_DEFER_UPDATE) != 0;

	RefreshProperties();
}

void PLSPropertiesView::RefreshProperties()
{
	if (pls_is_and(m_tmHelper, m_tmTabChanged, pls_is_equal(getSourceId(), PRISM_TEXT_TEMPLATE_ID))) {
		m_tmTabChanged = false;
		m_tmHelper->initTemplateButtons();
	}
	OBSPropertiesView::RefreshProperties();
	emit PropertiesRefreshed();
	if (m_basicSettings) {
		m_basicSettings->updateStreamEncoderPropsSize(this);
	}
}

#define NO_PROPERTIES_STRING QTStr("Basic.PropertiesWindow.NoProperties")

const char *PLSPropertiesView::getSourceId() const
{
	void *obj = GetSourceObj();
	if (!obj) {
		return "";
	} else if (auto source = pls_get_source_by_pointer_address(obj); source) {
		auto id = obs_source_get_id(source);
		return id ? id : "";
	}
	return "";
}

const char *PLSPropertiesView::getSourceId(OBSSource &source) const
{
	void *obj = GetSourceObj();
	if (!obj) {
		return "";
	} else if (source = pls_get_source_by_pointer_address(obj); source) {
		auto id = obs_source_get_id(source);
		return id ? id : "";
	}
	return "";
}

bool PLSPropertiesView::isFirstAddSource() const
{
	return false;
}

PLSPropertiesView::PLSPropertiesView(OBSData settings, obs_object_t *obj_, PropertiesReloadCallback reloadCallback_, PropertiesUpdateCallback callback_, PropertiesVisualUpdateCb cb_, int minSize_,
				     int maxSize_, bool showFiltersBtn_, bool showColorFilterPath_, bool colorFilterOriginalPressed_, bool refreshProperties_, bool reloadPropertyOnInit_)
	: PLSPropertiesView(nullptr, settings, obj_, reloadCallback_, callback_, cb_, minSize_, maxSize_, showFiltersBtn_, showColorFilterPath_, colorFilterOriginalPressed_, refreshProperties_,
			    reloadPropertyOnInit_)
{
}

PLSPropertiesView::PLSPropertiesView(OBSData settings, void *obj_, PropertiesReloadCallback reloadCallback_, PropertiesUpdateCallback callback_, PropertiesVisualUpdateCb cb_, int minSize_,
				     int maxSize_, bool showFiltersBtn_, bool showColorFilterPath_, bool colorFilterOriginalPressed_, bool refreshProperties_, bool reloadPropertyOnInit_)
	: PLSPropertiesView(nullptr, settings, obj_, reloadCallback_, callback_, cb_, minSize_, maxSize_, showFiltersBtn_, showColorFilterPath_, colorFilterOriginalPressed_, refreshProperties_,
			    reloadPropertyOnInit_)
{
}

PLSPropertiesView::PLSPropertiesView(const QWidget *parent, OBSData settings, obs_object_t *obj_, PropertiesReloadCallback reloadCallback_, PropertiesUpdateCallback callback_,
				     PropertiesVisualUpdateCb cb_, int minSize_, int maxSize_, bool showFiltersBtn_, bool showColorFilterPath_, bool colorFilterOriginalPressed_,
				     bool refreshProperties_, bool reloadPropertyOnInit_)
	: OBSPropertiesView(settings, obj_, reloadCallback_, callback_, cb_, minSize_),
	  maxSize(maxSize_),
	  showColorFilterPath(showColorFilterPath_),
	  colorFilterOriginalPressed(colorFilterOriginalPressed_),
	  m_tmHelper(pls_get_text_motion_template_helper_instance())
{
	pls_unused(parent);
	setInitData(showFiltersBtn_, refreshProperties_, reloadPropertyOnInit_);
}

PLSPropertiesView::PLSPropertiesView(const QWidget *parent, OBSData settings, void *obj_, PropertiesReloadCallback reloadCallback_, PropertiesUpdateCallback callback_, PropertiesVisualUpdateCb cb_,
				     int minSize_, int maxSize_, bool showFiltersBtn_, bool showColorFilterPath_, bool colorFilterOriginalPressed_, bool refreshProperties_, bool reloadPropertyOnInit_)
	: OBSPropertiesView(settings, obj_, reloadCallback_, callback_, cb_, minSize_),
	  maxSize(maxSize_),
	  showColorFilterPath(showColorFilterPath_),
	  colorFilterOriginalPressed(colorFilterOriginalPressed_),
	  m_tmHelper(pls_get_text_motion_template_helper_instance())
{
	pls_unused(parent);
	setInitData(showFiltersBtn_, refreshProperties_, reloadPropertyOnInit_);
}

PLSPropertiesView::PLSPropertiesView(OBSData settings, const char *type, PropertiesReloadCallback reloadCallback, int minSize_, int maxSize_, bool showFiltersBtn_, bool showColorFilterPath_,
				     bool colorFilterOriginalPressed_, bool refreshProperties_, bool reloadPropertyOnInit_)
	: OBSPropertiesView(settings, type, reloadCallback, minSize_),
	  maxSize(maxSize_),
	  showColorFilterPath(showColorFilterPath_),
	  colorFilterOriginalPressed(colorFilterOriginalPressed_),
	  m_tmHelper(pls_get_text_motion_template_helper_instance())
{
	setInitData(showFiltersBtn_, refreshProperties_, reloadPropertyOnInit_);
}

PLSPropertiesView::PLSPropertiesView(OBSBasicSettings *basicSettings, OBSData settings, const char *type, PropertiesReloadCallback reloadCallback, int minSize_, int maxSize_, bool showFiltersBtn_,
				     bool showColorFilterPath_, bool colorFilterOriginalPressed_, bool refreshProperties_, bool reloadPropertyOnInit_)
	: OBSPropertiesView(settings, type, reloadCallback, minSize_),
	  maxSize(maxSize_),
	  showColorFilterPath(showColorFilterPath_),
	  colorFilterOriginalPressed(colorFilterOriginalPressed_),
	  m_tmHelper(pls_get_text_motion_template_helper_instance()),
	  m_basicSettings(basicSettings)
{
	setInitData(showFiltersBtn_, refreshProperties_, reloadPropertyOnInit_);
}

void PLSPropertiesView::setInitData(bool showFiltersBtn_, bool refreshProperties_, bool reloadPropertyOnInit_)
{
	pls_add_css(this, {"PLSPropertiesView", "PLSMotionImageListView"});
	setProperty("sourceId", QString(getSourceId()));
	OBSPropertiesView::showFiltersBtn = showFiltersBtn_;

	if (!pls_is_equal(getSourceId(), OBS_DSHOW_SOURCE_ID) && !isPrismLensOrMobileSource()) {
		if (reloadPropertyOnInit_)
			ReloadPropertiesByBool(refreshProperties_);
	}
}

void PLSPropertiesView::refreshViewAfterUIChanged(obs_property_t *p)
{
	if (visUpdateCb && !deferUpdate) {
		//callback(GetSourceObj(), settings);
		//callback(GetSourceObj(), old_settings_cache, view->settings);
		OBSObject strongObj = GetObject();
		void *obj = strongObj ? strongObj.Get() : rawObj;
		if (obj)
			visUpdateCb(obj, settings);
	}

	SignalChanged();

	if (obs_property_modified(p, settings)) {
		lastFocused = obs_property_name(p);
		QMetaObject::invokeMethod(this, "RefreshProperties", Qt::QueuedConnection);
	}
}

void PLSPropertiesView::CheckValues()
{
	auto itr = children.begin();
	while (itr != children.end()) {
		(*itr)->CheckValue();
		++itr;
	}
}

void PLSPropertiesView::addWidgetToBottom(QWidget *addWid)
{

	auto fl = dynamic_cast<QFormLayout *>(boxLayout);
	if (!fl) {
		return;
	}
	int rowCount = fl->rowCount();
	if (rowCount > 0)
		fl->removeRow(rowCount - 1);
	auto item = pls_new<QSpacerItem>(1, PROPERTIES_VIEW_VERTICAL_SPACING_MAX, QSizePolicy::Fixed);
	fl->addItem(item);
	fl->addWidget(addWid);
	QSpacerItem *spaceItem = pls_new<QSpacerItem>(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
	fl->addItem(spaceItem);
}

void PLSPropertiesView::OnColorFilterOriginalPressed(bool state)
{
	colorFilterOriginalPressed = state;
	if (spinsView && sliderView) {
		if (infoView) {
			infoView->SetOriginalColorFilter(state);
		}
		if (colorFilterOriginalPressed && isColorFilter) {
			spinsView->setValue(0);
			spinsView->setEnabled(false);
			sliderView->setEnabled(false);
			sliderView->setProperty(common::STATUS_HANDLE, false);

		} else {
			spinsView->setEnabled(true);
			sliderView->setEnabled(true);
			sliderView->setProperty(common::STATUS_HANDLE, true);
		}
		pls_flush_style(sliderView);
	}
}

void PLSPropertiesView::OnIntValueChanged(int value)
{
	if (isColorFilter) {
		emit ColorFilterValueChanged(value);
	}
}

void PLSPropertiesView::UpdateColorFilterValue(int value, bool isOriginal)
{
	if (!isColorFilter) {
		return;
	}

	if (!spinsView) {
		return;
	}

	if (infoView) {
		infoView->SetOriginalColorFilter(isOriginal);
	}
	spinsView->setValue(value);
}
void PLSPropertiesView::OnVirtualBackgroundResourceOpenFilter() const
{
	obs_frontend_open_source_filters(pls_get_source_by_pointer_address(GetSourceObj()));
}

void PLSPropertiesView::PropertyUpdateNotify(const QString &name) const
{
	for (const std::shared_ptr<WidgetInfo> child : children) {
		const char *pname = obs_property_name(child->property);
		if (pname && pname[0] && (name == QString::fromUtf8(pname))) {
			auto plsView = dynamic_cast<PLSWidgetInfo *>(child.get());
			if (plsView) {
				plsView->PropertyUpdateNotify();
			}
			break;
		}
	}
}

void PLSPropertiesView::ResetProperties(obs_properties_t *newProperties)
{
	if (!newProperties || !reloadCallback || (properties && !properties.get_deleter()))
		return;

	if (GetSourceObj()) {
		properties.reset(newProperties);
	} else {
		properties.reset(newProperties);
		obs_properties_apply_settings(properties.get(), settings);
	}

	uint32_t flags = obs_properties_get_flags(properties.get());
	deferUpdate = (flags & OBS_PROPERTIES_DEFER_UPDATE) != 0;
}

void PLSPropertiesView::creatColorList(obs_property_t *prop, QGridLayout *&gLayout, int index, const long long colorValue, const QString &colorList)
{
	auto hLayout = pls_new<QHBoxLayout>();
	hLayout->setSpacing(6);
	hLayout->setContentsMargins(0, 0, 0, 0);
	hLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	auto group = pls_new<QButtonGroup>(hLayout);
	group->setProperty("index", index);
	group->setObjectName("colorListGroup");
	auto colors = colorList.split(',');
	auto colum = static_cast<int>(colors.size());
	for (size_t i = 0; i < colum; i++) {
		QString colorStr = colors[int(i)];
		bool isChecked = colorStr.toLongLong() == colorValue;
		BorderImageButton *button = pls_new<BorderImageButton>(group, PLS_IMAGE_STYLE_BORDER_BUTTON, colorStr, int(i), isChecked, false);
		hLayout->addWidget(button);
	}

	hLayout->addStretch(1);
	gLayout->addLayout(hLayout, index, 1, 1, gLayout->columnCount());
	auto wi = pls_new<PLSWidgetInfo>(this, prop, group);
	connect(group, &QButtonGroup::buttonClicked, wi, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(wi);

	gLayout->addItem(pls_new<QSpacerItem>(10, 4, QSizePolicy::Fixed, QSizePolicy::Fixed), gLayout->rowCount(), 0);
}

void PLSPropertiesView::createTMSlider(obs_property_t *prop, int propertyValue, int minVal, int maxVal, int stepVal, int val, QHBoxLayout *&hLayout, bool isSuffix, bool, bool isShowSliderIcon,
				       const QString &sliderName)
{
	auto sliderLayout = pls_new<QHBoxLayout>();
	sliderLayout->setSpacing(6);
	if (!sliderName.isEmpty()) {
		auto sliderNameLabel = pls_new<QLabel>();
		sliderNameLabel->setText(sliderName);
		sliderNameLabel->setObjectName("sliderLabel");
		hLayout->addWidget(sliderNameLabel);
	}
	if (isShowSliderIcon) {
		auto leftSliderLabel = pls_new<QLabel>();
		leftSliderLabel->setObjectName("leftSliderIcon");
		sliderLayout->addWidget(leftSliderLabel);
	}
	auto slider = pls_new<SliderIgnoreScroll>();
	slider->setObjectName("slider");
	slider->setProperty("index", propertyValue);
	slider->setMinimum(minVal);
	slider->setMaximum(maxVal);
	slider->setPageStep(stepVal);
	slider->setValue(val);
	slider->setOrientation(Qt::Horizontal);
	sliderLayout->addWidget(slider);
	if (isShowSliderIcon) {
		auto rigthSliderLabel = pls_new<QLabel>();
		rigthSliderLabel->setObjectName("rightSliderIcon");
		sliderLayout->addWidget(rigthSliderLabel);
	}
	auto spinBox = pls_new<PLSSpinBox>();
	spinBox->setObjectName("spinBox");
	spinBox->setProperty("index", propertyValue);
	spinBox->setRange(minVal, maxVal);
	spinBox->setSingleStep(stepVal);
	spinBox->setValue(val);
	if (isSuffix) {

		spinBox->setSuffix("%");
	}
	hLayout->addLayout(sliderLayout, 1);
	hLayout->addWidget(spinBox);

	connect(slider, SIGNAL(valueChanged(int)), spinBox, SLOT(setValue(int)));
	connect(spinBox, SIGNAL(valueChanged(int)), slider, SLOT(setValue(int)));
	auto w = pls_new<PLSWidgetInfo>(this, prop, slider);
	connect(slider, &SliderIgnoreScroll::valueChanged, w, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(w);

	auto w1 = pls_new<PLSWidgetInfo>(this, prop, spinBox);
	connect(spinBox, QOverload<int>::of(&PLSSpinBox::valueChanged), w1, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(w1);
}

void PLSPropertiesView::createTMSlider(SliderIgnoreScroll *&slider, PLSSpinBox *&spinBox, obs_property_t *prop, int minVal, int maxVal, int stepVal, int val, QHBoxLayout *&hLayout, bool isSuffix,
				       bool isEnable)
{
	pls_unused(isEnable);
	slider = pls_new<SliderIgnoreScroll>();
	slider->setObjectName("slider");
	slider->setMinimum(minVal);
	slider->setMaximum(maxVal);
	slider->setPageStep(stepVal);
	slider->setValue(val);
	slider->setOrientation(Qt::Horizontal);

	spinBox = pls_new<PLSSpinBox>();
	spinBox->setObjectName("spinBox");
	spinBox->setRange(minVal, maxVal);
	spinBox->setSingleStep(stepVal);
	spinBox->setValue(val);
	if (isSuffix) {

		spinBox->setSuffix("%");
	}
	hLayout->addWidget(slider, 1);
	hLayout->addWidget(spinBox);

	connect(slider, SIGNAL(valueChanged(int)), spinBox, SLOT(setValue(int)));
	connect(spinBox, SIGNAL(valueChanged(int)), slider, SLOT(setValue(int)));
	auto w = pls_new<PLSWidgetInfo>(this, prop, slider);
	connect(slider, &SliderIgnoreScroll::valueChanged, w, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(w);

	auto w1 = pls_new<PLSWidgetInfo>(this, prop, spinBox);
	connect(spinBox, QOverload<int>::of(&PLSSpinBox::valueChanged), w1, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(w1);
}

void PLSPropertiesView::createTMColorCheckBox(PLSCheckBox *&controlCheckBox, obs_property_t *prop, QFrame *&frame, int index, const QString &labelName, const QHBoxLayout *layout, bool isControlOn,
					      bool isControl)
{
	pls_unused(layout, isControl);
	auto bkLabelLalyout = pls_new<QHBoxLayout>();
	bkLabelLalyout->setSpacing(6);
	bkLabelLalyout->setContentsMargins(0, 0, 0, 0);
	auto bkLabel = pls_new<QLabel>(labelName);
	bkLabel->setObjectName("bk_outlineLabel");
	auto bkColorCheck = pls_new<TMCheckBox>();
	controlCheckBox = bkColorCheck;
	bkColorCheck->setObjectName("checkBox");
	bkColorCheck->setProperty("index", index);
	bkLabelLalyout->addWidget(bkLabel);
	bkLabelLalyout->addWidget(bkColorCheck);
	bkColorCheck->setChecked(isControlOn);
	if (isControl) {
		bkColorCheck->setEnabled(!isControl);
	}
	bkLabelLalyout->setAlignment(Qt::AlignLeft);

	auto checkInfo = pls_new<PLSWidgetInfo>(this, prop, bkColorCheck);
	connect(bkColorCheck, &TMCheckBox::clicked, [checkInfo](bool) { checkInfo->ControlChanged(); });

	frame->setLayout(bkLabelLalyout);
}

void PLSPropertiesView::createColorButton(obs_property_t *prop, QGridLayout *&gLayout, const PLSCheckBox *checkBox, const QString &opationName, int index, bool isSuffix, bool)
{
	const char *name = obs_property_name(prop);
	obs_data_t *val = obs_data_get_obj(settings, name);
	bool isColor = obs_data_get_bool(val, "is-bk-color");

	auto layoutIndex = index;
	if (index > 0) {
		layoutIndex = isColor ? index : index - 1;
	}
	auto button = pls_new<QPushButton>();
	button->setObjectName("textColorBtn");
	auto colorLabel = pls_new<QLabel>();
	colorLabel->setProperty("index", index);
	colorLabel->setObjectName("colorLabel");
	colorLabel->setAlignment(Qt::AlignCenter);

	auto layout = pls_new<QHBoxLayout>();
	layout->setAlignment(Qt::AlignLeft);
	layout->addWidget(colorLabel);
	layout->addWidget(button);
	layout->setSpacing(0);
	gLayout->addLayout(layout, layoutIndex, 1);

	bool isControlOn = false;
	bool colorControl = false;
	bool isAlaph;
	long long colorValue = 0;
	int alaphValue = 0;
	getTmColor(val, index, isControlOn, colorControl, colorValue, isAlaph, alaphValue);
	setLabelColor(colorLabel, colorValue, alaphValue);

	auto info = pls_new<PLSWidgetInfo>(this, prop, colorLabel);
	connect(button, &QPushButton::clicked, info, &PLSWidgetInfo::ControlChanged);
	children.emplace_back(info);

	auto opationLabel = pls_new<QLabel>(opationName);
	opationLabel->setObjectName("sliderLabel");
	gLayout->addWidget(opationLabel, layoutIndex, 2);

	int minVal = pls_property_tm_text_min(prop, PLS_PROPERTY_TM_COLOR);
	int maxVal = pls_property_tm_text_max(prop, PLS_PROPERTY_TM_COLOR);
	int stepVal = pls_property_tm_text_step(prop, PLS_PROPERTY_TM_COLOR);

	if (2 == index) {
		//outline slider value
		minVal = 1;
		maxVal = 20;
		stepVal = 1;
		alaphValue = static_cast<int>(obs_data_get_int(val, "outline-color-line"));
	}
	auto sliderHLayout = pls_new<QHBoxLayout>();
	sliderHLayout->setSpacing(20);
	sliderHLayout->setContentsMargins(0, 0, 0, 0);
	gLayout->addLayout(sliderHLayout, layoutIndex, 3);
	createTMSlider(prop, index, minVal, maxVal, stepVal, alaphValue, sliderHLayout, isSuffix, isControlOn, false);

	bool enabled = checkBox ? checkBox->isChecked() : isControlOn;
	setLayoutEnable(layout, enabled);
	setLayoutEnable(sliderHLayout, enabled);
	opationLabel->setEnabled(enabled);

	if (checkBox) {
		connect(checkBox, &PLSCheckBox::clicked, [this, layout, opationLabel, sliderHLayout](bool isChecked) {
			setLayoutEnable(layout, isChecked);
			setLayoutEnable(sliderHLayout, isChecked);
			opationLabel->setEnabled(isChecked);
		});
	}
	obs_data_release(val);
}

void PLSPropertiesView::setLabelColor(QLabel *label, const long long colorValue, const int, bool frameStyle) const
{
	QColor color = color_from_int(colorValue);
	color.setAlpha(255);
	auto palette = QPalette(color);
	if (frameStyle)
		label->setFrameStyle(QFrame::Sunken | QFrame::Panel);
	label->setText(color.name(QColor::HexRgb));
	label->setPalette(palette);
	label->setStyleSheet(
		QString("font-weight: normal;background-color :%1; color: %2;").arg(palette.color(QPalette::Window).name(QColor::HexRgb)).arg(palette.color(QPalette::WindowText).name(QColor::HexRgb)));
	label->setAutoFillBackground(true);
	pls_flush_style(label);
}

void PLSPropertiesView::getTmColor(obs_data_t *textData, int tabIndex, bool &isControlOn, bool &isColor, long long &color, bool &isAlaph, int &alaph) const
{
	switch (tabIndex) {
	case 0:
		isColor = obs_data_get_bool(textData, "is-color");
		isControlOn = true;
		color = obs_data_get_int(textData, "text-color");
		isAlaph = obs_data_get_bool(textData, "is-text-color-alpha");
		alaph = static_cast<int>(obs_data_get_int(textData, "text-color-alpha"));
		return;
	case 1:
		isControlOn = obs_data_get_bool(textData, "is-bk-color-on");
		isColor = obs_data_get_bool(textData, "is-bk-color");
		color = obs_data_get_int(textData, "bk-color");
		isAlaph = obs_data_get_bool(textData, "is-bk-color-alpha");
		alaph = static_cast<int>(obs_data_get_int(textData, "bk-color-alpha"));
		return;
	case 2:
		isControlOn = obs_data_get_bool(textData, "is-outline-color-on");
		isColor = obs_data_get_bool(textData, "is-outline-color");
		color = obs_data_get_int(textData, "outline-color");
		isAlaph = true;
		alaph = static_cast<int>(obs_data_get_int(textData, "outline-color-line"));
		return;
	default:
		break;
	}
}

void PLSPropertiesView::createTMButton(const int buttonCount, obs_data_t *obs_data, QHBoxLayout *&hLayout, QButtonGroup *&group, ButtonType buttonType, const QStringList &buttonObjs, bool isShowText,
				       bool) const
{
	pls_unused(obs_data);
	for (int index = 0; index != buttonCount; ++index) {
		QAbstractButton *button = nullptr;
		switch (buttonType) {
		case ButtonType::RadioButon:
			button = pls_new<QRadioButton>();
			break;
		case ButtonType::PushButton:
			button = pls_new<QPushButton>();
			break;
		case ButtonType::CustomButton:
			button = pls_new<TMTextAlignBtn>(buttonObjs.value(index));
			break;
		case ButtonType::LetterButton:
			button = pls_new<TMTextAlignBtn>(buttonObjs.value(index), false, false);
			break;
		default:
			break;
		}

		button->setAutoExclusive(true);
		button->setCheckable(true);
		if (buttonObjs.count() == buttonCount) {
			button->setObjectName(buttonObjs.value(index));
			if (isShowText) {
				button->setText(buttonObjs.value(index));
			}
		}

		group->addButton(button, index);
		hLayout->addWidget(button);
	}
}

void PLSPropertiesView::createRadioButton(const int buttonCount, obs_data_t *textData, QHBoxLayout *&hLayout, PLSRadioButtonGroup *&group, const QStringList &buttonObjs, bool isShowText,
					  QWidget *parent)
{
	for (int index = 0; index != buttonCount; ++index) {
		auto button = pls_new<PLSRadioButton>(parent);
		button->setCheckable(true);
		if (buttonObjs.count() == buttonCount) {
			button->setObjectName(buttonObjs.value(index));
			if (isShowText) {
				button->setText(buttonObjs.value(index));
			}
		}

		group->addButton(button, index);
		hLayout->addWidget(button);
	}
}

void PLSPropertiesView::creatTMTextWidget(obs_property_t *prop, const int textCount, obs_data_t *textData, QHBoxLayout *&hLayout)
{
	bool isTmControlOkEnable = true;
	QStringList disableTempalteName = {"poweredbyPRISMLive", "freeshipping", "notice1"};
	auto nameInter = find_if(disableTempalteName.constBegin(), disableTempalteName.constEnd(), [this](const QString &tempName) {
		auto name = obs_data_get_string(settings, "templateName");
		return (0 == strcmp(tempName.toUtf8().constData(), name));
	});
	bool isEqual = (nameInter != disableTempalteName.constEnd());
	for (int index = 0; index != textCount; ++index) {
		QString text = QT_UTF8(obs_data_get_string(textData, QString("text-content-%1").arg(index + 1).toUtf8()));
#if 0
		bool isChangeContent = obs_data_get_bool(textData, "text-content-change-1");
#endif
		auto edit = pls_new<QPlainTextEdit>(text);
		edit->horizontalScrollBar()->setVisible(false);
		edit->setObjectName(QString("%1_%2").arg(common::OBJECT_NAME_PLAINTEXTEDIT).arg(index + 1));
		edit->setFrameShape(QFrame::NoFrame);
		auto wi = pls_new<PLSWidgetInfo>(this, prop, edit);
		edit->setEnabled(!isEqual);
		connect(edit, &QPlainTextEdit::textChanged, [prop, edit, wi, this]() {
			if (edit->toPlainText().length() > MAX_TM_TEXT_CONTENT_LENGTH) {
				QSignalBlocker signalBlocker(edit);
				edit->setPlainText(edit->toPlainText().left(MAX_TM_TEXT_CONTENT_LENGTH));
				edit->moveCursor(QTextCursor::End);
			}
			const char *name = obs_property_name(prop);
			obs_data_t *val = obs_data_get_obj(settings, name);
			bool isTmControlOkEnable_ = true;
			if (edit->objectName() == "plainTextEdit_1" && (2 == obs_data_get_int(val, "text-count"))) {
				isTmControlOkEnable_ = obs_data_get_string(val, "text-content-2")[0] != '\0';
			} else if (edit->objectName() == "plainTextEdit_2" && edit->isVisible()) {
				isTmControlOkEnable_ = obs_data_get_string(val, "text-content-1")[0] != '\0';
			}
			isTmControlOkEnable_ = isTmControlOkEnable_ && (!edit->toPlainText().trimmed().isEmpty());
			okButtonControl(isTmControlOkEnable_);

			wi->ControlChanged();
			obs_data_release(val);
		});

		children.emplace_back(wi);
		hLayout->addWidget(edit);
		if (isTmControlOkEnable && !text.trimmed().isEmpty()) {
			isTmControlOkEnable = true;
		} else {
			isTmControlOkEnable = false;
		}
	}

	if (isEqual) {
		isTmControlOkEnable = isEqual;
	}
	QMetaObject::invokeMethod(
		this, [isTmControlOkEnable, this]() { okButtonControl(isTmControlOkEnable); }, Qt::QueuedConnection);
}

void PLSPropertiesView::updateTMTemplateButtons(const int, const QString &templateTabName, QGridLayout *gLayout)
{

	if (!gLayout) {
		return;
	}
	const QLayoutItem *child = nullptr;
	while ((child = gLayout->takeAt(0)) != nullptr) {
		QWidget *w = child->widget();
		w->setParent(nullptr);
	}
	auto buttonGroup = m_tmHelper->getTemplateButtons(templateTabName);
	if (nullptr == buttonGroup) {
		return;
	}
	auto buttons = buttonGroup->buttons();
	for (int index = 0; index != buttons.count(); ++index) {
		auto button = buttons.value(index);
		int Id = button->property("ID").toInt();
		buttonGroup->setId(button, Id);
		gLayout->addWidget(button, index / 4, (index % 4));
		pls_template_button_refresh_gif_geometry(button);
	}
}

void PLSPropertiesView::updateFontSytle(const QString &family, PLSComboBox *fontStyleBox) const
{
	if (fontStyleBox) {
		fontStyleBox->clear();
		if (!family.isEmpty()) {
			fontStyleBox->addItems(QFontDatabase::styles(family));
		}
	}
}

void PLSPropertiesView::setLayoutEnable(const QLayout *layout, bool isEnable)
{
	if (!layout)
		return;
	int count = layout->count();
	for (auto index = 0; index != count; ++index) {
		auto w = layout->itemAt(index)->widget();
		if (w) {
			w->setEnabled(isEnable);
		} else {
			setLayoutEnable(layout->itemAt(index)->layout(), isEnable);
		}
	}
}

void PLSPropertiesView::createColorTemplate(obs_property_t *prop, QLabel *colorLabel, QPushButton *button, QHBoxLayout *subLayout)
{
	colorLabel->setObjectName("baseColorLabel");
	const char *name = obs_property_name(prop);

	obs_property_type p_type = obs_property_get_type(prop);
	long long val = 0;
	if ((int)p_type == (int)PLS_PROPERTY_COLOR_CHECKBOX) {
		obs_data_t *color_obj = obs_data_get_obj(settings, name);
		val = obs_data_get_int(color_obj, "color_val");
		obs_data_release(color_obj);
	} else {
		val = obs_data_get_int(settings, name);
	}

	QColor color = color_from_int(val);

	button->setProperty("themeID", "settingsButtons");
	button->setText(QTStr("Basic.PropertiesWindow.SelectColor"));
	button->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	button->setStyleSheet("font-weight:bold;");

	color.setAlpha(255);

	auto palette = QPalette(color);
	colorLabel->setFrameStyle(QFrame::Sunken | QFrame::Panel);
	colorLabel->setText(color.name(QColor::HexRgb));
	colorLabel->setPalette(palette);
	colorLabel->setStyleSheet(QString("background-color :%1; color: %2;").arg(palette.color(QPalette::Window).name(QColor::HexRgb)).arg(palette.color(QPalette::WindowText).name(QColor::HexRgb)));
	colorLabel->setAutoFillBackground(true);
	colorLabel->setAlignment(Qt::AlignCenter);
	colorLabel->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	subLayout->setContentsMargins(0, 0, 0, 0);

	subLayout->addWidget(colorLabel);
	subLayout->addSpacing(10);
	subLayout->addWidget(button);

	auto info = pls_new<PLSWidgetInfo>(this, prop, colorLabel);
	connect(button, SIGNAL(clicked()), info, SLOT(UserOperation()));
	connect(button, SIGNAL(clicked()), info, SLOT(ControlChanged()));
	children.emplace_back(info);
}

void PLSPropertiesView::setPlaceholderColor_666666(QWidget *widget) const
{
	QPalette palette;
	palette.setColor(QPalette::All, QPalette::PlaceholderText, QColor("#666666"));
	widget->setPalette(palette);
}

QWidget *getPropertiesTopWidget(QWidget *w)
{
	if (!w)
		return nullptr;

	while (w) {
		auto parent = w->parentWidget();
		if (auto top = qobject_cast<PLSDialogView *>(parent); top) {
			return top->content();
		}
		w = parent;
	}

	return nullptr;
}

void PLSPropertiesView::ShowLoading()
{
	auto parent = getPropertiesTopWidget(this);

	if (!m_loadingPage) {
		m_loadingPage = PLSUIFunc::showLoadingView(parent, QTStr("main.camera.loading.devicelist"));
	}
}

void PLSPropertiesView::HideLoading()
{
	if (m_loadingPage)
		delete m_loadingPage;
}

void PLSWidgetInfo::UserOperation() const
{
	//the obs is removed this method?
	//this is print log
}

void PLSWidgetInfo::ControlChanged()
{
	obs_property_type type = obs_property_get_type(property);
	if ((pls_property_type)type < OBS_PROPERTY_BASE) {
		WidgetInfo::ControlChanged();
		return;
	}
	const char *setting = obs_property_name(property);

	if (!recently_updated) {
		old_settings_cache = obs_data_create();
		obs_data_apply(old_settings_cache, view->settings);
		obs_data_release(old_settings_cache);
	}
	if (type == OBS_PROPERTY_INVALID) {
		return;
	}
	switch ((pls_property_type)type) {
	case PLS_PROPERTY_TIPS: //OBS_PROPERTY_MOBILE_GUIDER
		break;
	case PLS_PROPERTY_LINE: // OBS_PROPERTY_H_LINE:
		break;
	case PLS_PROPERTY_BOOL_GROUP:
		if (!BoolGroupChanged(setting))
			return;
		break;
	case PLS_PROPERTY_BUTTON_GROUP:
		ButtonGroupClicked(setting);
		return;
	case PLS_PROPERTY_CUSTOM_GROUP:
		CustomButtonGroupClicked(setting);
		break;
	case PLS_PROPERTY_CHAT_TEMPLATE_LIST:
		ChatTemplateListChanged(setting);
		break;
	case PLS_PROPERTY_CHAT_FONT_SIZE:
		ChatFontSizeChanged(setting);
		break;
	case PLS_PROPERTY_TM_TAB:
		TMTextTabChanged(setting);
		break;
	case PLS_PROPERTY_TM_TEMPLATE_TAB:
		TMTextTemplateTabChanged(setting);
		break;
	case PLS_PROPERTY_TM_TEMPLATE_LIST:
		TMTextTemplateListChanged(setting);
		break;

	case PLS_PROPERTY_TM_DEFAULT_TEXT:
		// NO ?
		break;
	case PLS_PROPERTY_TM_TEXT_CONTENT:
		TMTextContentChanged(setting);
		break;
	case PLS_PROPERTY_TM_TEXT:
		TMTextChanged(setting);
		break;
	case PLS_PROPERTY_TM_COLOR:
		TMTextColorChanged(setting);
		break;
	case PLS_PROPERTY_TM_MOTION:
		TMTextMotionChanged(setting);
		break;
	case PLS_PROPERTY_REGION_SELECT:
		SelectRegionClicked(setting);
		break;
	case PLS_PROPERTY_IMAGE_GROUP:
		ImageGroupChanged(setting);
		break;
	case PLS_PROPERTY_VISUALIZER_CUSTOM_GROUP:
		CustomGroupChanged(setting);
		break;
	case PLS_PROPERTY_BOOL_LEFT:
		WidgetInfo::BoolChanged(setting);
		break;
	case PLS_PROPERTY_MOBILE_NAME: //OBS_PROPERTY_MOBILE_NAME:
		TextButtonClicked();
		break;
	case PLS_PROPERTY_FONT_SIMPLE: //OBS_PROPERTY_FONT_SIMPLE:
		FontSimpleChanged(setting);
		break;
	case PLS_PROPERTY_COLOR_CHECKBOX: //OBS_PROPERTY_COLOR_CHECKBOX:
		ColorCheckBoxChanged(setting);
		break;
	case PLS_PROPERTY_AUDIO_METER: //OBS_PROPERTY_AUDIO_METER:
		break;
	case PLS_PROPERTY_TEMPLATE_LIST: // OBS_PROPERTY_TEMPLATE_LIST :
		templateListChanged(setting);
		break;
	case PLS_PROPERTY_COLOR_ALPHA_CHECKBOX: //OBS_PROPERTY_COLOR_ALPHA_CHECKBOX:
		break;
	default:
		break;
	}

	ControlChangedToRefresh(setting);
}

void PLSWidgetInfo::VirtualBackgroundResourceMotionDisabledChanged(bool motionDisabled)
{
	obs_data_set_bool(view->settings, "motion_enabled", !motionDisabled);

	ControlChanged();
}

static QString processResourcePath(bool prismResource, const QString &resourcePath)
{
	if (prismResource)
		return pls_get_relative_config_path(resourcePath);
	return resourcePath;
}

void PLSWidgetInfo::VirtualBackgroundResourceSelected(const QString &itemId, int type, const QString &resourcePath, const QString &staticImgPath, const QString &thumbnailPath, bool prismResource,
						      const QString &foregroundPath, const QString &foregroundStaticImgPath, int dataType)
{
	Q_UNUSED(foregroundPath)
	Q_UNUSED(foregroundStaticImgPath)

	PLS_UI_STEP(PROPERTY_MODULE, "property-window:virtual background resource", ACTION_CLICK);

	obs_data_set_bool(view->settings, "prism_resource", prismResource);
	obs_data_set_string(view->settings, "item_id", itemId.toUtf8().constData());
	obs_data_set_int(view->settings, "item_type", type);
	obs_data_set_string(view->settings, "file_path", processResourcePath(prismResource, resourcePath).toUtf8().constData());
	obs_data_set_string(view->settings, "image_file_path", processResourcePath(prismResource, staticImgPath).toUtf8().constData());
	obs_data_set_string(view->settings, "thumbnail_file_path", processResourcePath(prismResource, thumbnailPath).toUtf8().constData());
	obs_data_set_int(view->settings, "category_id", dataType);

	ControlChanged();
}

void PLSWidgetInfo::VirtualBackgroundResourceDeleted(const QString &itemId)
{
	const char *currentItemId = obs_data_get_string(view->settings, "item_id");
	if (!currentItemId || !currentItemId[0]) {
		view->reloadOldSettings();
	} else if (QString::fromUtf8(currentItemId) == itemId) {
		obs_data_set_bool(view->settings, "prism_resource", false);
		obs_data_set_string(view->settings, "item_id", "");
		obs_data_set_int(view->settings, "item_type", 0);
		obs_data_set_string(view->settings, "file_path", "");
		obs_data_set_string(view->settings, "image_file_path", "");
		obs_data_set_string(view->settings, "thumbnail_file_path", "");

		view->reloadOldSettings();
		ControlChanged();
	}
}

void PLSWidgetInfo::VirtualBackgroundMyResourceDeleteAll(const QStringList &itemIds)
{
	const char *currentItemId = obs_data_get_string(view->settings, "item_id");
	if (!currentItemId || !currentItemId[0]) {
		view->reloadOldSettings();
	} else if (itemIds.contains(QString::fromUtf8(currentItemId))) {
		obs_data_set_bool(view->settings, "prism_resource", false);
		obs_data_set_string(view->settings, "item_id", "");
		obs_data_set_int(view->settings, "item_type", 0);
		obs_data_set_string(view->settings, "file_path", "");
		obs_data_set_string(view->settings, "image_file_path", "");

		view->reloadOldSettings();
		ControlChanged();
	}
}

void PLSWidgetInfo::ListChanged(const char *setting)
{

	bool isTimer = PROPERTY_FLAG_LIST_TIMER_LISTEN & pls_property_get_flags(property);
	if (!isTimer) {
		WidgetInfo::ListChanged(setting);
		return;
	}

	bool isSelect = obs_data_get_bool(view->settings, "listen_list_btn");
	QString objName = object->objectName();
	if (OBJECT_NAME_COMBOBOX == objName) {
		WidgetInfo::ListChanged(setting);
		if (isSelect) {
			obs_data_set_bool(view->settings, "listen_list_btn", !isSelect);
		}
	} else {
		obs_data_set_bool(view->settings, "listen_list_btn", !isSelect);
	}
}

bool PLSWidgetInfo::BoolGroupChanged(const char *setting)
{
	auto radiobutton = static_cast<PLSRadioButton *>(widget);
	int idx = radiobutton->property("idx").toInt();

	const auto *plsView = dynamic_cast<PLSPropertiesView *>(view);
	if (!plsView) {
		return false;
	}
	if (pls_is_equal(plsView->getSourceId(), common::PRISM_TIMER_SOURCE_ID) && obs_data_get_int(view->settings, setting) == idx) {
		PLS_INFO(PROPERTY_MODULE, "PropertyOperation %s: %d ignored, because it's already on", setting, idx);
		return false;
	}
	obs_data_set_int(view->settings, setting, idx);
	PLS_INFO(PROPERTY_MODULE, "PropertyOperation %s: %d", setting, idx);

	if (pls_property_bool_group_clicked(property, view->GetSourceObj(), idx)) {
		QMetaObject::invokeMethod(view, "RefreshProperties", Qt::QueuedConnection);
	}

	return true;
}

void PLSWidgetInfo::ButtonGroupClicked(const char *)
{
	auto button = static_cast<PLSPushButton *>(widget);
	int idx = button->property("idx").toInt();
	if (pls_property_button_group_clicked(property, view->GetSourceObj(), idx)) {
	}
}
void PLSWidgetInfo::CustomButtonGroupClicked(const char *setting)
{
	pls_unused(setting);
	QString childName = widget->property("child_name").toString();
	WidgetInfo::IntChanged(QT_TO_UTF8(childName));
}

void PLSWidgetInfo::SetOriginalColorFilter(bool state)
{
	isOriginColorFilter = state;
}

void PLSWidgetInfo::ChatTemplateListChanged(const char *setting)
{
	auto buttonGroup = static_cast<QButtonGroup *>(object);
	obs_data_set_int(view->settings, setting, buttonGroup->checkedId());
}

void PLSWidgetInfo::ChatFontSizeChanged(const char *setting)
{
	auto spinBox = static_cast<PLSSpinBox *>(widget);
	obs_data_set_int(view->settings, setting, spinBox->value());
}

void PLSWidgetInfo::TMTextChanged(const char *setting)
{
	obs_data_t *tm_text_obj = obs_data_get_obj(view->settings, setting);
	QString objName = object->objectName();
	obs_data_t *tm_new_text_obj = obs_data_create();
	obs_data_set_bool(tm_new_text_obj, "is-font-settings", obs_data_get_bool(tm_text_obj, "is-font-settings"));
	obs_data_set_bool(tm_new_text_obj, "is-font", obs_data_get_bool(tm_text_obj, "is-font"));
	obs_data_set_bool(tm_new_text_obj, "is-font-size", obs_data_get_bool(tm_text_obj, "is-font-size"));
	obs_data_set_bool(tm_new_text_obj, "is-box-size", obs_data_get_bool(tm_text_obj, "is-box-size"));
	obs_data_set_string(tm_new_text_obj, "font-family", obs_data_get_string(tm_text_obj, "font-family"));
	obs_data_set_string(tm_new_text_obj, "font-weight", obs_data_get_string(tm_text_obj, "font-weight"));
	obs_data_set_int(tm_new_text_obj, "font-size", obs_data_get_int(tm_text_obj, "font-size"));
	obs_data_set_bool(tm_new_text_obj, "is-h-aligin", obs_data_get_bool(tm_text_obj, "is-h-aligin"));
	obs_data_set_int(tm_new_text_obj, "h-aligin", obs_data_get_int(tm_text_obj, "h-aligin"));
	obs_data_set_int(tm_new_text_obj, "letter", obs_data_get_int(tm_text_obj, "letter"));

	if ("tmFontBox" == objName) {
		QString currentFamily(static_cast<PLSComboBox *>(object)->currentText());
		QStringList styles(QFontDatabase::styles(currentFamily));
		QString weight;
		if (!styles.isEmpty()) {
			weight = styles.first();
		}
		obs_data_set_string(tm_new_text_obj, "font-family", currentFamily.toUtf8());
		obs_data_set_string(tm_new_text_obj, "font-weight", weight.toUtf8());
		PLS_UI_STEP(PROPERTY_MODULE, "property window:family ", ACTION_CLICK);

	} else if ("tmFontStyleBox" == objName) {
		QString weight(static_cast<PLSComboBox *>(object)->currentText());
		obs_data_set_string(tm_new_text_obj, "font-weight", weight.toUtf8());
		PLS_UI_STEP(PROPERTY_MODULE, "property window:font-weight ", ACTION_CLICK);

	} else if (object->property("index").isValid() && "spinBox" == objName) {

		switch (object->property("index").toInt()) {
		case 0:
			obs_data_set_int(tm_new_text_obj, "font-size", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:font-size ", ACTION_CLICK);

			break;
		case 1:
			obs_data_set_int(view->settings, "width", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:box width ", ACTION_CLICK);

			break;
		case 2:
			obs_data_set_int(view->settings, "height", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:box height ", ACTION_CLICK);

			break;
		default:
			break;
		}

	} else if (object->property("index").isValid() && "slider" == objName) {
		switch (object->property("index").toInt()) {
		case 0:
			obs_data_set_int(tm_new_text_obj, "font-size", static_cast<QSlider *>(object)->value());
			break;
		case 1:
			obs_data_set_int(view->settings, "width", static_cast<QSlider *>(object)->value());
			break;
		case 2:
			obs_data_set_int(view->settings, "height", static_cast<QSlider *>(object)->value());
			break;
		default:
			break;
		}

	} else if ("group" == objName) {
		int checkId = static_cast<QButtonGroup *>(object)->checkedId();
		if (checkId >= 0) {
			obs_data_set_int(tm_new_text_obj, "h-aligin", checkId);
		}
		PLS_UI_STEP(PROPERTY_MODULE, "property window:h-aligin ", ACTION_CLICK);

	} else if ("group2" == objName) {
		int checkId = static_cast<QButtonGroup *>(object)->checkedId();
		if (checkId >= 0) {
			obs_data_set_int(tm_new_text_obj, "letter", checkId);
		} else {
			obs_data_set_int(tm_new_text_obj, "letter", -1);
		}
		PLS_UI_STEP(PROPERTY_MODULE, "property window:letter ", ACTION_CLICK);
	}
	obs_data_set_obj(view->settings, setting, tm_new_text_obj);
	obs_data_release(tm_text_obj);
	obs_data_release(tm_new_text_obj);
}
void PLSWidgetInfo::TMTextContentChanged(const char *setting)
{
	obs_data_t *text_content_obj = obs_data_get_obj(view->settings, setting);
	obs_data_t *text_net_content_obj = obs_data_create();

	obs_data_set_int(text_net_content_obj, "text-count", obs_data_get_int(text_content_obj, "text-count"));
	obs_data_set_string(text_net_content_obj, "text-content-1", obs_data_get_string(text_content_obj, "text-content-1"));
	obs_data_set_bool(text_net_content_obj, "text-content-change-1", obs_data_get_bool(text_content_obj, "text-content-change-1"));
	obs_data_set_string(text_net_content_obj, "text-content-2", obs_data_get_string(text_content_obj, "text-content-2"));
	obs_data_set_bool(text_net_content_obj, "text-content-change-2", obs_data_get_bool(text_content_obj, "text-content-change-2"));

	auto edit = static_cast<QPlainTextEdit *>(object);
	QString editStr = edit->toPlainText().trimmed();
	if (edit->objectName() == QString("%1_%2").arg(common::OBJECT_NAME_PLAINTEXTEDIT).arg(1)) {

		obs_data_set_string(text_net_content_obj, "text-content-1", editStr.toUtf8());
		obs_data_set_bool(text_net_content_obj, "text-content-change-1", true);

	} else if (edit->objectName() == QString("%1_%2").arg(common::OBJECT_NAME_PLAINTEXTEDIT).arg(2)) {
		obs_data_set_string(text_net_content_obj, "text-content-2", editStr.toUtf8());
		obs_data_set_bool(text_net_content_obj, "text-content-change-2", true);
	}
	obs_data_set_obj(view->settings, setting, text_net_content_obj);
	obs_data_release(text_content_obj);
	obs_data_release(text_net_content_obj);
}

void PLSWidgetInfo::TMTextTabChanged(const char *setting)
{
	auto buttonGroup = static_cast<QButtonGroup *>(object);
	int checkId = buttonGroup->checkedId();
	auto oldCheckId = obs_data_get_int(view->settings, setting);
	if (checkId != oldCheckId) {
		obs_data_set_int(view->settings, setting, checkId);
	}
}

void PLSWidgetInfo::TMTextTemplateTabChanged(const char *setting)
{
	TMTextTemplateListChanged(setting);
}

void PLSWidgetInfo::TMTextTemplateListChanged(const char *setting)
{
	auto buttonGroup = static_cast<QButtonGroup *>(object);
	int checkId = buttonGroup->checkedId();
	auto oldCheckId = obs_data_get_int(view->settings, setting);
	if (checkId >= 0 && oldCheckId != checkId) {
		obs_data_set_int(view->settings, setting, checkId);
	}
}

static bool _isValidComparedObj(const QObject *obj, const char *keyStr, const QString &objName)
{
	return obj->property(keyStr).isValid() && objName == obj->objectName();
}

void PLSWidgetInfo::TMTextColorChanged(const char *setting)
{
	bool isNeedRefresh = false;

	obs_data_t *tm_color = obs_data_get_obj(view->settings, setting);
	QString objName = object->objectName();

	obs_data_t *tm_new_color = obs_data_create();
	obs_data_set_bool(tm_new_color, "is-color-settings", obs_data_get_bool(tm_color, "is-color-settings"));

	obs_data_set_int(tm_new_color, "text-color", obs_data_get_int(tm_color, "text-color"));
	obs_data_set_int(tm_new_color, "text-color-alpha", obs_data_get_int(tm_color, "text-color-alpha"));
	obs_data_set_bool(tm_new_color, "text-color-change", obs_data_get_bool(tm_color, "text-color-change"));
	obs_data_set_bool(tm_new_color, "is-color", obs_data_get_bool(tm_color, "is-color"));
	obs_data_set_bool(tm_new_color, "is-text-color-alpha", obs_data_get_bool(tm_color, "is-text-color-alpha"));
	obs_data_set_string(tm_new_color, "text-color-list", obs_data_get_string(tm_color, "text-color-list"));
	obs_data_set_bool(tm_new_color, "is-bk-color-on", obs_data_get_bool(tm_color, "is-bk-color-on"));
	obs_data_set_bool(tm_new_color, "is-bk-color", obs_data_get_bool(tm_color, "is-bk-color"));
	obs_data_set_bool(tm_new_color, "is-bk-init-color-on", obs_data_get_bool(tm_color, "is-bk-init-color-on"));
	obs_data_set_int(tm_new_color, "bk-color", obs_data_get_int(tm_color, "bk-color"));
	obs_data_set_bool(tm_new_color, "is-bk-color-alpha", obs_data_get_bool(tm_color, "is-bk-color-alpha"));
	obs_data_set_int(tm_new_color, "bk-color-alpha", obs_data_get_int(tm_color, "bk-color-alpha"));
	obs_data_set_string(tm_new_color, "bk-color-list", obs_data_get_string(tm_color, "bk-color-list"));

	obs_data_set_bool(tm_new_color, "is-outline-color-on", obs_data_get_bool(tm_color, "is-outline-color-on"));
	obs_data_set_bool(tm_new_color, "is-outline-init-color-on", obs_data_get_bool(tm_color, "is-outline-init-color-on"));
	obs_data_set_bool(tm_new_color, "is-outline-color", obs_data_get_bool(tm_color, "is-outline-color"));
	obs_data_set_int(tm_new_color, "outline-color", obs_data_get_int(tm_color, "outline-color"));
	obs_data_set_int(tm_new_color, "outline-color-line", obs_data_get_int(tm_color, "outline-color-line"));

	if (_isValidComparedObj(object, "index", "spinBox")) {

		switch (object->property("index").toInt()) {
		case 0:
			obs_data_set_int(tm_new_color, "text-color-alpha", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:text-color-alpha ", ACTION_CLICK);

			break;
		case 1:
			obs_data_set_int(tm_new_color, "bk-color-alpha", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:bk-color-alpha ", ACTION_CLICK);

			break;
		case 2:
			obs_data_set_int(tm_new_color, "outline-color-line", static_cast<QSpinBox *>(object)->value());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:outline-color-line ", ACTION_CLICK);

			break;
		default:
			break;
		}

	} else if (_isValidComparedObj(object, "index", "slider")) {
		switch (object->property("index").toInt()) {
		case 0:
			obs_data_set_int(tm_new_color, "text-color-alpha", static_cast<QSlider *>(object)->value());

			break;
		case 1:
			obs_data_set_int(tm_new_color, "bk-color-alpha", static_cast<QSlider *>(object)->value());
			break;
		case 2:
			obs_data_set_int(tm_new_color, "outline-color-line", static_cast<QSlider *>(object)->value());

			break;
		default:
			break;
		}

	} else if (_isValidComparedObj(object, "index", "checkBox")) {
		switch (object->property("index").toInt()) {
		case 1:
			obs_data_set_bool(tm_new_color, "is-bk-color-on", static_cast<PLSCheckBox *>(object)->isChecked());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:is-bk-color ", ACTION_CLICK);

			break;
		case 2:
			obs_data_set_bool(tm_new_color, "is-outline-color-on", static_cast<PLSCheckBox *>(object)->isChecked());
			PLS_UI_STEP(PROPERTY_MODULE, "property window:is-outline-color ", ACTION_CLICK);

			break;
		default:
			break;
		}

	} else if ("group" == objName) {
		int checkId = static_cast<QButtonGroup *>(object)->checkedId();
		if (checkId >= 0) {
			obs_data_set_int(tm_new_color, "color-tab", checkId);
		}
		PLS_UI_STEP(PROPERTY_MODULE, "property window:color-tab ", ACTION_CLICK);
	} else if ("colorListGroup" == objName) {
		auto checkBtn = static_cast<QButtonGroup *>(object)->checkedButton();
		if (!checkBtn) {
			obs_data_release(tm_color);
			obs_data_release(tm_new_color);
			return;
		}
		isNeedRefresh = true;
		auto colorInt = checkBtn->property("color").toLongLong();
		switch (object->property("index").toInt()) {
		case 0:
			obs_data_set_bool(tm_new_color, "text-color-change", true);
			obs_data_set_int(tm_new_color, "text-color", colorInt);
			PLS_UI_STEP(PROPERTY_MODULE, "property window:text-color ", ACTION_CLICK);
			break;
		case 1:
			obs_data_set_int(tm_new_color, "bk-color", colorInt);
			PLS_UI_STEP(PROPERTY_MODULE, "property window:bk-color ", ACTION_CLICK);
			break;
		case 2:
			obs_data_set_int(tm_new_color, "outline-color", colorInt);
			PLS_UI_STEP(PROPERTY_MODULE, "property window:outline-color ", ACTION_CLICK);
			break;
		default:
			break;
		}
	} else {
		QColorDialog::ColorDialogOptions options;
		auto label = static_cast<QLabel *>(widget);
		const char *desc = obs_property_description(property);
		QColor color = PLSColorDialogView::getColor(label->text(), view->parentWidget(), QT_UTF8(desc), options);
		color.setAlpha(255);
		if (!color.isValid()) {
			obs_data_release(tm_color);
			obs_data_release(tm_new_color);
			return;
		}

		label->setText(color.name(QColor::HexRgb));
		auto palette = QPalette(color);
		label->setPalette(palette);
		label->setStyleSheet(QString("font-weight: normal;background-color :%1; color: %2;")
					     .arg(palette.color(QPalette::Window).name(QColor::HexRgb))
					     .arg(palette.color(QPalette::WindowText).name(QColor::HexRgb)));

		obs_data_set_int(view->settings, setting, color_to_int(color));
		switch (label->property("index").toInt()) {
		case 0:
			obs_data_set_bool(tm_new_color, "text-color-change", true);
			obs_data_set_int(tm_new_color, "text-color", color_to_int(color));
			PLS_UI_STEP(PROPERTY_MODULE, "property window:text-color ", ACTION_CLICK);

			break;
		case 1:
			obs_data_set_int(tm_new_color, "bk-color", color_to_int(color));
			PLS_UI_STEP(PROPERTY_MODULE, "property window:bk-color ", ACTION_CLICK);

			break;
		case 2:
			obs_data_set_int(tm_new_color, "outline-color", color_to_int(color));
			PLS_UI_STEP(PROPERTY_MODULE, "property window:outline-color ", ACTION_CLICK);

			break;
		default:
			break;
		}
	}
	obs_data_set_obj(view->settings, setting, tm_new_color);
	obs_data_release(tm_color);
	obs_data_release(tm_new_color);
	if (isNeedRefresh) {
		QMetaObject::invokeMethod(view, &OBSPropertiesView::RefreshProperties, Qt::QueuedConnection);
	}
}

void PLSWidgetInfo::TMTextMotionChanged(const char *setting)
{
	obs_data_t *tm_motion = obs_data_get_obj(view->settings, setting);
	QString objName = object->objectName();

	obs_data_t *tm_new_motion = obs_data_create();
	obs_data_set_bool(tm_new_motion, "is-motion-settings", obs_data_get_bool(tm_motion, "is-motion-settings"));
	obs_data_set_int(tm_new_motion, "text-motion", obs_data_get_int(tm_motion, "text-motion"));
	obs_data_set_bool(tm_new_motion, "is-text-motion-speed", obs_data_get_bool(tm_motion, "is-text-motion-speed"));
	obs_data_set_int(tm_new_motion, "text-motion-speed", obs_data_get_int(tm_motion, "text-motion-speed"));

	if ("spinBox" == objName) {

		obs_data_set_int(tm_new_motion, "text-motion-speed", static_cast<QSpinBox *>(object)->value());

	} else if ("slider" == objName) {
		obs_data_set_int(tm_new_motion, "text-motion-speed", static_cast<QSlider *>(object)->value());
		PLS_UI_STEP(PROPERTY_MODULE, "property window:text-motion-speed ", ACTION_CLICK);

	} else if ("group2" == objName) {
		int checkId = static_cast<PLSRadioButtonGroup *>(object)->checkedId();
		if (checkId >= 0) {
			obs_data_set_int(tm_new_motion, "text-motion", qAbs(checkId - 1));
		}
		PLS_UI_STEP(PROPERTY_MODULE, "property window:text-motion ", ACTION_CLICK);
	}
	obs_data_set_obj(view->settings, setting, tm_new_motion);
	obs_data_release(tm_motion);
	obs_data_release(tm_new_motion);
}

void PLSWidgetInfo::EditableListChanged()
{
	const char *setting = obs_property_name(property);
	auto list = static_cast<QListWidget *>(widget);
	obs_data_array *array = obs_data_array_create();

	for (int i = 0; i < list->count(); i++) {
		const QListWidgetItem *item = list->item(i);
		obs_data_t *arrayItem = obs_data_create();
		obs_data_set_string(arrayItem, "value", QT_TO_UTF8(item->text()));
		obs_data_set_bool(arrayItem, "selected", item->isSelected());
		obs_data_set_bool(arrayItem, "hidden", item->isHidden());
		obs_data_array_push_back(array, arrayItem);
		obs_data_release(arrayItem);
	}

	obs_data_set_array(view->settings, setting, array);
	obs_data_array_release(array);

	ControlChanged();
}

void PLSWidgetInfo::SelectRegionClicked(const char *setting)
{
	uint64_t max_size = pls_texture_get_max_size();
	PLSRegionCapture *regionCapture = pls_new<PLSRegionCapture>(view);
	connect(regionCapture, &PLSRegionCapture::selectedRegion, this, [this, regionCapture, setting](const QRect &selectedRect) {
		qInfo() << "user selected a new region=" << selectedRect;
		if (!selectedRect.isValid()) {
			regionCapture->deleteLater();
			return;
		}

		obs_data_t *region_obj = obs_data_create();
		obs_data_set_int(region_obj, "left", selectedRect.left());
		obs_data_set_int(region_obj, "top", selectedRect.top());
		obs_data_set_int(region_obj, "width", selectedRect.width());
		obs_data_set_int(region_obj, "height", selectedRect.height());
		obs_data_set_obj(view->settings, setting, region_obj);
		obs_data_release(region_obj);
		auto plsView = dynamic_cast<PLSPropertiesView *>(view);
		assert(plsView);
		plsView->refreshViewAfterUIChanged(property);
		regionCapture->deleteLater();
	});
	regionCapture->StartCapture(max_size, max_size);
}

void PLSWidgetInfo::ImageGroupChanged(const char *setting)
{
	auto buttons = static_cast<QButtonGroup *>(object);
	obs_data_set_int(view->settings, setting, buttons->checkedId());
	for (auto subButton : buttons->buttons()) {
		pls_flush_style_recursive(subButton, "checked", subButton == buttons->checkedButton());
	}

	if (pls_property_image_group_clicked(property, view->GetSourceObj(), buttons->checkedId())) {
		QMetaObject::invokeMethod(view, "RefreshProperties", Qt::QueuedConnection);
	}
}

void PLSWidgetInfo::intCustomGroupChanged(const char *setting)
{
	auto spin = static_cast<QSpinBox *>(widget);
	obs_data_set_int(view->settings, setting, spin->value());
}

void PLSWidgetInfo::CustomGroupChanged(const char *)
{
	auto type = (pls_custom_group_type)widget->property("child_type").toInt();
	QString childName = widget->property("child_name").toString();
	switch (type) {
	case PLS_CUSTOM_GROUP_UNKNOWN:
		break;
	case PLS_CUSTOM_GROUP_INT:
		intCustomGroupChanged(QT_TO_UTF8(childName));
		break;
	default:
		break;
	}
}

void PLSWidgetInfo::TextButtonClicked()
{
	if (pls_property_mobile_name_button_clicked(property, view->GetSourceObj())) {
		QMetaObject::invokeMethod(view, "RefreshProperties", Qt::QueuedConnection);
	}
}

void PLSWidgetInfo::FontSimpleChanged(const char *setting)
{
	obs_data_t *new_font_obj = obs_data_create();
	obs_data_t *font_obj = obs_data_get_obj(view->settings, setting);
	obs_data_apply(new_font_obj, font_obj);
	obs_data_release(font_obj);

	if (!font_obj) {
		PLS_WARN(PROPERTY_MODULE, "property window:font checked, the font_obj is nil");
	}

	auto plsView = dynamic_cast<PLSPropertiesView *>(view);
	if (!plsView) {
		return;
	}
	QString objName = object->objectName();
	if ("FontCheckedFamilyBox" == objName) {
		QString currentFamily(static_cast<PLSComboBox *>(object)->currentText());
		QString weight = QFontDatabase::styles(currentFamily).first();
		obs_data_set_string(new_font_obj, "font-family", currentFamily.toUtf8());
		obs_data_set_string(new_font_obj, "font-weight", weight.toUtf8());
		PLS_UI_STEP(PROPERTY_MODULE, "property window:family", ACTION_CLICK);
	} else if ("FontCheckedWidgetBox" == objName) {
		QString weight(static_cast<PLSComboBox *>(object)->currentText());
		obs_data_set_string(new_font_obj, "font-weight", weight.toUtf8());
		PLS_UI_STEP(PROPERTY_MODULE, "property window:font-weight ", ACTION_CLICK);
	}

	obs_data_set_obj(view->settings, setting, new_font_obj);
	obs_data_release(new_font_obj);
}

void PLSWidgetInfo::ColorCheckBoxChanged(const char *setting)
{
	auto plsView = dynamic_cast<PLSPropertiesView *>(view);
	if (!plsView) {
		return;
	}
	if (common::OBJECT_NAME_FORMCHECKBOX == object->objectName()) {
		auto checkBox = dynamic_cast<PLSCheckBox *>(object);
		if (checkBox) {
			obs_data_t *color_obj = obs_data_get_obj(view->settings, setting);
			obs_data_set_bool(color_obj, "is_enable", checkBox->isChecked());
			obs_data_set_obj(view->settings, setting, color_obj);
			obs_data_release(color_obj);
			plsView->refreshViewAfterUIChanged(property);
		}
	} else {
		ColorChanged(setting);
	}
}

void PLSWidgetInfo::templateListChanged(const char *setting)
{
	if (auto group = dynamic_cast<pls::ITemplateListPropertyModel::IButtonGroup *>(widget); group && group->selectedButton()) {
		obs_data_set_int(view->settings, setting, group->selectedButton()->value());
	}
}

QWidget *PLSPropertiesView::AddTextContent(obs_property_t *prop)
{
	auto content = pls_property_get_text_content(prop);

	auto pLineEdit = pls_new<PLSLineEdit>(this);
	pLineEdit->setText(content);
	pLineEdit->setObjectName(common::OBJECT_NAME_LINEEDIT);
	pLineEdit->setEnabled(false);

	return pLineEdit;
}
