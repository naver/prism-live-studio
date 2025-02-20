/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

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

#include "properties-view.hpp"
#include "window-namedialog.hpp"
#include "window-basic-main.hpp"
#include "window-basic-filters.hpp"
#include "display-helpers.hpp"
#include "qt-wrappers.hpp"
#include "visibility-item-widget.hpp"
#include "item-widget-helpers.hpp"
#include "obs-app.hpp"
#include "undo-stack-obs.hpp"
#include "PLSNameDialog.hpp"

#include "PLSMessageBox.h"
#include <QCloseEvent>
#include <obs-data.h>
#include <obs.h>
#include <util/base.h>
#include <vector>
#include <string>
#include <QMenu>
#include <QVariant>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#endif

#include "PLSPropertiesView.hpp"
#include "PLSVstFilterView.h"
#include "pls-common-define.hpp"
#include "PLSFiltersItemView.h"
#include <pls/pls-source.h>
#include "frontend-api.h"
#include "PLSPlatformApi.h"

using namespace std;

Q_DECLARE_METATYPE(OBSSource);

OBSBasicFilters::OBSBasicFilters(QWidget *parent, OBSSource source_)
	: PLSDialogView(parent),
	  ui(new Ui::OBSBasicFilters),
	  source(source_),
	  addSignal(obs_source_get_signal_handler(source), "filter_add",
		    OBSBasicFilters::OBSSourceFilterAdded, this),
	  removeSignal(obs_source_get_signal_handler(source), "filter_remove",
		       OBSBasicFilters::OBSSourceFilterRemoved, this),
	  reorderSignal(obs_source_get_signal_handler(source),
			"reorder_filters", OBSBasicFilters::OBSSourceReordered,
			this),
	  removeSourceSignal(obs_source_get_signal_handler(source), "remove",
			     OBSBasicFilters::SourceRemoved, this),
	  renameSourceSignal(obs_source_get_signal_handler(source), "rename",
			     OBSBasicFilters::SourceRenamed, this),
	  noPreviewMargin(13)
{
	main = reinterpret_cast<OBSBasic *>(parent);

	setupUi(ui);
	initSize({930, 700});
	setResizeEnabled(false);
	ui->asyncEmptyDesLabel->hide();
	ui->effectEmptyDesLabel->hide();

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	ui->asyncFilters->setItemDelegate(
		new PLSFiltersItemDelegate(ui->asyncFilters));
	ui->effectFilters->setItemDelegate(
		new PLSFiltersItemDelegate(ui->effectFilters));

	ui->asyncFilters->setHorizontalScrollBarPolicy(
		Qt::ScrollBarPolicy::ScrollBarAlwaysOff);
	ui->effectFilters->setHorizontalScrollBarPolicy(
		Qt::ScrollBarPolicy::ScrollBarAlwaysOff);

	/*ui->asyncFilters->setStyle(
		new PLSFiltersProxyStyle());
	ui->effectFilters->setStyle(
		new PLSFiltersProxyStyle());*/

	const char *name = obs_source_get_name(source);
	setWindowTitle(QTStr("Basic.Filters.Title").arg(QT_UTF8(name)));

#ifndef QT_NO_SHORTCUT
	ui->actionRemoveFilter->setShortcut(
		QApplication::translate("OBSBasicFilters", "Del", nullptr));
#endif // QT_NO_SHORTCUT

	//PRISM/FanZirong/20240326/4824/start/Create copy and past Action in advance
	copyAction = new QAction(QTStr("Copy"));
	connect(copyAction, &QAction::triggered, this,
		&OBSBasicFilters::CopyFilter);
	copyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
	ui->effectWidget->addAction(copyAction);
	ui->asyncWidget->addAction(copyAction);

	pasteAction = new QAction(QTStr("Paste"));
	pasteAction->setEnabled(main->copyFilter);
	connect(pasteAction, &QAction::triggered, this,
		&OBSBasicFilters::PasteFilter);
	pasteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
	ui->effectWidget->addAction(pasteAction);
	ui->asyncWidget->addAction(pasteAction);
	//PRISM/FanZirong/20240326/4824/end/Create copy and past Action in advance

	addAction(ui->actionRenameFilter);
	addAction(ui->actionRemoveFilter);
	addAction(ui->actionMoveUp);
	addAction(ui->actionMoveDown);

	installEventFilter(CreateShortcutFilter(parent));

	connect(ui->asyncEmptyDesLabel, &ClickableLabel::rightclicked, this, [=]() {
		QPoint pos = this->mapFromGlobal(QCursor::pos());
	CustomContextMenu(pos, true); });
	connect(ui->effectEmptyDesLabel, &ClickableLabel::rightclicked, this, [=]() {
		QPoint pos = this->mapFromGlobal(QCursor::pos());
	CustomContextMenu(pos, false); });

	connect(ui->asyncFilters->itemDelegate(),
		&QAbstractItemDelegate::closeEditor, [this](QWidget* editor) {
			FilterNameEdited(editor, ui->asyncFilters);
		});

	connect(ui->effectFilters->itemDelegate(),
		&QAbstractItemDelegate::closeEditor, [this](QWidget* editor) {
			FilterNameEdited(editor, ui->effectFilters);
		});

	QPushButton *close = ui->property_buttonBox->button(QDialogButtonBox::Close);
	connect(close, &QPushButton::clicked, this, &OBSBasicFilters::close);
	close->setDefault(true);

	connect(ui->property_buttonBox->button(QDialogButtonBox::RestoreDefaults),
		&QPushButton::clicked, this, &OBSBasicFilters::ResetFilters);

	connect(ui->asyncFilters->model(), &QAbstractItemModel::rowsMoved, this,
		&OBSBasicFilters::FiltersMoved);
	connect(ui->effectFilters->model(), &QAbstractItemModel::rowsMoved,
		this, &OBSBasicFilters::FiltersMoved);

	uint32_t caps = obs_source_get_output_flags(source);
	bool audio = (caps & OBS_SOURCE_AUDIO) != 0;
	bool audioOnly = (caps & OBS_SOURCE_VIDEO) == 0;
	bool async = (caps & OBS_SOURCE_ASYNC) != 0;

	if (!async && !audio) {
		ui->asyncWidget->setVisible(false);
		ui->separatorLine->setVisible(false);
	}
	if (audioOnly) {
		ui->effectWidget->setVisible(false);
		ui->separatorLine->setVisible(false);
		UpdateSplitter(false);
	}

	if (async && !audioOnly && ui->asyncFilters->count() == 0 &&
	    ui->effectFilters->count() != 0) {
		ui->effectFilters->setFocus();
	}

	if (audioOnly || (audio && !async))
		ui->asyncLabel->setText(QTStr("Basic.Filters.AudioFilters"));

	if (async && audio && ui->asyncFilters->count() == 0) {
		UpdateSplitter(false);
	} else if (!audioOnly) {
		UpdateSplitter();
	}

	obs_source_inc_showing(source);

	auto addDrawCallback = [this]() {
		obs_display_add_draw_callback(ui->preview->GetDisplay(),
					      OBSBasicFilters::DrawPreview,
					      this);
	};

	enum obs_source_type type = obs_source_get_type(source);
	bool drawable_type = type == OBS_SOURCE_TYPE_INPUT ||
			     type == OBS_SOURCE_TYPE_SCENE;

	ui->preview->show();
	if (drawable_type)
		connect(ui->preview, &OBSQTDisplay::DisplayCreated,
			addDrawCallback);

#ifdef __APPLE__
	ui->actionRenameFilter->setShortcut({ Qt::Key_Return });
#else
	ui->actionRenameFilter->setShortcut({ Qt::Key_F2 });
#endif

	UpdateFilters();
	pls_set_css(this, {"OBSBasicFilters"});

	blog(LOG_INFO, "OBSBasicFilters UI Inited (UI : %p , Source : %p)", this, source.Get());
}

OBSBasicFilters::~OBSBasicFilters()
{
	obs_source_dec_showing(source);
	ClearListItems(ui->asyncFilters);
	ClearListItems(ui->effectFilters);
	blog(LOG_INFO, "OBSBasicFilters UI Released (UI : %p , Source : %p)", this, source.Get());
}

void OBSBasicFilters::Init()
{
	show();
}

OBSSource OBSBasicFilters::GetSource() const
{
	return source;
}

inline OBSSource OBSBasicFilters::GetFilter(int row, bool async)
{
	if (row == -1)
		return OBSSource();

	QListWidget *list = async ? ui->asyncFilters : ui->effectFilters;
	QListWidgetItem *item = list->item(row);
	if (!item)
		return OBSSource();

	QVariant v = item->data(Qt::UserRole);
	return v.value<OBSSource>();
}

void FilterChangeUndoRedo(void *vp, obs_data_t *nd_old_settings,
			  obs_data_t *new_settings)
{
	obs_source_t *source = reinterpret_cast<obs_source_t *>(vp);
	const char *source_uuid = obs_source_get_uuid(source);
	OBSBasic *main = OBSBasic::Get();

	OBSDataAutoRelease redo_wrapper = obs_data_create();
	obs_data_set_string(redo_wrapper, "uuid", source_uuid);
	obs_data_set_string(redo_wrapper, "settings",
			    obs_data_get_json(new_settings));

	OBSDataAutoRelease undo_wrapper = obs_data_create();
	obs_data_set_string(undo_wrapper, "uuid", source_uuid);
	obs_data_set_string(undo_wrapper, "settings",
			    obs_data_get_json(nd_old_settings));

	auto undo_redo = [](const std::string &data) {
		OBSDataAutoRelease dat =
			obs_data_create_from_json(data.c_str());
		const char *filter_uuid = obs_data_get_string(dat, "uuid");
		OBSSourceAutoRelease filter =
			obs_get_source_by_uuid(filter_uuid);
		OBSDataAutoRelease new_settings = obs_data_create_from_json(
			obs_data_get_string(dat, "settings"));

		OBSDataAutoRelease current_settings =
			obs_source_get_settings(filter);
		obs_data_clear(current_settings);

		obs_source_update(filter, new_settings);
		obs_source_update_properties(filter);
	};

	main->undo_s.enable();

	std::string undo_data = obs_data_get_json(undo_wrapper);
	std::string redo_data = obs_data_get_json(redo_wrapper);
	main->undo_s.add_action(QTStr("Undo.Filters").arg(source_uuid),
				undo_redo, undo_redo, undo_data, redo_data);

	obs_source_update(source, new_settings);
}

void OBSBasicFilters::UpdatePropertiesView(int row, bool async)
{
	OBSSource filter = GetFilter(row, async);
	auto temp = dynamic_cast<OBSPropertiesView *>(view);
	if (filter && temp && temp->IsObject(filter)) {
		/* do not recreate properties view if already using a view
		 * with the same object */
		return;
	}

	if (view) {
		updatePropertiesSignal.Disconnect();
		ui->propertiesFrame->setVisible(true);
		/* Deleting a filter will trigger a visibility change, which will also
		 * trigger a focus change if the focus has not been on the list itself
		 * (e.g. after interacting with the property view).
		 *
		 * When an async filter list is available in the view, it will be the first
		 * candidate to receive focus. If this list is empty, we hide the property
		 * view by default and set the view to a `nullptr`.
		 *
		 * When the call for the visibility change returns, we need to check for
		 * this possibility, as another event might have hidden (and deleted) the
		 * view already.
		 *
		 * macOS might be especially affected as it doesn't switch keyboard focus
		 * to buttons like Windows does. */
		if (view) {
			view->hide();
			view->deleteLater();
			view = nullptr;
		}
	}

	if (!filter) {
		if (async) {
			ui->asyncFilters->hide();
			ui->asyncEmptyDesLabel->show();
		} else {
			ui->effectFilters->hide();
			ui->effectEmptyDesLabel->show();
		}

		return;
	}

	if (async) {
		ui->asyncFilters->show();
		ui->asyncEmptyDesLabel->hide();
	} else {
		ui->effectFilters->show();
		ui->effectEmptyDesLabel->hide();
	}

	OBSDataAutoRelease settings = obs_source_get_settings(filter);

	auto disabled_undo = [](void *vp, obs_data_t *settings) {
		OBSBasic *main =
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow());
		main->undo_s.disable();
		obs_source_t *source = reinterpret_cast<obs_source_t *>(vp);
		obs_source_update(source, settings);
	};

	const char *id = obs_source_get_id(filter);
	if (pls_is_equal(id, "vst_filter")) {
		view = pls_new<PLSVstFilterView>(settings.Get(), filter);
	} else {
		//PRISM/renjinbo/20230104/#/change to PLSPropertiesView
		view = new PLSPropertiesView(
			settings.Get(), filter,
			(PropertiesReloadCallback)obs_source_properties,
			(PropertiesUpdateCallback)FilterChangeUndoRedo,
			(PropertiesVisualUpdateCb)disabled_undo);

		updatePropertiesSignal.Connect(
			obs_source_get_signal_handler(filter),
			"update_properties", OBSBasicFilters::UpdateProperties,
			this);
	}

	view->setMinimumHeight(150);
	UpdateSplitter();
	ui->propertiesLayout->addWidget(view);
	view->show();

	//#PRISM_PC-1351 ren.jinbo windows need add a item after linked label, so can show mouse
	QWidget *placeholderMouse = new QWidget(this);
	placeholderMouse->hide();
	ui->propertiesLayout->addWidget(placeholderMouse);
}

void OBSBasicFilters::UpdateProperties(void *data, calldata_t *)
{
	if (auto view = dynamic_cast<PLSPropertiesView *>(
		    static_cast<OBSBasicFilters *>(data)->view);
	    view) {
		QMetaObject::invokeMethod(
			static_cast<OBSBasicFilters *>(data)->view,
			"ReloadProperties");
	}
}

void OBSBasicFilters::AddFilter(OBSSource filter, bool focus)
{
	uint32_t flags = obs_source_get_output_flags(filter);
	bool async = (flags & OBS_SOURCE_ASYNC) != 0;
	QListWidget* list = async ? ui->asyncFilters : ui->effectFilters;

	QListWidgetItem* item = new QListWidgetItem();
	Qt::ItemFlags itemFlags = item->flags();

	item->setFlags(itemFlags | Qt::ItemIsEditable);
	item->setData(Qt::UserRole, QVariant::fromValue(filter));

	list->addItem(item);
	if (focus)
		list->setCurrentItem(item);

	PLSSetupVisibilityItem(list, item, filter);
}

void OBSBasicFilters::RemoveFilter(OBSSource filter)
{
	uint32_t flags = obs_source_get_output_flags(filter);
	bool async = (flags & OBS_SOURCE_ASYNC) != 0;
	QListWidget *list = async ? ui->asyncFilters : ui->effectFilters;

	for (int i = 0; i < list->count(); i++) {
		QListWidgetItem *item = list->item(i);
		QVariant v = item->data(Qt::UserRole);
		OBSSource curFilter = v.value<OBSSource>();

		if (filter == curFilter) {
			DeleteListItem(list, item);
			break;
		}
	}

	const char *filterName = obs_source_get_name(filter);
	const char *sourceName = obs_source_get_name(source);
	if (!sourceName || !filterName)
		return;

	const char *filterId = obs_source_get_id(filter);

	blog(LOG_INFO, "User removed filter '%s' (%s) from source '%s'",
	     filterName, filterId, sourceName);

	main->SaveProject();
}

struct FilterOrderInfo {
	int asyncIdx = 0;
	int effectIdx = 0;
	OBSBasicFilters *window;

	inline FilterOrderInfo(OBSBasicFilters *window_) : window(window_) {}
};

void OBSBasicFilters::ReorderFilter(QListWidget* list, obs_source_t* filter,
	size_t idx)
{
	int count = list->count();

	for (int i = 0; i < count; i++) {
		QListWidgetItem* listItem = list->item(i);
		QVariant v = listItem->data(Qt::UserRole);
		OBSSource filterItem = v.value<OBSSource>();

		if (filterItem == filter) {
			if ((int)idx != i) {
				bool sel = (list->currentRow() == i);

				listItem = TakeListItem(list, i);
				if (listItem) {
					list->insertItem((int)idx, listItem);
					PLSSetupVisibilityItem(list, listItem,
						filterItem);

					if (sel)
						list->setCurrentRow((int)idx);
				}
			}

			break;
		}
	}
}

void OBSBasicFilters::ReorderFilters()
{
	FilterOrderInfo info(this);

	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *p) {
			FilterOrderInfo *info =
				reinterpret_cast<FilterOrderInfo *>(p);
			uint32_t flags;
			bool async;

			flags = obs_source_get_output_flags(filter);
			async = (flags & OBS_SOURCE_ASYNC) != 0;

			if (async) {
				info->window->ReorderFilter(
					info->window->ui->asyncFilters, filter,
					info->asyncIdx++);
			} else {
				info->window->ReorderFilter(
					info->window->ui->effectFilters, filter,
					info->effectIdx++);
			}

			
		},
		&info);
}

void OBSBasicFilters::UpdateFilters()
{
	if (!source)
		return;

	ClearListItems(ui->effectFilters);
	ClearListItems(ui->asyncFilters);

	obs_source_enum_filters(
		source,
		[](obs_source_t *, obs_source_t *filter, void *p) {
			OBSBasicFilters *window =
				reinterpret_cast<OBSBasicFilters *>(p);

			window->AddFilter(filter, false);
		},
		this);

	if (ui->asyncFilters->count() > 0) {
		ui->asyncFilters->setCurrentItem(ui->asyncFilters->item(0));
		ui->asyncFilters->setFocus();
	}
	if (ui->effectFilters->count() > 0) {
		ui->effectFilters->setCurrentItem(ui->effectFilters->item(0));
		ui->asyncFilters->setFocus();
	}

	if (0 == ui->asyncFilters->count()) {
		ui->asyncFilters->hide();
		ui->asyncEmptyDesLabel->show();
	}
	if (0 == ui->effectFilters->count()) {
		ui->effectFilters->hide();
		ui->effectEmptyDesLabel->show();
	}

	main->SaveProject();
}

void OBSBasicFilters::UpdateSplitter()
{
	bool show_splitter_frame =
		ui->asyncFilters->count() + ui->effectFilters->count() > 0;
	UpdateSplitter(show_splitter_frame);
}

void OBSBasicFilters::UpdateSplitter(bool show_splitter_frame)
{
	//bool show_splitter_handle = show_splitter_frame;
	//uint32_t caps = obs_source_get_output_flags(source);
	//if ((caps & OBS_SOURCE_VIDEO) == 0)
	//	show_splitter_handle = false;

	//for (int i = 0; i < ui->rightLayout->count(); i++) {
	//	QSplitterHandle* hndl = ui->rightLayout->handle(i);
	//	hndl->setEnabled(show_splitter_handle);
	//}

	//ui->propertiesFrame->setVisible(show_splitter_frame);
}

static bool filter_compatible(bool async, uint32_t sourceFlags,
			      uint32_t filterFlags)
{
	bool filterVideo = (filterFlags & OBS_SOURCE_VIDEO) != 0;
	bool filterAsync = (filterFlags & OBS_SOURCE_ASYNC) != 0;
	bool filterAudio = (filterFlags & OBS_SOURCE_AUDIO) != 0;
	bool audio = (sourceFlags & OBS_SOURCE_AUDIO) != 0;
	bool audioOnly = (sourceFlags & OBS_SOURCE_VIDEO) == 0;
	bool asyncSource = (sourceFlags & OBS_SOURCE_ASYNC) != 0;

	if (async &&
	    ((audioOnly && filterVideo) || (!audio && !asyncSource) ||
	     (filterAudio && !audio) || (!asyncSource && !filterAudio)))
		return false;

	return (async && (filterAudio || filterAsync)) ||
	       (!async && !filterAudio && !filterAsync);
}

QMenu *OBSBasicFilters::CreateAddFilterPopupMenu(bool async)
{
	uint32_t sourceFlags = obs_source_get_output_flags(source);
	const char *type_str;
	bool foundValues = false;
	size_t idx = 0;

	struct FilterInfo {
		string type;
		string name;

		inline FilterInfo(const char *type_, const char *name_)
			: type(type_),
			  name(name_)
		{
		}

		bool operator<(const FilterInfo &r) const
		{
			return name < r.name;
		}
	};

	vector<FilterInfo> types;
	while (obs_enum_filter_types(idx++, &type_str)) {
		const char *name = obs_source_get_display_name(type_str);
		uint32_t caps = obs_get_source_output_flags(type_str);

		if ((caps & OBS_SOURCE_DEPRECATED) != 0)
			continue;
		if ((caps & OBS_SOURCE_CAP_DISABLED) != 0)
			continue;
		if ((caps & OBS_SOURCE_CAP_OBSOLETE) != 0)
			continue;

		types.emplace_back(type_str, name);
	}

	sort(types.begin(), types.end());

	QMenu *popup = new QMenu(QTStr("Add"), this);
	for (FilterInfo &type : types) {
		uint32_t filterFlags =
			obs_get_source_output_flags(type.type.c_str());

		if (!filter_compatible(async, sourceFlags, filterFlags))
			continue;

		QAction *popupItem =
			new QAction(QT_UTF8(type.name.c_str()), this);
		popupItem->setData(QT_UTF8(type.type.c_str()));
		connect(popupItem, &QAction::triggered,
			[this, type]() { AddNewFilter(type.type.c_str()); });
		popup->addAction(popupItem);

		foundValues = true;
	}

	if (!foundValues) {
		delete popup;
		popup = nullptr;
	}

	return popup;
}

void OBSBasicFilters::AddNewFilter(const char *id)
{
	if (id && *id) {
		OBSSourceAutoRelease existing_filter;
		string name = obs_source_get_display_name(id);

		QString placeholder = QString::fromStdString(name);
		QString text{placeholder};
		int i = 2;
		while ((existing_filter = obs_source_get_filter_by_name(
				source, QT_TO_UTF8(text)))) {
			text = QString("%1 %2").arg(placeholder).arg(i++);
		}

		bool success = PLSNameDialog::AskForName(
			this, QTStr("Basic.Filters.AddFilter.Title"),
			QTStr("Basic.Filters.AddFilter.Text"), name, text);
		if (!success)
			return;

		if (name.empty()) {
			OBSMessageBox::warning(this, QTStr("Alert.Title"),
					       QTStr("NoNameEntered.Text"));
			AddNewFilter(id);
			return;
		}

		existing_filter =
			obs_source_get_filter_by_name(source, name.c_str());
		if (existing_filter) {
			OBSMessageBox::warning(this, QTStr("Alert.Title"),
					       QTStr("NameExists.Text"));
			AddNewFilter(id);
			return;
		}

		OBSSourceAutoRelease filter =
			obs_source_create(id, name.c_str(), nullptr, nullptr);
		if (filter) {
			const char *sourceName = obs_source_get_name(source);

			blog(LOG_INFO,
			     "User added filter '%s' (%s) to source '%s'",
			     name.c_str(), id, sourceName);

			obs_source_filter_add(source, filter);

			const char *filterId = obs_source_get_id(filter);
			pls_send_analog(AnalogType::ANALOG_ADD_FILTER,
					{{ANALOG_FILTER_TYPE_KEY,
					  pls_get_analog_filter_id(filterId)}});

		} else {
			blog(LOG_WARNING, "Creating filter '%s' failed!", id);
			return;
		}

		std::string parent_uuid(obs_source_get_uuid(source));
		std::string scene_uuid = obs_source_get_uuid(
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
				->GetCurrentSceneSource());
		/* In order to ensure that the UUID persists through undo/redo,
		 * we save the source data rather than just recreating the
		 * source from scratch. */
		OBSDataAutoRelease rwrapper = obs_save_source(filter);
		obs_data_set_string(rwrapper, "undo_uuid", parent_uuid.c_str());

		OBSDataAutoRelease uwrapper = obs_data_create();
		obs_data_set_string(uwrapper, "fname",
				    obs_source_get_name(filter));
		obs_data_set_string(uwrapper, "suuid", parent_uuid.c_str());

		auto undo = [scene_uuid](const std::string &data) {
			OBSSourceAutoRelease ssource =
				obs_get_source_by_uuid(scene_uuid.c_str());
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
				->SetCurrentScene(ssource.Get(), true);

			OBSDataAutoRelease dat =
				obs_data_create_from_json(data.c_str());
			OBSSourceAutoRelease source = obs_get_source_by_uuid(
				obs_data_get_string(dat, "suuid"));
			OBSSourceAutoRelease filter =
				obs_source_get_filter_by_name(
					source,
					obs_data_get_string(dat, "fname"));
			obs_source_filter_remove(source, filter);
		};

		auto redo = [scene_uuid](const std::string &data) {
			OBSSourceAutoRelease ssource =
				obs_get_source_by_uuid(scene_uuid.c_str());
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
				->SetCurrentScene(ssource.Get(), true);

			OBSDataAutoRelease dat =
				obs_data_create_from_json(data.c_str());
			OBSSourceAutoRelease source = obs_get_source_by_uuid(
				obs_data_get_string(dat, "undo_uuid"));
			OBSSourceAutoRelease filter = obs_load_source(dat);
			obs_source_filter_add(source, filter);
		};

		std::string undo_data(obs_data_get_json(uwrapper));
		std::string redo_data(obs_data_get_json(rwrapper));
		main->undo_s.add_action(
			QTStr("Undo.Add").arg(obs_source_get_name(filter)),
			undo, redo, undo_data, redo_data, false);
	}
}

void OBSBasicFilters::closeEvent(QCloseEvent *event)
{
	PLSDialogView::closeEvent(event);
	if (!event->isAccepted())
		return;

	obs_display_remove_draw_callback(ui->preview->GetDisplay(),
					 OBSBasicFilters::DrawPreview, this);

	main->SaveProject();
}

bool OBSBasicFilters::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef _WIN32
	const MSG &msg = *static_cast<MSG *>(message);
	switch (msg.message) {
	case WM_MOVE:
		for (OBSQTDisplay *const display :
		     findChildren<OBSQTDisplay *>()) {
			display->OnMove();
		}
		break;
	case WM_DISPLAYCHANGE:
		for (OBSQTDisplay *const display :
		     findChildren<OBSQTDisplay *>()) {
			display->OnDisplayChange();
		}
	}
#else
	UNUSED_PARAMETER(message);
#endif

	return PLSDialogView::nativeEvent(eventType, message, result);
}

/* OBS Signals */

void OBSBasicFilters::OBSSourceFilterAdded(void *param, calldata_t *data)
{
	OBSBasicFilters *window = reinterpret_cast<OBSBasicFilters *>(param);
	obs_source_t *filter = (obs_source_t *)calldata_ptr(data, "filter");

	QMetaObject::invokeMethod(window, "AddFilter",
				  Q_ARG(OBSSource, OBSSource(filter)));
}

void OBSBasicFilters::OBSSourceFilterRemoved(void *param, calldata_t *data)
{
	OBSBasicFilters *window = reinterpret_cast<OBSBasicFilters *>(param);
	obs_source_t *filter = (obs_source_t *)calldata_ptr(data, "filter");

	QMetaObject::invokeMethod(window, "RemoveFilter",
				  Q_ARG(OBSSource, OBSSource(filter)));
}

void OBSBasicFilters::OBSSourceReordered(void *param, calldata_t *)
{
	QMetaObject::invokeMethod(reinterpret_cast<OBSBasicFilters *>(param),
				  "ReorderFilters");
}

void OBSBasicFilters::SourceRemoved(void *param, calldata_t *)
{
	QMetaObject::invokeMethod(static_cast<OBSBasicFilters *>(param),
				  "close");
}

void OBSBasicFilters::SourceRenamed(void *param, calldata_t *data)
{
	const char *name = calldata_string(data, "new_name");
	QString title = QTStr("Basic.Filters.Title").arg(QT_UTF8(name));

	QMetaObject::invokeMethod(static_cast<OBSBasicFilters *>(param),
				  "setWindowTitle", Q_ARG(QString, title));
}

void OBSBasicFilters::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{

	OBSBasicFilters *window = static_cast<OBSBasicFilters *>(data);

	if (!window->source)
		return;

	uint32_t sourceCX = max(obs_source_get_width(window->source), 1u);
	uint32_t sourceCY = max(obs_source_get_height(window->source), 1u);

	int x, y;
	int newCX, newCY;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);

	newCX = int(scale * float(sourceCX));
	newCY = int(scale * float(sourceCY));

	gs_viewport_push();
	gs_projection_push();
	const bool previous = gs_set_linear_srgb(true);

	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);
	obs_source_video_render(window->source);

	gs_set_linear_srgb(previous);
	gs_projection_pop();
	gs_viewport_pop();
}

/* Qt Slots */

static bool QueryRemove(QWidget *parent, obs_source_t *source)
{
	const char *name = obs_source_get_name(source);

	QString text = QTStr("ConfirmRemove.Text.title");

	if (0 == strcmp(App()->GetLocale(), "ko-KR")) {
		return PLSAlertView::Button::Ok ==
		       PLSMessageBox::question(
			       parent, QTStr("ConfirmRemove.Title"), name, text,
			       PLSAlertView::Button::Ok |
				       PLSAlertView::Button::Cancel);

	} else {
		return PLSAlertView::Button::Ok ==
		       PLSMessageBox::question(
			       parent, QTStr("ConfirmRemove.Title"), text, name,
			       PLSAlertView::Button::Ok |
				       PLSAlertView::Button::Cancel);
	}
}

void OBSBasicFilters::on_addAsyncFilter_clicked()
{
	ui->asyncFilters->setFocus();
	QScopedPointer<QMenu> popup(CreateAddFilterPopupMenu(true));
	if (popup)
		popup->exec(QCursor::pos());
}

void OBSBasicFilters::on_removeAsyncFilter_clicked()
{
	OBSSource filter = GetFilter(ui->asyncFilters->currentRow(), true);
	if (filter) {
		if (QueryRemove(this, filter)) {
			delete_filter(filter);
			if (ui->asyncFilters->count() == 0)
				ui->effectFilters->setFocus();
		}
	}
}

void OBSBasicFilters::on_moveAsyncFilterUp_clicked()
{
	OBSSource filter = GetFilter(ui->asyncFilters->currentRow(), true);
	if (filter)
		obs_source_filter_set_order(source, filter, OBS_ORDER_MOVE_UP);
}

void OBSBasicFilters::on_moveAsyncFilterDown_clicked()
{
	OBSSource filter = GetFilter(ui->asyncFilters->currentRow(), true);
	if (filter)
		obs_source_filter_set_order(source, filter,
					    OBS_ORDER_MOVE_DOWN);
}

void OBSBasicFilters::on_asyncFilters_GotFocus()
{
	if (ui->asyncFilters->count() == 0)
		return;
	UpdatePropertiesView(ui->asyncFilters->currentRow(), true);
	isAsync = true;
}

void OBSBasicFilters::on_asyncFilters_currentRowChanged(int row)
{
	UpdatePropertiesView(row, true);
}

void OBSBasicFilters::on_addEffectFilter_clicked()
{
	ui->effectFilters->setFocus();
	QScopedPointer<QMenu> popup(CreateAddFilterPopupMenu(false));
	if (popup)
		popup->exec(QCursor::pos());
}

void OBSBasicFilters::on_removeEffectFilter_clicked()
{
	OBSSource filter = GetFilter(ui->effectFilters->currentRow(), false);
	if (filter) {
		if (QueryRemove(this, filter)) {
			delete_filter(filter);
			if (ui->effectFilters->count() == 0)
				ui->asyncFilters->setFocus();
		}
	}
}

void OBSBasicFilters::on_moveEffectFilterUp_clicked()
{
	OBSSource filter = GetFilter(ui->effectFilters->currentRow(), false);
	if (filter)
		obs_source_filter_set_order(source, filter, OBS_ORDER_MOVE_UP);
}

void OBSBasicFilters::on_moveEffectFilterDown_clicked()
{
	OBSSource filter = GetFilter(ui->effectFilters->currentRow(), false);
	if (filter)
		obs_source_filter_set_order(source, filter,
					    OBS_ORDER_MOVE_DOWN);
}

void OBSBasicFilters::on_effectFilters_GotFocus()
{
	if (ui->effectFilters->count() == 0)
		return;
	UpdatePropertiesView(ui->effectFilters->currentRow(), false);
	isAsync = false;
}

void OBSBasicFilters::on_effectFilters_currentRowChanged(int row)
{
	UpdatePropertiesView(row, false);
}

void OBSBasicFilters::on_actionRemoveFilter_triggered()
{
	if (ui->asyncFilters->hasFocus())
		on_removeAsyncFilter_clicked();
	else if (ui->effectFilters->hasFocus())
		on_removeEffectFilter_clicked();
}

void OBSBasicFilters::on_actionMoveUp_triggered()
{
	if (ui->asyncFilters->hasFocus())
		on_moveAsyncFilterUp_clicked();
	else if (ui->effectFilters->hasFocus())
		on_moveEffectFilterUp_clicked();
}

void OBSBasicFilters::on_actionMoveDown_triggered()
{
	if (ui->asyncFilters->hasFocus())
		on_moveAsyncFilterDown_clicked();
	else if (ui->effectFilters->hasFocus())
		on_moveEffectFilterDown_clicked();
}

void OBSBasicFilters::on_actionRenameFilter_triggered()
{
	//OnMouseStatusChanged(PROPERTY_VALUE_MOUSE_STATUS_NORMAL);
	if (ui->asyncFilters->hasFocus())
		RenameAsyncFilter();
	else if (ui->effectFilters->hasFocus())
		RenameEffectFilter();
}

void OBSBasicFilters::CustomContextMenu(const QPoint &pos, bool async)
{
	QListWidget *list = async ? ui->asyncFilters : ui->effectFilters;
	if (!list)
		return;
	QPoint localPos = list->mapFromGlobal(QCursor::pos());
	QListWidgetItem *item = list->itemAt(localPos);

	QMenu popup(window());

	popup.setObjectName("filterItemMenu");
	if (async)
		ui->asyncFilters->setFocus();
	else
		ui->effectFilters->setFocus();

	QPointer<QMenu> addMenu = CreateAddFilterPopupMenu(async);
	if (addMenu)
		popup.addMenu(addMenu);

	if (item) {
		if (async)
			ui->asyncFilters->setCurrentItem(item);
		else
			ui->effectFilters->setCurrentItem(item);

		popup.addSeparator();
		popup.addAction(QTStr("Duplicate"), this, [&]() {
			DuplicateItem(async ? ui->asyncFilters->currentItem()
					    : ui->effectFilters->currentItem());
		});
		popup.addSeparator();
		popup.addAction(ui->actionRenameFilter);
		popup.addAction(ui->actionRemoveFilter);
		popup.addSeparator();

		//PRISM/FanZirong/20240326/4824/Create copy and past Action in advance
		/*QAction *copyAction = new QAction(QTStr("Copy"));
		connect(copyAction, &QAction::triggered, this,
			&OBSBasicFilters::CopyFilter);
		copyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
		ui->effectWidget->addAction(copyAction);
		ui->asyncWidget->addAction(copyAction);*/
		popup.addAction(copyAction);
	}

	//PRISM/FanZirong/20240326/4824/Create copy and past Action in advance
	//QAction *pasteAction = new QAction(QTStr("Paste"));
	pasteAction->setEnabled(main->copyFilter);
	/*connect(pasteAction, &QAction::triggered, this,
		&OBSBasicFilters::PasteFilter);
	pasteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
	ui->effectWidget->addAction(pasteAction);
	ui->asyncWidget->addAction(pasteAction);*/

	popup.addAction(pasteAction);

	popup.exec(QCursor::pos());
}

void OBSBasicFilters::EditItem(QListWidgetItem *item, bool async)
{
	if (editActive)
		return;

	Qt::ItemFlags flags = item->flags();
	OBSSource filter = item->data(Qt::UserRole).value<OBSSource>();
	const char *name = obs_source_get_name(filter);
	QListWidget *list = async ? ui->asyncFilters : ui->effectFilters;
	
	list->setContentsMargins(0,0,0,0);
	list->setSpacing(0);

	item->setText(QT_UTF8(name));
	item->setFlags(flags | Qt::ItemIsEditable);
	list->removeItemWidget(item);
	list->editItem(item);
	item->setFlags(flags);

	editActive = true;
}

void OBSBasicFilters::DuplicateItem(QListWidgetItem *item)
{
	OBSSource filter = item->data(Qt::UserRole).value<OBSSource>();
	string name = obs_source_get_name(filter);
	OBSSourceAutoRelease existing_filter;

	QString placeholder = QString::fromStdString(name);
	QString text{placeholder};
	int i = 2;
	while ((existing_filter = obs_source_get_filter_by_name(
			source, QT_TO_UTF8(text)))) {
		text = QString("%1 %2").arg(placeholder).arg(i++);
	}

	bool success = PLSNameDialog::AskForName(
		this, QTStr("Basic.Filters.AddFilter.Title"),
		QTStr("Basic.Filters.AddFilter.Text"), name, text);
	if (!success)
		return;

	if (name.empty()) {
		OBSMessageBox::warning(this, QTStr("Alert.Title"),
				       QTStr("NoNameEntered.Text"));
		DuplicateItem(item);
		return;
	}

	existing_filter = obs_source_get_filter_by_name(source, name.c_str());
	if (existing_filter) {
		OBSMessageBox::warning(this, QTStr("Alert.Title"),
				       QTStr("NameExists.Text"));
		DuplicateItem(item);
		return;
	}
	bool enabled = obs_source_enabled(filter);
	OBSSourceAutoRelease new_filter =
		obs_source_duplicate(filter, name.c_str(), false);
	if (new_filter) {
		const char *sourceName = obs_source_get_name(source);
		const char *id = obs_source_get_id(new_filter);
		blog(LOG_INFO,
		     "User duplicated filter '%s' (%s) from '%s' "
		     "to source '%s'",
		     name.c_str(), id, name.c_str(), sourceName);
		obs_source_set_enabled(new_filter, enabled);
		obs_source_filter_add(source, new_filter);
	}
}

void OBSBasicFilters::on_asyncFilters_customContextMenuRequested(
	const QPoint &pos)
{
	CustomContextMenu(pos, true);
}

void OBSBasicFilters::on_effectFilters_customContextMenuRequested(
	const QPoint &pos)
{
	CustomContextMenu(pos, false);
}

void OBSBasicFilters::RenameAsyncFilter()
{
	EditItem(ui->asyncFilters->currentItem(), true);
}

void OBSBasicFilters::RenameEffectFilter()
{
	EditItem(ui->effectFilters->currentItem(), false);
}

void OBSBasicFilters::FilterNameEdited(QWidget* editor, QListWidget* list)
{
	QListWidgetItem* listItem = list->currentItem();
	OBSSource filter = listItem->data(Qt::UserRole).value<OBSSource>();
	QLineEdit* edit = qobject_cast<QLineEdit*>(editor);
	string name = QT_TO_UTF8(edit->text().trimmed());

	const char* prevName = obs_source_get_name(filter);
	bool sameName = (name == prevName);
	OBSSourceAutoRelease foundFilter = nullptr;

	if (!sameName)
		foundFilter =
		obs_source_get_filter_by_name(source, name.c_str());

	if (foundFilter || name.empty() || sameName) {
		listItem->setText(QT_UTF8(prevName));

		if (foundFilter) {
			OBSMessageBox::information(window(),
				QTStr("NameExists.Title"),
				QTStr("NameExists.Text"));
		}
		else if (name.empty()) {
			OBSMessageBox::information(window(),
				QTStr("NoNameEntered.Title"),
				QTStr("NoNameEntered.Text"));
		}
	}
	else {
		const char* sourceName = obs_source_get_name(source);

		blog(LOG_INFO,
			"User renamed filter '%s' on source '%s' to '%s'",
			prevName, sourceName, name.c_str());

		listItem->setText(QT_UTF8(name.c_str()));
		obs_source_set_name(filter, name.c_str());

		std::string scene_uuid = obs_source_get_uuid(
			reinterpret_cast<OBSBasic*>(App()->GetMainWindow())
			->GetCurrentSceneSource());
		auto undo = [scene_uuid, prev = std::string(prevName),
			name](const std::string& uuid) {
			OBSSourceAutoRelease ssource =
				obs_get_source_by_uuid(scene_uuid.c_str());
			reinterpret_cast<OBSBasic*>(App()->GetMainWindow())
				->SetCurrentScene(ssource.Get(), true);

			OBSSourceAutoRelease filter =
				obs_get_source_by_uuid(uuid.c_str());
			obs_source_set_name(filter, prev.c_str());
		};

		auto redo = [scene_uuid, prev = std::string(prevName),
			name](const std::string& uuid) {
			OBSSourceAutoRelease ssource =
				obs_get_source_by_uuid(scene_uuid.c_str());
			reinterpret_cast<OBSBasic*>(App()->GetMainWindow())
				->SetCurrentScene(ssource.Get(), true);

			OBSSourceAutoRelease filter =
				obs_get_source_by_uuid(uuid.c_str());
			obs_source_set_name(filter, name.c_str());
		};

		std::string filter_uuid(obs_source_get_uuid(filter));
		main->undo_s.add_action(QTStr("Undo.Rename").arg(name.c_str()),
			undo, redo, filter_uuid, filter_uuid);
	}

	listItem->setText(QString());
	PLSSetupVisibilityItem(list, listItem, filter);
	editActive = false;
}

static bool ConfirmReset(QWidget *parent)
{
	QMessageBox::StandardButton button;

	button = OBSMessageBox::question(parent, QTStr("ConfirmReset.Title"),
					 QTStr("ConfirmReset.Text"),
					 QMessageBox::Yes | QMessageBox::No);

	return button == QMessageBox::Yes;
}

void OBSBasicFilters::ResetFilters()
{
	QListWidget *list = isAsync ? ui->asyncFilters : ui->effectFilters;
	int row = list->currentRow();

	OBSSource filter = GetFilter(row, isAsync);

	if (!filter)
		return;

	if (!ConfirmReset(this))
		return;
	
	auto id = obs_source_get_id(filter);
	if(pls_is_equal("sound_touch_filter", id)){
		obs_data_t *data = obs_data_create();
		obs_data_set_string(data, "method",
							"set_default");
		pls_source_set_private_data(filter, data);
		obs_data_release(data);
	}

	OBSDataAutoRelease settings = obs_source_get_settings(filter);

	OBSDataAutoRelease empty_settings = obs_data_create();
	FilterChangeUndoRedo((void *)filter, settings, empty_settings);

	obs_data_clear(settings);

	if (auto filterView = dynamic_cast<PLSPropertiesView *>(view);
	    filterView) {
		if (!filterView->DeferUpdate())
			obs_source_update(filter, nullptr);

		filterView->ReloadProperties();
	} else if (auto filterView = dynamic_cast<PLSVstFilterView *>(view);
		   filterView) {
		filterView->resetProperties();
	}
}

void OBSBasicFilters::CopyFilter()
{
	OBSSource filter = nullptr;

	if (isAsync)
		filter = GetFilter(ui->asyncFilters->currentRow(), true);
	else
		filter = GetFilter(ui->effectFilters->currentRow(), false);

	main->copyFilter = OBSGetWeakRef(filter);

	//PRISM/FanZirong/20240326/4824/Create copy and past Action in advance
	pasteAction->setEnabled(main->copyFilter);
}

void OBSBasicFilters::PasteFilter()
{
	OBSSource filter = OBSGetStrongRef(main->copyFilter);
	if (!filter)
		return;

	OBSDataArrayAutoRelease undo_array = obs_source_backup_filters(source);
	obs_source_copy_single_filter(source, filter);
	OBSDataArrayAutoRelease redo_array = obs_source_backup_filters(source);

	const char *filterName = obs_source_get_name(filter);
	const char *sourceName = obs_source_get_name(source);
	QString text =
		QTStr("Undo.Filters.Paste.Single").arg(filterName, sourceName);

	main->CreateFilterPasteUndoRedoAction(text, source, undo_array,
					      redo_array);
}

void OBSBasicFilters::delete_filter(OBSSource filter)
{
	OBSDataAutoRelease wrapper = obs_save_source(filter);
	std::string parent_uuid(obs_source_get_uuid(source));
	obs_data_set_string(wrapper, "undo_uuid", parent_uuid.c_str());

	std::string scene_uuid = obs_source_get_uuid(
		reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
			->GetCurrentSceneSource());
	auto undo = [scene_uuid](const std::string &data) {
		OBSSourceAutoRelease ssource =
			obs_get_source_by_uuid(scene_uuid.c_str());
		reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
			->SetCurrentScene(ssource.Get(), true);

		OBSDataAutoRelease dat =
			obs_data_create_from_json(data.c_str());
		OBSSourceAutoRelease source = obs_get_source_by_uuid(
			obs_data_get_string(dat, "undo_uuid"));
		OBSSourceAutoRelease filter = obs_load_source(dat);
		obs_source_filter_add(source, filter);
	};

	OBSDataAutoRelease rwrapper = obs_data_create();
	obs_data_set_string(rwrapper, "fname", obs_source_get_name(filter));
	obs_data_set_string(rwrapper, "suuid", parent_uuid.c_str());
	auto redo = [scene_uuid](const std::string &data) {
		OBSSourceAutoRelease ssource =
			obs_get_source_by_uuid(scene_uuid.c_str());
		reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
			->SetCurrentScene(ssource.Get(), true);

		OBSDataAutoRelease dat =
			obs_data_create_from_json(data.c_str());
		OBSSourceAutoRelease source = obs_get_source_by_uuid(
			obs_data_get_string(dat, "suuid"));
		OBSSourceAutoRelease filter = obs_source_get_filter_by_name(
			source, obs_data_get_string(dat, "fname"));
		obs_source_filter_remove(source, filter);
	};

	std::string undo_data(obs_data_get_json(wrapper));
	std::string redo_data(obs_data_get_json(rwrapper));
	main->undo_s.add_action(
		QTStr("Undo.Delete").arg(obs_source_get_name(filter)), undo,
		redo, undo_data, redo_data, false);
	obs_source_filter_remove(source, filter);
}

void OBSBasicFilters::PLSSetupVisibilityItem(QListWidget* list, QListWidgetItem* item, obs_source_t* source)
{
	PLSFiltersItemView* baseWidget = new PLSFiltersItemView(source);

	connect(baseWidget, &PLSFiltersItemView::OnCreateCustomContextMenu, this, &OBSBasicFilters::CustomContextMenu);

	list->setItemWidget(item, baseWidget);
}

void OBSBasicFilters::FiltersMoved(const QModelIndex &, int srcIdxStart, int,
				   const QModelIndex &, int)
{
	QListWidget* list = isAsync ? ui->asyncFilters : ui->effectFilters;
	int neighborIdx = 0;

	if (srcIdxStart < list->currentRow())
		neighborIdx = list->currentRow() - 1;
	else if (srcIdxStart > list->currentRow())
		neighborIdx = list->currentRow() + 1;
	else
		return;

	if (neighborIdx > list->count() - 1)
		neighborIdx = list->count() - 1;
	else if (neighborIdx < 0)
		neighborIdx = 0;

	OBSSource neighbor = GetFilter(neighborIdx, isAsync);
	int idx = obs_source_filter_get_index(source, neighbor);

	OBSSource filter = GetFilter(list->currentRow(), isAsync);
	obs_source_filter_set_index(source, filter, idx);
}