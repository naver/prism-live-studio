/******************************************************************************
    Copyright (C) 2015 by Hugh Bailey <obs.jim@gmail.com>

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
	ui->asyncEmptyDesLabel->hide();
	ui->effectEmptyDesLabel->hide();

	ui->asyncFilters->setItemDelegate(
		new PLSFiltersItemDelegate(ui->asyncFilters));
	ui->effectFilters->setItemDelegate(
		new PLSFiltersItemDelegate(ui->effectFilters));

	const char *name = obs_source_get_name(source);
	setWindowTitle(QTStr("Basic.Filters.Title").arg(QT_UTF8(name)));

#ifndef QT_NO_SHORTCUT
	ui->actionRemoveFilter->setShortcut(
		QApplication::translate("OBSBasicFilters", "Del", nullptr));
#endif // QT_NO_SHORTCUT

	addAction(ui->actionRemoveFilter);
	addAction(ui->actionMoveUp);
	addAction(ui->actionMoveDown);

	installEventFilter(CreateShortcutFilter(parent));

	/*connect(ui->asyncFilters->itemDelegate(),
		SIGNAL(closeEditor(QWidget *,
				   QAbstractItemDelegate::EndEditHint)),
		this,
		SLOT(AsyncFilterNameEdited(
			QWidget *, QAbstractItemDelegate::EndEditHint)));*/

	/*connect(ui->effectFilters->itemDelegate(),
		SIGNAL(closeEditor(QWidget *,
				   QAbstractItemDelegate::EndEditHint)),
		this,
		SLOT(EffectFilterNameEdited(
			QWidget *, QAbstractItemDelegate::EndEditHint)));*/

	QPushButton *close =
		ui->property_buttonBox->button(QDialogButtonBox::Close);
	connect(close, SIGNAL(clicked()), this, SLOT(close()));
	close->setDefault(true);

	connect(ui->property_buttonBox->button(
			QDialogButtonBox::RestoreDefaults),
		SIGNAL(clicked()), this, SLOT(ResetFilters()));

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

	//QAction *renameAsync = new QAction(ui->asyncWidget);
	//renameAsync->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	//connect(renameAsync, SIGNAL(triggered()), this,
	//	SLOT(RenameAsyncFilter()));
	//ui->asyncWidget->addAction(renameAsync);

	//QAction *renameEffect = new QAction(ui->effectWidget);
	//renameEffect->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	//connect(renameEffect, SIGNAL(triggered()), this,
	//	SLOT(RenameEffectFilter()));
	//ui->effectWidget->addAction(renameEffect);

	//#ifdef __APPLE__
	//	renameAsync->setShortcut({Qt::Key_Return});
	//	renameEffect->setShortcut({Qt::Key_Return});
	//#else
	//	renameAsync->setShortcut({Qt::Key_F2});
	//	renameEffect->setShortcut({Qt::Key_F2});
	//#endif

	UpdateFilters();
	pls_set_css(this, {"OBSBasicFilters", "VisibilityCheckBox"});
}

OBSBasicFilters::~OBSBasicFilters()
{
	obs_source_dec_showing(source);
	ClearListItems(ui->asyncFilters);
	ClearListItems(ui->effectFilters);
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
	obs_source_t *parent = obs_filter_get_parent(source);
	const char *source_name = obs_source_get_name(source);
	OBSBasic *main = OBSBasic::Get();

	OBSDataAutoRelease redo_wrapper = obs_data_create();
	obs_data_set_string(redo_wrapper, "name", source_name);
	obs_data_set_string(redo_wrapper, "settings",
			    obs_data_get_json(new_settings));
	obs_data_set_string(redo_wrapper, "parent",
			    obs_source_get_name(parent));

	OBSDataAutoRelease filter_settings = obs_source_get_settings(source);

	OBSDataAutoRelease undo_wrapper = obs_data_create();
	obs_data_set_string(undo_wrapper, "name", source_name);
	obs_data_set_string(undo_wrapper, "settings",
			    obs_data_get_json(nd_old_settings));
	obs_data_set_string(undo_wrapper, "parent",
			    obs_source_get_name(parent));

	auto undo_redo = [](const std::string &data) {
		OBSDataAutoRelease dat =
			obs_data_create_from_json(data.c_str());
		OBSSourceAutoRelease parent_source = obs_get_source_by_name(
			obs_data_get_string(dat, "parent"));
		const char *filter_name = obs_data_get_string(dat, "name");
		OBSSourceAutoRelease filter = obs_source_get_filter_by_name(
			parent_source, filter_name);
		OBSDataAutoRelease new_settings = obs_data_create_from_json(
			obs_data_get_string(dat, "settings"));

		OBSDataAutoRelease current_settings =
			obs_source_get_settings(filter);
		obs_data_clear(current_settings);

		obs_source_update(filter, new_settings);
		obs_source_update_properties(filter);
	};

	main->undo_s.enable();

	std::string name = std::string(obs_source_get_name(source));
	std::string undo_data = obs_data_get_json(undo_wrapper);
	std::string redo_data = obs_data_get_json(redo_wrapper);
	main->undo_s.add_action(QTStr("Undo.Filters").arg(name.c_str()),
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
	FocusList *list = async ? ui->asyncFilters : ui->effectFilters;
	QListWidgetItem *item = new QListWidgetItem();
	Qt::ItemFlags itemFlags = item->flags();

	item->setFlags(itemFlags | Qt::ItemIsEditable);
	item->setData(Qt::UserRole, QVariant::fromValue(filter));

	list->addItem(item);
	bool current = false;
	if (focus) {
		current = true;
		list->setCurrentItem(item);
		isAsync = async;
	}

	SetupFiltersItem(list, item, filter, current);
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

void OBSBasicFilters::ReorderFilter(FocusList *list, obs_source_t *filter,
				    size_t idx, bool async)
{
	int count = list->count();

	for (int i = 0; i < count; i++) {
		QListWidgetItem *listItem = list->item(i);
		QVariant v = listItem->data(Qt::UserRole);
		OBSSource filterItem = v.value<OBSSource>();

		if (filterItem == filter) {
			if ((int)idx != i) {
				bool sel = (list->currentRow() == i);

				listItem = TakeListItem(list, i);
				if (listItem) {
					list->insertItem((int)idx, listItem);
					SetupFiltersItem(list, listItem,
							 filterItem, sel);

					if (sel) {
						list->setCurrentRow((int)idx);
					}
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
					info->asyncIdx++, true);
			} else {
				info->window->ReorderFilter(
					info->window->ui->effectFilters, filter,
					info->effectIdx++, false);
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
		QListWidgetItem *itemView = ui->asyncFilters->item(0);
		auto item = static_cast<PLSFiltersItemView *>(
			ui->asyncFilters->itemWidget(itemView));
		SetCurrentItem(item, ui->asyncFilters, true);

		//ui->asyncFilters->setCurrentItem(ui->asyncFilters->item(0));
		isAsync = true;

	} else if (ui->effectFilters->count() > 0) {
		QListWidgetItem *itemView = ui->effectFilters->item(0);
		auto item = static_cast<PLSFiltersItemView *>(
			ui->effectFilters->itemWidget(itemView));
		SetCurrentItem(item, ui->effectFilters, false);

		//ui->effectFilters->setCurrentItem(ui->effectFilters->item(0));
		isAsync = false;
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

	//for (int i = 0; i < ui->rightContainerLayout->count(); i++) {
	//	QSplitterHandle *hndl = ui->rightContainerLayout->handle(i);
	//	hndl->setEnabled(show_splitter_handle);
	//}

	//ui->propertiesFrame->setVisible(show_splitter_frame);
	ui->propertiesFrame->setVisible(true);
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

	if (async && ((audioOnly && filterVideo) || (!audio && !asyncSource) ||
		      (filterAudio && !audio)))
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
			: type(type_), name(name_)
		{
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

		auto it = types.begin();
		for (; it != types.end(); ++it) {
			if (it->name >= name)
				break;
		}

		types.emplace(it, type_str, name);
	}

	QMenu *popup = new QMenu(QTStr("Add"), this);
	for (FilterInfo &type : types) {
		uint32_t filterFlags =
			obs_get_source_output_flags(type.type.c_str());

		if (!filter_compatible(async, sourceFlags, filterFlags))
			continue;

		QAction *popupItem =
			new QAction(QT_UTF8(type.name.c_str()), this);
		popupItem->setData(QT_UTF8(type.type.c_str()));
		connect(popupItem, SIGNAL(triggered(bool)), this,
			SLOT(AddFilterFromAction()));
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

		OBSDataAutoRelease wrapper = obs_data_create();
		obs_data_set_string(wrapper, "sname",
				    obs_source_get_name(source));
		obs_data_set_string(wrapper, "fname", name.c_str());
		std::string scene_name = obs_source_get_name(
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
				->GetCurrentSceneSource());
		auto undo = [scene_name](const std::string &data) {
			obs_source_t *ssource =
				obs_get_source_by_name(scene_name.c_str());
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
				->SetCurrentScene(ssource, true);
			obs_source_release(ssource);

			obs_data_t *dat =
				obs_data_create_from_json(data.c_str());
			obs_source_t *source = obs_get_source_by_name(
				obs_data_get_string(dat, "sname"));
			obs_source_t *filter = obs_source_get_filter_by_name(
				source, obs_data_get_string(dat, "fname"));
			obs_source_filter_remove(source, filter);

			obs_data_release(dat);
			obs_source_release(source);
			obs_source_release(filter);
		};

		OBSDataAutoRelease rwrapper = obs_data_create();
		obs_data_set_string(rwrapper, "sname",
				    obs_source_get_name(source));
		auto redo = [scene_name, id = std::string(id),
			     name](const std::string &data) {
			OBSSourceAutoRelease ssource =
				obs_get_source_by_name(scene_name.c_str());
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
				->SetCurrentScene(ssource.Get(), true);

			OBSDataAutoRelease dat =
				obs_data_create_from_json(data.c_str());
			OBSSourceAutoRelease source = obs_get_source_by_name(
				obs_data_get_string(dat, "sname"));

			OBSSourceAutoRelease filter = obs_source_create(
				id.c_str(), name.c_str(), nullptr, nullptr);
			if (filter) {
				obs_source_filter_add(source, filter);
			}
		};

		std::string undo_data(obs_data_get_json(wrapper));
		std::string redo_data(obs_data_get_json(rwrapper));
		main->undo_s.add_action(QTStr("Undo.Add").arg(name.c_str()),
					undo, redo, undo_data, redo_data);

		OBSSourceAutoRelease filter =
			obs_source_create(id, name.c_str(), nullptr, nullptr);
		if (filter) {
			const char *sourceName = obs_source_get_name(source);

			blog(LOG_INFO,
			     "User added filter '%s' (%s) "
			     "to source '%s'",
			     name.c_str(), id, sourceName);

			obs_source_filter_add(source, filter);
		}
	}
}

void OBSBasicFilters::AddFilterFromAction()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (!action)
		return;

	AddNewFilter(QT_TO_UTF8(action->data().toString()));
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

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool OBSBasicFilters::nativeEvent(const QByteArray &eventType, void *message,
				  qintptr *result)
#else
bool OBSBasicFilters::nativeEvent(const QByteArray &eventType, void *message,
				  long *result)
#endif
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

void OBSBasicFilters::OBSSourceReordered(void *param, calldata_t *data)
{
	QMetaObject::invokeMethod(reinterpret_cast<OBSBasicFilters *>(param),
				  "ReorderFilters");

	UNUSED_PARAMETER(data);
}

void OBSBasicFilters::SourceRemoved(void *param, calldata_t *data)
{
	UNUSED_PARAMETER(data);

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
		if (QueryRemove(this, filter))
			delete_filter(filter);
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

void OBSBasicFilters::CustomContextMenu(const QPoint &pos, bool async)
{
	QListWidget *list = async ? ui->asyncFilters : ui->effectFilters;
	QListWidgetItem *item = list->itemAt(pos);

	QMenu popup(window());

	QPointer<QMenu> addMenu = CreateAddFilterPopupMenu(async);
	if (addMenu)
		popup.addMenu(addMenu);

	if (item) {
		const char *dulpicateSlot =
			async ? SLOT(DuplicateAsyncFilter())
			      : SLOT(DuplicateEffectFilter());

		const char *renameSlot = async ? SLOT(RenameAsyncFilter())
					       : SLOT(RenameEffectFilter());
		const char *removeSlot =
			async ? SLOT(on_removeAsyncFilter_clicked())
			      : SLOT(on_removeEffectFilter_clicked());

		popup.addSeparator();
		popup.addAction(QTStr("Duplicate"), this, dulpicateSlot);
		popup.addSeparator();
		popup.addAction(QTStr("Rename"), this, renameSlot);
		popup.addAction(QTStr("Remove"), this, removeSlot);
		popup.addSeparator();

		QAction *copyAction = new QAction(QTStr("Copy"));
		connect(copyAction, SIGNAL(triggered()), this,
			SLOT(CopyFilter()));
		copyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
		ui->effectWidget->addAction(copyAction);
		ui->asyncWidget->addAction(copyAction);
		popup.addAction(copyAction);
	}

	QAction *pasteAction = new QAction(QTStr("Paste"));
	pasteAction->setEnabled(main->copyFilter);
	connect(pasteAction, SIGNAL(triggered()), this, SLOT(PasteFilter()));
	pasteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
	ui->effectWidget->addAction(pasteAction);
	ui->asyncWidget->addAction(pasteAction);
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
	//CustomContextMenu(pos, true);
}

void OBSBasicFilters::on_effectFilters_customContextMenuRequested(
	const QPoint &pos)
{
	//CustomContextMenu(pos, false);
}

void OBSBasicFilters::RenameAsyncFilter()
{
	EditItem(ui->asyncFilters->currentItem(), true);
}

void OBSBasicFilters::RenameEffectFilter()
{
	EditItem(ui->effectFilters->currentItem(), false);
}

void OBSBasicFilters::DuplicateAsyncFilter()
{
	DuplicateItem(ui->asyncFilters->currentItem());
}

void OBSBasicFilters::DuplicateEffectFilter()
{
	DuplicateItem(ui->effectFilters->currentItem());
}

void OBSBasicFilters::FilterNameEdited(QWidget *editor,
				       PLSFiltersItemView *item,
				       FocusList *list, bool async)
{
	this->SetCurrentItem(item, list, async);

	OBSSource filter = item->GetFilter();

	QLineEdit *edit = static_cast<QLineEdit *>(editor);
	string name = QT_TO_UTF8(edit->text().trimmed());

	const char *prevName = obs_source_get_name(filter);
	bool sameName = (name == prevName);
	OBSSourceAutoRelease foundFilter = nullptr;

	if (!sameName)
		foundFilter =
			obs_source_get_filter_by_name(source, name.c_str());

	if (foundFilter || name.empty() || sameName) {
		item->SetText(QT_UTF8(prevName));

		if (foundFilter) {
			OBSMessageBox::information(window(),
						   QTStr("Alert.Title"),
						   QTStr("NameExists.Text"));
		} else if (name.empty()) {
			OBSMessageBox::information(window(),
						   QTStr("Alert.Title"),
						   QTStr("NoNameEntered.Text"));
		}
	} else {
		const char *sourceName = obs_source_get_name(source);

		blog(LOG_INFO,
		     "User renamed filter '%s' on source '%s' to '%s'",
		     prevName, sourceName, name.c_str());

		item->SetText(QT_UTF8(name.c_str()));
		obs_source_set_name(filter, name.c_str());

		std::string scene_name = obs_source_get_name(
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
				->GetCurrentSceneSource());
		auto undo = [scene_name, prev = std::string(prevName),
			     name](const std::string &data) {
			OBSSourceAutoRelease ssource =
				obs_get_source_by_name(scene_name.c_str());
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
				->SetCurrentScene(ssource.Get(), true);

			OBSSourceAutoRelease source =
				obs_get_source_by_name(data.c_str());
			OBSSourceAutoRelease filter =
				obs_source_get_filter_by_name(source,
							      name.c_str());
			obs_source_set_name(filter, prev.c_str());
		};

		auto redo = [scene_name, prev = std::string(prevName),
			     name](const std::string &data) {
			OBSSourceAutoRelease ssource =
				obs_get_source_by_name(scene_name.c_str());
			reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
				->SetCurrentScene(ssource.Get(), true);

			OBSSourceAutoRelease source =
				obs_get_source_by_name(data.c_str());
			OBSSourceAutoRelease filter =
				obs_source_get_filter_by_name(source,
							      prev.c_str());
			obs_source_set_name(filter, name.c_str());
		};

		std::string undo_data(sourceName);
		std::string redo_data(sourceName);
		main->undo_s.add_action(QTStr("Undo.Rename").arg(name.c_str()),
					undo, redo, undo_data, redo_data);
	}

	editActive = false;
}

void OBSBasicFilters::AsyncFilterNameEdited(
	QWidget *editor, QAbstractItemDelegate::EndEditHint endHint)
{
	//FilterNameEdited(editor, ui->asyncFilters);
	UNUSED_PARAMETER(endHint);
}

void OBSBasicFilters::EffectFilterNameEdited(
	QWidget *editor, QAbstractItemDelegate::EndEditHint endHint)
{
	//FilterNameEdited(editor, ui->effectFilters);
	UNUSED_PARAMETER(endHint);
}

void OBSBasicFilters::ResetFilters()
{
	QListWidget *list = isAsync ? ui->asyncFilters : ui->effectFilters;
	int row = list->currentRow();

	OBSSource filter = GetFilter(row, isAsync);

	if (!filter)
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

		filterView->RefreshProperties();
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
void OBSBasicFilters::SetCurrentItem(PLSFiltersItemView *item,
				     FocusList *filtersList, bool async)
{
	if (!item)
		return;
	int row = 0;

	for (int i = 0; i < filtersList->count(); i++) {
		QListWidgetItem *itemView = filtersList->item(i);
		if (!itemView) {
			continue;
		}
		auto filter = static_cast<PLSFiltersItemView *>(
			filtersList->itemWidget(itemView));
		if (filter) {
			if (filter == item) {
				row = i;
				async = isAsync;
				filtersList->setCurrentItem(itemView);
				filter->SetCurrentItemState(true);
			} else {
				filter->SetCurrentItemState(false);
			}
		}
	}
}

void OBSBasicFilters::delete_filter(OBSSource filter)
{
	OBSDataAutoRelease wrapper = obs_save_source(filter);
	std::string parent_name(obs_source_get_name(source));
	obs_data_set_string(wrapper, "undo_name", parent_name.c_str());

	std::string scene_name = obs_source_get_name(
		reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
			->GetCurrentSceneSource());
	auto undo = [scene_name](const std::string &data) {
		OBSSourceAutoRelease ssource =
			obs_get_source_by_name(scene_name.c_str());
		reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
			->SetCurrentScene(ssource.Get(), true);

		OBSDataAutoRelease dat =
			obs_data_create_from_json(data.c_str());
		OBSSourceAutoRelease source = obs_get_source_by_name(
			obs_data_get_string(dat, "undo_name"));
		OBSSourceAutoRelease filter = obs_load_source(dat);
		obs_source_filter_add(source, filter);
	};

	OBSDataAutoRelease rwrapper = obs_data_create();
	obs_data_set_string(rwrapper, "fname", obs_source_get_name(filter));
	obs_data_set_string(rwrapper, "sname", parent_name.c_str());
	auto redo = [scene_name](const std::string &data) {
		OBSSourceAutoRelease ssource =
			obs_get_source_by_name(scene_name.c_str());
		reinterpret_cast<OBSBasic *>(App()->GetMainWindow())
			->SetCurrentScene(ssource.Get(), true);

		OBSDataAutoRelease dat =
			obs_data_create_from_json(data.c_str());
		OBSSourceAutoRelease source = obs_get_source_by_name(
			obs_data_get_string(dat, "sname"));
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

void OBSBasicFilters::SetupFiltersItem(FocusList *list, QListWidgetItem *item,
				       obs_source_t *source, bool current)
{
	PLSFiltersItemView *baseWidget = new PLSFiltersItemView(source);
	uint32_t flags = obs_source_get_output_flags(source);
	bool async = (flags & OBS_SOURCE_ASYNC) != 0;

	if (async) {
		connect(baseWidget,
			&PLSFiltersItemView::OnFilterRenameTriggered,
			baseWidget,
			&PLSFiltersItemView::OnRenameActionTriggered);
		connect(baseWidget, &PLSFiltersItemView::FinishingEditName,
			this, [=](QWidget *edit, PLSFiltersItemView *item) {
				FilterNameEdited(edit, item, ui->asyncFilters,
						 true);
			});
		connect(baseWidget, &PLSFiltersItemView::CurrentItemChanged,
			this, [this](PLSFiltersItemView *item) {
				SetCurrentItem(item, ui->asyncFilters, true);
				SetCurrentItem(item, ui->effectFilters, false);
			});
	} else {
		connect(baseWidget,
			&PLSFiltersItemView::OnFilterRenameTriggered,
			baseWidget,
			&PLSFiltersItemView::OnRenameActionTriggered);
		connect(baseWidget, &PLSFiltersItemView::FinishingEditName,
			this, [=](QWidget *edit, PLSFiltersItemView *item) {
				FilterNameEdited(edit, item, ui->effectFilters,
						 false);
			});
		connect(baseWidget, &PLSFiltersItemView::CurrentItemChanged,
			this, [this](PLSFiltersItemView *item) {
				SetCurrentItem(item, ui->asyncFilters, true);
				SetCurrentItem(item, ui->effectFilters, false);
			});
	}

	list->setItemWidget(item, baseWidget);
	if (current) {
		SetCurrentItem(baseWidget, list, async);
	}
}
