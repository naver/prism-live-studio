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

#include "window-basic-main.hpp"
#include "window-basic-source-select.hpp"
#include "qt-wrappers.hpp"
#include "pls-app.hpp"
#include "PLSSceneDataMgr.h"
#include "action.h"
#include "liblog.h"
#include "log/module_names.h"
#include "pls-common-define.hpp"

#define SPECTRALIZER_MIN_WIDTH 210

struct AddSourceData {
	obs_source_t *source;
	bool visible;

	//zhangdewen, chat source, set source position
	bool setpos;

	//zengqin, audiovisualizer source, set source position and size
	obs_transform_info itemInfo{};

	AddSourceData(obs_source_t *source, bool visible)
	{
		this->source = source;
		this->visible = visible;
		this->setpos = false;

		vec2_set(&itemInfo.pos, 0.0f, 0.0f);
		vec2_set(&itemInfo.scale, 1.0f, 1.0f);
		itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
		itemInfo.rot = 0.0f;

		itemInfo.bounds_type = OBS_BOUNDS_NONE;
		itemInfo.bounds_alignment = OBS_ALIGN_CENTER;
		vec2_set(&itemInfo.bounds, 0.0f, 0.0f);
	}

	void setAlignment(uint32_t value) { itemInfo.alignment = value; }

	void centerShow(int width = 0, int height = 0)
	{
		setpos = true;

		obs_video_info ovi;
		vec3 center;
		obs_get_video_info(&ovi);
		vec3_set(&center, float(ovi.base_width), float(ovi.base_height), 0.0f);
		vec3_mulf(&center, &center, 0.5f);

		PLSBasic *main = PLSBasic::Get();
		int x, y, cx, cy;
		main->GetDisplayRect(x, y, cx, cy);
		vec2 baseSize;
		baseSize.x = obs_source_get_width(source);
		baseSize.y = obs_source_get_height(source);

		vec2 size;
		if (width && height) {
			size.x = cx ? width / (float)cx * ovi.base_width : baseSize.x;
			size.y = cy ? height / (float)cy * ovi.base_height : baseSize.y;
		} else if (width && !height) {
			size.x = cx ? width / (float)cx * ovi.base_width : baseSize.x;
			size.y = baseSize.x ? baseSize.y / (float)baseSize.x * size.x : baseSize.y;
		} else if (!width && height) {
			size.y = cy ? height / (float)cy * ovi.base_height : baseSize.y;
			size.x = baseSize.y ? baseSize.x / (float)baseSize.y * size.y : baseSize.x;

		} else {
			size.x = baseSize.x;
			size.y = baseSize.y;
		}

		float posX = center.x - size.x / 2.0f;
		float posY = center.y - size.y / 2.0f;
		if (OBS_ALIGN_CENTER == itemInfo.alignment) {
			//PRISM/WuLongyue/20201222/#6214/For PRISM Mobile source: Use center point to rotate
			/** Compatible with other source
			* Either here or obs-scene.cpp::obs_scene_add_internal
			* can make PRISM Mobile source rotate on center point
			* FEEL FREE to remove one of them in the future
			*/
			posX = center.x;
			posY = center.y;
		}

		vec2_div(&size, &size, &baseSize);

		vec2_set(&itemInfo.pos, posX, posY);
		if (baseSize.x > 0 && baseSize.y > 0) {
			vec2_set(&itemInfo.scale, size.x, size.y);
		}
	}
};

bool PLSBasicSourceSelect::EnumSources(void *data, obs_source_t *source)
{
	PLSBasicSourceSelect *window = static_cast<PLSBasicSourceSelect *>(data);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_id(source);

	if (strcmp(id, window->id) == 0)
		window->ui->sourceList->addItem(name);

	return true;
}

bool PLSBasicSourceSelect::EnumGroups(void *data, obs_source_t *source)
{
	PLSBasicSourceSelect *window = static_cast<PLSBasicSourceSelect *>(data);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_id(source);

	if (strcmp(id, window->id) == 0) {
		PLSBasic *main = reinterpret_cast<PLSBasic *>(App()->GetMainWindow());
		OBSScene scene = main->GetCurrentScene();

		obs_sceneitem_t *existing = obs_scene_get_group(scene, name);
		if (!existing)
			window->ui->sourceList->addItem(name);
	}

	return true;
}

void PLSBasicSourceSelect::OBSSourceAdded(void *data, calldata_t *calldata)
{
	PLSBasicSourceSelect *window = static_cast<PLSBasicSourceSelect *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(calldata, "source");

	QMetaObject::invokeMethod(window, "SourceAdded", Q_ARG(OBSSource, source));
}

void PLSBasicSourceSelect::OBSSourceRemoved(void *data, calldata_t *calldata)
{
	PLSBasicSourceSelect *window = static_cast<PLSBasicSourceSelect *>(data);
	obs_source_t *source = (obs_source_t *)calldata_ptr(calldata, "source");

	QMetaObject::invokeMethod(window, "SourceRemoved", Q_ARG(OBSSource, source));
}

void PLSBasicSourceSelect::SourceAdded(OBSSource source)
{
	const char *name = obs_source_get_name(source);
	const char *sourceId = obs_source_get_id(source);

	if (strcmp(sourceId, id) != 0)
		return;

	ui->sourceList->addItem(name);
}

void PLSBasicSourceSelect::SourceRemoved(OBSSource source)
{
	const char *name = obs_source_get_name(source);
	const char *sourceId = obs_source_get_id(source);

	if (strcmp(sourceId, id) != 0)
		return;

	QList<QListWidgetItem *> items = ui->sourceList->findItems(name, Qt::MatchFixedString);

	if (!items.count())
		return;

	delete items[0];
}

static void AddSource(void *_data, obs_scene_t *scene)
{
	AddSourceData *data = (AddSourceData *)_data;
	obs_sceneitem_t *sceneitem;

	sceneitem = obs_scene_add(scene, data->source);
	obs_sceneitem_set_visible(sceneitem, data->visible);

	if (data->setpos) {
		obs_sceneitem_set_info(sceneitem, &data->itemInfo);
	}

	const char *id = obs_source_get_id(data->source);
	if (id && !strcmp(id, PRISM_BACKGROUND_TEMPLATE_SOURCE_ID)) { // fit to screen
		obs_video_info ovi;
		obs_get_video_info(&ovi);

		obs_transform_info itemInfo;
		vec2_set(&itemInfo.pos, 0.0f, 0.0f);
		vec2_set(&itemInfo.scale, 1.0f, 1.0f);
		itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
		itemInfo.rot = 0.0f;

		vec2_set(&itemInfo.bounds, float(ovi.base_width), float(ovi.base_height));
		itemInfo.bounds_type = OBS_BOUNDS_SCALE_INNER;
		itemInfo.bounds_alignment = OBS_ALIGN_CENTER;

		obs_sceneitem_set_info(sceneitem, &itemInfo);
	}
}

static char *get_new_source_name(const char *name)
{
	struct dstr new_name = {0};
	int inc = 0;

	dstr_copy(&new_name, name);

	for (;;) {
		obs_source_t *existing_source = obs_get_source_by_name(new_name.array);
		if (!existing_source)
			break;

		obs_source_release(existing_source);

		dstr_printf(&new_name, "%s %d", name, ++inc + 1);
	}

	return new_name.array;
}

static void AddExisting(const char *name, bool visible, bool duplicate)
{
	PLSBasic *main = reinterpret_cast<PLSBasic *>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();
	if (!scene)
		return;

	obs_source_t *source = obs_get_source_by_name(name);
	if (source) {
		if (duplicate) {
			obs_source_t *from = source;
			char *new_name = get_new_source_name(name);
			source = obs_source_duplicate(from, new_name, false);
			bfree(new_name);
			obs_source_release(from);

			if (!source)
				return;
		}

		AddSourceData data(source, visible);

		const char *id = obs_source_get_id(source);
		if (id && !strcmp(id, PRISM_CHAT_SOURCE_ID)) {
			data.centerShow();
		} else if (id && !strcmp(id, PRISM_SPECTRALIZER_SOURCE_ID)) {
			data.centerShow(SPECTRALIZER_MIN_WIDTH);
		} else if (id && (!strcmp(id, PRISM_MOBILE_SOURCE_ID) || !strcmp(id, PRISM_TIMER_SOURCE_ID))) {
			//PRISM/WuLongyue/20201222/#6214/For PRISM Mobile source: Use center point to rotate
			/** Compatible with other source
			* Either here or obs-scene.cpp::obs_scene_add_internal
			* can make PRISM Mobile source rotate on center point
			* FEEL FREE to remove one of them in the future
			*/
			data.setAlignment(OBS_ALIGN_CENTER);
			data.centerShow();
		}

		obs_enter_graphics();
		obs_scene_atomic_update(scene, AddSource, &data);
		obs_leave_graphics();

		obs_source_release(source);
	}
}

bool AddNew(QWidget *parent, const char *id, const char *name, const bool visible, OBSSource &newSource)
{
	PLSBasic *main = reinterpret_cast<PLSBasic *>(App()->GetMainWindow());
	OBSScene scene = main->GetCurrentScene();
	bool success = false;
	if (!scene)
		return false;

	obs_source_t *source = obs_get_source_by_name(name);
	if (source) {
		PLSMessageBox::information(parent, QTStr("NameExists.Title"), QTStr("NameExists.Text"));

	} else {
		source = obs_source_create(id, name, NULL, nullptr);

		if (source) {
			AddSourceData data(source, visible);

			const char *id = obs_source_get_id(source);
			if (id && !strcmp(id, PRISM_CHAT_SOURCE_ID)) {
				data.centerShow();
			} else if (id && !strcmp(id, PRISM_SPECTRALIZER_SOURCE_ID)) {
				data.centerShow(SPECTRALIZER_MIN_WIDTH);
			} else if (id && (!strcmp(id, PRISM_MOBILE_SOURCE_ID) || !strcmp(id, PRISM_TIMER_SOURCE_ID))) {
				//PRISM/WuLongyue/20201222/#6214/For PRISM Mobile source: Use center point to rotate
				/** Compatible with other source
				* Either here or obs-scene.cpp::obs_scene_add_internal
				* can make PRISM Mobile source rotate on center point
				* FEEL FREE to remove one of them in the future
				*/
				data.setAlignment(OBS_ALIGN_CENTER);
				data.centerShow();
			}

			obs_enter_graphics();
			obs_scene_atomic_update(scene, AddSource, &data);
			obs_leave_graphics();

			newSource = source;

			/* set monitoring if source monitors by default */
			uint32_t flags = obs_source_get_output_flags(source);
			if ((flags & OBS_SOURCE_MONITOR_BY_DEFAULT) != 0) {
				obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_ONLY);
			}

			success = true;
		}
	}

	obs_source_release(source);
	return success;
}

void PLSBasicSourceSelect::on_buttonBox_accepted()
{
	bool useExisting = ui->selectExisting->isChecked();
	bool visible = ui->sourceVisible->isChecked();

	if (useExisting) {
		QListWidgetItem *item = ui->sourceList->currentItem();
		if (!item)
			return;

		AddExisting(item->text().toStdString().c_str(), visible, false);
	} else {
		if (ui->sourceName->text().simplified().isEmpty()) {
			PLSMessageBox::warning(this, QTStr("NoNameEntered.Title"), QTStr("NoNameEntered.Text"));
			return;
		}

		if (!AddNew(this, id, ui->sourceName->text().simplified().toStdString().c_str(), visible, newSource))
			return;
	}

	done(DialogCode::Accepted);
}

void PLSBasicSourceSelect::on_buttonBox_rejected()
{
	done(DialogCode::Rejected);
}

static inline const char *GetSourceDisplayName(const char *id)
{
	if (strcmp(id, "scene") == 0)
		return Str("Basic.Scene");
	return obs_source_get_display_name(id);
}

Q_DECLARE_METATYPE(OBSScene);

template<typename T> static inline T GetPLSRef(QListWidgetItem *item)
{
	return item->data(static_cast<int>(QtDataRole::PLSRef)).value<T>();
}

PLSBasicSourceSelect::PLSBasicSourceSelect(PLSBasic *parent, const char *id_, PLSDpiHelper dpiHelper) : PLSDialogView(parent, dpiHelper), ui(new Ui::PLSBasicSourceSelect), id(id_), previousIndex()
{
	dpiHelper.setCss(this, {PLSCssIndex::PLSBasicSourceSelect});
	ui->setupUi(this->content());

	QMetaObject::connectSlotsByName(this);

	ui->sourceList->setEnabled(false);
	ui->sourceList->setAttribute(Qt::WA_MacShowFocusRect, false);

	QObject::connect(ui->createNew, &QRadioButton::toggled, [this](bool checked) {
		if (checked) {
			PLS_UI_STEP(SOURCE_MODULE, "Create/Select create new", ACTION_CLICK);
			previousIndex = ui->sourceList->currentIndex();
			ui->sourceList->setCurrentIndex(QModelIndex());
			ui->sourceName->setFocus();
		}
	});

	QObject::connect(ui->selectExisting, &QRadioButton::toggled, [this](bool checked) {
		if (checked) {
			PLS_UI_STEP(SOURCE_MODULE, "Create/Select select existed", ACTION_CLICK);
			ui->sourceList->setCurrentIndex(previousIndex);
			ui->sourceList->setFocus();
		}
	});

	QObject::connect(ui->sourceVisible, &QRadioButton::toggled, [this](bool checked) {
		if (checked) {
			PLS_UI_STEP(SOURCE_MODULE, "Create/Select source visible", ACTION_CLICK);
		} else {
			PLS_UI_STEP(SOURCE_MODULE, "Create/Select source invisible", ACTION_CLICK);
		}
	});

	QString placeHolderText{QT_UTF8(GetSourceDisplayName(id))};
	if (0 == strcmp(id, SCENE_SOURCE_ID)) {
		placeHolderText = QTStr("Basic.Scene");
	} else if (0 == strcmp(id, GROUP_SOURCE_ID)) {
		placeHolderText = QTStr("Group");
	}

	QString text{placeHolderText};
	int i = 2;
	obs_source_t *source = nullptr;
	while ((source = obs_get_source_by_name(QT_TO_UTF8(text)))) {
		obs_source_release(source);
		text = QString("%1 %2").arg(placeHolderText).arg(i++);
	}

	ui->sourceName->setText(text);
	ui->sourceName->setFocus(); //Fixes deselect of text.
	ui->sourceName->selectAll();

	installEventFilter(CreateShortcutFilter());

	if (strcmp(id_, "scene") == 0) {
		PLSBasic *main = reinterpret_cast<PLSBasic *>(App()->GetMainWindow());
		OBSSource curSceneSource = main->GetCurrentSceneSource();

		ui->selectExisting->setChecked(true);
		ui->createNew->setChecked(false);
		ui->createNew->setEnabled(false);
		ui->sourceName->setEnabled(false);
		ui->sourceName->setSelection(0, 0);

		SceneDisplayVector data = PLSSceneDataMgr::Instance()->GetDisplayVector();
		for (auto iter = data.begin(); iter != data.end(); ++iter) {
			PLSSceneItemView *item = iter->second;
			if (!item) {
				continue;
			}
			OBSScene scene = item->GetData();
			OBSSource sceneSource = obs_scene_get_source(scene);

			if (curSceneSource == sceneSource)
				continue;

			const char *name = obs_source_get_name(sceneSource);
			ui->sourceList->addItem(name);
		}
	} else if (strcmp(id_, "group") == 0) {
		obs_enum_sources(EnumGroups, this);
	} else {
		obs_enum_sources(EnumSources, this);
	}
}

void PLSBasicSourceSelect::SourcePaste(const char *name, bool visible, bool dup)
{
	AddExisting(name, visible, dup);
}
