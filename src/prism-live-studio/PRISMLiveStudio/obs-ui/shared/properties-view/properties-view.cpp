#include <QFormLayout>
#include <QScrollBar>
#include <QLabel>
#include <QCheckBox>
#include <QFont>
#include <QLineEdit>
#include <QSpinBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
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
#include <QUuid>
#include "double-slider.hpp"
#include "spinbox-ignorewheel.hpp"
#include "moc_properties-view.cpp"
#include "properties-view.moc.hpp"
#include <plain-text-edit.hpp>
#include <slider-ignorewheel.hpp>
#include <icon-label.hpp>
#include "pls-common-define.hpp"
#include "PLSColorDialogView.h"
#include "PLSFontDialogView.h"

#include <cstdlib>
#include <initializer_list>
#include <obs-data.h>
#include <obs.h>
#include <qtimer.h>
#include <string>
#include <obs-frontend-api.h>

#include "pls/pls-properties.h"
#include "PLSComboBox.h"
#include "PLSSpinBox.h"
#include "PLSAction.h"
#include "PLSCommonScrollBar.h"
#include "PLSCheckBox.h"
#include "pls-common-define.hpp"
#include "liblog.h"
#include "PLSEdit.h"
#include "PLSLabel.h"
#include "PLSRadioButton.h"
#include "libutils-api.h"
#include "obs-app.hpp"

using namespace std;
using namespace common;

#if defined(_WIN32)
constexpr const char *CSTR_VIDEO_DEVICE_ID = "video_device_id";
#elif defined(__APPLE__)
constexpr const char *CSTR_VIDEO_DEVICE_ID = "device_name";
#endif

constexpr const char *CSTR_PRISM_LEN1 = "PRISM Lens 1";
constexpr const char *CSTR_PRISM_LEN2 = "PRISM Lens 2";
constexpr const char *CSTR_PRISM_LEN3 = "PRISM Lens 3";

QColor color_from_int(long long val)
{
	return QColor(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff, (val >> 24) & 0xff);
}

long long color_to_int(QColor color)
{
	auto shift = [&](unsigned val, int shift) {
		return ((val & 0xff) << shift);
	};

	return shift(color.red(), 0) | shift(color.green(), 8) | shift(color.blue(), 16) | shift(color.alpha(), 24);
}

QWidget *plsCreateHelpQWidget(QWidget *originWidget, const QString &longDesc, const char *name, const QVariant &value)
{
	QWidget *newWidget = new QWidget();
	newWidget->setObjectName("formLeftWidget");
	QHBoxLayout *boxLayout = new QHBoxLayout(newWidget);
	boxLayout->setContentsMargins(0, 0, 0, 0);
	boxLayout->setAlignment(Qt::AlignLeft);
	boxLayout->setSpacing(0);
	PLSHelpIcon *help = new PLSHelpIcon(newWidget);
	help->setToolTip(longDesc);
	if (name) {
		help->setProperty(name, value);
	}
	boxLayout->addWidget(originWidget);
	boxLayout->addWidget(help);
	return newWidget;
}

namespace {

struct frame_rate_tag {
	enum tag_type {
		SIMPLE,
		RATIONAL,
		USER,
	} type = SIMPLE;
	const char *val = nullptr;

	frame_rate_tag() = default;

	explicit frame_rate_tag(tag_type type) : type(type) {}

	explicit frame_rate_tag(const char *val) : type(USER), val(val) {}

	static frame_rate_tag simple() { return frame_rate_tag{SIMPLE}; }
	static frame_rate_tag rational() { return frame_rate_tag{RATIONAL}; }
};

struct common_frame_rate {
	const char *fps_name;
	media_frames_per_second fps;
};

} // namespace

Q_DECLARE_METATYPE(frame_rate_tag);
Q_DECLARE_METATYPE(media_frames_per_second);

void OBSPropertiesView::ReloadProperties()
{
	if (weakObj || rawObj) {
		OBSObject strongObj = GetObject();
		void *obj = strongObj ? strongObj.Get() : rawObj;
		if (obj) {
			properties.reset(reloadCallback(obj));
		}
	} else {
		properties.reset(reloadCallback((void *)type.c_str()));
		obs_properties_apply_settings(properties.get(), settings);
	}

	uint32_t flags = obs_properties_get_flags(properties.get());
	deferUpdate = enableDefer && (flags & OBS_PROPERTIES_DEFER_UPDATE) != 0;

	RefreshProperties();
}

void OBSPropertiesView::RefreshProperties()
{
	PLS_INFO("main/property", "OBSPropertiesView RefreshProperties() method start");
	int h, v, hend, vend;
	GetScrollPos(h, v, hend, vend);

	children.clear();
	auto source = pls_get_source_by_pointer_address(GetSourceObj());
	const auto id = obs_source_get_id(source);
	if (pls_is_equal(id, PRISM_TEXT_TEMPLATE_ID)) {
		pls_get_text_motion_template_helper_instance()->removeParent();
	}
	if (pls_is_equal(id, PRISM_CHATV2_SOURCE_ID)) {
		pls_get_chat_template_helper_instance()->removeParent();
	}
	if (widget) {
		widget->setParent(nullptr);
		pls_delete(widget, nullptr);
	}

	widget = new QWidget();
	widget->setFocus();
	widget->setObjectName("PropertiesContainer");
	setContentMarginAndWidth();

	QFormLayout *formLayout = nullptr;
	bool template_source = pls_is_equal(id, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID);

	obs_property_t *property = obs_properties_first(properties.get());
	bool hasNoProperties = !property;

	if (pls_is_or(hasNoProperties, showFiltersBtn, !template_source)) {
		boxLayout = formLayout = pls_new<QFormLayout>();
		formLayout->setContentsMargins(0, 0, 0, 0);
		formLayout->setHorizontalSpacing(
			pls_conditional_select(pls_is_equal(id, PRISM_TIMER_SOURCE_ID), 10, 20));
		formLayout->setVerticalSpacing(0);
		formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	} else { // virtual background
		boxLayout = pls_new<QVBoxLayout>();
		boxLayout->setContentsMargins(9, 0, 5, 0);
		boxLayout->setSpacing(0);
	}

	widget->setLayout(boxLayout);
	QSizePolicy policy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	widget->setSizePolicy(policy);
	lastPropertyType = OBS_PROPERTY_INVALID;

	while (property) {
		AddProperty(property, formLayout);
		obs_property_next(&property);
	}

	if (hasNoProperties && formLayout) {
		QLabel *noPropertiesLabel = new QLabel(NO_PROPERTIES_STRING);
		formLayout->addWidget(noPropertiesLabel);
	}

	if (showFiltersBtn) {
		showFilterButton(hasNoProperties, id);
	}

	if (formLayout) {
		QSpacerItem *spaceItem = pls_new<QSpacerItem>(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding);
		formLayout->addItem(spaceItem);
	}

	setWidgetResizable(true);
	setWidget(widget);
	SetScrollPos(h, v, hend, vend);
	QSizePolicy mainPolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setSizePolicy(mainPolicy);

	if (disableScrolling)
		setMinimumHeight(qMax(widget->minimumSizeHint().height(), widget->sizeHint().height()));

	lastFocused.clear();
	if (lastWidget) {
		lastWidget->setFocus(Qt::OtherFocusReason);
		lastWidget = nullptr;
	}
	PLS_INFO("main/property", "OBSPropertiesView RefreshProperties() method end");
}
void OBSPropertiesView::SetScrollPos(int h, int v, int old_hend, int old_vend)
{
	scroll = qobject_cast<PLSCommonScrollBar *>(horizontalScrollBar());
	if (scroll)
		scroll->setValue(h);

	scroll = qobject_cast<PLSCommonScrollBar *>(verticalScrollBar());
	if (scroll)
		scroll->setValue(v);
}

void OBSPropertiesView::GetScrollPos(int &h, int &v, int &hend, int &vend)
{
	h = v = 0;
	scroll = qobject_cast<PLSCommonScrollBar *>(horizontalScrollBar());
	if (scroll)
		h = scroll->value();

	scroll = qobject_cast<PLSCommonScrollBar *>(verticalScrollBar());
	if (scroll)
		v = scroll->value();

	connect(scroll, &PLSCommonScrollBar::isShowScrollBar, this, &OBSPropertiesView::OnShowScrollBar,
		Qt::UniqueConnection);
}

OBSPropertiesView::OBSPropertiesView(OBSData settings_, obs_object_t *obj, PropertiesReloadCallback reloadCallback,
				     PropertiesUpdateCallback callback_, PropertiesVisualUpdateCb visUpdateCb_,
				     int minSize_)
	: VScrollArea(nullptr),
	  properties(nullptr, obs_properties_destroy),
	  settings(settings_),
	  weakObj(obs_object_get_weak_object(obj)),
	  reloadCallback(reloadCallback),
	  callback(callback_),
	  visUpdateCb(visUpdateCb_),
	  minSize(minSize_)
{
	setFrameShape(QFrame::NoFrame);
	QMetaObject::invokeMethod(this, "ReloadProperties", Qt::QueuedConnection);
}

OBSPropertiesView::OBSPropertiesView(OBSData settings_, void *obj, PropertiesReloadCallback reloadCallback,
				     PropertiesUpdateCallback callback_, PropertiesVisualUpdateCb visUpdateCb_,
				     int minSize_)
	: VScrollArea(nullptr),
	  properties(nullptr, obs_properties_destroy),
	  settings(settings_),
	  rawObj(obj),
	  reloadCallback(reloadCallback),
	  callback(callback_),
	  visUpdateCb(visUpdateCb_),
	  minSize(minSize_)
{
	setFrameShape(QFrame::NoFrame);
	QMetaObject::invokeMethod(this, "ReloadProperties", Qt::QueuedConnection);
}

void OBSPropertiesView::SetDisabled(bool disabled)
{
	for (auto child : findChildren<QWidget *>()) {
		child->setDisabled(disabled);
	}
}

OBSPropertiesView::OBSPropertiesView(OBSData settings_, const char *type_, PropertiesReloadCallback reloadCallback_,
				     int minSize_, bool bFromSetting)
	: VScrollArea(nullptr),
	  properties(nullptr, obs_properties_destroy),
	  settings(settings_),
	  type(type_),
	  reloadCallback(reloadCallback_),
	  minSize(minSize_),
	  m_bFromSetting(bFromSetting)
{
	setFrameShape(QFrame::NoFrame);
	QMetaObject::invokeMethod(this, "ReloadProperties", Qt::QueuedConnection);
}

void OBSPropertiesView::resizeEvent(QResizeEvent *event)
{
	emit PropertiesResized();
	VScrollArea::resizeEvent(event);
}

QWidget *OBSPropertiesView::AddCheckbox(QFormLayout *parentLayout, obs_property_t *prop)
{
	const char *name = obs_property_name(prop);
	const char *desc = obs_property_description(prop);
	const char *long_desc = obs_property_long_description(prop);
	bool val = obs_data_get_bool(settings, name);

	PLSCheckBox *checkbox = new PLSCheckBox(QT_UTF8(desc));
	checkbox->setObjectName(OBJECT_NAME_CHECKBOX);
	checkbox->setChecked(val);

	OBSSource source = pls_get_source_by_pointer_address(GetSourceObj());
	if (source) {
		const char *id = obs_source_get_id(source);
		if (pls_is_equal(id, PRISM_SPECTRALIZER_SOURCE_ID)) {
			checkbox->setObjectName("tmLabel");
		} else if (pls_is_equal(name, "use_custom_audio_device") &&
			   (pls_is_equal(id, PRISM_LENS_SOURCE_ID) || pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID))) {
			QString program;
			checkbox->setEnabled(pls_is_install_cam_studio(program));
		}

#if defined(Q_OS_MACOS)
		if (pls_is_equal(id, OBS_DSHOW_SOURCE_ID_V2) || pls_is_equal(id, OBS_DSHOW_SOURCE_ID)) {
			parentLayout->addItem(new QSpacerItem(22, 14, QSizePolicy::Fixed, QSizePolicy::Fixed));
		}
#endif
	}

	QWidget *widget = NewWidget(prop, checkbox, &PLSCheckBox::stateChanged);

	if (!long_desc) {
		return widget;
	}

	checkbox->setToolTip(long_desc);
	widget = plsCreateHelpQWidget(checkbox, long_desc);

	return widget;
}

QWidget *OBSPropertiesView::AddText(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	const char *val = obs_data_get_string(settings, name);
	bool monospace = obs_property_text_monospace(prop);
	obs_text_type type = obs_property_text_type(prop);

	if (type == OBS_TEXT_MULTILINE) {
		OBSPlainTextEdit *edit = new OBSPlainTextEdit(this, monospace);
		edit->setPlainText(QT_UTF8(val));
		edit->setTabStopDistance(40);
		return NewWidget(prop, edit, &OBSPlainTextEdit::textChanged);

	} else if (type == OBS_TEXT_PASSWORD) {
		QLayout *subLayout = new QHBoxLayout();
		PLSLineEdit *edit = new PLSLineEdit();
		QPushButton *show = new QPushButton();

		show->setText(tr("Show"));
		show->setCheckable(true);
		edit->setText(QT_UTF8(val));
		edit->setEchoMode(QLineEdit::Password);

		subLayout->addWidget(edit);
		subLayout->addWidget(show);

		WidgetInfo *info = new WidgetInfo(this, prop, edit);
		pls_connect(show, &QAbstractButton::toggled, info, &WidgetInfo::TogglePasswordText);
		pls_connect(show, &QAbstractButton::toggled,
			    [=](bool hide) { show->setText(hide ? tr("Hide") : tr("Show")); });
		children.emplace_back(info);

		label = new QLabel(QT_UTF8(obs_property_description(prop)));
		layout->addRow(label, subLayout);
		AddSpacer(obs_property_get_type(prop), layout);

		edit->setToolTip(QT_UTF8(obs_property_long_description(prop)));

		pls_connect(edit, &QLineEdit::textEdited, info, &WidgetInfo::ControlChanged);
		return nullptr;
	} else if (type == OBS_TEXT_INFO) {
		QString desc = QT_UTF8(obs_property_description(prop));
		const char *long_desc = obs_property_long_description(prop);
		obs_text_info_type info_type = obs_property_text_info_type(prop);

		QLabel *info_label = new QLabel(QT_UTF8(val));

		if (info_label->text().isEmpty() && long_desc == NULL) {
			label = nullptr;
			info_label->setText(desc);
		} else
			label = new QLabel(desc);

		QWidget *replaceWidget = dynamic_cast<QWidget *>(info_label);
		if (long_desc != NULL && !info_label->text().isEmpty()) {
#if 0
			QString file = !App()->IsThemeDark()
					       ? ":/res/images/help.svg"
					       : ":/res/images/help_light.svg";
			QString lStr = "<html>%1 <img src='%2' style=' \
				vertical-align: bottom; ' /></html>";

			info_label->setText(lStr.arg(info_label->text(), file));
#endif
			info_label->setToolTip(QT_UTF8(long_desc));

			replaceWidget = plsCreateHelpQWidget(info_label, QT_UTF8(long_desc));

		} else if (long_desc != NULL) {
			info_label->setText(QT_UTF8(long_desc));
		}

		info_label->setOpenExternalLinks(true);
		info_label->setWordWrap(obs_property_text_info_word_wrap(prop));

		if (info_type == OBS_TEXT_INFO_WARNING) {
			info_label->setObjectName("warningLabel");
			info_label->setProperty("class", "text-warning");
		} else if (info_type == OBS_TEXT_INFO_ERROR) {
			info_label->setObjectName("errorLabel");
			info_label->setProperty("class", "text-danger");
		} else
			info_label->setObjectName("infoLabel");

		if (label)
			label->setObjectName(info_label->objectName());

		WidgetInfo *info = new WidgetInfo(this, prop, info_label);
		children.emplace_back(info);

		bool isFirstItem = lastPropertyType == OBS_PROPERTY_INVALID;
		if (isFirstItem) {
			layout->addItem(new QSpacerItem(22, 22, QSizePolicy::Fixed, QSizePolicy::Fixed));
		} else {
			AddSpacer(obs_property_get_type(prop), layout);
		}
		layout->addRow(label, replaceWidget);
		if (isFirstItem) {
			layout->addItem(new QSpacerItem(22, 22, QSizePolicy::Fixed, QSizePolicy::Fixed));
		}

		return nullptr;
	}

	PLSLineEdit *edit = new PLSLineEdit();

	edit->setText(QT_UTF8(val));
	edit->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	edit->setObjectName("lineedit");

	const char *placeholder = pls_property_get_placeholder(prop);
	if (placeholder && placeholder[0]) {
		edit->setPlaceholderText(QT_UTF8(placeholder));
	}

	return NewWidget(prop, edit, &QLineEdit::textEdited);
}

void OBSPropertiesView::AddPath(obs_property_t *prop, QFormLayout *layout, QLabel **label)
{
	const char *name = obs_property_name(prop);
	const char *val = obs_data_get_string(settings, name);
	QLayout *subLayout = new QHBoxLayout();
	PLSLineEdit *edit = new PLSLineEdit();
	edit->setObjectName(common::OBJECT_NAME_LINEEDIT);

	QPushButton *button = new QPushButton(tr("Browse"));

	if (!obs_property_enabled(prop)) {
		edit->setEnabled(false);
		button->setEnabled(false);
	}

	edit->setText(QT_UTF8(val));
	edit->setReadOnly(true);
	edit->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	subLayout->addWidget(edit);
	subLayout->addWidget(button);

	WidgetInfo *info = new WidgetInfo(this, prop, edit);
	pls_connect(button, &QPushButton::clicked, info, &WidgetInfo::ControlChanged);
	children.emplace_back(info);

	AddSpacer(obs_property_get_type(prop), layout);
	*label = new QLabel(QT_UTF8(obs_property_description(prop)));
	layout->addRow(*label, subLayout);
}

void OBSPropertiesView::AddInt(obs_property_t *prop, QFormLayout *layout, QLabel **label)
{
	obs_number_type type = obs_property_int_type(prop);
	QHBoxLayout *subLayout = new QHBoxLayout();

	const char *name = obs_property_name(prop);
	int val = (int)obs_data_get_int(settings, name);

	PLSSpinBox *spin = new PLSSpinBox(this);
	spin->makeTextVCenter();
	spin->setObjectName(OBJECT_NAME_SPINBOX);
	spin->setEnabled(obs_property_enabled(prop));

	int minVal = obs_property_int_min(prop);
	int maxVal = obs_property_int_max(prop);
	int stepVal = obs_property_int_step(prop);
	const char *suffix = obs_property_int_suffix(prop);

	spin->setMinimum(minVal);
	spin->setMaximum(maxVal);
	spin->setSingleStep(stepVal);
	spin->setValue(val);
	spin->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	spin->setSuffix(QT_UTF8(suffix));

	WidgetInfo *info = new WidgetInfo(this, prop, spin);
	children.emplace_back(info);

	if (type == OBS_NUMBER_SLIDER) {
		QSlider *slider = new SliderIgnoreScroll();
		slider->setMinimum(minVal);
		slider->setMaximum(maxVal);
		slider->setPageStep(stepVal);
		slider->setValue(val);
		slider->setOrientation(Qt::Horizontal);
		slider->setEnabled(obs_property_enabled(prop));
		subLayout->addWidget(slider);
		subLayout->addSpacing(20);

		pls_connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
		pls_connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);
	}

	pls_connect(spin, &QSpinBox::valueChanged, info, &WidgetInfo::ControlChanged);

	subLayout->addWidget(spin);

	*label = new QLabel(QT_UTF8(obs_property_description(prop)));
	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(*label, subLayout);
}

void OBSPropertiesView::AddFloat(obs_property_t *prop, QFormLayout *layout, QLabel **label)
{
	obs_number_type type = obs_property_float_type(prop);
	auto *subLayout = new QHBoxLayout();

	const char *name = obs_property_name(prop);
	double val = obs_data_get_double(settings, name);
	QDoubleSpinBox *spin = new PLSDoubleSpinBox();
	spin->setObjectName(OBJECT_NAME_SPINBOX);

	if (!obs_property_enabled(prop))
		spin->setEnabled(false);

	double minVal = obs_property_float_min(prop);
	double maxVal = obs_property_float_max(prop);
	double stepVal = obs_property_float_step(prop);
	const char *suffix = obs_property_float_suffix(prop);

	if (stepVal < 1.0) {
		constexpr int sane_limit = 8;
		const int decimals = std::min<int>(log10(1.0 / stepVal) + 0.99, sane_limit);
		if (decimals > spin->decimals())
			spin->setDecimals(decimals);
	}

	spin->setMinimum(minVal);
	spin->setMaximum(maxVal);
	spin->setSingleStep(stepVal);
	spin->setValue(val);
	spin->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	spin->setSuffix(QT_UTF8(suffix));

	WidgetInfo *info = new WidgetInfo(this, prop, spin);
	children.emplace_back(info);

	if (type == OBS_NUMBER_SLIDER) {
		DoubleSlider *slider = new DoubleSlider();
		slider->setDoubleConstraints(minVal, maxVal, stepVal, val);
		slider->setOrientation(Qt::Horizontal);
		subLayout->addWidget(slider);
		subLayout->addSpacing(20);

		pls_connect(slider, &DoubleSlider::doubleValChanged, spin, &QDoubleSpinBox::setValue);
		pls_connect(spin, &QDoubleSpinBox::valueChanged, slider, &DoubleSlider::setDoubleVal);
	}

	pls_connect(spin, &QDoubleSpinBox::valueChanged, info, &WidgetInfo::ControlChanged);

	subLayout->addWidget(spin);

	*label = new QLabel(QT_UTF8(obs_property_description(prop)));
	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(*label, subLayout);
}

static QVariant propertyListToQVariant(obs_property_t *prop, size_t idx)
{
	obs_combo_format format = obs_property_list_format(prop);

	QVariant var;
	if (format == OBS_COMBO_FORMAT_INT) {
		long long val = obs_property_list_item_int(prop, idx);
		var = QVariant::fromValue<long long>(val);
	} else if (format == OBS_COMBO_FORMAT_FLOAT) {
		double val = obs_property_list_item_float(prop, idx);
		var = QVariant::fromValue<double>(val);
	} else if (format == OBS_COMBO_FORMAT_STRING) {
		var = QByteArray(obs_property_list_item_string(prop, idx));
	} else if (format == OBS_COMBO_FORMAT_BOOL) {
		bool val = obs_property_list_item_bool(prop, idx);
		var = QVariant::fromValue<bool>(val);
	}
	return var;
}

static void AddComboItem(QComboBox *combo, obs_property_t *prop, size_t idx)
{
	const char *name = obs_property_list_item_name(prop, idx);
	QVariant var = propertyListToQVariant(prop, idx);

	combo->addItem(QT_UTF8(name), var);

	if (!obs_property_list_item_disabled(prop, idx))
		return;

	int index = combo->findText(QT_UTF8(name));
	if (index < 0)
		return;

	QStandardItemModel *model = dynamic_cast<QStandardItemModel *>(combo->model());
	if (!model)
		return;

	QStandardItem *item = model->item(index);
	item->setFlags(Qt::NoItemFlags);
}

static void AddRadioItem(PLSRadioButtonGroup *buttonGroup, QFormLayout *layout, obs_property_t *prop, QVariant value,
			 size_t idx)
{
	const char *name = obs_property_list_item_name(prop, idx);

	QVariant var = propertyListToQVariant(prop, idx);
	PLSRadioButton *button = new PLSRadioButton(name);
	button->setChecked(value == var);
	button->setProperty("value", var);
	buttonGroup->addButton(button);
	layout->addRow(button);
}

template<long long get_int(obs_data_t *, const char *), double get_double(obs_data_t *, const char *),
	 const char *get_string(obs_data_t *, const char *), bool get_bool(obs_data_t *, const char *)>
static QVariant from_obs_data(obs_data_t *data, const char *name, obs_combo_format format)
{
	switch (format) {
	case OBS_COMBO_FORMAT_INT:
		return QVariant::fromValue(get_int(data, name));
	case OBS_COMBO_FORMAT_FLOAT:
		return QVariant::fromValue(get_double(data, name));
	case OBS_COMBO_FORMAT_STRING:
		return QByteArray(get_string(data, name));
	case OBS_COMBO_FORMAT_BOOL:
		return QVariant::fromValue(get_bool(data, name));
	default:
		return QVariant();
	}
}

static QVariant from_obs_data(obs_data_t *data, const char *name, obs_combo_format format)
{
	return from_obs_data<obs_data_get_int, obs_data_get_double, obs_data_get_string, obs_data_get_bool>(data, name,
													    format);
}

static QVariant from_obs_data_autoselect(obs_data_t *data, const char *name, obs_combo_format format)
{
	return from_obs_data<obs_data_get_autoselect_int, obs_data_get_autoselect_double,
			     obs_data_get_autoselect_string, obs_data_get_autoselect_bool>(data, name, format);
}

QWidget *OBSPropertiesView::AddList(obs_property_t *prop, bool &warning)
{
	const char *name = obs_property_name(prop);
	obs_combo_type type = obs_property_list_type(prop);
	obs_combo_format format = obs_property_list_format(prop);
	size_t count = obs_property_list_item_count(prop);

	QVariant value = from_obs_data(settings, name, format);

	if (type == OBS_COMBO_TYPE_RADIO) {
		PLSRadioButtonGroup *buttonGroup = new PLSRadioButtonGroup();
		QFormLayout *subLayout = new QFormLayout();
		subLayout->setContentsMargins(0, 0, 0, 0);

		for (size_t idx = 0; idx < count; idx++)
			AddRadioItem(buttonGroup, subLayout, prop, value, idx);

		if (count > 0) {
			WidgetInfo *info = new WidgetInfo(this, prop, buttonGroup->buttons()[0]);
			children.emplace_back(info);
			pls_connect(buttonGroup, &PLSRadioButtonGroup::buttonClicked, info,
				    &WidgetInfo::ControlChanged);
		}

		QWidget *widget = new QWidget();
		widget->setLayout(subLayout);
		return widget;
	}
	int idx = -1;

	QComboBox *combo = new PLSComboBox();
	combo->view()->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	for (size_t i = 0; i < count; i++)
		AddComboItem(combo, prop, i);

	if (type == OBS_COMBO_TYPE_EDITABLE)
		combo->setEditable(true);

	combo->setMaxVisibleItems(40);
	combo->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	if (format == OBS_COMBO_FORMAT_STRING && type == OBS_COMBO_TYPE_EDITABLE) {
		combo->lineEdit()->setText(value.toString());
	} else {
		idx = combo->findData(value);
	}

	if (type == OBS_COMBO_TYPE_EDITABLE)
		return NewWidget(prop, combo, &QComboBox::editTextChanged);

	if (idx != -1)
		combo->setCurrentIndex(idx);

	if (obs_data_has_autoselect_value(settings, name)) {
		QVariant autoselect = from_obs_data_autoselect(settings, name, format);
		int id = combo->findData(autoselect);

		if (id != -1 && id != idx) {
			QString actual = combo->itemText(id);
			QString selected = combo->itemText(idx);
			QString combined = tr("Basic.PropertiesWindow.AutoSelectFormat");
			combo->setItemText(idx, combined.arg(selected).arg(actual));
		}
	}

	QAbstractItemModel *model = combo->model();
	warning = idx != -1 && model->flags(model->index(idx, 0)) == Qt::NoItemFlags;

	WidgetInfo *info = new WidgetInfo(this, prop, combo);
	pls_connect(combo, &QComboBox::currentIndexChanged, info, &WidgetInfo::ControlChanged);
	children.emplace_back(info);

	/* trigger a settings update if the index was not found */
	QString program;
	bool isLens = isPrismLensOrMobileSource();
	bool isLensInstall = pls_is_install_cam_studio(program);

#ifdef Q_OS_WINDOWS
	bool checkPrismLensUnInstalled = isLens && !isLensInstall;
	if (checkPrismLensUnInstalled) {
		combo->clear();
		combo->setEnabled(false);
		warning = false;
	}
	if (count && idx == -1)
		if (!checkPrismLensUnInstalled)
			info->ControlChanged();
#else
	if (count && idx == -1) {
		if (isLens && pls_is_equal("device", name) && !isLensInstall) {
			return combo;
		}

		info->ControlChanged();
	}
#endif // Q_OS_WINDOWS

	return combo;
}

static void NewButton(QLayout *layout, WidgetInfo *info, const char *themeIcon, void (WidgetInfo::*method)())
{
	QPushButton *button = new QPushButton();
	button->setProperty("themeID", themeIcon);
	button->setFlat(true);
	button->setProperty("toolButton", true);

	pls_connect(button, &QPushButton::clicked, info, method);

	layout->addWidget(button);
}

void OBSPropertiesView::AddEditableList(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	OBSDataArrayAutoRelease array = obs_data_get_array(settings, name);
	QListWidget *list = new QListWidget();
	size_t count = obs_data_array_count(array);

	if (!obs_property_enabled(prop))
		list->setEnabled(false);

	list->setSortingEnabled(false);
	list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	list->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	list->setSpacing(1);

	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease item = obs_data_array_item(array, i);
		list->addItem(QT_UTF8(obs_data_get_string(item, "value")));
		QListWidgetItem *const list_item = list->item((int)i);
		list_item->setSelected(obs_data_get_bool(item, "selected"));
		list_item->setHidden(obs_data_get_bool(item, "hidden"));
		QString uuid = QT_UTF8(obs_data_get_string(item, "uuid"));
		/* for backwards compatibility */
		if (uuid.isEmpty()) {
			uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
			obs_data_set_string(item, "uuid", uuid.toUtf8());
		}
		list_item->setData(Qt::UserRole, uuid);
	}

	WidgetInfo *info = new WidgetInfo(this, prop, list);

	list->setDragDropMode(QAbstractItemView::InternalMove);
	pls_connect(list->model(), &QAbstractItemModel::rowsMoved, [info]() { info->EditableListChanged(); });

	QVBoxLayout *sideLayout = new QVBoxLayout();
	NewButton(sideLayout, info, "addIconSmall", &WidgetInfo::EditListAdd);
	NewButton(sideLayout, info, "removeIconSmall", &WidgetInfo::EditListRemove);
	NewButton(sideLayout, info, "configIconSmall", &WidgetInfo::EditListEdit);
	NewButton(sideLayout, info, "upArrowIconSmall", &WidgetInfo::EditListUp);
	NewButton(sideLayout, info, "downArrowIconSmall", &WidgetInfo::EditListDown);
	sideLayout->addStretch(0);

	QHBoxLayout *subLayout = new QHBoxLayout();
	subLayout->addWidget(list);
	subLayout->addLayout(sideLayout);

	children.emplace_back(info);

	label = new QLabel(QT_UTF8(obs_property_description(prop)));
	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(label, subLayout);
}

QWidget *OBSPropertiesView::AddButton(obs_property_t *prop)
{
	const char *desc = obs_property_description(prop);
	const char *name = obs_property_name(prop);
	QString displayName = QT_UTF8(obs_property_description(prop));
	bool prismLensSource = isPrismLensOrMobileSource();
	if (pls_is_equal(name, "openPrismLens") && prismLensSource) {
		displayName = QTStr(desc);
	}

	QPushButton *button = new QPushButton(displayName);
	button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	QWidget *widget = NewWidget(prop, button, &QPushButton::clicked);

	bool isActivateBtn = pls_is_equal(name, "activate");
	if (prismLensSource && isActivateBtn) {
		QString program;
		button->setEnabled(pls_is_install_cam_studio(program));

		QWidget *lensWidget = pls_new<QWidget>(this);
		QHBoxLayout *hLayout = pls_new<QHBoxLayout>(lensWidget);
		hLayout->setContentsMargins(0, 0, 0, 0);
		hLayout->setSpacing(10);
		QPushButton *openLensBtn = pls_new<QPushButton>(QTStr("main.property.prism.lens.open"));
		pls_connect(openLensBtn, &QPushButton::clicked, this, &OBSPropertiesView::OnOpenPrismLensClicked);
		openLensBtn->setObjectName("openPrismLens");
		hLayout->addWidget(openLensBtn);
		hLayout->addWidget(widget);
		widget = lensWidget;
	}
	return widget;
}

void OBSPropertiesView::AddColorInternal(obs_property_t *prop, QFormLayout *layout, QLabel *&label, bool supportAlpha)
{
	QPushButton *button = new QPushButton;
	QLabel *colorLabel = new QLabel;
	const char *name = obs_property_name(prop);
	long long val = obs_data_get_int(settings, name);
	QColor color = color_from_int(val);
	QColor::NameFormat format;

	if (!obs_property_enabled(prop)) {
		button->setEnabled(false);
		colorLabel->setEnabled(false);
	}

	button->setText(tr("Basic.PropertiesWindow.SelectColor"));
	button->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	button->setStyleSheet("font-weight:bold;");

	if (supportAlpha) {
		format = QColor::HexArgb;
	} else {
		format = QColor::HexRgb;
		color.setAlpha(255);
	}

	QPalette palette = QPalette(color);
	colorLabel->setFrameStyle(QFrame::Sunken | QFrame::Panel);
	colorLabel->setText(color.name(format));
	colorLabel->setPalette(palette);
	colorLabel->setStyleSheet(QString("background-color :%1; color: %2;")
					  .arg(palette.color(QPalette::Window).name(format))
					  .arg(palette.color(QPalette::WindowText).name(format)));
	colorLabel->setAutoFillBackground(true);
	colorLabel->setAlignment(Qt::AlignCenter);
	colorLabel->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	colorLabel->setObjectName("baseColorLabel");

	QHBoxLayout *subLayout = new QHBoxLayout;
	subLayout->setContentsMargins(0, 0, 0, 0);

	subLayout->addWidget(colorLabel);
	subLayout->addSpacing(10);
	subLayout->addWidget(button);

	WidgetInfo *info = new WidgetInfo(this, prop, colorLabel);
	pls_connect(button, &QPushButton::clicked, info, &WidgetInfo::ControlChanged);
	children.emplace_back(info);

	label = new QLabel(QT_UTF8(obs_property_description(prop)));
	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(label, subLayout);
}

void OBSPropertiesView::AddColor(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	AddColorInternal(prop, layout, label, false);
}

void OBSPropertiesView::AddColorAlpha(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	AddColorInternal(prop, layout, label, true);
}

void MakeQFont(obs_data_t *font_obj, QFont &font, bool limit = false)
{
	const char *face = obs_data_get_string(font_obj, "face");
	const char *style = obs_data_get_string(font_obj, "style");
	int size = (int)obs_data_get_int(font_obj, "size");
	uint32_t flags = (uint32_t)obs_data_get_int(font_obj, "flags");

	if (face) {
		font.setFamily(face);
		font.setStyleName(style);
	}

	if (size) {
		if (limit) {
			int max_size = font.pointSize();
			if (max_size < 28)
				max_size = 28;
			if (size > max_size)
				size = max_size;
		}
		font.setPointSize(size);
	}

	if (flags & OBS_FONT_BOLD)
		font.setBold(true);
	if (flags & OBS_FONT_ITALIC)
		font.setItalic(true);
	if (flags & OBS_FONT_UNDERLINE)
		font.setUnderline(true);
	if (flags & OBS_FONT_STRIKEOUT)
		font.setStrikeOut(true);
}

void OBSPropertiesView::AddFont(obs_property_t *prop, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	OBSDataAutoRelease font_obj = obs_data_get_obj(settings, name);
	const char *face = obs_data_get_string(font_obj, "face");
	const char *style = obs_data_get_string(font_obj, "style");
	QPushButton *button = new QPushButton;
	QLabel *fontLabel = new QLabel;
	QFont font;

	if (!obs_property_enabled(prop)) {
		button->setEnabled(false);
		fontLabel->setEnabled(false);
	}

	font = fontLabel->font();
	MakeQFont(font_obj, font, true);

	button->setObjectName("fontButton");
	button->setProperty("themeID", "settingsButtons");
	button->setText(QTStr("Basic.PropertiesWindow.SelectFont"));
	button->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	fontLabel->setFrameStyle(QFrame::Sunken | QFrame::Panel);
	fontLabel->setFont(font);
	fontLabel->setText(QString("%1 %2").arg(face, style));
	fontLabel->setAlignment(Qt::AlignCenter);
	fontLabel->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	QHBoxLayout *subLayout = new QHBoxLayout;
	subLayout->setContentsMargins(0, 0, 0, 0);

	subLayout->addWidget(fontLabel);
	subLayout->addWidget(button);

	WidgetInfo *info = new WidgetInfo(this, prop, fontLabel);
	pls_connect(button, &QPushButton::clicked, info, &WidgetInfo::ControlChanged);
	children.emplace_back(info);

	label = new QLabel(QT_UTF8(obs_property_description(prop)));

	if (lastPropertyType == OBS_PROPERTY_INVALID) {
		layout->addItem(new QSpacerItem(10, 1, QSizePolicy::Fixed, QSizePolicy::Fixed));
	}
	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(label, subLayout);
}

namespace std {

template<> struct default_delete<obs_data_t> {
	void operator()(obs_data_t *data) { obs_data_release(data); }
};

template<> struct default_delete<obs_data_item_t> {
	void operator()(obs_data_item_t *item) { obs_data_item_release(&item); }
};

} // namespace std

template<typename T> static double make_epsilon(T val)
{
	return val * 0.00001;
}

static bool matches_range(media_frames_per_second &match, media_frames_per_second fps, const frame_rate_range_t &pair)
{
	auto val = media_frames_per_second_to_frame_interval(fps);
	auto max_ = media_frames_per_second_to_frame_interval(pair.first);
	auto min_ = media_frames_per_second_to_frame_interval(pair.second);

	if (min_ <= val && val <= max_) {
		match = fps;
		return true;
	}

	return false;
}

static bool matches_ranges(media_frames_per_second &best_match, media_frames_per_second fps,
			   const frame_rate_ranges_t &fps_ranges, bool exact = false)
{
	auto convert_fn = media_frames_per_second_to_frame_interval;
	auto val = convert_fn(fps);
	auto epsilon = make_epsilon(val);

	bool match = false;
	auto best_dist = numeric_limits<double>::max();
	for (auto &pair : fps_ranges) {
		auto max_ = convert_fn(pair.first);
		auto min_ = convert_fn(pair.second);
		/*blog(LOG_INFO, "%lg <= %lg <= %lg? %s %s %s",
				min_, val, max_,
				fabsl(min_ - val) < epsilon ? "true" : "false",
				min_ <= val && val <= max_  ? "true" : "false",
				fabsl(min_ - val) < epsilon ? "true" :
				"false");*/

		if (matches_range(best_match, fps, pair))
			return true;

		if (exact)
			continue;

		auto min_dist = fabsl(min_ - val);
		auto max_dist = fabsl(max_ - val);
		if (min_dist < epsilon && min_dist < best_dist) {
			best_match = pair.first;
			match = true;
			continue;
		}

		if (max_dist < epsilon && max_dist < best_dist) {
			best_match = pair.second;
			match = true;
			continue;
		}
	}

	return match;
}

static media_frames_per_second make_fps(uint32_t num, uint32_t den)
{
	media_frames_per_second fps{};
	fps.numerator = num;
	fps.denominator = den;
	return fps;
}

static const common_frame_rate common_fps[] = {
	{"240", {240, 1}},         {"144", {144, 1}},        {"120", {120, 1}}, {"119.88", {120000, 1001}},
	{"60", {60, 1}},           {"59.94", {60000, 1001}}, {"50", {50, 1}},   {"48", {48, 1}},
	{"30", {30, 1}},           {"29.97", {30000, 1001}}, {"25", {25, 1}},   {"24", {24, 1}},
	{"23.976", {24000, 1001}},
};

static void UpdateSimpleFPSSelection(OBSFrameRatePropertyWidget *fpsProps, const media_frames_per_second *current_fps)
{
	if (!current_fps || !media_frames_per_second_is_valid(*current_fps)) {
		fpsProps->simpleFPS->setCurrentIndex(0);
		return;
	}

	auto combo = fpsProps->simpleFPS;
	auto num = combo->count();
	for (int i = 0; i < num; i++) {
		auto variant = combo->itemData(i);
		if (!variant.canConvert<media_frames_per_second>())
			continue;

		auto fps = variant.value<media_frames_per_second>();
		if (fps != *current_fps)
			continue;

		combo->setCurrentIndex(i);
		return;
	}

	combo->setCurrentIndex(0);
}

static void AddFPSRanges(vector<common_frame_rate> &items, const frame_rate_ranges_t &ranges)
{
	auto InsertFPS = [&](media_frames_per_second fps) {
		auto fps_val = media_frames_per_second_to_fps(fps);

		auto end_ = end(items);
		auto i = begin(items);
		for (; i != end_; i++) {
			auto i_fps_val = media_frames_per_second_to_fps(i->fps);
			if (fabsl(i_fps_val - fps_val) < 0.01)
				return;

			if (i_fps_val > fps_val)
				continue;

			break;
		}

		items.insert(i, {nullptr, fps});
	};

	for (auto &range : ranges) {
		InsertFPS(range.first);
		InsertFPS(range.second);
	}
}

static QWidget *CreateSimpleFPSValues(OBSFrameRatePropertyWidget *fpsProps, bool &selected,
				      const media_frames_per_second *current_fps)
{
	auto widget = new QWidget{};
	widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto layout = new QVBoxLayout{};
	layout->setContentsMargins(0, 0, 0, 0);

	auto items = vector<common_frame_rate>{};
	items.reserve(sizeof(common_fps) / sizeof(common_frame_rate));

	auto combo = fpsProps->simpleFPS = new PLSComboBox();

	combo->addItem("", QVariant::fromValue(make_fps(0, 0)));
	for (const auto &fps : common_fps) {
		media_frames_per_second best_match{};
		if (!matches_ranges(best_match, fps.fps, fpsProps->fps_ranges))
			continue;

		items.push_back({fps.fps_name, best_match});
	}

	AddFPSRanges(items, fpsProps->fps_ranges);

	for (const auto &item : items) {
		auto var = QVariant::fromValue(item.fps);
		auto name = item.fps_name ? QString(item.fps_name)
					  : QString("%1").arg(media_frames_per_second_to_fps(item.fps));
		combo->addItem(name, var);

		bool select = current_fps && *current_fps == item.fps;
		if (select) {
			combo->setCurrentIndex(combo->count() - 1);
			selected = true;
		}
	}

	layout->addWidget(combo, 0, Qt::AlignTop);
	widget->setLayout(layout);

	return widget;
}

static void UpdateRationalFPSWidgets(OBSFrameRatePropertyWidget *fpsProps, const media_frames_per_second *current_fps)
{
	if (!current_fps || !media_frames_per_second_is_valid(*current_fps)) {
		fpsProps->numEdit->setValue(0);
		fpsProps->denEdit->setValue(0);
		return;
	}

	auto combo = fpsProps->fpsRange;
	auto num = combo->count();
	for (int i = 0; i < num; i++) {
		auto variant = combo->itemData(i);
		if (!variant.canConvert<size_t>())
			continue;

		auto idx = variant.value<size_t>();
		if (fpsProps->fps_ranges.size() < idx)
			continue;

		media_frames_per_second match{};
		if (!matches_range(match, *current_fps, fpsProps->fps_ranges[idx]))
			continue;

		combo->setCurrentIndex(i);
		break;
	}

	fpsProps->numEdit->setValue(current_fps->numerator);
	fpsProps->denEdit->setValue(current_fps->denominator);
}

static QWidget *CreateRationalFPS(OBSFrameRatePropertyWidget *fpsProps, bool &selected,
				  const media_frames_per_second *current_fps)
{
	auto widget = new QWidget{};
	widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto layout = new QFormLayout{};
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	auto str = QObject::tr("Basic.PropertiesView.FPS.ValidFPSRanges");
	auto rlabel = new QLabel{str};
	rlabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto combo = fpsProps->fpsRange = new PLSComboBox();
	combo->setObjectName("fpsRange");
	combo->setStyleSheet("width: 100%;");

	auto convert_fps = media_frames_per_second_to_fps;
	//auto convert_fi  = media_frames_per_second_to_frame_interval;

	for (size_t i = 0; i < fpsProps->fps_ranges.size(); i++) {
		auto &pair = fpsProps->fps_ranges[i];
		combo->addItem(QString{"%1 - %2"}.arg(convert_fps(pair.first)).arg(convert_fps(pair.second)),
			       QVariant::fromValue(i));

		media_frames_per_second match;
		if (!current_fps || !matches_range(match, *current_fps, pair))
			continue;

		combo->setCurrentIndex(combo->count() - 1);
		selected = true;
	}

	layout->addRow(rlabel, combo);

	auto numLabel = new QLabel(QObject::tr("Basic.Settings.Video.Numerator"));
	numLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto denLabel = new QLabel(QObject::tr("Basic.Settings.Video.Denominator"));
	denLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto num_edit = fpsProps->numEdit = new SpinBoxIgnoreScroll{};
	auto den_edit = fpsProps->denEdit = new SpinBoxIgnoreScroll{};

	num_edit->setRange(0, INT_MAX);
	den_edit->setRange(0, INT_MAX);

	num_edit->setStyleSheet("width: 100%;");
	den_edit->setStyleSheet("width: 100%;");

	if (current_fps) {
		num_edit->setValue(current_fps->numerator);
		den_edit->setValue(current_fps->denominator);
	}

	layout->addRow(numLabel, num_edit);
	layout->addRow(denLabel, den_edit);

	widget->setLayout(layout);

	return widget;
}

static OBSFrameRatePropertyWidget *CreateFrameRateWidget(obs_property_t *prop, bool &warning, const char *option,
							 media_frames_per_second *current_fps,
							 frame_rate_ranges_t &fps_ranges)
{
	auto widget = new OBSFrameRatePropertyWidget{};
	auto hlayout = new QVBoxLayout{};
	hlayout->setContentsMargins(0, 0, 0, 0);

	swap(widget->fps_ranges, fps_ranges);

	auto combo = widget->modeSelect = new PLSComboBox();
	combo->setObjectName("modeSelect");
	combo->addItem(QObject::tr("Basic.PropertiesView.FPS.Simple"), QVariant::fromValue(frame_rate_tag::simple()));
	combo->addItem(QObject::tr("Basic.PropertiesView.FPS.Rational"),
		       QVariant::fromValue(frame_rate_tag::rational()));

	combo->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	auto num = obs_property_frame_rate_options_count(prop);
	if (num)
		combo->insertSeparator(combo->count());

	bool option_found = false;
	for (size_t i = 0; i < num; i++) {
		auto name = obs_property_frame_rate_option_name(prop, i);
		auto desc = obs_property_frame_rate_option_description(prop, i);
		combo->addItem(desc, QVariant::fromValue(frame_rate_tag{name}));

		if (!name || !option || string(name) != option)
			continue;

		option_found = true;
		combo->setCurrentIndex(combo->count() - 1);
	}

	hlayout->addWidget(combo, 0, Qt::AlignTop);

	auto stack = widget->modeDisplay = new QStackedWidget{};
	stack->setObjectName("modeDisplay");

	bool match_found = option_found;
	auto AddWidget = [&](decltype(CreateRationalFPS) func) {
		bool selected = false;
		stack->addWidget(func(widget, selected, current_fps));

		if (match_found || !selected)
			return;

		match_found = true;

		stack->setCurrentIndex(stack->count() - 1);
		combo->setCurrentIndex(stack->count() - 1);
	};

	AddWidget(CreateSimpleFPSValues);
	AddWidget(CreateRationalFPS);
	stack->addWidget(new QWidget{});

	if (option_found)
		stack->setCurrentIndex(stack->count() - 1);
	else if (!match_found) {
		int idx = current_fps ? 1 : 0; // Rational for "unsupported"
					       // Simple as default
		stack->setCurrentIndex(idx);
		combo->setCurrentIndex(idx);
		warning = true;
	}

	auto hlayoutNextLine = new QHBoxLayout{};
	hlayoutNextLine->setContentsMargins(0, 0, 0, 0);
	hlayout->addLayout(hlayoutNextLine);
	hlayoutNextLine->addWidget(stack, 0, Qt::AlignTop);

	auto label_area = widget->labels = new QWidget{};
	label_area->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	auto vlayout = new QVBoxLayout{};
	vlayout->setContentsMargins(0, 0, 0, 0);

	auto fps_label = widget->currentFPS = new QLabel{"FPS: 22"};
	auto time_label = widget->timePerFrame = new QLabel{"Frame Interval: 0.123 ms"};
	auto min_label = widget->minLabel = new QLabel{"Min FPS: 1/1"};
	auto max_label = widget->maxLabel = new QLabel{"Max FPS: 2/1"};

	min_label->setHidden(true);
	max_label->setHidden(true);

	auto flags = Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard;
	min_label->setTextInteractionFlags(flags);
	max_label->setTextInteractionFlags(flags);

	vlayout->addWidget(fps_label);
	vlayout->addWidget(time_label);
	vlayout->addWidget(min_label);
	vlayout->addWidget(max_label);
	label_area->setLayout(vlayout);

	hlayoutNextLine->addWidget(label_area, 0, Qt::AlignTop);

	widget->setLayout(hlayout);

	return widget;
}

static void UpdateMinMaxLabels(OBSFrameRatePropertyWidget *w)
{
	auto Hide = [&](bool hide) {
		w->minLabel->setHidden(hide);
		w->maxLabel->setHidden(hide);
	};

	auto variant = w->modeSelect->currentData();
	if (!variant.canConvert<frame_rate_tag>() || variant.value<frame_rate_tag>().type != frame_rate_tag::RATIONAL) {
		Hide(true);
		return;
	}

	variant = w->fpsRange->currentData();
	if (!variant.canConvert<size_t>()) {
		Hide(true);
		return;
	}

	auto idx = variant.value<size_t>();
	if (idx >= w->fps_ranges.size()) {
		Hide(true);
		return;
	}

	Hide(false);

	auto min = w->fps_ranges[idx].first;
	auto max = w->fps_ranges[idx].second;

	w->minLabel->setText(QString("Min FPS: %1/%2").arg(min.numerator).arg(min.denominator));
	w->maxLabel->setText(QString("Max FPS: %1/%2").arg(max.numerator).arg(max.denominator));
}

static void UpdateFPSLabels(OBSFrameRatePropertyWidget *w)
{
	UpdateMinMaxLabels(w);

	unique_ptr<obs_data_item_t> obj{obs_data_item_byname(w->settings, w->name)};

	media_frames_per_second fps{};
	media_frames_per_second *valid_fps = nullptr;
	if (obs_data_item_get_autoselect_frames_per_second(obj.get(), &fps, nullptr) ||
	    obs_data_item_get_frames_per_second(obj.get(), &fps, nullptr))
		valid_fps = &fps;

	const char *option = nullptr;
	obs_data_item_get_frames_per_second(obj.get(), nullptr, &option);

	if (!valid_fps) {
		w->currentFPS->setHidden(true);
		w->timePerFrame->setHidden(true);
		if (!option) {
			w->warningLabel->setObjectName("errorLabel");
			w->warningLabel->setProperty("class", "text-danger");
		}

		return;
	}

	w->currentFPS->setHidden(false);
	w->timePerFrame->setHidden(false);

	media_frames_per_second match{};
	if (!option && !matches_ranges(match, *valid_fps, w->fps_ranges, true)) {
		w->warningLabel->setObjectName("errorLabel");
		w->warningLabel->setProperty("class", "text-danger");
	} else {
		w->warningLabel->setObjectName("");
		w->warningLabel->setProperty("class", "");
	}

	auto convert_to_fps = media_frames_per_second_to_fps;
	auto convert_to_frame_interval = media_frames_per_second_to_frame_interval;

	w->currentFPS->setText(QString("FPS: %1").arg(convert_to_fps(*valid_fps)));
	w->timePerFrame->setText(QString("Frame Interval: %1 ms").arg(convert_to_frame_interval(*valid_fps) * 1000));
}

void OBSPropertiesView::AddFrameRate(obs_property_t *prop, bool &warning, QFormLayout *layout, QLabel *&label)
{
	const char *name = obs_property_name(prop);
	bool enabled = obs_property_enabled(prop);
	unique_ptr<obs_data_item_t> obj{obs_data_item_byname(settings, name)};

	const char *option = nullptr;
	obs_data_item_get_frames_per_second(obj.get(), nullptr, &option);

	media_frames_per_second fps{};
	media_frames_per_second *valid_fps = nullptr;
	if (obs_data_item_get_frames_per_second(obj.get(), &fps, nullptr))
		valid_fps = &fps;

	frame_rate_ranges_t fps_ranges;
	size_t num = obs_property_frame_rate_fps_ranges_count(prop);
	fps_ranges.reserve(num);
	for (size_t i = 0; i < num; i++)
		fps_ranges.emplace_back(obs_property_frame_rate_fps_range_min(prop, i),
					obs_property_frame_rate_fps_range_max(prop, i));

	auto widget = CreateFrameRateWidget(prop, warning, option, valid_fps, fps_ranges);
	auto info = new WidgetInfo(this, prop, widget);

	widget->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	widget->name = name;
	widget->settings = settings;

	widget->modeSelect->setEnabled(enabled);
	widget->simpleFPS->setEnabled(enabled);
	widget->fpsRange->setEnabled(enabled);
	widget->numEdit->setEnabled(enabled);
	widget->denEdit->setEnabled(enabled);

	label = widget->warningLabel = new QLabel{obs_property_description(prop)};

	AddSpacer(obs_property_get_type(prop), layout);
	layout->addRow(label, widget);

	children.emplace_back(info);

	UpdateFPSLabels(widget);

	auto stack = widget->modeDisplay;
	auto combo = widget->modeSelect;

	stack->setToolTip(QT_UTF8(obs_property_long_description(prop)));
	combo->setToolTip(QT_UTF8(obs_property_long_description(prop)));

	auto comboIndexChanged = static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged);
	pls_connect(combo, comboIndexChanged, stack, [=](int index) {
		bool out_of_bounds = index >= stack->count();
		auto idx = out_of_bounds ? stack->count() - 1 : index;
		stack->setCurrentIndex(idx);

		if (widget->updating)
			return;

		UpdateFPSLabels(widget);
		emit info->ControlChanged();
	});

	pls_connect(widget->simpleFPS, comboIndexChanged, [=](int) {
		if (widget->updating)
			return;

		emit info->ControlChanged();
	});

	pls_connect(widget->fpsRange, comboIndexChanged, [=](int) {
		if (widget->updating)
			return;

		UpdateFPSLabels(widget);
	});

	auto sbValueChanged = static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged);
	pls_connect(widget->numEdit, sbValueChanged, [=](int) {
		if (widget->updating)
			return;

		emit info->ControlChanged();
	});

	pls_connect(widget->denEdit, sbValueChanged, [=](int) {
		if (widget->updating)
			return;

		emit info->ControlChanged();
	});
}

void OBSPropertiesView::AddGroup(obs_property_t *prop, QFormLayout *layout)
{
	if (lastPropertyType != OBS_PROPERTY_INVALID)
		layout->addItem(new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));

	const char *name = obs_property_name(prop);
	bool val = obs_data_get_bool(settings, name);
	const char *desc = obs_property_description(prop);
	enum obs_group_type type = obs_property_group_type(prop);

	QWidget *innerWidget = pls_new<QWidget>();
	innerWidget->setObjectName("innerGroupWidget");
	QVBoxLayout *innerLayout = pls_new<QVBoxLayout>(innerWidget);
	innerLayout->setContentsMargins(0, 15, 0, 0);

	// Create GroupBox
	QGroupBox *groupBox = new QGroupBox(QT_UTF8(desc));
	groupBox->setCheckable(type == OBS_GROUP_CHECKABLE);
	groupBox->setChecked(groupBox->isCheckable() ? val : true);
	groupBox->setAccessibleName("group");
	groupBox->setEnabled(obs_property_enabled(prop));

	innerLayout->addWidget(groupBox);

	// Create Layout and build content
	QFormLayout *subLayout = new QFormLayout();
	subLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	groupBox->setLayout(subLayout);

	obs_properties_t *content = obs_property_group_content(prop);
	obs_property_t *el = obs_properties_first(content);
	obs_property_type originType = lastPropertyType;
	while (el != nullptr) {
		lastPropertyType = OBS_PROPERTY_INVALID;
		AddProperty(el, subLayout);
		obs_property_next(&el);
	}
	lastPropertyType = originType;

	// Insert into UI
	layout->setWidget(layout->rowCount(), QFormLayout::ItemRole::SpanningRole, innerWidget);
	AddSpacer(obs_property_get_type(prop), layout);

	// Register Group Widget
	WidgetInfo *info = new WidgetInfo(this, prop, groupBox);
	children.emplace_back(info);

	// Signals
	pls_connect(groupBox, &QGroupBox::toggled, info, &WidgetInfo::ControlChanged);
}

void OBSPropertiesView::AddProperty(obs_property_t *property, QFormLayout *layout)
{
	const char *name = obs_property_name(property);
	obs_property_type type = obs_property_get_type(property);

	if (!obs_property_visible(property))
		return;

	QLabel *label = nullptr;
	QWidget *widget = nullptr;
	bool warning = false;

	switch (type) {
	case OBS_PROPERTY_INVALID:
		return;
	case OBS_PROPERTY_BOOL:
		widget = AddCheckbox(layout, property);
		break;
	case OBS_PROPERTY_INT:
		AddInt(property, layout, &label);
		break;
	case OBS_PROPERTY_FLOAT:
		AddFloat(property, layout, &label);
		break;
	case OBS_PROPERTY_TEXT:
		widget = AddText(property, layout, label);
		break;
	case OBS_PROPERTY_PATH:
		AddPath(property, layout, &label);
		break;
	case OBS_PROPERTY_LIST:
		widget = AddList(property, warning);
		break;
	case OBS_PROPERTY_COLOR:
		AddColor(property, layout, label);
		break;
	case OBS_PROPERTY_FONT:
		AddFont(property, layout, label);
		break;
	case OBS_PROPERTY_BUTTON:
		widget = AddButton(property);
		break;
	case OBS_PROPERTY_EDITABLE_LIST:
		AddEditableList(property, layout, label);
		break;
	case OBS_PROPERTY_FRAME_RATE:
		AddFrameRate(property, warning, layout, label);
		break;
	case OBS_PROPERTY_GROUP:
		AddGroup(property, layout);
		break;
	case OBS_PROPERTY_COLOR_ALPHA:
		AddColorAlpha(property, layout, label);
	}

	updateUIWhenAfterAddProperty(property, layout, label, widget, warning);
}

void OBSPropertiesView::SignalChanged()
{
	emit Changed();
}

static bool FrameRateChangedVariant(const QVariant &variant, media_frames_per_second &fps, obs_data_item_t *&obj,
				    const media_frames_per_second *valid_fps)
{
	if (!variant.canConvert<media_frames_per_second>())
		return false;

	fps = variant.value<media_frames_per_second>();
	if (valid_fps && fps == *valid_fps)
		return false;

	obs_data_item_set_frames_per_second(&obj, fps, nullptr);
	return true;
}

static bool FrameRateChangedCommon(OBSFrameRatePropertyWidget *w, obs_data_item_t *&obj,
				   const media_frames_per_second *valid_fps)
{
	media_frames_per_second fps{};
	if (!FrameRateChangedVariant(w->simpleFPS->currentData(), fps, obj, valid_fps))
		return false;

	UpdateRationalFPSWidgets(w, &fps);
	return true;
}

static bool FrameRateChangedRational(OBSFrameRatePropertyWidget *w, obs_data_item_t *&obj,
				     const media_frames_per_second *valid_fps)
{
	auto num = w->numEdit->value();
	auto den = w->denEdit->value();

	auto fps = make_fps(num, den);
	if (valid_fps && media_frames_per_second_is_valid(fps) && fps == *valid_fps)
		return false;

	obs_data_item_set_frames_per_second(&obj, fps, nullptr);
	UpdateSimpleFPSSelection(w, &fps);
	return true;
}

static bool FrameRateChanged(QWidget *widget, const char *name, OBSData &settings)
{
	auto w = qobject_cast<OBSFrameRatePropertyWidget *>(widget);
	if (!w)
		return false;

	auto variant = w->modeSelect->currentData();
	if (!variant.canConvert<frame_rate_tag>())
		return false;

	auto StopUpdating = [&](void *) {
		w->updating = false;
	};
	unique_ptr<void, decltype(StopUpdating)> signalGuard(static_cast<void *>(w), StopUpdating);
	w->updating = true;

	if (!obs_data_has_user_value(settings, name))
		obs_data_set_obj(settings, name, nullptr);

	unique_ptr<obs_data_item_t> obj{obs_data_item_byname(settings, name)};
	auto obj_ptr = obj.get();
	auto CheckObj = [&]() {
		if (!obj_ptr)
			obj.release();
	};

	const char *option = nullptr;
	obs_data_item_get_frames_per_second(obj.get(), nullptr, &option);

	media_frames_per_second fps{};
	media_frames_per_second *valid_fps = nullptr;
	if (obs_data_item_get_frames_per_second(obj.get(), &fps, nullptr))
		valid_fps = &fps;

	auto tag = variant.value<frame_rate_tag>();
	switch (tag.type) {
	case frame_rate_tag::SIMPLE:
		if (!FrameRateChangedCommon(w, obj_ptr, valid_fps))
			return false;
		break;

	case frame_rate_tag::RATIONAL:
		if (!FrameRateChangedRational(w, obj_ptr, valid_fps))
			return false;
		break;

	case frame_rate_tag::USER:
		if (tag.val && option && strcmp(tag.val, option) == 0)
			return false;

		obs_data_item_set_frames_per_second(&obj_ptr, {}, tag.val);
		break;
	}

	UpdateFPSLabels(w);
	CheckObj();
	return true;
}

void WidgetInfo::BoolChanged(const char *setting)
{
	PLSCheckBox *checkbox = static_cast<PLSCheckBox *>(widget);
	obs_data_set_bool(view->settings, setting, checkbox->isChecked());
	const char *name = obs_property_name(property);
	if (pls_is_equal(name, "use_custom_audio_device") && view->isPrismLensOrMobileSource()) {
		auto source = pls_get_source_by_pointer_address(view->GetSourceObj());
		PLS_INFO("main/property", "source [%s] use_custom_audio_device value changed to : %d",
			 obs_source_get_name(source), checkbox->isChecked());
	}
}

void WidgetInfo::IntChanged(const char *setting)
{
	QSpinBox *spin = static_cast<QSpinBox *>(widget);
	obs_data_set_int(view->settings, setting, spin->value());
}

void WidgetInfo::FloatChanged(const char *setting)
{
	QDoubleSpinBox *spin = static_cast<QDoubleSpinBox *>(widget);
	obs_data_set_double(view->settings, setting, spin->value());
}

void WidgetInfo::TextChanged(const char *setting)
{
	obs_text_type type = obs_property_text_type(property);

	if (type == OBS_TEXT_MULTILINE) {
		OBSPlainTextEdit *edit = static_cast<OBSPlainTextEdit *>(widget);
		obs_data_set_string(view->settings, setting, QT_TO_UTF8(edit->toPlainText()));
		return;
	}

	PLSLineEdit *edit = static_cast<PLSLineEdit *>(widget);
	QString limitText = edit->text();
	auto length = pls_property_get_length_limit(property);
	if (length > 0 && limitText.length() > length) {
		QSignalBlocker signalBlocker(edit);
		limitText = edit->text().left(length);
		edit->setText(limitText);
	}

	obs_data_set_string(view->settings, setting, QT_TO_UTF8(edit->text()));
}

bool WidgetInfo::PathChanged(const char *setting)
{
	const char *desc = obs_property_description(property);
	obs_path_type type = obs_property_path_type(property);
	const char *filter = obs_property_path_filter(property);
	const char *default_path = obs_property_path_default_path(property);

	QPointer<PLSLineEdit> edit = static_cast<PLSLineEdit *>(widget);

	QString startDir = edit->text();
	if (startDir.isEmpty())
		startDir = default_path;

	QString path;

	if (type == OBS_PATH_DIRECTORY)
		path = SelectDirectory(view, QT_UTF8(desc), startDir);
	else if (type == OBS_PATH_FILE)
		path = OpenFile(view, QT_UTF8(desc), startDir, QT_UTF8(filter));
	else if (type == OBS_PATH_FILE_SAVE)
		path = SaveFile(view, QT_UTF8(desc), startDir, QT_UTF8(filter));

#ifdef __APPLE__
	// TODO: Revisit when QTBUG-42661 is fixed
	pls_check_app_exiting(false);
	widget->window()->raise();
#endif

	if (path.isEmpty() || !edit)
		return false;

	edit->setText(path);
	obs_data_set_string(view->settings, setting, QT_TO_UTF8(path));
	return true;
}

void WidgetInfo::ListChanged(const char *setting)
{
	obs_combo_format format = obs_property_list_format(property);
	obs_combo_type type = obs_property_list_type(property);
	QVariant data;
	QComboBox *combo = nullptr;
	if (type == OBS_COMBO_TYPE_RADIO) {
		PLSRadioButtonGroup *group = static_cast<PLSRadioButton *>(widget)->group();
		PLSRadioButton *button = group->checkedButton();
		data = button->property("value");
	} else if (type == OBS_COMBO_TYPE_EDITABLE) {
		data = static_cast<QComboBox *>(widget)->currentText().toUtf8();
	} else {
		combo = static_cast<QComboBox *>(widget);
		int index = combo->currentIndex();
		if (index != -1)
			data = combo->itemData(index);
		else
			return;
	}

	switch (format) {
	case OBS_COMBO_FORMAT_INVALID:
		return;
	case OBS_COMBO_FORMAT_INT:
		obs_data_set_int(view->settings, setting, data.value<long long>());
		break;
	case OBS_COMBO_FORMAT_FLOAT:
		obs_data_set_double(view->settings, setting, data.value<double>());
		break;
	case OBS_COMBO_FORMAT_STRING: {
		obs_data_set_string(view->settings, setting, data.toByteArray().constData());
		auto source = pls_get_source_by_pointer_address(view->GetSourceObj());
		auto id = obs_source_get_id(source);
		bool isVideoDeviceId = false;
#if defined(Q_OS_WIN)
		bool isDshow = pls_is_equal(id, OBS_DSHOW_SOURCE_ID);
		isVideoDeviceId = pls_is_equal(setting, "video_device_id");
#elif defined(Q_OS_MACOS)
		bool isDshow = pls_is_equal(id, OBS_DSHOW_SOURCE_ID_V2) || pls_is_equal(id, OBS_DSHOW_SOURCE_ID);
		isVideoDeviceId = pls_is_equal(setting, "device");
#endif
		if (isVideoDeviceId && (view->isPrismLensOrMobileSource() || isDshow)) {
			if (combo) {
				DeviceChanged(combo->currentText(), isDshow,
					      pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID));
			}
		}
	} break;
	case OBS_COMBO_FORMAT_BOOL:
		obs_data_set_bool(view->settings, setting, data.value<double>());
		break;
	}
}

bool WidgetInfo::ColorChangedInternal(const char *setting, bool supportAlpha)
{
	shared_ptr<WidgetInfo> this_object = this->shared_from_this();
	QByteArray _id = setting;

	const char *desc = obs_property_description(property);
	obs_property_type type = obs_property_get_type(property);

	long long val = 0;
	if ((int)type == (int)PLS_PROPERTY_COLOR_CHECKBOX) {
		obs_data_t *color_obj = obs_data_get_obj(view->settings, setting);
		val = obs_data_get_int(color_obj, "color_val");
		obs_data_release(color_obj);
	} else {
		val = obs_data_get_int(view->settings, setting);
	}

	QColor color = color_from_int(val);
	QColor::NameFormat format;

	QColorDialog::ColorDialogOptions options;

	if (supportAlpha) {
		options |= QColorDialog::ShowAlphaChannel;
	}

#ifdef __linux__
	// TODO: Revisit hang on Ubuntu with native dialog
	options |= QColorDialog::DontUseNativeDialog;
#endif

	color = PLSColorDialogView::getColor(color, view, QT_UTF8(desc), options);

#ifdef __APPLE__
	// TODO: Revisit when QTBUG-42661 is fixed
	widget->window()->raise();
#endif

	if (!color.isValid())
		return false;

	if (supportAlpha) {
		format = QColor::HexArgb;
	} else {
		color.setAlpha(255);
		format = QColor::HexRgb;
	}

	setIsControlChanging(false);
	this_object->view->textColorChanged(_id, color, format);
	return false;
}

bool WidgetInfo::ColorChanged(const char *setting)
{
	return ColorChangedInternal(setting, false);
}

bool WidgetInfo::ColorAlphaChanged(const char *setting)
{
	return ColorChangedInternal(setting, true);
}

bool WidgetInfo::FontChanged(const char *setting)
{
	OBSDataAutoRelease font_obj = obs_data_get_obj(view->settings, setting);
	bool success;
	uint32_t flags;
	QFont font;

	QFontDialog::FontDialogOptions options;

#ifndef _WIN32
	options = QFontDialog::DontUseNativeDialog;
#endif

	if (!font_obj) {
		QFont initial;
		font = PLSFontDialogView::getFont(&success, initial, view,
						  tr("Basic.PropertiesWindow.SelectFont.WindowTitle"), options);
	} else {
		MakeQFont(font_obj, font);
		font = PLSFontDialogView::getFont(&success, font, view,
						  tr("Basic.PropertiesWindow.SelectFont.WindowTitle"), options);
	}

	if (!success)
		return false;

	font_obj = obs_data_create();

	obs_data_set_string(font_obj, "face", QT_TO_UTF8(font.family()));
	obs_data_set_string(font_obj, "style", QT_TO_UTF8(font.styleName()));
	obs_data_set_int(font_obj, "size", font.pointSize());
	flags = font.bold() ? OBS_FONT_BOLD : 0;
	flags |= font.italic() ? OBS_FONT_ITALIC : 0;
	flags |= font.underline() ? OBS_FONT_UNDERLINE : 0;
	flags |= font.strikeOut() ? OBS_FONT_STRIKEOUT : 0;
	obs_data_set_int(font_obj, "flags", flags);

	QLabel *label = static_cast<QLabel *>(widget);
	QFont labelFont;
	MakeQFont(font_obj, labelFont, true);
	label->setFont(labelFont);
	label->setText(QString("%1 %2").arg(font.family(), font.styleName()));

	obs_data_set_obj(view->settings, setting, font_obj);
	return true;
}

void WidgetInfo::GroupChanged(const char *setting)
{
	QGroupBox *groupbox = static_cast<QGroupBox *>(widget);
	obs_data_set_bool(view->settings, setting, groupbox->isCheckable() ? groupbox->isChecked() : true);
}

void WidgetInfo::EditableListChanged()
{
	const char *setting = obs_property_name(property);
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	OBSDataArrayAutoRelease array = obs_data_array_create();

	for (int i = 0; i < list->count(); i++) {
		QListWidgetItem *item = list->item(i);
		OBSDataAutoRelease arrayItem = obs_data_create();
		obs_data_set_string(arrayItem, "value", QT_TO_UTF8(item->text()));
		obs_data_set_string(arrayItem, "uuid", QT_TO_UTF8(item->data(Qt::UserRole).toString()));
		obs_data_set_bool(arrayItem, "selected", item->isSelected());
		obs_data_set_bool(arrayItem, "hidden", item->isHidden());
		obs_data_array_push_back(array, arrayItem);
	}

	obs_data_set_array(view->settings, setting, array);

	ControlChanged();
}

void WidgetInfo::ButtonClicked()
{
	obs_button_type type = obs_property_button_type(property);
	const char *savedUrl = obs_property_button_url(property);

	if (type == OBS_BUTTON_URL && strcmp(savedUrl, "") != 0) {
		QUrl url(savedUrl, QUrl::StrictMode);
		if (url.isValid() && (url.scheme().compare("http") == 0 || url.scheme().compare("https") == 0)) {
			QString msg(tr("Basic.PropertiesView.UrlButton.Text"));
			msg += "\n\n";
			msg += QString(tr("Basic.PropertiesView.UrlButton.Text.Url")).arg(savedUrl);

			QMessageBox::StandardButton button =
				OBSMessageBox::question(view->window(), tr("Basic.PropertiesView.UrlButton.OpenUrl"),
							msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

			if (button == QMessageBox::Yes)
				QDesktopServices::openUrl(url);
		}
		return;
	}

	OBSSource source = pls_get_source_by_pointer_address(view->GetSourceObj());
	if (source) {
		if (0 == strcmp(obs_source_get_id(source), BGM_SOURCE_ID)) {
			pls_set_bgm_visible(true);
		}
	}
	QPointer<WidgetInfo> weakThis(this);
	const char *name = obs_property_name(property);
	if (pls_is_equal(name, "openPrismLens")) {
		view->OnOpenPrismLensClicked();
	}

	if (!weakThis) {
		return;
	}

	OBSObject strongObj = view->GetObject();
	void *obj = strongObj ? strongObj.Get() : view->rawObj;

	if (obs_property_button_clicked(property, obj) && weakThis) {
		setIsControlChanging(false);
		QMetaObject::invokeMethod(view, "RefreshProperties", Qt::QueuedConnection);
	}
}

void WidgetInfo::TogglePasswordText(bool show)
{
	reinterpret_cast<PLSLineEdit *>(widget)->setEchoMode(show ? PLSLineEdit::Normal : PLSLineEdit::Password);
}

void WidgetInfo::ControlChanged()
{
	const char *setting = obs_property_name(property);
	obs_property_type type = obs_property_get_type(property);

	if (!recently_updated) {
		old_settings_cache = obs_data_create();
		obs_data_apply(old_settings_cache, view->settings);
		obs_data_release(old_settings_cache);
	}

	switch (type) {
	case OBS_PROPERTY_INVALID:
		return;
	case OBS_PROPERTY_BOOL:
		BoolChanged(setting);
		break;
	case OBS_PROPERTY_INT:
		IntChanged(setting);
		break;
	case OBS_PROPERTY_FLOAT:
		FloatChanged(setting);
		break;
	case OBS_PROPERTY_TEXT:
		TextChanged(setting);
		break;
	case OBS_PROPERTY_LIST:
		ListChanged(setting);
		break;
	case OBS_PROPERTY_BUTTON:
		ButtonClicked();
		return;
	case OBS_PROPERTY_COLOR:
		ColorChanged(setting);
		return;
	case OBS_PROPERTY_FONT:
		if (!FontChanged(setting))
			return;
		break;
	case OBS_PROPERTY_PATH:
		if (!PathChanged(setting))
			return;
		break;
	case OBS_PROPERTY_EDITABLE_LIST:
		break;
	case OBS_PROPERTY_FRAME_RATE:
		setIsControlChanging(false);
		if (!FrameRateChanged(widget, setting, view->settings))
			return;
		break;
	case OBS_PROPERTY_GROUP:
		GroupChanged(setting);
		break;
	case OBS_PROPERTY_COLOR_ALPHA:
		ColorAlphaChanged(setting);
		return;
	}

	ControlChangedToRefresh(setting);
}

class EditableItemDialog : public PLSDialogView {
	PLSLineEdit *edit;
	QString filter;
	QString default_path;

	void BrowseClicked()
	{
		QString curPath = QFileInfo(edit->text()).absoluteDir().path();

		if (curPath.isEmpty())
			curPath = default_path;

		QString path = OpenFile(this, tr("Browse"), curPath, filter);
		if (path.isEmpty())
			return;

		edit->setText(path);
	}

public:
	EditableItemDialog(QWidget *parent, const QString &text, bool browse, const char *filter_ = nullptr,
			   const char *default_path_ = nullptr)
		: PLSDialogView(parent),
		  filter(QT_UTF8(filter_)),
		  default_path(QT_UTF8(default_path_))
	{
		QHBoxLayout *topLayout = new QHBoxLayout();
		QVBoxLayout *mainLayout = new QVBoxLayout();

		edit = new PLSLineEdit();
		edit->setText(text);
		topLayout->addWidget(edit);
		topLayout->setAlignment(edit, Qt::AlignVCenter);

		if (browse) {
			QPushButton *browseButton = new QPushButton(tr("Browse"));
			topLayout->addWidget(browseButton);
			topLayout->setAlignment(browseButton, Qt::AlignVCenter);

			pls_connect(browseButton, &QPushButton::clicked, this, &EditableItemDialog::BrowseClicked);
		}

		QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel;

		QDialogButtonBox *buttonBox = new QDialogButtonBox(buttons);
		buttonBox->setCenterButtons(true);

		mainLayout->addLayout(topLayout);
		mainLayout->addWidget(buttonBox);

		content()->setLayout(mainLayout);
		resize(QSize(400, 80));

		pls_connect(buttonBox, &QDialogButtonBox::accepted, this, &EditableItemDialog::accept);
		pls_connect(buttonBox, &QDialogButtonBox::rejected, this, &EditableItemDialog::reject);
	}

	inline QString GetText() const { return edit->text(); }
};

void WidgetInfo::ControlChangedToRefresh(const char *setting)
{
	if (!recently_updated) {
		recently_updated = true;
		update_timer = new QTimer;
		pls_connect(update_timer.get(), &QTimer::timeout, [this, &ru = recently_updated]() {
			if (!pls_object_is_valid(view)) {
				return;
			}
			void *obj = view->GetSourceObj();
			if (obj && view->callback && !view->deferUpdate) {
				view->callback(obj, old_settings_cache, view->settings);
			}

			ru = false;
		});
		pls_connect(update_timer.get(), &QTimer::timeout, &QTimer::deleteLater);
		update_timer->setSingleShot(true);
	}

	if (update_timer) {
		update_timer->stop();
		update_timer->start(500);
	} else {
		blog(LOG_DEBUG, "No update timer or no callback!");
	}

	if (!pls_object_is_valid(view)) {
		PLS_INFO("main/property", "ControlChangedToRefresh view is invalid");
		return;
	}
	view->controlChangedToRefresh(property, setting);
}

void WidgetInfo::CheckValue()
{
	obs_property_type type = obs_property_get_type(property);
	auto combo = dynamic_cast<QComboBox *>(widget);
	if (type == OBS_PROPERTY_LIST && combo) {
		QString key = QString(obs_property_name(property)) + action::PROPERTY_LIST_SELECTED_KEY;
		if (combo->currentIndex() != -1)
			obs_data_set_string(view->settings, key.toUtf8(), combo->currentText().toUtf8());
		else
			obs_data_set_string(view->settings, key.toUtf8(), "");
	}
}

void WidgetInfo::EditListAdd()
{
	enum obs_editable_list_type type = obs_property_editable_list_type(property);

	if (type == OBS_EDITABLE_LIST_TYPE_STRINGS) {
		EditListAddText();
		return;
	}

	/* Files and URLs */
	QMenu popup(view->window());

	QAction *action;

	action = new QAction(tr("Basic.PropertiesWindow.AddFiles"), this);
	pls_connect(action, &QAction::triggered, this, &WidgetInfo::EditListAddFiles);
	popup.addAction(action);

	action = new QAction(tr("Basic.PropertiesWindow.AddDir"), this);
	pls_connect(action, &QAction::triggered, this, &WidgetInfo::EditListAddDir);
	popup.addAction(action);

	if (type == OBS_EDITABLE_LIST_TYPE_FILES_AND_URLS) {
		action = new QAction(tr("Basic.PropertiesWindow.AddURL"), this);
		pls_connect(action, &QAction::triggered, this, &WidgetInfo::EditListAddText);
		popup.addAction(action);
	}

	popup.exec(QCursor::pos());
}

void WidgetInfo::EditListAddText()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	const char *desc = obs_property_description(property);

	EditableItemDialog dialog(widget->window(), QString(), false);
	auto title = tr("Basic.PropertiesWindow.AddEditableListEntry").arg(QT_UTF8(desc));
	dialog.setWindowTitle(title);
	if (dialog.exec() == QDialog::Rejected)
		return;

	QString text = dialog.GetText();
	if (text.isEmpty())
		return;

	QListWidgetItem *item = new QListWidgetItem(text);
	item->setData(Qt::UserRole, QUuid::createUuid().toString(QUuid::WithoutBraces));
	list->addItem(item);

	EditableListChanged();
}

void WidgetInfo::EditListAddFiles()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	const char *desc = obs_property_description(property);
	const char *filter = obs_property_editable_list_filter(property);
	const char *default_path = obs_property_editable_list_default_path(property);

	QString title = tr("Basic.PropertiesWindow.AddEditableListFiles").arg(QT_UTF8(desc));

	QStringList files = OpenFiles(App()->GetMainWindow(), title, QT_UTF8(default_path), QT_UTF8(filter));
#ifdef __APPLE__
	// TODO: Revisit when QTBUG-42661 is fixed
	widget->window()->raise();
#endif

	if (files.count() == 0)
		return;

	for (QString file : files) {
		QListWidgetItem *item = new QListWidgetItem(file);
		item->setData(Qt::UserRole, QUuid::createUuid().toString(QUuid::WithoutBraces));
		list->addItem(item);
	}
	EditableListChanged();
}

void WidgetInfo::EditListAddDir()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	const char *desc = obs_property_description(property);
	const char *default_path = obs_property_editable_list_default_path(property);

	QString title = tr("Basic.PropertiesWindow.AddEditableListDir").arg(QT_UTF8(desc));

	QString dir = SelectDirectory(list, title, QT_UTF8(default_path));
#ifdef __APPLE__
	// TODO: Revisit when QTBUG-42661 is fixed
	widget->window()->raise();
#endif

	if (dir.isEmpty())
		return;

	QListWidgetItem *item = new QListWidgetItem(dir);
	item->setData(Qt::UserRole, QUuid::createUuid().toString(QUuid::WithoutBraces));
	list->addItem(item);
	EditableListChanged();
}

void WidgetInfo::EditListRemove()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	QList<QListWidgetItem *> items = list->selectedItems();

	for (QListWidgetItem *item : items)
		delete item;
	EditableListChanged();
}

void WidgetInfo::EditListEdit()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	enum obs_editable_list_type type = obs_property_editable_list_type(property);
	const char *desc = obs_property_description(property);
	const char *filter = obs_property_editable_list_filter(property);
	QList<QListWidgetItem *> selectedItems = list->selectedItems();

	if (!selectedItems.count())
		return;

	QListWidgetItem *item = selectedItems[0];

	if (type == OBS_EDITABLE_LIST_TYPE_FILES) {
		QDir pathDir(item->text());
		QString path;

		if (pathDir.exists())
			path = SelectDirectory(list, tr("Browse"), item->text());
		else
			path = OpenFile(list, tr("Browse"), item->text(), QT_UTF8(filter));

#ifdef __APPLE__
		pls_check_app_exiting();
		widget->window()->raise();
#endif
		if (path.isEmpty())
			return;

		item->setText(path);
		EditableListChanged();
		return;
	}

	EditableItemDialog dialog(widget->window(), item->text(), type != OBS_EDITABLE_LIST_TYPE_STRINGS, filter);
	auto title = tr("Basic.PropertiesWindow.EditEditableListEntry").arg(QT_UTF8(desc));
	dialog.setWindowTitle(title);
	if (dialog.exec() == QDialog::Rejected)
		return;

	QString text = dialog.GetText();
	if (text.isEmpty())
		return;

	item->setText(text);
	EditableListChanged();
}

void WidgetInfo::EditListUp()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	int lastItemRow = -1;

	for (int i = 0; i < list->count(); i++) {
		QListWidgetItem *item = list->item(i);
		if (!item->isSelected())
			continue;

		int row = list->row(item);

		if ((row - 1) != lastItemRow) {
			lastItemRow = row - 1;
			list->takeItem(row);
			list->insertItem(lastItemRow, item);
			item->setSelected(true);
		} else {
			lastItemRow = row;
		}
	}

	EditableListChanged();
}

void WidgetInfo::EditListDown()
{
	QListWidget *list = reinterpret_cast<QListWidget *>(widget);
	int lastItemRow = list->count();

	for (int i = list->count() - 1; i >= 0; i--) {
		QListWidgetItem *item = list->item(i);
		if (!item->isSelected())
			continue;

		int row = list->row(item);

		if ((row + 1) != lastItemRow) {
			lastItemRow = row + 1;
			list->takeItem(row);
			list->insertItem(lastItemRow, item);
			item->setSelected(true);
		} else {
			lastItemRow = row;
		}
	}

	EditableListChanged();
}

#pragma mark - prism add mtehod
static bool isSamePropertyType(obs_property_type a, obs_property_type b)
{
	switch ((int)a) {
	case OBS_PROPERTY_BOOL:
		switch ((int)b) {
		case PLS_PROPERTY_BOOL_LEFT:
		case OBS_PROPERTY_BOOL:
			return true;
		default:
			return false;
		}
		break;
	case OBS_PROPERTY_INT:
	case OBS_PROPERTY_FLOAT:
	case OBS_PROPERTY_TEXT:
	case OBS_PROPERTY_PATH:
	case OBS_PROPERTY_LIST:
	case OBS_PROPERTY_COLOR:
	case OBS_PROPERTY_BUTTON:
	case OBS_PROPERTY_FONT:
	case OBS_PROPERTY_EDITABLE_LIST:
	case OBS_PROPERTY_FRAME_RATE:
	case PLS_PROPERTY_FONT_SIMPLE:
	case PLS_PROPERTY_COLOR_CHECKBOX:
	case PLS_PROPERTY_IMAGE_GROUP:
	case PLS_PROPERTY_CUSTOM_GROUP:
		switch ((int)b) {
		case OBS_PROPERTY_INT:
		case OBS_PROPERTY_FLOAT:
		case OBS_PROPERTY_TEXT:
		case OBS_PROPERTY_PATH:
		case OBS_PROPERTY_LIST:
		case OBS_PROPERTY_COLOR:
		case OBS_PROPERTY_BUTTON:
		case OBS_PROPERTY_FONT:
		case OBS_PROPERTY_EDITABLE_LIST:
		case OBS_PROPERTY_FRAME_RATE:
		case PLS_PROPERTY_IMAGE_GROUP:
		case PLS_PROPERTY_FONT_SIMPLE:
		case PLS_PROPERTY_COLOR_CHECKBOX:
		case PLS_PROPERTY_BUTTON_GROUP:
			return true;
		default:
			return false;
		}
	case PLS_PROPERTY_CHAT_TEMPLATE_LIST:
	case PLS_PROPERTY_CHAT_FONT_SIZE:
	case PLS_PROPERTY_TM_TEXT_CONTENT:
	case PLS_PROPERTY_TM_TAB:
	case PLS_PROPERTY_TM_TEMPLATE_TAB:
	case PLS_PROPERTY_TM_TEMPLATE_LIST:
	case PLS_PROPERTY_TM_TEXT:
	case PLS_PROPERTY_TM_COLOR:
	case PLS_PROPERTY_TM_MOTION:
	case PLS_PROPERTY_LINE:
	case PLS_PROPERTY_VIRTUAL_BACKGROUND_CAMERA_STATE:
	case PLS_PROPERTY_VIRTUAL_BACKGROUND_RESOURCE:
	case PLS_PROPERTY_TIPS:
		return false;
	default:
		break;
	}
	return true;
}

void OBSPropertiesView::AddSpacer(const obs_property_type &currentType, QFormLayout *layout)
{
	if (lastPropertyType != OBS_PROPERTY_INVALID) {
		if (PLS_PROPERTY_LINE == (pls_property_type)currentType ||
		    PLS_PROPERTY_LINE == (pls_property_type)lastPropertyType) {
			layout->addItem(new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));
		} else if (PLS_PROPERTY_MOBILE_NAME == (pls_property_type)currentType) {
			layout->addItem(new QSpacerItem(10, 20, QSizePolicy::Fixed, QSizePolicy::Fixed));
		} else {
			if (isSamePropertyType(lastPropertyType, currentType)) {
				layout->addItem(new QSpacerItem(10, common::PROPERTIES_VIEW_VERTICAL_SPACING_MIN,
								QSizePolicy::Fixed, QSizePolicy::Fixed));
			} else {
				layout->addItem(new QSpacerItem(10, common::PROPERTIES_VIEW_VERTICAL_SPACING_MAX,
								QSizePolicy::Fixed, QSizePolicy::Fixed));
			}
		}
	}
	lastPropertyType = currentType;
}

void OBSPropertiesView::updateUIWhenAfterAddProperty(obs_property_t *property, QFormLayout *layout, QLabel *label,
						     QWidget *widget, bool warning)
{
	obs_property_type type = obs_property_get_type(property);
	if (!widget && !label)
		return;

	if (!label && type != OBS_PROPERTY_BOOL && type != OBS_PROPERTY_BUTTON && type != OBS_PROPERTY_GROUP)
		label = new QLabel(QT_UTF8(obs_property_description(property)));

	if (label) {
		if (warning) //TODO: select color based on background color
			label->setStyleSheet("QLabel { color: #C34151; }");

		if (minSize) {
			label->setMinimumWidth(minSize);
			label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		}

		if (!obs_property_enabled(property))
			label->setEnabled(false);

		auto prop_flags = pls_property_get_flags(property);
		bool child = PROPERTY_FLAG_CHILD_CONTROL & prop_flags;
		label->setObjectName(pls_conditional_select(!child, OBJECT_NAME_FORMLABEL, "subLabel"));

		OBSSource source = pls_get_source_by_pointer_address(GetSourceObj());
		if (source) {
			const char *id = obs_source_get_id(source);
			if (pls_is_equal(id, PRISM_SPECTRALIZER_SOURCE_ID) && !child) {
				label->setObjectName("titleLabel");
			} else if (pls_is_equal(id, PRISM_LENS_SOURCE_ID) ||
				   pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID)) {
				label->setText(QTStr(obs_property_description(property)));
				label->setProperty("lensSource", true);
			} else if (pls_is_equal(id, PRISM_CHATV2_SOURCE_ID)) {
				label->setObjectName("tmLabel");
			}
		}

		if (pls_is_in((int)type, (int)PLS_PROPERTY_TM_TEXT, (int)PLS_PROPERTY_TM_COLOR,
			      (int)PLS_PROPERTY_TM_MOTION)) {
			label->setObjectName("tmLabel");
		}
		if ((int)type == (int)PLS_PROPERTY_TM_DEFAULT_TEXT) {
			label->setStyleSheet("QLabel { color: #bababa;}");
		}
	}

	if (!widget)
		return;

	if (!obs_property_enabled(property))
		widget->setEnabled(false);

	QWidget *labelWidget = dynamic_cast<QWidget *>(label);
	if (obs_property_long_description(property) && label) {
		label->setToolTip(obs_property_long_description(property));
		labelWidget = plsCreateHelpQWidget(label, obs_property_long_description(property));
	}
	AddSpacer(obs_property_get_type(property), layout);

	if (obs_data_get_bool(settings, "noLabelHeader") ||
	    PROPERTY_FLAG_NO_LABEL_SINGLE & pls_property_get_flags(property)) {
		layout->addRow(widget);
	} else {
		layout->addRow(labelWidget, widget);
	}

	const char *name = obs_property_name(property);
	if (!lastFocused.empty())
		if (lastFocused.compare(name) == 0)
			lastWidget = widget;
}

void OBSPropertiesView::updateTimerUiClickStatus(bool isClick)
{
	qDebug() << "updateTimerUiClickStatus" << isClick;
	obs_data_set_bool(settings, "is_ui_click_cb", isClick);
}

void OBSPropertiesView::controlChangedToRefresh(obs_property_t *p_, const char *setting_)
{
	if (visUpdateCb && !deferUpdate) {
		auto updateCallBack = [this]() {
			void *obj = GetSourceObj();
			if (obj)
				visUpdateCb(obj, settings);
		};

		auto source = pls_get_source_by_pointer_address(GetSourceObj());
		auto id = obs_source_get_id(source);
		if (pls_is_os_sys_macos() && pls_is_equal(id, AUDIO_INPUT_SOURCE_ID)) {
			updateCallBack();
		} else {
			pls_async_call_mt(this, [this, updateCallBack]() { updateCallBack(); });
		}
	}

	SignalChanged();

	updateTimerUiClickStatus(true);
	if (obs_property_modified(p_, settings)) {
		lastFocused = setting_;
		QMetaObject::invokeMethod(this, "RefreshProperties", Qt::QueuedConnection);
	}
	updateTimerUiClickStatus(false);
	PLS_INFO("main/property", "properties control changed to refresh view");
}

void OBSPropertiesView::showFilterButton(bool hasNoProperties, const char *id)
{
	//add preview button
	auto previewButton = pls_new<QPushButton>(QTStr("Filters"));
	previewButton->setObjectName(common::OBJECT_NMAE_PREVIEW_BUTTON);
	auto spaceLabel = pls_new<QLabel>(this);
	spaceLabel->setObjectName(common::OBJECT_NAME_SPACELABEL);
	pls_connect(previewButton, &QPushButton::clicked, this,
		    [this]() { obs_frontend_open_source_filters(pls_get_source_by_pointer_address(GetSourceObj())); });

	auto formLayout = dynamic_cast<QFormLayout *>(boxLayout);

	if (hasNoProperties) {
		spaceLabel->setFixedSize({10, 22});
		formLayout->addRow(nullptr, spaceLabel);
		formLayout->addRow(previewButton);
	} else {
		if (pls_is_equal(id, common::PRISM_VIEWER_COUNT_SOURCE_ID)) {
			spaceLabel->setFixedSize({10, 20});
		} else if (pls_is_equal(id, common::PRISM_TEXT_TEMPLATE_ID)) {
			spaceLabel->setFixedSize({10, 18});
		} else if (pls_is_equal(id, common::PRISM_LENS_MOBILE_SOURCE_ID) ||
			   pls_is_equal(id, common::PRISM_LENS_SOURCE_ID)) {
			spaceLabel->setFixedSize({10, 40});
		}
#if defined(Q_OS_MACOS)
		else if (pls_is_equal(id, OBS_DSHOW_SOURCE_ID_V2) || pls_is_equal(id, OBS_DSHOW_SOURCE_ID)) {
			spaceLabel->setFixedSize({10, 40});
		}
#endif
		else {
			spaceLabel->setFixedSize({10, 30});
		}

		formLayout->addRow(nullptr, spaceLabel);

		if (pls_is_in_str(id, common::PRISM_CHAT_SOURCE_ID, common::PRISM_TEXT_TEMPLATE_ID,
				  common::BGM_SOURCE_ID, common::PRISM_BACKGROUND_TEMPLATE_SOURCE_ID,
				  common::PRISM_SPECTRALIZER_SOURCE_ID, common::PRISM_TIMER_SOURCE_ID,
				  common::PRISM_VIEWER_COUNT_SOURCE_ID, common::PRISM_CHZZK_SPONSOR_SOURCE_ID) ||
		    obs_data_get_bool(settings, "noLabelHeader")) {
			formLayout->addRow(previewButton);
		} else if (pls_is_equal(id, common::PRISM_CHATV2_SOURCE_ID)) {
			auto layout = pls_new<QHBoxLayout>();
			layout->setSpacing(0);
			layout->setContentsMargins(0, 0, 0, 0);
			layout->setAlignment(Qt::AlignRight);
			m_ctSaveTemplateBtn = pls_new<QPushButton>(tr("ChatTemplate.Save.Tempate.Button"));
			bool btnEnable = obs_data_get_bool(settings, "ctParamChanged");
			m_ctSaveTemplateBtn->setEnabled(btnEnable);
			connect(m_ctSaveTemplateBtn, &QPushButton::clicked, [this]() {
				auto isSuccess = pls_get_chat_template_helper_instance()->saveCustomObj(
					settings, obs_data_get_int(settings, "Chat.Template.List"));
				m_ctSaveTemplateBtn->setEnabled(!isSuccess);
				obs_data_set_bool(settings, "ctParamChanged", false);
			});
			layout->addWidget(m_ctSaveTemplateBtn);
			m_ctSaveTemplateBtn->setObjectName("CtChatSaveButton");
			formLayout->addRow(previewButton, layout);
			m_ctSaveTemplateBtn->setVisible(obs_data_get_int(settings, "Tab") != 0);
		} else {
			formLayout->addRow(nullptr, previewButton);
		}
	}
}

bool OBSPropertiesView::isPrismLensOrMobileSource()
{
	auto source = pls_get_source_by_pointer_address(GetSourceObj());
	auto id = obs_source_get_id(source);
	return pls_is_equal(id, PRISM_LENS_SOURCE_ID) || pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID);
}

int OBSPropertiesView::getPrismLensOutputIndex()
{
	auto source = pls_get_source_by_pointer_address(GetSourceObj());
	OBSDataAutoRelease settings = obs_source_get_settings(source);
	QString videoDeviceId = obs_data_get_string(settings, CSTR_VIDEO_DEVICE_ID);
	return getPrismLensOutputIndex(videoDeviceId);
}

int OBSPropertiesView::getPrismLensOutputIndex(QString name)
{
	int outputCamIndex = -1;

	if (name.contains(CSTR_PRISM_LEN1)) {
		outputCamIndex = 0;
	} else if (name.contains(CSTR_PRISM_LEN2)) {
		outputCamIndex = 1;
	} else if (name.contains(CSTR_PRISM_LEN3)) {
		outputCamIndex = 2;
	}
	return outputCamIndex;
}

void OBSPropertiesView::showLensUninstallTips(bool isMobile)
{
	QString content = QTStr("main.property.lens.install");
	if (isMobile) {
		content = QTStr("main.property.mobile.install");
	}
	pls_show_cam_studio_uninstall(pls_get_toplevel_view(this), QTStr("Alert.Title"), content,
				      QTStr("Main.cam.install.now"), QTStr("main.property.lens.later"));
}

void OBSPropertiesView::textColorChanged(const QByteArray &_id, const QColor &color, QColor::NameFormat format)
{

	std::shared_ptr<WidgetInfo> info = nullptr;
	std::vector<std::shared_ptr<WidgetInfo>>::iterator itr = children.begin();
	while (itr != children.end()) {
		const char *_settingID = obs_property_name((*itr)->property);
		if (_settingID && _settingID[0] && !strcmp(_id.constData(), _settingID)) {

			info = (*itr);
			break;
		}
		++itr;
	}

	if (info == nullptr) {
		return;
	}
	QLabel *label = dynamic_cast<QLabel *>(info->widget);
	if (label == nullptr) {
		return;
	}

	label->setText(color.name(format));
	QPalette palette = QPalette(color);
	label->setPalette(palette);
	label->setStyleSheet(QString("background-color :%1; color: %2;")
				     .arg(palette.color(QPalette::Window).name(format))
				     .arg(palette.color(QPalette::WindowText).name(format)));

	if ((int)obs_property_get_type(info->property) == (int)PLS_PROPERTY_COLOR_CHECKBOX) {
		obs_data_t *color_obj = obs_data_get_obj(settings, _id.constData());
		obs_data_set_int(color_obj, "color_val", color_to_int(color));
		obs_data_release(color_obj);
	} else {
		obs_data_set_int(settings, _id.constData(), color_to_int(color));
	}
	info->ControlChangedToRefresh(_id.constData());
}

void OBSPropertiesView::OnShowScrollBar(bool isShow)
{
	if (!widget) {
		return;
	}
	setContentMarginAndWidth();
}

void OBSPropertiesView::OnOpenPrismLensClicked()
{
	QString program;
	OBSSource source = pls_get_source_by_pointer_address(GetSourceObj());
	const char *id = obs_source_get_id(source);
	bool installed = pls_is_install_cam_studio(program);
	if (!installed) {
		showLensUninstallTips(pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID));
		return;
	}

	QStringList arguments;
	arguments << "--display_control=top";
	int outputCamIndex = getPrismLensOutputIndex();
	if (outputCamIndex != -1) {
		arguments << QString("--output_cam=%1").arg(outputCamIndex);
		if (pls_is_equal(id, PRISM_LENS_MOBILE_SOURCE_ID)) {
			arguments << "--active_tab=mobile";
		}
	}
	pls_open_cam_studio(arguments, this);
#if defined(Q_OS_MACOS)
	pls_set_current_lens(outputCamIndex);
#endif
}

bool OBSPropertiesView::getIsCustomContentMargins(const char *sourceId)
{
	bool isVirtualBackgroundSource = false;
	if (sourceId) {
		if (pls_is_equal(sourceId, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID)) {
			isVirtualBackgroundSource = true;
		}
	} else {
		OBSSource source = pls_get_source_by_pointer_address(GetSourceObj());
		if (source) {
			if (const char *id = obs_source_get_id(source);
			    pls_is_equal(id, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID)) {
				isVirtualBackgroundSource = true;
			}
		}
	}
	return !isVirtualBackgroundSource;
}

void OBSPropertiesView::setContentMarginAndWidth()
{
	auto isPrismMobileSource = false;
	int marginLeft = 10;
	int marginRight = 19;
	OBSSource source = pls_get_source_by_pointer_address(GetSourceObj());
	if (source) {
		if (const char *id = obs_source_get_id(source); pls_is_equal(id, PRISM_MOBILE_SOURCE_ID)) {
			isPrismMobileSource = true;
		}
	} else {
		//The setting left and right margin is zero
		marginLeft = 0;
		marginRight = 0;
	}

	bool isCustomMargin = getIsCustomContentMargins();
	if (pls_is_or(isCustomMargin, setCustomContentWidth)) {
		int scrollWidth = pls_get_visible_width(scroll);

		int widgetMarginRight = qMax(marginRight - scrollWidth, 0);
		if (isCustomMargin) {
			widget->setContentsMargins(marginLeft, 0, widgetMarginRight,
						   pls_conditional_select(isPrismMobileSource, 15,
									  m_bFromSetting ? 0 : 50));
		}

		if (setCustomContentWidth) {
			widget->setMaximumWidth(width() - widgetMarginRight);
		}
	}
}

void WidgetInfo::DeviceChanged(QString text, bool isDshow, bool isMobile)
{
	QString program;
	if (pls_is_install_cam_studio(program)) {
		int index = view->getPrismLensOutputIndex(text);
		if (index != -1) {
			QStringList arguments;
			arguments << "--display_control=keep";
			arguments << QString("--output_cam=%1").arg(index);
			pls_open_cam_studio(arguments, view);
#if defined(Q_OS_MACOS)
			pls_set_current_lens(index);
#endif
		}

		return;
	}
#ifdef Q_OS_MACOS
	if (!isDshow) {
		view->showLensUninstallTips(isMobile);
	}
#endif // Q_OS_MACOS
}

void WidgetInfo::setIsControlChanging(bool isControlChanging_)
{
	view->isControlChanging = isControlChanging_;
}
