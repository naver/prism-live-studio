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

#include "pls-app.hpp"
#include "window-basic-properties.hpp"
#include "window-basic-main.hpp"
#include "qt-wrappers.hpp"
#include "display-helpers.hpp"
#include "properties-view.hpp"
#include "pls-common-define.hpp"
#include "PLSAction.h"
#include "main-view.hpp"
#include "PLSMotionNetwork.h"
#include "PLSMotionFileManager.h"
#include "PLSVirtualBgManager.h"
#include "loading-event.hpp"
#include "PLSGetPropertiesThread.h"

#include <QCloseEvent>
#include <QWindow>
#include <QLayout>
#include <QSpacerItem>
using namespace std;

static void CreateTransitionScene(OBSSource scene, const char *text, uint32_t color);

#define PROPERTY_WINDOW_DEFAULT_W 720
#define PROPERTY_WINDOW_DEFAULT_H 710

// this key is from dshow plugin
#define LAST_RESOLUTION "last_resolution"

namespace {
class CustomPropertiesView : public PLSPropertiesView {
	bool isTransition;
	PLSBasicProperties *basicProperties;

public:
	explicit CustomPropertiesView(PLSBasicProperties *basicProperties_, bool isTransition_, OBSData settings, void *obj, PropertiesReloadCallback reloadCallback, PropertiesUpdateCallback callback,
				      int minSize = 0, int maxSize = -1, bool showFiltersBtn = false, bool showColorFilterPath = true, bool colorFilterOriginalPressed = false,
				      PLSDpiHelper dpiHelper = PLSDpiHelper(), bool reloadPropertyOnInit = true)
		: PLSPropertiesView(settings, obj, reloadCallback, callback, minSize, maxSize, showFiltersBtn, showColorFilterPath, colorFilterOriginalPressed, false, dpiHelper, reloadPropertyOnInit),
		  isTransition(isTransition_),
		  basicProperties(basicProperties_)
	{
	}

	void RefreshProperties()
	{
		PLSPropertiesView::RefreshProperties(
			[this](QWidget *widget) {
				if (isTransition) {
					basicProperties->AddPreviewButton(widget);
				}

				PLSDpiHelper::dpiDynamicUpdate(widget, true);
			},
			false);
	}

	bool isFirstAddSource() const { return basicProperties->isFirstAddSource(); }
};

void obs_source_reset(obs_source_t *source)
{
	obs_data_t *settings = obs_source_get_settings(source);
	obs_data_clear(settings);
	obs_data_release(settings);
}

bool isSourceId(obs_source_t *source, const char *id)
{
	if (!source || !id || !id[0]) {
		return false;
	}

	const char *_id = obs_source_get_id(source);
	if (_id && !strcmp(_id, id)) {
		return true;
	}
	return false;
}

bool isValidBackgroundResource(const QString &itemId)
{
	PLSMotionNetwork *network = PLSMotionNetwork::instance();
	PLSMotionFileManager *fileManager = PLSMotionFileManager::instance();

	MotionData md;
	if (network->findPrismOrFree(md, itemId)) {
		return fileManager->isValidMotionData(md, true);
	} else if (fileManager->findMy(md, itemId)) {
		return fileManager->isValidMotionData(md, true);
	}
	return false;
}
}

void PLSBasicProperties::onReloadOldSettings()
{
	const char *id = obs_source_get_id(source);
	if (!id || !id[0]) {
		return;
	}

	if (!strcmp(id, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID)) {
		const char *item_id = obs_data_get_string(oldSettings, "item_id");
		if (!item_id || !item_id[0] || !isValidBackgroundResource(QString::fromUtf8(item_id))) {
			obs_data_set_bool(oldSettings, "prism_resource", false);
			obs_data_set_string(oldSettings, "item_id", "");
			obs_data_set_int(oldSettings, "item_type", 0);
			obs_data_set_string(oldSettings, "file_path", "");
			obs_data_set_string(oldSettings, "image_file_path", "");
			obs_data_set_string(oldSettings, "thumbnail_file_path", "");
		}
	}
}

quint64 ConvertPointer(void *ptr)
{
	return reinterpret_cast<quint64>(ptr);
}

void PLSBasicProperties::SetOwnerWindow(OBSSource source, long long hwnd)
{
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "method", "owner");
	obs_data_set_int(data, "hwnd", hwnd);
	obs_source_set_private_data(source, data);
	obs_data_release(data);
}

PLSBasicProperties::PLSBasicProperties(QWidget *parent, OBSSource source_, unsigned flags, bool isPrivSetting_, bool autoRenameTitle_, PLSDpiHelper dpiHelper)
	: PLSDialogView(parent, dpiHelper),
	  preview(new PLSQTDisplay(this)),
	  acceptClicked(false),
	  source(source_),
	  removedSignal(obs_source_get_signal_handler(source), "remove", PLSBasicProperties::SourceRemoved, this),
	  renamedSignal(obs_source_get_signal_handler(source), "rename", PLSBasicProperties::SourceRenamed, this),
	  oldSettings(obs_data_create()),
	  operationFlags(flags),
	  isPrivSetting(isPrivSetting_),
	  autoRenameTitle(autoRenameTitle_),
	  obsSourceProperties(isPrivSetting ? obs_source_private_properties : obs_source_properties),
	  obsSourceUpdate(isPrivSetting ? obs_source_private_update : obs_source_update),
	  obsSourceGetSettings(isPrivSetting ? obs_source_get_private_settings : obs_source_get_settings),
	  obsSourceReset(isPrivSetting ? obs_source_private_reset : obs_source_reset)
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSBasicProperties, PLSCssIndex::PLSLoadingBtn});
	dpiHelper.setInitSize(this, {PROPERTY_WINDOW_DEFAULT_W, PROPERTY_WINDOW_DEFAULT_H});

	if (!isPrivSetting) {
		obs_source_properties_edit_start(source);
	}

	openCameraProperties(false);

	signal_handler_connect_ref(obs_source_get_signal_handler(source), "capture_state", PLSQTDisplay::OnSourceCaptureState, preview);

	preview->UpdateSourceState(source);
	preview->AttachSource(source);
	preview->setObjectName(OBJECT_NAME_PROPERTYVIEW);
	preview->setMouseTracking(true);

	enum obs_source_type type = obs_source_get_type(source);
	const char *id = obs_source_get_id(source);
	if (id) {
		PLS_INFO(PROPERTY_MODULE, "Property window for %s is openned", id);
		setProperty("sourceId", id);

		if (!strcmp(id, PRISM_CHAT_SOURCE_ID) || !strcmp(id, PRISM_TEXT_MOTION_ID) || !strcmp(id, PRISM_SPECTRALIZER_SOURCE_ID) || !strcmp(id, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID) ||
		    !strcmp(id, PRISM_MOBILE_SOURCE_ID) || !strcmp(id, PRISM_TIMER_SOURCE_ID)) {
			setWidthResizeEnabled(false);
		}
	}

	uint32_t caps = obs_source_get_output_flags(source);
	bool drawable_type = type == OBS_SOURCE_TYPE_INPUT || type == OBS_SOURCE_TYPE_SCENE;
	bool isVirtualBackgroundSource = id ? (strcmp(id, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID) == 0) : false;
	bool showFilterButton = drawable_type && (!isVirtualBackgroundSource) && (!isPrivSetting);
	bool drawable_preview = (caps & OBS_SOURCE_VIDEO) != 0;

	// use unified default preview height of 210px, window height of 710px
	// For fixing issue #6057
	int marginViewTop = 0;
	bool isCameraSource = false;
	if (id) {
		if (strcmp(id, PRISM_MOBILE_SOURCE_ID) == 0 || strcmp(id, PRISM_CHAT_SOURCE_ID) == 0 || strcmp(id, BGM_SOURCE_ID) == 0) {
			marginViewTop = 10;
		}

		isCameraSource = (0 == strcmp(id, DSHOW_SOURCE_ID));
	}

	buttonBox = new PLSDialogButtonBox(this->content());
	buttonBox->setMouseTracking(true);
	dpiHelper.setContentsMargins(buttonBox, {24, 0, 24, 0});
	buttonBox->setObjectName(QStringLiteral(OBJECT_NAME_BUTTON_BOX));
	buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
	buttonBox->setProperty("owner", "PLSBasicProperties");

	buttonBox->button(QDialogButtonBox::Ok)->setText(QTStr("OK"));
	buttonBox->button(QDialogButtonBox::Cancel)->setText(QTStr("Cancel"));

	buttonBox->button(QDialogButtonBox::RestoreDefaults)->setText(QTStr("Defaults"));

	QMetaObject::connectSlotsByName(this);

	/* The OBSData constructor increments the reference once */
	obs_data_release(oldSettings);

	OBSData settings = obsSourceGetSettings(source);
	obs_data_apply(oldSettings, settings);
	obs_data_release(settings);

	bool isChatSource = id && id[0] ? (strcmp(id, PRISM_CHAT_SOURCE_ID) == 0) : false;
	extern bool hasActivedChatChannel();
	if (isChatSource && obs_frontend_streaming_active()) {
		if (!hasActivedChatChannel()) {
			preview->showGuideText(tr("Chat.Property.ChatSource.NoSupportChannel.InStreaming"));
		}
	}
	view = new CustomPropertiesView(this, type == OBS_SOURCE_TYPE_TRANSITION, settings, source, (PropertiesReloadCallback)obsSourceProperties, (PropertiesUpdateCallback)obsSourceUpdate, 0, -1,
					showFilterButton, true, false, PLSDpiHelper(), !isCameraSource);
	view->SetForProperty(true);
	view->SetCustomContentMargins((!isVirtualBackgroundSource) ? true : false);
	view->setMouseTracking(true);
	// modify by xiewei issue #5324
	view->setCursor(Qt::ArrowCursor);
	view->verticalScrollBar()->setObjectName("PropertiesViewVScrollBar");

	if (0 == strcmp(id, GDIP_TEXT_SOURCE_ID) /*|| 0 == strcmp(id, PRISM_SPECTRALIZER_SOURCE_ID)*/) {
		view->SetCustomContentWidth(true);
	}

	if (isCameraSource) {
		preview->hide();
		QTimer::singleShot(0, this, [this]() { AsyncLoadCameraProperties(); });
	} else {
		view->RefreshProperties();
	}

	if (isVirtualBackgroundSource) {
		view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	}

	connect(view, &PLSPropertiesView::OpenFilters, this, [this]() { emit OpenFilters(source); });
	connect(view, &PLSPropertiesView::OpenStickers, this, [this]() { emit OpenStickers(source); });
	connect(view, &PLSPropertiesView::OpenMusicButtonClicked, this, [=](OBSSource source) { emit OpenMusicButtonClicked(); });
	connect(view, &PLSPropertiesView::okButtonControl, buttonBox->button(QDialogButtonBox::Ok), &QAbstractButton::setEnabled);
	connect(view, &PLSPropertiesView::reloadOldSettings, this, &PLSBasicProperties::onReloadOldSettings);

	this->setMouseTracking(true);
	// Create a QSplitter to keep a unified workflow here.
	windowSplitter = new QSplitter(Qt::Orientation::Vertical, this->content());
	windowSplitter->setMouseTracking(true);
	windowSplitter->setObjectName(OBJECT_NAME_PROPERTY_SPLITTER);
	dpiHelper.setContentsMargins(windowSplitter, {0, 0, 0, 0});

	QWidget *preview_container = new QWidget(this->content());
	preview_container->setFocusPolicy(Qt::NoFocus);
	preview_container->setObjectName("preview_container");
	preview_container->setMouseTracking(true);
	QVBoxLayout *layout_preview = new QVBoxLayout(preview_container);
	layout_preview->setContentsMargins(0, 0, 0, 0);
	layout_preview->setSpacing(0);
	layout_preview->addWidget(preview);
	windowSplitter->addWidget(preview_container);

	QWidget *view_contianter = new QWidget(this->content());
	view_contianter->setFocusPolicy(Qt::NoFocus);
	view_contianter->setObjectName(OBJECT_NAME_PROPERTY_VIEW_CONTAINER);
	view_contianter->setMouseTracking(true);
	// commented by xiewei issue #5324
	//view_contianter->setCursor(Qt::ArrowCursor);
	QVBoxLayout *layout_view = new QVBoxLayout(view_contianter);
	layout_view->setContentsMargins(15, marginViewTop, 6, 0);
	layout_view->setSpacing(0);
	layout_view->addWidget(view);
	windowSplitter->addWidget(view_contianter);
	windowSplitter->setStretchFactor(1, 1);
	windowSplitter->setChildrenCollapsible(false);
	dpiHelper.notifyDpiChanged(windowSplitter, [=](double dpi) {
		windowSplitter->setSizes(QList<int>({PLSDpiHelper::calculate(dpi, DISPLAY_VIEW_DEFAULT_HEIGHT), PLSDpiHelper::calculate(dpi, 368)}));
	});

	//fixed the preview height with timer clock source
	/*if (0 == strcmp(id, PRISM_TIMER_SOURCE_ID)) {
		QSplitterHandle *splitterHandle = windowSplitter->handle(1);
		if (splitterHandle) {
			splitterHandle->setDisabled(true);
		}
		dpiHelper.setMinimumHeight(view, 90);
		dpiHelper.setFixedHeight(preview, 210);
		preview->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
	} else {*/
	windowSplitter->setStretchFactor(0, 1);
	dpiHelper.setMinimumHeight(view, 150);
	dpiHelper.setMinimumSize(preview, {20, 150});
	preview->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));

	QLabel *seperatorLabel = new QLabel(this);
	seperatorLabel->setObjectName(OBJECT_NAME_SEPERATOR_LABEL);

	QHBoxLayout *hLayoutSplitter = new QHBoxLayout;
	hLayoutSplitter->setContentsMargins(0, 0, 0, 0);
	hLayoutSplitter->addWidget(windowSplitter);

	QHBoxLayout *hLayoutBtnBox = new QHBoxLayout;
	hLayoutBtnBox->setContentsMargins(ORIGINAL_RESIZE_BORDER_WIDTH, ORIGINAL_RESIZE_BORDER_WIDTH, ORIGINAL_RESIZE_BORDER_WIDTH, ORIGINAL_RESIZE_BORDER_WIDTH);
	hLayoutBtnBox->addWidget(buttonBox);

	QVBoxLayout *hLayout = new QVBoxLayout(this->content());
	hLayout->setContentsMargins(0, 0, 0, 0);
	hLayout->setSpacing(0);
	hLayout->addLayout(hLayoutSplitter);

	// if (type == OBS_SOURCE_TYPE_TRANSITION) {
	// 	AddPreviewButton();
	// 	connect(view, SIGNAL(PropertiesRefreshed()), this, SLOT(AddPreviewButton()));
	// }

	hLayout->addWidget(seperatorLabel);
	hLayout->addLayout(hLayoutBtnBox);
	hLayout->setAlignment(buttonBox, Qt::AlignBottom);

	//view->show();
	installEventFilter(CreateShortcutFilter());

	const char *name = obs_source_get_name(source);
	setWindowTitle(QTStr("Basic.PropertiesWindow").arg(QT_UTF8(name)));

	obs_source_inc_showing(source);

	if (!isPrivSetting) {
		updatePropertiesSignal.Connect(obs_source_get_signal_handler(source), "update_properties", PLSBasicProperties::UpdateProperties, this);
		updatePropertiesNotifySignal.Connect(obs_source_get_signal_handler(source), "property_update_notify", PLSBasicProperties::PropertyUpdateNotify, this);
		updatePropertiesOKButtonSignal.Connect(obs_source_get_signal_handler(source), "update_properties_ok_button_enable", PLSBasicProperties::UpdatePropertiesOkButtonEnable, this);
	}

	auto addDrawCallback = [this]() { obs_display_add_draw_callback(preview->GetDisplay(), PLSBasicProperties::DrawPreview, this); };
	auto addTransitionDrawCallback = [this]() { obs_display_add_draw_callback(preview->GetDisplay(), PLSBasicProperties::DrawTransitionPreview, this); };

	if (drawable_preview && drawable_type) {
		if (!isCameraSource)
			preview->show();
		connect(preview.data(), &PLSQTDisplay::DisplayCreated, addDrawCallback);

	} else if (type == OBS_SOURCE_TYPE_TRANSITION) {
		sourceA = obs_source_create_private("scene", "sourceA", nullptr);
		sourceB = obs_source_create_private("scene", "sourceB", nullptr);

		obs_source_release(sourceA);
		obs_source_release(sourceB);

		uint32_t colorA = 0xFFB26F52;
		uint32_t colorB = 0xFF6FB252;

		CreateTransitionScene(sourceA, "A", colorA);
		CreateTransitionScene(sourceB, "B", colorB);

		/**
		 * The cloned source is made from scratch, rather than using
		 * obs_source_duplicate, as the stinger transition would not
		 * play correctly otherwise.
		 */

		obs_data_t *settings = obs_source_get_settings(source);

		sourceClone = obs_source_create_private(obs_source_get_id(source), "clone", settings);
		obs_source_release(sourceClone);

		obs_source_inc_active(sourceClone);
		obs_transition_set(sourceClone, sourceA);

		obs_data_release(settings);

		auto updateCallback = [=]() {
			obs_data_t *settings = obsSourceGetSettings(source);
			obsSourceUpdate(sourceClone, settings);

			obs_transition_clear(sourceClone);
			obs_transition_set(sourceClone, sourceA);
			obs_transition_force_stop(sourceClone);

			obs_data_release(settings);

			direction = true;
		};

		connect(view, &PLSPropertiesView::Changed, updateCallback);

		preview->show();
		connect(preview.data(), &PLSQTDisplay::DisplayCreated, addTransitionDrawCallback);

	} else {
		preview_container->hide();
		preview->setProperty("forceHidden", true);
		preview->hide();
		layout_view->setContentsMargins(15, 25, 15, 0);
	}

	SetOwnerWindow(source, window()->winId());
}

PLSBasicProperties::~PLSBasicProperties()
{
	HideLoading();
	SetOwnerWindow(source, 0);

	Cleanup();

	signal_handler_disconnect(obs_source_get_signal_handler(source), "capture_state", PLSQTDisplay::OnSourceCaptureState, preview);
	if (sourceClone) {
		obs_source_dec_active(sourceClone);
	}
	obs_source_dec_showing(source);
	PLSBasic *main = PLSBasic::Get();
	if (main)
		main->SaveProject();

	view->CheckValues();

	if (acceptClicked) {
		OBSData srcSettings = obsSourceGetSettings(source);
		obs_data_release(srcSettings);
	}

	const char *id = obs_source_get_id(source);
	if (id)
		PLS_INFO(PROPERTY_MODULE, "Property window for %s is closed", id);
}

void PLSBasicProperties::AddPreviewButton(QWidget *widget)
{
	QPushButton *playButton = new QPushButton(QTStr("PreviewTransition"), this);
	playButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	playButton->setObjectName("playButton");
	QSpacerItem *item = new QSpacerItem(1, PROPERTIES_VIEW_VERTICAL_SPACING_MAX, QSizePolicy::Fixed);
	widget->layout()->addItem(item);
	widget->layout()->addWidget(playButton);

	PLSDpiHelper dpiHelper;
	dpiHelper.setFixedHeight(playButton, FILTERS_TRANSITION_VIEW_FIXED_HEIGHT);
	dpiHelper.setMinimumWidth(playButton, FILTERS_TRANSITION_VIEW_FIXED_WIDTH);
	auto play = [=]() {
		OBSSource start;
		OBSSource end;

		if (direction) {
			start = sourceA;
			end = sourceB;
		} else {
			start = sourceB;
			end = sourceA;
		}

		PLS_UI_STEP(PROPERTY_MODULE, "PlayButton", ACTION_CLICK);
		obs_transition_set(sourceClone, start);
		PLSBasic *main = PLSBasic::Get();
		if (main)
			obs_transition_start(sourceClone, OBS_TRANSITION_MODE_AUTO, main->GetTransitionDuration(), end);
		direction = !direction;

		start = nullptr;
		end = nullptr;
	};

	connect(playButton, &QPushButton::clicked, play);
}

void PLSBasicProperties::UpdateOldSettings(obs_source_t *source)
{
	if (!source) {
		return;
	}
	obs_data_t *settings = obsSourceGetSettings(source);
	obs_data_apply(oldSettings, settings);
}

bool PLSBasicProperties::isFirstAddSource() const
{
	return (operationFlags & OPERATION_ADD_SOURCE) ? true : false;
}

void PLSBasicProperties::closeNoCancelButton()
{
	questionHasCancelButton = false;
	close();
}

static obs_source_t *CreateLabel(const char *name, size_t h)
{
	obs_data_t *settings = obs_data_create();
	obs_data_t *font = obs_data_create();

	std::string text;
	text += " ";
	text += name;
	text += " ";

#if defined(_WIN32)
	obs_data_set_string(font, "face", "Arial");
#elif defined(__APPLE__)
	obs_data_set_string(font, "face", "Helvetica");
#else
	obs_data_set_string(font, "face", "Monospace");
#endif
	obs_data_set_int(font, "flags", 1); // Bold text
	obs_data_set_int(font, "size", min(int(h), 300));

	obs_data_set_obj(settings, "font", font);
	obs_data_set_string(settings, "text", text.c_str());
	obs_data_set_bool(settings, "outline", false);

#ifdef _WIN32
	const char *text_source_id = "text_gdiplus";
#else
	const char *text_source_id = "text_ft2_source";
#endif

	obs_source_t *txtSource = obs_source_create_private(text_source_id, name, settings);

	obs_data_release(font);
	obs_data_release(settings);

	return txtSource;
}

static void CreateTransitionScene(OBSSource scene, const char *text, uint32_t color)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "width", obs_source_get_width(scene));
	obs_data_set_int(settings, "height", obs_source_get_height(scene));
	obs_data_set_int(settings, "color", color);

	obs_source_t *colorBG = obs_source_create_private("color_source", "background", settings);

	obs_scene_add(obs_scene_from_source(scene), colorBG);

	obs_source_t *label = CreateLabel(text, obs_source_get_height(scene));
	obs_sceneitem_t *item = obs_scene_add(obs_scene_from_source(scene), label);

	vec2 size;
	vec2_set(&size, obs_source_get_width(scene),
#ifdef _WIN32
		 obs_source_get_height(scene));
#else
		 obs_source_get_height(scene) * 0.8);
#endif

	obs_sceneitem_set_bounds(item, &size);
	obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);

	obs_data_release(settings);
	obs_source_release(colorBG);
	obs_source_release(label);
}

void PLSBasicProperties::SourceRemoved(void *, calldata_t *params)
{
	// #6377
	//QMetaObject::invokeMethod(static_cast<PLSBasicProperties *>(data), "close");

	UNUSED_PARAMETER(params);
}

void PLSBasicProperties::SourceRenamed(void *data, calldata_t *params)
{
	if (PLSBasicProperties *properties = static_cast<PLSBasicProperties *>(data); properties->autoRenameTitle) {
		const char *name = calldata_string(params, "new_name");
		QString title = QTStr("Basic.PropertiesWindow").arg(QT_UTF8(name));
		QMetaObject::invokeMethod(properties, "setWindowTitle", Q_ARG(QString, title));
	}
}

void PLSBasicProperties::UpdateProperties(void *data, calldata_t *)
{
	QMetaObject::invokeMethod(static_cast<PLSBasicProperties *>(data)->view, "ReloadProperties", Qt::QueuedConnection);
}

void PLSBasicProperties::PropertyUpdateNotify(void *data, calldata_t *params)
{
	const char *cname = nullptr;
	calldata_get_ptr(params, "name", (void *)&cname);
	QString name = cname && cname[0] ? QString::fromUtf8(cname) : QString();
	QMetaObject::invokeMethod(static_cast<PLSBasicProperties *>(data)->view, "PropertyUpdateNotify", Qt::QueuedConnection, Q_ARG(QString, name));
}

void PLSBasicProperties::UpdatePropertiesOkButtonEnable(void *data, calldata_t *params)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(params, "source");
	if (!source) {
		return;
	}
	bool enable = calldata_bool(params, "enable");
	QMetaObject::invokeMethod(static_cast<PLSBasicProperties *>(data)->view, "okButtonControl", Qt::QueuedConnection, Q_ARG(bool, enable));
}

void PLSBasicProperties::on_buttonBox_clicked(QAbstractButton *button)
{
	QDialogButtonBox::ButtonRole val = buttonBox->buttonRole(button);
	if (val == QDialogButtonBox::AcceptRole) {
		closeCameraProperties(true);
		m_isSaveClick = true; //RenJinbo/add is save button clicked
		acceptClicked = true;
		close();

		if (view->DeferUpdate())
			view->UpdateSettings();

	} else if (val == QDialogButtonBox::RejectRole) {
		OnButtonBoxCancelClicked(source);

	} else if (val == QDialogButtonBox::ResetRole) {
		const char *id = obs_source_get_id(source);
		//add by xiewei, gipy sticker source dosen't provide reset operation.
		if (id && 0 == strcmp(id, PRISM_GIPHY_STICKER_SOURCE_ID))
			return;

		//add by zengqin, ffmepg source disable defaults when media is loading.
		if (id && 0 == strcmp(id, MEDIA_SOURCE_ID)) {
			obs_data_t *loadData = obs_data_create();
			obs_data_set_string(loadData, "method", "media_load");
			obs_source_get_private_data(source, loadData);
			bool loading = obs_data_get_bool(loadData, "media_load");
			obs_data_release(loadData);
			if (loading)
				return;
		}

		if (isPrivSetting && id && !strcmp(id, DSHOW_SOURCE_ID)) {
			bool ckCapture = obs_data_get_bool(view->GetSettings(), "ck_capture");
			obsSourceReset(source);
			obs_data_t *privateSettings = obs_source_get_private_settings(source);
			obs_data_set_bool(privateSettings, "ck_capture", ckCapture);
			obs_data_release(privateSettings);
		} else {
			obsSourceReset(source);
		}

		//add by zengqin, clear spectralizer config
		if (id && (0 == strcmp(id, PRISM_SPECTRALIZER_SOURCE_ID) || 0 == strcmp(id, PRISM_TIMER_SOURCE_ID))) {
			obs_data_t *data = obs_data_create();
			obs_data_set_string(data, "method", "default_properties");
			obs_source_set_private_data(source, data);
			obs_data_release(data);
		}

		if (!view->DeferUpdate())
			obsSourceUpdate(source, nullptr);

		if (!isPrivSetting && 0 != strcmp(id, PRISM_TIMER_SOURCE_ID)) {
			obs_source_properties_edit_start(source);
		}

		view->ReloadProperties();
	}
}

void PLSBasicProperties::OnButtonBoxCancelClicked(OBSSource source)
{
	m_isSaveClick = false; //RenJinbo/add is save button clicked
	const char *pluginID = obs_source_get_id(source);
	if (pluginID && strcmp(pluginID, DSHOW_SOURCE_ID) == 0) {
		OBSData currentSettings = obsSourceGetSettings(source);
		const char *oldSettingsJson = obs_data_get_json(oldSettings);
		const char *currentSettingsJson = obs_data_get_json(currentSettings);
		int ret = strcmp(currentSettingsJson, oldSettingsJson);
		obs_data_release(currentSettings);
		if (ret == 0) {
			close();
			return;
		}
	}

	obsSourceReset(source);

	if (view->DeferUpdate())
		obs_data_apply(obsSourceGetSettings(source), oldSettings);
	else
		obsSourceUpdate(source, oldSettings);

	close();
}

void PLSBasicProperties::ReloadProperties()
{
	if (!view) {
		return;
	}

	view->ReloadProperties();
}

static inline void GetChatScaleAndCenterPos(double dpi, int baseCX, int baseCY, int windowCX, int windowCY, int &x, int &y, float &scale)
{
	float WIDTH = 365 * dpi;

	int newCX = int(WIDTH);
	int newCY = int(float(WIDTH * baseCY) / float(baseCX));
	scale = newCX / float(baseCX);
	x = windowCX / 2 - newCX / 2;
	y = windowCY - newCY;
}

static inline void GetSpectralizerScaleAndCenterPos(double dpi, int baseCX, int baseCY, int windowCX, int windowCY, bool mirror, int &x, int &y, int &newCX, int &newCY)
{
	baseCX = baseCX * dpi;
	baseCY = baseCY * dpi;
	int windowCX_ = windowCX - (40 * dpi);
	int windowCY_ = windowCY - (40 * dpi);

	double windowAspect = double(windowCX_) / double(windowCY_);
	double baseAspect = double(baseCX) / double(baseCY);
	float scale_x = float(windowCX_) / float(baseCX);
	float scale_y = float(windowCY_) / float(baseCY);

	if (windowAspect > baseAspect) {
		newCY = scale_y > 1 ? baseCY : (scale_y * baseCY);
		newCX = scale_x > 1 ? baseCX : (scale_x * baseCX);

		x = (windowCX - newCX) / 2;
		y = mirror ? (windowCY - newCY) / 2 : windowCY - newCY;

	} else {
		newCX = scale_x > 1 ? baseCX : (scale_x * baseCX);
		newCY = scale_y > 1 ? baseCY : (scale_y * baseCY);

		x = windowCX / 2 - newCX / 2;
		y = mirror ? (windowCY - newCY) / 2 : windowCY - newCY;
	}
}

void PLSBasicProperties::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	PLSBasicProperties *window = static_cast<PLSBasicProperties *>(data);

	if (!window->source)
		return;

	uint32_t sourceCX = max(obs_source_get_width(window->source), 1u);
	uint32_t sourceCY = max(obs_source_get_height(window->source), 1u);
	const char *sourceId = obs_source_get_id(window->source);

	if (!window->isPrivSetting && (sourceCX != 1) && (sourceCY != 1)) {
		if ((window->originalCameraResolution.width() == 1) || (window->originalCameraResolution.height() == 1)) {
			window->originalCameraResolution = window->oldCameraResolution = window->currentCameraResolution = QSize(sourceCX, sourceCY);
			window->openCameraProperties(true);
		} else if ((sourceCX != uint32_t(window->currentCameraResolution.width())) || (sourceCY != uint32_t(window->currentCameraResolution.height()))) {
			window->oldCameraResolution = window->currentCameraResolution;
			window->currentCameraResolution = QSize(sourceCX, sourceCY);
			window->cameraPropertiesResolutionChanged();
		}
	}

	int x, y;
	int newCX, newCY;
	float scale;

	if (sourceId && sourceId[0] && !strcmp(sourceId, PRISM_CHAT_SOURCE_ID)) {
		GetChatScaleAndCenterPos(window->dpi, sourceCX, sourceCY, cx, cy, x, y, scale);
	} else {
		GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);
	}

	if (sourceId && sourceId[0] && !strcmp(sourceId, PRISM_SPECTRALIZER_SOURCE_ID)) {
		bool mirror = obs_data_get_bool(pls_get_source_setting(window->source), "stereo");
		GetSpectralizerScaleAndCenterPos(window->dpi, sourceCX, sourceCY, cx, cy, mirror, x, y, newCX, newCY);
	} else {
		newCX = int(scale * float(sourceCX));
		newCY = int(scale * float(sourceCY));
	}

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);

	obs_source_video_render(window->source);

	gs_projection_pop();
	gs_viewport_pop();
}

void PLSBasicProperties::DrawTransitionPreview(void *data, uint32_t cx, uint32_t cy)
{
	PLSBasicProperties *window = static_cast<PLSBasicProperties *>(data);

	if (!window->sourceClone)
		return;

	uint32_t sourceCX = max(obs_source_get_width(window->sourceClone), 1u);
	uint32_t sourceCY = max(obs_source_get_height(window->sourceClone), 1u);

	int x, y;
	int newCX, newCY;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	newCX = int(scale * float(sourceCX));
	newCY = int(scale * float(sourceCY));

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);

	obs_source_video_render(window->sourceClone);

	gs_projection_pop();
	gs_viewport_pop();
}

void PLSBasicProperties::Cleanup()
{
	config_set_int(App()->GlobalConfig(), "PropertiesWindow", "cx", width());
	config_set_int(App()->GlobalConfig(), "PropertiesWindow", "cy", height());

	obs_display_remove_draw_callback(preview->GetDisplay(), PLSBasicProperties::DrawPreview, this);
	obs_display_remove_draw_callback(preview->GetDisplay(), PLSBasicProperties::DrawTransitionPreview, this);
}

void PLSBasicProperties::openCameraProperties(bool defer)
{
	if (!isPrivSetting && isSourceId(source, DSHOW_SOURCE_ID)) {
		if (!defer) {
			originalCameraResolution = oldCameraResolution = currentCameraResolution = QSize(max(obs_source_get_width(source), 1u), max(obs_source_get_height(source), 1u));
			if ((originalCameraResolution.width() != 1) && (originalCameraResolution.height() != 1)) {
				PLSBasic::Get()->openCameraProperties(source, originalCameraResolution);
			}
		} else {
			auto basic = PLSBasic::Get();
			QMetaObject::invokeMethod(
				basic,
				[basic, source = (obs_source_t *)source, originalCameraResolution = originalCameraResolution]() { basic->openCameraProperties(source, originalCameraResolution); },
				Qt::QueuedConnection);
		}
	}
}

void PLSBasicProperties::closeCameraProperties(bool ok /*ok or cancel*/)
{
	if (!isPrivSetting && isSourceId(source, DSHOW_SOURCE_ID)) {
		PLSBasic::Get()->closeCameraProperties(source, ok, originalCameraResolution, currentCameraResolution);
		if (ok && ((originalCameraResolution != currentCameraResolution) || view->isResolutionChanged())) {
			obs_data_t *privateSettings = obs_source_get_private_settings(source);
			PLSVrBgModel bgModel = static_cast<PLSVrBgModel>(obs_data_get_int(privateSettings, "background_select_type"));
			obs_data_release(privateSettings);

			if (bgModel == PLSVrBgModel::OtherResource) {
				// select image resource
				PLSAlertView::information(this, tr("Alert.Title"), tr("virtual.camera.resolution.change.reset.image"), PLSAlertView::Button::Ok, PLSAlertView::Button::Ok);
			}
		}
	}
}

void PLSBasicProperties::cameraPropertiesResolutionChanged()
{
	if (!isPrivSetting && isSourceId(source, DSHOW_SOURCE_ID)) {
		auto basic = PLSBasic::Get();
		QMetaObject::invokeMethod(
			basic,
			[basic, source = (obs_source_t *)source, oldResolution = oldCameraResolution, currentResolution = currentCameraResolution]() {
				basic->cameraPropertiesResolutionChanged(source, currentResolution, oldResolution);
			},
			Qt::QueuedConnection);
	}
}

void PLSBasicProperties::ShowLoading()
{
	HideLoading();
	m_pWidgetLoadingBG = new QWidget(content());
	m_pWidgetLoadingBG->setObjectName("loadingBG");
	m_pWidgetLoadingBG->setGeometry(content()->geometry());
	m_pWidgetLoadingBG->show();

	m_pLoadingEvent = new PLSLoadingEvent();

	auto layout = new QVBoxLayout(m_pWidgetLoadingBG);
	auto loadingBtn = new QPushButton(m_pWidgetLoadingBG);
	auto tips = new QLabel(tr("main.camera.loading.devicelist"));
	layout->addStretch();
	layout->addWidget(loadingBtn, 0, Qt::AlignHCenter);
	layout->addWidget(tips, 0, Qt::AlignHCenter);
	layout->addStretch();
	loadingBtn->setObjectName("loadingBtn");
	loadingBtn->show();

	m_pLoadingEvent->startLoadingTimer(loadingBtn);
}

void PLSBasicProperties::HideLoading()
{
	if (nullptr != m_pLoadingEvent) {
		m_pLoadingEvent->stopLoadingTimer();
		delete m_pLoadingEvent;
		m_pLoadingEvent = nullptr;
	}

	if (nullptr != m_pWidgetLoadingBG) {
		m_pWidgetLoadingBG->hide();
		delete m_pWidgetLoadingBG;
		m_pWidgetLoadingBG = nullptr;
	}
}

void PLSBasicProperties::AsyncLoadCameraProperties()
{
	ShowLoading();
	connect(
		PLSGetPropertiesThread::Instance(), &PLSGetPropertiesThread::OnProperties, this,
		[this](PropertiesParam_t param) {
			if (param.id == ConvertPointer(this) && param.properties) {
				HideLoading();
				if (view) {
					view->ResetProperties(param.properties);
					QTimer::singleShot(0, this, [this]() {
						if (view)
							view->RefreshProperties();
						if (preview)
							preview->show();
					});
				}
			}
		},
		Qt::QueuedConnection);
	PLSGetPropertiesThread::Instance()->GetPropertiesBySource(source, ConvertPointer(this));
}

void PLSBasicProperties::reject()
{
	dialogClosedToSendNoti();
}
void PLSBasicProperties::accept()
{
	dialogClosedToSendNoti();
}

void PLSBasicProperties::dialogClosedToSendNoti()
{
	PLS_INFO(PROPERTY_MODULE, "Property window will close to send notifications");
	if (!acceptClicked && (CheckSettings() != 0)) {
		if (!ConfirmQuit()) {
			return;
		}
	} else if (!acceptClicked) {
		closeCameraProperties(false);
	}

	if (!isPrivSetting) {
		//RenJinbo/add is save button clicked
		obs_source_properties_edit_end(source, m_isSaveClick);
	}

	Cleanup();
	emit AboutToClose();
	done(0);
}

void PLSBasicProperties::showEvent(QShowEvent *event)
{
	__super::showEvent(event);
	activateWindow();
}

void PLSBasicProperties::closeEvent(QCloseEvent *event)
{
	if (!acceptClicked && (CheckSettings() != 0)) {
		if (!ConfirmQuit()) {
			event->ignore();
			return;
		}
	}
	QDialog::closeEvent(event);
	if (!event->isAccepted())
		return;

	Cleanup();
}

bool PLSBasicProperties::eventFilter(QObject *watcher, QEvent *event)
{
	if (watcher == content()) {
		switch (event->type()) {
		case QEvent::ChildAdded:
			// to avoid setting cursor in father
			return false;
		}
	}
	return PLSDialogView::eventFilter(watcher, event);
}

void PLSBasicProperties::Init()
{
	show();
}

OBSSource PLSBasicProperties::GetSource()
{
	return source;
}

int PLSBasicProperties::CheckSettings()
{
	obs_data_t *temp = NULL;
	OBSData currentSettings = obsSourceGetSettings(source);
	const char *oldSettingsJson = obs_data_get_json(oldSettings);
	const char *currentSettingsJson = obs_data_get_json(currentSettings);

	const char *pluginID = obs_source_get_id(source);
	if (pluginID && strcmp(pluginID, DSHOW_SOURCE_ID) == 0) {
		const char *oldRes = obs_data_get_string(oldSettings, LAST_RESOLUTION);
		const char *newRes = obs_data_get_string(currentSettings, LAST_RESOLUTION);
		if (oldRes && newRes && 0 != strcmp(oldRes, newRes)) {
			temp = obs_data_create();
			obs_data_apply(temp, oldSettings);
			obs_data_set_string(temp, LAST_RESOLUTION, newRes);
			oldSettingsJson = obs_data_get_json(temp);
		}
	}

	int ret = 0;
	if (currentSettingsJson && oldSettingsJson) {
		ret = strcmp(currentSettingsJson, oldSettingsJson);
	}

	obs_data_release(currentSettings);
	if (temp)
		obs_data_release(temp);

	return ret;
}

bool PLSBasicProperties::ConfirmQuit()
{
	PLSAlertView::Button button =
		PLSMessageBox::question(this, QTStr("Basic.PropertiesWindow.ConfirmTitle"), QTStr("Basic.PropertiesWindow.Confirm"),
					PLSAlertView::Button::Save | PLSAlertView::Button::Discard | (questionHasCancelButton ? PLSAlertView::Button::Cancel : PLSAlertView::Button::NoButton));

	switch (button) {
	case PLSAlertView::Button::Save:
		acceptClicked = true;
		m_isSaveClick = true; //RenJinbo/add is save button clicked
		if (view->DeferUpdate())
			view->UpdateSettings();
		// Do nothing because the settings are already updated
		closeCameraProperties(true);
		break;
	case PLSAlertView::Button::Discard:
		acceptClicked = true;  //#4341 by zengqin
		m_isSaveClick = false; //RenJinbo/add is save button clicked
		obsSourceReset(source);
		obsSourceUpdate(source, oldSettings);
		closeCameraProperties(false);
		break;
	case PLSAlertView::Button::NoButton: //#4186 by zengqin
	case PLSAlertView::Button::Cancel:
		return false;
		break;
	default:
		/* If somehow the dialog fails to show, just default to
		 * saving the settings. */
		break;
	}
	return true;
}
